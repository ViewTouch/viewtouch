// PosService: the Menu Builder (Manager -> Menu Builder). Categories (their
// order, color, meal periods and what new items in them start with), an
// item's card (name, price, category, photo, choices, what's on it), and
// what's on an item: its own group of ingredients that come on it, so No,
// Lite, Extra and on the Side work for it everywhere.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// A text size in percent (Small 80, Large 125...); 0 or nonsense: the usual.
int textSizeFrom(const QVariant &v)
{
    const int pct = v.toInt();
    return pct <= 0 ? 0 : std::clamp(pct, 50, 200);
}

QString slugOf(const QString &text)
{
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QString s = text.toLower().normalized(QString::NormalizationForm_KD);
    s.replace(nonWord, u"-"_s);
    while (s.startsWith(u'-'))
        s.remove(0, 1);
    while (s.endsWith(u'-'))
        s.chop(1);
    return s.isEmpty() ? u"item"_s : s;
}

template <typename Taken>
std::string freeId(const QString &wanted, Taken taken)
{
    QString id = slugOf(wanted);
    for (int n = 2; taken(id.toStdString()); ++n)
        id = slugOf(wanted) + u'-' + QString::number(n);
    return id.toStdString();
}

// What's on an item: its own group.
std::string onItGroupId(const std::string &itemId)
{
    return "on-" + itemId;
}

// Its sizes (Small, Large...): its own choice group, asked first.
std::string sizeGroupId(const std::string &itemId)
{
    return "size-" + itemId;
}

// One item's own groups (what's on it, its sizes): not on the Choice Groups list.
bool isOwnGroup(const std::string &groupId)
{
    return groupId.starts_with("on-") || groupId.starts_with("size-");
}

// A list, or typed: "lettuce, tomato, onion".
QStringList strings(const QVariant &v)
{
    QStringList out;
    if (v.typeId() == QMetaType::QString) {
        for (const QString &s : v.toString().split(u',', Qt::SkipEmptyParts))
            if (!s.trimmed().isEmpty())
                out << s.trimmed();
        return out;
    }
    for (const QVariant &x : v.toList())
        if (const QString s = x.toString().trimmed(); !s.isEmpty())
            out << s;
    return out;
}

} // namespace

// "11.50", "$11.50", "11,50": a price in cents; -1 if it isn't one.
qint64 priceCents(QString text, const QString &symbol)
{
    text = text.remove(symbol).trimmed();
    if (!text.contains(u'.') && text.count(u',') == 1)
        text.replace(u',', u'.');           // 3,50
    else
        text.remove(u',');                  // 1,250.00
    bool ok = false;
    const double v = text.toDouble(&ok);
    return ok && v >= 0 ? std::llround(v * 100.0) : -1;
}

PosShared::MenuState PosService::menuState() const
{
    return {s_->menu, s_->settings.menuCategories, s_->settings.modifierGroups};
}

// One Menu Builder change, as one step to undo: what it was before, kept if
// the change changed something (nested changes are part of the outer one).
PosService::MenuStep::MenuStep(PosService *pos, QString label)
    : pos_(pos), label_(std::move(label)), outer_(pos->s_->menuUndoDepth++ == 0)
{
    if (outer_)
        before_ = pos_->menuState();
}

PosService::MenuStep::~MenuStep()
{
    --pos_->s_->menuUndoDepth;
    if (!outer_)
        return;
    PosShared::MenuState after = pos_->menuState();
    if (after == before_)
        return;
    pos_->logMenuChanges(before_, after, label_);
    auto &undo = pos_->s_->menuUndo;
    undo.push_back({label_, std::move(before_), std::move(after)});
    if (undo.size() > 20)
        undo.erase(undo.begin());
    ++pos_->s_->adminRevision;
    emit pos_->s_->adminChanged();
}

QString PosService::menuUndoText() const
{
    return s_->menuUndo.empty() ? QString() : s_->menuUndo.back().label;
}

bool PosService::undoMenuChange()
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    auto &undo = s_->menuUndo;
    if (undo.empty())
        return fail(tr("Nothing to undo."));
    if (!(menuState() == undo.back().after)) {
        // Changed since, elsewhere (another screen, Manager -> Menu): not over that.
        undo.clear();
        ++s_->adminRevision;
        emit s_->adminChanged();
        return fail(tr("The menu has changed since, so that can't be undone."));
    }
    PosShared::MenuUndo step = std::move(undo.back());
    undo.pop_back();
    logMenuChanges(step.after, step.before, tr("Undone: %1").arg(step.label));
    // Items gone from the store's records, then every item where it was.
    for (const MenuItem &m : s_->menu)
        if (s_->sink && std::ranges::none_of(step.before.menu, [&](const MenuItem &x) { return x.id == m.id; }))
            s_->sink->deleteMenuItem(m.id);
    s_->menu = std::move(step.before.menu);
    if (s_->sink)
        for (int i = 0; i < int(s_->menu.size()); ++i)
            s_->sink->saveMenuItem(s_->menu[i], i);
    s_->settings.menuCategories = std::move(step.before.categories);
    s_->settings.modifierGroups = std::move(step.before.groups);
    settingsChanged();
    menuChanged();
    emit notice(tr("Undone: %1").arg(step.label));
    return true;
}

// For the Changes report: each item's name, price and sold out; added and
// removed items and categories; anything else as the step's name.
void PosService::logMenuChanges(const PosShared::MenuState &before, const PosShared::MenuState &after,
                                const QString &step)
{
    const QString area = tr("Menu");
    int logged = 0;
    const auto log = [&](const QString &item, const QString &field, const QString &was, const QString &is) {
        if (++logged <= 40)
            logChange(area, item, field, was, is);
    };
    for (const MenuItem &m : after.menu) {
        const auto old = std::ranges::find(before.menu, m.id, &MenuItem::id);
        if (old == before.menu.end()) {
            log(qs(m.name), QString(), QString(), tr("Added, %1").arg(format(m.price)));
            continue;
        }
        if (*old == m)
            continue;
        MenuItem rest = *old;
        if (old->name != m.name)
            log(qs(old->name), tr("Name"), qs(old->name), qs(m.name));
        if (old->price != m.price)
            log(qs(m.name), tr("Price"), format(old->price), format(m.price));
        if (old->available != m.available)
            log(qs(m.name), tr("Sold out"), old->available ? tr("no") : tr("yes"), m.available ? tr("no") : tr("yes"));
        rest.name = m.name;
        rest.price = m.price;
        rest.available = m.available;
        rest.soldOutToday = m.soldOutToday;
        if (!(rest == m))
            log(qs(m.name), tr("Other details"), QString(), step);
    }
    for (const MenuItem &m : before.menu)
        if (std::ranges::find(after.menu, m.id, &MenuItem::id) == after.menu.end())
            log(qs(m.name), QString(), QString(), tr("Removed"));
    for (const MenuCategory &c : after.categories) {
        const auto old = std::ranges::find(before.categories, c.id, &MenuCategory::id);
        if (old == before.categories.end())
            log(tr("Category %1").arg(qs(c.name)), QString(), QString(), tr("Added"));
        else if (old->name != c.name)
            log(tr("Category %1").arg(qs(old->name)), tr("Name"), qs(old->name), qs(c.name));
        else if (!(*old == c))
            log(tr("Category %1").arg(qs(c.name)), tr("Other details"), QString(), step);
    }
    for (const MenuCategory &c : before.categories)
        if (std::ranges::find(after.categories, c.id, &MenuCategory::id) == after.categories.end())
            log(tr("Category %1").arg(qs(c.name)), QString(), QString(), tr("Removed"));
    if (!(before.groups == after.groups))
        log(tr("Choices"), QString(), QString(), step);
    if (logged > 40)
        logChange(area, QString(), QString(), QString(), tr("%1 (and %2 more changes)").arg(step).arg(logged - 40));
}

void PosService::menuChanged()
{
    refreshSoldOut();
    ++s_->adminRevision;
    emit s_->adminChanged();
}

// --- categories ----------------------------------------------------------------------

