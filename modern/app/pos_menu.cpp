// PosService: the menu while ordering - modifier groups (choose Temperature,
// Toppings...), prices that change with the meal period, and 86'ing.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

std::string PosService::currentMealPeriod() const
{
    const QTime t = QDateTime::fromMSecsSinceEpoch(now()).time();
    return mealPeriodAt(s_->settings.mealPeriods, t.hour() * 60 + t.minute());
}

// --- choosing modifiers ---------------------------------------------------------------------

namespace {
// Choices made in a group ("No Onion" leaves something off: it isn't one).
// Choices made in a group: not "No", and not how something that comes on
// it is had (Lite Mayo isn't a choice of topping).
int chosenIn(const OrderLine &l, const ModifierGroup &group)
{
    return int(std::ranges::count_if(l.modifiers, [&](const Modifier &m) {
        if (m.group != group.id || m.qualifier == Qualifier::No)
            return false;
        return std::ranges::none_of(group.options, [&](const ModifierOption &o) { return o.included && o.name == m.name; });
    }));
}
} // namespace

QVariantMap PosService::choosingInfo() const
{
    const Check *c = currentCheck();
    const OrderLine *l = c ? c->line(choosingLine_) : nullptr;
    if (!l)
        return {{u"active"_s, false}};
    const MenuItem *item = findItem(qs(l->itemId));
    QVariantList groups;
    for (const std::string &gid : item ? item->modifierGroups : std::vector<std::string>{}) {
        const ModifierGroup *g = s_->settings.modifierGroup(gid);
        if (!g)
            continue;
        QVariantList options;
        for (int i = 0; i < int(g->options.size()); ++i) {
            const ModifierOption &o = g->options[i];
            const auto it = std::ranges::find_if(l->modifiers, [&](const Modifier &m) {
                return m.group == g->id && m.name == o.name;
            });
            const bool chosen = it != l->modifiers.end();
            const MenuItem *linked = o.itemId.empty() ? nullptr : findItem(qs(o.itemId));
            options.append(QVariantMap{{u"index"_s, i}, {u"name"_s, qs(o.name)},
                                       {u"price"_s, optionPrice(o, *l).cents() ? format(optionPrice(o, *l)) : QString()},
                                       {u"chosen"_s, chosen}, {u"soldOut"_s, linked && !linked->available},
                                       // "No", "Extra"...: how it was chosen (and the price that way)
                                       {u"qualifier"_s, chosen ? qs(qualifierPrefix(it->qualifier)).trimmed() : QString()},
                                       {u"chosenPrice"_s, chosen && it->price().cents() ? format(it->price()) : QString()},
                                       // Comes on it; how it's had: "", no, lite, extra, side.
                                       {u"included"_s, o.included},
                                       {u"how"_s, chosen ? qs(toString(it->qualifier)) : QString()}});
        }
        const int n = chosenIn(*l, *g);
        const QString rule = g->min == 1 && g->max == 1 ? tr("Choose 1")
                             : g->min > 0               ? tr("Choose at least %1").arg(g->min)
                             : g->max == 1              ? tr("Optional")
                             : g->max > 1               ? tr("Up to %1").arg(g->max)
                                                        : tr("Any");
        const bool anyIncluded = std::ranges::any_of(g->options, &ModifierOption::included);
        groups.append(QVariantMap{{u"id"_s, qs(g->id)}, {u"name"_s, qs(g->name)}, {u"rule"_s, rule},
                                  {u"chosen"_s, n}, {u"done"_s, n >= g->min}, {u"options"_s, options},
                                  {u"askHow"_s, g->askHow || anyIncluded}, {u"included"_s, anyIncluded},
                                  // Only what comes on it: nothing to choose (no rule to show).
                                  {u"onlyIncluded"_s, std::ranges::all_of(g->options, &ModifierOption::included)}});
    }
    return {{u"active"_s, true}, {u"lineId"_s, qint64(l->id)}, {u"item"_s, qs(l->displayName())},
            {u"groups"_s, groups},
            // Already on the check (Choose): Cancel Changes puts it back, not Cancel Item.
            {u"editing"_s, choosingBefore_.has_value() && choosingBefore_->id == l->id}};
}

