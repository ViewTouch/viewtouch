#include <catch2/catch_test_macros.hpp>

#include "app/navigator.hh"
#include "app/pos_json.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// M7: meal periods as settings, database backups and restore.

namespace {

// A database with pages and POS data, as vtmodern leaves it.
bool makeDatabase(const QString &path, const QString &storeName)
{
    storage::LayoutStore pages(path);
    storage::PosStore pos(path);
    if (!pages.open() || !pos.open())
        return false;
    app::PosData d = test::seedPosData();
    d.settings.storeName = storeName.toStdString();
    return pos.seed(d.settings, d.menu, d.employees);
}

QString storeNameIn(const QString &path)
{
    storage::PosStore pos(path);
    if (!pos.open())
        return {};
    const auto data = pos.load();
    return data ? QString::fromStdString(data->settings.storeName) : QString();
}

} // namespace

TEST_CASE("Meal periods: the latest start at or before now; overnight wraps", "[m7][meal]")
{
    const auto periods = core::defaultMealPeriods();
    CHECK(core::mealPeriodAt(periods, 7 * 60 + 30) == "breakfast");
    CHECK(core::mealPeriodAt(periods, 11 * 60) == "lunch");
    CHECK(core::mealPeriodAt(periods, 19 * 60) == "dinner");
    CHECK(core::mealPeriodAt(periods, 2 * 60) == "dinner");   // before breakfast: dinner runs on

    std::vector<core::MealPeriod> custom{{"late", "Late Night", 22 * 60}, {"day", "All Day", 6 * 60}};
    CHECK(core::mealPeriodAt(custom, 5 * 60) == "late");
    CHECK(core::mealPeriodAt(custom, 12 * 60) == "day");
    CHECK(core::mealPeriodAt(custom, 23 * 60) == "late");
    CHECK(core::mealPeriodAt({}, 12 * 60).empty());

    CHECK(app::clockMinutes(u"16:30"_s) == 16 * 60 + 30);
    CHECK(app::clockMinutes(u"7"_s) == 7 * 60);
    CHECK(app::clockMinutes(u"24:00"_s) == -1);
    CHECK(app::clockMinutes(u"noon"_s) == -1);
    CHECK(app::clockText(4 * 60 + 5) == u"04:05"_s);   // how it's stored
    // Typed and shown 12-hour too.
    CHECK(app::clockMinutes(u"4:30 PM"_s) == 16 * 60 + 30);
    CHECK(app::clockMinutes(u"4:30pm"_s) == 16 * 60 + 30);
    CHECK(app::clockMinutes(u"9 a.m."_s) == 9 * 60);
    CHECK(app::clockMinutes(u"12:15 AM"_s) == 15);
    CHECK(app::clockMinutes(u"12 PM"_s) == 12 * 60);
    CHECK(app::clockMinutes(u"13:00 PM"_s) == -1);
    CHECK(app::clockText12(16 * 60 + 30) == u"4:30 PM"_s);
    CHECK(app::clockText12(0) == u"12:00 AM"_s);
    const std::int64_t at = app::dateTimeMs(u"2026-10-09 7:05 PM"_s);
    CHECK(at == QDateTime(QDate(2026, 10, 9), QTime(19, 5)).toMSecsSinceEpoch());
    CHECK(app::dateTimeMs(u"2026-10-09 19:05"_s) == at);
    CHECK(app::dateTimeField(at) == u"2026-10-09 7:05 PM"_s);
    CHECK(app::dateTimeMs(u"tomorrow"_s) == 0);
}

TEST_CASE("Meal periods are edited on the manager screen and saved with the settings", "[m7][meal][admin]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.adminRecords(u"mealPeriods"_s).size() == 3);

    QSignalSpy admin(&pos, &PosService::adminChanged);
    QVariantMap late = pos.adminNewRecord(u"mealPeriods"_s);
    late[u"name"_s] = u"Late Night"_s;
    late[u"start"_s] = u"2x:00"_s;
    CHECK_FALSE(pos.adminSave(u"mealPeriods"_s, -1, late));
    late[u"start"_s] = u"16:00"_s;                        // dinner starts then
    CHECK_FALSE(pos.adminSave(u"mealPeriods"_s, -1, late));
    late[u"start"_s] = u"22:00"_s;
    REQUIRE(pos.adminSave(u"mealPeriods"_s, -1, late));
    CHECK(admin.count() >= 1);

    const QVariantList periods = pos.mealPeriods();
    REQUIRE(periods.size() == 4);
    CHECK(periods[3].toMap()[u"id"_s] == u"late-night"_s);
    CHECK(periods[3].toMap()[u"start"_s].toInt() == 22 * 60);

    // Moving lunch earlier keeps the list in time order.
    QVariantMap lunch = pos.adminRecords(u"mealPeriods"_s)[1].toMap();
    lunch[u"start"_s] = u"03:00"_s;
    REQUIRE(pos.adminSave(u"mealPeriods"_s, 1, lunch));
    CHECK(pos.mealPeriods()[0].toMap()[u"id"_s] == u"lunch"_s);

    // JSON keeps them; settings saved before M7 get the defaults.
    const core::PosSettings back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(back.mealPeriods == pos.shared()->settings.mealPeriods);
    CHECK(app::settingsFromJson(QJsonObject{{u"storeName"_s, u"Old"_s}}).mealPeriods == core::defaultMealPeriods());

    REQUIRE(pos.adminDelete(u"mealPeriods"_s, 0));
    CHECK(pos.mealPeriods().size() == 3);

    // Servers can't change them.
    PosService server(test::seedPosData(), nullptr);
    REQUIRE(server.loginWithPin(u"1111"_s));
    CHECK_FALSE(server.adminSave(u"mealPeriods"_s, -1, late));
}

