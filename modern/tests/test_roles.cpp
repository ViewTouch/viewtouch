#include <catch2/catch_test_macros.hpp>
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "layout/layout.hh"
#include "layout_fixture.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "stationcontroller.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QProcess>
#include <QTest>

#include <functional>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Who may do what: managers run the shift, the owner has the money, taxes,
// labor law, pay and who's a manager, and the maintainer (a code set on the
// store's computer) what can break the store.

namespace {

bool saveStore(PosService &pos, const QVariantMap &changes)
{
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    for (auto it = changes.begin(); it != changes.end(); ++it)
        store[it.key()] = it.value();
    return pos.adminSave(u"store"_s, 0, store);
}

int employeeIndex(PosService &pos, const QString &id)
{
    const QVariantList list = pos.adminRecords(u"employees"_s);
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap()[u"id"_s] == id)
            return i;
    return -1;
}

} // namespace

TEST_CASE("Roles: the store's first manager is its owner", "[roles]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // Morgan, the seed's manager
    CHECK(pos.userRole() == u"owner"_s);
    CHECK(pos.can(u"owner"_s));
    CHECK_FALSE(pos.can(u"maintain"_s));
    CHECK(pos.roleName(u"owner"_s) == u"Owner"_s);
    CHECK(pos.roleName(u"admin"_s) == u"Owner"_s);   // before there were owners
}

TEST_CASE("Roles: a manager runs the shift; money, labor law, pay and managers are the owner's", "[roles]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // the owner adds a manager
    QVariantMap pat = pos.adminNewRecord(u"employees"_s);
    pat[u"name"_s] = u"Pat"_s;
    pat[u"role"_s] = u"manager"_s;
    pat[u"pin"_s] = u"246810"_s;
    REQUIRE(pos.adminSave(u"employees"_s, -1, pat));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"246810"_s));
    CHECK(pos.userRole() == u"manager"_s);
    CHECK_FALSE(pos.can(u"owner"_s));

    // Store Settings: the store's name yes; cards, labor law, tip-outs no.
    CHECK(saveStore(pos, {{u"storeName"_s, u"Pat's Café"_s}}));
    CHECK(pos.storeName() == u"Pat's Café"_s);
    CHECK_FALSE(saveStore(pos, {{u"surchargePercent"_s, 3}}));
    CHECK_FALSE(saveStore(pos, {{u"stripeSecretKey"_s, u"sk_test_new"_s}}));
    CHECK_FALSE(saveStore(pos, {{u"laborState"_s, u"CA"_s}}));
    CHECK_FALSE(saveStore(pos, {{u"tipOuts"_s, u"busser 50 tips"_s}}));
    CHECK_FALSE(saveStore(pos, {{u"encryptBackups"_s, true}}));   // the maintainer's
    // ...shown, not editable, saying whose.
    bool shownLocked = false;
    for (const QVariant &f : pos.adminFields(u"store"_s))
        if (f.toMap()[u"path"_s] == u"surchargePercent"_s)
            shownLocked = f.toMap()[u"readOnly"_s].toBool() && f.toMap()[u"hint"_s].toString().startsWith(u"The owner's"_s);
    CHECK(shownLocked);
    // Taxes, card readers.
    QVariantMap taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    taxes[u"food"_s] = 9.5;
    CHECK_FALSE(pos.adminSave(u"taxes"_s, 0, taxes));

    // Staff: a server's name yes; pay, a new manager, a manager's job no.
    const int sam = employeeIndex(pos, u"sam"_s);
    QVariantMap s = pos.adminRecords(u"employees"_s)[sam].toMap();
    s[u"name"_s] = u"Samuel"_s;
    CHECK(pos.adminSave(u"employees"_s, sam, s));
    s[u"payRate"_s] = s[u"payRate"_s].toDouble() + 2;
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, s));
    s = pos.adminRecords(u"employees"_s)[sam].toMap();
    s[u"role"_s] = u"manager"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, s));
    QVariantMap boss = pos.adminNewRecord(u"employees"_s);
    boss[u"name"_s] = u"Another"_s;
    boss[u"role"_s] = u"manager"_s;
    boss[u"pin"_s] = u"135790"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, -1, boss));
    CHECK_FALSE(pos.adminDelete(u"employees"_s, employeeIndex(pos, u"manager"_s)));   // the owner
    CHECK_FALSE(pos.forgetCustomer(u"anyone"_s));

    // The owner can, and the store keeps an owner.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(saveStore(pos, {{u"surchargePercent"_s, 3}}));
    CHECK(pos.adminSave(u"taxes"_s, 0, taxes));
    s = pos.adminRecords(u"employees"_s)[sam].toMap();
    s[u"role"_s] = u"manager"_s;
    s[u"pin"_s] = u"112233"_s;
    CHECK(pos.adminSave(u"employees"_s, sam, s));
    QVariantMap me = pos.adminRecords(u"employees"_s)[employeeIndex(pos, u"manager"_s)].toMap();
    me[u"role"_s] = u"manager"_s;   // the only owner, stepping down: not without another owner
    CHECK_FALSE(pos.adminSave(u"employees"_s, employeeIndex(pos, u"manager"_s), me));
}