bool PosService::saveCategory(const QVariantMap &record)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Category %1").arg(record.value(u"name"_s).toString().trimmed()));
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The category needs a name."));
    // The categories as shown (families not set up yet become set up now).
    std::vector<MenuCategory> list = s_->categories();
    const std::string id = ss(record.value(u"id"_s).toString());
    auto it = std::ranges::find_if(list, [&](const MenuCategory &c) { return !id.empty() && c.id == id; });
    for (const MenuCategory &c : list)
        if (c.id != id && QString::compare(qs(c.name), name, Qt::CaseInsensitive) == 0)
            return fail(tr("There's already a category called %1.").arg(name));
    MenuCategory c = it != list.end() ? *it : MenuCategory{};
    c.name = ss(name);
    if (c.id.empty())
        c.id = freeId(name, [&](const std::string &x) {
            return std::ranges::any_of(list, [&](const MenuCategory &o) { return o.id == x; })
                   || std::ranges::any_of(s_->menu, [&](const MenuItem &m) { return m.family == x; });
        });
    if (record.contains(u"color"_s))
        c.color = ss(record.value(u"color"_s).toString());
    if (record.contains(u"periods"_s)) {
        c.periods.clear();
        for (const QString &p : strings(record.value(u"periods"_s))) {
            if (std::ranges::none_of(s_->settings.mealPeriods, [&](const MealPeriod &m) { return qs(m.id) == p; }))
                return fail(tr("There is no meal period '%1'.").arg(p));
            c.periods.push_back(ss(p));
        }
        // Every meal period: all day.
        if (c.periods.size() == s_->settings.mealPeriods.size())
            c.periods.clear();
    }
    if (record.contains(u"printer"_s))
        c.printer = ss(record.value(u"printer"_s).toString());
    if (record.contains(u"station"_s))
        c.station = ss(record.value(u"station"_s).toString());
    if (record.contains(u"taxClass"_s))
        c.taxClass = ss(record.value(u"taxClass"_s).toString());
    if (record.contains(u"buttonSize"_s)) {
        const QString size = record.value(u"buttonSize"_s).toString();
        if (!QStringList{QString(), u"small"_s, u"medium"_s, u"large"_s}.contains(size))
            return fail(tr("Button size is small, medium or large."));
        c.buttonSize = ss(size);
    }
    if (record.contains(u"photos"_s))
        c.photos = record.value(u"photos"_s).toBool();
    if (record.contains(u"hidePrice"_s))
        c.hidePrice = record.value(u"hidePrice"_s).toBool();
    if (record.contains(u"textSize"_s))
        c.textSize = textSizeFrom(record.value(u"textSize"_s));
    if (record.contains(u"shades"_s))
        c.shades = record.value(u"shades"_s).toBool();
    if (it != list.end())
        *it = c;
    else
        list.push_back(c);
    s_->settings.menuCategories = list;
    settingsChanged();
    menuChanged();
    emit notice(tr("Saved"));
    return true;
}

// Colors…: every category's color at once ({id: "#rrggbb"}); one Undo step.
bool PosService::setCategoryColors(const QVariantMap &colors)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    static const QRegularExpression hex(u"^#[0-9a-fA-F]{6}$"_s);
    std::vector<MenuCategory> list = s_->categories();
    for (auto c = colors.begin(); c != colors.end(); ++c) {
        if (!hex.match(c.value().toString()).hasMatch())
            return fail(tr("That isn't a color."));
        if (std::ranges::none_of(list, [&](const MenuCategory &x) { return qs(x.id) == c.key(); }))
            return fail(tr("There is no category '%1'.").arg(c.key()));
    }
    const MenuStep step(this, tr("Category colors"));
    for (MenuCategory &c : list)
        if (colors.contains(qs(c.id)))
            c.color = ss(colors.value(qs(c.id)).toString().toLower());
    s_->settings.menuCategories = list;
    settingsChanged();
    menuChanged();
    emit notice(tr("Saved"));
    return true;
}

bool PosService::moveCategory(const QString &id, int by)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Move a category"));
    std::vector<MenuCategory> list = s_->categories();
    const auto it = std::ranges::find_if(list, [&](const MenuCategory &c) { return qs(c.id) == id; });
    if (it == list.end())
        return fail(tr("That category is gone."));
    const int from = int(it - list.begin());
    const int to = std::clamp(from + (by > 0 ? 1 : -1), 0, int(list.size()) - 1);
    if (to == from)
        return true;
    std::swap(list[from], list[to]);
    s_->settings.menuCategories = list;
    settingsChanged();
    menuChanged();
    return true;
}

bool PosService::moveCategoryTo(const QString &id, int position)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Move a category"));
    std::vector<MenuCategory> list = s_->categories();
    const auto it = std::ranges::find_if(list, [&](const MenuCategory &c) { return qs(c.id) == id; });
    if (it == list.end())
        return fail(tr("That category is gone."));
    MenuCategory c = *it;
    list.erase(it);
    list.insert(list.begin() + std::clamp(position, 0, int(list.size())), c);
    s_->settings.menuCategories = list;
    settingsChanged();
    menuChanged();
    return true;
}

// Its place among its category's items (dragged in the Menu Builder): the
// category's items trade the places they have in the menu.
bool PosService::moveMenuItemTo(const QString &id, int position)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Move an item"));
    auto &menu = s_->menu;
    const auto it = std::ranges::find_if(menu, [&](const MenuItem &m) { return qs(m.id) == id; });
    if (it == menu.end())
        return fail(tr("'%1' is not on the menu.").arg(id));
    const std::string family = it->family;
    std::vector<int> places;
    std::vector<MenuItem> items;
    for (int i = 0; i < int(menu.size()); ++i)
        if (menu[i].family == family && !menu[i].isModifier) {
            places.push_back(i);
            items.push_back(menu[i]);
        }
    const auto from = std::ranges::find_if(items, [&](const MenuItem &m) { return qs(m.id) == id; });
    if (from == items.end())
        return fail(tr("'%1' is not on the menu.").arg(id));   // a choice, not an item
    MenuItem moved = *from;
    items.erase(from);
    items.insert(items.begin() + std::clamp(position, 0, int(items.size())), moved);
    for (std::size_t k = 0; k < places.size(); ++k) {
        if (menu[places[k]].id == items[k].id)
            continue;
        menu[places[k]] = items[k];
        if (s_->sink)
            s_->sink->saveMenuItem(menu[places[k]], places[k]);
    }
    menuChanged();
    return true;
}

bool PosService::deleteCategory(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Remove a category"));
    const auto items = std::ranges::count_if(s_->menu, [&](const MenuItem &m) { return qs(m.family) == id && !m.isModifier; });
    if (items > 0)
        return fail(tr("Move or remove its %n item(s) first.", nullptr, int(items)));
    std::vector<MenuCategory> list = s_->categories();
    std::erase_if(list, [&](const MenuCategory &c) { return qs(c.id) == id; });
    s_->settings.menuCategories = list;
    // Screens that started orders on it: the meal's categories again.
    for (TerminalConfig &t : s_->settings.terminals)
        if (qs(t.startCategory) == id)
            t.startCategory.clear();
    settingsChanged();
    menuChanged();
    emit notice(tr("Category removed"));
    return true;
}

// --- ready to go? ----------------------------------------------------------------------

