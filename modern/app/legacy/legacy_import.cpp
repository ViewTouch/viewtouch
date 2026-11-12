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

QString printerName(const QString &id)
{
    static const QHash<QString, QString> names{{u"kitchen"_s, u"Kitchen"_s},   {u"kitchen2"_s, u"Kitchen 2"_s},
                                               {u"kitchen3"_s, u"Kitchen 3"_s}, {u"kitchen4"_s, u"Kitchen 4"_s},
                                               {u"bar"_s, u"Bar"_s},           {u"bar2"_s, u"Bar 2"_s},
                                               {u"expo"_s, u"Expediter"_s},    {u"receipt"_s, u"Receipts"_s},
                                               {u"report"_s, u"Reports"_s}};
    return names.value(id, id);
}

// Jobs: legacy's job numbers as Modern's roles.
QString roleOf(int job)
{
    switch (job) {
    case 2: return u"busser"_s;
    case 5: return u"cashier"_s;
    case 6: case 7: return u"server"_s;
    case 8: return u"host"_s;
    case 10: case 11: case 12: return u"manager"_s;
    case 13: return u"bartender"_s;
    case 1: case 3: case 4: case 9: case 14: return u"kitchen"_s;
    }
    return u"server"_s;
}

bool managerRole(const QString &role) { return role == u"manager" || role == u"owner" || role == u"admin"; }

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
    if (store.menu && (parts.menu || parts.pages)) {
        QSet<QString> taken;
        for (const MenuItem &m : store.menu->items) {
            if (m.name.trimmed().isEmpty() || itemIds.contains(key(m.name)))
                continue;   // (legacy keys items by name: the first of a name is the one it finds)
            itemIds.insert(key(m.name), idFrom({}, m.name, taken));
            if (!categoryOf.contains(m.family))
                categoryOf.insert(m.family, ss(u"family-%1"_s.arg(m.family)));
        }
    }
    if (store.menu && parts.menu) {
        for (const auto &[family, id] : categoryOf.asKeyValueRange()) {
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
            i.isModifier = m.type == 1 || m.type == 2;
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

    // --- settings -----------------------------------------------------------
    if (store.settings && parts.settings) {
        const Settings &s = *store.settings;
        if (!s.storeName.trimmed().isEmpty())
            out.storeName = s.storeName.trimmed();
        QStringList header;
        for (const QString &l : {s.storeAddress, s.storeAddress2})
            if (!l.trimmed().isEmpty())
                header.append(l.trimmed());
        for (const QString &l : s.receiptHeader)
            if (!l.trimmed().isEmpty() && !header.contains(l.trimmed()) && l.trimmed() != s.storeName.trimmed())
                header.append(l.trimmed());
        out.receiptHeader = header.join(u'\n');
        QStringList footer;
        for (const QString &l : s.receiptFooter)
            if (!l.trimmed().isEmpty())
                footer.append(l.trimmed());
        out.receiptFooter = footer.join(u'\n');
        core::TaxRates t = current ? current->tax : core::TaxRates{};
        t.foodPpm = ppm(s.taxFood);
        t.alcoholPpm = ppm(s.taxAlcohol);
        t.roomPpm = ppm(s.taxRoom);
        t.merchandisePpm = ppm(s.taxMerchandise);
        t.taxTakeoutFood = s.taxTakeoutFood;
        out.tax = t;
        if (s.taxGst || s.taxPst || s.taxHst || s.taxQst || s.taxVat)
            out.notes.append(tr("Canada's GST, PST, HST and QST, and VAT, aren't kept apart in ViewTouch Modern: "
                                "check the tax rates in Store Settings."));
        if (std::ranges::any_of(s.taxInclusive, [](int x) { return x != 0; }))
            out.notes.append(tr("Legacy had prices with the tax in them; ViewTouch Modern adds the tax on top: "
                                "check the prices."));

        // Discounts, comps and employee meals: ways to take money off a check.
        QSet<QString> tenderIds;
        if (current)
            for (const core::Tender &t : current->tenders)
                tenderIds.insert(QString::fromStdString(t.id));
        const auto addTender = [&](const QString &prefix, const Discount &d, std::int64_t bp, bool meal) {
            core::Tender t;
            t.id = idFrom(prefix, d.name, tenderIds);
            t.name = ss(d.name);
            t.kind = core::TenderKind::Discount;
            t.percentBp = bp;
            t.staffMeal = meal;
            out.tenders.push_back(t);
        };
        for (const Discount &d : s.media.discounts)
            if (d.active)
                addTender(u"discount-"_s, d, (d.flags & 1) ? d.amount : 0, false);
        for (const Discount &d : s.media.comps)
            if (d.active)
                addTender(u"comp-"_s, d, 10000, false);
        for (const Discount &d : s.media.meals)
            if (d.active)
                addTender(u"meal-"_s, d, (d.flags & 1) ? d.amount : 10000, true);
        QSet<QString> promoIds;
        for (const Discount &c : s.media.coupons) {
            if (!c.active)
                continue;
            core::PosSettings::Promotion p;
            p.id = idFrom(u"coupon-"_s, c.name, promoIds);
            p.name = ss(c.name);
            if (c.flags & 1)
                p.percentBp = c.amount;
            else
                p.amount = cents(c.amount);
            if (c.family >= 0 && categoryOf.contains(c.family))
                p.families.push_back(categoryOf.value(c.family));
            if (!c.itemName.isEmpty() && itemIds.contains(key(c.itemName)))
                p.items.push_back(itemIds.value(key(c.itemName)));
            if (c.startTime.isSet() && c.endTime.isSet()) {
                p.startMinute = c.startTime.time.hour() * 60 + c.startTime.time.minute();
                p.endMinute = c.endTime.time.hour() * 60 + c.endTime.time.minute();
            }
            if (c.days)
                p.days = c.days & 0x7F;
            if (c.startDate.isSet())
                p.fromDay = ss(c.startDate.date.toString(Qt::ISODate));
            if (c.endDate.isSet())
                p.untilDay = ss(c.endDate.date.toString(Qt::ISODate));
            // Legacy's coupons were touched on Pay; here, one that isn't
            // automatic takes a code: its name, without spaces.
            if (!c.automatic)
                p.code = ss(c.name.toUpper().remove(QRegularExpression(u"[^A-Z0-9]"_s)));
            out.promotions.push_back(p);
        }
        if (std::ranges::any_of(s.media.coupons, [](const Discount &c) { return c.active && !c.automatic; }))
            out.notes.append(tr("Coupons legacy took on Pay are promotions with a code here: their names, "
                                "without spaces (Store Settings > Promotions)."));
        if (!s.media.creditCards.isEmpty())
            out.notes.append(tr("Card kinds (Visa, MasterCard...) aren't tenders of their own here: a card "
                                "reader says which card it was."));

        // Printers: the network ones, where legacy sent each kind of ticket.
        for (const Printer &p : s.printers) {
            const QString id = printerId(p.type);
            if (id.isEmpty() || std::ranges::any_of(out.printers, [&](const core::PrinterConfig &c) {
                    return c.id == ss(id);
                }))
                continue;
            core::PrinterConfig c;
            c.id = ss(id);
            c.name = ss(p.name.isEmpty() ? printerName(id) : p.name);
            const bool network = p.host.contains(u'.') || p.host.contains(u':');
            c.type = network ? "network" : "none";
            c.host = ss(network ? p.host.section(u':', 0, 0) : QString());
            c.port = p.port > 0 ? p.port : 9100;
            c.receipts = id == u"receipt";
            out.printers.push_back(c);
            if (!network)
                out.notes.append(tr("Printer \"%1\" was on legacy's own computer (%2): set it up again in Store "
                                    "Settings > Printers.")
                                     .arg(QString::fromStdString(c.name), p.host));
        }
        if (!s.terminals.isEmpty())
            out.notes.append(tr("Legacy's %n screen(s) aren't brought over: pair each in ViewTouch Modern.", nullptr,
                                int(s.terminals.size())));
    }

    // --- staff --------------------------------------------------------------
    QHash<int, std::pair<std::string, std::string>> staff;   // legacy user id -> Modern id, name
    if (parts.staff || parts.history) {
        QSet<QString> pins;
        for (const Employee &e : store.employees) {
            core::Employee m;
            m.id = ss(u"legacy-%1"_s.arg(e.id));
            QString name = (e.firstName + u' ' + e.lastName).trimmed();
            if (name.isEmpty())
                name = e.systemName.trimmed();
            if (name.isEmpty())
                name = tr("Employee %1").arg(e.id);
            m.name = ss(name);
            staff.insert(e.id, {m.id, m.name});
            if (!parts.staff)
                continue;
            const QString role = e.jobs.isEmpty() ? u"server"_s : roleOf(e.jobs.first().job);
            m.role = ss(role);
            const auto rate = [&](const Job &j) {
                if (j.payRate == 1)
                    return cents(j.payAmount);
                return Money();
            };
            if (!e.jobs.isEmpty())
                m.payRate = rate(e.jobs.first());
            for (qsizetype k = 1; k < e.jobs.size(); ++k)
                m.otherJobs.push_back({ss(roleOf(e.jobs[k].job)), rate(e.jobs[k])});
            if (!e.jobs.isEmpty() && e.jobs.first().job == 7)
                m.otherJobs.push_back({"cashier", m.payRate});   // Server/Cashier: both jobs
            if (std::ranges::any_of(e.jobs, [](const Job &j) { return j.payRate != 1 && j.payAmount > 0; }))
                out.notes.append(tr("%1 was paid by the day, week or month; ViewTouch Modern pays by the hour: "
                                    "set their pay in Staff.")
                                     .arg(name));
            m.active = e.active;
            m.training = e.training;
            // Their PIN as it was: a manager's under 6 digits (or anyone's
            // under 4) logs them in once, then asks for a new one.
            const QString pin = QString::number(e.key);
            if (e.key > 0 && !pins.contains(pin)) {
                pins.insert(pin);
                m.pinSalt = app::newSalt();
                m.pinHash = app::hashPin(pin, m.pinSalt);
                if (pin.size() < 4 || (managerRole(role) && pin.size() < 6)) {
                    m.pinMustChange = true;
                    out.managersNewPin.append(name);
                }
            } else {
                m.pinMustChange = true;   // no PIN, or one someone else has: a new one at first login
                out.managersNewPin.append(name);
            }
            out.employees.push_back(m);
        }
    }

    // --- customers ----------------------------------------------------------
    if (parts.customers) {
        for (const Customer &c : store.customers) {
            core::CustomerRecord r;
            r.id = ss(u"legacy-%1"_s.arg(c.id));
            QString name = (c.firstName + u' ' + c.lastName).trimmed();
            if (!c.company.trimmed().isEmpty())
                name = name.isEmpty() ? c.company.trimmed() : name + u" (" + c.company.trimmed() + u')';
            r.name = ss(name);
            r.phone = ss(c.extension.isEmpty() ? c.phone : c.phone + u" x" + c.extension);
            QStringList address;
            for (const QString &l : {c.address, c.address2})
                if (!l.trimmed().isEmpty())
                    address.append(l.trimmed());
            const QString place = QStringList{c.city.trimmed(), (c.state + u' ' + c.postal).trimmed()}
                                      .filter(QRegularExpression(u"."_s))
                                      .join(u", "_s);
            if (!place.isEmpty())
                address.append(place);
            r.address = ss(address.join(u", "_s));
            QStringList note;
            if (!c.crossStreet.trimmed().isEmpty())
                note.append(tr("Cross street: %1").arg(c.crossStreet.trimmed()));
            if (!c.comment.trimmed().isEmpty())
                note.append(c.comment.trimmed());
            r.note = ss(note.join(u"; "_s));
            if (r.name.empty() && r.phone.empty() && r.address.empty())
                continue;
            out.customers.push_back(r);
        }
    }

    // --- pages and the dining room -------------------------------------------
    if (store.pages && parts.pages) {
        PageOptions options;
        options.defaults = store.pages->defaults.value_or(PageDefaults{});
        options.item = [&](const QString &name) {
            return QString::fromStdString(itemIds.value(key(name)));   // empty: not on its menu
        };
        // Legacy's system pages are the Legacy style's: the store's pages are
        // framed by them and go to them (its order screen, Pay, Manager...).
        for (const layout::Page &p : layout::Layout::stylePages(u"legacy"_s))
            if (const int lid = p.extra.value(u"legacyId"_s).toInt(); lid < 0)
                options.knownPages.insert(lid);
        // A store that edited its system pages kept them (vt_data): those, in place of legacy's.
        QList<const PageFile *> files;
        if (store.vtData)
            files.append(&*store.vtData);
        files.append(&*store.pages);
        PageSet set = convertPages(files, options);
        out.pages = set.pages;
        out.notInModern = set.missing;
        if (store.vtData)
            out.notes.append(tr("This store's own system pages (vt_data) are used in place of legacy's."));
    }
    if (store.tables && parts.tables) {
        QJsonArray floor;
        for (const Page &p : store.tables->pages) {
            for (const Zone &z : p.zones) {
                if (z.kind != u"ZONE_TABLE")
                    continue;
                const QString label = z.name.trimmed();
                if (label.isEmpty())
                    continue;
                floor.append(QJsonObject{
                    {u"id"_s, u"table-%1"_s.arg(label.toLower().replace(QRegularExpression(u"[^a-z0-9]+"_s), u"-"_s))},
                    {u"kind"_s, u"table"_s},
                    {u"label"_s, label},
                    {u"shape"_s, z.shape == 3 ? u"circle"_s : u"rect"_s},
                    {u"rect"_s, QJsonObject{{u"x"_s, z.x}, {u"y"_s, z.y}, {u"w"_s, z.w}, {u"h"_s, z.h}}},
                    {u"legacyPage"_s, p.id}});
            }
        }
        if (!floor.isEmpty())
            out.floor = floor;
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
        std::vector<WorkEntry> work;
        for (const LaborPeriod &p : store.labor)
            for (const WorkEntry &w : p.entries)
                if (w.start.isSet() && w.end.isSet())
                    work.push_back(w);
        std::ranges::sort(work, {}, [&](const WorkEntry &w) { return toEpoch(w.start, cx.zone); });
        std::size_t nextWork = 0;
        for (const Archive &a : store.archives) {
            ImportedDay day;
            day.legacyId = a.id;
            day.closedAt = toEpoch(a.end, cx.zone);
            day.openedAt = a.start.isSet() ? toEpoch(a.start, cx.zone) : previousEnd;
            core::TaxRates rates = out.tax.value_or(current ? current->tax : core::TaxRates{});
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
                s.openedBy = staff.value(d.owner).first;
                s.closedBy = staff.value(d.puller).first;
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
            // The day's time clock: shifts that began in it.
            while (nextWork < work.size() && toEpoch(work[nextWork].start, cx.zone) < day.closedAt) {
                const WorkEntry &w = work[nextWork++];
                if (toEpoch(w.start, cx.zone) < day.openedAt - 24LL * 3'600'000)
                    continue;   // long before this day: no day of its own here
                core::TimePunch p;
                p.employeeId = staff.value(w.userId).first;
                if (p.employeeId.empty())
                    continue;
                p.clockIn = toEpoch(w.start, cx.zone);
                p.clockOut = toEpoch(w.end, cx.zone);
                p.job = ss(roleOf(w.job));
                p.rate = w.payRate == 1 ? cents(w.payAmount) : Money();
                if (w.tips > 0) {
                    p.cashTipsDeclared = true;
                    p.cashTips = cents(w.tips);
                }
                day.punches.push_back(p);
            }
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
                    {u"storeName"_s, in.storeName.value_or(QString())},
                    {u"pages"_s, in.pages.size()},
                    {u"items"_s, qint64(in.menu.size())},
                    {u"categories"_s, qint64(in.categories.size())},
                    {u"tables"_s, in.floor ? in.floor->size() : 0},
                    {u"staff"_s, qint64(in.employees.size())},
                    {u"managersNewPin"_s, in.managersNewPin},
                    {u"customers"_s, qint64(in.customers.size())},
                    {u"days"_s, qint64(in.days.size())},
                    {u"checks"_s, qint64(checks)},
                    {u"sales"_s, qint64(sales.cents())},
                    {u"mismatches"_s, qint64(in.mismatches.size())},
                    {u"notInModern"_s, in.notInModern.size()},
                    {u"notes"_s, in.notes},
                    {u"errors"_s, in.errors}};
    if (!in.days.empty()) {
        out.insert(u"from"_s, dayText(in.days.front().openedAt ? in.days.front().openedAt : in.days.front().closedAt));
        out.insert(u"to"_s, dayText(in.days.back().closedAt));
    }
    return out;
}

} // namespace vt::legacy
