#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::storage::AsyncWriter;
using vt::storage::PosStore;
using vt::storage::SqlPosSink;

TEST_CASE("PosStore seeds, then loads settings, menu, employees", "[posstore]")
{
    QTemporaryDir dir;
    const auto seed = test::seedPosData();
    PosStore store(dir.filePath(u"vt.db"_s));
    QString error;
    REQUIRE(store.open(&error));
    CHECK_FALSE(store.started());
    REQUIRE(store.seed(seed.settings, seed.menu, seed.employees, &error));
    CHECK(store.started());
    {
        // Started: an empty menu (all of it deleted, or an empty store) isn't a new store.
        QTemporaryDir other;
        PosStore emptied(other.filePath(u"vt.db"_s));
        REQUIRE(emptied.open());
        REQUIRE(emptied.seed(seed.settings, {}, seed.employees, &error));
        CHECK(emptied.started());
    }

    const auto loaded = store.load();
    REQUIRE(loaded);
    CHECK(loaded->settings == seed.settings);
    CHECK(loaded->menu == seed.menu);
    CHECK(loaded->employees.size() == seed.employees.size());
    CHECK(loaded->lastCheckId == 0);

    // The PIN still works after a round trip (hash + salt persisted).
    app::PosService pos(*loaded, nullptr);
    CHECK(pos.loginWithPin(u"1234"_s));
}

TEST_CASE("Service writes through the background writer and reloads", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }

    qint64 closedId = 0;
    {
        PosStore store(path);
        REQUIRE(store.open());
        AsyncWriter writer(path);
        SqlPosSink sink(writer);
        app::PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1111"_s));
        pos.pinKey(u"1"_s);   // PIN for clock in is the logged-in user; harmless
        REQUIRE(pos.clockIn());

        pos.selectTable(u"T2"_s);
        pos.startCheck(core::CheckType::DineIn);
        pos.addItem(u"caesar"_s);
        pos.sendOrder();

        pos.releaseCheck();
        REQUIRE(pos.openDrawerSession());   // cash needs an open drawer
        pos.startCheck(core::CheckType::Takeout);
        pos.addItem(u"soda"_s);
        closedId = pos.checkInfo().value(u"id"_s).toLongLong();
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
        // writer destructor flushes
    }

    PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->openChecks.size() == 1);
    CHECK(data->openChecks[0].label == "T2");
    CHECK(data->openChecks[0].lines[0].sent);
    CHECK(data->lastCheckId == 2);
    REQUIRE(data->punches.size() == 1);

    const auto closed = store.checks(core::CheckStatus::Closed);
    REQUIRE(closed.size() == 1);
    CHECK(closed[0].id == closedId);
    CHECK(closed[0].payments.size() == 1);
}

TEST_CASE("AsyncWriter coalesces repeated writes of one row", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);

    core::TimePunch p{1, "sam", 1000, 0};
    for (int i = 0; i < 500; ++i) {
        p.clockOut = 2000 + i;
        writer.upsert(u"time_punches"_s, u"1"_s,
                      {{u"id"_s, 1}, {u"employee_id"_s, u"sam"_s}, {u"clock_in"_s, 1000}, {u"clock_out"_s, qint64(p.clockOut)}});
    }
    CHECK(writer.pending() <= 1);   // never more than one queued version of the row
    writer.flush();
    CHECK(writer.pending() == 0);
    const auto punches = store.punches();
    REQUIRE(punches.size() == 1);
    CHECK(punches[0].clockOut == 2499);
}

TEST_CASE("AsyncWriter reports failures and keeps the row queued", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);
    QSignalSpy failed(&writer, &AsyncWriter::writeFailed);

    writer.upsert(u"no_such_table"_s, u"1"_s, {{u"id"_s, 1}});
    writer.flush();
    QCoreApplication::processEvents();   // deliver the queued signal
    REQUIRE(failed.size() >= 1);
    CHECK(failed.first()[0].toString().contains(u"no_such_table"_s));
    CHECK(writer.pending() == 1);
}

