#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"
#include "storage/sealed.hh"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::storage::AsyncWriter;
using vt::storage::PosStore;
using vt::storage::SqlPosSink;

// Safety under real use: power cuts, a damaged database, a second backup
// copy, and a long busy service on several terminals.

namespace {

QString seeded(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    storage::LayoutStore pages(path);   // a real store has its pages too
    REQUIRE(pages.open());
    PosStore store(path);
    REQUIRE(store.open());
    REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    return path;
}

// What a power cut leaves: the files as they are on disk right now, with
// nothing closed or checkpointed.
QString pullThePlug(const QString &db, const QString &to)
{
    QDir().mkpath(to);
    const QString copy = QDir(to).filePath(u"vt.db"_s);
    for (const QString &suffix : {u""_s, u"-wal"_s}) {
        if (QFile::exists(db + suffix))
            REQUIRE(QFile::copy(db + suffix, copy + suffix));
    }
    return copy;
}

} // namespace

TEST_CASE("Power cut mid-service: committed sales and the open check survive", "[safety]")
{
    QTemporaryDir dir;
    const QString path = seeded(dir);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);
    SqlPosSink sink(writer);
    PosService pos(*store.load(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());

    // One check closed, one half done: items sent and partly paid.
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"cobb"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.selectTable(u"T2"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"caesar"_s);
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    pos.entryKey(u"500"_s);
    REQUIRE(pos.tender(u"cash"_s));
    writer.flush();                                        // committed...

    QTemporaryDir after;
    const QString copy = pullThePlug(path, after.path()); // ...then the power goes
    QString problem;
    REQUIRE(storage::databaseIntact(copy, &problem));
    PosStore back(copy);
    REQUIRE(back.open());
    const auto data = back.load();
    REQUIRE(data);
    REQUIRE(data->closedToday.size() == 1);
    REQUIRE(data->openChecks.size() == 1);
    const core::Check &open = data->openChecks.front();
    CHECK(open.label == "T2");
    REQUIRE(open.lines.size() == 2);
    CHECK(open.lines[0].sent);
    REQUIRE(open.payments.size() == 1);
    CHECK(open.payments[0].amount == Money::fromCents(500));
    REQUIRE(data->drawers.size() == 1);
    CHECK(data->drawers[0].open());

    // And the store carries on from there.
    PosService again(*data, nullptr);
    REQUIRE(again.loginWithPin(u"1234"_s));
    REQUIRE(again.openCheck(open.id));
    REQUIRE(again.tender(u"cash"_s));
    REQUIRE(again.closeCheck());
}

TEST_CASE("A damaged database is caught at start", "[safety][backup]")
{
    QTemporaryDir dir;
    const QString path = seeded(dir);
    QString problem;
    REQUIRE(storage::databaseIntact(path, &problem));

    QFile f(path);
    REQUIRE(f.open(QIODevice::ReadWrite));
    const qint64 size = f.size();
    for (qint64 at = 4096 + 100; at < size; at += 4096) {   // scribble on every page but the first
        f.seek(at);
        f.write(QByteArray(400, '\x5a'));
    }
    f.close();
    CHECK_FALSE(storage::databaseIntact(path, &problem));
    CHECK_FALSE(problem.isEmpty());

    QFile junk(dir.filePath(u"junk.db"_s));
    REQUIRE(junk.open(QIODevice::WriteOnly));
    junk.write("this is not a database, just some text that is long enough to look like one");
    junk.close();
    CHECK_FALSE(storage::databaseIntact(junk.fileName(), &problem));
}

