#include <catch2/catch_test_macros.hpp>

#include "core/report.hh"
#include "core/settings.hh"
#include "pos_fixture.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using core::HoursSplit;
using core::OvertimeRules;

namespace {
constexpr std::int64_t H = 3'600'000;
std::vector<std::int64_t> days(std::initializer_list<double> hours)
{
    std::vector<std::int64_t> out;
    for (double h : hours)
        out.push_back(std::int64_t(h * double(H)));
    return out;
}
void check(const HoursSplit &s, double regular, double overtime, double doubleTime)
{
    CHECK(s.regular == std::int64_t(regular * double(H)));
    CHECK(s.overtime == std::int64_t(overtime * double(H)));
    CHECK(s.doubleTime == std::int64_t(doubleTime * double(H)));
}
} // namespace

TEST_CASE("Overtime by the state's law: California, federal, Colorado", "[labor][law]")
{
    const OvertimeRules california{8, 12, 40, true};
    // One 13-hour day: 8 regular, 4 at time and a half, 1 double.
    check(core::splitHours(days({13}), california), 8, 4, 1);
    // Five 9-hour days: 1 hour of daily overtime each; 40 regular (no more weekly).
    check(core::splitHours(days({9, 9, 9, 9, 9}), california), 40, 5, 0);
    // Six 8-hour days: the sixth is past 40 hours in the week.
    check(core::splitHours(days({8, 8, 8, 8, 8, 8}), california), 40, 8, 0);
    // Seven 9-hour days: the seventh day in a row is 8 at time and a half, 1 double.
    check(core::splitHours(days({9, 9, 9, 9, 9, 9, 9}), california), 40, 22, 1);
    // A day off in between: no seventh-day rule.
    check(core::splitHours(days({9, 9, 9, 0, 9, 9, 9}), california), 40, 14, 0);

    // Federal only: past 40 in the week.
    const OvertimeRules federal{0, 0, 40, false};
    check(core::splitHours(days({10, 10, 10, 10, 10}), federal), 40, 10, 0);
    check(core::splitHours(days({13}), federal), 13, 0, 0);
    // Colorado: past 12 in a day.
    check(core::splitHours(days({13}), OvertimeRules{12, 0, 40, false}), 12, 1, 0);
}

TEST_CASE("Picking the store's state sets its rules; each can still be changed", "[labor][law]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"laborState"_s] = u"CA"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    const core::PosSettings &s = pos.shared()->settings;
    CHECK(s.laborState == "CA");
    CHECK(s.overtimeDailyHours == 8);
    CHECK(s.doubleTimeDailyHours == 12);
    CHECK(s.seventhDayRule);
    CHECK(s.overtimeWeeklyHours == 40);
    CHECK(s.mealBreakAfterHours == 5);
    CHECK(s.giftCardCashBelowCents == 1000);
    // Changed by hand afterwards (the state stays).
    store = pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"mealBreakAfterHours"_s] = 6;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.mealBreakAfterHours == 6);
    CHECK(pos.shared()->settings.overtimeDailyHours == 8);
    // Texas: federal only.
    store = pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"laborState"_s] = u"TX"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.overtimeDailyHours == 0);
    CHECK_FALSE(pos.shared()->settings.seventhDayRule);
    CHECK(pos.shared()->settings.giftCardCashBelowCents == 0);
    CHECK(core::stateRules().size() == 51);   // 50 states and DC
}

TEST_CASE("Meal breaks the state requires: a reminder, Store Check, the Labor report", "[labor][law][meals]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = QDateTime(QDate::currentDate(), QTime(10, 0)).toMSecsSinceEpoch();
    pos.shared()->setClock([&] { return clock; });
    core::applyStateRules(pos.shared()->settings, "CA");
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.clockInEmployee(u"sam"_s));
    REQUIRE(pos.clockInEmployee(u"rosa"_s));
    pos.logout();
    const auto checkItem = [&] {
        pos.logout();
        pos.loginWithPin(u"1234"_s);
        for (const QVariant &v : pos.storeCheck()[u"checks"_s].toList())
            if (v.toMap()[u"id"_s] == u"meals"_s)
                return v.toMap()[u"detail"_s].toString();
        return QString();
    };
    QStringList said;
    QObject::connect(&pos, &app::PosService::notice, [&](const QString &n) { said << n; });

    // 4 hours 40 in: Sam is reminded when he logs in.
    clock += (4 * 60 + 40) * 60'000;
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(said.join(u'|').contains(u"Your meal break is due by 3:00"_s));
    // Rosa takes a 30-minute meal at 4 hours 45.
    pos.logout();
    clock += 5 * 60'000;
    REQUIRE(pos.loginWithPin(u"5555"_s));
    REQUIRE(pos.toggleBreak());   // on break
    clock += 31 * 60'000;
    REQUIRE(pos.toggleBreak());     // back
    pos.logout();
    // Past 5 hours: Sam is late, Rosa isn't.
    const QString late = checkItem();
    CHECK(late.contains(u"Sam"_s));
    CHECK_FALSE(late.contains(u"Rosa"_s));
    bool section = false, samLine = false, rosaLine = false;
    for (const QVariant &v : pos.report(u"labor"_s)[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (v.toMap()[u"kind"_s] == u"section"_s)
            section = cells.value(0) == u"Meal breaks late or missed"_s;
        samLine = samLine || (section && cells.value(0) == u"Sam"_s && cells.value(2) == u"none"_s);
        rosaLine = rosaLine || (section && cells.value(0).startsWith(u"Rosa"_s));
    }
    CHECK(samLine);
    CHECK_FALSE(rosaLine);
}

