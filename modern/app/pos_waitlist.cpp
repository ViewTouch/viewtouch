// PosService: the host stand - a waitlist (quotes, "your table is ready",
// seating) and reservations (booked, checked in, seated or no-show).

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kMinute = 60'000;

QString clock(std::int64_t ms) { return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms), u"h:mm AP"_s); }

QString dayAndTime(std::int64_t ms, std::int64_t now)
{
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms);
    const QDate today = QDateTime::fromMSecsSinceEpoch(now).date();
    if (t.date() == today)
        return i18n::locale().toString(t, u"h:mm AP"_s);
    if (t.date() == today.addDays(1))
        return QObject::tr("Tomorrow %1").arg(i18n::locale().toString(t, u"h:mm AP"_s));
    return i18n::locale().toString(t, u"ddd MMM d, h:mm AP"_s);
}

} // namespace

Party *PosService::party(qint64 id)
{
    auto it = std::ranges::find_if(s_->parties, [&](const Party &p) { return p.id == id; });
    return it == s_->parties.end() ? nullptr : &*it;
}

void PosService::saveParty(const Party &p)
{
    if (s_->sink)
        s_->sink->saveParty(p);
    emit s_->dayChanged();
}

int PosService::quoteFor(int ahead) const
{
    const int minutes = (ahead + 1) * std::max(1, s_->settings.waitMinutesPerParty);
    return std::max(5, (minutes + 4) / 5 * 5);   // to the next 5 minutes
}

QVariantMap PosService::waitlistInfo() const
{
    if (!user())
        return {};
    const std::int64_t t = now();
    const auto row = [&](const Party &p, int position) {
        const int waited = int((t - p.waitingSince()) / kMinute);
        return QVariantMap{
            {u"id"_s, qint64(p.id)}, {u"name"_s, qs(p.name)}, {u"phone"_s, qs(p.phone)}, {u"size"_s, p.size},
            {u"note"_s, qs(p.note)}, {u"status"_s, qs(toString(p.status))}, {u"position"_s, position},
            {u"waited"_s, waited}, {u"quoted"_s, p.quotedMinutes},
            {u"late"_s, p.quotedMinutes > 0 && waited > p.quotedMinutes},
            {u"notifiedAgo"_s, p.notifiedAt ? int((t - p.notifiedAt) / kMinute) : -1},
            {u"reservation"_s, p.reservedFor != 0},
            {u"time"_s, p.reservedFor ? dayAndTime(p.reservedFor, t) : clock(p.addedAt)},
            {u"reservedForMs"_s, qint64(p.reservedFor)},
            // A booking that should have arrived by now (15 minutes' grace).
            {u"overdue"_s, p.status == Party::Status::Booked && t > p.reservedFor + 15 * kMinute},
            {u"table"_s, qs(p.table)}, {u"regular"_s, regularText(p.customerId)},
            {u"regularNote"_s, [&] {
                 const CustomerRecord *r = p.customerId.empty() ? nullptr : s_->customer(p.customerId);
                 return r ? qs(r->note) : QString();
             }()},
            {u"visits"_s, [&] {
                 const CustomerRecord *r = p.customerId.empty() ? nullptr : s_->customer(p.customerId);
                 return r ? r->visits : 0;
             }()},
        };
    };
    QVariantList waiting, booked;
    std::vector<const Party *> line;
    for (const Party &p : s_->parties) {
        if (p.inLine())
            line.push_back(&p);
    }
    std::ranges::sort(line, {}, &Party::waitingSince);
    for (const Party *p : line)
        waiting.append(row(*p, int(waiting.size()) + 1));

    std::vector<const Party *> books;
    for (const Party &p : s_->parties) {
        if (p.status == Party::Status::Booked)
            books.push_back(&p);
    }
    std::ranges::sort(books, {}, &Party::reservedFor);
    for (const Party *p : books)
        booked.append(row(*p, 0));

    // Today's numbers: seated, average wait, no-shows. Today: since the
    // business day opened or since midnight, whichever came first.
    const std::int64_t midnight = QDateTime(QDateTime::fromMSecsSinceEpoch(t).date(), QTime(0, 0)).toMSecsSinceEpoch();
    const std::int64_t since = std::min(s_->day.openedAt, midnight);
    int seated = 0, noShows = 0;
    std::int64_t waitedTotal = 0;
    for (const Party &p : s_->parties) {
        if (p.status == Party::Status::Seated && p.seatedAt >= since && !p.walkIn) {
            ++seated;
            waitedTotal += p.seatedAt - p.waitingSince();
        } else if (p.status == Party::Status::NoShow && p.reservedFor >= since) {
            ++noShows;
        }
    }
    return {
        {u"waiting"_s, waiting}, {u"booked"_s, booked}, {u"seatedToday"_s, seated},
        {u"averageWait"_s, seated ? int(waitedTotal / seated / kMinute) : 0}, {u"noShows"_s, noShows},
        {u"nextQuote"_s, quoteFor(int(waiting.size()))},
    };
}

