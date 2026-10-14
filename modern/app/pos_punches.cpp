// PosService: Manager -> Time Punches. The last month's punches, newest first:
// a manager fixes a time, a break or the job, adds a punch someone missed, or
// removes one made by mistake. Every change needs a reason and is kept
// (PosSettings::punchChanges), for the Labor report.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kMinute = 60'000;

QVariantMap field(const QString &path, const QString &label, const QString &type, const QString &hint = {})
{
    return {{u"path"_s, path}, {u"label"_s, label}, {u"type"_s, type}, {u"hint"_s, hint}};
}

QString stamp(std::int64_t ms)
{
    return ms ? dateTimeField(ms) : QString();
}

std::int64_t parseStamp(const QString &text)
{
    const std::int64_t ms = dateTimeMs(text);
    return ms > 0 ? ms : -1;
}

QString timeOfDay(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

QString hm(std::int64_t ms)
{
    return QLocale(QLocale::English).toString(QDateTime::fromMSecsSinceEpoch(ms).time(), u"h:mm AP"_s);
}

// Breaks as "12:30-12:45", one per line (on the day the shift started; a time
// earlier than the clock-in is the next day).
QString breaksText(const TimePunch &p)
{
    QStringList lines;
    for (const TimePunch::Break &b : p.breaks)
        lines << hm(b.start) + u'-' + (b.end ? hm(b.end) : QString());
    return lines.join(u'\n');
}

} // namespace

std::vector<TimePunch *> PosService::punchList()
{
    std::vector<TimePunch *> out;
    std::set<std::int64_t> ids;
    for (TimePunch &p : s_->punches)
        if (ids.insert(p.id).second)
            out.push_back(&p);
    for (TimePunch &p : s_->earlierPunches)
        if (ids.insert(p.id).second)
            out.push_back(&p);
    // Older ones (a month back), read from the store when this list is first wanted.
    if (s_->punchHistory && s_->olderPunches.empty()) {
        const std::int64_t t = now();
        s_->olderPunches = s_->punchHistory(t - 31LL * 24 * 3'600'000, t + 3'600'000);
    }
    for (TimePunch &p : s_->olderPunches)
        if (ids.insert(p.id).second && p.clockIn >= now() - 31LL * 24 * 3'600'000)
            out.push_back(&p);
    std::ranges::sort(out, [](const TimePunch *a, const TimePunch *b) { return a->clockIn > b->clockIn; });
    return out;
}

QVariantList PosService::punchFields()
{
    QVariantList people;
    for (const Employee &e : s_->employees)
        if (e.active)
            people.append(QVariantMap{{u"value"_s, qs(e.id)}, {u"text"_s, qs(e.name)}});
    QVariantList jobs{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("Their role")}}};
    for (const char *r : {"server", "bartender", "cashier", "host", "busser", "driver", "manager"})
        jobs.append(QVariantMap{{u"value"_s, QString::fromLatin1(r)}, {u"text"_s, roleName(QString::fromLatin1(r))}});
    QVariantMap who = field(u"employeeId"_s, tr("Who"), u"enum"_s);
    who.insert(u"options"_s, people);
    who.insert(u"readonlyExisting"_s, true);
    QVariantMap job = field(u"job"_s, tr("Job"), u"enum"_s, tr("The pay is that job's (Employees)."));
    job.insert(u"options"_s, jobs);
    return {
        who,
        job,
        field(u"clockIn"_s, tr("Clocked in"), u"string"_s, tr("Like 2026-10-06 9:00 AM")),
        field(u"clockOut"_s, tr("Clocked out"), u"string"_s, tr("Like 2026-10-06 5:30 PM. Empty: still clocked in.")),
        field(u"breaks"_s, tr("Breaks"), u"text"_s, tr("One per line, like 12:30 PM-1:00 PM.")),
        field(u"reason"_s, tr("Reason for the change"), u"string"_s,
              tr("Required: \"forgot to clock out\", \"came in early\"… It goes on the Labor report.")),
        field(u"remove"_s, tr("Remove this punch (made by mistake)"), u"bool"_s),
    };
}

