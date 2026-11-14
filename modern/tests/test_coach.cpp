#include <catch2/catch_test_macros.hpp>

#include "app/pos_service.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>
#include <QTest>
#include <QTimeZone>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The profit coach: plain advice from four weeks of the store's own sales.

namespace {

struct Coach {
    test::RecordingSink sink;
    PosService pos{test::seedPosData(), &sink};
    // Thursday Nov 12 2026, 11 PM, here.
    std::int64_t clock = QDateTime(QDate(2026, 11, 12), QTime(23, 0)).toMSecsSinceEpoch();
    std::int64_t nextId = 1000;
    std::vector<core::Check> checks;

    Coach()
    {
        pos.setClock([this] { return clock; });
        REQUIRE(pos.loginWithPin(u"1234"_s));
        // The Classic Burger costs a lot to make (its own patty, $8): it earns little.
        auto &s = *pos.shared();
        core::Ingredient pricey;
        pricey.id = "wagyu";
        pricey.name = "Wagyu patty";
        pricey.cost = Money::fromCents(800);
        s.ingredients.push_back(pricey);
        for (core::MenuItem &m : s.menu) {
            if (m.id == "classic-burger" || m.id == "kids-burger")   // (the kids' too: it earns nothing)
                m.recipe = {{"wagyu", 1}};
        }
    }
    const core::MenuItem &item(const char *id) { return *pos.findItem(QString::fromLatin1(id)); }
    // A check closed `daysAgo` days back at `hour`, with these items.
    core::Check &sale(int daysAgo, int hour, std::initializer_list<std::pair<const char *, int>> lines)
    {
        core::Check c;
        c.id = ++nextId;
        c.label = "T1";
        c.serverName = "Sam";
        c.status = core::CheckStatus::Closed;
        c.closedAt = QDateTime(QDateTime::fromMSecsSinceEpoch(clock).date().addDays(-daysAgo), QTime(hour, 10))
                         .toMSecsSinceEpoch();
        std::int64_t lineId = 0;
        for (const auto &[id, n] : lines) {
            core::OrderLine l;
            l.id = ++lineId;
            l.itemId = id;
            l.name = item(id).name;
            l.unitPrice = item(id).price;
            l.quantity = n;
            c.lines.push_back(l);
        }
        checks.push_back(c);
        return checks.back();
    }
};

QVariantMap tipOf(const QVariantMap &coach, const QString &kind)
{
    for (const QVariant &v : coach.value(u"tips"_s).toList())
        if (v.toMap().value(u"kind"_s) == kind)
            return v.toMap();
    return {};
}

} // namespace

