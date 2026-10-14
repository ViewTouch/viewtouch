// PosService: time off and shift swaps, asked for on the Time Clock and
// decided by a manager (Schedule -> Requests). Giving a shift away puts it
// "up for grabs" on everyone's Time Clock; when someone takes it, a manager
// approves and the shift is theirs.

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

QString hourText(std::int64_t ms)
{
    const QTime t = QDateTime::fromMSecsSinceEpoch(ms).time();
    return t.minute() ? i18n::locale().toString(t, u"h:mm AP"_s) : i18n::locale().toString(t, u"h AP"_s);
}

QString dayText(std::int64_t ms)
{
    return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms).date(), u"ddd MMM d"_s);
}

} // namespace

PosSettings::StaffRequest *PosService::staffRequest(std::int64_t id)
{
    for (PosSettings::StaffRequest &r : s_->settings.staffRequests)
        if (r.id == id)
            return &r;
    return nullptr;
}

const Shift *PosService::shiftById(std::int64_t id) const
{
    for (const Shift &s : s_->shifts)
        if (s.id == id)
            return &s;
    return nullptr;
}

QString PosService::requestText(const PosSettings::StaffRequest &r) const
{
    if (r.kind == "timeOff")
        return tr("Time off %1").arg(dayText(r.day)) + (r.note.empty() ? QString() : u" (%1)"_s.arg(qs(r.note)));
    const Shift *s = shiftById(r.shiftId);
    const QString shift = s ? tr("%1 %2 - %3").arg(dayText(s->start), hourText(s->start), hourText(s->end)) : tr("a shift");
    const Employee *taker = s_->employee(r.takerId);
    return taker ? tr("Give away %1 to %2").arg(shift, qs(taker->name)) : tr("Give away %1").arg(shift);
}

QString PosService::requestStatusText(const PosSettings::StaffRequest &r) const
{
    if (r.status == "approved")
        return tr("approved by %1").arg(qs(r.decidedBy));
    if (r.status == "denied")
        return tr("not approved (%1)").arg(qs(r.decidedBy));
    if (r.status == "cancelled")
        return tr("cancelled");
    if (r.kind == "swap" && r.takerId.empty())
        return tr("up for grabs");
    return tr("waiting for a manager");
}

void PosService::addStaffRequest(PosSettings::StaffRequest r)
{
    auto &list = s_->settings.staffRequests;
    std::int64_t id = 1;
    for (const PosSettings::StaffRequest &x : list)
        id = std::max(id, x.id + 1);
    r.id = id;
    r.at = now();
    list.push_back(std::move(r));
    // Kept: everything still waiting, and the last 300.
    while (list.size() > 300) {
        const auto old = std::ranges::find_if(list, [](const auto &x) { return x.status != "pending"; });
        if (old == list.end())
            break;
        list.erase(old);
    }
    s_->saveSettings();
    emit sessionChanged();
    emit s_->dayChanged();
}

bool PosService::timeClockRequestOff(const QString &date, const QString &reason)
{
    const Employee *e = s_->employee(clockWho_);
    if (!e)
        return fail(tr("Type your PIN first."));
    const QDate day = QDate::fromString(date, u"yyyy-MM-dd"_s);
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    if (!day.isValid() || day < today || day > today.addDays(90))
        return fail(tr("Pick a day from today to three months out."));
    const std::int64_t midnight = QDateTime(day, QTime(0, 0)).toMSecsSinceEpoch();
    for (const PosSettings::StaffRequest &r : s_->settings.staffRequests)
        if (r.kind == "timeOff" && r.employeeId == e->id && r.day == midnight
            && (r.status == "pending" || r.status == "approved"))
            return fail(tr("You already asked for %1 off.").arg(dayText(midnight)));
    PosSettings::StaffRequest r;
    r.kind = "timeOff";
    r.employeeId = e->id;
    r.day = midnight;
    r.note = ss(reason.left(60));
    addStaffRequest(r);
    emit notice(tr("Asked for %1 off: a manager will decide.").arg(dayText(midnight)));
    return true;
}