QVariantList PosService::menuProblems() const
{
    QVariantList out;
    const auto problem = [&](const QString &text, const QString &item = {}, const QString &group = {},
                             const QString &category = {}, bool serious = true) {
        out.append(QVariantMap{{u"text"_s, text}, {u"item"_s, item}, {u"group"_s, group}, {u"category"_s, category},
                               {u"serious"_s, serious}});
    };
    const std::vector<MenuCategory> categories = s_->categories();
    QHash<QString, QString> seen;   // name, lower case -> the first item's id
    // Kitchen tickets for a printer that isn't set up print in the kitchen,
    // or nowhere without one (a store with no printers at all uses screens).
    QMap<QString, QStringList> unrouted;
    QHash<QString, QString> firstOf;
    for (const MenuItem &m : s_->menu) {
        if (m.isModifier)
            continue;
        const QString id = qs(m.id), name = qs(m.name);
        if (m.price.cents() == 0 && !m.byWeight)
            problem(tr("%1 has no price.").arg(name), id, {}, {}, false);
        if (m.family.empty())
            problem(tr("%1 is in no category, so it's on no menu screen.").arg(name), id);
        for (const std::string &g : m.modifierGroups)
            if (!s_->settings.modifierGroup(g))
                problem(tr("%1 asks for a choice group that's gone.").arg(name), id);
        if (!m.printer.empty() && !s_->settings.printer(m.printer) && !s_->settings.printers.empty())
            unrouted[qs(m.printer)] << name;
        if (unrouted.contains(qs(m.printer)) && !firstOf.contains(qs(m.printer)))
            firstOf.insert(qs(m.printer), id);
        if (const QString key = name.toLower(); seen.contains(key))
            problem(tr("There are two %1: the screens can't tell them apart.").arg(name), id);
        else
            seen.insert(key, id);
    }
    const bool kitchen = s_->settings.printer("kitchen");
    for (auto it = unrouted.cbegin(); it != unrouted.cend(); ++it) {
        QStringList names = it.value();
        const int n = int(names.size());
        if (n > 3)
            names = names.mid(0, 3) << tr("%n more", nullptr, n - 3);
        if (kitchen)
            problem(tr("%n item(s) go to the printer %1, which isn't set up, so they print in the kitchen: %2", nullptr, n)
                        .arg(it.key(), names.join(u", "_s)), firstOf.value(it.key()), {}, {}, false);
        else
            problem(tr("%n item(s) go to the printer %1, which isn't set up, and there's no kitchen printer: their tickets print nowhere: %2", nullptr, n)
                        .arg(it.key(), names.join(u", "_s)), firstOf.value(it.key()));
    }
    for (const ModifierGroup &g : s_->settings.modifierGroups) {
        if (isOwnGroup(g.id))
            continue;
        if (g.options.empty())
            problem(tr("The choice group %1 has no options.").arg(qs(g.name)), {}, qs(g.id));
        else if (g.min > int(g.options.size()))
            problem(tr("%1 asks for %2 choices but has only %3 options.").arg(qs(g.name)).arg(g.min).arg(g.options.size()),
                    {}, qs(g.id));
    }
    for (const MenuCategory &c : categories) {
        for (const std::string &p : c.periods)
            if (std::ranges::none_of(s_->settings.mealPeriods, [&](const MealPeriod &x) { return x.id == p; }))
                problem(tr("%1 is on a meal period that isn't set up (%2).").arg(qs(c.name), qs(p)), {}, {}, qs(c.id));
        if (std::ranges::none_of(s_->menu, [&](const MenuItem &m) { return m.family == c.id && !m.isModifier; }))
            problem(tr("%1 has no items yet.").arg(qs(c.name)), {}, {}, qs(c.id), false);
    }
    return out;
}

// --- choice groups ---------------------------------------------------------------------

bool PosService::saveChoiceGroup(const QVariantMap &record)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Choice group %1").arg(record.value(u"name"_s).toString().trimmed()));
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The choice group needs a name."));
    auto &list = s_->settings.modifierGroups;
    const std::string id = ss(record.value(u"id"_s).toString());
    auto it = std::ranges::find_if(list, [&](const ModifierGroup &g) { return !id.empty() && g.id == id; });
    ModifierGroup g = it != list.end() ? *it : ModifierGroup{};
    g.name = ss(name);
    g.min = std::max(0, record.value(u"min"_s).toInt());
    g.max = std::max(0, record.value(u"max"_s).toInt());
    if (g.max > 0 && g.min > g.max)
        return fail(tr("It can't require more choices than it allows."));
    g.askHow = record.value(u"askHow"_s).toBool();
    std::vector<ModifierOption> options;
    for (const QVariant &v : record.value(u"options"_s).toList()) {
        const QVariantMap o = v.toMap();
        const QString optName = o.value(u"name"_s).toString().trimmed();
        if (optName.isEmpty())
            continue;   // a row left empty
        const QString priceText = o.value(u"price"_s).toString().remove(qs(s_->settings.currencySymbol)).trimmed();
        const qint64 price = priceText.isEmpty() ? 0 : priceCents(priceText, {});
        if (price < 0)
            return fail(tr("%1: type its price, like 1.50 (or leave it empty).").arg(optName));
        if (std::ranges::any_of(options, [&](const ModifierOption &x) { return QString::compare(qs(x.name), optName, Qt::CaseInsensitive) == 0; }))
            return fail(tr("%1 is in it twice.").arg(optName));
        // Kept: what the Menu Builder doesn't show (a combo's menu item).
        ModifierOption opt;
        if (it != list.end())
            if (const auto old = std::ranges::find_if(it->options, [&](const ModifierOption &x) { return qs(x.name) == optName; });
                old != it->options.end())
                opt = *old;
        opt.name = ss(optName);
        opt.price = Money::fromCents(price);
        opt.included = o.value(u"included"_s).toBool();
        if (o.contains(u"kitchenName"_s))
            opt.kitchenName = ss(o.value(u"kitchenName"_s).toString().trimmed());
        // By size: {"Large": "1.50"}; empty: its price on that size too.
        if (o.contains(u"sizePrices"_s)) {
            opt.sizePrices.clear();
            const QVariantMap sizes = o.value(u"sizePrices"_s).toMap();
            for (auto s = sizes.begin(); s != sizes.end(); ++s) {
                const QString text = s.value().toString().trimmed();
                if (text.isEmpty())
                    continue;
                const qint64 cents = priceCents(text, qs(s_->settings.currencySymbol));
                if (cents < 0)
                    return fail(tr("%1 on a %2: type its price, like 1.50 (or leave it empty).").arg(optName, s.key()));
                opt.sizePrices[ss(s.key().toLower())] = Money::fromCents(cents);
            }
        }
        options.push_back(std::move(opt));
    }
    if (options.empty())
        return fail(tr("Add its options."));
    if (g.min > int(options.size()))
        return fail(tr("It requires more choices than it has options."));
    g.options = std::move(options);
    if (it != list.end()) {
        *it = g;
    } else {
        g.id = freeId(name, [&](const std::string &x) {
            return std::ranges::any_of(list, [&](const ModifierGroup &o) { return o.id == x; });
        });
        list.push_back(g);
    }
    settingsChanged();
    menuChanged();
    emit notice(tr("Saved"));
    return true;
}

bool PosService::deleteChoiceGroup(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Remove a choice group"));
    auto &list = s_->settings.modifierGroups;
    if (std::erase_if(list, [&](const ModifierGroup &g) { return qs(g.id) == id; }) == 0)
        return fail(tr("That choice group is gone."));
    // Items stop asking for it.
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (std::erase(m.modifierGroups, ss(id)) > 0 && s_->sink)
            s_->sink->saveMenuItem(m, i);
    }
    settingsChanged();
    menuChanged();
    emit notice(tr("Choice group removed"));
    return true;
}

// --- an item's card ------------------------------------------------------------------