qint64 PosService::addParty(const QVariantMap &r, bool reservation)
{
    if (!require(perm::Order, reservation ? tr("Reservations") : tr("The waitlist")))
        return 0;
    const std::string name = ss(r.value(u"name"_s).toString().trimmed());
    if (name.empty()) {
        fail(tr("Whose party is it? Enter a name."));
        return 0;
    }
    Party p;
    p.name = name;
    p.phone = ss(r.value(u"phone"_s).toString().trimmed());
    p.size = std::clamp(r.value(u"size"_s, 2).toInt(), 1, 99);
    p.note = ss(r.value(u"note"_s).toString().trimmed());
    p.customerId = ss(r.value(u"customerId"_s).toString());
    // On file by their phone number: a regular (their visits, note and last order).
    if (const std::string digits = CustomerRecord::digits(p.phone); p.customerId.empty() && digits.size() >= 7)
        for (const CustomerRecord &c : s_->customers)
            if (CustomerRecord::digits(c.phone) == digits)
                p.customerId = c.id;
    p.addedAt = now();
    if (reservation) {
        std::int64_t at = r.value(u"at"_s).toLongLong();
        if (at <= 0)   // "2026-10-02 7:30 PM" (or 19:30)
            at = dateTimeMs(r.value(u"at"_s).toString());
        if (at <= 0) {
            fail(tr("When is the reservation for?"));
            return 0;
        }
        if (at < now() - 60 * kMinute) {
            fail(tr("That time has already passed."));
            return 0;
        }
        p.reservedFor = at;
        p.status = Party::Status::Booked;
    } else {
        int ahead = 0;
        for (const Party &x : s_->parties)
            ahead += x.inLine() ? 1 : 0;
        p.quotedMinutes = r.contains(u"quote"_s) ? std::clamp(r.value(u"quote"_s).toInt(), 0, 600) : quoteFor(ahead);
        p.status = Party::Status::Waiting;
    }
    p.id = ++s_->lastPartyId;
    s_->parties.push_back(p);
    saveParty(p);
    emit notice(reservation ? tr("%1, %2 people, booked for %3").arg(qs(p.name)).arg(p.size).arg(dayAndTime(p.reservedFor, now()))
                            : tr("%1, %2 people: about %3 minutes").arg(qs(p.name)).arg(p.size).arg(p.quotedMinutes));
    return p.id;
}

bool PosService::updateParty(qint64 id, const QVariantMap &r)
{
    if (!require(perm::Order, tr("The waitlist")))
        return false;
    Party *p = party(id);
    if (!p || p->done())
        return fail(tr("That party isn't waiting any more."));
    if (r.contains(u"name"_s) && !r.value(u"name"_s).toString().trimmed().isEmpty())
        p->name = ss(r.value(u"name"_s).toString().trimmed());
    if (r.contains(u"phone"_s))
        p->phone = ss(r.value(u"phone"_s).toString().trimmed());
    if (r.contains(u"size"_s))
        p->size = std::clamp(r.value(u"size"_s).toInt(), 1, 99);
    if (r.contains(u"note"_s))
        p->note = ss(r.value(u"note"_s).toString().trimmed());
    if (r.contains(u"quote"_s))
        p->quotedMinutes = std::clamp(r.value(u"quote"_s).toInt(), 0, 600);
    if (r.contains(u"at"_s) && p->status == Party::Status::Booked && r.value(u"at"_s).toLongLong() > 0)
        p->reservedFor = r.value(u"at"_s).toLongLong();
    saveParty(*p);
    return true;
}

bool PosService::checkInParty(qint64 id)
{
    if (!require(perm::Order, tr("Reservations")))
        return false;
    Party *p = party(id);
    if (!p || p->status != Party::Status::Booked)
        return fail(tr("That reservation isn't waiting to be checked in."));
    p->arrivedAt = now();
    p->status = Party::Status::Waiting;
    p->quotedMinutes = 0;   // seated as soon as their table is free
    saveParty(*p);
    emit notice(tr("%1 is here (%2 people)").arg(qs(p->name)).arg(p->size));
    return true;
}

