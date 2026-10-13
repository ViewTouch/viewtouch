#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The host stand: waitlist and reservations.

namespace {

qint64 todayAt(int hour, int minute = 0)
{
    return QDateTime(QDate::currentDate(), QTime(hour, minute)).toMSecsSinceEpoch();
}

QVariantList waiting(const PosService &pos) { return pos.waitlistInfo()[u"waiting"_s].toList(); }
QVariantList booked(const PosService &pos) { return pos.waitlistInfo()[u"booked"_s].toList(); }

QString idOf(const PosService &pos, const QString &name)
{
    for (const QVariant &v : pos.staff()) {
        if (v.toMap()[u"name"_s].toString().startsWith(name))
            return v.toMap()[u"id"_s].toString();
    }
    return {};
}

} // namespace

TEST_CASE("Waitlist: quotes, the table-ready text, seating for a server", "[waitlist]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(18);
    pos.shared()->setClock([&] { return clock; });
    QStringList texts;
    pos.shared()->sendText = [&](const QString &phone, const QString &message) { texts << phone + u": "_s + message; };
    REQUIRE(pos.loginWithPin(u"1234"_s));

    CHECK(pos.addToWaitlist({{u"size"_s, 2}}) == 0);              // a name, please
    const qint64 dana = pos.addToWaitlist({{u"name"_s, u"Dana"_s}, {u"phone"_s, u"555-0101"_s}, {u"size"_s, 4}});
    const qint64 alex = pos.addToWaitlist({{u"name"_s, u"Alex"_s}, {u"size"_s, 2}});
    const qint64 sam = pos.addToWaitlist({{u"name"_s, u"Sam"_s}, {u"size"_s, 3}, {u"quote"_s, 45}});
    REQUIRE(dana > 0);
    QVariantList line = waiting(pos);
    REQUIRE(line.size() == 3);
    CHECK(line[0].toMap()[u"quoted"_s] == 10);                    // 10 minutes a party ahead
    CHECK(line[1].toMap()[u"quoted"_s] == 20);
    CHECK(line[2].toMap()[u"quoted"_s] == 45);                    // the host's own quote
    CHECK(pos.waitlistInfo()[u"nextQuote"_s].toInt() == 40);

    clock += 25 * 60'000;
    line = waiting(pos);
    CHECK(line[0].toMap()[u"waited"_s] == 25);
    CHECK(line[0].toMap()[u"late"_s].toBool());                   // told 10, waited 25

    REQUIRE(pos.notifyParty(dana));
    REQUIRE(texts.size() == 1);
    CHECK(texts[0].startsWith(u"555-0101: Hi Dana, your table at"_s));
    CHECK(waiting(pos)[0].toMap()[u"status"_s] == u"notified"_s);
    REQUIRE(pos.notifyParty(alex));                               // no phone: no text
    CHECK(texts.size() == 1);

    // Seat Dana at T5 for Sam (the server): the table's check is Sam's.
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));             // T1 is busy
    pos.releaseCheck();
    CHECK_FALSE(pos.seatParty(dana, u"T1"_s));
    CHECK_FALSE(pos.seatParty(dana, u""_s));
    REQUIRE(pos.seatParty(dana, u"T5"_s, idOf(pos, u"Sam"_s)));
    CHECK_FALSE(pos.hasCheck());                                  // the host doesn't keep it
    const QVariantMap t5 = pos.tableStatus(u"T5"_s);
    CHECK(t5[u"open"_s].toBool());
    const core::Check *check = nullptr;
    for (const auto &[id, c] : pos.shared()->open)
        if (c.label == "T5") check = &c;
    REQUIRE(check);
    CHECK(check->serverName == "Sam");
    CHECK(check->guests == 4);
    CHECK(check->customer.name == "Dana");
    CHECK_FALSE(pos.seatParty(dana, u"T6"_s));                    // already seated

    REQUIRE(pos.partyGone(sam));
    CHECK(waiting(pos).size() == 1);
    CHECK(pos.waitlistInfo()[u"seatedToday"_s].toInt() == 1);
    CHECK(pos.waitlistInfo()[u"averageWait"_s].toInt() == 25);
}