QString PosService::missingChoice(const std::vector<OrderLine> &lines) const
{
    for (const OrderLine &l : lines) {
        const MenuItem *item = l.isComment() || l.voided ? nullptr : findItem(qs(l.itemId));
        if (!item)
            continue;
        for (const std::string &gid : item->modifierGroups) {
            const ModifierGroup *g = s_->settings.modifierGroup(gid);
            if (g && g->min > 0 && chosenIn(l, *g) < g->min)
                return tr("%1 needs a %2. Touch it, then choose.").arg(qs(l.displayName()), qs(g->name));
        }
    }
    return {};
}

bool PosService::chooseLine(qint64 lineId)
{
    Check *c = current();
    const OrderLine *l = c ? c->line(lineId) : nullptr;
    if (!l || l->sent)
        return fail(tr("Only items not yet sent can be changed."));
    const MenuItem *item = findItem(qs(l->itemId));
    if (!item || item->modifierGroups.empty())
        return fail(tr("%1 has no choices.").arg(qs(l->displayName())));
    choosingLine_ = l->id;
    choosingBefore_ = *l;   // Cancel Changes puts it back
    selectedLine_ = l->id;
    emit checkChanged();
    return true;
}

bool PosService::chooseOption(const QString &groupId, int index)
{
    Check *c = current();
    OrderLine *l = c ? c->line(choosingLine_) : nullptr;
    if (!l)
        return fail(tr("Nothing to choose for."));
    const ModifierGroup *g = s_->settings.modifierGroup(ss(groupId));
    if (!g || index < 0 || index >= int(g->options.size()))
        return false;
    const ModifierOption &o = g->options[index];
    const MenuItem *linked = o.itemId.empty() ? nullptr : findItem(qs(o.itemId));
    // A qualifier touched first: No onion, Extra cheese, Lite mayo, ranch on the Side.
    const Qualifier q = qualifier_;
    if (q != Qualifier::None) {
        qualifier_ = Qualifier::None;
        emit qualifierChanged();
    }
    if (q == Qualifier::Sub)
        return fail(tr("Sub is for menu items: touch Sub, then the item to swap in."));
    // Comes on it already: it's how it's had that matters.
    if (o.included && q == Qualifier::None
        && std::ranges::none_of(l->modifiers, [&](const Modifier &m) { return m.group == g->id && m.name == o.name; }))
        return fail(tr("%1 comes on it: touch No, Lite, Extra or Side first.").arg(qs(o.name)));
    // Touching a chosen option takes it off (or, with a qualifier, changes how it's had).
    auto same = [&](const Modifier &m) { return m.group == g->id && m.name == o.name; };
    const auto had = std::ranges::find_if(l->modifiers, same);
    if (had != l->modifiers.end() && (q == Qualifier::None || had->qualifier == q)) {
        std::erase_if(l->modifiers, same);
    } else {
        if (had != l->modifiers.end())
            std::erase_if(l->modifiers, same);   // the same option, another way
        // "No" leaves something off: it doesn't replace the group's choice or count toward it.
        if (q != Qualifier::No) {
            if (g->max == 1 && !o.included)   // one choice: the new one replaces it
                std::erase_if(l->modifiers, [&](const Modifier &m) { return m.group == g->id && m.qualifier != Qualifier::No; });
            else if (g->max > 1 && !o.included && chosenIn(*l, *g) >= g->max)
                return fail(tr("%1: up to %2.").arg(qs(g->name)).arg(g->max));
        }
        if (linked && !linked->available)
            return fail(tr("%1 is sold out.").arg(qs(o.name)));
        Modifier m;
        m.itemId = o.itemId;   // its stock (a combo's side or drink)
        if (linked)
            m.station = linked->station;   // made at the fryer, say
        m.name = o.name;
        m.unitPrice = q == Qualifier::Extra ? s_->settings.withExtra(optionPrice(o, *l)) : optionPrice(o, *l);
        m.qualifier = q;
        m.group = g->id;
        m.kitchenName = o.kitchenName;
        m.kitchenHide = o.kitchenHide;
        // A combo's part goes on the combo's ticket only if it is made there
        // too: a drink poured at the counter (or the bar) isn't the kitchen's.
        if (linked) {
            const auto where = [](const std::string &p) { return p.empty() ? std::string("kitchen") : p; };
            if (linked->kitchenHide || where(linked->printer) != where(l->printer))
                m.kitchenHide = true;
        }
        // Keep the group's choices together, in the order of the groups.
        l->modifiers.push_back(m);
    }
    repriceForSize(*l);   // a size chosen (or changed): its choices' prices follow
    changed(*c);
    return true;
}