bool PosService::timeClockGiveAway(qint64 shiftId)
{
    const Employee *e = s_->employee(clockWho_);
    if (!e)
        return fail(tr("Type your PIN first."));
    const Shift *s = shiftById(shiftId);
    if (!s || s->employeeId != e->id)
        return fail(tr("That isn't your shift."));
    if (s->start <= now())
        return fail(tr("That shift has already started."));
    for (const PosSettings::StaffRequest &r : s_->settings.staffRequests)
        if (r.kind == "swap" && r.shiftId == shiftId && r.status == "pending")
            return fail(tr("That shift is already up for grabs."));
    PosSettings::StaffRequest r;
    r.kind = "swap";
    r.employeeId = e->id;
    r.shiftId = shiftId;
    addStaffRequest(r);
    emit notice(tr("Your %1 shift is up for grabs.").arg(dayText(s->start)));
    return true;
}

bool PosService::timeClockTake(qint64 requestId)
{
    const Employee *e = s_->employee(clockWho_);
    if (!e)
        return fail(tr("Type your PIN first."));
    PosSettings::StaffRequest *r = staffRequest(requestId);
    if (!r || r->kind != "swap" || r->status != "pending" || !r->takerId.empty())
        return fail(tr("Someone has already taken it."));
    if (r->employeeId == e->id)
        return fail(tr("That's your own shift."));
    const Shift *s = shiftById(r->shiftId);
    if (!s || s->start <= now())
        return fail(tr("That shift has already started."));
    for (const Shift &mine : s_->shifts)
        if (mine.employeeId == e->id && mine.start < s->end && s->start < mine.end)
            return fail(tr("You already work then."));
    r->takerId = e->id;
    s_->saveSettings();
    emit sessionChanged();
    emit s_->dayChanged();
    emit notice(tr("You'll take it once a manager approves."));
    return true;
}

bool PosService::timeClockCancelRequest(qint64 requestId)
{
    const Employee *e = s_->employee(clockWho_);
    PosSettings::StaffRequest *r = staffRequest(requestId);
    if (!e || !r || r->status != "pending")
        return fail(tr("That request is already decided."));
    if (r->employeeId == e->id)
        r->status = "cancelled";
    else if (r->takerId == e->id)
        r->takerId.clear();   // back up for grabs
    else
        return fail(tr("That isn't your request."));
    s_->saveSettings();
    emit sessionChanged();
    emit s_->dayChanged();
    return true;
}

QVariantMap PosService::requestsFor(const std::string &employeeId) const
{
    const std::int64_t t = now();
    QVariantList mine, grabs;
    for (auto it = s_->settings.staffRequests.rbegin(); it != s_->settings.staffRequests.rend(); ++it) {
        const PosSettings::StaffRequest &r = *it;
        const Shift *s = r.kind == "swap" ? shiftById(r.shiftId) : nullptr;
        if ((r.employeeId == employeeId || r.takerId == employeeId) && mine.size() < 8
            && (r.status == "pending" || r.decidedAt > t - 14LL * 24 * 3'600'000))
            mine.append(QVariantMap{{u"id"_s, qint64(r.id)}, {u"text"_s, requestText(r)}, {u"status"_s, qs(r.status)},
                                    {u"statusText"_s, requestStatusText(r)}, {u"taking"_s, r.takerId == employeeId},
                                    {u"canCancel"_s, r.status == "pending"}});
        if (r.kind == "swap" && r.status == "pending" && r.takerId.empty() && r.employeeId != employeeId && s
            && s->start > t) {
            const Employee *who = s_->employee(r.employeeId);
            grabs.append(QVariantMap{{u"id"_s, qint64(r.id)}, {u"who"_s, who ? qs(who->name) : QString()},
                                     {u"day"_s, dayText(s->start)},
                                     {u"hours"_s, tr("%1 - %2").arg(hourText(s->start), hourText(s->end))},
                                     {u"note"_s, qs(s->note)}});
        }
    }
    return {{u"mine"_s, mine}, {u"upForGrabs"_s, grabs}};
}

// --- Schedule -> Requests (the admin form) ---------------------------------------