bool PosService::saveMenuItemCard(const QVariantMap &card)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, card.value(u"name"_s).toString().trimmed());
    const QString name = card.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The item needs a name."));
    // Sizes on the card (the quick card): checked first, put on once it's saved.
    std::optional<std::vector<std::pair<QString, qint64>>> sizes;
    if (card.contains(u"sizes"_s) && !(sizes = sizeList(card.value(u"sizes"_s).toList())))
        return false;
    const bool sized = sizes && !sizes->empty();
    const qint64 price = priceCents(card.value(u"price"_s).toString(), qs(s_->settings.currencySymbol));
    if (card.contains(u"price"_s) && price < 0 && !sized)
        return fail(tr("Type the price, like 11.50."));
    const std::string id = ss(card.value(u"id"_s).toString());
    auto &menu = s_->menu;
    auto it = std::ranges::find_if(menu, [&](const MenuItem &m) { return !id.empty() && m.id == id; });
    const bool adding = it == menu.end();
    for (const MenuItem &m : menu)
        if (m.id != id && !m.isModifier && QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0)
            return fail(tr("%1 is already on the menu.").arg(name));
    const std::string family = card.contains(u"family"_s) ? ss(card.value(u"family"_s).toString())
                                                         : (adding ? std::string() : it->family);
    if (family.empty())
        return fail(tr("Choose its category."));
    const std::vector<MenuCategory> categories = s_->categories();
    const auto category = std::ranges::find_if(categories, [&](const MenuCategory &c) { return c.id == family; });
    if (category == categories.end())
        return fail(tr("There is no category '%1'.").arg(qs(family)));

    MenuItem item = adding ? MenuItem{} : *it;
    if (adding) {
        // What its category's items start with.
        item.id = freeId(name, [&](const std::string &x) {
            return std::ranges::any_of(menu, [&](const MenuItem &m) { return m.id == x; });
        });
        item.printer = category->printer.empty() ? "kitchen" : category->printer;
        item.station = category->station;
        item.taxClass = taxClassFromString(category->taxClass.empty() ? "food" : category->taxClass);
        item.available = true;
    } else if (item.family != family) {
        // Moved to another category: its kitchen and tax come along only if
        // they were the old category's.
        const auto old = std::ranges::find_if(categories, [&](const MenuCategory &c) { return c.id == item.family; });
        if (old != categories.end()) {
            if (item.printer == (old->printer.empty() ? "kitchen" : old->printer) && !category->printer.empty())
                item.printer = category->printer;
            if (item.station == old->station)
                item.station = category->station;
        }
    }
    item.name = ss(name);
    item.family = family;
    if (card.contains(u"price"_s) && price >= 0)
        item.price = Money::fromCents(price);
    if (card.contains(u"image"_s))
        item.image = ss(card.value(u"image"_s).toString());
    if (card.contains(u"description"_s))
        item.description = ss(card.value(u"description"_s).toString().trimmed());
    if (card.contains(u"available"_s) && card.value(u"available"_s).toBool() != item.available) {
        item.available = card.value(u"available"_s).toBool();
        item.soldOutToday = false;   // decided here: until changed here
    }
    if (card.contains(u"kioskHide"_s))
        item.kioskHide = card.value(u"kioskHide"_s).toBool();
    if (card.contains(u"favorite"_s))
        item.favorite = card.value(u"favorite"_s).toBool();
    if (card.contains(u"allergens"_s)) {
        const QStringList wanted = strings(card.value(u"allergens"_s));
        item.allergens.clear();
        for (const std::string &a : allergenIds())
            if (wanted.contains(qs(a)))
                item.allergens.push_back(a);
    }
    if (card.contains(u"taxClass"_s))
        item.taxClass = taxClassFromString(ss(card.value(u"taxClass"_s).toString()));
    if (card.contains(u"printer"_s))
        item.printer = ss(card.value(u"printer"_s).toString());
    if (card.contains(u"station"_s))
        item.station = ss(card.value(u"station"_s).toString());
    if (card.contains(u"kitchenName"_s))
        item.kitchenName = ss(card.value(u"kitchenName"_s).toString().trimmed());
    if (card.contains(u"section"_s))
        item.section = ss(card.value(u"section"_s).toString().simplified());
    if (card.contains(u"breakBefore"_s)) {
        const QString b = card.value(u"breakBefore"_s).toString();
        if (!QStringList{QString(), u"space"_s, u"row"_s}.contains(b))
            return fail(tr("Before an item: nothing, a space or a new row."));
        item.breakBefore = ss(b);
    }
    if (card.contains(u"buttonColor"_s))
        item.buttonColor = ss(card.value(u"buttonColor"_s).toString().trimmed());
    if (card.contains(u"textSize"_s))
        item.textSize = textSizeFrom(card.value(u"textSize"_s));
    if (card.contains(u"prepMinutes"_s)) {
        const QString t = card.value(u"prepMinutes"_s).toString().trimmed();
        bool ok = t.isEmpty();
        const int minutes = ok ? 0 : t.toInt(&ok);
        if (!ok || minutes < 0 || minutes > 240)
            return fail(tr("Kitchen time is minutes, like 12 (or empty)."));
        item.prepMinutes = minutes;
    }
    if (card.contains(u"number"_s)) {
        const QString plu = card.value(u"number"_s).toString().trimmed();
        if (!plu.isEmpty()) {
            if (plu.size() > 6 || !std::ranges::all_of(plu, [](QChar c) { return c.isDigit(); }))
                return fail(tr("The number is digits only (up to 6), like 104."));
            for (const MenuItem &m : menu)
                if (m.id != item.id && qs(m.number) == plu)
                    return fail(tr("%1 already has number %2.").arg(qs(m.name), plu));
        }
        item.number = ss(plu);
    }
    if (card.contains(u"barcodes"_s)) {
        const QStringList codes = barcodeList(card.value(u"barcodes"_s).toString());
        if (const QString problem = barcodeProblem(codes, item.id); !problem.isEmpty())
            return fail(problem);
        item.barcodes.clear();
        for (const QString &c : codes)
            item.barcodes.push_back(ss(c));
    }
    // Other prices: empty is the regular price.
    const auto otherPrice = [&](const QVariant &v, Money &into) {
        const QString t = v.toString().trimmed();
        const qint64 c = t.isEmpty() ? 0 : priceCents(t, qs(s_->settings.currencySymbol));
        if (c < 0)
            return false;
        into = Money::fromCents(c);
        return true;
    };
    if (card.contains(u"takeoutPrice"_s) && !otherPrice(card.value(u"takeoutPrice"_s), item.takeoutPrice))
        return fail(tr("Type the takeout price, like 11.50, or leave it empty."));
    if (card.contains(u"periodPrices"_s)) {
        const QVariantMap wanted = card.value(u"periodPrices"_s).toMap();
        const QStringList known = periodIds();
        item.periodPrices.clear();
        for (auto p = wanted.begin(); p != wanted.end(); ++p) {
            if (p.value().toString().trimmed().isEmpty())
                continue;
            if (!known.contains(p.key()))
                return fail(tr("There is no meal period '%1'.").arg(p.key()));
            Money m;
            if (!otherPrice(p.value(), m))
                return fail(tr("Type the price, like 11.50, or leave it empty."));
            item.periodPrices[ss(p.key())] = m;
        }
    }

    // Its choices: groups picked from the list (its own What's on it first).
    const std::string onIt = onItGroupId(item.id);
    if (card.contains(u"groups"_s)) {
        std::vector<std::string> groups;
        for (const QString &g : strings(card.value(u"groups"_s))) {
            if (!s_->settings.modifierGroup(ss(g)))
                return fail(tr("There is no choice group '%1'.").arg(g));
            if (ss(g) != onIt && ss(g) != sizeGroupId(item.id))
                groups.push_back(ss(g));
        }
        const bool hadOnIt = std::ranges::find(item.modifierGroups, onIt) != item.modifierGroups.end();
        const bool hadSizes = std::ranges::find(item.modifierGroups, sizeGroupId(item.id)) != item.modifierGroups.end();
        item.modifierGroups = groups;
        if (hadOnIt)
            item.modifierGroups.insert(item.modifierGroups.begin(), onIt);
        if (hadSizes)
            item.modifierGroups.insert(item.modifierGroups.begin(), sizeGroupId(item.id));
    }
    // What's on it: "lettuce, tomato, onion" -> its own group, each one that
    // comes on it (No, Lite, Extra, on the Side).
    if (card.contains(u"onIt"_s)) {
        const QStringList ingredients = strings(card.value(u"onIt"_s));
        auto &groups = s_->settings.modifierGroups;
        std::erase_if(groups, [&](const ModifierGroup &g) { return g.id == onIt; });
        std::erase(item.modifierGroups, onIt);
        if (!ingredients.isEmpty()) {
            ModifierGroup g;
            g.id = onIt;
            g.name = ss(tr("What's on it"));
            g.min = 0;
            g.max = 0;
            g.askHow = true;
            for (const QString &i : ingredients) {
                ModifierOption o;
                o.name = ss(i);
                o.included = true;
                g.options.push_back(std::move(o));
            }
            groups.push_back(std::move(g));
            item.modifierGroups.insert(item.modifierGroups.begin(), onIt);
        }
        // Its size is still asked first.
        if (std::erase(item.modifierGroups, sizeGroupId(item.id)) > 0)
            item.modifierGroups.insert(item.modifierGroups.begin(), sizeGroupId(item.id));
        settingsChanged();
    }

    if (adding) {
        // After the last item of its category, so it shows at its end.
        auto at = menu.end();
        for (auto i = menu.begin(); i != menu.end(); ++i)
            if (i->family == family)
                at = i + 1;
        it = menu.insert(at, item);
        if (s_->sink)
            for (int i = int(it - menu.begin()); i < int(menu.size()); ++i)
                s_->sink->saveMenuItem(menu[i], i);
    } else {
        *it = item;
        if (s_->sink)
            s_->sink->saveMenuItem(*it, int(it - menu.begin()));
    }
    menuChanged();
    const bool hadSizes = s_->settings.modifierGroup(sizeGroupId(item.id)) != nullptr;
    if (card.contains(u"sizes"_s) && (sized || hadSizes) && !setItemSizes(qs(item.id), card.value(u"sizes"_s).toList()))
        return false;   // (checked above: not expected)
    emit notice(adding ? tr("%1 added").arg(name) : tr("Saved"));
    return true;
}

