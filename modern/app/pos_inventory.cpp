// PosService: inventory - what each line uses up (recipes), stock taken
// when orders go to the kitchen and given back on a void, items sold out by
// themselves when their stock runs short, low-stock alerts, food cost.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// How much of a modifier's recipe a qualifier means: none of it for "No",
// twice for "Extra"/"Double", half for "Lite".
double qualifierFactor(Qualifier q)
{
    switch (q) {
    case Qualifier::No: return 0;
    case Qualifier::Extra:
    case Qualifier::Double: return 2;
    case Qualifier::Lite: return 0.5;
    default: return 1;
    }
}

QString amountText(double v)
{
    return QString::number(std::round(v * 100) / 100, 'g', 8);
}

} // namespace

Ingredient *PosShared::ingredient(const std::string &id)
{
    auto it = std::ranges::find_if(ingredients, [&](const Ingredient &i) { return i.id == id; });
    return it == ingredients.end() ? nullptr : &*it;
}

std::map<std::string, double> PosService::stockUse(const OrderLine &l) const
{
    std::map<std::string, double> use;
    if (l.isComment() || l.isGiftCard())
        return use;
    const auto add = [&](const MenuItem *m, double times) {
        if (!m)
            return;
        for (const RecipeLine &r : m->recipe)
            use[r.ingredientId] += r.quantity * times;
    };
    // By weight: the recipe is per unit (a pound of brisket), times the weight.
    const double qty = l.counted() * (l.weight > 0 ? l.weight / 1000.0 : 1.0);
    add(findItem(qs(l.itemId)), qty * (l.qualifier == Qualifier::No ? 0 : 1));
    for (const Modifier &m : l.modifiers) {
        if (!m.itemId.empty())
            add(findItem(qs(m.itemId)), qty * qualifierFactor(m.qualifier));
    }
    return use;
}

void PosService::takeStock(const std::vector<OrderLine> &lines, int sign)
{
    std::map<std::string, double> total;
    for (const OrderLine &l : lines) {
        if (l.voided && sign > 0)
            continue;
        for (const auto &[id, q] : stockUse(l))
            total[id] += q;
    }
    if (total.empty())
        return;
    QStringList nowLow;
    for (int i = 0; i < int(s_->ingredients.size()); ++i) {
        Ingredient &g = s_->ingredients[i];
        const auto it = total.find(g.id);
        if (it == total.end() || it->second == 0)
            continue;
        const bool wasLow = g.low();
        g.onHand -= it->second * sign;
        g.sold += it->second * sign;
        if (!wasLow && g.low())
            nowLow << tr("%1 (%2 %3 left)").arg(qs(g.name), amountText(g.onHand), qs(g.unit));
        if (s_->sink)
            s_->sink->saveIngredient(g, i);
    }
    if (!nowLow.isEmpty())
        emit notice(tr("Running low: %1").arg(nowLow.join(u", "_s)));
    refreshSoldOut();
    emit s_->dayChanged();   // "5 left" on the buttons
}

QVariantMap PosService::stockLeft() const
{
    QVariantMap out;
    for (const MenuItem &m : s_->menu) {
        if (m.recipe.empty() || !m.available)
            continue;
        bool low = false;
        double left = -1;
        for (const RecipeLine &r : m.recipe) {
            const Ingredient *g = s_->ingredient(r.ingredientId);
            if (!g || r.quantity <= 0)
                continue;
            low = low || g->low();
            const double n = std::floor((g->onHand + 1e-9) / r.quantity);
            left = left < 0 ? n : std::min(left, n);
        }
        if (low && left > 0) {
            out.insert(qs(m.id), int(left));
            out.insert(qs(m.name).toLower(), int(left));
        }
    }
    return out;
}

void PosService::refreshSoldOut()
{
    QStringList out, back;
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (m.recipe.empty())
            continue;
        bool shortOf = false;
        for (const RecipeLine &r : m.recipe) {
            const Ingredient *g = s_->ingredient(r.ingredientId);
            if (g && r.quantity > 0 && g->onHand + 1e-9 < r.quantity)
                shortOf = true;
        }
        if (shortOf && m.available) {
            m.available = false;
            m.autoSoldOut = true;
            out << qs(m.name);
        } else if (!shortOf && m.autoSoldOut) {
            m.available = true;
            m.autoSoldOut = false;
            back << qs(m.name);
        } else {
            continue;
        }
        if (s_->sink)
            s_->sink->saveMenuItem(m, i);
    }
    if (out.isEmpty() && back.isEmpty())
        return;
    if (!out.isEmpty())
        emit notice(tr("Out of stock, now sold out: %1").arg(out.join(u", "_s)));
    if (!back.isEmpty())
        emit notice(tr("Back in stock: %1").arg(back.join(u", "_s)));
    ++s_->adminRevision;
    emit s_->adminChanged();
}

