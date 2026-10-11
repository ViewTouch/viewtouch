#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Loyalty points and rewards; promotions that apply themselves.

namespace {

// The next `weekday` (1 Monday ... 7 Sunday) at hour:minute.
qint64 nextDay(int weekday, int hour, int minute = 0)
{
    QDate d = QDate::currentDate();
    while (d.dayOfWeek() != weekday)
        d = d.addDays(1);
    return QDateTime(d, QTime(hour, minute)).toMSecsSinceEpoch();
}

app::PosData withPromotions()
{
    app::PosData d = test::seedPosData();
    d.settings.promotions = app::settingsFromJson(test::readSeed("pos/settings.json").object()).promotions;
    return d;
}

QString promo(const PosService &pos)
{
    for (const QVariant &v : pos.payments()) {
        const QVariantMap p = v.toMap();
        if (p[u"name"_s].toString() == u"Happy Hour"_s || p[u"name"_s].toString() == u"Burger Tuesday"_s)
            return p[u"name"_s].toString() + u" "_s + p[u"amount"_s].toString();
    }
    return {};
}

} // namespace

TEST_CASE("Loyalty: points for what's spent, rewards off the check, taken back on reopen", "[loyalty]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.shared()->settings.loyaltyEnabled);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());

    // Signed up from the customer display with a phone number.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 4; ++i)
        pos.addItem(u"cobb"_s);                                // $50.00
    CHECK_FALSE(pos.customerJoin(u"555"_s));
    REQUIRE(pos.customerJoin(u"(555) 404-1234"_s));
    QVariantMap loyalty = pos.customerPrompt()[u"loyalty"_s].toMap();
    CHECK(loyalty[u"earning"_s] == 50);
    CHECK(loyalty[u"points"_s] == 0);
    CHECK_FALSE(pos.redeemReward(0));                          // not enough points yet
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    const qint64 first = pos.shared()->closedToday.back().id;

    // Back again: the same number finds them; 50 points buys $5 off.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);                                    // $12.50
    REQUIRE(pos.customerJoin(u"555-404-1234"_s));
    loyalty = pos.customerPrompt()[u"loyalty"_s].toMap();
    CHECK(loyalty[u"points"_s] == 50);
    CHECK(loyalty[u"rewards"_s].toList()[0].toMap()[u"ready"_s].toBool());
    REQUIRE(pos.redeemReward(0));
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$7.50"_s);
    CHECK(pos.shared()->customers.back().points == 0);
    REQUIRE(pos.removePayment());                              // the points go back
    CHECK(pos.shared()->customers.back().points == 50);
    REQUIRE(pos.redeemReward(0));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.shared()->customers.back().points == 7);         // $7.50 after the reward

    // Reopening the first check takes its 50 back (not below zero).
    REQUIRE(pos.reopenCheck(first));
    CHECK(pos.shared()->customers.back().points == 0);
    CHECK(pos.shared()->customers.back().lifetimePoints == 7);

    const core::CustomerRecord &c = pos.shared()->customers.back();
    CHECK(app::customerFromJson(app::toJson(c)) == c);
}

TEST_CASE("Promotions: happy hour by the clock, buy one get one, by day", "[loyalty][promotions]")
{
    PosService pos(withPromotions(), nullptr);
    qint64 clock = nextDay(3, 16);                              // a Wednesday, 4 PM: happy hour
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"draft-beer"_s);                              // $6.00
    pos.addItem(u"cobb"_s);
    CHECK(promo(pos) == u"Happy Hour -$3.00"_s);
    pos.addItem(u"draft-beer"_s);
    CHECK(promo(pos) == u"Happy Hour -$6.00"_s);
    clock = nextDay(3, 19);                                     // over at 6
    pos.addItem(u"water"_s);
    CHECK(promo(pos).isEmpty());
    pos.releaseCheck();

    // Tuesday: the second burger half off (the cheaper one).
    clock = nextDay(2, 12);
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"bacon-burger"_s);                            // $13.50
    CHECK(promo(pos).isEmpty());
    pos.addItem(u"classic-burger"_s);                          // $11.50
    CHECK(promo(pos) == u"Burger Tuesday -$5.75"_s);
    pos.addItem(u"veggie-burger"_s);                           // $12.00: pairs go dearest first,
    CHECK(promo(pos) == u"Burger Tuesday -$6.00"_s);           // so $13.50 + $12.00 (half off), $11.50 alone
    REQUIRE(pos.voidItem());                                   // the veggie burger comes off
    CHECK(promo(pos) == u"Burger Tuesday -$5.75"_s);
}