TEST_CASE("Backups: a second checked copy elsewhere; a missing drive is reported", "[safety][backup]")
{
    QTemporaryDir dir;
    const QString path = seeded(dir);
    const QString backups = dir.filePath(u"backups"_s);
    const QString usb = dir.filePath(u"usb/viewtouch"_s);

    storage::BackupScheduler scheduler(path, backups, 2, 0);
    scheduler.setCopyDirectory(usb);
    QSignalSpy done(&scheduler, &storage::BackupScheduler::finished);
    for (int i = 0; i < 3; ++i) {
        scheduler.backupNow();
        REQUIRE(done.wait(10000));
        INFO(done.last()[2].toString().toStdString());
        REQUIRE(done.last()[0].toBool());
        CHECK(done.last()[4].toBool());
        CHECK(done.last()[3].toString().contains(usb));
    }
    // Both places keep the newest two, and the copies are good databases.
    CHECK(storage::listBackups(backups).size() == 2);
    const QStringList copies = storage::listBackups(usb);
    REQUIRE(copies.size() == 2);
    CHECK(storage::verifyDatabase(copies.first()));
    CHECK(QFileInfo(copies.first()).fileName() == QFileInfo(storage::listBackups(backups).first()).fileName());

    // The drive is gone (a file where the folder should be): the backup
    // itself still works, the copy is reported.
    QFile blocker(dir.filePath(u"gone"_s));
    REQUIRE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    scheduler.setCopyDirectory(dir.filePath(u"gone/viewtouch"_s));
    scheduler.backupNow();
    REQUIRE(done.wait(10000));
    CHECK(done.last()[0].toBool());
    CHECK_FALSE(done.last()[4].toBool());
    CHECK(done.last()[3].toString().startsWith(u"Second copy failed"_s));
}

TEST_CASE("Encrypted files: only the password opens them, and changes are caught", "[safety][backup][sealed]")
{
    REQUIRE(storage::sealingAvailable());
    QTemporaryDir dir;
    const QString plain = seeded(dir);
    const QByteArray salt = storage::newSalt();
    REQUIRE(salt.size() == 16);
    const QByteArray key = storage::sealingKey(u"correct horse"_s, salt);
    REQUIRE(key.size() == 32);
    CHECK(key == storage::sealingKey(u"correct horse"_s, salt));
    CHECK(key != storage::sealingKey(u"correct horsf"_s, salt));
    CHECK(key != storage::sealingKey(u"correct horse"_s, storage::newSalt()));

    const QString sealed = dir.filePath(u"backup.vtbak"_s);
    REQUIRE(storage::sealFile(plain, sealed, key, salt));
    CHECK(storage::isSealed(sealed));
    CHECK_FALSE(storage::isSealed(plain));
    QFile f(sealed);
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();
    CHECK_FALSE(bytes.contains("Morgan"));   // nothing readable inside
    CHECK_FALSE(bytes.contains("SQLite format"));

    QString error;
    CHECK_FALSE(storage::openSealedFile(sealed, dir.filePath(u"wrong.db"_s), u"wrong password"_s, &error));
    CHECK(error.startsWith(u"Wrong password"_s));
    CHECK_FALSE(QFile::exists(dir.filePath(u"wrong.db"_s)));
    REQUIRE(storage::openSealedFile(sealed, dir.filePath(u"open.db"_s), u"correct horse"_s));
    CHECK(storage::verifyDatabase(dir.filePath(u"open.db"_s)));
    REQUIRE(storage::openSealedFileWithKey(sealed, dir.filePath(u"open2.db"_s), key));

    // One byte changed anywhere (here in the middle): refused.
    QByteArray changed = bytes;
    changed[changed.size() / 2] = char(changed[changed.size() / 2] ^ 1);
    QFile g(dir.filePath(u"changed.vtbak"_s));
    REQUIRE(g.open(QIODevice::WriteOnly));
    g.write(changed);
    g.close();
    CHECK_FALSE(storage::openSealedFile(g.fileName(), dir.filePath(u"changed.db"_s), u"correct horse"_s));
}

TEST_CASE("Encrypted backups: sealed, checked, copied, and restorable with the password", "[safety][backup][sealed]")
{
    QTemporaryDir dir;
    const QString path = seeded(dir);
    const QString backups = dir.filePath(u"backups"_s);
    const QString usb = dir.filePath(u"usb/viewtouch"_s);
    const QByteArray salt = storage::newSalt();
    const QByteArray key = storage::sealingKey(u"store password"_s, salt);

    storage::BackupScheduler scheduler(path, backups, 2, 0);
    scheduler.setCopyDirectory(usb);
    scheduler.setSealing(key, salt);
    QSignalSpy done(&scheduler, &storage::BackupScheduler::finished);
    scheduler.backupNow();
    REQUIRE(done.wait(20000));
    INFO(done.last()[2].toString().toStdString());
    REQUIRE(done.last()[0].toBool());
    CHECK(done.last()[4].toBool());
    const QStringList files = storage::listBackups(backups);
    REQUIRE(files.size() == 1);
    CHECK(files.first().endsWith(u".vtbak"_s));   // no plain copy left behind
    CHECK(QDir(backups).entryList(QDir::Files).size() == 1);
    const QStringList copies = storage::listBackups(usb);
    REQUIRE(copies.size() == 1);
    CHECK(storage::isSealed(copies.first()));
    REQUIRE(storage::openSealedFile(copies.first(), dir.filePath(u"restored.db"_s), u"store password"_s));
    CHECK(storage::verifyDatabase(dir.filePath(u"restored.db"_s)));
}