// The size chosen on this line (its item's own Size choice), lower case; "" none.
std::string PosService::sizeOf(const OrderLine &l) const
{
    for (const Modifier &m : l.modifiers)
        if (m.group.starts_with("size-"))
            return QString::fromStdString(m.name).toLower().toStdString();
    return {};
}

Money PosService::optionPrice(const ModifierOption &o, const OrderLine &l) const
{
    if (o.sizePrices.empty())
        return o.price;
    const auto it = o.sizePrices.find(sizeOf(l));
    return it == o.sizePrices.end() ? o.price : it->second;
}

void PosService::repriceForSize(OrderLine &l) const
{
    for (Modifier &m : l.modifiers) {
        if (m.group.starts_with("size-"))
            continue;
        const ModifierGroup *g = s_->settings.modifierGroup(m.group);
        const auto o = g ? std::ranges::find_if(g->options, [&](const ModifierOption &x) { return x.name == m.name; })
                         : std::vector<ModifierOption>::const_iterator{};
        if (!g || o == g->options.end() || o->sizePrices.empty())
            continue;
        const Money base = optionPrice(*o, l);
        m.unitPrice = m.qualifier == Qualifier::Extra ? s_->settings.withExtra(base) : base;
    }
}

bool PosService::chooseOptionAs(const QString &groupId, int index, const QString &qualifier)
{
    qualifier_ = qualifierFromString(ss(qualifier));
    return chooseOption(groupId, index);
}

bool PosService::setChoice(const QString &groupId, int index, const QString &how)
{
    Check *c = current();
    OrderLine *l = c ? c->line(choosingLine_) : nullptr;
    if (!l)
        return fail(tr("Nothing to choose for."));
    const ModifierGroup *g = s_->settings.modifierGroup(ss(groupId));
    if (!g || index < 0 || index >= int(g->options.size()))
        return false;
    const ModifierOption &o = g->options[index];
    const bool anyIncluded = std::ranges::any_of(g->options, &ModifierOption::included);
    // How it can be had: off/as it comes; plain (an add-on); Lite, Extra,
    // Side where the group allows; No only for what comes on it.
    static const QStringList hows{u"off"_s, u""_s, u"lite"_s, u"extra"_s, u"side"_s, u"no"_s};
    if (!hows.contains(how) || (how == u"no" && !o.included) || (o.included && how.isEmpty())
        || ((how == u"lite" || how == u"extra" || how == u"side") && !g->askHow && !anyIncluded))
        return fail(tr("%1 can't be had that way.").arg(qs(o.name)));
    const auto same = [&](const Modifier &m) { return m.group == g->id && m.name == o.name; };
    if (how == u"off") {   // as it comes (or not added)
        std::erase_if(l->modifiers, same);
        repriceForSize(*l);
        changed(*c);
        return true;
    }
    const std::vector<Modifier> before = l->modifiers;
    std::erase_if(l->modifiers, same);
    if (!chooseOptionAs(groupId, index, how)) {
        l->modifiers = before;
        return false;
    }
    return true;
}