TEST_CASE("Promotions and rewards in Manager screens", "[loyalty][admin]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap p = pos.adminNewRecord(u"promotions"_s);
    p[u"name"_s] = u"Kids Eat Half"_s;
    p[u"items"_s] = u"kids-burger"_s;
    p[u"percent"_s] = 50.0;
    p[u"days"_s] = u"Sat, Sun"_s;
    REQUIRE(pos.adminSave(u"promotions"_s, -1, p));
    const auto &promo = pos.shared()->settings.promotions.back();
    CHECK(promo.id == "kids-eat-half");
    CHECK(promo.days == 0b1000001);
    CHECK(pos.adminRecords(u"promotions"_s).last().toMap()[u"days"_s] == u"Sun, Sat"_s);
    p[u"items"_s] = u"unicorn"_s;
    CHECK_FALSE(pos.adminSave(u"promotions"_s, -1, p));
    p[u"items"_s] = u"kids-burger"_s;
    p[u"days"_s] = u"Funday"_s;
    CHECK_FALSE(pos.adminSave(u"promotions"_s, -1, p));
    p[u"days"_s] = u"Fri-Mon"_s;                                // wraps around
    p[u"buy"_s] = 2;
    p[u"get"_s] = 0;
    CHECK_FALSE(pos.adminSave(u"promotions"_s, -1, p));         // buy 2 get how many?

    QVariantMap store = pos.adminRecords(u"store"_s).first().toMap();
    CHECK(store[u"rewards"_s].toString().startsWith(u"50 = 5.00"_s));
    store[u"rewards"_s] = u"200 = 20\n100 = 8.50"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    REQUIRE(pos.shared()->settings.rewards.size() == 2);
    CHECK(pos.shared()->settings.rewards[0].points == 100);     // sorted
    store[u"rewards"_s] = u"lots = free"_s;
    CHECK_FALSE(pos.adminSave(u"store"_s, 0, store));
}

TEST_CASE("Coupons: a code typed or scanned; dates, a minimum, an amount, a use limit", "[loyalty][coupons]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = nextDay(3, 12);
    pos.shared()->setClock([&] { return clock; });
    const QDate today = QDateTime::fromMSecsSinceEpoch(clock).date();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());

    // $5 off orders of $20 or more, twice in all; this week only.
    QVariantMap p = pos.adminNewRecord(u"promotions"_s);
    p[u"name"_s] = u"Five Off"_s;
    p[u"code"_s] = u"save5"_s;
    p[u"amount"_s] = 5.0;
    p[u"minimum"_s] = 20.0;
    p[u"maxUses"_s] = 2;
    p[u"fromDay"_s] = today.addDays(-1).toString(Qt::ISODate);
    p[u"untilDay"_s] = today.addDays(6).toString(Qt::ISODate);
    REQUIRE(pos.adminSave(u"promotions"_s, -1, p));             // no categories: the whole order
    CHECK(pos.shared()->settings.promotions.back().code == "SAVE5");
    CHECK(pos.adminRecords(u"promotions"_s).last().toMap()[u"code"_s] == u"SAVE5"_s);
    QVariantMap dup = p;
    dup[u"name"_s] = u"Another"_s;
    CHECK_FALSE(pos.adminSave(u"promotions"_s, -1, dup));       // the code is taken
    QVariantMap bad = p;
    bad[u"untilDay"_s] = u"next week"_s;
    CHECK_FALSE(pos.adminSave(u"promotions"_s, int(pos.shared()->settings.promotions.size()) - 1, bad));

    // Not by itself: only once typed, any case, spaces around.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);                                    // $12.50
    CHECK(pos.totals()[u"hasDiscount"_s] == false);
    CHECK_FALSE(pos.applyCoupon(u"NOPE"_s));
    CHECK_FALSE(pos.applyCoupon(u"save5"_s));                  // under $20
    pos.addItem(u"cobb"_s);                                    // $25.00
    REQUIRE(pos.applyCoupon(u"  save5 "_s));
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$20.00"_s);
    CHECK_FALSE(pos.applyCoupon(u"SAVE5"_s));                  // already on it
    QVariantList on = pos.checkInfo()[u"coupons"_s].toList();
    REQUIRE(on.size() == 1);
    CHECK(on[0].toMap()[u"off"_s] == u"$5.00"_s);
    // Below the minimum again: it stays on the check but takes nothing off.
    REQUIRE(pos.voidItem());
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$12.50"_s);
    CHECK(pos.checkInfo()[u"coupons"_s].toList()[0].toMap()[u"working"_s] == false);
    pos.addItem(u"cobb"_s);
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$20.00"_s);
    // Undo Payment on it takes the coupon off (it doesn't come back).
    for (const QVariant &v : pos.payments())
        if (v.toMap()[u"name"_s] == u"Five Off"_s)
            pos.selectPayment(v.toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.removePayment());
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$25.00"_s);
    CHECK(pos.checkInfo()[u"coupons"_s].toList().isEmpty());
    REQUIRE(pos.applyCoupon(u"SAVE5"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    const qint64 first = pos.shared()->closedToday.back().id;
    CHECK(pos.shared()->settings.promotions.back().uses == 1);  // counted when the check closes

    // Second use, then it's used up; reopening one gives a use back.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"cobb"_s);
    REQUIRE(pos.applyCoupon(u"SAVE5"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.shared()->settings.promotions.back().uses == 2);
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"cobb"_s);
    CHECK_FALSE(pos.applyCoupon(u"SAVE5"_s));
    pos.releaseCheck();
    REQUIRE(pos.reopenCheck(first));
    CHECK(pos.shared()->settings.promotions.back().uses == 1);
    REQUIRE(pos.closeCheck());
    CHECK(pos.shared()->settings.promotions.back().uses == 2);

    // Past its last day.
    clock = QDateTime(today.addDays(8), QTime(12, 0)).toMSecsSinceEpoch();
    pos.shared()->settings.promotions.back().uses = 0;
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"cobb"_s);
    CHECK_FALSE(pos.applyCoupon(u"SAVE5"_s));

    // Kept with the check (saved and loaded).
    const core::Check *c = pos.shared()->open.empty() ? nullptr : &pos.shared()->open.begin()->second;
    REQUIRE(c);
    core::Check with = *c;
    with.coupons = {"five-off"};
    CHECK(app::checkFromJson(app::toJson(with))->coupons == with.coupons);
}