TEST_CASE("Roles: the maintainer, by a code set on the store's computer", "[roles][maintainer]")
{
    PosService pos(test::seedPosData(), nullptr);
    // No code: nobody is the maintainer; the owner can't erase the store.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });
    CHECK_FALSE(pos.factoryReset(u"RESET"_s));
    CHECK(notices.join(u" "_s).contains(u"Factory reset"_s));
    CHECK_FALSE(pos.startPairing(true));   // a standby computer
    CHECK(pos.startPairing(false));        // a new handheld: a manager's
    pos.stopPairing();
    pos.logout();

    pos.shared()->setMaintainerPin(u"86420"_s);
    CHECK(pos.shared()->settings.maintainerPinHash != "86420");   // kept hashed
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).maintainerPinHash
          == pos.shared()->settings.maintainerPinHash);
    REQUIRE(pos.loginWithPin(u"86420"_s));
    CHECK(pos.userName() == u"Maintainer"_s);
    CHECK(pos.can(u"maintain"_s));
    CHECK(pos.can(u"owner"_s));
    CHECK_FALSE(pos.clockIn());   // not staff: no hours
    // Not on the staff list: nobody can remove or demote them.
    for (const QVariant &e : pos.adminRecords(u"employees"_s))
        CHECK(e.toMap()[u"id"_s] != u"maintainer"_s);
    CHECK(saveStore(pos, {{u"encryptBackups"_s, false}, {u"surchargePercent"_s, 2}}));
    CHECK(pos.startPairing(true));
    pos.stopPairing();
    pos.logout();

    // Nobody's PIN can be the maintainer's.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int sam = employeeIndex(pos, u"sam"_s);
    QVariantMap s = pos.adminRecords(u"employees"_s)[sam].toMap();
    s[u"pin"_s] = u"86420"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, s));
    pos.logout();

    // Off: the code no longer logs in.
    pos.shared()->setMaintainerPin({});
    CHECK_FALSE(pos.loginWithPin(u"86420"_s));
}

TEST_CASE("Roles: templates are the owner's, the store's own pages the maintainer's", "[roles][layout]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    const layout::Layout before = *l;
    CHECK(layout::protectedEdit(before, before).isEmpty());
    // A menu page's button moved: anyone who edits pages.
    layout::Layout menu = before;
    for (layout::Page &p : menu.pages)
        if (p.kind == u"items" && !p.zones.isEmpty()) {
            p.zones.first().rect.moveLeft(p.zones.first().rect.x() + 8);
            break;
        }
    CHECK(layout::protectedEdit(before, menu).isEmpty());
    // A template changed: the maintainer's.
    layout::Layout templ = before;
    bool changed = false;
    for (layout::Page &p : templ.pages)
        if (p.kind == u"template" && !p.zones.isEmpty() && !changed) {
            p.zones.first().rect.moveLeft(p.zones.first().rect.x() + 8);
            changed = true;
        }
    REQUIRE(changed);
    CHECK(layout::protectedEdit(before, templ).contains(u"template"_s));
    // A page with a role (the login, the tables...) removed: the maintainer's.
    layout::Layout gone = before;
    const auto it = std::ranges::find_if(gone.pages, [](const layout::Page &p) { return !p.role.isEmpty(); });
    REQUIRE(it != gone.pages.end());
    gone.pages.erase(it);
    bool system = false;
    CHECK_FALSE(layout::protectedEdit(before, gone, &system).isEmpty());
    CHECK(system);
    layout::protectedEdit(before, templ, &system);
    CHECK_FALSE(system);
}

