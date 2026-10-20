// PosService: Rush Radar, each kitchen station's next half hour. What's ahead
// of each station now, what's coming minute by minute (orders not sent yet,
// tables just seated, the waitlist and reservations, orders for later, and
// the usual flow of new tables for this weekday and time), and the wait that
// makes at each station's own pace (core/rush.hh).

#include "app/pos_service.hh"

#include "app/i18n.hh"
#include "app/pos_json.hh"

#include <QDateTime>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr int kHorizon = 30;              // minutes ahead
constexpr std::int64_t kMinuteMs = 60'000;
constexpr int kSlots = 7 * 144;           // weekday x 10 minutes
constexpr int kFirstOrderDefault = 8;     // minutes from sitting down to the first order, until learned

int slotOf(std::int64_t ms)
{
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms);
    return (t.date().dayOfWeek() % 7) * 144 + (t.time().hour() * 60 + t.time().minute()) / 10;
}

QString clock(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

} // namespace

void PosShared::RushPast::count(const Check &c)
{
    if (c.training || c.status == CheckStatus::Discarded || c.status == CheckStatus::Merged)
        return;
    if (c.openedAt > 0) {
        const QDateTime opened = QDateTime::fromMSecsSinceEpoch(c.openedAt);
        ++checkHours[(opened.date().dayOfWeek() % 7) * 24 + opened.time().hour()];
        days.insert(opened.date().toJulianDay());
    }
    std::int64_t first = 0;
    for (const OrderLine &l : c.lines) {
        if (!l.sent || l.voided || l.isComment() || !l.forKitchen() || l.sentAt <= 0)
            continue;
        std::vector<int> &counts = itemSlots[l.itemId];
        if (counts.empty())
            counts.assign(kSlots, 0);
        counts[slotOf(l.sentAt)] += l.quantity;
        days.insert(QDateTime::fromMSecsSinceEpoch(l.sentAt).date().toJulianDay());
        if (c.type == CheckType::DineIn && c.guests > 0)
            tableItems[l.itemId] += l.quantity;
        first = first == 0 ? l.sentAt : std::min(first, l.sentAt);
    }
    if (c.type == CheckType::DineIn && c.guests > 0 && first > 0) {
        tableGuests += c.guests;
        if (first > c.openedAt && first - c.openedAt < 60 * kMinuteMs) {
            firstOrderMs += first - c.openedAt;
            ++firstOrders;
        }
    }
}

std::map<std::string, int> PosService::stationQueues(std::int64_t sentBefore) const
{
    std::map<std::string, int> out;
    const std::int64_t recent = now() - 90 * kMinuteMs;   // older: forgotten on the screen
    const auto look = [&](const Check &c) {
        if (c.training)
            return;
        for (const OrderLine &l : c.lines)
            if (l.sent && !l.made && !l.voided && !l.isComment() && l.forKitchen() && !c.held(l) && l.sentAt > recent
                && (sentBefore == 0 || l.sentAt < sentBefore))
                if (const std::string at = stationOf(l); !at.empty())
                    out[at] += l.quantity;
    };
    for (const auto &[id, c] : s_->open)
        look(c);
    for (const Check &c : s_->closedToday)
        look(c);
    return out;
}

void PosService::noteQueuesAtSend(Check &check)
{
    // Lines just sent: what was ahead of each at its station (sent before it, not made).
    std::map<std::int64_t, std::map<std::string, int>> bySend;
    for (OrderLine &l : check.lines) {
        if (!l.sent || l.queueAtSend >= 0 || l.sentAt <= 0 || l.isComment() || check.training)
            continue;
        auto it = bySend.find(l.sentAt);
        if (it == bySend.end())
            it = bySend.emplace(l.sentAt, stationQueues(l.sentAt)).first;
        const std::string at = stationOf(l);
        l.queueAtSend = at.empty() ? 0 : it->second[at];
    }
}

