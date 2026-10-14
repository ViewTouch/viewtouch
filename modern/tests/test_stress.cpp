#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "net/layout_hub.hh"
#include "net/stress.hh"
#include "pos_fixture.hh"
#include "print/spooler.hh"
#include "print/ticket_printer.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>

using namespace Qt::StringLiterals;
using namespace vt;

// A short busy dinner with no screen (hidden: "[.stress]"; VTM_STRESS_MINUTES
// and VTM_STRESS_SPEED change it). Prints the report.
TEST_CASE("Stress: a busy dinner, every check balanced and saved", "[.stress]")
{
    QTemporaryDir dir;
    const QString db = dir.filePath(u"stress.db"_s);
    {
        storage::PosStore store(db);
        REQUIRE(store.open());
        const app::PosData seed = test::seedPosData();
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees, nullptr, seed.ingredients));
    }
    storage::PosStore store(db);
    REQUIRE(store.open());
    auto data = store.load();
    REQUIRE(data);
    storage::AsyncWriter writer(db);
    storage::SqlPosSink sink(writer);
    app::PosShared shared(std::move(*data), &sink);
    print::PrintSpooler spooler;
    print::TicketPrinter printer(spooler, dir.filePath(u"printouts"_s));
    shared.printer = &printer;
    auto layout = test::loadTestLayout();
    REQUIRE(layout);
    net::LayoutHub hub(*layout);

    net::StressOptions o;
    o.minutes = qEnvironmentVariableIntValue("VTM_STRESS_MINUTES") > 0 ? qEnvironmentVariableIntValue("VTM_STRESS_MINUTES") : 1;
    o.speed = qEnvironmentVariableIntValue("VTM_STRESS_SPEED") > 0 ? qEnvironmentVariableIntValue("VTM_STRESS_SPEED") : 60;
    o.hours = qEnvironmentVariable("VTM_STRESS_HOURS").toDouble();
    o.load = std::max(1, qEnvironmentVariableIntValue("VTM_STRESS_LOAD"));
    o.dbPath = db;
    o.writer = &writer;
    net::StressRunner::tablesOn(*layout, o.tables, o.barSeats);
    REQUIRE(o.tables.size() >= 4);
    net::StressRunner run(&shared, &hub, o);
    QString error;
    REQUIRE(run.start(&error));
    QElapsedTimer t;
    t.start();
    const int minutes = o.hours > 0 ? int(std::ceil(o.hours * 60 / o.speed)) : o.minutes;
    while (!run.isFinished() && t.elapsed() < (minutes + 6) * 60'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(2);
    }
    REQUIRE(run.isFinished());
    WARN(run.report().toStdString());
    CHECK(run.passed());
    CHECK(run.stats()[u"closed"_s].toInt() > 20);
}