TEST_CASE("Roles: the home screen's maintainer code guards this computer's settings", "[roles][station]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    {
        QFile f(QDir(dir.path()).filePath(u"station.json"_s));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(R"({"role": "terminal", "name": "Register-2"})");
    }
    StationController station(dir.path(), {}, false);
    CHECK_FALSE(station.hasPin());
    QSignalSpy done(&station, &StationController::finished);
    CHECK_FALSE(station.setMaintainerCode(u"12"_s));   // 4 to 8 digits
    REQUIRE(station.setMaintainerCode(u"86420"_s));
    CHECK(station.hasPin());
    CHECK(station.checkPin(u"86420"_s));
    CHECK_FALSE(station.checkPin(u"11111"_s));
    REQUIRE(done.size() == 1);
    CHECK(done[0][0].toString() == u"maintainer"_s);
    CHECK(done[0][1].toBool());
    // Kept hashed, not as digits.
    QFile f(QDir(dir.path()).filePath(u"station.json"_s));
    REQUIRE(f.open(QIODevice::ReadOnly));
    CHECK_FALSE(f.readAll().contains("86420"));
}

TEST_CASE("Roles: the home screen asks for the maintainer's code, and changes it", "[roles][station][ui]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    {
        QFile f(QDir(dir.path()).filePath(u"station.json"_s));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(R"({"role": "kitchen", "name": "Kitchen"})");
    }
    StationController station(dir.path(), {}, false);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"station"_s, QVariant::fromValue(&station)}, {u"kiosk"_s, false},
                                 {u"width"_s, 1280}, {u"height"_s, 800}});
    engine.loadFromModule("ViewTouch", "StationWindow");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    const auto find = [&](const QString &name) { return window->findChild<QQuickItem *>(name); };
    REQUIRE(find(u"tileMaintainer"_s));
    // No code yet: the panel opens; type it twice and save.
    window->setProperty("panel", u"maintainer"_s);
    QTest::qWait(150);
    QQuickItem *save = nullptr;
    std::function<QQuickItem *(QQuickItem *)> walk = [&](QQuickItem *it) -> QQuickItem * {
        if (it->objectName() == u"maintainerSave")
            return it;
        for (QQuickItem *c : it->childItems())
            if (QQuickItem *hit = walk(c))
                return hit;
        return nullptr;
    };
    save = walk(window->contentItem());
    REQUIRE(save);
    QObject *panel = save;
    while (panel && panel->property("code").isNull())
        panel = panel->parent();
    REQUIRE(panel);
    panel->setProperty("code", u"86420"_s);
    panel->setProperty("again", u"86420"_s);
    panel->setProperty("confirming", true);
    QTest::qWait(50);
    REQUIRE(save->isVisible());
    QMetaObject::invokeMethod(save, "clicked");
    QTest::qWait(50);
    CHECK(station.checkPin(u"86420"_s));
}

// Run by the next test in a process of its own: the maintainer's built-in
// profile is set up once per process, and other tests have done it by then.
TEST_CASE("Roles: a staff login first after a start, on its own", "[.firstLoginAlone]")
{
    i18n::install({});
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    pos.shared()->setMaintainerPin(u"86420"_s);
    for (const char d : std::string("1234"))
        pos.pinKey(QString(QChar(d)));
    // As a tap does: in the language of whoever is logged in, which the
    // translator asks the session for (it used to wait for itself here).
    bool ok = false;
    pos.invoke(u"login"_s, {}, [&ok](const QVariant &r) { ok = r.toBool(); });
    CHECK(ok);
    CHECK(pos.userName() == u"Morgan (Manager)"_s);
    pos.logout();
    REQUIRE(pos.loginWithPin(u"86420"_s));
    CHECK(pos.userName() == u"Maintainer"_s);
}