TEST_CASE("AsyncWriter sets aside a row it can never write, and saves the rest", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);
    QSignalSpy setAside(&writer, &AsyncWriter::rowSetAside);

    writer.upsert(u"no_such_table"_s, u"1"_s, {{u"id"_s, 1}});
    writer.upsert(u"time_punches"_s, u"7"_s,
                  {{u"id"_s, 7}, {u"employee_id"_s, u"sam"_s}, {u"clock_in"_s, 1000}, {u"clock_out"_s, 0}});
    for (int i = 0; i < 6 && writer.pending() > 0; ++i)
        writer.flush();
    writer.flush();   // the rest goes at once after the bad row leaves
    QCoreApplication::processEvents();
    CHECK(writer.pending() == 0);
    REQUIRE(setAside.size() == 1);
    CHECK(setAside.first()[0].toString() == u"no_such_table"_s);
    CHECK(store.punches().size() == 1);

    QFile kept(writer.setAsidePath());
    REQUIRE(kept.open(QIODevice::ReadOnly));
    const QJsonObject line = QJsonDocument::fromJson(kept.readLine()).object();
    CHECK(line.value(u"table"_s).toString() == u"no_such_table"_s);
    CHECK(line.value(u"row"_s).toObject().value(u"id"_s).toInt() == 1);
}

TEST_CASE("AsyncWriter takes only plain table and column names", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);
    QSignalSpy failed(&writer, &AsyncWriter::writeFailed);
    writer.upsert(u"time_punches; DROP TABLE checks"_s, u"1"_s, {{u"id"_s, 1}});
    writer.upsert(u"time_punches"_s, u"1"_s, {{u"id) VALUES (1); --"_s, 1}});
    writer.remove(u"time_punches"_s, u"id = id OR 1"_s, u"1"_s);
    CHECK(writer.pending() == 0);   // never queued
    CHECK(failed.size() == 3);
}

TEST_CASE("Queuing writes never blocks the caller", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);

    // 2000 distinct rows: the caller only queues; the worker does the I/O.
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 2000; ++i) {
        writer.upsert(u"time_punches"_s, QString::number(i),
                      {{u"id"_s, i}, {u"employee_id"_s, u"sam"_s}, {u"clock_in"_s, i}, {u"clock_out"_s, 0}});
    }
    const qint64 queueMs = timer.elapsed();
    CHECK(queueMs < 200);
    writer.flush();
    CHECK(store.punches().size() == 2000);
}

TEST_CASE("Day, drawer and past-day reports survive a restart", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    auto run = [&](const std::function<void(app::PosService &)> &body) {
        PosStore store(path);
        REQUIRE(store.open());
        AsyncWriter writer(path);
        SqlPosSink sink(writer);
        app::PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        body(pos);
    };

    run([](app::PosService &pos) {
        pos.entryKey(u"5000"_s);
        REQUIRE(pos.openDrawerSession());
        pos.addItem(u"coffee"_s);
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
    });
    run([](app::PosService &pos) {
        // Same day after a restart: the sale and the open drawer are back.
        CHECK(pos.currentDay().id == 1);
        CHECK(pos.closedToday().size() == 1);
        CHECK(pos.drawerInfo()[u"open"_s].toBool());
        CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$52.98"_s);
        pos.entryKey(u"5298"_s);
        REQUIRE(pos.countDrawer());
        REQUIRE(pos.endOfDay());
    });
    run([](app::PosService &pos) {
        CHECK(pos.currentDay().id == 2);
        CHECK(pos.closedToday().empty());
        CHECK_FALSE(pos.drawerInfo()[u"exists"_s].toBool());
        REQUIRE(pos.days().size() == 2);
        const QVariantList rows = pos.report(u"sales"_s, 1)[u"rows"_s].toList();
        CHECK_FALSE(rows.isEmpty());
        // The next check continues the serial numbers.
        pos.addItem(u"tea"_s);
        CHECK(pos.checkInfo()[u"id"_s].toLongLong() == 2);
    });
}