std::vector<PosSettings::StaffRequest *> PosService::requestList()
{
    std::vector<PosSettings::StaffRequest *> out;
    const std::int64_t t = now();
    for (PosSettings::StaffRequest &r : s_->settings.staffRequests)
        if (r.status == "pending" || (r.status != "cancelled" && r.decidedAt > t - 30LL * 24 * 3'600'000))
            out.push_back(&r);
    // Waiting first, then the newest.
    std::ranges::stable_sort(out, [](const auto *a, const auto *b) {
        if ((a->status == "pending") != (b->status == "pending"))
            return a->status == "pending";
        return a->at > b->at;
    });
    return out;
}

QVariantList PosService::requestFields()
{
    QVariantMap summary{{u"path"_s, u"summary"_s}, {u"label"_s, tr("Request")}, {u"type"_s, u"text"_s},
                        {u"readonlyExisting"_s, true}, {u"hint"_s, QString()}};
    QVariantMap decision{{u"path"_s, u"status"_s}, {u"label"_s, tr("Decision")}, {u"type"_s, u"enum"_s},
                         {u"hint"_s, tr("Approving a swap gives the shift to whoever took it.")}};
    decision.insert(u"options"_s, QVariantList{
        QVariantMap{{u"value"_s, u"pending"_s}, {u"text"_s, tr("Waiting")}},
        QVariantMap{{u"value"_s, u"approved"_s}, {u"text"_s, tr("Approve")}},
        QVariantMap{{u"value"_s, u"denied"_s}, {u"text"_s, tr("Don't approve")}}});
    return {summary, decision};
}

QVariantList PosService::requestRecords()
{
    QVariantList out;
    for (const PosSettings::StaffRequest *r : requestList()) {
        const Employee *e = s_->employee(r->employeeId);
        QString summary = requestText(*r) + u"\n"_s + tr("Asked %1 %2").arg(dayText(r->at), hourText(r->at));
        // Time off on a day they're scheduled: say so.
        if (r->kind == "timeOff")
            for (const Shift &s : s_->shifts)
                if (s.employeeId == r->employeeId && s.start < r->day + 24LL * 3'600'000 && s.end > r->day)
                    summary += u"\n"_s + tr("They're on the schedule that day (%1 - %2): change it in the Schedule.")
                                             .arg(hourText(s.start), hourText(s.end));
        out.append(QVariantMap{{u"id"_s, qint64(r->id)}, {u"summary"_s, summary}, {u"status"_s, qs(r->status)},
                               {u"_title"_s, (e ? qs(e->name) : qs(r->employeeId)) + u"  ·  "_s + requestText(*r)},
                               {u"_detail"_s, requestStatusText(*r)}});
    }
    return out;
}

bool PosService::saveRequestRecord(int index, const QVariantMap &record)
{
    std::vector<PosSettings::StaffRequest *> list = requestList();
    if (index < 0 || index >= int(list.size()))
        return fail(tr("Choose a request."));
    PosSettings::StaffRequest &r = *list[index];
    const std::string status = ss(record.value(u"status"_s).toString());
    if (status == r.status)
        return true;
    if (r.status != "pending")
        return fail(tr("That request is already decided."));
    if (status != "approved" && status != "denied")
        return fail(tr("Approve it, or don't."));
    if (status == "approved" && r.kind == "swap") {
        if (r.takerId.empty())
            return fail(tr("Nobody has taken the shift yet."));
        Shift *s = nullptr;
        for (Shift &x : s_->shifts)
            if (x.id == r.shiftId)
                s = &x;
        if (!s)
            return fail(tr("That shift is no longer on the schedule."));
        s->employeeId = r.takerId;
        if (s_->sink)
            s_->sink->saveShift(*s);
        emit s_->staffChanged();
    }
    r.status = status;
    r.decidedBy = user() ? user()->name : std::string();
    r.decidedAt = now();
    s_->saveSettings();
    emit sessionChanged();
    emit s_->dayChanged();
    emit notice(status == "approved" ? tr("Approved: %1").arg(requestText(r)) : tr("Not approved: %1").arg(requestText(r)));
    return true;
}

int PosService::requestsWaiting() const
{
    return int(std::ranges::count_if(s_->settings.staffRequests, [](const auto &r) {
        return r.status == "pending" && !(r.kind == "swap" && r.takerId.empty());
    }));
}

} // namespace vt::app