TEST_CASE("Profit coach: too little to go on, then tips from four weeks of sales", "[coach]")
{
    Coach c;
    // A week of sales: too soon.
    for (int d = 1; d <= 7; ++d)
        c.sale(d, 12, {{"classic-burger", 2}});
    const QVariantMap early = c.pos.coachFrom(c.checks);
    CHECK(early.value(u"waiting"_s).toString().contains(u"two weeks"_s));
    CHECK(early.value(u"tips"_s).toList().isEmpty());

    // Four weeks: every day 11 AM to 9 PM, but Tuesdays 2 to 5 PM nobody comes.
    c.checks.clear();
    for (int d = 1; d <= 28; ++d) {
        const QDate day = QDateTime::fromMSecsSinceEpoch(c.clock).date().addDays(-d);
        for (int h = 11; h <= 21; ++h) {
            if (day.dayOfWeek() == 2 && h >= 14 && h < 17)
                continue;
            c.sale(d, h, {{"classic-burger", 1}});   // the Classic sells and sells (but earns little)
        }
        c.sale(d, 13, {{"cheeseburger", 1}, {"mushroom-swiss", 1}, {"veggie-burger", 1}});
        if (d % 9 == 0)
            c.sale(d, 19, {{"bacon-burger", 1}});    // earns well, few order it
        if (d % 14 == 0)
            c.sale(d, 18, {{"kids-burger", 1}});     // few order it, earns little
        // (Nobody orders the Burger of the Day.)
    }
    // A heavy hand with comps: a $25 comp a day (over a tenth of sales).
    for (int d = 1; d <= 28; ++d) {
        core::Check &k = c.sale(d, 20, {{"classic-burger", 2}});
        core::Payment comp;
        comp.id = 1;
        comp.kind = core::TenderKind::Discount;
        comp.tenderName = "Manager Comp";
        comp.amount = Money::fromCents(2500);
        k.payments.push_back(comp);
    }

    const QVariantMap coach = c.pos.coachFrom(c.checks);
    CHECK(coach.value(u"waiting"_s).toString().isEmpty());
    const QVariantList tips = coach.value(u"tips"_s).toList();
    QStringList titles;
    for (const QVariant &v : tips)
        titles << v.toMap().value(u"title"_s).toString();
    CAPTURE(titles);

    // Raise the Classic: $11.50 -> $12.50 ($1 above $10), worth ~0.9 x sold x $1 a month.
    const QVariantMap raise = tipOf(coach, u"raise"_s);
    REQUIRE_FALSE(raise.isEmpty());
    CHECK(raise.value(u"item"_s) == u"classic-burger"_s);
    CHECK(raise.value(u"title"_s) == u"Raise Classic Burger to $12.50"_s);
    CHECK(raise.value(u"price"_s) == u"12.50"_s);
    CHECK(raise.value(u"worth"_s).toLongLong() > 20000);   // over $200 a month
    // The biggest first.
    for (int i = 1; i < tips.size(); ++i)
        CHECK(tips[i - 1].toMap().value(u"worth"_s).toLongLong() >= tips[i].toMap().value(u"worth"_s).toLongLong());

    CHECK(tipOf(coach, u"gem"_s).value(u"item"_s) == u"bacon-burger"_s);
    CHECK(tipOf(coach, u"weak"_s).value(u"item"_s) == u"kids-burger"_s);
    CHECK(tipOf(coach, u"never"_s).value(u"names"_s).toStringList().contains(u"Burger of the Day"_s));
    // One hidden gem a category, not every burger that sells less than the Classic.
    int gems = 0;
    for (const QVariant &v : tips)
        gems += v.toMap().value(u"kind"_s) == u"gem"_s && v.toMap().value(u"item"_s).toString().contains(u"burger"_s);
    CHECK(gems == 1);
    const QVariantMap quiet = tipOf(coach, u"quiet"_s);
    REQUIRE_FALSE(quiet.isEmpty());
    CHECK(quiet.value(u"title"_s).toString().startsWith(u"Quiet on Tuesday"_s));
    CHECK(quiet.value(u"title"_s).toString().contains(u"2:00"_s));
    CHECK(quiet.value(u"title"_s).toString().contains(u"5:00"_s));
    const QVariantMap discounts = tipOf(coach, u"discounts"_s);
    REQUIRE_FALSE(discounts.isEmpty());
    CHECK(discounts.value(u"why"_s).toString().contains(u"Manager Comp"_s));

    // Not Now: hidden for 30 days, then back.
    c.pos.shared()->history = [checks = c.checks](std::int64_t, std::int64_t) { return checks; };
    (void)c.pos.coach();   // starts reading
    for (int i = 0; i < 100 && c.pos.coach().value(u"busy"_s).toBool(); ++i)
        QTest::qWait(20);
    REQUIRE(c.pos.coachNotNow(raise.value(u"id"_s).toString()));
    CHECK(tipOf(c.pos.coach(), u"raise"_s).isEmpty());
    c.clock += 31LL * 86'400'000;
    c.pos.shared()->history = [checks = c.checks](std::int64_t, std::int64_t) { return checks; };
    (void)c.pos.coach();
    for (int i = 0; i < 100 && tipOf(c.pos.coach(), u"raise"_s).isEmpty(); ++i)
        QTest::qWait(20);
    // (A month on, the same sales are a month older: the tip is back only if it still holds.)
    CHECK(c.pos.shared()->settings.coachDismissed.contains(raise.value(u"id"_s).toString().toStdString()));

    // The report: each tip with what it's worth.
    const core::Report r = c.pos.buildReport(u"coach"_s);
    CHECK(r.title == "Profit Coach");
}
