#include <catch2/catch_test_macros.hpp>

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
