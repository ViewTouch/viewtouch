#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

namespace {

struct Pos {
    test::RecordingSink sink;
    PosService pos{test::seedPosData(), &sink};
    std::int64_t clock = 1'700'000'000'000;

    Pos() { pos.setClock([this] { return clock; }); }

    void typePin(const char *pin)
    {
        for (const char *p = pin; *p; ++p)
            pos.pinKey(QString(QChar(*p)));
    }
    void login(const char *pin)
    {
        typePin(pin);
        REQUIRE(pos.login());
    }
    std::int64_t balance() const { return pos.totals().value(u"balanceCents"_s).toLongLong(); }
};

} // namespace

TEST_CASE("PIN login: stored hashed, wrong PIN refused", "[pos]")
{
    Pos t;
    for (const core::Employee &e : test::seedPosData().employees) {
        CHECK(e.pinHash.size() == 64);          // SHA-256 hex
        // Salted: not the bare PIN's hash. (Its hex can hold "1234" by chance.)
        CHECK_FALSE(e.pinSalt.empty());
        CHECK(e.pinHash != app::hashPin(u"1234"_s, {}));
    }

    QSignalSpy notices(&t.pos, &PosService::notice);
    t.typePin("9999");
    CHECK(t.pos.pinLength() == 4);
    CHECK_FALSE(t.pos.login());
    CHECK(t.pos.pinLength() == 0);   // cleared either way
    CHECK_FALSE(t.pos.loggedIn());
    CHECK(notices.last()[0].toString().contains(u"not recognized"_s));

    t.login("1111");
    CHECK(t.pos.userName() == u"Sam"_s);
    CHECK(t.pos.can(u"order"_s));
    CHECK_FALSE(t.pos.can(u"layout.edit"_s));

    t.pos.logout();
    CHECK_FALSE(t.pos.loggedIn());
}

TEST_CASE("Nothing can be ordered before logging in", "[pos]")
{
    Pos t;
    CHECK_FALSE(t.pos.addItem(u"classic-burger"_s));
    CHECK_FALSE(t.pos.hasCheck());
    CHECK(t.sink.checks.empty());
}

TEST_CASE("Clock in and out with a PIN", "[pos]")
{
    Pos t;
    t.typePin("1111");
    REQUIRE(t.pos.clockIn());
    REQUIRE(t.sink.punches.size() == 1);
    CHECK(t.sink.punches.begin()->second.open());

    t.typePin("1111");
    CHECK_FALSE(t.pos.clockIn());   // already in

    t.clock += 4 * 3'600'000;
    t.login("1111");
    CHECK(t.pos.clockedIn());
    REQUIRE(t.pos.clockOut());
    CHECK(t.sink.punches.begin()->second.clockOut == t.clock);
    CHECK_FALSE(t.pos.clockedIn());
}

TEST_CASE("Table flow: new table asks for guests, then reopens", "[pos]")
{
    Pos t;
    t.login("1111");
    CHECK(t.pos.selectTable(u"T4"_s) == PosService::TableNeedsGuests);
    CHECK(t.pos.pendingTable() == u"T4"_s);
    t.pos.entryKey(u"3"_s);
    CHECK(t.pos.entryGuests() == 3);
    t.pos.adjustGuests(+1);
    REQUIRE(t.pos.startCheck(core::CheckType::DineIn));
    CHECK(t.pos.checkInfo().value(u"label"_s).toString() == u"T4"_s);
    CHECK(t.pos.checkInfo().value(u"guests"_s).toInt() == 4);
    CHECK(t.pos.checkInfo().value(u"server"_s).toString() == u"Sam"_s);
    CHECK(t.pos.tableStatus(u"T4"_s).value(u"open"_s).toBool());
    CHECK_FALSE(t.pos.tableStatus(u"T5"_s).value(u"open"_s).toBool());

    const qint64 id = t.pos.checkInfo().value(u"id"_s).toLongLong();
    t.pos.releaseCheck();
    CHECK_FALSE(t.pos.hasCheck());
    CHECK(t.pos.selectTable(u"T4"_s) == PosService::TableOpened);
    CHECK(t.pos.checkInfo().value(u"id"_s).toLongLong() == id);
}