TEST_CASE("Roles: the first staff login after a start doesn't freeze the screen", "[roles]")
{
    QProcess p;
    p.start(QCoreApplication::applicationFilePath(), {u"[.firstLoginAlone]"_s});
    REQUIRE(p.waitForStarted());
    const bool finished = p.waitForFinished(60'000);
    if (!finished)
        p.kill();
    INFO(p.readAllStandardOutput().right(2000).toStdString());
    REQUIRE(finished);   // it froze for good
    CHECK(p.exitCode() == 0);
}

TEST_CASE("Station: the first-time setup keeps its answers while Wi-Fi is open", "[station][ui]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    StationController station(dir.path(), {}, false);   // a new computer: the setup
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"station"_s, QVariant::fromValue(&station)}, {u"kiosk"_s, false},
                                 {u"width"_s, 1280}, {u"height"_s, 800}});
    engine.loadFromModule("ViewTouch", "StationWindow");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    REQUIRE(window->property("panel").toString() == u"setup"_s);
    auto wizard = [&] { return window->findChild<QQuickItem *>(u"setupWizard"_s); };
    REQUIRE(wizard());
    wizard()->setProperty("role", u"store"_s);
    wizard()->setProperty("step", 3);   // the network step
    // Its name, as typed.
    QQuickItem *name = nullptr;
    std::function<QQuickItem *(QQuickItem *)> field = [&](QQuickItem *it) -> QQuickItem * {
        if (it->inherits("QQuickTextInput") || it->inherits("QQuickTextField"))
            return it;
        for (QQuickItem *c : it->childItems())
            if (QQuickItem *hit = field(c))
                return hit;
        return nullptr;
    };
    name = field(wizard());
    REQUIRE(name);
    name->setProperty("text", u"Front-Counter"_s);
    // Wi-Fi…, then ‹ Back.
    window->setProperty("panel", u"network"_s);
    QTest::qWait(100);
    CHECK_FALSE(wizard());
    window->setProperty("panel", u"setup"_s);
    QTest::qWait(100);
    REQUIRE(wizard());
    CHECK(wizard()->property("step").toInt() == 3);
    CHECK(wizard()->property("role").toString() == u"store"_s);
    name = field(wizard());
    REQUIRE(name);
    CHECK(name->property("text").toString() == u"Front-Counter"_s);
}

TEST_CASE("Station: every tile of the main register's home screen fits a 1280 x 800 screen", "[station][ui]")
{
    QTemporaryDir dir;
    QFile f(QDir(dir.path()).filePath(u"station.json"_s));
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"({"role": "store", "name": "Main Register"})");
    f.close();
    StationController station(dir.path(), {}, false);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"station"_s, QVariant::fromValue(&station)}, {u"kiosk"_s, false},
                                 {u"width"_s, 1280}, {u"height"_s, 800}});
    engine.loadFromModule("ViewTouch", "StationWindow");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(300);
    // Inside what's shown: the page clips at its margins, before the window's edge.
    auto *page = window->findChild<QQuickItem *>(u"stationPage"_s);
    REQUIRE(page);
    const QRectF shown = page->mapRectToScene(QRectF(0, 0, page->width(), page->height()));
    CHECK(page->property("contentHeight").toDouble() <= page->height() + 1);   // nothing to scroll to
    int tiles = 0;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *it) {
        // The Tile inline component (StationWindow_QMLTYPE_..._Tile... as its class).
        if (it->isVisible() && QByteArray(it->metaObject()->className()).contains("Tile")) {
            ++tiles;
            const QRectF r = it->mapRectToScene(QRectF(0, 0, it->width(), it->height()));
            INFO(it->property("label").toString().toStdString());
            CHECK(r.bottom() <= shown.bottom() + 0.5);
            CHECK(r.right() <= shown.right() + 0.5);
            // Its icon, name and detail inside it.
            for (QQuickItem *column : it->childItems()) {
                if (!QByteArray(column->metaObject()->className()).contains("ColumnLayout"))
                    continue;
                CHECK(column->implicitHeight() <= it->height() + 0.5);
            }
        }
        for (QQuickItem *c : it->childItems())
            walk(c);
    };
    walk(window->contentItem());
    CHECK(tiles >= 10);
}

