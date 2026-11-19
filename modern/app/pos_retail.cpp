// PosService: a shop's price check (scan or touch an item: its price and
// how many are in stock, nothing rung in) and items with sizes and colors
// (a grid to choose from, each with its own barcode, price and stock).

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <optional>

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

QString PosService::variantLabel(const std::string &key) const
{
    return qs(key).split(u'|').join(u", "_s);
}

double PosService::variantStock(const MenuItem &m, const std::string &key) const
{
    const Ingredient *g = const_cast<PosShared *>(s_)->ingredient(variantStockId(m.id, key));
    return g ? g->onHand : -1;
}

std::pair<const MenuItem *, std::string> PosService::findVariantCode(const QString &code) const
{
    const std::string c = ss(code.trimmed());
    if (c.empty())
        return {nullptr, {}};
    for (const MenuItem &m : s_->menu)
        for (const MenuItem::Variant &v : m.variants)
            if ((!v.barcode.empty() && sameBarcode(v.barcode, c)) || (!v.sku.empty() && v.sku == c))
                return {&m, v.key};
    return {nullptr, {}};
}

QVariantList PosService::variantCells(const MenuItem &m) const
{
    const std::string period = currentMealPeriod();
    QVariantList cells;
    for (const MenuItem::Variant &v : m.variants) {
        const double stock = variantStock(m, v.key);
        cells.append(QVariantMap{{u"key"_s, qs(v.key)}, {u"label"_s, variantLabel(v.key)},
                                 {u"values"_s, qs(v.key).split(u'|')},
                                 {u"price"_s, format(v.price.cents() > 0 ? v.price : m.priceDuring(period))},
                                 {u"stock"_s, stock < 0 ? QVariant() : QVariant(int(std::floor(stock + 1e-9)))},
                                 {u"hidden"_s, v.hidden}, {u"barcode"_s, qs(v.barcode)}, {u"sku"_s, qs(v.sku)},
                                 {u"priceValue"_s, v.price.cents() > 0 ? QString::number(double(v.price.cents()) / 100.0, 'f', 2) : QString()}});
    }
    return cells;
}

QVariantList PosService::variantAxesOf(const MenuItem &m) const
{
    QVariantList axes;
    for (const MenuItem::VariantAxis &a : m.variantAxes) {
        QStringList values;
        for (const std::string &v : a.values)
            values << qs(v);
        axes.append(QVariantMap{{u"name"_s, qs(a.name)}, {u"values"_s, values}});
    }
    return axes;
}

// --- sizes and colors, rung in ----------------------------------------------------------

QVariantMap PosService::variantAsk() const
{
    if (variantAsk_.isEmpty())
        return {};
    const MenuItem *m = findItem(variantAsk_);
    if (!m || m->variants.empty())
        return {};
    return {{u"item"_s, qs(m->id)}, {u"name"_s, qs(m->name)}, {u"axes"_s, variantAxesOf(*m)}, {u"cells"_s, variantCells(*m)}};
}

bool PosService::addVariant(const QString &itemId, const QString &key)
{
    const MenuItem *m = findItem(itemId);
    if (!m)
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    const MenuItem::Variant *v = m->variant(ss(key));
    if (m->byWeight)
        return fail(tr("Sizes and colors are for items sold one at a time."));
    if (!v || v->hidden)
        return fail(tr("%1 doesn't come in %2.").arg(qs(m->name), variantLabel(ss(key))));
    variantAsk_.clear();
    variantNext_ = v->key;
    const double stock = variantStock(*m, v->key);
    const bool ok = addItem(qs(m->id));
    variantNext_.clear();
    if (ok && stock >= 0 && stock < 1)
        emit notice(tr("%1 (%2): none left in stock").arg(qs(m->name), variantLabel(v->key)));
    emit checkChanged();
    return ok;
}

void PosService::cancelVariant()
{
    variantAsk_.clear();
    emit checkChanged();
}

bool PosService::addItemByCode(const QString &code)
{
    // A size and color's barcode (or SKU): that one; else the item's own.
    if (const auto [m, key] = findVariantCode(code); m)
        return addVariant(qs(m->id), qs(key));
    return addItem(code);
}

