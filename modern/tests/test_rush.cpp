#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "core/rush.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using Catch::Approx;
using vt::app::PosService;

// Rush Radar: each kitchen station's next half hour.

TEST_CASE("Rush Radar: a station's pace is learned from its tickets", "[rush]")
{
    core::StationPace p;
    CHECK_FALSE(p.known());
    CHECK(p.minutesAt(4, 6) == Approx(8));   // not known yet: the usual, plus half a minute an item ahead
    // This grill: 4 minutes for a ticket with nothing ahead, 1.5 more for each item ahead.
    for (int i = 0; i < 40; ++i) {
        const double q = i % 9;
        p.learn(q, 4 + 1.5 * q);
    }
    REQUIRE(p.known());
    CHECK(p.minutesAt(0, 6) == Approx(4).margin(0.01));
    CHECK(p.minutesAt(10, 6) == Approx(19).margin(0.05));
    CHECK(p.throughput(6) == Approx(1 / 1.5).margin(0.01));
    p.learn(3, 500);   // a ticket forgotten on the screen: ignored
    CHECK(p.minutesAt(10, 6) == Approx(19).margin(0.05));
}

TEST_CASE("Rush Radar: the line grows while more comes than the station clears, then goes down", "[rush]")
{
    core::StationPace p;
    for (int i = 0; i < 40; ++i)
        p.learn(i % 6, 3 + 1.0 * (i % 6));   // 1 minute an item ahead: clears one a minute
    std::vector<double> arrivals(30, 0.0);
    for (int m = 0; m < 10; ++m)
        arrivals[m] = 3;                      // a rush: 3 items a minute for 10 minutes
    const auto points = core::projectRush(2, arrivals, p, 5);
    REQUIRE(points.size() == 31);
    CHECK(points[0].queue == Approx(2));
    CHECK(points[10].queue == Approx(22));    // 2 + 10 x (3 - 1)
    CHECK(points[10].wait == Approx(25).margin(0.1));
    CHECK(points[30].queue == Approx(2));     // cleared at one a minute after it
    double worst = 0;
    for (const auto &r : points)
        worst = std::max(worst, r.wait);
    CHECK(worst == Approx(points[10].wait));
}

TEST_CASE("Rush Radar: the line now, what's being rung in, tables just seated; the headline", "[rush]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = QDateTime(QDate(2026, 10, 16), QTime(19, 0)).toMSecsSinceEpoch();   // a Friday at 7
    pos.shared()->setClock([&] { return clock; });
    pos.shared()->settings.kitchenWarnMinutes = 10;
    pos.shared()->settings.kitchenLateMinutes = 15;
    // The grill's pace: 4 minutes, 1.5 more for each item ahead.
    for (int i = 0; i < 40; ++i)
        pos.shared()->settings.stationPace["grill"].learn(i % 9, 4 + 1.5 * (i % 9));
    // Tables here usually order a grill item per guest.
    pos.shared()->rushPast.tableItems["classic-burger"] = 100;
    pos.shared()->rushPast.tableGuests = 100;
    REQUIRE(pos.loginWithPin(u"1111"_s));

    // Six burgers on the grill now (sent: each knows what was ahead of it).
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    pos.entryKey(u"6"_s);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    for (int i = 0; i < 6; ++i) {
        REQUIRE(pos.addItem(u"classic-burger"_s));
        for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList())
            if (!g.toMap()[u"done"_s].toBool())
                pos.chooseOption(g.toMap()[u"id"_s].toString(), 0);
        pos.finishChoosing();
    }
    REQUIRE(pos.sendOrder());
    const core::Check &t1 = pos.shared()->open.begin()->second;
    CHECK(t1.lines.front().queueAtSend == 0);
    CHECK(app::checkFromJson(app::toJson(t1))->lines.front().queueAtSend == 0);
    pos.releaseCheck();

    QVariantMap radar = pos.rushRadar();
    QVariantMap grill;
    for (const QVariant &s : radar[u"stations"_s].toList())
        if (s.toMap()[u"id"_s] == u"grill"_s)
            grill = s.toMap();
    REQUIRE_FALSE(grill.isEmpty());
    CHECK(grill[u"name"_s] == u"Grill"_s);
    CHECK(grill[u"queue"_s] == 6);
    CHECK(grill[u"waitNow"_s] == 13);   // 4 + 1.5 x 6
    CHECK(grill[u"learned"_s].toBool());
    CHECK(grill[u"ahead"_s].toList().size() == 7);   // now, then every 5 minutes

    // A party of 8 just sat down, nothing ordered: about 8 grill items soon.
    clock += 60'000;
    REQUIRE(pos.selectTable(u"T2"_s) == PosService::TableNeedsGuests);
    pos.entryKey(u"8"_s);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.releaseCheck();
    radar = pos.rushRadar();
    for (const QVariant &s : radar[u"stations"_s].toList())
        if (s.toMap()[u"id"_s] == u"grill"_s)
            grill = s.toMap();
    CHECK(grill[u"coming"_s].toInt() == 8);
    CHECK(grill[u"peakWait"_s].toInt() > 13);
    CHECK(grill[u"level"_s] != u"ok"_s);
    CHECK(radar[u"headline"_s].toString().startsWith(u"Grill"_s));

    // Bumped: the line goes down, and the pace learns from it.
    const double before = pos.shared()->settings.stationPace["grill"].n;
    clock += 9 * 60'000;
    REQUIRE(pos.bumpTicket(t1.id, t1.lines.front().sentAt, u"grill"_s));
    CHECK(pos.shared()->settings.stationPace["grill"].n > before);
    radar = pos.rushRadar();
    for (const QVariant &s : radar[u"stations"_s].toList())
        if (s.toMap()[u"id"_s] == u"grill"_s)
            grill = s.toMap();
    CHECK(grill[u"queue"_s] == 0);
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).stationPace.at("grill")
          == pos.shared()->settings.stationPace.at("grill"));
}