TEST_CASE("Gift cards: a small balance cashed out where state law says so", "[law][giftcards]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    for (const auto &[number, cents] : {std::pair{"600000000007", 700}, std::pair{"600000000015", 1500}}) {
        core::GiftCard g;
        g.number = number;
        g.post(1, Money::fromCents(cents), "Sold", 1, "sale");
        pos.shared()->giftCards.push_back(g);
    }
    // No state chosen: not offered.
    REQUIRE(pos.lookupGiftCard(u"600000000007"_s));
    CHECK_FALSE(pos.giftCardInfo()[u"cashOut"_s].toBool());
    CHECK_FALSE(pos.cashOutGiftCard());

    core::applyStateRules(pos.shared()->settings, "CA");   // under $10
    REQUIRE(pos.lookupGiftCard(u"600000000007"_s));
    CHECK(pos.giftCardInfo()[u"cashOut"_s].toBool());
    REQUIRE(pos.cashOutGiftCard());
    CHECK(pos.giftCardInfo()[u"balanceCents"_s].toLongLong() == 0);
    const core::CashMovement &out = pos.shared()->drawers.back().movements.back();   // out of the drawer, counted
    CHECK(out.kind == core::CashMovement::Kind::Payout);
    CHECK(out.amount.cents() == 700);
    CHECK_FALSE(pos.cashOutGiftCard());                       // empty now
    // $15 is over California's limit.
    REQUIRE(pos.lookupGiftCard(u"600000000015"_s));
    CHECK_FALSE(pos.giftCardInfo()[u"cashOut"_s].toBool());
    CHECK_FALSE(pos.cashOutGiftCard());
}

TEST_CASE("Labor law: Store Check warns about pay under the state's minimum wage", "[labor][minwage]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto warning = [&] {
        for (const QVariant &v : pos.storeCheck()[u"checks"_s].toList())
            if (v.toMap()[u"id"_s] == u"minimumWage"_s)
                return v.toMap()[u"detail"_s].toString();
        return QString();
    };
    core::Employee ana;
    ana.id = "ana";
    ana.name = "Ana";
    ana.role = "server";
    ana.payRate = Money::fromCents(213);   // the federal tipped cash wage
    core::Employee cook = ana;
    cook.id = "cook";
    cook.name = "Cook";
    cook.role = "cook";
    cook.payRate = Money::fromCents(1500);
    pos.shared()->employees.push_back(ana);
    pos.shared()->employees.push_back(cook);

    core::applyStateRules(pos.shared()->settings, "TX");   // tip credit: $2.13 is allowed
    CHECK(warning().isEmpty());
    core::applyStateRules(pos.shared()->settings, "CA");   // no tip credit: $16.90 for everyone
    const QString w = warning();
    CHECK(w.contains(u"California"_s));
    CHECK(w.contains(u"Ana"_s));
    CHECK(w.contains(u"$16.90"_s));
    CHECK(w.contains(u"Cook"_s));                            // $15.00 is under $16.90 too
    CHECK(core::minimumWageFor("").cents == 725);           // no state: federal
}

TEST_CASE("Labor law: a paid rest break every 4 hours where the state requires one", "[labor][rest]")
{
    core::PosSettings s;
    core::applyStateRules(s, "CA");
    CHECK(s.restBreakEveryHours == 4);
    core::applyStateRules(s, "TX");
    CHECK(s.restBreakEveryHours == 0);

    core::TimePunch p;
    p.clockIn = 0;
    CHECK(p.restDueAt(4) == 4 * 3'600'000);
    p.breaks.push_back({3 * 3'600'000, 3 * 3'600'000 + 10 * 60'000});   // a 10-minute break at 3 h
    CHECK(p.restDueAt(4) == 7 * 3'600'000 + 10 * 60'000);
    // Short breaks are paid; a 30-minute meal isn't.
    p.breaks.push_back({5 * 3'600'000, 5 * 3'600'000 + 30 * 60'000});
    p.clockOut = 8 * 3'600'000;
    CHECK(p.workedMs(p.clockOut, false) == 7 * 3'600'000 + 30 * 60'000);
}