bool PosService::setItemVariants(const QString &itemId, const QVariantList &axes, const QVariantList &variants)
{
    if (!require(perm::Manager, tr("Sizes and colors")))
        return false;
    const auto it = std::ranges::find(s_->menu, ss(itemId), &MenuItem::id);
    if (it == s_->menu.end())
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    if (it->isModifier || it->byWeight)
        return fail(tr("Sizes and colors are for items sold one at a time."));
    std::vector<MenuItem::VariantAxis> newAxes;
    for (const QVariant &a : axes) {
        const QVariantMap x = a.toMap();
        MenuItem::VariantAxis axis;
        axis.name = ss(x.value(u"name"_s).toString().trimmed());
        QStringList seen;
        for (const QVariant &v : x.value(u"values"_s).toList()) {
            const QString value = v.toString().trimmed().remove(u'|');
            if (value.isEmpty() || seen.contains(value, Qt::CaseInsensitive))
                continue;
            if (value.size() > 30)
                return fail(tr("\"%1\" is too long: 30 letters at most.").arg(value));
            seen << value;
            axis.values.push_back(ss(value));
        }
        if (axis.values.empty())
            continue;
        if (axis.values.size() > 30)
            return fail(tr("30 sizes or colors at most."));
        if (axis.name.empty())
            axis.name = ss(newAxes.empty() ? tr("Size") : tr("Color"));
        newAxes.push_back(std::move(axis));
    }
    if (newAxes.size() > 2)
        return fail(tr("Two lists at most: sizes and colors."));
    // Every combination, keeping what was said of each (barcode, SKU, price, not sold).
    std::map<std::string, QVariantMap> given;
    for (const QVariant &v : variants)
        given[ss(v.toMap().value(u"key"_s).toString())] = v.toMap();
    std::vector<MenuItem::Variant> list;
    std::vector<std::string> keys{std::string()};
    for (const MenuItem::VariantAxis &a : newAxes) {
        std::vector<std::string> next;
        for (const std::string &k : keys)
            for (const std::string &v : a.values)
                next.push_back(k.empty() ? v : k + "|" + v);
        keys = std::move(next);
    }
    if (newAxes.empty())
        keys.clear();
    QStringList codes;
    for (const std::string &k : keys) {
        MenuItem::Variant v;
        v.key = k;
        if (const MenuItem::Variant *was = it->variant(k))
            v = *was;
        if (const auto g = given.find(k); g != given.end()) {
            const QVariantMap &x = g->second;
            if (x.contains(u"barcode"_s))
                v.barcode = ss(x.value(u"barcode"_s).toString().trimmed());
            if (x.contains(u"sku"_s))
                v.sku = ss(x.value(u"sku"_s).toString().trimmed());
            if (x.contains(u"hidden"_s))
                v.hidden = x.value(u"hidden"_s).toBool();
            if (x.contains(u"price"_s)) {
                const QString p = x.value(u"price"_s).toString().trimmed();
                const qint64 cents = p.isEmpty() ? 0 : priceCents(p, qs(s_->settings.currencySymbol));
                if (cents < 0)
                    return fail(tr("Type a price like 24.00, or leave it empty for the item's."));
                v.price = Money::fromCents(cents);
            }
        }
        if (!v.barcode.empty()) {
            if (codes.contains(qs(v.barcode)))
                return fail(tr("Two sizes have the barcode %1.").arg(qs(v.barcode)));
            codes << qs(v.barcode);
        }
        list.push_back(v);
    }
    if (list.size() > 200)
        return fail(tr("Up to 200 sizes and colors."));
    // Not another item's barcode (nor its own sizes' as they were).
    for (const QString &c : codes) {
        static const QRegularExpression shape(u"^[0-9A-Za-z-]{4,40}$"_s);
        if (!shape.match(c).hasMatch())
            return fail(tr("A barcode is 4 to 40 letters and digits: \"%1\".").arg(c));
        for (const MenuItem &m : s_->menu) {
            if (m.id == it->id)
                continue;
            if (std::ranges::any_of(m.barcodes, [&](const std::string &b) { return sameBarcode(b, ss(c)); })
                || std::ranges::any_of(m.variants, [&](const MenuItem::Variant &v) { return !v.barcode.empty() && sameBarcode(v.barcode, ss(c)); }))
                return fail(tr("%1 already has the barcode %2.").arg(qs(m.name), c));
        }
        if (std::ranges::any_of(it->barcodes, [&](const std::string &b) { return sameBarcode(b, ss(c)); }))
            return fail(tr("%1 is the item's own barcode: a size needs one of its own.").arg(c));
    }
    it->variantAxes = std::move(newAxes);
    it->variants = std::move(list);
    // Counting its stock: each size and color counts its own.
    const bool tracked = std::ranges::any_of(it->recipe, [&](const RecipeLine &r) { return r.ingredientId == ownStockId(it->id); });
    if (tracked) {
        // (A copy: adding each size's stock moves the list, and a pointer into it.)
        const Ingredient *ownNow = s_->ingredient(ownStockId(it->id));
        const std::optional<Ingredient> own = ownNow ? std::optional(*ownNow) : std::nullopt;
        for (const MenuItem::Variant &v : it->variants) {
            const std::string id = variantStockId(it->id, v.key);
            if (s_->ingredient(id))
                continue;
            Ingredient g{id, it->name + " " + ss(variantLabel(v.key)), "each"};
            if (own) {
                g.lowAt = own->lowAt;
                g.cost = own->cost;
                g.vendor = own->vendor;
            }
            s_->ingredients.push_back(g);
            if (s_->sink)
                s_->sink->saveIngredient(g, int(s_->ingredients.size()) - 1);
        }
    }
    if (s_->sink)
        s_->sink->saveMenuItem(*it, int(it - s_->menu.begin()));
    ++s_->adminRevision;
    emit s_->menuChanged();
    emit s_->adminChanged();
    emit notice(it->variants.empty() ? tr("%1: no sizes or colors").arg(qs(it->name))
                                     : tr("%1: %n size(s) and color(s)", nullptr, int(it->variants.size())).arg(qs(it->name)));
    return true;
}