Money PosService::lineCost(const OrderLine &l) const
{
    double cents = 0;
    for (const auto &[id, q] : stockUse(l))
        if (const Ingredient *g = const_cast<PosShared *>(s_)->ingredient(id))
            cents += q * double(g->cost.cents());
    return Money::fromCents(std::llround(cents));
}

Report PosService::foodCostReport(const std::vector<Check> &closed, const ReportContext &ctx) const
{
    Report r;
    r.id = "foodcost";
    r.title = "Food Cost";
    r.subtitle = ctx.period;
    r.columns = {"Item", "Sold", "Sales", "Cost", "Cost %"};

    struct Row { std::string name; int sold = 0; Money sales, cost; };
    std::map<std::string, Row> rows;
    const auto costOf = [&](const OrderLine &l) { return lineCost(l); };
    for (const Check &c : closed) {
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment() || l.isGiftCard())
                continue;
            Row &row = rows[l.itemId];
            row.name = l.name;
            row.sold += l.counted();
            row.sales += l.total();
            row.cost += costOf(l);
        }
    }
    const auto percent = [](Money part, Money whole) {
        return whole.cents() > 0 ? QString::number(100.0 * double(part.cents()) / double(whole.cents()), 'f', 1).toStdString() + "%"
                                 : std::string("-");
    };
    std::vector<const Row *> sorted;
    for (const auto &[id, row] : rows)
        sorted.push_back(&row);
    std::ranges::sort(sorted, [](const Row *a, const Row *b) { return a->sales > b->sales; });
    Money sales, cost;
    if (sorted.empty())
        r.note("Nothing sold.");
    else
        r.section("Sold");
    for (const Row *row : sorted) {
        r.line({row->name, std::to_string(row->sold), ctx.money(row->sales),
                row->cost.cents() ? ctx.money(row->cost) : "-", row->cost.cents() ? percent(row->cost, row->sales) : "-"});
        sales += row->sales;
        cost += row->cost;
    }
    if (!sorted.empty())
        r.total({"All", "", ctx.money(sales), ctx.money(cost), percent(cost, sales)});

    r.section("Stock");
    bool any = false;
    for (const Ingredient &g : s_->ingredients) {
        any = true;
        r.line({g.name + (g.onHand <= 0 ? "  (out)" : g.low() ? "  (low)" : ""), "",
                amountText(g.onHand).toStdString() + " " + g.unit, "", ""});
    }
    if (!any)
        r.note("No ingredients yet (Manager -> Inventory).");
    return r;
}