// Sizes…: [{name, price}] for an item (Small 3.00, Large 4.50): its own
// choice group, asked first; the item's price is the smallest, each size adds
// the rest. Empty: one size again.
std::optional<std::vector<std::pair<QString, qint64>>> PosService::sizeList(const QVariantList &sizes)
{
    std::vector<std::pair<QString, qint64>> list;
    for (const QVariant &v : sizes) {
        const QVariantMap m = v.toMap();
        const QString name = m.value(u"name"_s).toString().simplified();
        const qint64 cents = priceCents(m.value(u"price"_s).toString(), qs(s_->settings.currencySymbol));
        if (name.isEmpty() && m.value(u"price"_s).toString().trimmed().isEmpty())
            continue;   // a row left empty
        if (name.isEmpty()) {
            fail(tr("Each size needs a name."));
            return std::nullopt;
        }
        if (cents < 0) {
            fail(tr("Type %1's price, like 4.50.").arg(name));
            return std::nullopt;
        }
        if (std::ranges::any_of(list, [&](const auto &x) { return QString::compare(x.first, name, Qt::CaseInsensitive) == 0; })) {
            fail(tr("%1 is there twice.").arg(name));
            return std::nullopt;
        }
        list.emplace_back(name, cents);
    }
    if (list.size() == 1) {
        fail(tr("Two sizes or more (or none)."));
        return std::nullopt;
    }
    return list;
}

bool PosService::setItemSizes(const QString &itemId, const QVariantList &sizes)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const auto it = std::ranges::find_if(s_->menu, [&](const MenuItem &m) { return qs(m.id) == itemId; });
    if (it == s_->menu.end())
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    const auto checked = sizeList(sizes);
    if (!checked)
        return false;
    const std::vector<std::pair<QString, qint64>> &list = *checked;
    const MenuStep step(this, tr("Sizes for %1").arg(qs(it->name)));
    const std::string id = sizeGroupId(it->id);
    auto &groups = s_->settings.modifierGroups;
    std::erase_if(groups, [&](const ModifierGroup &g) { return g.id == id; });
    std::erase(it->modifierGroups, id);
    if (!list.empty()) {
        const qint64 base = std::ranges::min(list | std::views::transform([](const auto &x) { return x.second; }));
        ModifierGroup g;
        g.id = id;
        g.name = ss(tr("Size"));
        g.min = 1;
        g.max = 1;
        for (const auto &[name, cents] : list) {
            ModifierOption o;
            o.name = ss(name);
            o.price = Money::fromCents(cents - base);
            g.options.push_back(std::move(o));
        }
        groups.push_back(std::move(g));
        it->modifierGroups.insert(it->modifierGroups.begin(), id);
        it->price = Money::fromCents(base);
    }
    if (s_->sink)
        s_->sink->saveMenuItem(*it, int(it - s_->menu.begin()));
    settingsChanged();
    menuChanged();
    emit notice(list.empty() ? tr("One size") : tr("Sizes saved"));
    return true;
}

// Many prices at once (Prices…): [{id, price, takeoutPrice?, deliveryPrice?,
// periodPrices?}], exactly as previewed; one Undo step.
bool PosService::setMenuPrices(const QVariantList &prices)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    return applyPrices(prices, {});
}

bool PosService::applyPrices(const QVariantList &prices, const QString &label)
{
    const QString symbol = qs(s_->settings.currencySymbol);
    const auto cents = [&](const QVariant &v) { return priceCents(v.toString(), symbol); };
    std::vector<std::pair<int, MenuItem>> changes;
    for (const QVariant &v : prices) {
        const QVariantMap p = v.toMap();
        const std::string id = ss(p.value(u"id"_s).toString());
        const auto it = std::ranges::find_if(s_->menu, [&](const MenuItem &m) { return m.id == id; });
        if (it == s_->menu.end())
            return fail(tr("That item is gone."));
        MenuItem m = *it;
        const qint64 price = cents(p.value(u"price"_s));
        if (price < 0)
            return fail(tr("Type the price, like 11.50."));
        m.price = Money::fromCents(price);
        for (const auto &[key, into] : {std::pair{u"takeoutPrice"_s, &m.takeoutPrice}, std::pair{u"deliveryPrice"_s, &m.deliveryPrice}}) {
            if (!p.contains(key))
                continue;
            const qint64 c = cents(p.value(key));
            if (c < 0)
                return fail(tr("Type the price, like 11.50."));
            *into = Money::fromCents(c);
        }
        const QVariantMap periods = p.value(u"periodPrices"_s).toMap();
        for (auto q = periods.begin(); q != periods.end(); ++q) {
            const auto at = m.periodPrices.find(ss(q.key()));
            const qint64 c = cents(q.value());
            if (at == m.periodPrices.end() || c < 0)
                return fail(tr("Type the price, like 11.50."));
            at->second = Money::fromCents(c);
        }
        if (!(m == *it))
            changes.emplace_back(int(it - s_->menu.begin()), std::move(m));
    }
    if (changes.empty())
        return true;
    const MenuStep step(this, label.isEmpty() ? tr("Change %n price(s)", "", int(changes.size())) : label);
    for (auto &[index, item] : changes) {
        s_->menu[index] = std::move(item);
        if (s_->sink)
            s_->sink->saveMenuItem(s_->menu[index], index);
    }
    menuChanged();
    emit notice(tr("%n price(s) changed", "", int(changes.size())));
    return true;
}

// Prices… → Later: checked now (every item there, every price a price), put
// on the menu at `at`.
bool PosService::schedulePrices(const QVariantList &prices, qint64 at, const QString &label)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    if (at <= now())
        return fail(tr("Choose a time still to come."));
    if (prices.isEmpty())
        return fail(tr("No prices to change."));
    const QString symbol = qs(s_->settings.currencySymbol);
    for (const QVariant &v : prices) {
        const QVariantMap p = v.toMap();
        if (!findItem(p.value(u"id"_s).toString()))
            return fail(tr("That item is gone."));
        if (priceCents(p.value(u"price"_s).toString(), symbol) < 0)
            return fail(tr("Type the price, like 11.50."));
    }
    auto &list = s_->settings.priceChanges;
    std::int64_t id = 1;
    for (const PosSettings::PriceChange &c : list)
        id = std::max(id, c.id + 1);
    list.push_back({id, at, ss(label.trimmed().isEmpty() ? tr("Price change") : label.trimmed()),
                    QJsonDocument(QJsonArray::fromVariantList(prices)).toJson(QJsonDocument::Compact).toStdString()});
    std::ranges::sort(list, {}, &PosSettings::PriceChange::at);
    settingsChanged();
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(tr("%n price(s) change %1", "", int(prices.size())).arg(dueText(at)));
    return true;
}

bool PosService::cancelPriceChange(qint64 id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    if (std::erase_if(s_->settings.priceChanges, [&](const PosSettings::PriceChange &c) { return c.id == id; }) == 0)
        return fail(tr("That price change is gone."));
    settingsChanged();
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(tr("Price change canceled"));
    return true;
}