TEST_CASE("Encrypt backups: a manager sets a backup password", "[safety][backup][sealed]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.shared()->backupKeyFor = [](const QString &password) {
        const QByteArray salt = storage::newSalt();
        return std::pair{storage::sealingKey(password, salt), salt};
    };
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    CHECK_FALSE(store[u"encryptBackups"_s].toBool());
    store[u"encryptBackups"_s] = true;
    store[u"backupPassword"_s] = u"short"_s;
    CHECK_FALSE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.backupKey.empty());
    store[u"backupPassword"_s] = u"long enough"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    const std::string key = pos.shared()->settings.backupKey;
    CHECK(QByteArray::fromBase64(QByteArray::fromStdString(key)).size() == 32);
    CHECK(pos.adminRecords(u"store"_s).value(0).toMap()[u"backupPassword"_s].toString().isEmpty());   // never shown
    // Saving other settings keeps it; turning it off forgets it.
    store = pos.adminRecords(u"store"_s).value(0).toMap();
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.backupKey == key);
    store[u"encryptBackups"_s] = false;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.backupKey.empty());
}

TEST_CASE("Back Up Now: managers only, and the status shows on End of Day", "[safety][backup]")
{
    PosService pos(test::seedPosData(), nullptr);
    int asked = 0;
    pos.shared()->requestBackup = [&] { ++asked; return true; };
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.backupNow());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.backupNow());
    CHECK(asked == 1);
    pos.shared()->setBackupStatus({{u"at"_s, u"6:02 PM"_s}, {u"ok"_s, true}, {u"copy"_s, u"Copied to /media/usb."_s},
                                   {u"copyOk"_s, true}});
    CHECK(pos.dayInfo()[u"backup"_s].toMap()[u"at"_s] == u"6:02 PM"_s);

    pos.shared()->requestBackup = nullptr;                 // a store without backups
    CHECK_FALSE(pos.backupNow());
}