bool PosService::finishChoosing()
{
    const QVariantMap info = choosingInfo();
    if (!info.value(u"active"_s).toBool())
        return true;
    for (const QVariant &v : info.value(u"groups"_s).toList()) {
        const QVariantMap g = v.toMap();
        if (!g.value(u"done"_s).toBool())
            return fail(tr("%1: %2.").arg(g.value(u"name"_s).toString(), g.value(u"rule"_s).toString().toLower()));
    }
    choosingLine_ = 0;
    choosingBefore_.reset();
    emit checkChanged();
    return true;
}

bool PosService::cancelChoosing()
{
    Check *c = current();
    if (!c || choosingLine_ == 0)
        return true;
    OrderLine *l = c->line(choosingLine_);
    if (l && choosingBefore_ && choosingBefore_->id == l->id) {
        *l = *choosingBefore_;   // an item already ordered: its choices as they were (never off the check)
    } else {
        c->removeLine(choosingLine_);   // just added: the item comes off
        selectedLine_ = 0;
    }
    choosingLine_ = 0;
    choosingBefore_.reset();
    changed(*c);
    return true;
}

// --- 86 (sold out) ----------------------------------------------------------------------------

bool PosService::setAvailable(const QString &itemId, bool available)
{
    if (!require(perm::Order, tr("Marking items sold out")))
        return false;
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (qs(m.id) != itemId)
            continue;
        if (m.available == available)
            return true;
        m.available = available;
        m.autoSoldOut = false;   // a person decided
        m.soldOutToday = !available;
        if (s_->sink)
            s_->sink->saveMenuItem(m, i);
        emit notice(available ? tr("%1 is back").arg(qs(m.name)) : tr("%1 is sold out (86)").arg(qs(m.name)));
        ++s_->adminRevision;
        emit s_->adminChanged();
        return true;
    }
    return fail(tr("'%1' is not on the menu.").arg(itemId));
}

int PosService::ticketsSold(const MenuItem &item) const
{
    // Earlier days', plus every ticket on today's checks, open or closed.
    int sold = item.ticketsSoldBefore;
    const auto count = [&](const Check &c) {
        if (c.training)
            return;
        for (const OrderLine &l : c.lines)
            if (!l.voided && l.itemId == item.id)
                sold += l.counted();
    };
    for (const auto &[id, c] : s_->open)
        count(c);
    for (const Check &c : s_->closedToday)
        count(c);
    return sold;
}

int PosService::ticketsLeft(const MenuItem &item) const
{
    return item.ticketCapacity > 0 ? std::max(0, item.ticketCapacity - ticketsSold(item)) : -1;
}

bool PosService::moveMenuItem(const QString &id, int by)
{
    if (!require(perm::Manager, tr("Arranging the menu")))
        return false;
    auto &menu = s_->menu;
    const auto it = std::ranges::find_if(menu, [&](const MenuItem &m) { return qs(m.id) == id; });
    if (it == menu.end())
        return fail(tr("'%1' is not on the menu.").arg(id));
    // The next (or previous) item of the same family: they trade places.
    const int from = int(it - menu.begin());
    int to = from;
    for (int i = from + (by > 0 ? 1 : -1); i >= 0 && i < int(menu.size()); i += by > 0 ? 1 : -1)
        if (menu[i].family == it->family && !menu[i].isModifier) {
            to = i;
            break;
        }
    if (to == from)
        return true;   // already first (or last)
    std::swap(menu[from], menu[to]);
    if (s_->sink) {
        s_->sink->saveMenuItem(menu[from], from);
        s_->sink->saveMenuItem(menu[to], to);
    }
    ++s_->adminRevision;
    emit s_->adminChanged();
    return true;
}

// A color mixed in a color picker: first among the store's own (a dozen kept).
bool PosService::addCustomColor(const QString &color)
{
    if (!require(perm::Manager, tr("Arranging the menu")))
        return false;
    static const QRegularExpression hex(u"^#[0-9a-fA-F]{6}$"_s);
    if (!hex.match(color).hasMatch())
        return fail(tr("That isn't a color."));
    const std::string c = ss(color.toLower());
    auto &list = s_->settings.customColors;
    if (!list.empty() && list.front() == c)
        return true;
    std::erase(list, c);
    list.insert(list.begin(), c);
    if (list.size() > 12)
        list.resize(12);
    settingsChanged();
    ++s_->adminRevision;
    emit s_->adminChanged();
    return true;
}