TEST_CASE("Reservations: booked, checked in, seated; no-shows", "[waitlist][reservations]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(17);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1111"_s));

    CHECK(pos.addReservation({{u"name"_s, u"Lee"_s}, {u"size"_s, 6}}) == 0);           // when?
    CHECK(pos.addReservation({{u"name"_s, u"Lee"_s}, {u"at"_s, todayAt(9)}}) == 0);   // in the past
    const QString tomorrow = QDate::currentDate().addDays(1).toString(u"yyyy-MM-dd"_s) + u" 19:30"_s;
    const qint64 later = pos.addReservation({{u"name"_s, u"Kim"_s}, {u"size"_s, 2}, {u"at"_s, tomorrow}});
    const qint64 lee = pos.addReservation({{u"name"_s, u"Lee"_s}, {u"size"_s, 6}, {u"at"_s, todayAt(19)},
                                           {u"note"_s, u"birthday"_s}});
    const qint64 park = pos.addReservation({{u"name"_s, u"Park"_s}, {u"size"_s, 2}, {u"at"_s, todayAt(18)}});
    REQUIRE(later > 0);
    QVariantList list = booked(pos);
    REQUIRE(list.size() == 3);
    CHECK(list[0].toMap()[u"name"_s] == u"Park"_s);               // soonest first
    CHECK(list[2].toMap()[u"time"_s].toString().startsWith(u"Tomorrow"_s));
    CHECK(waiting(pos).isEmpty());                                // bookings aren't in the line

    clock = todayAt(18, 20);
    CHECK(booked(pos)[0].toMap()[u"overdue"_s].toBool());         // Park is 20 minutes late
    REQUIRE(pos.partyGone(park, true));
    CHECK(pos.waitlistInfo()[u"noShows"_s].toInt() == 1);

    clock = todayAt(18, 55);
    REQUIRE(pos.checkInParty(lee));
    CHECK_FALSE(pos.checkInParty(lee));
    REQUIRE(waiting(pos).size() == 1);
    CHECK(waiting(pos)[0].toMap()[u"reservation"_s].toBool());
    clock = todayAt(19, 5);
    REQUIRE(pos.seatParty(lee, u"T5"_s));
    CHECK(booked(pos).size() == 1);
    CHECK(pos.waitlistInfo()[u"averageWait"_s].toInt() == 10);

    const core::Party &p = pos.shared()->parties[1];
    CHECK(app::partyFromJson(app::toJson(p)) == p);
}

TEST_CASE("The waitlist is saved and comes back", "[waitlist][store]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1111"_s));
        REQUIRE(pos.addToWaitlist({{u"name"_s, u"Dana"_s}, {u"size"_s, 4}}) > 0);
        const QString tomorrow = QDate::currentDate().addDays(1).toString(u"yyyy-MM-dd"_s) + u" 19:30"_s;
        REQUIRE(pos.addReservation({{u"name"_s, u"Kim"_s}, {u"at"_s, tomorrow}}) > 0);
        const qint64 gone = pos.addToWaitlist({{u"name"_s, u"Alex"_s}});
        REQUIRE(pos.partyGone(gone));
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->parties.size() == 3);
    CHECK(data->lastPartyId == 3);
    PosService pos(*data, nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(waiting(pos).size() == 1);
    CHECK(booked(pos).size() == 1);
}