// Menu engineering: in each category, every item rated by how well it sells
// (its share of the category's count against 70% of an even share) and how
// much each one earns (price less food cost, against the category's
// average). Stars sell and earn; the rest say what to try.
Report PosService::menuEngineeringReport(const std::vector<Check> &closed, const ReportContext &ctx) const
{
    Report r;
    r.id = "engineering";
    r.title = "Menu Mix";
    r.subtitle = ctx.period;
    r.columns = {"Item", "Sold", "Of category", "Earns each", "Earned", "Rating"};

    struct Row { std::string name; std::int64_t sold = 0; Money sales, cost; bool costed = false; };
    std::map<std::string, std::map<std::string, Row>> families;   // family -> item id -> row
    // Everything on the menu, sold or not (a slow item is one not sold).
    for (const MenuItem &m : s_->menu) {
        if (m.isModifier || m.family.empty() || m.id.starts_with("giftcard"))
            continue;
        Row &row = families[m.family][m.id];
        row.name = m.name;
        row.costed = !m.recipe.empty();
    }
    for (const Check &c : closed) {
        if (c.training)
            continue;
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment() || l.isGiftCard() || l.isFee())
                continue;
            const MenuItem *m = findItem(qs(l.itemId));
            if (!m || m->isModifier)
                continue;
            Row &row = families[m->family.empty() ? std::string("other") : m->family][l.itemId];
            if (row.name.empty())
                row.name = l.name;
            row.costed = row.costed || !m->recipe.empty();
            row.sold += l.counted();
            row.sales += l.total();
            row.cost += lineCost(l);
        }
    }
    std::map<std::string, std::string> names;
    for (const MenuCategory &c : s_->categories())
        names[c.id] = c.name;

    const auto percent = [](double v) { return QString::number(v, 'f', 1).toStdString() + "%"; };
    bool anySold = false;
    bool uncosted = false;
    for (const auto &[family, items] : families) {
        std::int64_t sold = 0;
        Money earned;
        for (const auto &[id, row] : items) {
            sold += row.sold;
            earned += row.sales - row.cost;
        }
        if (sold == 0)
            continue;   // nothing sold in it: nothing to compare
        anySold = true;
        const double popular = 0.7 / double(items.size());       // the share that counts as selling well
        const double average = double(earned.cents()) / double(sold);   // earned per item sold, in cents
        std::vector<std::pair<std::string, const Row *>> sorted;
        for (const auto &[id, row] : items)
            sorted.emplace_back(id, &row);
        std::ranges::sort(sorted, [](const auto &a, const auto &b) {
            return (a.second->sales - a.second->cost) > (b.second->sales - b.second->cost);
        });
        const auto it = names.find(family);
        std::string title = it != names.end() ? it->second : family;
        if (!title.empty())
            title[0] = char(std::toupper(static_cast<unsigned char>(title[0])));
        r.section(title);
        for (const auto &[id, row] : sorted) {
            const double mix = double(row->sold) / double(sold);
            // Each one: what it sold for, else what it sells for now.
            Money each;
            if (row->sold > 0) {
                each = Money::fromCents((row->sales - row->cost).cents() / row->sold);
            } else if (const MenuItem *m = findItem(qs(id))) {
                OrderLine one;
                one.itemId = m->id;
                one.quantity = 1;
                each = m->price - lineCost(one);
            }
            const bool sells = mix >= popular;
            const bool earns = double(each.cents()) >= average;
            const std::string rating = sells && earns ? "Star" : sells ? "Workhorse" : earns ? "Hidden gem" : "Weak";
            if (!row->costed)
                uncosted = true;
            r.line({row->name + (row->costed ? "" : " *"), std::to_string(row->sold), percent(100.0 * mix), ctx.money(each),
                    ctx.money(row->sales - row->cost), rating});
        }
        r.total({"All", std::to_string(sold), "100.0%", ctx.money(Money::fromCents(std::llround(average))),
                 ctx.money(earned), ""});
    }
    if (!anySold)
        r.note("Nothing sold.");
    else {
        r.note("Star: sells well and earns well. Keep it as it is.");
        r.note("Workhorse: sells well, earns little. Raise the price a little, or make it for less.");
        r.note("Hidden gem: earns well, sells slowly. Show it off: a picture, the top of the page, suggest it.");
        r.note("Weak: sells slowly and earns little. Change it, or take it off the menu.");
        r.note("Sells well: at least 70% of an even share of its category. Earns well: at least the category's "
               "average, after food cost.");
        if (uncosted)
            r.note("* No recipe: its food cost isn't counted (Manager -> Inventory).");
    }
    return r;
}

QVariantList PosService::lowStock() const
{
    QVariantList out;
    for (const Ingredient &g : s_->ingredients) {
        if (g.low())
            out.append(QVariantMap{{u"name"_s, qs(g.name)}, {u"onHand"_s, amountText(g.onHand)},
                                   {u"unit"_s, qs(g.unit)}, {u"out"_s, g.onHand <= 0}});
    }
    return out;
}

// --- deliveries ------------------------------------------------------------------------