QVariantList PosService::punchRecords()
{
    QVariantList out;
    std::set<std::int64_t> changed;
    for (const PosSettings::PunchChange &c : s_->settings.punchChanges)
        changed.insert(c.punchId);
    for (const TimePunch *p : punchList()) {
        const Employee *e = s_->employee(p->employeeId);
        const double hours = double(p->workedMs(now(), s_->settings.paidBreaks)) / 3'600'000.0;
        const QString day = i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(p->clockIn).date(), u"ddd MMM d"_s);
        const QString span = p->open() ? tr("%1 - still clocked in").arg(timeOfDay(p->clockIn))
                                       : tr("%1 - %2  ·  %3 h").arg(timeOfDay(p->clockIn), timeOfDay(p->clockOut))
                                             .arg(i18n::locale().toString(hours, 'f', 2));
        out.append(QVariantMap{
            {u"id"_s, qint64(p->id)}, {u"employeeId"_s, qs(p->employeeId)}, {u"job"_s, qs(p->job)},
            {u"clockIn"_s, stamp(p->clockIn)}, {u"clockOut"_s, stamp(p->clockOut)}, {u"breaks"_s, breaksText(*p)},
            {u"reason"_s, QString()}, {u"remove"_s, false},
            {u"_title"_s, (e ? qs(e->name) : qs(p->employeeId)) + u"  ·  "_s + day},
            {u"_detail"_s, span + (changed.contains(p->id) ? tr("  ·  changed") : QString())},
        });
    }
    return out;
}

QVariantMap PosService::punchNewRecord()
{
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(now());
    return {{u"id"_s, 0}, {u"employeeId"_s, QString()}, {u"job"_s, QString()},
            {u"clockIn"_s, t.toString(u"yyyy-MM-dd 09:00"_s)}, {u"clockOut"_s, t.toString(u"yyyy-MM-dd 17:00"_s)},
            {u"breaks"_s, QString()}, {u"reason"_s, QString()}, {u"remove"_s, false}};
}

void PosService::logPunchChange(const TimePunch &p, const QString &what, const QString &why)
{
    const Employee *e = s_->employee(p.employeeId);
    s_->settings.punchChanges.push_back({now(), p.id, user() ? user()->name : std::string(),
                                         e ? e->name : p.employeeId, ss(what), ss(why)});
    if (s_->settings.punchChanges.size() > 500)
        s_->settings.punchChanges.erase(s_->settings.punchChanges.begin());
    s_->saveSettings();
}