TEST_CASE("Host stand: seat at tables pushed together, hold tables, bussing", "[waitlist][host]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(17);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto state = [&](const QString &t) { return pos.floor().value(t).toMap().value(u"state"_s).toString(); };

    // A party of 8 at 7:00: two tables held for them.
    const qint64 lee = pos.addReservation({{u"name"_s, u"Lee"_s}, {u"size"_s, 8}, {u"at"_s, todayAt(19)}});
    REQUIRE(lee > 0);
    REQUIRE(pos.reserveTables(lee, {u"T5"_s, u"T6"_s}));
    CHECK(state(u"T5"_s) == u"reserved"_s);
    CHECK(pos.floor().value(u"T6"_s).toMap().value(u"party"_s) == u"Lee"_s);
    CHECK(pos.tableStatus(u"T6"_s).value(u"floor"_s) == u"reserved"_s);
    CHECK(state(u"T1"_s).isEmpty());                                  // available

    // Someone else can't have them.
    const qint64 kim = pos.addToWaitlist({{u"name"_s, u"Kim"_s}, {u"size"_s, 2}});
    CHECK_FALSE(pos.seatPartyAt(kim, {u"T5"_s}, {}));
    CHECK_FALSE(pos.reserveTables(kim, {u"T6"_s}));
    REQUIRE(pos.seatPartyAt(kim, {u"T1"_s}, {}));
    CHECK(state(u"T1"_s) == u"seated"_s);
    CHECK(pos.floor().value(u"T1"_s).toMap().value(u"guests"_s) == 2);

    // Lee arrives: seated at both, one check on T5, T6 with it.
    REQUIRE(pos.seatPartyAt(lee, {u"T5"_s, u"T6"_s}, {}));
    CHECK(state(u"T5"_s) == u"seated"_s);
    CHECK(state(u"T6"_s) == u"seated"_s);
    CHECK(pos.floor().value(u"T6"_s).toMap().value(u"with"_s) == u"T5"_s);
    CHECK(pos.tableStatus(u"T6"_s).value(u"floor"_s) == u"joined"_s);
    CHECK(booked(pos).isEmpty());

    // Walk-ins, no name.
    REQUIRE(pos.seatWalkIn(3, {u"T2"_s}, {}));
    CHECK(state(u"T2"_s) == u"seated"_s);
    CHECK_FALSE(pos.seatWalkIn(2, {u"T2"_s}, {}));                     // taken

    // Lee's check closes: both tables need bussing.
    qint64 leeCheck = 0;
    for (const QVariant &v : pos.openChecks())
        if (v.toMap().value(u"label"_s) == u"T5"_s)
            leeCheck = v.toMap().value(u"id"_s).toLongLong();
    REQUIRE(pos.openCheck(leeCheck));
    REQUIRE(pos.closeCheck());
    CHECK(state(u"T5"_s) == u"dirty"_s);
    CHECK(state(u"T6"_s) == u"dirty"_s);
    CHECK(pos.tableStatus(u"T5"_s).value(u"floor"_s) == u"dirty"_s);
    CHECK_FALSE(pos.setTableState(u"T1"_s, u"clean"_s));               // has guests
    REQUIRE(pos.setTableState(u"T5"_s, u"clean"_s));
    CHECK(state(u"T5"_s).isEmpty());
    REQUIRE(pos.setTableState(u"T3"_s, u"dirty"_s));
    CHECK(state(u"T3"_s) == u"dirty"_s);

    // A hold lets go when the party leaves.
    const qint64 ana = pos.addReservation({{u"name"_s, u"Ana"_s}, {u"size"_s, 2}, {u"at"_s, todayAt(20)}});
    REQUIRE(pos.reserveTables(ana, {u"T4"_s}));
    REQUIRE(pos.partyGone(ana, true));
    CHECK(state(u"T4"_s).isEmpty());
}