bool PosService::notifyParty(qint64 id)
{
    if (!require(perm::Order, tr("The waitlist")))
        return false;
    Party *p = party(id);
    if (!p || !p->inLine())
        return fail(tr("That party isn't waiting."));
    p->status = Party::Status::Notified;
    p->notifiedAt = now();
    saveParty(*p);
    QString message = qs(s_->settings.tableReadyText);
    message.replace(u"{name}"_s, qs(p->name)).replace(u"{store}"_s, qs(s_->settings.storeName));
    if (!p->phone.empty() && s_->sendText) {
        s_->sendText(qs(p->phone), message);
        emit notice(tr("Texted %1: their table is ready").arg(qs(p->name)));
    } else {
        emit notice(tr("Tell %1 their table is ready").arg(qs(p->name)));
    }
    return true;
}

bool PosService::seatParty(qint64 id, const QString &table, const QString &serverId)
{
    if (!require(perm::Order, tr("Seating guests")))
        return false;
    Party *p = party(id);
    if (!p || p->done())
        return fail(tr("That party isn't waiting."));
    const QString label = table.trimmed();
    if (label.isEmpty())
        return fail(tr("Choose a table."));
    for (const auto &[cid, c] : s_->open) {
        if (c.type == CheckType::DineIn && QString::compare(qs(c.label), label, Qt::CaseInsensitive) == 0)
            return fail(tr("%1 is taken.").arg(label));
    }
    const Employee *server = serverId.isEmpty() ? user() : s_->employee(ss(serverId));
    if (!server)
        return fail(tr("Who is their server?"));

    // Their table's check, with the party size and their name, for the server.
    releaseCheck();
    pendingTable_ = label;
    entry_ = QString::number(p->size);
    if (!startCheck(CheckType::DineIn))
        return false;
    Check *c = current();
    c->serverId = server->id;
    c->serverName = server->name;
    c->customer.name = p->name;
    c->customer.phone = p->phone;
    c->customer.note = p->note;
    c->customerId = p->customerId;
    if (p->reservedFor)
        noteEvent(*c, tr("Reservation for %1").arg(clock(p->reservedFor)), "seat");
    changed(*c);
    p->status = Party::Status::Seated;
    p->seatedAt = now();
    p->table = ss(label);
    p->checkId = c->id;
    saveParty(*p);
    releaseCheck();   // it's the server's now
    emit notice(tr("%1 seated at %2 with %3").arg(qs(p->name), label, qs(server->name)));
    return true;
}

bool PosService::partyGone(qint64 id, bool noShow)
{
    if (!require(perm::Order, tr("The waitlist")))
        return false;
    Party *p = party(id);
    if (!p || p->done())
        return fail(tr("That party isn't waiting."));
    p->status = noShow && p->status == Party::Status::Booked ? Party::Status::NoShow : Party::Status::Left;
    saveParty(*p);
    emit notice(p->status == Party::Status::NoShow ? tr("%1 marked as a no-show").arg(qs(p->name))
                                                   : tr("%1 is off the list").arg(qs(p->name)));
    return true;
}

} // namespace vt::app

// --- the host stand: the floor, seating at tables, reserving, bussing --------------