void PosService::applyDuePriceChanges()
{
    auto &list = s_->settings.priceChanges;
    while (!list.empty() && list.front().at <= now()) {
        const PosSettings::PriceChange c = list.front();
        list.erase(list.begin());
        settingsChanged();
        const QVariantList prices = QJsonDocument::fromJson(QByteArray::fromStdString(c.prices)).array().toVariantList();
        // Items gone since: the rest still change.
        QVariantList here;
        for (const QVariant &p : prices)
            if (findItem(p.toMap().value(u"id"_s).toString()))
                here << p;
        applyPrices(here, qs(c.label));
        ++s_->adminRevision;
        emit s_->adminChanged();
    }
}

QVariantList PosService::priceChanges() const
{
    QVariantList out;
    for (const PosSettings::PriceChange &c : s_->settings.priceChanges)
        out.append(QVariantMap{{u"id"_s, qint64(c.id)}, {u"when"_s, dueText(c.at)}, {u"at"_s, qint64(c.at)},
                               {u"label"_s, qs(c.label)},
                               {u"count"_s, qint64(QJsonDocument::fromJson(QByteArray::fromStdString(c.prices)).array().size())}});
    return out;
}

bool PosService::duplicateMenuItem(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Duplicate an item"));
    auto &menu = s_->menu;
    const auto it = std::ranges::find_if(menu, [&](const MenuItem &m) { return qs(m.id) == id; });
    if (it == menu.end())
        return fail(tr("'%1' is not on the menu.").arg(id));
    MenuItem copy = *it;
    // "Fish Tacos 2": a name of its own, to change.
    QString name;
    for (int n = 2;; ++n) {
        name = u"%1 %2"_s.arg(qs(it->name)).arg(n);
        if (std::ranges::none_of(menu, [&](const MenuItem &m) { return QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0; }))
            break;
    }
    copy.name = ss(name);
    copy.number.clear();   // numbers and barcodes are one item's
    copy.barcodes.clear();
    copy.breakBefore.clear();   // one space or new row is enough
    copy.ticketsSoldBefore = 0;
    copy.id = freeId(name, [&](const std::string &x) { return std::ranges::any_of(menu, [&](const MenuItem &m) { return m.id == x; }); });
    // Its own What's on it, copied too.
    const std::string oldOnIt = onItGroupId(it->id), newOnIt = onItGroupId(copy.id);
    for (std::string &g : copy.modifierGroups)
        if (g == oldOnIt)
            g = newOnIt;
    if (const ModifierGroup *g = s_->settings.modifierGroup(oldOnIt)) {
        ModifierGroup own = *g;
        own.id = newOnIt;
        s_->settings.modifierGroups.push_back(std::move(own));
        settingsChanged();
    }
    // And its sizes.
    for (std::string &g : copy.modifierGroups)
        if (g == sizeGroupId(it->id))
            g = sizeGroupId(copy.id);
    if (const ModifierGroup *g = s_->settings.modifierGroup(sizeGroupId(it->id))) {
        ModifierGroup own = *g;
        own.id = sizeGroupId(copy.id);
        s_->settings.modifierGroups.push_back(std::move(own));
        settingsChanged();
    }
    const int at = int(it - menu.begin()) + 1;
    menu.insert(menu.begin() + at, copy);
    if (s_->sink)
        for (int i = at; i < int(menu.size()); ++i)
            s_->sink->saveMenuItem(menu[i], i);
    menuChanged();
    emit notice(tr("%1 added: change its name and price").arg(name));
    return true;
}

// "Tacos: Carne Asada 3.50, Al Pastor 3.25" or one per line ("Horchata 2.75"):
// before a colon, the category (made if there's none); each item's price last.
int PosService::addMenuItemsFromText(const QString &categoryId, const QString &text)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return 0;
    const MenuStep step(this, tr("Add several"));
    static const QRegularExpression priced(uR"(^(.*?)[\s\-–:]*\$?\s*(\d+(?:[.,]\d{1,2})?)\s*$)"_s);
    QString category = categoryId;
    struct Entry { QString category, name; double price; };
    std::vector<Entry> entries;
    QStringList problems;
    for (QString line : text.split(u'\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        // "Tacos:" (or "Tacos: ..."): what follows goes in it.
        if (const qsizetype colon = line.indexOf(u':'); colon > 0 && !line.left(colon).contains(QRegularExpression(u"\\d"_s))) {
            const QString name = line.left(colon).trimmed();
            line = line.mid(colon + 1).trimmed();
            const auto cats = s_->categories();
            const auto c = std::ranges::find_if(cats, [&](const MenuCategory &x) {
                return QString::compare(qs(x.name), name, Qt::CaseInsensitive) == 0 || qs(x.id) == name;
            });
            if (c != cats.end()) {
                category = qs(c->id);
            } else {
                if (!saveCategory({{u"name"_s, name}}))
                    return 0;
                for (const MenuCategory &x : s_->categories())
                    if (qs(x.name) == name)
                        category = qs(x.id);
            }
            if (line.isEmpty())
                continue;
        }
        // Commas between items; "4,25" is a price (a comma then cents).
        static const QRegularExpression between(uR"(,(?!\d{1,2}\s*(?:,|$)))"_s);
        for (QString part : line.split(between, Qt::SkipEmptyParts)) {
            part = part.trimmed();
            if (part.isEmpty())
                continue;
            const auto m = priced.match(part);
            if (!m.hasMatch() || m.captured(1).trimmed().isEmpty()) {
                problems << part;
                continue;
            }
            entries.push_back({category, m.captured(1).trimmed(), m.captured(2).replace(u',', u'.').toDouble()});
        }
    }
    if (!problems.isEmpty()) {
        fail(tr("Give each its price, like \"Al Pastor 3.25\": %1").arg(problems.join(u", "_s)));
        return 0;
    }
    if (category.isEmpty() && !entries.empty() && entries.front().category.isEmpty()) {
        fail(tr("Choose a category first, or start with one: \"Tacos: Carne Asada 3.50, ...\"."));
        return 0;
    }
    int added = 0;
    QStringList skipped;
    for (const Entry &e : entries) {
        if (std::ranges::any_of(s_->menu, [&](const MenuItem &x) { return QString::compare(qs(x.name), e.name, Qt::CaseInsensitive) == 0; })) {
            skipped << e.name;
            continue;
        }
        if (!saveMenuItemCard({{u"name"_s, e.name}, {u"price"_s, QString::number(e.price, 'f', 2)}, {u"family"_s, e.category}}))
            return added;
        ++added;
    }
    emit notice(skipped.isEmpty() ? tr("Added %n item(s)", nullptr, added)
                                  : tr("Added %n item(s); already on the menu: %1", nullptr, added).arg(skipped.join(u", "_s)));
    return added;
}

