// PosService: managing checks - transfer to another server, move to another
// table, merge two checks, reopen a closed one; and each check's history.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <algorithm>
#include <QLocale>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {
QString timeOf(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}
} // namespace

void PosService::noteEvent(Check &c, const QString &what, const char *kind, Money amount, const std::string &reason)
{
    c.note(now(), user() ? user()->name : std::string(), ss(what), kind, amount, reason);
}

// Servers manage their own checks; managers anyone's.
bool PosService::mayManage(const Check &c, const QString &action)
{
    if (!require(perm::Order, action))
        return false;
    if (c.serverId == user()->id || user()->can(perm::Manager))
        return true;
    return fail(tr("%1 is %2's check: ask them or a manager.").arg(qs(c.label), qs(c.serverName)));
}

bool PosService::transferCheck(const QString &employeeId)
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (!mayManage(*c, tr("Transferring checks")))
        return false;
    const Employee *to = s_->employee(ss(employeeId));
    if (!to || !to->active)
        return fail(tr("Choose someone who is working."));
    if (to->id == c->serverId)
        return fail(tr("%1 is already %2's.").arg(qs(c->label), qs(to->name)));
    noteEvent(*c, tr("Transferred from %1 to %2").arg(qs(c->serverName), qs(to->name)), "transfer");
    c->serverId = to->id;
    c->serverName = to->name;
    emit notice(tr("%1 is now %2's").arg(qs(c->label), qs(to->name)));
    changed(*c);
    return true;
}

bool PosService::moveCheck(const QString &table)
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (!mayManage(*c, tr("Moving checks")))
        return false;
    const QString to = table.trimmed();
    if (to.isEmpty())
        return fail(tr("Choose a table."));
    if (c->type != CheckType::DineIn)
        return fail(tr("Only table checks move between tables."));
    if (qs(c->label) == to)
        return fail(tr("The check is already at %1.").arg(to));
    noteEvent(*c, tr("Moved from %1 to %2").arg(qs(c->label), to), "move");
    const QString from = qs(c->label);
    c->label = ss(to);
    emit notice(tr("Moved from %1 to %2").arg(from, to));
    changed(*c);
    if (!c->training)
        tableEmptied(ss(from));   // the table they left needs bussing (if no one else is there)
    return true;
}

bool PosService::mergeCheck(qint64 otherId)
{
    Check *c = current();
    if (!c)
        return fail(tr("Open the check to merge into first."));
    if (otherId == c->id)
        return fail(tr("Choose another check."));
    auto it = s_->open.find(otherId);
    if (it == s_->open.end())
        return fail(tr("That check is no longer open."));
    Check &other = it->second;
    if (const QString holder = lockHolder(other.id); !holder.isEmpty())
        return fail(tr("%1 is open on %2.").arg(qs(other.label), holder));
    if (!mayManage(*c, tr("Merging checks")) || !mayManage(other, tr("Merging checks")))
        return false;

    const QString what = tr("%1 #%2 merged into %3 #%4").arg(qs(other.label)).arg(other.id).arg(qs(c->label)).arg(c->id);
    c->absorb(other);
    // A held card comes along (one is enough: the other's is let go).
    if (!other.hold.empty() && c->hold.empty())
        std::swap(c->hold, other.hold);
    letHoldGo(other, tr("Held card let go"));
    noteEvent(*c, what, "merge");
    noteEvent(other, what, "merge");
    other.status = CheckStatus::Merged;
    other.closedAt = now();
    if (s_->sink)
        s_->sink->saveCheck(other);
    const bool otherTable = other.type == CheckType::DineIn && !other.training && other.label != c->label;
    const std::string left = other.label;
    s_->open.erase(it);
    emit notice(what);
    changed(*c);
    if (otherTable)
        tableEmptied(left);   // its guests joined this table: theirs needs bussing
    return true;
}