bool PosService::receiveDelivery(const QVariantMap &r)
{
    if (!require(perm::Manager, tr("Receiving deliveries")))
        return false;
    Delivery d;
    const QString vendorId = r.value(u"vendor"_s).toString();
    if (!vendorId.isEmpty()) {
        const auto v = std::ranges::find_if(s_->settings.vendors, [&](const Vendor &x) { return qs(x.id) == vendorId; });
        if (v == s_->settings.vendors.end())
            return fail(tr("There is no vendor '%1'.").arg(vendorId));
        d.vendorId = v->id;
        d.vendorName = v->name;
    }
    d.invoice = ss(r.value(u"invoice"_s).toString().trimmed());
    for (const QVariant &v : r.value(u"lines"_s).toList()) {
        const QVariantMap l = v.toMap();
        const double qty = l.value(u"qty"_s).toDouble();
        if (qty == 0)
            continue;
        const auto g = std::ranges::find_if(s_->ingredients, [&](const Ingredient &x) { return qs(x.id) == l.value(u"ingredient"_s).toString(); });
        if (g == s_->ingredients.end())
            return fail(tr("There is no ingredient '%1' (see Manager → Inventory).").arg(l.value(u"ingredient"_s).toString()));
        if (qty < 0)
            return fail(tr("Amounts received can't be negative: %1.").arg(qs(g->name)));
        const double cost = l.contains(u"cost"_s) ? l.value(u"cost"_s).toDouble() : double(g->cost.cents()) / 100.0;
        if (cost < 0)
            return fail(tr("Costs can't be negative: %1.").arg(qs(g->name)));
        d.lines.push_back({g->id, g->name, g->unit, qty, Money::fromCents(std::llround(cost * 100))});
    }
    if (d.lines.empty())
        return fail(tr("Type how much came of each item."));
    // On the shelf now, at the latest cost.
    for (const Delivery::Line &l : d.lines) {
        const auto g = std::ranges::find_if(s_->ingredients, [&](const Ingredient &x) { return x.id == l.ingredientId; });
        g->onHand += l.qty;
        g->received += l.qty;
        if (l.unitCost.cents() > 0)
            g->cost = l.unitCost;
        if (s_->sink)
            s_->sink->saveIngredient(*g, int(g - s_->ingredients.begin()));
    }
    d.id = ++s_->lastDeliveryId;
    d.at = now();
    d.by = user()->name;
    s_->deliveries.push_back(d);
    if (s_->sink)
        s_->sink->saveDelivery(d);
    refreshSoldOut();   // what was out is back
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(d.vendorName.empty()
                    ? tr("Received %n item(s): %1", "", int(d.lines.size())).arg(format(d.total()))
                    : tr("Received %n item(s) from %1: %2", "", int(d.lines.size())).arg(qs(d.vendorName), format(d.total())));
    return true;
}

QVariantMap PosService::receiving() const
{
    if (!can(u"manager"_s))
        return {};
    QVariantList vendors;
    for (const Vendor &v : s_->settings.vendors)
        vendors.append(QVariantMap{{u"id"_s, qs(v.id)}, {u"name"_s, qs(v.name)}});
    QVariantList ingredients;
    for (const Ingredient &g : s_->ingredients)
        ingredients.append(QVariantMap{{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"unit"_s, qs(g.unit)},
                                       {u"onHand"_s, g.onHand}, {u"cost"_s, double(g.cost.cents()) / 100.0},
                                       {u"vendor"_s, qs(g.vendor)}, {u"low"_s, g.low()}});
    QVariantList recent;
    for (auto it = s_->deliveries.rbegin(); it != s_->deliveries.rend() && recent.size() < 15; ++it)
        recent.append(QVariantMap{{u"when"_s, i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(it->at), u"ddd M/d h:mm AP"_s)},
                                  {u"vendor"_s, qs(it->vendorName)}, {u"invoice"_s, qs(it->invoice)},
                                  {u"items"_s, int(it->lines.size())}, {u"total"_s, format(it->total())},
                                  {u"by"_s, qs(it->by)}});
    return {{u"vendors"_s, vendors}, {u"ingredients"_s, ingredients}, {u"recent"_s, recent}};
}

// --- stock counts ------------------------------------------------------------------------