TEST_CASE("Station: status at a glance, backups to restore, a locked power button, a turned screen", "[station]")
{
    QTemporaryDir dir;
    {
        QFile f(QDir(dir.path()).filePath(u"station.json"_s));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(R"({"role": "store", "name": "Main Register"})");
    }
    StationController station(dir.path(), {}, false);
    // No backup yet: said, for a main register.
    station.refreshStatus();
    CHECK(station.status()[u"lastBackupAt"_s].toLongLong() == 0);
    CHECK(station.status()[u"problems"_s].toStringList().contains(u"No backup yet"_s));
    CHECK(station.status()[u"diskFreePercent"_s].toInt() > 0);

    // A backup made last night, and an encrypted one: listed, newest first.
    QDir(dir.path()).mkpath(u"backups"_s);
    const QString older = QDir(dir.path()).filePath(u"backups/viewtouch-20261001-020000.vtbak"_s);
    const QString newer = QDir(dir.path()).filePath(u"backups/viewtouch-20261029-020000.db"_s);
    for (const QString &path : {older, newer}) {
        QFile f(path);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("x");
    }
    {
        QFile f(older);
        REQUIRE(f.open(QIODevice::ReadWrite));
        REQUIRE(f.setFileTime(QDateTime::currentDateTime().addDays(-20), QFileDevice::FileModificationTime));
    }
    const QVariantList list = station.backups();
    REQUIRE(list.size() == 2);
    CHECK(list[0].toMap()[u"path"_s] == newer);
    CHECK_FALSE(list[0].toMap()[u"encrypted"_s].toBool());
    CHECK(list[1].toMap()[u"encrypted"_s].toBool());
    CHECK(list[0].toMap()[u"where"_s] == u"This computer"_s);
    station.refreshStatus();
    CHECK(station.status()[u"lastBackupAt"_s].toLongLong() > 0);
    CHECK_FALSE(station.status()[u"problems"_s].toStringList().contains(u"No backup yet"_s));

    // Only a backup from the list is restored, never a path from elsewhere.
    QSignalSpy finished(&station, &StationController::finished);
    station.restoreBackup(u"/etc/passwd"_s);
    REQUIRE(finished.size() == 1);
    CHECK(finished[0][0] == u"restore"_s);
    CHECK_FALSE(finished[0][1].toBool());

    // Restart and Shut Down with the code; the screen turned: kept.
    station.setLockPower(true);
    station.setRotation(90);
    station.setRotation(45);   // not a way to turn a screen: ignored
    StationController again(dir.path(), {}, false);
    CHECK(again.lockPower());
    CHECK(again.rotation() == 90);
}

TEST_CASE("Station: a backup made, the store changed, the backup restored by touch", "[station][restore]")
{
    QTemporaryDir dir;
    {
        QFile f(QDir(dir.path()).filePath(u"station.json"_s));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(R"({"role": "store", "name": "Main Register"})");
    }
    const QString db = QDir(dir.path()).filePath(u"viewtouch.db"_s);
    // A store as ViewTouch makes one (its pages too), with two months of sales.
    {
        QProcess demo;
        demo.start(QString::fromLocal8Bit(VTM_PROGRAM), {u"--data-dir"_s, dir.path(), u"--demo-data"_s});
        REQUIRE(demo.waitForFinished(120000));
        REQUIRE(demo.exitCode() == 0);
    }
    const auto named = [&](const char *name) {
        storage::PosStore store(db);
        REQUIRE(store.open());
        auto data = store.load();
        REQUIRE(data);
        data->settings.storeName = name;
        storage::AsyncWriter writer(db);
        storage::SqlPosSink(writer).saveSettings(data->settings);
        writer.flush();
    };
    const auto storeName = [&] {
        storage::PosStore store(db);
        REQUIRE(store.open());
        return QString::fromStdString(store.load()->settings.storeName);
    };
    named("Before");
    qputenv("VTM_STATION_VTMODERN", VTM_PROGRAM);
    StationController station(dir.path(), {}, false);
    QSignalSpy finished(&station, &StationController::finished);
    station.backupNow();   // ViewTouch itself, as Back Up Now runs it
    REQUIRE(finished.wait(60000));
    INFO(finished.last()[2].toString().toStdString() << " / " << station.output().toStdString());
    REQUIRE(finished.last()[1].toBool());
    const QVariantList backups = station.backups();
    REQUIRE(backups.size() == 1);

    named("After");
    REQUIRE(storeName() == u"After"_s);
    finished.clear();
    station.restoreBackup(backups[0].toMap()[u"path"_s].toString());
    REQUIRE(finished.wait(60000));
    INFO(finished.last()[2].toString().toStdString());
    REQUIRE(finished.last()[0] == u"restore"_s);
    REQUIRE(finished.last()[1].toBool());
    CHECK(storeName() == u"Before"_s);
    // What was there kept beside it: undone the same way.
    CHECK(QDir(dir.path()).entryList({u"viewtouch.db.before-restore-*"_s}).size() == 1);
    qunsetenv("VTM_STATION_VTMODERN");
}

