// PosService: Manager -> Dashboard. Today so far at a glance: sales against
// the same day last week (by this time), labor cost, open checks, kitchen
// times, who's on the clock, the best sellers and what's running low.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <map>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kHour = 3'600'000;

QString clock(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

} // namespace

QVariantMap PosService::dashboardNow() const
{
    if (!user() || !user()->can(perm::Manager))
        return {};
    const std::int64_t t = now();
    const PosSettings &st = s_->settings;

    // Sales: today's closed checks (not practice).
    Money net;
    int checks = 0, guests = 0;
    for (const Check &c : s_->closedToday) {
        if (c.training)
            continue;
        net += c.totals(st.tax).netSales();
        ++checks;
        guests += c.guests;
    }
    // The same weekday last week, up to this time of day.
    const QDateTime nowDt = QDateTime::fromMSecsSinceEpoch(t);
    const QDateTime weekAgo = nowDt.addDays(-7);
    if (s_->history && (lastWeekChecks_ < 0 || t - lastWeekAt_ > 5 * 60'000)) {
        lastWeekNet_ = Money();
        lastWeekChecks_ = 0;
        for (const Check &c : s_->history(QDateTime(weekAgo.date().startOfDay()).toMSecsSinceEpoch(),
                                          weekAgo.toMSecsSinceEpoch())) {
            if (c.training)
                continue;
            lastWeekNet_ += c.totals(st.tax).netSales();
            ++lastWeekChecks_;
        }
        lastWeekAt_ = t;
    }
    QVariantMap sales{{u"net"_s, format(net)}, {u"checks"_s, checks}, {u"guests"_s, guests},
                      {u"average"_s, checks ? format(Money::fromCents(net.cents() / checks)) : format(Money())},
                      {u"lastWeekDay"_s, i18n::locale().toString(weekAgo.date(), u"ddd MMM d"_s)}};
    if (lastWeekChecks_ >= 0 && s_->history) {
        sales.insert(u"lastWeek"_s, format(lastWeekNet_));
        if (lastWeekNet_.cents() > 0)
            sales.insert(u"change"_s, int(std::lround(100.0 * double(net.cents() - lastWeekNet_.cents())
                                                      / double(lastWeekNet_.cents()))));
    }

    // Labor: today's shifts at the pay they were clocked in at; who's on now.
    double laborCents = 0;
    QVariantList onClock;
    for (const TimePunch &p : s_->punches) {
        laborCents += double(p.workedMs(t, st.paidBreaks)) / double(kHour) * double(p.rate.cents());
        if (p.open()) {
            const Employee *e = s_->employee(p.employeeId);
            const QVariantMap ot = overtimeFor(p.employeeId);
            onClock.append(QVariantMap{{u"name"_s, e ? qs(e->name) : qs(p.employeeId)},
                                       {u"job"_s, roleName(qs(p.job))}, {u"since"_s, clock(p.clockIn)},
                                       {u"onBreak"_s, p.onBreak()}, {u"overTwelve"_s, t - p.clockIn > 12 * kHour},
                                       {u"overtime"_s, ot.value(u"state"_s)}, {u"overtimeLeft"_s, ot.value(u"left"_s)}});
        }
    }
    const Money labor = Money::fromCents(std::llround(laborCents));
    QVariantMap laborInfo{{u"cost"_s, format(labor)}, {u"onClock"_s, onClock}};
    if (net.cents() > 0)
        laborInfo.insert(u"percent"_s, int(std::lround(100.0 * double(labor.cents()) / double(net.cents()))));

    // Open checks and what's still due on them.
    Money due;
    int open = 0;
    for (const auto &[id, c] : s_->open) {
        if (c.training)
            continue;
        ++open;
        due += c.totals(st.tax).balance;
    }

    // Kitchen: how long items took today (sent to made), and what's waiting.
    std::int64_t madeMs = 0;
    int made = 0, waiting = 0;
    std::int64_t oldestWait = 0;
    const auto kitchen = [&](const Check &c) {
        for (const OrderLine &l : c.lines) {
            if (!l.sent || l.voided || !l.forKitchen() || l.sentAt <= 0)
                continue;
            if (l.made && l.madeAt > l.sentAt) {
                madeMs += l.madeAt - l.sentAt;
                ++made;
            } else if (!l.made && !c.held(l)) {
                ++waiting;
                oldestWait = std::max(oldestWait, t - l.sentAt);
            }
        }
    };
    for (const Check &c : s_->closedToday)
        kitchen(c);
    for (const auto &[id, c] : s_->open)
        kitchen(c);
    QVariantMap kitchenInfo{{u"waiting"_s, waiting}, {u"oldestMinutes"_s, int(oldestWait / 60'000)}};
    if (made > 0)
        kitchenInfo.insert(u"averageMinutes"_s, i18n::locale().toString(double(madeMs) / made / 60'000.0, 'f', 1));

    // Best sellers today (how many of each).
    std::map<std::string, int> sold;
    const auto count = [&](const Check &c) {
        if (c.training)
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
        ranked.emplace_back(n, id);
    std::ranges::stable_sort(ranked, [](const auto &a, const auto &b) { return a.first > b.first; });
    QVariantList top;
    for (const auto &[n, id] : ranked) {
        if (top.size() >= 5)
            break;
        const MenuItem *m = findItem(qs(id));
        top.append(QVariantMap{{u"name"_s, m ? qs(m->name) : qs(id)}, {u"count"_s, n}});
    }

    // Running low, and what's sold out.
    QVariantList low, out;
    for (const Ingredient &g : s_->ingredients)
        if (g.low())
            low.append(QVariantMap{{u"name"_s, qs(g.name)},
                                   {u"left"_s, i18n::locale().toString(std::max(0.0, g.onHand), 'g', 4) + u' ' + qs(g.unit)},
                                   {u"out"_s, g.onHand <= 0}});
    for (const MenuItem &m : s_->menu)
        if (!m.available && !m.isModifier)
            out.append(qs(m.name));

    return {{u"sales"_s, sales}, {u"labor"_s, laborInfo},
            {u"open"_s, QVariantMap{{u"count"_s, open}, {u"due"_s, format(due)}}},
            {u"kitchen"_s, kitchenInfo}, {u"top"_s, top}, {u"low"_s, low}, {u"soldOut"_s, out},
            {u"requestsWaiting"_s, requestsWaiting()}, {u"at"_s, clock(t)}};
}

// --- opening and closing checklists ------------------------------------------------

QVariantMap PosService::checklistsNow() const
{
    const PosSettings &st = s_->settings;
    const bool today = st.checklistDayId == s_->day.id;
    QVariantMap out;
    for (const auto &[name, tasks] : {std::pair{u"opening"_s, &st.openingChecklist}, std::pair{u"closing"_s, &st.closingChecklist}}) {
        QVariantList list;
        int done = 0;
        for (const std::string &task : *tasks) {
            const PosSettings::ChecklistTick *tick = nullptr;
            if (today)
                for (const PosSettings::ChecklistTick &t : st.checklistTicks)
                    if (qs(t.list) == name && t.task == task)
                        tick = &t;
            done += tick ? 1 : 0;
            list.append(QVariantMap{{u"task"_s, qs(task)}, {u"done"_s, tick != nullptr},
                                    {u"by"_s, tick ? qs(tick->by) : QString()}, {u"at"_s, tick ? clock(tick->at) : QString()}});
        }
        out.insert(name, list);
        out.insert(name + u"Done"_s, done);
    }
    return out;
}

bool PosService::tickChecklist(const QString &list, int index)
{
    if (!user())
        return fail(tr("Log in first."));
    PosSettings &st = s_->settings;
    const std::vector<std::string> &tasks = list == u"opening" ? st.openingChecklist : st.closingChecklist;
    if ((list != u"opening" && list != u"closing") || index < 0 || index >= int(tasks.size()))
        return fail(tr("There is no such task."));
    if (st.checklistDayId != s_->day.id) {   // a new business day: a clean list
        st.checklistTicks.clear();
        st.checklistDayId = s_->day.id;
    }
    const std::string &task = tasks[index];
    const auto it = std::ranges::find_if(st.checklistTicks, [&](const auto &t) { return qs(t.list) == list && t.task == task; });
    if (it != st.checklistTicks.end())
        st.checklistTicks.erase(it);   // touched again: not done after all
    else
        st.checklistTicks.push_back({ss(list), task, user()->name, now()});
    s_->saveSettings();
    emit s_->dayChanged();
    return true;
}

Report PosService::checklistReport(const ReportContext &ctx) const
{
    Report r;
    r.id = "checklists";
    r.title = "Checklists";
    r.subtitle = ctx.period;
    r.columns = {"Task", "Done by", "At"};
    const QVariantMap lists = checklists();
    for (const auto &[name, title] : {std::pair{u"opening"_s, "Opening"}, std::pair{u"closing"_s, "Closing"}}) {
        const QVariantList tasks = lists.value(name).toList();
        if (tasks.isEmpty())
            continue;
        r.section(title);
        for (const QVariant &v : tasks) {
            const QVariantMap t = v.toMap();
            r.line({ss(t.value(u"task"_s).toString()), t.value(u"done"_s).toBool() ? ss(t.value(u"by"_s).toString()) : "NOT DONE",
                    ss(t.value(u"at"_s).toString())});
        }
    }
    if (r.rows.empty())
        r.note("No checklists (Manager -> Settings: Opening / Closing checklist).");
    return r;
}

// What's the same on every screen (in its language), made once while the
// server updates them all.
QVariantList PosService::kitchenTickets() const
{
    return s_->memo(u"kitchenTickets "_s + language(), [this] { return QVariant(kitchenTicketsNow()); }).toList();
}
QVariantList PosService::expoTickets() const
{
    return s_->memo(u"expoTickets "_s + language(), [this] { return QVariant(expoTicketsNow()); }).toList();
}
QVariantMap PosService::dashboard() const
{
    if (!user() || !user()->can(perm::Manager))
        return {};
    return s_->memo(u"dashboard "_s + language(), [this] { return QVariant(dashboardNow()); }).toMap();
}
QVariantList PosService::menuItems() const
{
    if (!user())
        return {};
    return s_->memo(u"menuItems "_s + language(), [this] { return QVariant(menuItemsNow()); }).toList();
}
QVariantList PosService::menuCategories() const
{
    return s_->memo(u"menuCategories "_s + language(), [this] { return QVariant(menuCategoriesNow()); }).toList();
}
QVariantList PosService::choiceGroups() const
{
    return s_->memo(u"choiceGroups "_s + language(), [this] { return QVariant(choiceGroupsNow()); }).toList();
}

QVariantList PosService::closedChecks() const
{
    if (!can(QString::fromLatin1(perm::Manager)))
        return {};
    return s_->memo(u"closedChecks "_s + language(), [this] { return QVariant(closedChecksNow()); }).toList();
}
QVariantMap PosService::waitlistInfo() const
{
    if (!user())
        return {};
    return s_->memo(u"waitlistInfo "_s + language(), [this] { return QVariant(waitlistInfoNow()); }).toMap();
}
QVariantMap PosService::floor() const
{
    if (!user())
        return {};
    return s_->memo(u"floor "_s + language(), [this] { return QVariant(floorNow()); }).toMap();
}
QVariantMap PosService::stockLeft() const
{
    return s_->memo(u"stockLeft "_s + language(), [this] { return QVariant(stockLeftNow()); }).toMap();
}
QVariantMap PosService::dayInfo() const
{
    return s_->memo(u"dayInfo "_s + language(), [this] { return QVariant(dayInfoNow()); }).toMap();
}
QVariantList PosService::days() const
{
    return s_->memo(u"days "_s + language(), [this] { return QVariant(daysNow()); }).toList();
}
QVariantMap PosService::checklists() const
{
    return s_->memo(u"checklists "_s + language(), [this] { return QVariant(checklistsNow()); }).toMap();
}
QStringList PosService::popularItems() const
{
    return s_->memo(u"popularItems "_s + language(), [this] { return QVariant(popularItemsNow()); }).toStringList();
}

} // namespace vt::app