TEST_CASE("A version 1 POS database is migrated in place", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    {
        // The M3 schema, with one closed check in it.
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"v1"_s);
        db.setDatabaseName(path);
        REQUIRE(db.open());
        QSqlQuery q(db);
        for (const char *sql : {
                 "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
                 "INSERT INTO meta VALUES ('pos_schema_version', '1')",
                 "CREATE TABLE settings (key TEXT PRIMARY KEY, json TEXT NOT NULL)",
                 "CREATE TABLE menu_items (id TEXT PRIMARY KEY, position INTEGER NOT NULL, json TEXT NOT NULL)",
                 "CREATE TABLE employees (id TEXT PRIMARY KEY, json TEXT NOT NULL)",
                 "CREATE TABLE checks (id INTEGER PRIMARY KEY, status TEXT NOT NULL, label TEXT, server_id TEXT, "
                 "opened_at INTEGER, closed_at INTEGER, json TEXT NOT NULL)",
                 "CREATE INDEX checks_status ON checks (status)",
                 "CREATE TABLE time_punches (id INTEGER PRIMARY KEY, employee_id TEXT NOT NULL, "
                 "clock_in INTEGER NOT NULL, clock_out INTEGER NOT NULL DEFAULT 0)",
                 "INSERT INTO checks VALUES (7, 'closed', 'T1', 'sam', 1, 2, '{\"id\":7,\"status\":\"closed\"}')"}) {
            REQUIRE(q.exec(QString::fromLatin1(sql)));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(u"v1"_s);

    PosStore store(path);
    QString error;
    REQUIRE(store.open(&error));
    const auto seed = test::seedPosData();
    REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    const auto data = store.load();
    REQUIRE(data);
    CHECK(data->lastCheckId == 7);
    CHECK(store.checks(core::CheckStatus::Closed).size() == 1);
    CHECK(data->lastDayId == 0);   // no days yet: the service opens day 1
}

TEST_CASE("A database stopped halfway through adding cash tips still opens", "[posstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    {
        PosStore store(path);
        REQUIRE(store.open());
    }
    {
        // The column is there but the version says it isn't: what a stop
        // between the two steps left behind before they were one transaction.
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"half"_s);
        db.setDatabaseName(path);
        REQUIRE(db.open());
        QSqlQuery q(db);
        REQUIRE(q.exec(u"UPDATE meta SET value = '12' WHERE key = 'pos_schema_version'"_s));
        db.close();
    }
    QSqlDatabase::removeDatabase(u"half"_s);

    PosStore store(path);
    QString error;
    const bool opened = store.open(&error);
    INFO(error.toStdString());
    CHECK(opened);
}

TEST_CASE("A shift keeps the job worked and its pay in the database", "[posstore][pay]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        PosStore store(path);
        QString why;
        const bool opened = store.open(&why);
        INFO(why.toStdString());
        REQUIRE(opened);
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
        AsyncWriter writer(path);
        SqlPosSink sink(writer);
        core::TimePunch p{7, "jo", QDateTime::currentMSecsSinceEpoch(), 0, {}, "server", Money::fromCents(725)};
        sink.savePunch(p);
    }
    PosStore store(path);
    QString why2;
    const bool reopened = store.open(&why2);
    INFO(why2.toStdString());
    REQUIRE(reopened);
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->punches.size() == 1);
    CHECK(data->punches[0].job == "server");
    CHECK(data->punches[0].rate.cents() == 725);
    CHECK(data->employees.size() == seed.employees.size());
    const auto jo = std::ranges::find_if(data->employees, [](const core::Employee &e) { return e.id == "jo"; });
    REQUIRE(jo != data->employees.end());
    CHECK(jo->payRate.cents() == 900);
    CHECK(jo->otherJobs.size() == 1);
}