// Rows read from a spreadsheet or another POS's menu
// (LayoutController::readMenuFile): each in its category (made if new; none
// named: `categoryId`); an item already on the menu is skipped, or gets the
// new price with `updatePrices`. `groups` ({name, min, max, options}): the
// choice groups the rows ask for by name; one of that name here is used as
// it is.
int PosService::importMenuRows(const QVariantList &rows, const QString &categoryId, bool updatePrices, const QVariantList &groups)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return 0;
    const MenuStep step(this, tr("Import"));
    int added = 0, updated = 0;
    QStringList skipped, noGroup;
    const auto groupNamed = [&](const QString &name) -> std::string {
        for (const ModifierGroup &g : s_->settings.modifierGroups)
            if (!isOwnGroup(g.id) && QString::compare(qs(g.name), name, Qt::CaseInsensitive) == 0)
                return g.id;
        return {};
    };
    // A barcode another item has already is left off (with its owner named).
    QStringList taken;
    const auto freeBarcodes = [&](const QString &text) {
        QStringList out;
        for (const QString &c : barcodeList(text)) {
            const auto owner = std::ranges::find_if(s_->menu, [&](const MenuItem &m) {
                return std::ranges::any_of(m.barcodes, [&](const std::string &b) { return sameBarcode(b, ss(c)); });
            });
            if (owner == s_->menu.end())
                out << c;
            else
                taken << tr("%1 (%2's)").arg(c, qs(owner->name));
        }
        return out.join(u", "_s);
    };
    for (const QVariant &v : groups) {
        const QVariantMap g = v.toMap();
        const QString name = g.value(u"name"_s).toString().trimmed();
        const bool asked = std::ranges::any_of(rows, [&](const QVariant &r) {
            return r.toMap().value(u"groups"_s).toStringList().contains(name, Qt::CaseInsensitive);
        });
        if (name.isEmpty() || !asked || !groupNamed(name).empty())
            continue;
        if (!saveChoiceGroup({{u"name"_s, name}, {u"min"_s, g.value(u"min"_s)}, {u"max"_s, g.value(u"max"_s)},
                              {u"options"_s, g.value(u"options"_s)}}))
            return 0;
    }
    for (const QVariant &v : rows) {
        const QVariantMap r = v.toMap();
        const QString name = r.value(u"name"_s).toString().trimmed();
        const double price = r.value(u"price"_s).toDouble();
        QString category = categoryId;
        if (const QString wanted = r.value(u"category"_s).toString().trimmed(); !wanted.isEmpty()) {
            const auto cats = s_->categories();
            const auto c = std::ranges::find_if(cats, [&](const MenuCategory &x) {
                return QString::compare(qs(x.name), wanted, Qt::CaseInsensitive) == 0 || qs(x.id) == wanted;
            });
            if (c == cats.end()) {
                if (!saveCategory({{u"name"_s, wanted}}))
                    return added;
                for (const MenuCategory &x : s_->categories())
                    if (qs(x.name) == wanted)
                        category = qs(x.id);
            } else {
                category = qs(c->id);
            }
        }
        if (category.isEmpty()) {
            fail(tr("%1 has no category: choose one, or add a Category column.").arg(name));
            return added;
        }
        auto existing = std::ranges::find_if(s_->menu, [&](const MenuItem &m) {
            return !m.isModifier && QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0;
        });
        // Its photo, into the store's pictures.
        QString photo;
        if (const QString data = r.value(u"photoData"_s).toString(); !data.isEmpty())
            photo = putMenuPicture(r.value(u"photoName"_s).toString(), QByteArray::fromBase64(data.toLatin1()));
        if (existing != s_->menu.end()) {
            // One already on the menu: a photo if it has none; the price if asked.
            QVariantMap card{{u"id"_s, qs(existing->id)}, {u"name"_s, qs(existing->name)}};
            if (!photo.isEmpty() && existing->image.empty())
                card.insert(u"image"_s, photo);
            if (const QString b = freeBarcodes(r.value(u"barcodes"_s).toString()); !b.isEmpty() && existing->barcodes.empty())
                card.insert(u"barcodes"_s, b);
            if (updatePrices)
                card.insert(u"price"_s, QString::number(price, 'f', 2));
            if (card.size() == 2) {
                skipped << name;
                continue;
            }
            if (!saveMenuItemCard(card))
                return added;
            ++updated;
            continue;
        }
        QVariantMap card{{u"name"_s, name}, {u"price"_s, QString::number(price, 'f', 2)}, {u"family"_s, category}};
        if (!photo.isEmpty())
            card.insert(u"image"_s, photo);
        if (const QString d = r.value(u"description"_s).toString().trimmed(); !d.isEmpty())
            card.insert(u"description"_s, d);
        if (const QString on = r.value(u"onIt"_s).toString().trimmed(); !on.isEmpty())
            card.insert(u"onIt"_s, on);
        if (const QString b = freeBarcodes(r.value(u"barcodes"_s).toString()); !b.isEmpty())
            card.insert(u"barcodes"_s, b);
        if (const QString s = r.value(u"section"_s).toString().simplified(); !s.isEmpty())
            card.insert(u"section"_s, s);
        // Its PLU, unless another item has it.
        if (const QString n = r.value(u"number"_s).toString().trimmed();
            !n.isEmpty() && std::ranges::none_of(s_->menu, [&](const MenuItem &m) { return qs(m.number) == n; }))
            card.insert(u"number"_s, n);
        if (r.value(u"alcohol"_s).toBool())
            card.insert(u"taxClass"_s, u"alcohol"_s);
        if (const QVariantList sizes = r.value(u"sizes"_s).toList(); sizes.size() >= 2) {
            QVariantList list;
            for (const QVariant &s : sizes)
                list.append(QVariantMap{{u"name"_s, s.toMap().value(u"name"_s)},
                                        {u"price"_s, QString::number(s.toMap().value(u"price"_s).toDouble(), 'f', 2)}});
            card.insert(u"sizes"_s, list);
        }
        QStringList asks;
        for (const QString &g : r.value(u"groups"_s).toStringList()) {
            if (const std::string id = groupNamed(g); !id.empty())
                asks << qs(id);
            else if (!noGroup.contains(g, Qt::CaseInsensitive))
                noGroup << g;
        }
        if (!asks.isEmpty())
            card.insert(u"groups"_s, asks);
        if (!saveMenuItemCard(card))
            return added;
        ++added;
    }
    QString what = tr("Added %n item(s)", nullptr, added);
    if (updated)
        what += u"; "_s + tr("%n changed", nullptr, updated);
    if (!skipped.isEmpty())
        what += u"; "_s + tr("already on the menu: %1").arg(skipped.join(u", "_s));
    if (!taken.isEmpty())
        what += u"; "_s + tr("barcodes left off, another item has them: %1").arg(taken.join(u", "_s));
    // Square's library names its modifier sets but not what's in them.
    if (!noGroup.isEmpty())
        what += u"; "_s + tr("the file doesn't say what's in %1: make them in Choice Groups, then pick them on the items")
                              .arg(noGroup.join(u", "_s));
    emit notice(what);
    return added + updated;
}

// --- a whole menu, to another store ---------------------------------------------------

// The menu as a file: categories, choice groups and items with every
// setting; what's this store's alone stays (sales so far, stock recipes,
// photos, today's 86).
QVariantMap PosService::menuExport()
{
    if (!require(perm::Manager, tr("Copying the menu")))
        return {};
    PosSettings only;
    only.menuCategories = s_->categories();
    QJsonArray items;
    QVariantMap pictures;
    for (const MenuItem &m : s_->menu) {
        QJsonObject o = toJson(m);
        for (const char *k : {"ticketsSoldBefore", "autoSoldOut", "soldOutToday", "recipe"})
            o.remove(QLatin1String(k));
        // Its photo goes along (the store's pictures, by name); other kinds stay.
        if (!m.image.starts_with("store:") || !s_->images.contains(m.image.substr(6)))
            o.remove(u"image"_s);
        else
            pictures.insert(qs(m.image.substr(6)), QString::fromLatin1(s_->images.at(m.image.substr(6)).toBase64()));
        items.append(o);
    }
    return {{u"format"_s, u"viewtouch-menu"_s}, {u"version"_s, 1}, {u"store"_s, storeName()},
            {u"exported"_s, QDateTime::fromMSecsSinceEpoch(now()).toString(Qt::ISODate)},
            {u"categories"_s, toJson(only).value(u"menuCategories").toArray().toVariantList()},
            {u"choiceGroups"_s, modifierGroupsToJson(s_->settings.modifierGroups).toVariantList()},
            {u"items"_s, items.toVariantList()}, {u"pictures"_s, pictures}};
}