TEST_CASE("Station: the computer's commands go to the helper as they should", "[station][helper]")
{
    QTemporaryDir dir;
    // A pretend helper: notes what it's asked, says the last line back; "fail" fails.
    const QString log = dir.filePath(u"asked.txt"_s);
    const QString helper = dir.filePath(u"helper.sh"_s);
    {
        QFile f(helper);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(("#!/bin/sh\necho \"$*\" >> '" + log.toUtf8() + "'\n"
                 "[ \"$2\" = fail ] && { echo \"Wrong password.\"; exit 1; }\n"
                 "echo \"Done: $*\"\n"));
        f.setPermissions(f.permissions() | QFile::ExeOwner);
    }
    qputenv("VTM_STATION_HELPER", helper.toLocal8Bit());
    StationController station(dir.path(), {}, false);
    CHECK(station.canManage());
    QSignalSpy finished(&station, &StationController::finished);
    const auto asked = [&] {
        QFile f(log);
        REQUIRE(f.open(QIODevice::ReadOnly));
        return QString::fromUtf8(f.readAll()).trimmed().split(u'\n').last();
    };
    const auto done = [&](const QString &action) {
        REQUIRE(finished.wait(10000));
        CHECK(finished.last()[0] == action);
        return finished.last()[1].toBool();
    };

    // Brightness: a percent, never quite dark, never over 100.
    station.setBrightness(150);
    CHECK(done(u"brightness"_s));
    CHECK(asked() == u"brightness 100"_s);
    station.setBrightness(0);
    CHECK(done(u"brightness"_s));
    CHECK(asked() == u"brightness 5"_s);
    station.usbOpen();
    CHECK(done(u"usb-open"_s));
    CHECK(asked() == u"usb-open"_s);
    station.saveLogs();
    CHECK(done(u"logs-usb"_s));
    CHECK(asked() == u"logs-usb"_s);
    CHECK(finished.last()[2] == u"Done: logs-usb"_s);   // what it said, for the screen

    // One at a time: a second while the first runs is refused, not lost silently.
    station.setBrightness(50);
    station.setBrightness(60);
    REQUIRE(finished.size() >= 5);
    CHECK_FALSE(finished.last()[1].toBool());
    CHECK(finished.last()[2] == u"Wait for the last one to finish."_s);
    CHECK(done(u"brightness"_s));
    CHECK(asked() == u"brightness 50"_s);

    station.setRemoteSupport(true);
    CHECK(done(u"ssh"_s));
    CHECK(asked() == u"ssh on"_s);

    // A failure says why.
    station.connectWifi(u"fail"_s, u"secret"_s);
    CHECK_FALSE(done(u"wifi"_s));
    CHECK(finished.last()[2] == u"Wrong password."_s);
    qunsetenv("VTM_STATION_HELPER");
}