bool PosService::reopenCheck(qint64 checkId)
{
    if (!require(perm::Manager, tr("Reopening checks")))
        return false;
    auto it = std::ranges::find_if(s_->closedToday, [&](const Check &c) { return c.id == checkId; });
    if (it == s_->closedToday.end())
        return fail(tr("Only checks closed today (since the last End of Day) can be reopened."));
    if (const QString blocked = reopenBlocked(*it); !blocked.isEmpty())
        return fail(blocked);
    undoCloseEffects(*it);   // gift cards sold on it, the customer's visit
    // Its cash leaves the drawer or bank it went into until it closes again.
    Check c = *it;
    s_->closedToday.erase(it);
    c.status = CheckStatus::Open;
    c.closedAt = 0;
    c.businessDay = 0;
    c.drawerSession = 0;
    noteEvent(c, tr("Reopened"), "reopen", c.totals(s_->settings.tax).total);
    const std::int64_t id = c.id;
    s_->open[id] = std::move(c);
    if (s_->sink)
        s_->sink->saveCheck(s_->open[id]);
    emit notice(tr("%1 #%2 is open again").arg(qs(s_->open[id].label)).arg(id));
    emit s_->dayChanged();
    emit s_->drawerChanged();
    return openCheck(id);
}

// --- rush and VIP --------------------------------------------------------------------