TEST_CASE("Ordering: modifiers attach, qualifiers apply once", "[pos]")
{
    Pos t;
    t.login("1111");
    REQUIRE(t.pos.addItem(u"classic-burger"_s));    // starts a quick check
    CHECK(t.pos.checkInfo().value(u"type"_s).toString() == u"quick"_s);
    REQUIRE(t.pos.addItem(u"Medium Rare"_s));       // by name, case-insensitive lookup
    t.pos.setQualifier(u"no"_s);
    CHECK(t.pos.pendingQualifier() == u"no"_s);
    REQUIRE(t.pos.addItem(u"onion-rings"_s));
    CHECK(t.pos.pendingQualifier().isEmpty());      // used up
    REQUIRE(t.pos.addItem(u"draft-beer"_s));

    const QVariantList lines = t.pos.lines();
    REQUIRE(lines.size() == 2);
    const QVariantMap burger = lines[0].toMap();
    CHECK(burger[u"name"_s].toString() == u"Classic Burger"_s);
    CHECK(burger[u"price"_s].toString() == u"$11.50"_s);   // "No Onion Rings" is free
    const QVariantList mods = burger[u"modifiers"_s].toList();
    REQUIRE(mods.size() == 2);
    CHECK(mods[1].toMap()[u"name"_s].toString() == u"No Onion Rings"_s);

    // Food 11.50 @ 8.25% = 0.95 (0.94875); beer 6.00 @ 10% = 0.60
    CHECK(t.pos.totals()[u"tax"_s].toString() == u"$1.55"_s);
    CHECK(t.pos.totals()[u"total"_s].toString() == u"$19.05"_s);

    // A modifier with no item to attach to is refused.
    t.pos.releaseCheck();
    t.pos.startCheck(core::CheckType::Takeout);
    CHECK_FALSE(t.pos.addItem(u"fries"_s));
    CHECK_FALSE(t.pos.addItem(u"not-on-menu"_s));
}

TEST_CASE("Void: unsent removed by anyone, sent needs a manager", "[pos]")
{
    Pos t;
    t.login("1111");
    t.pos.addItem(u"coffee"_s);
    t.pos.addItem(u"tea"_s);
    REQUIRE(t.pos.voidItem());                      // removes Tea (newest, unsent)
    CHECK(t.pos.lines().size() == 1);

    REQUIRE(t.pos.sendOrder());
    CHECK_FALSE(t.pos.sendOrder());                 // nothing new
    CHECK_FALSE(t.pos.voidItem());                  // server may not void sent items
    CHECK(t.pos.lines().size() == 1);

    const qint64 id = t.pos.checkInfo().value(u"id"_s).toLongLong();
    t.pos.logout();
    t.login("1234");
    REQUIRE(t.pos.openCheck(id));
    REQUIRE(t.pos.voidItem());
    CHECK(t.pos.lines()[0].toMap()[u"voided"_s].toBool());
    CHECK(t.pos.totals()[u"total"_s].toString() == u"$0.00"_s);
}

