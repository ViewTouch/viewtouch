#include "app/legacy/legacy_import.hh"

#include "app/pos_json.hh"
#include "layout/layout.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <map>

using namespace Qt::StringLiterals;

namespace vt::legacy {

using vt::Money;

namespace {

QString tr(const char *s, const char *c = nullptr, int n = -1) { return QCoreApplication::translate("vt::legacy", s, c, n); }
std::string ss(const QString &s) { return s.toStdString(); }
Money cents(std::int64_t c) { return Money::fromCents(c); }
std::int64_t ppm(double fraction) { return std::llround(fraction * 1'000'000); }

// "Fish Tacos (2)" -> "fish-tacos-2": an id from a name, made unique in `taken`.
std::string idFrom(const QString &prefix, const QString &name, QSet<QString> &taken)
{
    QString base = name.toLower();
    base.replace(QRegularExpression(u"[^a-z0-9]+"_s), u"-"_s);
    base = prefix + base.remove(QRegularExpression(u"^-+|-+$"_s));
    if (base == prefix)
        base = prefix + u"item"_s;
    QString id = base;
    for (int n = 2; taken.contains(id); ++n)
        id = base + u'-' + QString::number(n);
    taken.insert(id);
    return ss(id);
}

// --- tenders legacy took: what each is in Modern ------------------------------------

enum class Kind { Money, Card, Change, Discount, Comp, Meal, Gratuity, CardTip, CapturedTip, Fee, Skip };

Kind kindOf(int tender)
{
    switch (tender) {
    case 0: case 1: return Kind::Money;              // cash, check
    case 2: case 4: case 6: case 7: case 11: case 12: return Kind::Card;   // card, gift certificate, account, room
    case 18: return Kind::Change;
    case 3: case 8: return Kind::Discount;           // coupon, discount
    case 5: case 22: return Kind::Comp;              // comp, an item comped
    case 10: return Kind::Meal;
    case 21: return Kind::Gratuity;
    case 13: return Kind::CardTip;                   // a tip charged on the card
    case 9: return Kind::CapturedTip;                // the part of a payment that was a tip
    case 25: case 26: case 27: case 28: return Kind::Fee;   // a card fee charged to the guest
    default: return Kind::Skip;                      // tips paid out, payouts, overage, card fees: the drawer's
    }
}

QString tenderName(int tender, int id, const Media &media)
{
    const auto named = [&](const QList<Discount> &list, const QString &otherwise) {
        const auto it = std::ranges::find(list, id, &Discount::id);
        return it != list.end() ? it->name : otherwise;
    };
    switch (tender) {
    case 0: return tr("Cash");
    case 1: return tr("Check");
    case 2: return named(media.creditCards, tr("Charge Card"));
    case 3: return named(media.coupons, tr("Coupon"));
    case 4: return tr("Gift Certificate");
    case 5: return named(media.comps, tr("Comp"));
    case 6: return tr("House Account");
    case 7: return tr("Room Charge");
    case 8: return named(media.discounts, tr("Discount"));
    case 10: return named(media.meals, tr("Employee Meal"));
    case 11: return tr("Credit Card");
    case 12: return tr("Debit Card");
    case 22: return tr("Item Comp");
    }
    return tr("Other");
}

core::CheckType checkTypeOf(int type)
{
    switch (type) {
    case 2: case 12: case 13: case 15: return core::CheckType::Takeout;
    case 3: return core::CheckType::Tab;
    case 5: return core::CheckType::Delivery;
    case 8: case 9: case 10: return core::CheckType::Quick;
    default: return core::CheckType::DineIn;
    }
}

core::TaxClass taxClassOf(int salesType)
{
    if (salesType & 2)
        return core::TaxClass::None;
    if (salesType & 1)
        return core::TaxClass::Alcohol;
    if (salesType & 4)
        return core::TaxClass::Room;
    if (salesType & 8)
        return core::TaxClass::Merchandise;
    return core::TaxClass::Food;
}

// The words legacy puts before an item for its qualifiers (sales.hh, bit by bit).
QString qualifierWords(int q)
{
    static const QStringList names{u"No"_s,     u"Side"_s,   u"Sub"_s,   u"Lite"_s,  u"Only"_s,     u"Extra"_s,
                                   u"Double"_s, u"Dry"_s,    u"Plain"_s, u"Toasted"_s, u"Untoasted"_s, u"Crispy"_s,
                                   u"Hard"_s,   u"Soft"_s,   u"Grilled"_s, u"Left"_s, u"Right"_s,    u"Whole"_s,
                                   u"Cut/2"_s,  u"Cut/3"_s,  u"Cut/4"_s, u"Easy"_s,  u"Add"_s,      u"Senior Share"_s};
    QStringList w;
    for (int i = 0; i < names.size(); ++i)
        if (q & (1 << i))
            w.append(names.at(i));
    return w.join(u' ');
}

// Printers: legacy's targets as Modern's printer ids.
QString printerId(int target)
{
    switch (target) {
    case 1: case 10: return u"kitchen"_s;
    case 2: case 11: return u"kitchen2"_s;
    case 3: return u"bar"_s;
    case 4: return u"bar2"_s;
    case 5: return u"expo"_s;
    case 6: return u"receipt"_s;
    case 7: return u"report"_s;
    case 12: case 14: return u"kitchen3"_s;
    case 13: case 15: return u"kitchen4"_s;
    }
    return {};
}



// Legacy worked some discounts out its own way: one that didn't lower the
// tax (the tax on the full price, then so much off), or a percent of the
// items with their tax. Put as what they took off, so the check comes to
// what legacy took: first the amount as it was, the tax left on the full
// price; else the amount that made legacy's total, off as usual. Nothing
// when neither does (the check is then listed as not adding up).
std::optional<core::Totals> fitDiscounts(core::Check &m, const core::TaxRates &rates, Money taken)
{
    std::vector<core::Payment *> off;
    for (core::Payment &p : m.payments)
        if (p.kind == core::TenderKind::Discount)
            off.push_back(&p);
    if (off.empty())
        return std::nullopt;
    const core::Check before = m;
    const core::Totals was = m.totals(rates);
    // 1. As they were, not lowering the tax.
    for (core::Payment *p : off) {
        p->amount = was.discountOf.at(p->id);
        p->percentBp = 0;
        p->classShares = {{core::TaxClass::None, p->amount}};
    }
    if (core::Totals t = m.totals(rates); t.total == taken)
        return t;
    // 2. One discount: the amount that comes to what legacy took.
    m = before;
    if (off.size() == 1) {
        core::Payment &p = *std::ranges::find_if(m.payments, [](const core::Payment &x) {
            return x.kind == core::TenderKind::Discount;
        });
        p.percentBp = 0;
        p.classShares.clear();
        std::int64_t lo = 0, hi = was.items.cents();
        while (lo < hi) {   // the smallest amount off that brings it down to what was taken
            const std::int64_t mid = (lo + hi) / 2;
            p.amount = cents(mid);
            if (m.totals(rates).total > taken)
                lo = mid + 1;
            else
                hi = mid;
        }
        p.amount = cents(lo);
        if (core::Totals t = m.totals(rates); t.total == taken)
            return t;
    }
    m = before;
    return std::nullopt;
}

// A modifier: legacy's modifier kinds, and what it kept in its "Modifier"
// family (23), used only on modifier pages.
bool modifier(const MenuItem &m) { return m.type == 1 || m.type == 2 || m.family == 23; }

} // namespace

QString familyName(int family)
{
    static const std::map<int, QString> names{
        {0, u"Appetizer"_s},     {1, u"Beverage"_s},       {2, u"Lunch Entree"_s},   {3, u"Child Menu"_s},
        {4, u"Dessert"_s},       {5, u"Sandwich"_s},       {6, u"Side Order"_s},     {7, u"Breakfast Entree"_s},
        {8, u"A La Carte"_s},    {10, u"Burger"_s},        {11, u"Dinner Entree"_s}, {12, u"Salad"_s},
        {13, u"Soup"_s},         {14, u"Pizza"_s},         {15, u"Specialty"_s},     {16, u"Beer"_s},
        {17, u"Bottled Beer"_s}, {18, u"Wine"_s},          {19, u"Bottled Wine"_s},  {20, u"Cocktail"_s},
        {21, u"Malt Beverage"_s}, {22, u"Seafood"_s},      {23, u"Modifier"_s},      {24, u"Light Dinner"_s},
        {25, u"Reorder"_s},      {26, u"Merchandise"_s},   {27, u"Specialty Entree"_s}, {28, u"Alcohol"_s},
        {29, u"Banquet"_s},      {30, u"Bakery"_s},        {31, u"Room"_s}};
    const auto it = names.find(family);
    return it != names.end() ? it->second : tr("Other");
}

std::int64_t toEpoch(const WallTime &t, const QTimeZone &zone)
{
    if (!t.isSet())
        return 0;
    return QDateTime(t.date, t.time, zone).toMSecsSinceEpoch();
}

Imported convert(const Store &store, const ImportContext &cx)
{
    Imported out;
    out.errors = store.errors;
    const ImportParts &parts = cx.parts;
    const core::PosSettings *current = cx.current;

    // --- the menu -----------------------------------------------------------
    // Legacy finds an item by its name, capitals or not (StringCompare).
    QHash<QString, std::string> itemIds;   // legacy name, lower case -> Modern id
    const auto key = [](const QString &name) { return name.trimmed().toLower(); };
    QHash<int, std::string> categoryOf;    // legacy family -> category id
    QSet<int> ordered;                     // families with items (not only modifiers)
    if (store.menu) {   // (the sales name items too)
        QSet<QString> taken;
        for (const MenuItem &m : store.menu->items) {
            if (m.name.trimmed().isEmpty() || itemIds.contains(key(m.name)))
                continue;   // (legacy keys items by name: the first of a name is the one it finds)
            itemIds.insert(key(m.name), idFrom({}, m.name, taken));
            if (!categoryOf.contains(m.family))
                categoryOf.insert(m.family, ss(familyName(m.family)));   // (reports name a family by it)
            if (!modifier(m))
                ordered.insert(m.family);   // a family with something to order
        }
    }
    if (store.menu && parts.menu) {
        for (const auto &[family, id] : categoryOf.asKeyValueRange()) {
            if (!ordered.contains(family))
                continue;   // only modifiers (legacy's "Modifier" family): asked as choices, not a section
            core::MenuCategory c;
            c.id = id;
            c.name = ss(familyName(family));
            out.categories.push_back(c);
        }
        std::ranges::sort(out.categories, {}, &core::MenuCategory::id);
        QSet<QString> seen;
        for (const MenuItem &m : store.menu->items) {
            if (m.name.trimmed().isEmpty() || seen.contains(key(m.name)))
                continue;
            seen.insert(key(m.name));
            core::MenuItem i;
            i.id = itemIds.value(key(m.name));
            i.name = ss(m.name);
            i.family = categoryOf.value(m.family);
            i.price = cents(m.cost);
            if (m.takeoutCost > 0 && m.takeoutCost != m.cost)
                i.takeoutPrice = cents(m.takeoutCost);
            if (m.deliveryCost > 0 && m.deliveryCost != m.cost)
                i.deliveryPrice = cents(m.deliveryCost);
            if (m.type == 3) {
                i.substitute = true;
                i.substitutePrice = cents(m.subCost);
            }
            i.byWeight = m.type == 6;
            i.taxClass = taxClassOf(m.salesType);
            i.noDiscount = m.salesType & (16 | 64);
            i.noStaffDiscount = m.salesType & 32;
            i.isModifier = modifier(m);
            int target = m.printer;
            if (target == 0 && store.settings && m.family >= 0 && m.family < 64)
                target = store.settings->familyPrinter[m.family];   // "default": the family's
            if (target == 99)
                i.kitchenHide = true;   // printed nowhere
            else
                i.printer = ss(printerId(target));
            if (!m.printName.isEmpty() && m.printName != m.name)
                i.kitchenName = ss(m.printName);
            i.number = ss(m.itemCode);
            i.available = !m.outOfStock;
            i.prepMinutes = m.prepareTime > 0 ? std::max(1, m.prepareTime / 60) : 0;
            out.menu.push_back(i);
        }
    }

    // --- choice groups: legacy's modifier pages ---------------------------------
    // An item's button went to a page of modifiers (No Rice, Corn Tortilla...):
    // in Modern that page is a choice group the item asks for.
    if (store.menu && store.pages && parts.menu) {
        QHash<QString, const MenuItem *> byName;
        for (const MenuItem &m : store.menu->items)
            if (!byName.contains(key(m.name)))
                byName.insert(key(m.name), &m);
        QHash<int, const Page *> pageOf;
        for (const Page &p : store.pages->pages)
            pageOf.insert(p.id, &p);
        QHash<int, std::string> groupOf;   // legacy page -> group id
        const auto groupFor = [&](int pageId) -> std::string {
            if (groupOf.contains(pageId))
                return groupOf.value(pageId);
            const Page *p = pageOf.value(pageId);
            if (!p)
                return {};
            core::ModifierGroup g;
            for (const Zone &z : p->zones) {
                const QString name = z.fields.value(u"ItemName"_s).toString();
                const MenuItem *m = byName.value(key(name));
                if (!m || !modifier(*m))
                    continue;
                if (std::ranges::any_of(g.options, [&](const core::ModifierOption &o) { return o.itemId == itemIds.value(key(name)); }))
                    continue;
                core::ModifierOption o;
                o.name = ss(m->name);
                o.price = cents(m->cost);
                o.itemId = itemIds.value(key(name));
                if (!m->printName.isEmpty() && m->printName != m->name)
                    o.kitchenName = ss(m->printName);
                g.options.push_back(o);
            }
            if (g.options.empty()) {
                groupOf.insert(pageId, {});
                return {};
            }
            g.id = ss(u"legacy-page-%1"_s.arg(pageId));
            g.name = ss(p->name.trimmed());
            g.min = 0;
            g.max = int(g.options.size());
            out.modifierGroups.push_back(g);
            groupOf.insert(pageId, g.id);
            return g.id;
        };
        for (const Page &p : store.pages->pages)
            for (const Zone &z : p.zones) {
                const QString name = z.fields.value(u"ItemName"_s).toString();
                const int jump = z.fields.value(u"JumpID"_s).toInt();
                const MenuItem *m = byName.value(key(name));
                if (!m || modifier(*m) || !jump || jump == p.id)
                    continue;
                const std::string g = groupFor(jump);
                if (g.empty())
                    continue;
                for (core::MenuItem &i : out.menu)
                    if (i.id == itemIds.value(key(name))
                        && std::ranges::find(i.modifierGroups, g) == i.modifierGroups.end())
                        i.modifierGroups.push_back(g);
            }
    }

    // Whose checks: legacy's staff by name (they aren't brought over).
    QHash<int, std::pair<std::string, std::string>> staff;   // legacy user id -> an id, a name
    for (const Employee &e : store.employees) {
        QString name = (e.firstName + u' ' + e.lastName).trimmed();
        if (name.isEmpty())
            name = e.systemName.trimmed();
        if (name.isEmpty())
            name = tr("Employee %1").arg(e.id);
        staff.insert(e.id, {ss(u"legacy-%1"_s.arg(e.id)), ss(name)});
    }

    // --- closed business days ---------------------------------------------------
    // Cash and cards as this store calls them, so the reports put legacy's with its own.
    std::string cashId = "cash", cardId = "credit";
    if (current) {
        for (const core::Tender &t : current->tenders)
            if (t.kind == core::TenderKind::Card) {
                cardId = t.id;
                break;
            }
        for (const core::Tender &t : current->tenders)
            if (t.kind == core::TenderKind::Cash) {
                cashId = t.id;
                break;
            }
    }
    if (parts.history && !store.archives.isEmpty()) {
        const Settings *settings = store.settings ? &*store.settings : nullptr;
        std::int64_t previousEnd = 0;
        for (const Archive &a : store.archives) {
            ImportedDay day;
            day.legacyId = a.id;
            day.closedAt = toEpoch(a.end, cx.zone);
            day.openedAt = a.start.isSet() ? toEpoch(a.start, cx.zone) : previousEnd;
            core::TaxRates rates = current ? current->tax : core::TaxRates{};
            if (settings) {   // legacy's rates, unless the day kept its own
                rates.foodPpm = ppm(settings->taxFood);
                rates.alcoholPpm = ppm(settings->taxAlcohol);
                rates.roomPpm = ppm(settings->taxRoom);
                rates.merchandisePpm = ppm(settings->taxMerchandise);
            }
            if (a.taxFood) {
                rates.foodPpm = ppm(*a.taxFood);
                rates.alcoholPpm = ppm(*a.taxAlcohol);
                rates.roomPpm = ppm(*a.taxRoom);
                rates.merchandisePpm = ppm(*a.taxMerchandise);
            }
            if (settings)
                rates.taxTakeoutFood = settings->taxTakeoutFood;
            day.rates = rates;
            const Media media = a.media.value_or(settings ? settings->media : Media{});
            // Its drawers, each a drawer session.
            QHash<int, std::int64_t> drawerOf;   // legacy serial -> index + 1
            for (const Drawer &d : a.drawers) {
                core::DrawerSession s;
                s.name = ss(d.number < 0 ? tr("%1's bank").arg(QString::fromStdString(staff.value(-d.number).second))
                                         : tr("Drawer %1").arg(d.number > 0 ? d.number : d.serial));
                if (d.number < 0)
                    s.employeeId = staff.value(-d.number).first;
                s.terminal = ss(d.host);
                s.openedAt = toEpoch(d.start, cx.zone);
                s.closedAt = toEpoch(d.balanced.isSet() ? d.balanced : d.pulled, cx.zone);
                if (!s.closedAt)
                    s.closedAt = day.closedAt;
                if (!s.openedAt)
                    s.openedAt = day.openedAt;
                s.openedBy = staff.value(d.owner).second;    // (by name, as Modern's own)
                s.closedBy = staff.value(d.puller).second;
                for (const DrawerBalance &b : d.balances)
                    if (b.tenderType == 0)
                        s.counted += cents(b.entered);
                day.drawers.push_back(s);
                drawerOf.insert(d.serial, std::int64_t(day.drawers.size()));
            }
            // Its checks: each part legacy split a check into is a check here.
            for (const Check &c : a.checks) {
                if (c.training())
                    continue;
                for (qsizetype part = 0; part < c.subchecks.size(); ++part) {
                    const SubCheck &sc = c.subchecks[part];
                    if (sc.status != 2)
                        continue;   // voided (or never finished)
                    core::Check m;
                    m.type = checkTypeOf(c.type);
                    m.status = core::CheckStatus::Closed;
                    QString label = c.label.isEmpty() ? QString::number(c.number ? c.number : c.serial) : c.label;
                    if (c.subchecks.size() > 1)
                        label += u" (%1)"_s.arg(part + 1);
                    m.label = ss(label);
                    m.guests = std::max(1, c.guests);
                    const auto server = staff.value(c.userOwner ? c.userOwner : c.userOpen);
                    m.serverId = server.first;
                    m.serverName = server.second;
                    m.openedAt = toEpoch(c.opened, cx.zone);
                    m.closedAt = toEpoch(sc.settleTime, cx.zone);
                    if (!m.closedAt)
                        m.closedAt = m.openedAt ? m.openedAt : day.closedAt;
                    if (!m.openedAt)
                        m.openedAt = m.closedAt;
                    m.taxExempt = ss(sc.taxExempt);
                    m.closedRates = rates;
                    m.drawerSession = drawerOf.value(sc.drawerId);
                    if (c.customerId >= 0)
                        m.customerId = ss(u"legacy-%1"_s.arg(c.customerId));
                    // Lines: a modifier rides on the item before it.
                    Money comped;
                    for (const Order &o : sc.orders) {
                        const int each = (o.isReduced && o.reducedCost) || o.employeeMeal ? o.reducedCost : o.cost;
                        QString name = o.name;
                        const QString words = qualifierWords(o.qualifier);
                        if (!words.isEmpty())
                            name = words + u' ' + name;
                        const std::string itemId = itemIds.value(key(o.name), ss(u"legacy:"_s + o.name));
                        if (o.isModifier() && !m.lines.empty()) {
                            core::OrderLine &parent = m.lines.back();
                            core::Modifier mod;
                            mod.itemId = itemId;
                            mod.name = ss(name);
                            mod.unitPrice = (o.qualifier & 1) ? Money() : cents(std::int64_t(each) * o.count);
                            parent.modifiers.push_back(mod);
                            if (o.status & 16)
                                comped += mod.unitPrice * parent.quantity;
                            continue;
                        }
                        core::OrderLine l;
                        l.id = m.nextLineId++;
                        l.itemId = itemId;
                        l.name = ss(name);
                        l.taxClass = taxClassOf(o.salesType);
                        l.seat = o.seat;
                        l.sent = true;
                        if (o.type == 6) {   // by the pound: count is hundredths of a pound
                            l.unitPrice = cents(each);
                            l.weight = std::int64_t(o.count) * 10;
                            l.weightUnit = "lb";
                            l.quantity = 1;
                        } else {
                            l.unitPrice = (o.qualifier & 1) ? Money() : cents(each);
                            l.quantity = std::max(1, o.count);
                        }
                        m.lines.push_back(l);
                        if (o.status & 16)
                            comped += l.total();
                    }
                    if (sc.deliveryCharge > 0) {
                        core::OrderLine l;
                        l.id = m.nextLineId++;
                        l.itemId = "fee:delivery";
                        l.name = ss(tr("Delivery"));
                        l.unitPrice = cents(sc.deliveryCharge);
                        l.kitchenHide = true;
                        m.lines.push_back(l);
                    }
                    // Payments.
                    Money taken;
                    bool compPaid = false;
                    for (const Payment &p : sc.payments) {
                        const Kind kind = kindOf(p.tenderType);
                        core::Payment pay;
                        pay.id = m.nextPaymentId;
                        pay.tenderName = ss(tenderName(p.tenderType, p.tenderId, media));
                        switch (kind) {
                        case Kind::Money:
                            pay.tenderId = p.tenderType == 0 ? cashId : "legacy-check";
                            pay.kind = core::TenderKind::Cash;
                            pay.amount = cents(p.amount);
                            taken += pay.amount;
                            break;
                        case Kind::Card:
                            pay.tenderId = p.tenderType == 2 || p.tenderType >= 11 ? cardId : ss(u"legacy-%1"_s.arg(p.tenderType));
                            pay.kind = core::TenderKind::Card;
                            pay.amount = cents(p.amount);
                            pay.tip = cents(p.cardTip);
                            taken += pay.amount;
                            break;
                        case Kind::Change:
                            taken -= cents(p.amount);
                            continue;   // Modern works out the change itself
                        case Kind::Discount:
                        case Kind::Comp:
                        case Kind::Meal:
                            pay.tenderId = ss(u"legacy-%1-%2"_s.arg(p.tenderType).arg(p.tenderId));
                            pay.kind = core::TenderKind::Discount;
                            if (p.flags & 1)
                                pay.percentBp = p.amount;
                            else
                                pay.amount = cents(p.amount);
                            pay.staffMeal = kind == Kind::Meal;
                            compPaid |= p.tenderType == 22;
                            break;
                        case Kind::Gratuity:
                            if (p.flags & 1)
                                m.gratuityBp = p.amount;
                            else
                                m.gratuityBp = 0;   // (a set amount: worked out below)
                            continue;
                        case Kind::CardTip:
                            for (auto it = m.payments.rbegin(); it != m.payments.rend(); ++it)
                                if (it->kind == core::TenderKind::Card) {
                                    it->tip += cents(p.amount);
                                    break;
                                }
                            continue;
                        case Kind::CapturedTip:   // out of the payment before it, as its tip
                            for (auto it = m.payments.rbegin(); it != m.payments.rend(); ++it)
                                if (it->kind != core::TenderKind::Discount) {
                                    it->amount -= cents(p.amount);
                                    it->tip += cents(p.amount);
                                    taken -= cents(p.amount);
                                    break;
                                }
                            continue;
                        case Kind::Fee: {   // a charge on the check, taxed like food
                            core::OrderLine l;
                            l.id = m.nextLineId++;
                            l.itemId = "fee:card";
                            l.name = ss(tr("Card fee"));
                            l.unitPrice = cents(p.amount);
                            l.kitchenHide = true;
                            m.lines.push_back(l);
                            continue;
                        }
                        case Kind::Skip:
                            continue;
                        }
                        ++m.nextPaymentId;
                        m.payments.push_back(pay);
                    }
                    // An item comped without legacy recording a payment for it.
                    if (comped.cents() > 0 && !compPaid) {
                        core::Payment pay;
                        pay.id = m.nextPaymentId++;
                        pay.tenderId = "legacy-comp";
                        pay.tenderName = ss(tr("Item Comp"));
                        pay.kind = core::TenderKind::Discount;
                        pay.amount = comped;
                        m.payments.push_back(pay);
                    }
                    core::Totals t = m.totals(rates);
                    if (t.total != taken)
                        t = fitDiscounts(m, rates, taken).value_or(t);
                    if (t.total != taken) {
                        out.mismatches.push_back({a.id, c.serial, label, taken, t.total});
                        // Closed for what legacy took, not owing the difference:
                        // that much more off (not lowering the tax), said so.
                        if (t.total > taken) {
                            core::Payment even;
                            even.id = m.nextPaymentId++;
                            even.tenderId = "legacy-difference";
                            even.tenderName = ss(tr("Legacy difference"));
                            even.kind = core::TenderKind::Discount;
                            even.amount = t.total - taken;
                            even.classShares = {{core::TaxClass::None, even.amount}};
                            m.payments.push_back(even);
                        }
                    }
                    day.sales += taken;
                    day.checks.push_back(std::move(m));
                }
            }
            std::ranges::sort(day.checks, {}, &core::Check::closedAt);
            previousEnd = day.closedAt;
            out.days.push_back(std::move(day));
        }
        std::int64_t voids = 0, comps = 0, expenses = 0, cards = 0;
        for (const Archive &a : store.archives) {
            voids += a.voids;
            comps += a.comps;
            expenses += a.expenses;
            cards += a.cardRecords;
        }
        if (expenses)
            out.notes.append(tr("%n expense(s) recorded in legacy aren't brought over.", nullptr, int(expenses)));
        if (voids || comps)
            out.notes.append(tr("Legacy's list of voids and comps (who and why) isn't brought over; the checks "
                                "show what was comped."));
        if (cards)
            out.notes.append(tr("Card batch records aren't brought over (the card processor keeps them)."));
    }
    if (store.current && !store.current->checks.isEmpty() && parts.history)
        out.errors.append(tr("Legacy's day isn't closed: %n check(s) are still open. Close the day in legacy "
                             "ViewTouch first (End Day), then bring the store over.",
                             nullptr, int(store.current->checks.size())));
    return out;
}

QVariantMap preview(const Store &store, const Imported &in)
{
    std::int64_t checks = 0;
    Money sales;
    for (const ImportedDay &d : in.days) {
        checks += qint64(d.checks.size());
        sales += d.sales;
    }
    const auto dayText = [](std::int64_t ms) { return QDateTime::fromMSecsSinceEpoch(ms).date().toString(Qt::ISODate); };
    QVariantMap out{{u"folder"_s, store.folder},
                    {u"storeName"_s, store.settings ? store.settings->storeName.trimmed() : QString()},
                    {u"items"_s, qint64(in.menu.size())},
                    {u"categories"_s, qint64(in.categories.size())},
                    {u"choices"_s, qint64(in.modifierGroups.size())},
                    {u"days"_s, qint64(in.days.size())},
                    {u"checks"_s, qint64(checks)},
                    {u"sales"_s, qint64(sales.cents())},
                    {u"mismatches"_s, qint64(in.mismatches.size())},
                    {u"notes"_s, in.notes},
                    {u"errors"_s, in.errors}};
    if (!in.days.empty()) {
        out.insert(u"from"_s, dayText(in.days.front().openedAt ? in.days.front().openedAt : in.days.front().closedAt));
        out.insert(u"to"_s, dayText(in.days.back().closedAt));
    }
    return out;
}

} // namespace vt::legacy