TEST_CASE("A long busy service on four terminals: quick, and nothing lost", "[safety][stress]")
{
    QTemporaryDir dir;
    const QString path = seeded(dir);
    PosStore store(path);
    REQUIRE(store.open());
    AsyncWriter writer(path);
    SqlPosSink sink(writer);
    app::PosShared shared(*store.load(), &sink);
    shared.settings.cashMode = core::CashMode::ServerBank;

    std::vector<std::unique_ptr<PosService>> terminals;
    for (int t = 0; t < 4; ++t)
        terminals.push_back(std::make_unique<PosService>(&shared, u"T%1"_s.arg(t)));
    const QStringList pins{u"1111"_s, u"2222"_s, u"4444"_s, u"1234"_s};   // Sam, Casey, Jo, Morgan
    for (int t = 0; t < 4; ++t) {
        REQUIRE(terminals[t]->loginWithPin(pins[t]));
        terminals[t]->entryKey(u"10000"_s);
        REQUIRE(terminals[t]->openDrawerSession());
    }

    const char *items[] = {"coffee", "cobb", "caesar", "pancakes", "water", "kids-burger", "soda"};
    std::vector<qint64> us;   // how long each step held the screen
    QElapsedTimer step;
    const auto timed = [&](auto f) {
        step.start();
        const bool ok = f();
        us.push_back(step.nsecsElapsed() / 1000);
        return ok;
    };
    constexpr int perTerminal = 250;   // 1000 checks, ~5000 steps
    Money expected;
    for (int n = 0; n < perTerminal; ++n) {
        for (int t = 0; t < 4; ++t) {
            PosService &pos = *terminals[t];
            REQUIRE(timed([&] { return pos.startCheck(core::CheckType::Takeout); }));
            for (int i = 0; i < 3; ++i)
                REQUIRE(timed([&] { return pos.addItem(QString::fromLatin1(items[(n + t + i) % 7])); }));
            REQUIRE(timed([&] { return pos.sendOrder(); }));
            REQUIRE(timed([&] { return pos.tender(u"cash"_s); }));
            REQUIRE(timed([&] { return pos.closeCheck(); }));
        }
    }
    for (const core::Check &c : shared.closedToday)
        expected += c.totals(shared.settings.tax).total;

    std::ranges::sort(us);
    const qint64 p99 = us[us.size() * 99 / 100];
    const qint64 worst = us.back();
    INFO("steps " << us.size() << ", p99 " << p99 << " us, worst " << worst << " us");
    CHECK(p99 < 20'000);     // the screen answers right away...
    CHECK(worst < 250'000);  // ...and never stalls

    // Everything reached the disk.
    QElapsedTimer drain;
    drain.start();
    writer.flush();
    INFO("writer drained in " << drain.elapsed() << " ms");
    CHECK(drain.elapsed() < 30'000);
    PosStore back(path);
    REQUIRE(back.open());
    const auto data = back.load();
    REQUIRE(data);
    REQUIRE(data->closedToday.size() == 4 * perTerminal);
    Money reloaded;
    for (const core::Check &c : data->closedToday)
        reloaded += c.totals(data->settings.tax).total;
    CHECK(reloaded == expected);
    CHECK(data->openChecks.empty());
}

#include "app/pos_demo.hh"

#include <QDateTime>
#include <QJsonArray>
#include <QProcess>

TEST_CASE("Demo data: months of real service, everything in use, then refuses a store with sales", "[demo]")
{
    PosService pos(test::seedPosData(true), nullptr);
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const QString done = app::fillDemoData(pos, now);
    INFO(done.toStdString());
    CHECK(done.startsWith(u"Added"_s));
    app::PosShared *s = pos.shared();
    CHECK(s->pastDays.size() == 126);
    // Some of them years back (Find a Check: 5 Years).
    CHECK(s->pastDays.back().day.openedAt < now - 4LL * 365 * 24 * 3'600'000);
    CHECK(s->customers.size() >= 12);                                    // and phone orders' guests
    CHECK(s->giftCards.size() == 8);
    CHECK(s->shifts.size() > 20);
    for (const core::CustomerRecord &c : s->customers)   // accounts are paid weekly
        if (c.houseAccount) CHECK(c.accountBalance < c.accountLimit);
    // Every past day balanced its banks and closed.
    for (const app::PastDay &d : s->pastDays)
        CHECK(d.day.closedAt > d.day.openedAt);
    // Over the days: refunds, deliveries with the driver, the checklists.
    const auto rowsOf = [&](const char *report, const QString &first) {
        int n = 0;
        for (const app::PastDay &d : s->pastDays)
            for (const QJsonValue &row : d.reports.value(QLatin1StringView(report)).toObject().value(u"rows").toArray())
                n += row.toObject().value(u"cells").toArray().first().toString().startsWith(first);
        return n;
    };
    CHECK(rowsOf("sales", u"Refunds"_s) > 5);
    CHECK(rowsOf("drivers", u"Lou"_s) > 30);
    CHECK(rowsOf("checklists", u"Count the starting cash"_s) > 100);
    CHECK_FALSE(s->deliveries.empty());                                   // vendors delivered
    CHECK(s->stockCounts.size() >= 10);                                   // Mondays' stock counts
    CHECK(s->waste.size() >= 10);                                         // and some waste
    CHECK(std::ranges::any_of(s->stockCounts, [](const core::StockCount &c) { return c.value().cents() < 0; }));
    // Regulars remember their last order; card payments say which card.
    CHECK(std::ranges::any_of(s->customers, [](const core::CustomerRecord &c) { return !c.lastOrder.empty(); }));
    // (Every card payment today, however early in the day it is.)
    for (const core::Check &c : s->closedToday)
        for (const core::Payment &p : c.payments)
            if (p.kind == core::TenderKind::Card)
                CHECK_FALSE(p.last4.empty());

    // Right now: tables seated, a tab, a phone order, a delivery out, one for later.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(s->open.size() >= 6);
    const QVariantMap floor = pos.floor();
    CHECK(floor[u"T2"_s].toMap()[u"state"_s] == u"seated"_s);
    CHECK(floor[u"T1"_s].toMap()[u"state"_s] == u"seated"_s);            // the walk-in
    CHECK(floor[u"T5"_s].toMap()[u"party"_s] == u"Martinez"_s);          // held
    CHECK(floor[u"T3"_s].toMap()[u"state"_s] == u"dirty"_s);
    bool out = false;
    for (const QVariant &v : pos.deliveries())
        out = out || v.toMap()[u"state"_s] == u"out"_s;
    CHECK(out);
    CHECK_FALSE(pos.kitchenTickets().isEmpty());
    CHECK(pos.waitlistInfo()[u"waiting"_s].toList().size() == 3);
    CHECK(pos.checklists()[u"openingDone"_s] == 5);
    CHECK(s->settings.staffRequests.size() >= 3);
    CHECK(std::ranges::any_of(s->settings.staffRequests, [](const auto &r) { return r.status == "approved"; }));
    CHECK(s->settings.notices.size() == 1);
    CHECK(app::fillDemoData(pos, now).startsWith(u"This store"_s));
}

TEST_CASE("Factory reset: backed up, then a fresh store", "[demo][backup]")
{
    QTemporaryDir dir;
    const QString exe = QCoreApplication::applicationDirPath() + u"/../vtmodern"_s;
    if (!QFile::exists(exe))
        SKIP("vtmodern is not built next to the tests");
    QProcess p;
    p.setProcessEnvironment([] { auto e = QProcessEnvironment::systemEnvironment(); e.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s); return e; }());
    p.start(exe, {u"--data-dir"_s, dir.path(), u"--demo-data"_s});
    REQUIRE(p.waitForFinished(120000));
    REQUIRE(p.exitCode() == 0);
    p.start(exe, {u"--data-dir"_s, dir.path(), u"--demo-data"_s});       // already has sales
    REQUIRE(p.waitForFinished(60000));
    CHECK(p.exitCode() == 1);
    p.start(exe, {u"--data-dir"_s, dir.path(), u"--factory-reset"_s});
    REQUIRE(p.waitForFinished(60000));
    REQUIRE(p.exitCode() == 0);
    CHECK_FALSE(QFile::exists(dir.filePath(u"viewtouch.db"_s)));
    const QStringList backups = storage::listBackups(dir.filePath(u"backups"_s));
    REQUIRE(backups.size() == 1);
    CHECK(backups.first().endsWith(u"-before-reset.db"_s));
    CHECK(storage::verifyDatabase(backups.first()));
}

TEST_CASE("Factory reset from the Manager page: managers, typed RESET, backed up first", "[demo][reset]")
{
    PosService pos(test::seedPosData(), nullptr);
    int asked = 0;
    bool backupWorks = true;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.factoryReset(u"RESET"_s));                     // not on the machine with the data
    pos.shared()->requestFactoryReset = [&] { ++asked; return backupWorks; };
    CHECK_FALSE(pos.factoryReset(u"reset please"_s));
    backupWorks = false;
    CHECK_FALSE(pos.factoryReset(u"RESET"_s));                     // the backup failed: nothing happens
    backupWorks = true;
    REQUIRE(pos.factoryReset(u" RESET "_s));
    CHECK(asked == 2);
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.factoryReset(u"RESET"_s));                     // managers only
    CHECK(asked == 2);
}