bool PosService::setMenuItemColor(const QString &id, const QString &color)
{
    if (!require(perm::Manager, tr("Arranging the menu")))
        return false;
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (qs(m.id) != id)
            continue;
        static const QRegularExpression hex(u"^#[0-9a-fA-F]{6}$"_s);
        if (!color.isEmpty() && !hex.match(color).hasMatch())
            return fail(tr("That isn't a color."));
        m.buttonColor = ss(color);
        if (s_->sink)
            s_->sink->saveMenuItem(m, i);
        ++s_->adminRevision;
        emit s_->adminChanged();
        return true;
    }
    return fail(tr("'%1' is not on the menu.").arg(id));
}

QStringList PosService::popularItemsNow() const
{
    // Today's checks, closed and open (not practice): how many of each item.
    std::map<std::string, int> sold;
    const auto count = [&](const Check &c) {
        if (c.training || c.status == CheckStatus::Discarded || c.status == CheckStatus::Merged)
            return;
        for (const OrderLine &l : c.lines)
            if (!l.isComment() && !l.voided && !l.isGiftCard())
                sold[l.itemId] += l.counted();
    };
    for (const Check &c : s_->closedToday)
        count(c);
    for (const auto &[id, c] : s_->open)
        count(c);
    std::vector<std::pair<int, std::string>> ranked;
    for (const auto &[id, n] : sold)
        if (const MenuItem *m = findItem(qs(id)); m && !m->isModifier)
            ranked.emplace_back(n, id);
    std::ranges::stable_sort(ranked, [](const auto &a, const auto &b) { return a.first > b.first; });
    QStringList out;
    for (const auto &[n, id] : ranked)
        if (out.size() < 24)
            out << qs(id);
    return out;
}

namespace {
// Each item on a check once (no modifiers, comments, voids or gift cards).
std::vector<std::string> itemsOn(const Check &c)
{
    std::vector<std::string> out;
    for (const OrderLine &l : c.lines)
        if (!l.isComment() && !l.voided && !l.isGiftCard() && std::ranges::find(out, l.itemId) == out.end())
            out.push_back(l.itemId);
    return out;
}

// How many of each item a check's server ordered on it.
void countByServer(const Check &c, std::map<std::string, std::map<std::string, int>> &byServer)
{
    if (c.training || c.serverId.empty() || c.status == CheckStatus::Discarded || c.status == CheckStatus::Merged)
        return;
    for (const OrderLine &l : c.lines)
        if (!l.isComment() && !l.voided && !l.isGiftCard() && !l.isFee())
            byServer[c.serverId][l.itemId] += l.counted();
}

void countTogether(const Check &c, std::map<std::string, int> &with, std::map<std::string, std::map<std::string, int>> &together)
{
    if (c.training || c.status == CheckStatus::Discarded || c.status == CheckStatus::Merged)
        return;
    const std::vector<std::string> items = itemsOn(c);
    for (const std::string &a : items) {
        ++with[a];
        for (const std::string &b : items)
            if (a != b)
                ++together[a][b];
    }
}
} // namespace

void PosService::askPastOnce() const
{
    // The past four weeks, once, off the screen's thread.
    if (s_->pastTogetherAsked || !s_->history)
        return;
    s_->pastTogetherAsked = true;
    auto history = s_->history;
    const std::int64_t to = now(), from = to - 28LL * 24 * 3600 * 1000;
    QPointer<PosShared> shared(s_);
    QThreadPool::globalInstance()->start([=] {
        std::map<std::string, int> with;
        std::map<std::string, std::map<std::string, int>> together, byServer;
        PosShared::RushPast rush;
        for (const Check &x : history(from, to)) {
            countTogether(x, with, together);
            countByServer(x, byServer);
            rush.count(x);
        }
        QMetaObject::invokeMethod(shared, [shared, with = std::move(with), together = std::move(together),
                                           byServer = std::move(byServer), rush = std::move(rush)]() mutable {
            if (!shared)
                return;
            shared->pastChecksWith = std::move(with);
            shared->pastTogether = std::move(together);
            shared->pastByServer = std::move(byServer);
            shared->rushPast = std::move(rush);
            emit shared->checksChanged();
        }, Qt::QueuedConnection);
    });
}