// Another store's menu file: what isn't here by name is added (categories,
// choice groups, items); nothing here changes. Returns the items added.
int PosService::importMenuFile(const QVariantMap &file)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return 0;
    const MenuStep step(this, tr("Import a menu"));
    if (file.value(u"format"_s).toString() != u"viewtouch-menu"_s) {
        fail(tr("That isn't a ViewTouch menu file."));
        return 0;
    }
    const auto sameName = [](const std::string &a, const QString &b) { return QString::compare(qs(a), b, Qt::CaseInsensitive) == 0; };

    // Categories: the one by that name, or a new one.
    PosSettings read = settingsFromJson(QJsonObject{
        {u"menuCategories"_s, QJsonArray::fromVariantList(file.value(u"categories"_s).toList())},
        {u"modifierGroups"_s, QJsonArray::fromVariantList(file.value(u"choiceGroups"_s).toList())}});
    QHash<std::string, std::string> categoryOf;
    for (const MenuCategory &c : read.menuCategories) {
        const auto here = s_->categories();
        if (const auto it = std::ranges::find_if(here, [&](const MenuCategory &x) { return sameName(x.name, qs(c.name)); });
            it != here.end()) {
            categoryOf[c.id] = it->id;
            continue;
        }
        QStringList periods;   // the meal periods this store has (others: all day)
        for (const std::string &p : c.periods)
            if (std::ranges::any_of(s_->settings.mealPeriods, [&](const MealPeriod &m) { return m.id == p; }))
                periods << qs(p);
        if (!saveCategory({{u"name"_s, qs(c.name)}, {u"color"_s, qs(c.color)}, {u"periods"_s, periods},
                           {u"printer"_s, qs(c.printer)}, {u"station"_s, qs(c.station)}, {u"taxClass"_s, qs(c.taxClass)},
                           {u"buttonSize"_s, qs(c.buttonSize)}, {u"photos"_s, c.photos}, {u"hidePrice"_s, c.hidePrice}, {u"shades"_s, c.shades},
                           {u"textSize"_s, c.textSize}}))
            return 0;
        for (const MenuCategory &x : s_->categories())
            if (sameName(x.name, qs(c.name)))
                categoryOf[c.id] = x.id;
    }

    const QVariantMap pictures = file.value(u"pictures"_s).toMap();
    // Items: the one by that name here, or a new id.
    std::vector<MenuItem> items;
    for (const QVariant &v : file.value(u"items"_s).toList())
        items.push_back(menuItemFromJson(QJsonObject::fromVariantMap(v.toMap())));
    QHash<std::string, std::string> itemOf;
    std::vector<std::string> taken;
    QStringList skipped;
    std::vector<MenuItem *> adding;
    for (MenuItem &m : items) {
        if (const auto it = std::ranges::find_if(s_->menu, [&](const MenuItem &x) { return sameName(x.name, qs(m.name)); });
            it != s_->menu.end()) {
            itemOf[m.id] = it->id;
            skipped << qs(m.name);
            continue;
        }
        const std::string id = freeId(qs(m.name), [&](const std::string &x) {
            return std::ranges::any_of(s_->menu, [&](const MenuItem &o) { return o.id == x; })
                   || std::ranges::find(taken, x) != taken.end();
        });
        taken.push_back(id);
        itemOf[m.id] = id;
        adding.push_back(&m);
    }

    // Choice groups: the one by that name here, or added; an item's own
    // (What's on it) goes with the item.
    QHash<std::string, std::string> groupOf;
    const auto withItems = [&](ModifierGroup g) {
        for (ModifierOption &o : g.options)
            if (!o.itemId.empty())
                o.itemId = itemOf.value(o.itemId, o.itemId);
        return g;
    };
    for (const ModifierGroup &g : read.modifierGroups) {
        if (isOwnGroup(g.id))
            continue;
        auto &list = s_->settings.modifierGroups;
        if (const auto it = std::ranges::find_if(list, [&](const ModifierGroup &x) {
                return !isOwnGroup(x.id) && sameName(x.name, qs(g.name)); });
            it != list.end()) {
            groupOf[g.id] = it->id;
            continue;
        }
        ModifierGroup copy = withItems(g);
        copy.id = freeId(qs(g.name), [&](const std::string &x) {
            return std::ranges::any_of(list, [&](const ModifierGroup &o) { return o.id == x; });
        });
        groupOf[g.id] = copy.id;
        list.push_back(std::move(copy));
    }

    for (MenuItem *m : adding) {
        MenuItem item = *m;
        const std::string oldId = item.id;
        item.id = itemOf.value(oldId);
        item.family = categoryOf.value(item.family, item.family);
        item.soldOutToday = false;
        // Its photo: into this store's pictures (by its name, or one of its own).
        if (item.image.starts_with("store:")) {
            const QString name = qs(item.image.substr(6));
            const QString data = pictures.value(name).toString();
            item.image = data.isEmpty() ? std::string() : ss(putMenuPicture(name, QByteArray::fromBase64(data.toLatin1())));
        } else {
            item.image.clear();
        }
        std::vector<std::string> groups;
        for (const std::string &g : item.modifierGroups) {
            if (g == onItGroupId(oldId) || g == sizeGroupId(oldId)) {
                const auto own = std::ranges::find_if(read.modifierGroups, [&](const ModifierGroup &x) { return x.id == g; });
                if (own == read.modifierGroups.end())
                    continue;
                ModifierGroup copy = withItems(*own);
                copy.id = g == sizeGroupId(oldId) ? sizeGroupId(item.id) : onItGroupId(item.id);
                std::erase_if(s_->settings.modifierGroups, [&](const ModifierGroup &x) { return x.id == copy.id; });
                groups.push_back(copy.id);
                s_->settings.modifierGroups.push_back(std::move(copy));
            } else if (groupOf.contains(g)) {
                groups.push_back(groupOf.value(g));
            }
        }
        item.modifierGroups = std::move(groups);
        s_->menu.push_back(std::move(item));
        if (s_->sink)
            s_->sink->saveMenuItem(s_->menu.back(), int(s_->menu.size()) - 1);
    }
    settingsChanged();
    menuChanged();
    const int added = int(adding.size());
    emit notice(skipped.isEmpty() ? tr("Added %n item(s)", nullptr, added)
                                  : tr("Added %n item(s); already on the menu: %1", nullptr, added).arg(skipped.join(u", "_s)));
    return added;
}

// Several items at once (the Menu Builder's Select): the same change to
// each (any card field: family, buttonColor, available, taxClass, printer,
// price...), or removed; one Undo step. Prices can also move by an amount
// (priceAdd, in cents, may be negative) or a share (pricePercent, in basis
// points: 500 = 5% more, -1000 = 10% less), each item from its own price,
// rounded to the cent and never below zero.
bool PosService::changeMenuItems(const QStringList &ids, const QVariantMap &changes)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    for (const QString &id : ids)
        if (!findItem(id))
            return fail(tr("'%1' is not on the menu.").arg(id));
    const bool adding = changes.contains(u"priceAdd"_s), scaling = changes.contains(u"pricePercent"_s);
    const qint64 add = changes.value(u"priceAdd"_s).toLongLong();
    const qint64 bp = changes.value(u"pricePercent"_s).toLongLong();
    if (scaling && (bp <= -10000 || bp > 100000))
        return fail(tr("A price change is between -99% and +1000%."));
    const MenuStep step(this, tr("Change %n item(s)", "", int(ids.size())));
    for (const QString &id : ids) {
        const MenuItem *was = findItem(id);
        QVariantMap card = changes;
        card.remove(u"priceAdd"_s);
        card.remove(u"pricePercent"_s);
        if (adding || scaling) {
            Money price = was->price;
            if (scaling)
                price += price.percent(bp);
            if (adding)
                price += Money::fromCents(add);
            card.insert(u"price"_s, QString::number(std::max<qint64>(0, price.cents()) / 100.0, 'f', 2));
        }
        card.insert(u"id"_s, id);
        card.insert(u"name"_s, qs(was->name));
        if (!saveMenuItemCard(card))
            return false;
    }
    emit notice(tr("%n item(s) changed", "", int(ids.size())));
    return true;
}

bool PosService::removeMenuItems(const QStringList &ids)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    for (const QString &id : ids)
        if (!findItem(id))
            return fail(tr("'%1' is not on the menu.").arg(id));
    const MenuStep step(this, tr("Remove %n item(s)", "", int(ids.size())));
    for (const QString &id : ids)
        if (!deleteMenuItemCard(id))
            return false;
    emit notice(tr("%n item(s) removed from the menu", "", int(ids.size())));
    return true;
}

bool PosService::deleteMenuItemCard(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Remove an item"));
    auto &menu = s_->menu;
    const auto it = std::ranges::find_if(menu, [&](const MenuItem &m) { return qs(m.id) == id; });
    if (it == menu.end())
        return fail(tr("'%1' is not on the menu.").arg(id));
    const QString name = qs(it->name);
    const std::string onIt = onItGroupId(it->id), sizes = sizeGroupId(it->id);
    menu.erase(it);
    if (s_->sink)
        s_->sink->deleteMenuItem(ss(id));
    if (std::erase_if(s_->settings.modifierGroups, [&](const ModifierGroup &g) { return g.id == onIt || g.id == sizes; }) > 0)
        settingsChanged();
    menuChanged();
    emit notice(tr("%1 removed from the menu").arg(name));
    return true;
}

} // namespace vt::app