TEST_CASE("Today's banks: one closed for last night's End of Day isn't today's", "[storage][banks]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    const std::int64_t dayStart = QDateTime::currentMSecsSinceEpoch() - 3'600'000;
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        sink.saveDay({1, dayStart - 14 * 3'600'000, dayStart}, {});   // yesterday, ended at dayStart
        sink.saveDay({2, dayStart, 0}, {});                            // today
        core::DrawerSession last;   // tips paid out at closing, counted the same moment
        last.id = 1;
        last.name = "Morgan's bank";
        last.employeeId = "manager";
        last.openedAt = dayStart;
        last.closedAt = dayStart;
        core::CashMovement tips;
        tips.kind = core::CashMovement::Kind::TipPayout;
        tips.employeeId = "manager";
        tips.amount = Money::fromCents(1377);
        last.movements.push_back(tips);
        sink.saveDrawer(last);
        core::DrawerSession today = last;   // opened this morning, still open
        today.id = 2;
        today.openedAt = dayStart + 600'000;
        today.closedAt = 0;
        today.movements.clear();
        sink.saveDrawer(today);
        core::DrawerSession counted = today;   // opened and counted today
        counted.id = 3;
        counted.closedAt = dayStart + 1'200'000;
        sink.saveDrawer(counted);
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    QList<qint64> ids;
    for (const core::DrawerSession &d : data->drawers)
        ids << d.id;
    CHECK(ids == QList<qint64>{2, 3});
    // So nobody's tips today go below zero from last night's pay-out.
    PosService pos(*store.load(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.tipsOwed() == u"$0.00"_s);
}

TEST_CASE("Demo data made at any hour: today has guests already, nothing closed in the future", "[demo][demomidnight]")
{
    for (const int minutes : {1, 16, 90, 4 * 60, 9 * 60, 14 * 60 + 10}) {
        PosService pos(test::seedPosData(true), nullptr);
        const std::int64_t now = QDateTime(QDate::currentDate(), QTime(0, 0)).addSecs(minutes * 60).toMSecsSinceEpoch();
        REQUIRE(app::fillDemoData(pos, now).startsWith(u"Added"_s));
        INFO(minutes << " minutes after midnight");
        CHECK(pos.shared()->pastDays.size() == 126);
        CHECK(pos.shared()->day.openedAt <= now);
        CHECK_FALSE(pos.shared()->closedToday.empty());
        for (const core::Check &c : pos.shared()->closedToday)
            CHECK(c.closedAt <= now);
    }
}

TEST_CASE("Store Check: ready to open, and the problems kept for the managers", "[storeCheck]")
{
    app::PosData data = test::seedPosData();
    app::PosService pos(data, nullptr);
    app::PosShared *s = pos.shared();
    std::int64_t clock = QDateTime::currentMSecsSinceEpoch();
    s->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto check = [&](const QString &id) {
        for (const QVariant &v : pos.storeCheck()[u"checks"_s].toList())
            if (v.toMap()[u"id"_s] == id)
                return v.toMap();
        return QVariantMap();
    };
    CHECK(check(u"problems"_s)[u"ok"_s].toBool());
    CHECK(check(u"clock"_s)[u"ok"_s].toBool());

    // A printer out of paper: on the check, and kept as a problem (once, not every minute).
    REQUIRE(!s->settings.printers.empty());
    core::PrinterConfig &printer = s->settings.printers.front();
    printer.watch = true;
    s->setPrinterProblem(printer.id, "paperOut");
    s->setPrinterProblem(printer.id, {});
    s->setPrinterProblem(printer.id, "paperOut");
    CHECK_FALSE(check(u"printer:"_s + QString::fromStdString(printer.id))[u"ok"_s].toBool());
    QVariantMap info = pos.storeCheck();
    CHECK_FALSE(info[u"ready"_s].toBool());
    REQUIRE(info[u"problems"_s].toList().size() == 1);
    CHECK(info[u"unseen"_s].toInt() == 1);
    CHECK(info[u"problems"_s].toList()[0].toMap()[u"text"_s].toString().contains(u"out of paper"_s));
    s->setPrinterProblem(printer.id, "paperLow");   // a heads-up, not a problem
    CHECK(pos.storeCheck()[u"problems"_s].toList().size() == 1);

    // Others: the database, a backup; a crash.
    s->noteProblem("database", {}, "disk full");
    s->noteProblem("crash", "Mon 9:00 AM");
    info = pos.storeCheck();
    CHECK(info[u"unseen"_s].toInt() == 3);
    CHECK(info[u"problems"_s].toList()[0].toMap()[u"text"_s].toString().contains(u"closed without being told to"_s));
    CHECK(app::problemFromJson(app::toJson(s->problems.front())) == s->problems.front());

    // Seen: no longer new; a new one is.
    REQUIRE(pos.problemsSeen());
    CHECK(pos.storeCheck()[u"unseen"_s].toInt() == 0);
    CHECK(check(u"problems"_s)[u"ok"_s].toBool());
    s->noteProblem("backup", {}, "no room");
    CHECK(pos.storeCheck()[u"unseen"_s].toInt() == 1);

    // Older than a week: gone.
    clock += 8LL * 24 * 3'600'000;
    s->noteProblem("standby", {});
    CHECK(s->problems.size() == 1);

    // The clock behind the last sale (a dead clock battery).
    s->setPrinterProblem(printer.id, {});
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());
    clock -= 3'600'000;
    CHECK_FALSE(check(u"clock"_s)[u"ok"_s].toBool());

    // Servers don't get it.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.storeCheck().isEmpty());
}
