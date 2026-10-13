// PosService: the staff schedule - shifts by week, the next shift on the
// logout screen, and (when the store wants it) clocking in only near a
// scheduled shift, with a manager able to clock someone in anyway.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kMinute = 60'000;

QString hourText(std::int64_t ms)
{
    const QTime t = QDateTime::fromMSecsSinceEpoch(ms).time();
    return t.minute() ? i18n::locale().toString(t, u"h:mm AP"_s) : i18n::locale().toString(t, u"h AP"_s);
}

// "2026-10-02 4:00 PM" (or 16:00) or epoch ms.
std::int64_t timeFrom(const QVariant &v)
{
    if (v.typeId() != QMetaType::QString && v.toLongLong() > 0)
        return v.toLongLong();
    const std::int64_t ms = dateTimeMs(v.toString());
    return ms > 0 ? ms : (v.toLongLong() > 0 ? v.toLongLong() : 0);
}

} // namespace

const Shift *PosService::shiftNow(const std::string &employeeId) const
{
    const std::int64_t t = now();
    const std::int64_t early = std::int64_t(s_->settings.clockInEarlyMinutes) * kMinute;
    for (const Shift &s : s_->shifts) {
        if (s.employeeId == employeeId && s.start - early <= t && t < s.end)
            return &s;
    }
    return nullptr;
}

QString PosService::nextShift() const
{
    const Employee *e = user();
    if (!e)
        return {};
    const Shift *next = nullptr;
    for (const Shift &s : s_->shifts) {
        if (s.employeeId == e->id && s.end > now() && (!next || s.start < next->start))
            next = &s;
    }
    if (!next)
        return {};
    const QDate day = QDateTime::fromMSecsSinceEpoch(next->start).date();
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    const QString when = day == today ? tr("today") : day == today.addDays(1) ? tr("tomorrow") : i18n::locale().toString(day, u"ddd MMM d"_s);
    return tr("%1 %2 - %3").arg(when, hourText(next->start), hourText(next->end));
}

// --- the Time Clock screen: clock in / out and the schedule, without logging in ----

QVariantMap PosService::overtimeFor(const std::string &employeeId) const
{
    const PosSettings &st = s_->settings;
    const std::int64_t t = now();
    const std::int64_t weekStart = reportContext(QString()).weekStart;
    const QDate today = QDateTime::fromMSecsSinceEpoch(t).date();
    std::int64_t weekMs = 0, todayMs = 0;
    const auto add = [&](const TimePunch &p) {
        if (p.employeeId != employeeId || p.clockIn < weekStart)
            return;
        const std::int64_t ms = p.workedMs(t, st.paidBreaks);
        weekMs += ms;
        if (QDateTime::fromMSecsSinceEpoch(p.clockIn).date() == today)
            todayMs += ms;
    };
    for (const TimePunch &p : s_->punches)
        add(p);
    for (const TimePunch &p : s_->earlierPunches)
        add(p);
    std::optional<std::int64_t> left;   // minutes until the first overtime rule
    if (st.overtimeWeeklyHours > 0)
        left = (st.overtimeWeeklyHours * 60 * kMinute - weekMs) / kMinute;
    if (st.overtimeDailyHours > 0) {
        const std::int64_t daily = (st.overtimeDailyHours * 60 * kMinute - todayMs) / kMinute;
        left = left ? std::min(*left, daily) : daily;
    }
    QVariantMap out{{u"weekHours"_s, i18n::locale().toString(double(weekMs) / (60.0 * kMinute), 'f', 1)},
                    {u"todayHours"_s, i18n::locale().toString(double(todayMs) / (60.0 * kMinute), 'f', 1)},
                    {u"state"_s, u"ok"_s}};
    if (left) {
        out.insert(u"leftMinutes"_s, qint64(*left));
        out.insert(u"left"_s, *left > 0 ? i18n::locale().toString(double(*left) / 60.0, 'f', 1) : QString());
        out.insert(u"state"_s, *left <= 0 ? u"over"_s : *left <= 120 ? u"soon"_s : u"ok"_s);
    }
    return out;
}

bool PosService::timeClockStart(const QString &pin)
{
    const Employee *e = employeeByPin(pin);
    if (!e || !e->active)
        return fail(tr("That PIN is not recognized."));
    clockWho_ = e->id;
    emit sessionChanged();
    return true;
}

void PosService::timeClockDone()
{
    if (clockWho_.empty())
        return;
    clockWho_.clear();
    if (!jobChoice_.empty())
        jobChoice_.clear();
    emit sessionChanged();
}