QVariantMap PosService::stockCounting() const
{
    if (!can(u"manager"_s))
        return {};
    const bool show = s_->settings.stockCountShowsExpected;
    const auto date = [](std::int64_t at) {
        return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(at), u"ddd M/d h:mm AP"_s);
    };
    QVariantList vendors;
    for (const Vendor &v : s_->settings.vendors)
        vendors.append(QVariantMap{{u"id"_s, qs(v.id)}, {u"name"_s, qs(v.name)}});
    QVariantList ingredients;
    for (const Ingredient &g : s_->ingredients) {
        QVariantMap m{{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"unit"_s, qs(g.unit)},
                      {u"vendor"_s, qs(g.vendor)},
                      {u"lastCounted"_s, g.countedAt ? date(g.countedAt) : tr("never")}};
        if (show)
            m.insert(u"expected"_s, amountText(g.onHand));
        ingredients.append(m);
    }
    QVariantList recent;
    for (auto it = s_->stockCounts.rbegin(); it != s_->stockCounts.rend() && recent.size() < 15; ++it)
        recent.append(QVariantMap{{u"when"_s, date(it->at)}, {u"by"_s, qs(it->by)},
                                  {u"items"_s, int(it->lines.size())}, {u"value"_s, format(it->value())},
                                  {u"short"_s, it->value().cents() < 0}});
    return {{u"vendors"_s, vendors}, {u"ingredients"_s, ingredients}, {u"showExpected"_s, show},
            {u"recent"_s, recent}};
}

bool PosService::saveStockCount(const QVariantMap &r)
{
    if (!require(perm::Manager, tr("Counting stock")))
        return false;
    StockCount count;
    for (const QVariant &v : r.value(u"lines"_s).toList()) {
        const QVariantMap l = v.toMap();
        const QVariant typed = l.value(u"counted"_s);
        if (!typed.isValid() || typed.toString().trimmed().isEmpty())
            continue;   // not counted
        bool ok = false;
        const double counted = typed.toString().trimmed().replace(u',', u'.').toDouble(&ok);
        const Ingredient *g = s_->ingredient(ss(l.value(u"ingredient"_s).toString()));
        if (!g)
            return fail(tr("There is no ingredient '%1' (see Manager → Inventory).").arg(l.value(u"ingredient"_s).toString()));
        if (!ok || counted < 0)
            return fail(tr("Type how many there are of %1 (0 or more).").arg(qs(g->name)));
        count.lines.push_back({g->id, g->name, g->unit, g->countedAt, g->start, g->received, g->sold, g->adjusted,
                               g->onHand, counted, g->cost, g->wasted});
    }
    if (count.lines.empty())
        return fail(tr("Type how many there are of at least one item."));
    count.id = ++s_->lastStockCountId;
    count.at = now();
    count.by = user()->name;
    // What was counted is what there is, from now on.
    for (const StockCount::Line &l : count.lines) {
        const auto g = std::ranges::find(s_->ingredients, l.ingredientId, &Ingredient::id);
        g->onHand = g->start = l.counted;
        g->countedAt = count.at;
        g->received = g->sold = g->wasted = g->adjusted = 0;
        if (s_->sink)
            s_->sink->saveIngredient(*g, int(g - s_->ingredients.begin()));
    }
    s_->stockCounts.push_back(count);
    if (s_->stockCounts.size() > 30)
        s_->stockCounts.erase(s_->stockCounts.begin());
    if (s_->sink)
        s_->sink->saveStockCount(count);
    refreshSoldOut();
    ++s_->adminRevision;
    emit s_->adminChanged();
    const Money value = count.value();
    emit notice(value.cents() < 0 ? tr("Counted %n item(s): %1 missing", "", int(count.lines.size())).arg(format(-value))
                : value.cents() > 0 ? tr("Counted %n item(s): %1 more than expected", "", int(count.lines.size())).arg(format(value))
                                    : tr("Counted %n item(s): as expected", "", int(count.lines.size())));
    return true;
}

// --- waste -------------------------------------------------------------------------------

QVariantMap PosService::wasteLog() const
{
    if (!user())
        return {};
    QVariantList items;
    for (const MenuItem &m : s_->menu)
        if (!m.recipe.empty())
            items.append(QVariantMap{{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}});
    QVariantList ingredients;
    for (const Ingredient &g : s_->ingredients)
        ingredients.append(QVariantMap{{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"unit"_s, qs(g.unit)}});
    // The starting reasons, in the screen's language (a store's own as typed).
    static constexpr const char *kStarting[] = {
        QT_TR_NOOP("Dropped or spilled"), QT_TR_NOOP("Expired or spoiled"), QT_TR_NOOP("Made wrong"),
        QT_TR_NOOP("Sent back by a guest"), QT_TR_NOOP("Overcooked or burnt"),
    };
    QStringList reasons;
    for (const std::string &r : s_->settings.wasteReasons)
        reasons << (std::ranges::find(kStarting, r, [](const char *k) { return std::string(k); }) != std::end(kStarting)
                        ? tr(r.c_str()) : qs(r));
    QVariantList today;
    for (auto it = s_->waste.rbegin(); it != s_->waste.rend() && it->at >= s_->day.openedAt; ++it)
        today.append(QVariantMap{{u"when"_s, i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(it->at), u"h:mm AP"_s)},
                                 {u"what"_s, qs(it->what)},
                                 {u"qty"_s, amountText(it->qty) + (it->unit.empty() ? QString() : u" "_s + qs(it->unit))},
                                 {u"reason"_s, qs(it->reason)}, {u"by"_s, qs(it->by)}, {u"value"_s, format(it->value())}});
    return {{u"items"_s, items}, {u"ingredients"_s, ingredients}, {u"reasons"_s, reasons}, {u"today"_s, today}};
}