double PosService::typicalMinutes(const std::string &station) const
{
    // An item's usual ticket time at a station before its pace is known: the
    // prep times learned for what it makes, else half the store's warning time.
    double sum = 0;
    int n = 0;
    for (const auto &[item, sec] : s_->settings.prepSeconds)
        if (const MenuItem *m = findItem(qs(item))) {
            OrderLine probe;
            probe.itemId = item;
            probe.station = m->station;
            probe.printer = m->printer;
            if (stationOf(probe) == station) {
                sum += sec / 60.0;
                ++n;
            }
        }
    return n > 0 ? sum / n : double(s_->settings.kitchenWarnMinutes) / 2.0;
}

int PosService::kitchenMinutesFor(const Check &c, bool *paceKnown) const
{
    // Each station this order uses: what's ahead of it now (its own items
    // already sent are in that), plus its items not sent yet, at the
    // station's pace. Ready when the slowest is.
    std::map<std::string, int> unsent;
    std::set<std::string> uses;
    for (const OrderLine &l : c.lines) {
        if (l.voided || l.isComment() || !l.forKitchen() || (l.sent && l.made))
            continue;
        const std::string at = stationOf(l);
        if (at.empty())
            continue;
        uses.insert(at);
        if (!l.sent)
            unsent[at] += l.quantity;
    }
    if (uses.empty())
        return 0;
    const std::map<std::string, int> queues = stationQueues();
    double worst = 0;
    bool known = true;
    for (const std::string &at : uses) {
        const auto pace = s_->settings.stationPace.find(at);
        const StationPace p = pace != s_->settings.stationPace.end() ? pace->second : StationPace{};
        known = known && p.known();
        const double ahead = (queues.contains(at) ? queues.at(at) : 0) + (unsent.contains(at) ? unsent.at(at) : 0);
        // Its own items count as ahead of the last of them (none ahead of a lone item).
        worst = std::max(worst, p.minutesAt(std::max(0.0, ahead - 1), typicalMinutes(at)));
    }
    if (paceKnown)
        *paceKnown = known;
    return int(std::ceil(worst));
}