bool PosService::timeClockAct(const QString &action)
{
    const Employee *e = s_->employee(clockWho_);
    if (!e)
        return fail(tr("Type your PIN first."));
    bool ok = false;
    if (action == u"in") {
        if (openPunch(e->id))
            return fail(tr("%1 is already clocked in.").arg(qs(e->name)));
        if (const QString why = scheduleCheck(*e); !why.isEmpty())
            return fail(why);
        const std::vector<Job> jobs = e->jobs();
        if (jobs.size() > 1) {   // which job today? (the job chooser)
            jobChoice_ = e->id;
            emit sessionChanged();
            return true;
        }
        ok = punchIn(*e, jobs.front());
    } else if (action == u"out") {
        ok = clockOutFor(*e);
    } else if (action == u"break") {
        ok = toggleBreakFor(*e);
    } else {
        return fail(tr("Unknown operation '%1'").arg(action));
    }
    emit sessionChanged();
    return ok;
}

QVariantMap PosService::timeClock() const
{
    const Employee *e = s_->employee(clockWho_);
    if (!e)
        return {};
    const std::int64_t t = now();
    QString status = u"out"_s, since, job, breakSince;
    std::int64_t workedMs = 0;
    QVariantList punches;
    for (const TimePunch &p : s_->punches) {
        if (p.employeeId != e->id)
            continue;
        if (QDateTime::fromMSecsSinceEpoch(p.clockIn).date() == QDateTime::fromMSecsSinceEpoch(t).date() || p.open()) {
            workedMs += p.workedMs(t, s_->settings.paidBreaks);
            punches.append(QVariantMap{{u"in"_s, hourText(p.clockIn)}, {u"out"_s, p.open() ? QString() : hourText(p.clockOut)},
                                       {u"job"_s, roleName(qs(p.job))}});
        }
        if (p.open()) {
            status = p.onBreak() ? u"break"_s : u"in"_s;
            since = hourText(p.clockIn);
            job = roleName(qs(p.job));
            if (p.onBreak())
                breakSince = hourText(p.breaks.back().start);
        }
    }
    // Shifts from today through the next two weeks.
    const QDate today = QDateTime::fromMSecsSinceEpoch(t).date();
    std::vector<const Shift *> mine;
    for (const Shift &s : s_->shifts)
        if (s.employeeId == e->id && s.end > QDateTime(today.startOfDay()).toMSecsSinceEpoch()
            && QDateTime::fromMSecsSinceEpoch(s.start).date() <= today.addDays(14))
            mine.push_back(&s);
    std::ranges::sort(mine, {}, &Shift::start);
    QVariantList shifts;
    double weekHours = 0;
    for (const Shift *s : mine) {
        const QDate day = QDateTime::fromMSecsSinceEpoch(s->start).date();
        if (day < today.addDays(7))
            weekHours += s->hours();
        bool givingAway = false;
        for (const PosSettings::StaffRequest &r : s_->settings.staffRequests)
            givingAway = givingAway || (r.kind == "swap" && r.shiftId == s->id && r.status == "pending");
        shifts.append(QVariantMap{
            {u"id"_s, qint64(s->id)}, {u"givingAway"_s, givingAway}, {u"future"_s, s->start > t},
            {u"day"_s, day == today ? tr("Today") : day == today.addDays(1) ? tr("Tomorrow")
                                                                            : i18n::locale().toString(day, u"ddd MMM d"_s)},
            {u"hours"_s, tr("%1 - %2").arg(hourText(s->start), hourText(s->end))},
            {u"length"_s, i18n::locale().toString(s->hours(), 'f', 1)},
            {u"note"_s, qs(s->note)},
            {u"now"_s, s->start <= t && t < s->end},
        });
    }
    return {{u"name"_s, qs(e->name)}, {u"status"_s, status}, {u"since"_s, since}, {u"job"_s, job},
            {u"breakSince"_s, breakSince}, {u"todayHours"_s, i18n::locale().toString(double(workedMs) / 3'600'000.0, 'f', 2)},
            {u"punches"_s, punches}, {u"shifts"_s, shifts},
            {u"weekHours"_s, i18n::locale().toString(weekHours, 'f', 1)}, {u"overtime"_s, overtimeFor(e->id)},
            {u"choosingJob"_s, jobChoice_ == e->id}, {u"requests"_s, requestsFor(e->id)}};
}

QString PosService::scheduleCheck(const Employee &e) const
{
    if (!s_->settings.scheduleRequired || e.can(perm::Manager) || shiftNow(e.id))
        return {};
    return tr("%1 isn't on the schedule now. A manager can clock you in from Manager → Schedule.").arg(qs(e.name));
}