TEST_CASE("Station: a new ViewTouch looked for twice a day, installed at night only when allowed", "[station][helper][updates]")
{
    QTemporaryDir dir;
    const QString log = dir.filePath(u"asked.txt"_s);
    const QString helper = dir.filePath(u"helper.sh"_s);
    {
        QFile f(helper);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(("#!/bin/sh\necho \"$*\" >> '" + log.toUtf8() + "'\n"
                 "case \"$1\" in online-check) echo 'READY 9.9 0.14' ;; online-update) echo 'INSTALLED 9.9' ;; esac\n"));
        f.setPermissions(f.permissions() | QFile::ExeOwner);
    }
    qputenv("VTM_STATION_HELPER", helper.toLocal8Bit());
    StationController station(dir.path(), {}, false);
    for (int i = 0; i < 300 && !station.network()[u"online"_s].toBool(); ++i)
        QTest::qWait(10);
    REQUIRE(station.network()[u"online"_s].toBool());
    QSignalSpy finished(&station, &StationController::finished);
    const auto asked = [&] {
        QFile f(log);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed().split(u'\n') : QStringList{};
    };
    // Midday: looked for, found; Updates says so.
    const QDateTime noon(QDate(2026, 11, 2), QTime(12, 0));
    station.updatesTick(noon);
    REQUIRE(finished.wait(10000));
    CHECK(station.newVersion() == u"9.9"_s);
    CHECK(asked() == QStringList{u"online-check"_s});
    // Not again for twelve hours; at night, not installed unless allowed.
    station.updatesTick(noon.addSecs(3600));
    const QDateTime night(QDate(2026, 11, 3), QTime(3, 30));
    station.updatesTick(night);
    REQUIRE(finished.wait(10000));
    CHECK(asked() == (QStringList{u"online-check"_s, u"online-check"_s}));   // only looked again
    // Allowed (kept): installed at night, once.
    station.setAutoUpdate(true);
    CHECK(StationController(dir.path(), {}, false).autoUpdate());
    station.updatesTick(night.addSecs(600));
    REQUIRE(finished.wait(10000));
    CHECK(asked().last() == u"online-update"_s);
    CHECK(station.newVersion().isEmpty());
    // Not by day.
    REQUIRE(station.newVersion().isEmpty());
    qunsetenv("VTM_STATION_HELPER");
}

// The manual's pictures of the home screen and the first-time setup
// (hidden: run with "[manual]" and VTM_SHOTS).
TEST_CASE("Manual: the home screen and the first-time setup", "[.manual]")
{
    const QByteArray shots = qgetenv("VTM_SHOTS");
    const auto open = [](StationController &station, QQmlApplicationEngine &engine) {
        engine.setInitialProperties({{u"station"_s, QVariant::fromValue(&station)}, {u"kiosk"_s, false},
                                     {u"width"_s, 1280}, {u"height"_s, 800}});
        engine.loadFromModule("ViewTouch", "StationWindow");
        REQUIRE_FALSE(engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        REQUIRE(window);
        REQUIRE(QTest::qWaitForWindowExposed(window));
        QTest::qWait(400);
        return window;
    };
    {
        QTemporaryDir dir;
        QFile f(QDir(dir.path()).filePath(u"station.json"_s));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(R"({"role": "store", "name": "Main Register"})");
        f.close();
        StationController station(dir.path(), {}, false);
        QQmlApplicationEngine engine;
        QQuickWindow *window = open(station, engine);
        CHECK(window->findChild<QQuickItem *>(u"tileMaintainer"_s));
        if (!shots.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/m147-home-screen.png"_s);
        // Its panels: restoring a backup, the screen, about (what to look at).
        QDir(dir.path()).mkpath(u"backups"_s);
        for (const char *name : {"viewtouch-20261029-020000.db", "viewtouch-20261028-230000.db"}) {
            QFile b(QDir(dir.path()).filePath(u"backups/"_s + QString::fromLatin1(name)));
            REQUIRE(b.open(QIODevice::WriteOnly));
            b.write(QByteArray(1'500'000, 'x'));
        }
        for (const auto &[panel, file] : {std::pair{"restore", "m190-station-restore"}, {"display", "m191-station-display"},
                                          {"about", "m192-station-about"}}) {
            window->setProperty("panel", QString::fromLatin1(panel));
            QTest::qWait(300);
            if (!shots.isEmpty())
                window->grabWindow().save(QString::fromLocal8Bit(shots) + u'/' + QString::fromLatin1(file) + u".png"_s);
        }
    }
    {
        QTemporaryDir dir;
        StationController station(dir.path(), {}, false);   // a new computer
        QQmlApplicationEngine engine;
        QQuickWindow *window = open(station, engine);
        CHECK(window->property("panel").toString() == u"setup"_s);
        if (!shots.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/m148-first-setup.png"_s);
    }
}