TEST_CASE("Payments: exact card, cash change, discount, undo, close", "[pos]")
{
    Pos t;
    t.login("2222");                                 // cashier
    t.pos.addItem(u"cobb"_s);                        // 12.50 + 1.03 tax = 13.53
    REQUIRE(t.balance() == 1353);

    // Card is capped at the balance.
    t.pos.entryKey(u"2000"_s);
    REQUIRE(t.pos.tender(u"credit"_s));
    CHECK(t.balance() == 0);
    REQUIRE(t.pos.removePayment());
    CHECK(t.balance() == 1353);

    // 10% off: 12.50 - 1.25 = 11.25, tax 0.93 -> 12.18. A cashier needs a manager for it.
    CHECK_FALSE(t.pos.tender(u"discount"_s));
    CHECK(t.balance() == 1353);
    t.pos.invoke(u"tender"_s, {u"discount"_s});
    REQUIRE(t.pos.giveReason(u"Guest unhappy"_s));
    REQUIRE(t.pos.approve(u"1234"_s));
    CHECK(t.balance() == 1218);

    // Cash over-tender gives change.
    t.pos.entryKey(u"20"_s);
    t.pos.entryKey(u"00"_s);
    CHECK(t.pos.entryAmount() == u"$20.00"_s);
    REQUIRE(t.pos.tender(u"cash"_s));
    CHECK(t.pos.totals()[u"change"_s].toString() == u"$7.82"_s);
    CHECK(t.pos.entry().isEmpty());

    QSignalSpy closed(&t.pos, &PosService::checkClosed);
    REQUIRE(t.pos.openDrawerSession());            // cash needs an open drawer
    REQUIRE(t.pos.closeCheck());
    CHECK(closed.size() == 1);
    CHECK_FALSE(t.pos.hasCheck());
    CHECK(t.pos.openChecks().isEmpty());

    // Saved closed, and everything was sent on close.
    const core::Check &saved = t.sink.checks.begin()->second;
    CHECK(saved.status == core::CheckStatus::Closed);
    CHECK(saved.closedAt == t.clock);
    CHECK(saved.unsentCount() == 0);
}

TEST_CASE("A check with money due cannot be closed", "[pos]")
{
    Pos t;
    t.login("1111");
    t.pos.addItem(u"soda"_s);
    QSignalSpy notices(&t.pos, &PosService::notice);
    CHECK_FALSE(t.pos.closeCheck());
    CHECK(notices.last()[0].toString().contains(u"still due"_s));
    CHECK(t.pos.hasCheck());
}

TEST_CASE("Every change is handed to the sink", "[pos]")
{
    Pos t;
    t.login("1111");
    t.pos.addItem(u"coffee"_s);
    t.pos.addItem(u"juice"_s);
    t.pos.sendOrder();
    CHECK(t.sink.checkSaves == 4);   // start, 2 items, send
    const core::Check &saved = t.sink.checks.begin()->second;
    CHECK(saved.lines.size() == 2);
    CHECK(saved.lines[0].sent);
}

TEST_CASE("Open checks survive a restart", "[pos]")
{
    Pos first;
    first.login("1111");
    first.pos.selectTable(u"T1"_s);
    first.pos.startCheck(core::CheckType::DineIn);
    first.pos.addItem(u"pancakes"_s);

    app::PosData data = test::seedPosData();
    for (const auto &[id, c] : first.sink.checks)
        data.openChecks.push_back(c);
    PosService second(data, nullptr);
    CHECK(second.openChecks().size() == 1);
    REQUIRE(second.loginWithPin(u"1111"_s));
    CHECK(second.selectTable(u"T1"_s) == PosService::TableOpened);
    CHECK(second.lines().size() == 1);

    // New checks continue the serial numbers.
    second.startCheck(core::CheckType::Takeout);
    CHECK(second.checkInfo().value(u"id"_s).toLongLong() == 2);
}

TEST_CASE("Comments come from the keyboard entry", "[pos]")
{
    Pos t;
    t.login("1111");
    t.pos.addItem(u"greek"_s);
    CHECK_FALSE(t.pos.addComment());
    for (const QString &k : {u"N"_s, u"o"_s, u"space"_s, u"f"_s, u"e"_s, u"t"_s, u"a"_s})
        t.pos.textKey(k);
    CHECK(t.pos.textEntry() == u"No feta"_s);
    REQUIRE(t.pos.addComment());
    CHECK(t.pos.lines().last().toMap()[u"comment"_s].toBool());
    CHECK(t.pos.textEntry().isEmpty());
}