TEST_CASE("Who changed what: kept through a restart, in the Changes report, archived at End of Day", "[posstore][changes]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    auto run = [&](const std::function<void(app::PosService &)> &body) {
        PosStore store(path);
        REQUIRE(store.open());
        AsyncWriter writer(path);
        SqlPosSink sink(writer);
        app::PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        body(pos);
    };
    const auto has = [](const app::PosService &pos, const std::string &item, const std::string &field,
                        const std::string &before, const std::string &after) {
        return std::ranges::any_of(pos.changesToday(), [&](const core::Change &c) {
            return c.item == item && c.field == field && c.before == before && c.after == after;
        });
    };

    run([&](app::PosService &pos) {
        const std::string me = pos.userName().toStdString();
        // Store Settings: the name and a switch.
        QVariantMap store = pos.adminRecords(u"store"_s)[0].toMap();
        const std::string was = store[u"storeName"_s].toString().toStdString();
        store[u"storeName"_s] = u"Casa Ariel"_s;
        store[u"blindDrawerCount"_s] = true;
        REQUIRE(pos.adminSave(u"store"_s, 0, store));
        CHECK(has(pos, "", "Store name", was, "Casa Ariel"));
        CHECK(has(pos, "", "Count drawers blind", "no", "yes"));
        CHECK(pos.changesToday().front().who == me);
        CHECK(pos.changesToday().front().area == "Store Settings");

        // An employee's PIN: said, never shown.
        QVariantList staff = pos.adminRecords(u"employees"_s);
        int sam = -1;
        for (int i = 0; i < staff.size(); ++i)
            if (staff[i].toMap()[u"name"_s] == u"Sam"_s)
                sam = i;
        REQUIRE(sam >= 0);
        QVariantMap e = staff[sam].toMap();
        e[u"pin"_s] = u"9876"_s;
        e[u"role"_s] = u"bartender"_s;
        REQUIRE(pos.adminSave(u"employees"_s, sam, e));
        const auto pinChange = std::ranges::find_if(pos.changesToday(), [](const core::Change &c) {
            return c.item == "Sam" && c.after == "(changed)";
        });
        REQUIRE(pinChange != pos.changesToday().end());
        CHECK(pinChange->before.empty());
        for (const core::Change &c : pos.changesToday())
            CHECK(c.after.find("9876") == std::string::npos);
        CHECK(std::ranges::any_of(pos.changesToday(), [](const core::Change &c) {
            return c.item == "Sam" && c.before == "Server" && c.after == "Bartender";
        }));

        // The Menu Builder: a price, a new item, and taking one out.
        const std::string cobb = pos.findItem(u"cobb"_s)->name;
        const vt::Money old = pos.findItem(u"cobb"_s)->price;
        REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, QString::fromStdString(cobb)}, {u"price"_s, u"13.25"_s}}));
        CHECK(has(pos, cobb, "Price", pos.format(old).toStdString(), "$13.25"));
        REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"Horchata"_s}, {u"price"_s, u"3.00"_s}, {u"family"_s, u"drinks"_s}}));
        CHECK(has(pos, "Horchata", "", "", "Added, $3.00"));
        REQUIRE(pos.undoMenuChange());
        CHECK(has(pos, "Horchata", "", "", "Removed"));

        // A page saved in edit mode.
        REQUIRE(pos.logPageEdits({}, {}, {u"Settle"_s}));
        CHECK(has(pos, "Settle", "", "", "Edited"));

        const core::Report r = pos.buildReport(u"changes"_s);
        CHECK(r.rows.size() == pos.changesToday().size() + 1);   // and the count
        CHECK(r.rows.front().cells[0].ends_with("Pages: Settle"));   // newest first
    });
    run([&](app::PosService &pos) {
        // After a restart: still there; End of Day keeps the report with the day.
        CHECK(std::ranges::any_of(pos.changesToday(), [](const core::Change &c) { return c.after == "Casa Ariel"; }));
        const std::size_t n = pos.changesToday().size();
        CHECK(n >= 6);
        CHECK(app::changeFromJson(app::toJson(pos.changesToday().front())) == pos.changesToday().front());
        REQUIRE(pos.endOfDay());
        CHECK(pos.changesToday().empty());
        const QVariantList rows = pos.report(u"changes"_s, 1)[u"rows"_s].toList();
        CHECK(std::size_t(rows.size()) == n + 1);
    });
}
