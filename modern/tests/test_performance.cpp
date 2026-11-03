#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstdio>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// How long the things a store waits on take, after a month of business on
// a real database: End of Day, starting up (loading the store), a month's
// report, and how big the database grows. Each has a budget with room for
// a slow CI machine and a Raspberry Pi, so a change that makes one many
// times slower (or the database much bigger) fails here, not in a store. The
// numbers are printed ("perf: ...") so CI's log shows the trend.

namespace {

// A day: 40 takeout checks of two items, half cash, then End of Day.
qint64 workADay(PosService &pos, std::int64_t &clock, const QDate &date)
{
    app::PosShared &s = *pos.shared();
    clock = QDateTime(date, QTime(10, 0)).toMSecsSinceEpoch();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    for (int n = 0; n < 40; ++n) {
        clock += 10 * 60'000;
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        REQUIRE(pos.addItem(n % 3 ? u"cobb"_s : u"caesar"_s));
        REQUIRE(pos.addItem(n % 2 ? u"coffee"_s : u"soda"_s));
        REQUIRE(pos.tender(n % 2 ? u"cash"_s : u"credit"_s));
        REQUIRE(pos.closeCheck());
    }
    clock = QDateTime(date, QTime(22, 0)).toMSecsSinceEpoch();
    for (const core::DrawerSession &d : s.drawers)
        if (d.open())
            pos.entryKey(QString::number(core::expectedCash(d, s.closedToday, s.settings.tax).cents()));
    REQUIRE(pos.countDrawer());
    QElapsedTimer t;
    t.start();
    REQUIRE(pos.endOfDay());
    const qint64 ms = t.elapsed();
    pos.logout();
    return ms;
}

} // namespace

TEST_CASE("Performance: a month on a real database, End of Day, startup and a month's report within budget", "[perf]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"viewtouch.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    const QDate first(2026, 3, 2);
    const int days = 30;
    qint64 endOfDayMax = 0;
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        std::int64_t clock = 0;
        pos.shared()->setClock([&clock] { return clock; });
        for (int d = 0; d < days; ++d)
            endOfDayMax = std::max(endOfDayMax, workADay(pos, clock, first.addDays(d)));
        writer.flush();
    }

    // Starting up: the store loaded from its database.
    QElapsedTimer t;
    t.start();
    storage::PosStore store(path);
    REQUIRE(store.open());
    auto data = store.load();
    REQUIRE(data);
    const qint64 loadMs = t.elapsed();
    REQUIRE(int(data->pastDays.size()) == days);

    // The month's sales report, worked out from the closed checks.
    PosService pos(std::move(*data), nullptr);
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) { return storage::closedChecksBetween(path, from, to); };
    pos.shared()->refundHistory = [&](std::int64_t from, std::int64_t to) { return storage::refundsBetween(path, from, to); };
    REQUIRE(pos.loginWithPin(u"1234"_s));
    t.restart();
    REQUIRE(pos.requestRangeReport(u"sales"_s, u"custom"_s, first.toString(u"yyyy-MM-dd"_s),
                                   first.addDays(days - 1).toString(u"yyyy-MM-dd"_s)));
    for (int i = 0; i < 1000 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    REQUIRE_FALSE(pos.rangeReport()[u"loading"_s].toBool());
    const qint64 reportMs = t.elapsed();

    const double megabytes = double(QFileInfo(path).size() + QFileInfo(path + u"-wal"_s).size()) / 1e6;
    std::printf("perf: End of Day %lld ms (most), startup %lld ms, month's report %lld ms, database %.1f MB "
                "after %d days of %d checks\n",
                qlonglong(endOfDayMax), qlonglong(loadMs), qlonglong(reportMs), megabytes, days, 40);
    std::fflush(stdout);
    // Here (a 16-core PC): 2 ms, 6 ms, 105 ms and 3.1 MB.
    CHECK(endOfDayMax < 500);
    CHECK(loadMs < 500);
    CHECK(reportMs < 2000);
    CHECK(megabytes < 12);
}