bool PosService::logWaste(const QVariantMap &r)
{
    if (!require(perm::Order, tr("Logging waste")))
        return false;
    bool ok = false;
    const double qty = r.value(u"qty"_s, 1).toString().trimmed().replace(u',', u'.').toDouble(&ok);
    if (!ok || qty <= 0)
        return fail(tr("Type how much was thrown away."));
    const QString reason = r.value(u"reason"_s).toString().trimmed();
    if (reason.isEmpty())
        return fail(tr("Choose why it was thrown away."));
    Waste w;
    w.qty = qty;
    w.reason = ss(reason.left(80));
    std::map<std::string, double> use;
    if (const QString itemId = r.value(u"item"_s).toString(); !itemId.isEmpty()) {
        const MenuItem *m = findItem(itemId);
        if (!m || m->recipe.empty())
            return fail(tr("%1 has no recipe to take off the shelf (Menu Builder).").arg(m ? qs(m->name) : itemId));
        w.what = m->name;
        for (const RecipeLine &l : m->recipe)
            use[l.ingredientId] += l.quantity * qty;
    } else {
        const Ingredient *g = s_->ingredient(ss(r.value(u"ingredient"_s).toString()));
        if (!g)
            return fail(tr("Choose what was thrown away."));
        w.what = g->name;
        w.unit = g->unit;
        use[g->id] = qty;
    }
    for (const auto &[id, q] : use) {
        const auto g = std::ranges::find(s_->ingredients, id, &Ingredient::id);
        if (g == s_->ingredients.end())
            continue;
        g->onHand -= q;
        g->wasted += q;
        w.lines.push_back({g->id, g->name, g->unit, q, g->cost});
        if (s_->sink)
            s_->sink->saveIngredient(*g, int(g - s_->ingredients.begin()));
    }
    w.id = ++s_->lastWasteId;
    w.at = now();
    w.by = user()->name;
    s_->waste.push_back(w);
    std::erase_if(s_->waste, [&](const Waste &x) { return x.at < now() - 60LL * 24 * 3'600'000; });
    if (s_->sink)
        s_->sink->saveWaste(w);
    refreshSoldOut();
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(tr("Logged: %1 %2 (%3), %4").arg(amountText(qty), qs(w.what), reason, format(w.value())));
    return true;
}

// The Stock report's other parts: today's waste, and what to order (below par).
void PosService::stockExtras(Report &r, const ReportContext &ctx) const
{
    std::vector<const Waste *> today;
    for (const Waste &w : s_->waste)
        if (w.at >= s_->day.openedAt)
            today.push_back(&w);
    if (!today.empty()) {
        r.section("Thrown away today");
        Money total;
        for (const Waste *w : today) {
            r.line({ctx.clock(w->at) + " · " + w->what + " · " + w->reason + " · " + w->by,
                    amountText(w->qty).toStdString() + (w->unit.empty() ? "" : " " + w->unit), "", "",
                    ctx.money(-w->value())});
            total += w->value();
        }
        r.total({"All", "", "", "", ctx.money(-total)});
    }
    std::map<std::string, std::vector<const Ingredient *>> byVendor;
    for (const Ingredient &g : s_->ingredients)
        if (g.par > 0 && g.onHand < g.par)
            byVendor[g.vendor].push_back(&g);
    for (const auto &[vendor, list] : byVendor) {
        const auto v = std::ranges::find(s_->settings.vendors, vendor, &Vendor::id);
        r.section("To order: " + (v == s_->settings.vendors.end() ? std::string("(no vendor)") : v->name)
                  + " (on hand, par, order)");
        Money total;
        for (const Ingredient *g : list) {
            const double order = std::ceil(g->par - g->onHand - 1e-9);
            const Money cost = Money::fromCents(std::llround(double(g->cost.cents()) * order));
            r.line({g->name + " (" + g->unit + ")", amountText(g->onHand).toStdString(), amountText(g->par).toStdString(),
                    amountText(order).toStdString(), ctx.money(cost)});
            total += cost;
        }
        r.total({"About", "", "", "", ctx.money(total)});
    }
}