bool PosService::addShift(const QVariantMap &r)
{
    if (!require(perm::Manager, tr("The schedule")))
        return false;
    const Employee *e = s_->employee(ss(r.value(u"employeeId"_s).toString()));
    if (!e || !e->active)
        return fail(tr("Choose who works the shift."));
    Shift s;
    s.employeeId = e->id;
    s.start = timeFrom(r.value(u"start"_s));
    s.end = timeFrom(r.value(u"end"_s));
    if (s.start <= 0 || s.end <= 0)
        return fail(tr("When does the shift start and end?"));
    if (s.end <= s.start)
        s.end += 24 * 60 * kMinute;   // past midnight
    if (s.end - s.start > 16 * 60 * kMinute)
        return fail(tr("A shift can be at most 16 hours."));
    for (const Shift &o : s_->shifts) {
        if (o.employeeId == s.employeeId && o.start < s.end && s.start < o.end)
            return fail(tr("%1 already works %2 - %3 then.").arg(qs(e->name), hourText(o.start), hourText(o.end)));
    }
    s.note = ss(r.value(u"note"_s).toString().trimmed());
    s.id = ++s_->lastShiftId;
    s_->shifts.push_back(s);
    std::ranges::sort(s_->shifts, {}, &Shift::start);
    if (s_->sink)
        s_->sink->saveShift(s);
    emit notice(tr("%1: %2 %3 - %4").arg(qs(e->name), i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(s.start), u"ddd"_s),
                                         hourText(s.start), hourText(s.end)));
    emit s_->staffChanged();
    return true;
}

bool PosService::removeShift(qint64 id)
{
    if (!require(perm::Manager, tr("The schedule")))
        return false;
    const auto it = std::ranges::find_if(s_->shifts, [&](const Shift &s) { return s.id == id; });
    if (it == s_->shifts.end())
        return fail(tr("That shift is gone."));
    s_->shifts.erase(it);
    if (s_->sink)
        s_->sink->deleteShift(id);
    emit notice(tr("Shift removed"));
    emit s_->staffChanged();
    return true;
}

bool PosService::clockInEmployee(const QString &employeeId)
{
    if (!require(perm::Manager, tr("Clocking someone in")))
        return false;
    const Employee *e = s_->employee(ss(employeeId));
    if (!e || !e->active)
        return fail(tr("No such employee."));
    if (openPunch(e->id))
        return fail(tr("%1 is already clocked in.").arg(qs(e->name)));
    return punchIn(*e, e->jobs().front(), qs(user()->name));   // their main job
}

bool PosService::setScheduleWeek(int offset)
{
    scheduleWeek_ = std::clamp(offset, -2, 8);
    emit sessionChanged();
    return true;
}

QVariantMap PosService::scheduleInfo() const
{
    if (!user())
        return {};
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    const int back = (today.dayOfWeek() % 7 - s_->settings.weekStartsOn + 7) % 7;
    const QDate first = today.addDays(-back + 7 * scheduleWeek_);
    QVariantList days;
    std::map<std::string, double> hours;
    for (int d = 0; d < 7; ++d) {
        const QDate date = first.addDays(d);
        const std::int64_t from = QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch();
        const std::int64_t to = QDateTime(date.addDays(1), QTime(0, 0)).toMSecsSinceEpoch();
        QVariantList list;
        for (const Shift &s : s_->shifts) {
            if (s.start < from || s.start >= to)
                continue;
            const Employee *e = s_->employee(s.employeeId);
            const bool onClock = std::ranges::any_of(s_->punches, [&](const TimePunch &p) {
                return p.employeeId == s.employeeId && p.open();
            });
            list.append(QVariantMap{
                {u"id"_s, qint64(s.id)}, {u"employeeId"_s, qs(s.employeeId)},
                {u"name"_s, e ? qs(e->name) : qs(s.employeeId)}, {u"role"_s, e ? qs(e->role) : QString()},
                {u"time"_s, hourText(s.start) + u" - "_s + hourText(s.end)}, {u"note"_s, qs(s.note)},
                {u"hours"_s, s.hours()}, {u"now"_s, s.start <= now() && now() < s.end}, {u"onClock"_s, onClock},
            });
            hours[s.employeeId] += s.hours();
        }
        days.append(QVariantMap{{u"date"_s, date.toString(u"yyyy-MM-dd"_s)},
                                {u"label"_s, i18n::locale().toString(date, u"ddd M/d"_s)}, {u"today"_s, date == today},
                                {u"shifts"_s, list}});
    }
    QVariantList totals;
    for (const auto &[id, h] : hours) {
        const Employee *e = s_->employee(id);
        totals.append(QVariantMap{{u"name"_s, e ? qs(e->name) : qs(id)}, {u"hours"_s, h},
                                  {u"over"_s, s_->settings.overtimeWeeklyHours > 0 && h > s_->settings.overtimeWeeklyHours}});
    }
    QVariantList staff;
    for (const Employee &e : s_->employees) {
        if (e.active)
            staff.append(QVariantMap{{u"id"_s, qs(e.id)}, {u"name"_s, qs(e.name)}, {u"role"_s, qs(e.role)}});
    }
    return {
        {u"week"_s, scheduleWeek_}, {u"title"_s, scheduleWeek_ == 0 ? tr("This week") : scheduleWeek_ == 1 ? tr("Next week")
                                                  : tr("Week of %1").arg(i18n::locale().toString(first, u"MMM d"_s))},
        {u"days"_s, days}, {u"totals"_s, totals}, {u"staff"_s, staff},
        {u"required"_s, s_->settings.scheduleRequired},
    };
}

} // namespace vt::app