namespace vt::app {

bool PosService::tableTaken(const QString &table) const
{
    for (const auto &[id, c] : s_->open)
        if (c.type == CheckType::DineIn && !c.training && QString::compare(qs(c.label), table, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

QVariantMap PosService::floor() const
{
    if (!user())
        return {};
    const std::int64_t t = now();
    QVariantMap out;
    // How long a party of this size stays: today's (3 or more), else the usual.
    const auto typicalMinutes = [&](int guests) {
        const int bucket = guests <= 2 ? 0 : guests <= 4 ? 1 : 2;
        std::vector<std::int64_t> stays;
        for (const Check &c : s_->closedToday)
            if (c.type == CheckType::DineIn && !c.training && c.closedAt > c.openedAt
                && (c.guests <= 2 ? 0 : c.guests <= 4 ? 1 : 2) == bucket)
                stays.push_back((c.closedAt - c.openedAt) / kMinute);
        if (stays.size() >= 3) {
            std::ranges::sort(stays);
            return int(stays[stays.size() / 2]);
        }
        return bucket == 0 ? 50 : bucket == 1 ? 65 : 80;
    };
    // Seated: a table check is open there.
    for (const auto &[id, c] : s_->open) {
        if (c.type != CheckType::DineIn || c.training)
            continue;
        const QString label = qs(c.label);
        if (out.contains(label))
            continue;
        const int seatedFor = int((t - c.openedAt) / kMinute);
        // Where the table is: paying, check printed, eating, or still ordering.
        const bool paying = std::ranges::any_of(c.payments, [](const Payment &p) { return p.kind != TenderKind::Discount; });
        const bool printed = std::ranges::any_of(c.events, [](const CheckEvent &e) { return e.kind == "printed"; });
        std::int64_t lastSent = 0;
        for (const OrderLine &l : c.lines)
            if (l.sent && !l.voided)
                lastSent = std::max(lastSent, l.sentAt);
        const int left = typicalMinutes(c.guests) - seatedFor;
        int freeIn = paying ? 3 : printed ? std::clamp(left, 3, 8) : lastSent == 0 ? std::max(left, 30) : std::max(left, 5);
        // A nudge for its server: nothing ordered yet, or a while since the last order.
        QString nudge;
        if (!paying && !printed) {
            if (lastSent == 0 && seatedFor >= 8)
                nudge = tr("Nothing ordered · %1m").arg(seatedFor);
            else if (lastSent > 0 && (t - lastSent) / kMinute >= 25)
                nudge = tr("Last order %1m ago").arg((t - lastSent) / kMinute);
        }
        out.insert(label, QVariantMap{{u"state"_s, u"seated"_s}, {u"guests"_s, c.guests},
                                      {u"server"_s, qs(c.serverName)}, {u"serverId"_s, qs(c.serverId)},
                                      {u"minutes"_s, seatedFor}, {u"freeIn"_s, freeIn},
                                      {u"stage"_s, paying ? u"paying"_s : printed ? u"check"_s : lastSent ? u"eating"_s : u"ordering"_s},
                                      {u"nudge"_s, nudge}, {u"party"_s, qs(c.customer.name)}});
    }
    // Who gets the next table: of the servers on the clock, the one with the
    // fewest guests seated now, and then the one seated longest ago.
    {
        struct Up { const Employee *e; int covers = 0; std::int64_t last = 0; };
        std::vector<Up> ups;
        for (const Employee &e : s_->employees) {
            if (!e.active || e.role != "server")
                continue;
            if (!std::ranges::any_of(s_->punches, [&](const TimePunch &p) { return p.employeeId == e.id && p.clockOut == 0; }))
                continue;
            Up u{&e};
            for (const auto &[id, c] : s_->open)
                if (c.serverId == e.id && c.type == CheckType::DineIn) {
                    u.covers += c.guests;
                    u.last = std::max(u.last, c.openedAt);
                }
            for (const Check &c : s_->closedToday)
                if (c.serverId == e.id && c.type == CheckType::DineIn)
                    u.last = std::max(u.last, c.openedAt);
            ups.push_back(u);
        }
        if (!ups.empty()) {
            const Up &next = *std::ranges::min_element(ups, [](const Up &a, const Up &b) {
                return a.covers != b.covers ? a.covers < b.covers : a.last < b.last;
            });
            out.insert(u"_next"_s, QVariantMap{{u"id"_s, qs(next.e->id)}, {u"name"_s, qs(next.e->name)},
                                               {u"covers"_s, next.covers}});
        }
    }
    for (const PosSettings::TableState &s : s_->settings.tableStates) {
        const QString label = qs(s.table);
        if (out.contains(label))
            continue;   // seated wins
        if (s.state == "joined") {
            // Pushed together with a seated table: seated too, with it.
            QVariantMap with = out.value(qs(s.with)).toMap();
            if (with.value(u"state"_s).toString() == u"seated") {
                with.insert(u"with"_s, qs(s.with));
                out.insert(label, with);
            }
        } else if (s.state == "dirty") {
            out.insert(label, QVariantMap{{u"state"_s, u"dirty"_s}, {u"minutes"_s, int((t - s.since) / kMinute)}});
        } else if (s.state == "reserved") {
            const Party *p = nullptr;
            for (const Party &x : s_->parties)
                if (x.id == s.partyId)
                    p = &x;
            if (!p || p->done())
                continue;   // seated somewhere else, left, or a no-show: free again
            out.insert(label, QVariantMap{{u"state"_s, u"reserved"_s}, {u"partyId"_s, qint64(p->id)},
                                          {u"party"_s, qs(p->name)}, {u"size"_s, p->size},
                                          {u"time"_s, p->reservedFor ? clock(p->reservedFor) : QString()}});
        }
    }
    return out;
}

void PosService::tableEmptied(const std::string &table)
{
    if (tableTaken(qs(table)))
        return;   // another check is still open there
    auto &states = s_->settings.tableStates;
    // It, and the tables pushed together with it, need bussing.
    std::vector<std::string> tables{table};
    for (const PosSettings::TableState &s : states)
        if (s.state == "joined" && s.with == table)
            tables.push_back(s.table);
    std::erase_if(states, [&](const PosSettings::TableState &s) {
        return std::ranges::find(tables, s.table) != tables.end();
    });
    for (const std::string &t : tables)
        states.push_back({t, "dirty", {}, {}, 0, now()});
    s_->saveSettings();
    emit s_->checksChanged();
}

bool PosService::setTableState(const QString &table, const QString &state)
{
    if (!require(perm::Order, tr("The host stand")))
        return false;
    const std::string label = ss(table.trimmed());
    if (label.empty())
        return fail(tr("Choose a table."));
    if (state != u"clean" && state != u"dirty")
        return fail(tr("Mark it clean or dirty."));
    if (tableTaken(table))
        return fail(tr("%1 has guests: close its check first.").arg(table));
    auto &states = s_->settings.tableStates;
    std::erase_if(states, [&](const PosSettings::TableState &s) { return s.table == label; });
    if (state == u"dirty")
        states.push_back({label, "dirty", {}, user()->name, 0, now()});
    s_->saveSettings();
    emit notice(state == u"dirty" ? tr("%1 needs bussing").arg(table) : tr("%1 is ready").arg(table));
    emit s_->checksChanged();
    return true;
}

bool PosService::reserveTables(qint64 partyId, const QStringList &tables)
{
    if (!require(perm::Order, tr("Reservations")))
        return false;
    Party *p = party(partyId);
    if (!p || p->done())
        return fail(tr("That party isn't waiting."));
    if (tables.isEmpty())
        return fail(tr("Choose a table."));
    const QVariantMap now_ = floor();
    for (const QString &t : tables) {
        const QVariantMap f = now_.value(t).toMap();
        if (f.value(u"state"_s).toString() == u"seated")
            return fail(tr("%1 has guests.").arg(t));
        if (f.value(u"state"_s).toString() == u"reserved" && f.value(u"partyId"_s).toLongLong() != partyId)
            return fail(tr("%1 is held for %2.").arg(t, f.value(u"party"_s).toString()));
    }
    auto &states = s_->settings.tableStates;
    // Their tables now: these (any held before are let go).
    std::erase_if(states, [&](const PosSettings::TableState &s) {
        return (s.state == "reserved" && s.partyId == partyId) || tables.contains(qs(s.table), Qt::CaseInsensitive);
    });
    for (const QString &t : tables)
        states.push_back({ss(t), "reserved", {}, user()->name, partyId, now()});
    p->table = ss(tables.join(u'+'));
    saveParty(*p);
    s_->saveSettings();
    emit notice(tr("%1 held for %2").arg(tables.join(u" + "_s), qs(p->name)));
    emit s_->checksChanged();
    return true;
}

bool PosService::seatPartyAt(qint64 partyId, const QStringList &tables, const QString &serverId)
{
    if (tables.isEmpty())
        return fail(tr("Choose a table."));
    const QVariantMap f = floor();
    for (const QString &t : tables) {
        const QVariantMap s = f.value(t).toMap();
        if (s.value(u"state"_s).toString() == u"seated")
            return fail(tr("%1 is taken.").arg(t));
        if (s.value(u"state"_s).toString() == u"reserved" && s.value(u"partyId"_s).toLongLong() != partyId)
            return fail(tr("%1 is held for %2.").arg(t, s.value(u"party"_s).toString()));
    }
    // The check goes on the first table; the others are pushed together with it.
    if (!seatParty(partyId, tables.first(), serverId))
        return false;
    auto &states = s_->settings.tableStates;
    std::erase_if(states, [&](const PosSettings::TableState &s) {
        return tables.contains(qs(s.table), Qt::CaseInsensitive) || (s.state == "reserved" && s.partyId == partyId);
    });
    for (qsizetype i = 1; i < tables.size(); ++i)
        states.push_back({ss(tables[i]), "joined", ss(tables.first()), user()->name, partyId, now()});
    if (Party *p = party(partyId); p && tables.size() > 1) {
        p->table = ss(tables.join(u'+'));
        saveParty(*p);
    }
    s_->saveSettings();
    emit s_->checksChanged();
    return true;
}

bool PosService::seatWalkIn(int size, const QStringList &tables, const QString &serverId)
{
    if (!require(perm::Order, tr("Seating guests")))
        return false;
    Party p;
    p.id = ++s_->lastPartyId;
    p.name = ss(tr("Walk-in"));
    p.size = std::clamp(size, 1, 99);
    p.addedAt = now();
    p.walkIn = true;
    p.status = Party::Status::Waiting;
    s_->parties.push_back(p);
    saveParty(p);
    return seatPartyAt(p.id, tables, serverId);
}

} // namespace vt::app