TEST_CASE("The floor: when tables free up, nudges for their servers, and who's next up", "[waitlist][floorhints]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = QDateTime(QDate::currentDate(), QTime(18, 0)).toMSecsSinceEpoch();
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.clockInEmployee(u"sam"_s));
    REQUIRE(pos.clockInEmployee(u"rosa"_s));
    const auto floorOf = [&](const QString &t) { return pos.floor().value(t).toMap(); };
    const auto nextUp = [&] { return pos.floor().value(u"_next"_s).toMap().value(u"id"_s).toString(); };

    // Sam gets four at T1: Rosa is next up (no guests yet).
    REQUIRE(pos.seatWalkIn(4, {u"T1"_s}, u"sam"_s));
    CHECK(nextUp() == u"rosa"_s);
    REQUIRE(pos.seatWalkIn(2, {u"T2"_s}, u"rosa"_s));
    CHECK(nextUp() == u"rosa"_s);                          // 2 guests against Sam's 4

    // Ten minutes and nothing ordered: a nudge, and a while yet.
    clock += 10 * 60'000;
    QVariantMap t1 = floorOf(u"T1"_s);
    CHECK(t1[u"stage"_s] == u"ordering"_s);
    CHECK(t1[u"nudge"_s].toString().startsWith(u"Nothing ordered"_s));
    CHECK(t1[u"freeIn"_s].toInt() >= 30);
    pos.logout();

    // Sam orders: no nudge; then half an hour on, one.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.selectTable(u"T1"_s);
    REQUIRE(pos.hasCheck());
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    const qint64 check = pos.checkInfo().value(u"id"_s).toLongLong();
    t1 = floorOf(u"T1"_s);
    CHECK(t1[u"stage"_s] == u"eating"_s);
    CHECK(t1[u"nudge"_s].toString().isEmpty());
    clock += 30 * 60'000;
    CHECK(floorOf(u"T1"_s)[u"nudge"_s].toString() == u"Last order 30m ago"_s);
    CHECK(pos.tableStatus(u"T1"_s)[u"nudge"_s].toString() == u"Last order 30m ago"_s);   // on their table button

    // The check printed: nearly free, and nothing to nudge about.
    pos.notePrinted(check);
    t1 = floorOf(u"T1"_s);
    CHECK(t1[u"stage"_s] == u"check"_s);
    CHECK(t1[u"freeIn"_s].toInt() >= 3);
    CHECK(t1[u"freeIn"_s].toInt() <= 8);
    CHECK(t1[u"nudge"_s].toString().isEmpty());
}

TEST_CASE("A regular is recognized by their phone: on the waitlist, and for their server", "[waitlist][regulars]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana Lee"_s}, {u"phone"_s, u"(555) 010-2030"_s},
                              {u"note"_s, u"Allergic to nuts"_s}}));
    core::CustomerRecord &dana = pos.shared()->customers.back();
    dana.visits = 7;

    // They join the line with their number written another way.
    const qint64 id = pos.addToWaitlist({{u"name"_s, u"Dana"_s}, {u"phone"_s, u"555-010-2030"_s}, {u"size"_s, 2}});
    REQUIRE(id > 0);
    QVariantMap row;
    for (const QVariant &v : pos.waitlistInfo()[u"waiting"_s].toList())
        if (v.toMap()[u"id"_s].toLongLong() == id)
            row = v.toMap();
    CHECK(row[u"regular"_s] == u"Regular · 7 visits  ·  Allergic to nuts"_s);

    // Seated: their server sees it on the check.
    REQUIRE(pos.seatParty(id, u"T3"_s, u"sam"_s));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.selectTable(u"T3"_s);
    REQUIRE(pos.hasCheck());
    CHECK(pos.checkInfo()[u"regular"_s] == u"Regular · 7 visits  ·  Allergic to nuts"_s);

    // Someone new: nothing.
    const qint64 other = pos.addToWaitlist({{u"name"_s, u"Pat"_s}, {u"phone"_s, u"555-999-0000"_s}});
    for (const QVariant &v : pos.waitlistInfo()[u"waiting"_s].toList())
        if (v.toMap()[u"id"_s].toLongLong() == other)
            CHECK(v.toMap()[u"regular"_s].toString().isEmpty());
}