TEST_CASE("The page controller picks the index page from the store's meal periods", "[m7][meal][ui]")
{
    const QVariantList periods{QVariantMap{{u"id"_s, u"brunch"_s}, {u"start"_s, 9 * 60}},
                               QVariantMap{{u"id"_s, u"dinner"_s}, {u"start"_s, 15 * 60}}};
    CHECK(LayoutController::mealPeriodAt(periods, QTime(10, 0)) == u"brunch"_s);
    CHECK(LayoutController::mealPeriodAt(periods, QTime(8, 0)) == u"dinner"_s);
    CHECK(LayoutController::mealPeriodAt(QTime(12, 0)) == u"lunch"_s);

    // With no page for the period, an all-day index page wins over the first one.
    layout::Layout l;
    for (const auto &[id, period] : {std::pair{u"idx-a"_s, u"breakfast"_s}, std::pair{u"idx-b"_s, u"all"_s}}) {
        layout::Page p;
        p.id = id;
        p.name = id;
        p.kind = u"index"_s;
        p.mealPeriod = period;
        l.pages.append(p);
    }
    layout::Page home;
    home.id = u"home"_s;
    home.kind = u"custom"_s;
    l.pages.append(home);
    app::Navigator nav(l);
    nav.reset(u"home"_s);
    nav.setMealPeriod(u"brunch"_s);
    REQUIRE(nav.jump(app::Navigator::Mode::Index));
    CHECK(nav.current() == u"idx-b"_s);
}

TEST_CASE("Backups: consistent copy, checked, pruned, restored", "[m7][backup]")
{
    QTemporaryDir dir;
    const QString db = dir.filePath(u"viewtouch.db"_s);
    REQUIRE(makeDatabase(db, u"Before"_s));

    const QString backups = dir.filePath(u"backups"_s);
    REQUIRE(QDir().mkpath(backups));
    const QString first = QDir(backups).filePath(storage::backupFileName(QDateTime(QDate(2026, 9, 1), QTime(23, 0))));
    QString error;
    REQUIRE(storage::backupDatabase(db, first, &error));
    CHECK(error.isEmpty());
    CHECK(storage::verifyDatabase(first));
    CHECK_FALSE(storage::backupDatabase(db, first, &error));   // never overwrites
    CHECK_FALSE(QFile::exists(first + u".part"_s));

    // Not a ViewTouch database.
    QFile junk(dir.filePath(u"junk.db"_s));
    REQUIRE(junk.open(QIODevice::WriteOnly));
    junk.write("not sqlite");
    junk.close();
    CHECK_FALSE(storage::verifyDatabase(junk.fileName(), &error));
    CHECK_FALSE(storage::restoreDatabase(junk.fileName(), db, nullptr, &error));
    CHECK(storeNameIn(db) == u"Before"_s);

    // Newest first; pruning keeps the newest.
    for (int day = 2; day <= 4; ++day)
        REQUIRE(storage::backupDatabase(
            db, QDir(backups).filePath(storage::backupFileName(QDateTime(QDate(2026, 9, day), QTime(23, 0))))));
    QStringList all = storage::listBackups(backups);
    REQUIRE(all.size() == 4);
    CHECK(all.first().endsWith(u"viewtouch-20260904-230000.db"_s));
    CHECK(storage::pruneBackups(backups, 2).size() == 2);
    all = storage::listBackups(backups);
    REQUIRE(all.size() == 2);
    CHECK(all.last().endsWith(u"viewtouch-20260903-230000.db"_s));

    // Restore puts the backup in place and keeps the current database aside.
    const QString other = dir.filePath(u"other.db"_s);
    REQUIRE(makeDatabase(other, u"After"_s));
    const QString otherBackup = dir.filePath(u"other-backup.db"_s);
    REQUIRE(storage::backupDatabase(other, otherBackup));
    QString keptAs;
    REQUIRE(storage::restoreDatabase(otherBackup, db, &keptAs, &error));
    CHECK(storeNameIn(db) == u"After"_s);
    CHECK(storeNameIn(keptAs) == u"Before"_s);
}

TEST_CASE("Backup scheduler: backs up when due, not again right after", "[m7][backup]")
{
    QTemporaryDir dir;
    const QString db = dir.filePath(u"viewtouch.db"_s);
    REQUIRE(makeDatabase(db, u"Sched"_s));
    const QString backups = dir.filePath(u"backups"_s);

    {
        storage::BackupScheduler scheduler(db, backups, 5, 24);
        QSignalSpy done(&scheduler, &storage::BackupScheduler::finished);
        scheduler.start();                     // none yet: due now
        REQUIRE(done.wait(10000));
        CHECK(done.first()[0].toBool());
        CHECK(storage::verifyDatabase(done.first()[1].toString()));
    }
    {
        storage::BackupScheduler scheduler(db, backups, 5, 24);
        QSignalSpy done(&scheduler, &storage::BackupScheduler::finished);
        scheduler.start();                     // one from a moment ago: not due
        CHECK_FALSE(done.wait(300));
        scheduler.backupNow();                 // End of Day asks anyway
        REQUIRE(done.wait(10000));
        CHECK(done.first()[0].toBool());
    }
    CHECK(storage::listBackups(backups).size() == 2);

    // Nothing to back up: reported, not crashed.
    storage::BackupScheduler missing(dir.filePath(u"none.db"_s), backups, 5, 0);
    QSignalSpy failed(&missing, &storage::BackupScheduler::finished);
    missing.backupNow();
    REQUIRE(failed.wait(10000));
    CHECK_FALSE(failed.first()[0].toBool());
}