bool PosService::savePunchRecord(int index, const QVariantMap &r)
{
    std::vector<TimePunch *> list = punchList();
    TimePunch *existing = index >= 0 && index < int(list.size()) ? list[index] : nullptr;
    const QString why = r.value(u"reason"_s).toString().simplified();
    if (why.isEmpty())
        return fail(tr("Say why (Reason for the change): it goes on the Labor report."));
    if (r.value(u"remove"_s).toBool())
        return existing ? deletePunchRecord(index, why) : fail(tr("There is nothing to remove yet."));

    const Employee *e = s_->employee(existing ? existing->employeeId : ss(r.value(u"employeeId"_s).toString()));
    if (!e)
        return fail(tr("Choose who worked."));
    const std::int64_t in = parseStamp(r.value(u"clockIn"_s).toString());
    if (in < 0)
        return fail(tr("Write the clock-in time like 2026-10-06 9:00 AM."));
    const QString outText = r.value(u"clockOut"_s).toString().simplified();
    const std::int64_t out = outText.isEmpty() ? 0 : parseStamp(outText);
    if (out < 0)
        return fail(tr("Write the clock-out time like 2026-10-06 5:30 PM, or leave it empty."));
    if (in > now() || out > now())
        return fail(tr("A punch can't be in the future."));
    if (out && out <= in)
        return fail(tr("Clocked out must be after clocked in."));
    if (out && out - in > 24 * 60 * kMinute)
        return fail(tr("That's more than 24 hours: check the dates."));
    if (!out && in < now() - 24 * 60 * kMinute)
        return fail(tr("Still clocked in for more than a day? Fill in when they clocked out."));

    // Breaks, inside the shift.
    std::vector<TimePunch::Break> breaks;
    const QDate day = QDateTime::fromMSecsSinceEpoch(in).date();
    for (const QString &line : r.value(u"breaks"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
        const QStringList parts = line.simplified().remove(u' ').split(u'-');
        const auto at = [&](const QString &t) -> std::int64_t {
            const int minutes = clockMinutes(t);
            if (minutes < 0)
                return -1;
            const QTime time(minutes / 60, minutes % 60);
            std::int64_t ms = QDateTime(day, time).toMSecsSinceEpoch();
            if (ms < in)
                ms = QDateTime(day.addDays(1), time).toMSecsSinceEpoch();
            return ms;
        };
        const std::int64_t start = parts.size() == 2 ? at(parts[0]) : -1;
        const std::int64_t end = parts.size() == 2 ? (parts[1].isEmpty() ? 0 : at(parts[1])) : -1;
        if (start < 0 || end < 0 || (end && end <= start) || (out && (start > out || end > out)))
            return fail(tr("Write breaks like 12:30 PM-1:00 PM, inside the shift: %1").arg(line.trimmed()));
        if (end == 0 && out)
            return fail(tr("A break with no end needs the punch still open: %1").arg(line.trimmed()));
        breaks.push_back({start, end});
    }
    // No overlap with their other punches; one open punch at most.
    for (const TimePunch *other : list) {
        if (other == existing || other->employeeId != e->id)
            continue;
        const std::int64_t otherOut = other->clockOut ? other->clockOut : now();
        const std::int64_t mineOut = out ? out : now();
        if (in < otherOut && other->clockIn < mineOut)
            return fail(tr("%1 already has a punch then (%2).").arg(qs(e->name), timeOfDay(other->clockIn)));
    }

    std::string job = ss(r.value(u"job"_s).toString());
    if (job.empty())
        job = existing && !existing->job.empty() ? existing->job : e->role;
    Money rate = existing && existing->job == job ? existing->rate : Money();
    if (!(existing && existing->job == job)) {
        rate = e->payRate;
        for (const Job &j : e->jobs())
            if (j.role == job)
                rate = j.rate;
    }

    if (existing) {
        QStringList what;
        if (existing->clockIn != in)
            what << tr("in %1 → %2").arg(stamp(existing->clockIn), stamp(in));
        if (existing->clockOut != out)
            what << tr("out %1 → %2").arg(existing->clockOut ? stamp(existing->clockOut) : tr("(open)"),
                                          out ? stamp(out) : tr("(open)"));
        if (breaksText(*existing) != r.value(u"breaks"_s).toString().trimmed())
            what << tr("breaks changed");
        if (existing->job != job)
            what << tr("job %1 → %2").arg(roleName(qs(existing->job)), roleName(qs(job)));
        if (what.isEmpty())
            return true;
        existing->clockIn = in;
        existing->clockOut = out;
        existing->breaks = breaks;
        existing->job = job;
        existing->rate = rate;
        if (s_->sink)
            s_->sink->savePunch(*existing);
        logPunchChange(*existing, what.join(u", "_s), why);
    } else {
        TimePunch p{++s_->lastPunchId, e->id, in, out, breaks, job, rate};
        // Today's (or still open) with today's; earlier ones with the week's.
        const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
        if (!out || QDateTime::fromMSecsSinceEpoch(in).date() == today)
            s_->punches.push_back(p);
        else
            s_->earlierPunches.push_back(p);
        if (s_->sink)
            s_->sink->savePunch(p);
        logPunchChange(p, tr("added %1 - %2").arg(stamp(in), out ? stamp(out) : tr("(open)")), why);
    }
    emit sessionChanged();
    emit s_->dayChanged();
    emit s_->staffChanged();
    emit notice(tr("%1's punch saved").arg(qs(e->name)));
    return true;
}

bool PosService::deletePunchRecord(int index, const QString &why)
{
    if (why.isEmpty())
        return fail(tr("To remove a punch: tick \"Remove this punch\", give the reason, and Save."));
    std::vector<TimePunch *> list = punchList();
    if (index < 0 || index >= int(list.size()))
        return fail(tr("Choose a punch."));
    const TimePunch p = *list[index];
    logPunchChange(p, tr("removed %1 - %2").arg(stamp(p.clockIn), p.clockOut ? stamp(p.clockOut) : tr("(open)")), why);
    std::erase_if(s_->punches, [&](const TimePunch &x) { return x.id == p.id; });
    std::erase_if(s_->earlierPunches, [&](const TimePunch &x) { return x.id == p.id; });
    std::erase_if(s_->olderPunches, [&](const TimePunch &x) { return x.id == p.id; });
    if (s_->sink)
        s_->sink->deletePunch(p.id);
    emit sessionChanged();
    emit s_->dayChanged();
    emit s_->staffChanged();
    emit notice(tr("Punch removed"));
    return true;
}

} // namespace vt::app