bool PosService::toggleFlag(const QString &flag)
{
    if (!require(perm::Order, tr("Rush and VIP")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    bool *f = flag == u"rush" ? &c->rush : flag == u"vip" ? &c->vip : nullptr;
    if (!f)
        return fail(tr("Unknown flag: %1").arg(flag));
    *f = !*f;
    const QString name = flag == u"rush" ? tr("Rush") : tr("VIP");
    noteEvent(*c, *f ? tr("%1 on").arg(name) : tr("%1 off").arg(name), "flag");
    emit notice(*f ? tr("%1: the kitchen sees it").arg(name) : tr("%1 off").arg(name));
    changed(*c);
    emit s_->checksChanged();   // the kitchen display re-sorts
    return true;
}

// --- allergies ------------------------------------------------------------------------

bool PosService::setAllergies(const QStringList &ids)
{
    if (!require(perm::Order, tr("Allergies")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    std::vector<std::string> list;
    for (const std::string &a : allergenIds())   // in their usual order
        if (ids.contains(qs(a)))
            list.push_back(a);
    if (list == c->allergies)
        return true;
    c->allergies = list;
    noteEvent(*c, list.empty() ? tr("Allergies cleared")
                               : tr("Allergic to %1").arg(allergenNames(list).join(u", "_s)), "flag");
    // What's already on it that has one.
    QStringList risky;
    for (const OrderLine &l : c->lines)
        if (!l.voided && !allergyHits(*c, l).empty())
            risky << qs(l.name);
    emit notice(risky.isEmpty() ? (list.empty() ? tr("Allergies cleared") : tr("Allergies: the kitchen sees them"))
                                : tr("⚠ Already on the check, with one of them: %1").arg(risky.join(u", "_s)));
    changed(*c);
    emit s_->checksChanged();
    return true;
}

std::vector<std::string> PosService::allergyHits(const Check &c, const OrderLine &l)
{
    std::vector<std::string> out;
    for (const std::string &a : l.allergens)
        if (std::ranges::find(c.allergies, a) != c.allergies.end())
            out.push_back(a);
    return out;
}

// --- seats and courses ------------------------------------------------------------

bool PosService::setSeat(int seat)
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    seat_ = std::clamp(seat, 0, 99);
    // A line someone touched moves to that seat; otherwise this is the seat
    // for the next items.
    if (OrderLine *l = c->line(selectedLine_); l && lineTouched_ && !l->voided)
        l->seat = seat_;
    changed(*c);
    return true;
}

bool PosService::setCourse(int course)
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    course_ = std::clamp(course, 1, 9);
    if (OrderLine *l = c->line(selectedLine_); l && lineTouched_ && !l->sent)
        l->course = course_;
    changed(*c);
    return true;
}

bool PosService::fireCourse()
{
    if (!require(perm::Order, tr("Firing courses")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    return fireCourseOn(*c, false);
}

bool PosService::fireCourseIn(int minutes)
{
    if (!require(perm::Order, tr("Firing courses")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (minutes < 0) {
        c->fireAt = 0;
        emit notice(tr("The next course waits until someone fires it."));
        changed(*c);
        return true;
    }
    if (minutes == 0)
        return fireCourseOn(*c, false);
    if (c->heldCount() == 0)
        return fail(tr("No course is on hold."));
    c->fireAt = now() + std::int64_t(std::min(minutes, 120)) * 60'000;
    emit notice(tr("The next course fires at %1.").arg(i18n::timeText(QDateTime::fromMSecsSinceEpoch(c->fireAt).time())));
    changed(*c);
    return true;
}

bool PosService::fireCourseOn(Check &check, bool paced)
{
    Check *c = &check;
    // The course about to go out must be complete.
    int next = 0;
    for (const OrderLine &l : c->lines) {
        if (c->held(l) && (next == 0 || l.course < next))
            next = l.course;
    }
    std::vector<OrderLine> course_lines;
    for (const OrderLine &l : c->lines) {
        if (c->held(l) && l.course == next)
            course_lines.push_back(l);
    }
    if (const QString missing = missingChoice(course_lines); !missing.isEmpty())
        return fail(missing);
    if (!c->training)
        takeStock(course_lines, 1);
    const int course = c->fireNextCourse();
    if (course == 0)
        return fail(tr("No course is on hold."));
    c->fireAt = 0;   // fired: any pacing is done
    const std::vector<OrderLine> fresh = c->sendable();
    const int n = c->sendAll(now());
    if (s_->printer && !fresh.empty() && !c->training)
        s_->printer->printKitchen(s_->settings, *c, fresh, false);
    emit notice(paced ? tr("%1: course %2 fired on time (%3 items)").arg(qs(c->label)).arg(course).arg(n)
                      : tr("Fired course %1 (%2 items)").arg(course).arg(n));
    changed(*c);
    return true;
}

QVariantList PosService::closedChecks() const
{
    QVariantList out;
    if (!can(QString::fromLatin1(perm::Manager)))
        return out;
    for (auto it = s_->closedToday.rbegin(); it != s_->closedToday.rend(); ++it) {   // newest first
        const Check &c = *it;
        const Totals t = c.totals(s_->settings.tax);
        out.append(QVariantMap{
            {u"id"_s, qint64(c.id)}, {u"label"_s, qs(c.label)}, {u"server"_s, qs(c.serverName)},
            {u"guests"_s, c.guests}, {u"total"_s, format(t.total)}, {u"closed"_s, timeOf(c.closedAt)},
            {u"type"_s, qs(toString(c.type))}, {u"customer"_s, qs(c.customer.name)},
        });
    }
    return out;
}

QVariantList PosService::staff() const
{
    QVariantList out;
    if (!user())
        return out;
    for (const Employee &e : s_->employees) {
        if (!e.active)
            continue;
        const bool onClock = std::ranges::any_of(s_->punches, [&](const TimePunch &p) {
            return p.employeeId == e.id && p.clockOut == 0;
        });
        out.append(QVariantMap{{u"id"_s, qs(e.id)}, {u"name"_s, qs(e.name)}, {u"role"_s, qs(e.role)},
                               {u"clockedIn"_s, onClock}, {u"me"_s, e.id == user()->id}});
    }
    return out;
}

QVariantList PosService::checkHistory() const
{
    QVariantList out;
    if (const Check *c = currentCheck()) {
        for (const CheckEvent &e : c->events)
            out.append(QVariantMap{{u"time"_s, timeOf(e.at)}, {u"who"_s, qs(e.who)}, {u"what"_s, qs(e.what)}});
    }
    return out;
}

} // namespace vt::app