QVariantMap PosService::staffingNow() const
{
    const std::int64_t t = now();
    const PosSettings &st = s_->settings;
    askPastOnce();
    const PosShared::RushPast &past = s_->rushPast;
    const QDateTime nowDt = QDateTime::fromMSecsSinceEpoch(t);
    const int today = nowDt.date().dayOfWeek() % 7;
    int days = 0;
    for (const std::int64_t d : past.days)
        days += QDate::fromJulianDay(d).dayOfWeek() % 7 == today;
    if (days == 0)
        return {{u"known"_s, false}};
    const auto usual = [&](int hour) {   // checks this weekday usually opens in that hour
        return hour < 24 ? double(past.checkHours[today * 24 + hour]) / days : 0.0;
    };
    const int hour = nowDt.time().hour();
    const double frac = nowDt.time().minute() / 60.0;
    // Today's pace: checks opened so far against the usual by now.
    double usualSoFar = usual(hour) * frac, todaySoFar = 0;
    for (int h = 0; h < hour; ++h)
        usualSoFar += usual(h);
    const std::int64_t dayStart = QDateTime(nowDt.date(), QTime(0, 0)).toMSecsSinceEpoch();
    for (const Check &c : s_->closedToday)
        todaySoFar += !c.training && c.openedAt >= dayStart;
    for (const auto &[id, c] : s_->open)
        todaySoFar += !c.training && c.openedAt >= dayStart;
    const double pace = usualSoFar >= 5 ? std::clamp(todaySoFar / usualSoFar, 0.4, 2.5) : 1.0;
    // The next hour, and the one after (not cut before a rush).
    const double next = (usual(hour) * (1 - frac) + usual(hour + 1) * frac) * pace;
    const double after = (usual(hour + 1) * (1 - frac) + usual(hour + 2) * frac) * pace;
    const int perServer = std::max(1, st.checksPerServerHour);
    const auto need = [&](double checks) { return checks < 0.3 ? 0 : std::max(1, int(std::ceil(checks / perServer))); };
    const int needed = need(next), neededAfter = need(after);

    // Servers on the clock now (back from a break counts), and their tables.
    struct On { const Employee *e; std::int64_t since; int tables; };
    std::vector<On> on;
    for (const TimePunch &p : s_->punches) {
        if (!p.open())
            continue;
        const Employee *e = s_->employee(p.employeeId);
        if (!e || (p.job.empty() ? e->role : p.job) != "server")
            continue;
        int tables = 0;
        for (const auto &[id, c] : s_->open)
            tables += c.serverId == e->id && !c.training;
        on.push_back({e, p.clockIn, tables});
    }
    const int servers = int(on.size());
    const auto hoursText = [](std::int64_t ms) {
        return i18n::locale().toString(double(ms) / 3'600'000.0, 'f', 1);
    };
    QVariantMap out{{u"known"_s, true}, {u"expected"_s, int(std::lround(next))}, {u"needed"_s, needed},
                    {u"servers"_s, servers}, {u"pacePercent"_s, int(std::lround(pace * 100))}};
    if (servers > std::max(needed, neededAfter) && servers > 1) {
        // Cut one: whoever has no open tables, the longest on first.
        std::ranges::stable_sort(on, [](const On &a, const On &b) {
            return std::pair(a.tables, a.since) < std::pair(b.tables, b.since);
        });
        const On &who = on.front();
        out.insert(u"action"_s, u"cut"_s);
        out.insert(u"who"_s, qs(who.e->name));
        out.insert(u"text"_s, tr("Cut a server: about %n check(s) the next hour, ", "", int(std::lround(next)))
                                  + tr("%n server(s) on, ", "", servers) + tr("%1 needed.").arg(std::max(needed, neededAfter)) + u' '
                                  + (who.tables == 0 ? tr("%1 has no open tables and has worked %2 h.").arg(qs(who.e->name), hoursText(t - who.since))
                                                     : tr("%1 has the fewest tables (%2).").arg(qs(who.e->name)).arg(who.tables)));
    } else if (servers < needed) {
        // Call one in: whoever's shift starts next (today, the next 3 hours).
        const Shift *soonest = nullptr;
        for (const Shift &sh : s_->shifts) {
            if (sh.start <= t || sh.start > t + 3 * 3'600'000LL)
                continue;
            const Employee *e = s_->employee(sh.employeeId);
            if (!e || e->role != "server" || std::ranges::any_of(on, [&](const On &o) { return o.e == e; }))
                continue;
            if (!soonest || sh.start < soonest->start)
                soonest = &sh;
        }
        out.insert(u"action"_s, u"call"_s);
        QString text = tr("Call a server in: about %n check(s) the next hour", "", int(std::lround(next)))
                       + (std::abs(pace - 1.0) >= 0.15 ? tr(" (today is running at %1% of usual)").arg(int(std::lround(pace * 100))) : QString())
                       + u", "_s + tr("%n server(s) on, ", "", servers) + tr("%1 needed.").arg(needed);
        if (soonest) {
            const Employee *e = s_->employee(soonest->employeeId);
            out.insert(u"who"_s, qs(e->name));
            text += u' ' + tr("%1 starts at %2: ask them in early.").arg(qs(e->name), clock(soonest->start));
        }
        out.insert(u"text"_s, text);
    } else {
        out.insert(u"action"_s, u"ok"_s);
        out.insert(u"text"_s, tr("Staffing fits: about %n check(s) the next hour, ", "", int(std::lround(next)))
                                  + tr("%n server(s) on.", "", servers));
    }
    return out;
}

QVariantMap PosService::rushRadar() const
{
    return s_->memo(u"rushRadar "_s + language(), [this] { return QVariant(rushRadarNow()); }).toMap();
}