Report PosService::stockCountReport(const ReportContext &ctx) const
{
    Report r;
    r.id = "stock";
    r.title = "Stock Count";
    r.subtitle = ctx.period;
    r.columns = {"", "Should be", "Counted", "Off by", "Cost"};
    std::vector<const StockCount *> counts;
    for (const StockCount &c : s_->stockCounts)
        if (c.at >= s_->day.openedAt)
            counts.push_back(&c);
    const bool earlier = counts.empty() && !s_->stockCounts.empty();
    if (earlier)
        counts.push_back(&s_->stockCounts.back());   // none today: the latest one
    if (counts.empty()) {
        r.note("No stock counted yet (Manager -> Inventory -> Count Stock).");
        stockExtras(r, ctx);
        return r;
    }
    const auto amount = [](double v) { return amountText(v).toStdString(); };
    for (const StockCount *c : counts) {
        const QString when = earlier ? i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(c->at), u"ddd M/d h:mm AP"_s)
                                     : QString::fromStdString(ctx.clock(c->at));
        r.section(when.toStdString() + " · " + c->by);
        std::vector<const StockCount::Line *> lines;
        for (const StockCount::Line &l : c->lines)
            lines.push_back(&l);
        // The biggest losses first.
        std::ranges::stable_sort(lines, [](const auto *a, const auto *b) { return a->value().cents() < b->value().cents(); });
        for (const StockCount::Line *l : lines) {
            const double off = std::round(l->off() * 100) / 100;
            r.line({l->name + " (" + l->unit + ")", amount(l->expected), amount(l->counted),
                    off == 0 ? std::string("0") : (off > 0 ? "+" : "") + amount(off), ctx.money(l->value())});
        }
        if (c->wasteValue().cents() != 0)
            r.total({"Thrown away (logged)", "", "", "", ctx.money(-c->wasteValue())});
        r.total({"Missing or extra", "", "", "", ctx.money(c->value())});
    }
    stockExtras(r, ctx);
    r.note("Should be: last count + deliveries - sales (by recipe) - waste logged + changes by hand.");
    r.note("Less than that: unlogged waste, big portions, theft, or a recipe that's off.");
    return r;
}

Report PosService::purchasesReport(const ReportContext &ctx) const
{
    Report r;
    r.id = "purchases";
    r.title = "Purchases";
    r.subtitle = ctx.period;
    r.columns = {"", "Items", "Invoice", "Total"};
    std::vector<const Delivery *> today;
    for (const Delivery &d : s_->deliveries)
        if (d.at >= s_->day.openedAt)
            today.push_back(&d);
    if (today.empty()) {
        r.note("No deliveries received today (Manager -> Inventory -> Receive a Delivery).");
        return r;
    }
    std::map<std::string, Money> byVendor;
    Money total;
    for (const Delivery *d : today) {
        byVendor[d->vendorName.empty() ? "(no vendor)" : d->vendorName] += d->total();
        total += d->total();
    }
    r.section("By vendor");
    for (const auto &[vendor, amount] : byVendor)
        r.line({vendor, "", "", ctx.money(amount)});
    r.total({"Total received", "", "", ctx.money(total)});
    for (const Delivery *d : today) {
        r.section((d->vendorName.empty() ? std::string("Delivery") : d->vendorName) + ", " + ctx.clock(d->at) + ", " + d->by);
        for (const Delivery::Line &l : d->lines) {
            char qty[32];
            std::snprintf(qty, sizeof qty, "%g %s", l.qty, l.unit.c_str());
            r.line({l.name, qty, ctx.money(l.unitCost) + " each", ctx.money(l.total())});
        }
        r.total({"Total", "", d->invoice.empty() ? "" : "#" + d->invoice, ctx.money(d->total())});
    }
    return r;
}

} // namespace vt::app