QStringList PosService::myUsuals() const
{
    const core::Employee *e = user();
    if (!e)
        return {};
    return s_->memo(u"myUsuals "_s + qs(e->id), [this, e] {
        askPastOnce();
        // The past weeks, and today's checks (closed and open) on top.
        std::map<std::string, std::map<std::string, int>> today;
        for (const Check &c : s_->closedToday)
            if (c.serverId == e->id)
                countByServer(c, today);
        for (const auto &[id, c] : s_->open)
            if (c.serverId == e->id)
                countByServer(c, today);
        std::map<std::string, int> n;
        if (const auto p = s_->pastByServer.find(e->id); p != s_->pastByServer.end())
            n = p->second;
        for (const auto &[item, k] : today[e->id])
            n[item] += k;
        std::vector<std::pair<int, std::string>> ranked;
        for (const auto &[id, k] : n)
            if (const MenuItem *m = findItem(qs(id)); m && !m->isModifier && k >= 2)
                ranked.emplace_back(k, id);
        std::ranges::stable_sort(ranked, [](const auto &a, const auto &b) { return a.first > b.first; });
        QStringList out;
        for (const auto &[k, id] : ranked)
            if (out.size() < 12)
                out << qs(id);
        return QVariant(out);
    }).toStringList();
}

QVariantList PosService::goesWith(const Check &c) const
{
    askPastOnce();
    // Today's, counted once as checks close (from the start again if one
    // was reopened or the day closed).
    const auto &closed = s_->closedToday;
    if (closed.size() < s_->todayCounted
        || (s_->todayCounted > 0 && closed[s_->todayCounted - 1].id != s_->todayLastId)) {
        s_->todayChecksWith.clear();
        s_->todayTogether.clear();
        s_->todayCounted = 0;
    }
    for (; s_->todayCounted < closed.size(); ++s_->todayCounted)
        countTogether(closed[s_->todayCounted], s_->todayChecksWith, s_->todayTogether);
    s_->todayLastId = closed.empty() ? 0 : closed.back().id;
    const auto withOf = [this](const std::string &a) {
        const auto p = s_->pastChecksWith.find(a), t = s_->todayChecksWith.find(a);
        return (p == s_->pastChecksWith.end() ? 0 : p->second) + (t == s_->todayChecksWith.end() ? 0 : t->second);
    };

    // For each item not on the check: how often it went with what's on it (the
    // most likely of them); at least 3 checks, and 1 in 4 of them.
    const std::vector<std::string> on = itemsOn(c);
    std::map<std::string, double> score;
    for (const std::string &a : on) {
        // Past and today together.
        std::map<std::string, int> pairs;
        for (const auto *source : {&s_->pastTogether, &s_->todayTogether})
            if (const auto t = source->find(a); t != source->end())
                for (const auto &[b, n] : t->second)
                    pairs[b] += n;
        const int checks = withOf(a);
        for (const auto &[b, n] : pairs)
            if (n >= 3 && checks > 0 && n * 4 >= checks && std::ranges::find(on, b) == on.end())
                score[b] = std::max(score[b], double(n) / checks);
    }
    std::vector<std::pair<double, std::string>> ranked;
    for (const auto &[id, s] : score)
        if (const MenuItem *m = findItem(qs(id)); m && !m->isModifier && m->available && ticketsLeft(*m) != 0)
            ranked.emplace_back(s, id);
    std::ranges::stable_sort(ranked, [](const auto &a, const auto &b) { return a.first > b.first; });
    QVariantList out;
    for (const auto &[s, id] : ranked) {
        if (out.size() == 3)
            break;
        out << QVariantMap{{u"id"_s, qs(id)}, {u"name"_s, qs(findItem(qs(id))->name)}};
    }
    return out;
}