QVariantMap PosService::rushRadarNow() const
{
    const std::int64_t t = now();
    const PosSettings &st = s_->settings;
    askPastOnce();
    const PosShared::RushPast &past = s_->rushPast;

    // Each station's arrivals, minute by minute, and its line now.
    std::map<std::string, std::vector<double>> arrivals;
    const auto add = [&](const std::string &station, double minute, double items) {
        if (station.empty() || items <= 0)
            return;
        std::vector<double> &a = arrivals[station];
        if (a.empty())
            a.assign(kHorizon, 0.0);
        const int m = std::clamp(int(std::floor(minute)), 0, kHorizon - 1);
        if (minute < kHorizon)
            a[m] += items;
    };
    // Items a table orders, by station: what tables here usually order per guest.
    std::map<std::string, double> perGuest;
    if (past.tableGuests >= 20)
        for (const auto &[item, n] : past.tableItems) {
            OrderLine probe;
            if (const MenuItem *m = findItem(qs(item))) {
                probe.itemId = item;
                probe.station = m->station;
                probe.printer = m->printer;
                perGuest[stationOf(probe)] += double(n) / double(past.tableGuests);
            }
        }
    const double firstOrder = past.firstOrders >= 10 ? double(past.firstOrderMs) / past.firstOrders / kMinuteMs
                                                     : double(kFirstOrderDefault);
    const auto table = [&](double minute, int guests) {   // a table about to order
        for (const auto &[station, n] : perGuest)
            add(station, minute, n * guests);
    };

    for (const auto &[id, c] : s_->open) {
        if (c.training)
            continue;
        bool anySent = false;
        std::map<std::string, double> unsent;
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment() || !l.forKitchen())
                continue;
            anySent = anySent || l.sent;
            if (l.sent)
                continue;
            const std::string at = stationOf(l);
            // Held for a later course: when it's fired (or in a quarter of an hour).
            if (c.held(l))
                add(at, c.fireAt > t ? double(c.fireAt - t) / kMinuteMs : 15.0, l.quantity);
            else if (c.dueAt > 0)   // an order for later: when it goes to the kitchen
                add(at, double(c.dueAt - std::int64_t(st.laterLeadMinutes) * kMinuteMs - t) / kMinuteMs, l.quantity);
            else
                unsent[at] += l.quantity;   // being rung in: sent in a few minutes
        }
        for (const auto &[at, n] : unsent)
            add(at, 2, n);
        // A table just sat down, nothing ordered yet: what tables usually order.
        if (!anySent && unsent.empty() && c.type == CheckType::DineIn && c.guests > 0 && t - c.openedAt < 30 * kMinuteMs)
            table(std::max(1.0, firstOrder - double(t - c.openedAt) / kMinuteMs), c.guests);
    }
    // The waitlist (seated when quoted) and reservations coming in.
    for (const Party &p : s_->parties) {
        if (p.status != Party::Status::Waiting && p.status != Party::Status::Booked)
            continue;
        const std::int64_t seat = p.reservedFor > 0 ? std::max(p.reservedFor, t)
                                                    : std::max(t, p.waitingSince() + std::int64_t(p.quotedMinutes) * kMinuteMs);
        table(double(seat - t) / kMinuteMs + firstOrder, p.size);
    }
    // New tables nobody knows of yet: the usual for this weekday and time
    // (the last four weeks), at today's pace, after the known tables have ordered.
    std::map<int, int> weekdays;   // weekday -> days seen
    for (const std::int64_t d : past.days)
        ++weekdays[QDate::fromJulianDay(d).dayOfWeek() % 7];
    const int today = QDateTime::fromMSecsSinceEpoch(t).date().dayOfWeek() % 7;
    if (const int days = weekdays[today]; days > 0) {
        // Today's pace: items sent so far against the usual by this time.
        double usualSoFar = 0, todaySoFar = 0;
        const int nowSlot = slotOf(t);
        for (const auto &[item, counts] : past.itemSlots)
            for (int s = today * 144; s < nowSlot; ++s)
                usualSoFar += double(counts[s]) / days;
        for (const Check &c : s_->closedToday)
            for (const OrderLine &l : c.lines)
                todaySoFar += l.sent && !l.voided && l.forKitchen() && !c.training ? l.quantity : 0;
        for (const auto &[id, c] : s_->open)
            for (const OrderLine &l : c.lines)
                todaySoFar += l.sent && !l.voided && l.forKitchen() && !c.training ? l.quantity : 0;
        const double pace = usualSoFar >= 20 ? std::clamp(todaySoFar / usualSoFar, 0.5, 2.0) : 1.0;
        std::vector<int> minuteSlot(kHorizon);
        for (int minute = 0; minute < kHorizon; ++minute)
            minuteSlot[minute] = slotOf(t + std::int64_t(minute) * kMinuteMs);
        for (const auto &[item, counts] : past.itemSlots) {
            OrderLine probe;
            const MenuItem *m = findItem(qs(item));
            if (!m)
                continue;
            probe.itemId = item;
            probe.station = m->station;
            probe.printer = m->printer;
            const std::string at = stationOf(probe);
            for (int minute = int(std::ceil(firstOrder)); minute < kHorizon; ++minute)
                add(at, minute, double(counts[minuteSlot[minute]]) / days / 10.0 * pace);
        }
    }

    // Each station: its line now, and the half hour ahead at its pace.
    const std::map<std::string, int> queues = stationQueues();
    std::set<std::string> places;
    for (const auto &[at, q] : queues)
        places.insert(at);
    for (const auto &[at, a] : arrivals)
        places.insert(at);
    for (const Station &x : st.stations)   // the store's stations, idle or not (what's quick right now)
        places.insert(x.id);
    const auto nameOf = [&](const std::string &id) {
        for (const Station &x : st.stations)
            if (x.id == id)
                return qs(x.name);
        for (const PrinterConfig &p : st.printers)
            if (p.id == id)
                return qs(p.name);
        QString n = qs(id);
        if (!n.isEmpty())
            n[0] = n[0].toUpper();
        return n;
    };
    QVariantList stations;
    QString worst;
    double worstWait = 0;
    for (const std::string &at : places) {
        const std::vector<double> a = arrivals.contains(at) ? arrivals.at(at) : std::vector<double>(kHorizon, 0.0);
        const double usual = typicalMinutes(at);
        const auto pace = st.stationPace.find(at);
        const StationPace p = pace != st.stationPace.end() ? pace->second : StationPace{};
        const double queueNow = queues.contains(at) ? queues.at(at) : 0;
        const std::vector<RushPoint> points = projectRush(queueNow, a, p, usual);
        QVariantList ahead;
        RushPoint peak = points.front();
        for (const RushPoint &r : points) {
            if (r.minute % 5 == 0)
                ahead.append(QVariantMap{{u"minute"_s, r.minute}, {u"at"_s, clock(t + std::int64_t(r.minute) * kMinuteMs)},
                                         {u"wait"_s, int(std::lround(r.wait))}, {u"queue"_s, int(std::lround(r.queue))}});
            if (r.wait > peak.wait)
                peak = r;
        }
        double coming = 0;
        for (const double x : a)
            coming += x;
        const int peakWait = int(std::lround(peak.wait));
        const QString level = peakWait >= st.kitchenLateMinutes ? u"late"_s : peakWait >= st.kitchenWarnMinutes ? u"busy"_s : u"ok"_s;
        stations.append(QVariantMap{
            {u"id"_s, qs(at)}, {u"name"_s, nameOf(at)}, {u"queue"_s, int(queueNow)},
            {u"waitNow"_s, int(std::lround(points.front().wait))}, {u"coming"_s, int(std::lround(coming))},
            {u"peakWait"_s, peakWait}, {u"peakAt"_s, clock(t + std::int64_t(peak.minute) * kMinuteMs)},
            {u"peakMinute"_s, peak.minute}, {u"level"_s, level}, {u"learned"_s, p.known()}, {u"ahead"_s, ahead}});
        if (peak.wait > worstWait && level != u"ok") {
            worstWait = peak.wait;
            worst = peak.minute == 0 ? tr("%1 is at %2 min now.").arg(nameOf(at)).arg(peakWait)
                                     : tr("%1 backs up to %2 min at %3.").arg(nameOf(at)).arg(peakWait)
                                           .arg(clock(t + std::int64_t(peak.minute) * kMinuteMs));
        }
    }
    std::ranges::stable_sort(stations, [](const QVariant &x, const QVariant &y) {
        return x.toMap().value(u"peakWait"_s).toInt() > y.toMap().value(u"peakWait"_s).toInt();
    });
    return {{u"stations"_s, stations}, {u"headline"_s, worst}, {u"horizon"_s, kHorizon},
            {u"warn"_s, st.kitchenWarnMinutes}, {u"late"_s, st.kitchenLateMinutes},
            {u"updated"_s, clock(t)}, {u"history"_s, !past.days.empty()}};
}

} // namespace vt::app