bool PosService::setVariantStock(const QString &itemId, const QString &key, int onHand)
{
    if (!require(perm::Manager, tr("Counting stock")))
        return false;
    const MenuItem *m = findItem(itemId);
    if (!m || !m->variant(ss(key)))
        return fail(tr("Choose a size and color."));
    if (onHand < 0 || onHand > 1'000'000)
        return fail(tr("On hand is how many there are, like 24."));
    for (int i = 0; i < int(s_->ingredients.size()); ++i) {
        Ingredient &g = s_->ingredients[i];
        if (g.id != variantStockId(m->id, ss(key)))
            continue;
        g.adjusted += onHand - g.onHand;
        g.onHand = onHand;
        g.settle();
        if (s_->sink)
            s_->sink->saveIngredient(g, i);
        refreshSoldOut();
        emit s_->dayChanged();
        emit s_->menuChanged();
        return true;
    }
    return fail(tr("Turn on Track stock for %1 first.").arg(qs(m->name)));
}

// --- price check --------------------------------------------------------------------------

bool PosService::setPriceCheck(bool on)
{
    if (!user())
        return fail(tr("Log in first."));
    priceCheckOn_ = on;
    priceChecked_.clear();
    emit sessionChanged();
    return true;
}

bool PosService::priceCheckCode(const QString &code)
{
    if (!user())
        return fail(tr("Log in first."));
    const MenuItem *m = nullptr;
    std::string key;
    if (const auto found = findVariantCode(code); found.first) {
        m = found.first;
        key = found.second;
    } else {
        m = findItem(code);
    }
    if (!m || m->isModifier) {
        priceChecked_ = {{u"code"_s, code.trimmed()}, {u"error"_s, tr("Nothing has the code %1.").arg(code.trimmed())}};
        emit sessionChanged();
        return false;
    }
    priceCheckOn_ = true;
    const std::string period = currentMealPeriod();
    const MenuItem::Variant *v = key.empty() ? nullptr : m->variant(key);
    const Money price = v && v->price.cents() > 0 ? v->price : m->priceDuring(period);
    // How many: its own stock, or (sizes and colors) theirs added up.
    QVariant stock;
    if (v) {
        if (const double s = variantStock(*m, key); s >= 0)
            stock = int(std::floor(s + 1e-9));
    } else if (!m->variants.empty()) {
        int sum = 0;
        bool any = false;
        for (const MenuItem::Variant &x : m->variants)
            if (const double s = variantStock(*m, x.key); s >= 0 && !x.hidden) {
                sum += int(std::floor(s + 1e-9));
                any = true;
            }
        if (any)
            stock = sum;
    } else if (std::ranges::any_of(m->recipe, [&](const RecipeLine &r) { return r.ingredientId == ownStockId(m->id); })) {
        if (const Ingredient *g = const_cast<PosShared *>(s_)->ingredient(ownStockId(m->id)))
            stock = int(std::floor(g->onHand + 1e-9));
    }
    priceChecked_ = {{u"code"_s, code.trimmed()}, {u"item"_s, qs(m->id)}, {u"variant"_s, qs(key)},
                     {u"name"_s, qs(m->name) + (v ? u" (%1)"_s.arg(variantLabel(key)) : QString())},
                     {u"price"_s, m->byWeight ? tr("%1 / %2").arg(format(price), qs(m->weightUnit)) : format(price)},
                     {u"stock"_s, stock}, {u"soldOut"_s, !m->available},
                     {u"axes"_s, v ? QVariantList() : variantAxesOf(*m)},
                     {u"cells"_s, v ? QVariantList() : variantCells(*m)}};
    emit sessionChanged();
    return true;
}

QVariantMap PosService::priceCheck() const
{
    if (!priceCheckOn_)
        return {};
    QVariantMap out = priceChecked_;
    out.insert(u"on"_s, true);
    return out;
}

} // namespace vt::app