QStringList PosService::soldOut() const
{
    QStringList out;
    for (const MenuItem &m : s_->menu) {
        if (!m.available || ticketsLeft(m) == 0)
            out << qs(m.id) << qs(m.name).toLower();
    }
    return out;
}

namespace {

// The groups an item asks for (not its own What's on it).
QStringList groupsOf(const MenuItem &m)
{
    QStringList out;
    for (const std::string &g : m.modifierGroups)
        if (g != "on-" + m.id && g != "size-" + m.id)
            out << QString::fromStdString(g);
    return out;
}

} // namespace

// What's on an item (its own group's ingredients).
QStringList PosService::onItOf(const MenuItem &m) const
{
    QStringList out;
    if (const ModifierGroup *g = s_->settings.modifierGroup("on-" + m.id))
        for (const ModifierOption &o : g->options)
            out << qs(o.name);
    return out;
}

// Its sizes with their whole prices ([{name, price: "4.50"}]); none: [].
QVariantList PosService::sizesOf(const MenuItem &m) const
{
    QVariantList out;
    if (const ModifierGroup *g = s_->settings.modifierGroup("size-" + m.id))
        for (const ModifierOption &o : g->options)
            out.append(QVariantMap{{u"name"_s, qs(o.name)},
                                   {u"price"_s, QString::number(double(m.price.cents() + o.price.cents()) / 100.0, 'f', 2)},
                                   {u"priceText"_s, format(m.price + o.price)}});
    return out;
}

QVariantList PosService::choiceGroupsNow() const
{
    QVariantList out;
    for (const ModifierGroup &g : s_->settings.modifierGroups) {
        QVariantList options;
        for (const ModifierOption &o : g.options)
            options.append(QVariantMap{{u"name"_s, qs(o.name)}, {u"price"_s, double(o.price.cents()) / 100.0},
                                       {u"included"_s, o.included}, {u"kitchenName"_s, qs(o.kitchenName)},
                                       {u"kitchenHide"_s, o.kitchenHide},
                                       {u"sizePrices"_s, [&o] {
                                            QVariantMap m;
                                            for (const auto &[size, price] : o.sizePrices)
                                                m.insert(qs(size), QString::number(double(price.cents()) / 100.0, 'f', 2));
                                            return m;
                                        }()}});
        QStringList usedBy;
        for (const MenuItem &m : s_->menu)
            if (std::ranges::find(m.modifierGroups, g.id) != m.modifierGroups.end())
                usedBy << qs(m.name);
        const QString rule = g.min == 1 && g.max == 1 ? tr("Pick 1")
                             : g.min == 0 && g.max == 1 ? tr("Optional, pick 1")
                             : g.max == 0 ? (g.min > 0 ? tr("At least %1").arg(g.min) : tr("Any"))
                             : g.min > 0 ? tr("%1 to %2").arg(g.min).arg(g.max) : tr("Up to %1").arg(g.max);
        out.append(QVariantMap{{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"min"_s, g.min}, {u"max"_s, g.max},
                               {u"rule"_s, rule}, {u"askHow"_s, g.askHow}, {u"menuItems"_s, g.menuItems},
                               {u"own"_s, g.id.starts_with("on-") || g.id.starts_with("size-")}, {u"options"_s, options}, {u"usedBy"_s, usedBy}});
    }
    return out;
}

std::vector<MenuCategory> PosShared::categories() const
{
    std::vector<MenuCategory> out = settings.menuCategories;
    for (const MenuItem &m : menu) {
        if (m.isModifier || m.family.empty() || std::ranges::any_of(out, [&](const MenuCategory &c) { return c.id == m.family; }))
            continue;
        MenuCategory c;
        c.id = m.family;
        c.name = m.family;
        c.name[0] = char(std::toupper(static_cast<unsigned char>(c.name[0])));
        out.push_back(std::move(c));
    }
    return out;
}

QVariantList PosService::menuCategoriesNow() const
{
    QVariantList out;
    const std::string period = currentMealPeriod();
    for (const MenuCategory &c : s_->categories()) {
        const auto count = std::ranges::count_if(s_->menu, [&](const MenuItem &m) { return m.family == c.id && !m.isModifier; });
        QStringList periods;
        for (const std::string &p : c.periods)
            periods << qs(p);
        out.append(QVariantMap{{u"id"_s, qs(c.id)}, {u"name"_s, qs(c.name)}, {u"color"_s, qs(c.color)},
                               {u"periods"_s, periods}, {u"count"_s, qint64(count)},
                               {u"now"_s, c.periods.empty() || std::ranges::find(c.periods, period) != c.periods.end()},
                               {u"printer"_s, qs(c.printer)}, {u"station"_s, qs(c.station)},
                               {u"taxClass"_s, qs(c.taxClass)}, {u"buttonSize"_s, qs(c.buttonSize)},
                               {u"photos"_s, c.photos}, {u"hidePrice"_s, c.hidePrice}, {u"shades"_s, c.shades},
                               {u"textSize"_s, c.textSize}});
    }
    return out;
}

QVariantList PosService::menuItemsNow() const
{
    QVariantList out;
    if (!user())
        return out;
    const std::string period = currentMealPeriod();
    // A category's name as the store wrote it ("Breakfast Plates"), not its id.
    const auto familyName = [&](const std::string &id) {
        for (const core::MenuCategory &c : s_->settings.menuCategories)
            if (c.id == id && !c.name.empty())
                return qs(c.name);
        return qs(id);
    };
    for (const MenuItem &m : s_->menu) {
        out.append(QVariantMap{{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"family"_s, qs(m.family)},
                               {u"familyName"_s, familyName(m.family)},
                               {u"price"_s, format(m.priceDuring(period))}, {u"modifier"_s, m.isModifier},
                               {u"regularPrice"_s, format(m.price)},
                               {u"available"_s, m.available && ticketsLeft(m) != 0}, {u"image"_s, qs(m.image)},
                               {u"color"_s, qs(m.kitchenColor)}, {u"byWeight"_s, m.byWeight},
                               {u"unit"_s, qs(m.weightUnit)}, {u"number"_s, qs(m.number)},
                               {u"barcodes"_s, [&m] { QStringList b; for (const std::string &x : m.barcodes) b << qs(x); return b.join(u", "_s); }()},
                               {u"buttonColor"_s, qs(m.buttonColor)}, {u"textSize"_s, m.textSize},
                               // The Menu Builder's card.
                               {u"priceValue"_s, double(m.price.cents()) / 100.0}, {u"groups"_s, groupsOf(m)},
                               {u"onIt"_s, onItOf(m)}, {u"sizes"_s, sizesOf(m)}, {u"taxClass"_s, qs(toString(m.taxClass))},
                               {u"printer"_s, qs(m.printer)}, {u"station"_s, qs(m.station)},
                               {u"description"_s, qs(m.description)}, {u"kioskHide"_s, m.kioskHide},
                               {u"availableSet"_s, m.available}, {u"favorite"_s, m.favorite},
                               {u"kitchenName"_s, qs(m.kitchenName)}, {u"prepMinutes"_s, m.prepMinutes},
                               {u"section"_s, qs(m.section)}, {u"breakBefore"_s, qs(m.breakBefore)},
                               {u"takeoutPrice"_s, m.takeoutPrice.cents() ? QString::number(double(m.takeoutPrice.cents()) / 100.0, 'f', 2) : QString()},
                               {u"deliveryPrice"_s, m.deliveryPrice.cents() ? QString::number(double(m.deliveryPrice.cents()) / 100.0, 'f', 2) : QString()},
                               {u"periodPrices"_s, [&m] { QVariantMap p; for (const auto &[k, v] : m.periodPrices) p.insert(qs(k), QString::number(double(v.cents()) / 100.0, 'f', 2)); return p; }()},
                               {u"allergens"_s, [&m] { QStringList a; for (const std::string &x : m.allergens) a << qs(x); return a; }()}});
    }
    return out;
}

} // namespace vt::app
