#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>
#include <QQmlApplicationEngine>
#include <functional>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;

// Every shipped page must render without a single QML warning, running and
// in the editor. Catches broken bindings in widgets nobody tapped yet.

namespace {

QStringList g_warnings;

void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        g_warnings << msg;
}

} // namespace

TEST_CASE("Every seed page renders cleanly, running and editing", "[ui][pages]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    QStringList pageIds;
    for (const auto &p : layout->pages)
        pageIds << p.id;

    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.addItem(u"classic-burger"_s);   // give order widgets something to show

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    g_warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));

    for (const QString &id : pageIds) {
        INFO(id.toStdString());
        CHECK(c.showPage(id));
        QTest::qWait(20);
    }
    c.enterEditMode();
    for (const QString &id : pageIds) {
        INFO("edit " << id.toStdString());
        CHECK(c.showPage(id));
        QTest::qWait(20);
    }
    c.leaveEditMode(false);
    QTest::qWait(20);
    qInstallMessageHandler(previous);
    CHECK(g_warnings == QStringList{});
}

TEST_CASE("A food truck's screens render cleanly with orders waiting", "[ui][pages][truck]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    test::RecordingSink sink;
    app::PosData data = test::seedPosData();
    data.settings.serviceMode = "foodTruck";
    app::PosService pos(std::move(data), &sink);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.serviceInfo()[u"counter"_s].toBool());

    // One order called, one being made with its name and pager on the check.
    REQUIRE(pos.addItem(u"classic-burger"_s));
    REQUIRE(pos.setPager(4));
    REQUIRE(pos.sendOrder());
    const qint64 first = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();
    REQUIRE(pos.orderReady(first));
    REQUIRE(pos.addItem(u"classic-burger"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Maria"_s}}));
    REQUIRE(pos.setPager(12));
    REQUIRE(pos.sendOrder());

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    g_warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(50);   // the order screen, the check's + Name and + Pager
    REQUIRE(c.showPage(u"truck-orders"_s));
    QTest::qWait(50);
    CHECK(pos.truckOrders().size() == 2);
    c.setFormFactorOverride(u"phone"_s);
    QTest::qWait(50);
    qInstallMessageHandler(previous);
    CHECK(g_warnings == QStringList{});
}

TEST_CASE("Now Serving: the board on a TV, the Orders button at a counter", "[ui][pages][truck]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    test::RecordingSink sink;
    app::PosData data = test::seedPosData();
    data.settings.orderNumbers = true;   // a restaurant at the counter
    app::PosService pos(std::move(data), &sink);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);

    // The order page: Orders in Tables' place.
    const auto rule = [&](const QString &zone) {
        const vt::layout::Page *page = c.activeLayout().page(u"order-template"_s);
        REQUIRE(page);
        const vt::layout::Zone *z = page->zone(zone);
        REQUIRE(z);
        return z->extra.value(u"showWhen").toObject();
    };
    CHECK(c.ruleShows(rule(u"flow-orders"_s)));
    CHECK_FALSE(c.ruleShows(rule(u"flow-tables"_s)));

    // Two orders: Ana's ready, Lee's being made.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"classic-burger"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Ana"_s}}));
    REQUIRE(pos.sendOrder());
    const qint64 ana = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();
    REQUIRE(pos.orderReady(ana));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"classic-burger"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Lee"_s}}));
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();
    pos.logout();

    c.setFormFactorOverride(u"nowServing"_s);
    CHECK(c.nowServingScreen());
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1920}, {u"height"_s, 1080}});
    g_warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    auto *board = window->findChild<QQuickItem *>(u"nowServingBoard"_s);
    REQUIRE(board);
    CHECK(board->isVisible());
    const std::function<QQuickItem *(QQuickItem *, const QString &)> named = [&](QQuickItem *r, const QString &name) -> QQuickItem * {
        for (QQuickItem *i : r->childItems()) {
            if (i->objectName() == name) return i;
            if (QQuickItem *h = named(i, name)) return h;
        }
        return nullptr;
    };
    QQuickItem *ready = named(board, u"boardReady"_s);
    QQuickItem *making = named(board, u"boardPreparing"_s);
    REQUIRE(ready);
    REQUIRE(making);
    CHECK(ready->property("count").toInt() == 1);
    CHECK(making->property("count").toInt() == 1);
    if (const QByteArray shots = qgetenv("VTM_SHOTS"); !shots.isEmpty()) {
        QTest::qWait(4000);   // after the just-called flash
        window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/m193-now-serving-board.png"_s);
    }

    // A rush: more than fit, a page at a time, the pages said.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    for (int i = 0; i < 24; ++i) {
        REQUIRE(pos.startCheck(core::CheckType::Quick));
        REQUIRE(pos.addItem(u"soda"_s));
        REQUIRE(pos.sendOrder());
        pos.releaseCheck();
    }
    pos.logout();
    QTest::qWait(100);
    QQuickItem *pages = named(board, u"boardPreparingPages"_s);
    REQUIRE(pages);
    CHECK(pages->isVisible());
    CHECK(making == named(board, u"boardPreparing"_s));   // the same column, not a new one
    CHECK(making->property("count").toInt() == 25);
    CHECK(pages->property("text").toString().startsWith(u"1 of "_s));
    // Turned after a few seconds, and kept while orders keep coming.
    QTest::qWait(6500);
    CHECK(pages->property("text").toString().startsWith(u"2 of "_s));
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();
    pos.logout();
    QTest::qWait(100);
    CHECK(making->property("count").toInt() == 26);
    CHECK(pages->property("text").toString().startsWith(u"2 of "_s));

    // Every order picked up: the store's messages; an hour of that: dark,
    // and the next order lights it again.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    for (const QVariant &o : pos.truckOrders())
        REQUIRE(pos.orderPickedUp(o.toMap()[u"id"_s].toLongLong()));
    pos.logout();
    QTest::qWait(100);
    QQuickItem *idle = named(board, u"boardIdle"_s);
    QQuickItem *dark = named(board, u"boardDark"_s);
    REQUIRE(idle);
    REQUIRE(dark);
    CHECK(idle->isVisible());
    CHECK_FALSE(dark->isVisible());
    board->setProperty("lastBusy", QDateTime::currentMSecsSinceEpoch() - 2 * 3'600'000.0);
    board->setProperty("dark", true);   // (the minute's check, now)
    CHECK(dark->isVisible());
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();
    pos.logout();
    QTest::qWait(100);
    CHECK_FALSE(dark->isVisible());
    CHECK_FALSE(idle->isVisible());

    // Someone logs in: the pages; out again: the board.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QTest::qWait(50);
    CHECK_FALSE(board->isVisible());
    pos.logout();
    QTest::qWait(50);
    CHECK(board->isVisible());
    qInstallMessageHandler(previous);
    CHECK(g_warnings == QStringList{});
}

TEST_CASE("Now Serving: a register's own screen facing the guests can be the board", "[ui][truck][board]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    app::PosData data = test::seedPosData();
    data.settings.orderNumbers = true;
    app::PosService pos(std::move(data), nullptr);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // Manager -> Terminals: this register's guest screen, the board.
    QVariantList terminals = pos.adminRecords(u"terminals"_s);
    int mine = -1;
    for (int i = 0; i < terminals.size(); ++i)
        if (terminals[i].toMap()[u"name"_s].toString() == pos.terminalName())
            mine = i;
    QVariantMap me = mine >= 0 ? terminals[mine].toMap() : QVariantMap{{u"name"_s, pos.terminalName()}};
    me[u"guestScreen"_s] = u"wall"_s;
    CHECK_FALSE(pos.adminSave(u"terminals"_s, mine, me));   // not a choice
    me[u"guestScreen"_s] = u"board"_s;
    REQUIRE(pos.adminSave(u"terminals"_s, mine, me));
    CHECK(pos.terminalGuestScreen() == u"board"_s);
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)}, {u"customerDisplay"_s, true},
                                 {u"width"_s, 2560}, {u"height"_s, 800}});
    g_warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    const std::function<QQuickItem *(QQuickItem *, const QString &)> named = [&](QQuickItem *r, const QString &name) -> QQuickItem * {
        for (QQuickItem *i : r->childItems()) {
            if (i->objectName() == name) return i;
            if (QQuickItem *h = named(i, name)) return h;
        }
        return nullptr;
    };
    QQuickItem *making = named(window->contentItem(), u"boardPreparing"_s);
    REQUIRE(making);
    CHECK(making->isVisible());
    CHECK(making->property("count").toInt() == 1);
    CHECK_FALSE(named(window->contentItem(), u"nowServing"_s));   // not the order display's
    qInstallMessageHandler(previous);
    CHECK(g_warnings == QStringList{});
}

TEST_CASE("Now Serving: the chime is a real sound file", "[ui][truck][board]")
{
    const QString file = LayoutController::chimeFile();
    QFile f(file);
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QByteArray wav = f.readAll();
    REQUIRE(wav.size() > 20000);
    CHECK(wav.startsWith("RIFF"));
    CHECK(wav.mid(8, 8) == "WAVEfmt ");
    const auto u32 = [&](int at) { return quint32(quint8(wav[at])) | quint32(quint8(wav[at + 1])) << 8
                                          | quint32(quint8(wav[at + 2])) << 16 | quint32(quint8(wav[at + 3])) << 24; };
    CHECK(u32(4) == quint32(wav.size() - 8));      // RIFF size
    CHECK(u32(24) == 22050u);                      // samples a second
    CHECK(u32(40) == quint32(wav.size() - 44));    // its sound, all there
}

TEST_CASE("Counter service: an order page edited before 0.15 still gets Orders in Tables' place", "[ui][pages][truck][upgrade]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    // As it was saved then: Orders for a food truck only, Tables for restaurants.
    vt::layout::Page *page = layout->page(u"order-template"_s);
    REQUIRE(page);
    for (vt::layout::Zone &z : page->zones) {
        if (z.id == u"flow-orders")
            z.extra.insert(u"showWhen"_s, QJsonObject{{u"service"_s, u"foodTruck"_s}});
        if (z.id == u"flow-tables")
            z.extra.insert(u"showWhen"_s, QJsonObject{{u"service"_s, u"restaurant"_s}});
    }
    app::PosData data = test::seedPosData();
    data.settings.orderNumbers = true;
    app::PosService pos(std::move(data), nullptr);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(c.showPage(u"order-template"_s));
    const auto shows = [&](const QString &zone) {
        ZoneModel *m = c.zones();
        for (int i = 0; i < m->rowCount(); ++i) {
            const QModelIndex at = m->index(i);
            if (m->data(at, ZoneModel::ZoneIdRole).toString() == zone)
                return c.ruleShows(QJsonObject::fromVariantMap(m->data(at, ZoneModel::ShowWhenRole).toMap()));
        }
        FAIL("no zone " << zone.toStdString());
        return false;
    };
    CHECK(shows(u"flow-orders"_s));
    CHECK_FALSE(shows(u"flow-tables"_s));

    // The setting off: Tables again, no Orders.
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"orderNumbers"_s] = false;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK_FALSE(shows(u"flow-orders"_s));
    CHECK(shows(u"flow-tables"_s));
}

namespace {

int countVisible(QQuickItem *root, const char *classPrefix)
{
    int n = 0;
    for (QQuickItem *item : root->childItems()) {
        if (!item->isVisible())
            continue;
        if (QByteArray(item->metaObject()->className()).startsWith(classPrefix))
            ++n;
        n += countVisible(item, classPrefix);
    }
    return n;
}

} // namespace

TEST_CASE("Admin screen: touching a record opens its form", "[ui][pages]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(c.showPage(u"admin-menu"_s));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(50);
    CHECK(countVisible(window->contentItem(), "FieldEditor") == 0);

    // The list's first row, where it is drawn.
    // (zones are drawn by a Repeater: looked for down the drawn items)
    const std::function<QQuickItem *(QQuickItem *)> named = [&](QQuickItem *r) -> QQuickItem * {
        for (QQuickItem *i : r->childItems()) {
            if (i->objectName() == u"adminList"_s) return i;
            if (QQuickItem *h = named(i)) return h;
        }
        return nullptr;
    };
    QQuickItem *list = named(window->contentItem());
    REQUIRE(list);
    QQuickItem *row = nullptr;
    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0));
    REQUIRE(row);
    QTest::mouseClick(window, Qt::LeftButton, {}, row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
    QTest::qWait(50);
    CHECK(countVisible(window->contentItem(), "FieldEditor") == 31);   // menu item fields

    if (const QByteArray dir = qgetenv("VTM_SHOTS"); !dir.isEmpty())
        window->grabWindow().save(QString::fromLocal8Bit(dir) + u"/5-admin-menu.png"_s);
}

TEST_CASE("Kitchen display shows sent orders; touching one bumps it", "[ui][pages]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);

    // Two orders: a takeout for a named customer, then a table.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.setCustomer({{u"name"_s, u"Ana Ruiz"_s}, {u"phone"_s, u"555-0100"_s}, {u"note"_s, u"Extra napkins"_s}});
    pos.addItem(u"classic-burger"_s);
    pos.addItem(u"medium-rare"_s);
    pos.addItem(u"fries"_s);
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();
    pos.selectTable(u"T4"_s);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"pancakes"_s);
    REQUIRE(pos.sendOrder());
    pos.logout();                                   // kitchen screens run logged out

    REQUIRE(c.showPage(u"kitchen"_s));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(50);
    const QByteArray shots = qgetenv("VTM_SHOTS");
    if (!shots.isEmpty())
        window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/6-kitchen.png"_s);

    REQUIRE(pos.kitchenTickets().size() == 2);
    auto *surface = window->findChild<QQuickItem *>(u"pageSurface"_s);
    REQUIRE(surface);
    // First card (oldest: the takeout) sits at the top left.
    QTest::mouseClick(window, Qt::LeftButton, {}, surface->mapToScene(QPointF(180, 200)).toPoint());
    QTest::qWait(30);
    REQUIRE(pos.kitchenTickets().size() == 1);
    CHECK(pos.kitchenTickets()[0].toMap()[u"label"_s].toString() == u"T4"_s);

    // The customer page for the takeout.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.openCheck(1));
    REQUIRE(c.showPage(u"customer"_s));
    QTest::qWait(50);
    if (!shots.isEmpty())
        window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/7-customer.png"_s);
}

TEST_CASE("Customer details typed then Continue are kept", "[ui][pages]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    app::PosService pos(test::seedPosData(), nullptr);
    LayoutController c(std::move(*layout));
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1111"_s));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));

    c.activate(u"delivery"_s);                     // start a delivery
    REQUIRE(c.jumpTo(u"customer"_s));              // the full form page still works
    QTest::qWait(50);
    auto *surface = window->findChild<QQuickItem *>(u"pageSurface"_s);
    REQUIRE(surface);
    // Name field: the first text field in the form (wherever the zoom puts it).
    std::function<QQuickItem *(QQuickItem *)> firstField = [&](QQuickItem *item) -> QQuickItem * {
        if (item->inherits("QQuickTextField") && item->isVisible())
            return item;
        for (QQuickItem *child : item->childItems())
            if (QQuickItem *found = firstField(child))
                return found;
        return nullptr;
    };
    QQuickItem *name = firstField(surface);
    REQUIRE(name);
    QTest::mouseClick(window, Qt::LeftButton, {},
                      name->mapToScene(QPointF(name->width() / 2, name->height() / 2)).toPoint());
    for (char ch : std::string("Lee"))
        QTest::keyClick(window, ch);
    c.activate(u"menu"_s);                          // Continue at once, no Save button
    QTest::qWait(50);
    CHECK(c.pageId() == u"index-lunch"_s);
    CHECK(pos.checkInfo()[u"customer"_s].toMap()[u"name"_s].toString() == u"Lee"_s);
}

TEST_CASE("F1 opens the editor only for someone allowed to edit pages", "[ui][pages][security]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    app::PosService pos(test::seedPosData(), nullptr);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                 {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));

    // Nobody logged in, on the login page.
    REQUIRE(c.pageId() == u"login"_s);
    QTest::keyClick(window, Qt::Key_F1);
    CHECK_FALSE(c.editing());
    CHECK(c.statusText().contains(u"manager"_s));

    // A server may not either.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    QTest::keyClick(window, Qt::Key_F1);
    CHECK_FALSE(c.editing());

    // A manager may.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QTest::keyClick(window, Qt::Key_F1);
    CHECK(c.editing());
}

TEST_CASE("Logging out closes the editor", "[pages][security]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    app::PosService pos(test::seedPosData(), nullptr);
    LayoutController c(std::move(*layout));
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(c.requestEditMode());
    c.editor()->addZone(u"button"_s);   // unsaved

    pos.logout();
    CHECK_FALSE(c.editing());
    CHECK(c.pageId() == u"login"_s);
    CHECK(c.statusText().contains(u"Edit mode closed"_s));
}

// --- an audit of every page's words (hidden: run with "[.audit]") ------------------------------
//
// VTM_AUDIT=<file> QT_QPA_PLATFORM=offscreen build/tests/vtm_tests "[.audit]"
// Every page at several screen sizes, in Classic and Modern, in English and
// the longer languages: words past their button's edge, cut short with "…",
// or off the screen. One line each, with where it was seen.

#include "app/i18n.hh"

#include <QFile>
#include <QJsonDocument>
#include <map>
#include <set>

namespace {

struct Finding {
    QString kind;   // "outside", "cut", "offscreen"
    QString page;
    QString text;
};

bool isText(const QQuickItem *i)
{
    return QByteArray(i->metaObject()->className()).startsWith("QQuickText")
           && !QByteArray(i->metaObject()->className()).startsWith("QQuickTextInput")
           && !QByteArray(i->metaObject()->className()).startsWith("QQuickTextEdit");
}

bool scrolls(const QQuickItem *i)
{
    for (const QQuickItem *p = i->parentItem(); p; p = p->parentItem()) {
        const QByteArray c = p->metaObject()->className();
        if (c.contains("Flickable") || c.contains("ListView") || c.contains("GridView") || c.contains("PathView"))
            return true;
    }
    return false;
}

qreal opacityOf(const QQuickItem *i)
{
    qreal o = 1;
    for (; i; i = i->parentItem())
        o *= i->opacity();
    return o;
}

// The box a text belongs in: the nearest filled or bordered rectangle (a button, a card).
const QQuickItem *boxOf(const QQuickItem *t)
{
    for (const QQuickItem *p = t->parentItem(); p; p = p->parentItem()) {
        if (p->width() <= 0 || p->height() <= 0)
            continue;
        if (QByteArray(p->metaObject()->className()).startsWith("QQuickRectangle")) {
            const QColor fill = p->property("color").value<QColor>();
            const QObject *border = qvariant_cast<QObject *>(p->property("border"));
            const bool edged = border && border->property("width").toReal() > 0;
            if (fill.alpha() > 0 || edged)
                return p;
        }
        if (QByteArray(p->metaObject()->className()).contains("Window"))
            return nullptr;
    }
    return nullptr;
}

void auditTexts(QQuickItem *root, QQuickWindow *window, const QString &page, std::vector<Finding> &out)
{
    for (QQuickItem *i : root->childItems()) {
        if (!i->isVisible() || opacityOf(i) < 0.05)
            continue;
        if (isText(i) && !i->property("text").toString().trimmed().isEmpty() && i->width() > 0 && i->height() > 0) {
            const QString text = i->property("text").toString().simplified().left(80);
            const qreal cw = i->property("contentWidth").toReal(), ch = i->property("contentHeight").toReal();
            if (i->property("truncated").toBool()) {
                out.push_back({u"cut"_s, page, text});
            } else {
                // Where the words are painted (the text box, moved by its alignment).
                const int h = i->property("horizontalAlignment").toInt(), v = i->property("verticalAlignment").toInt();
                const qreal x = h == Qt::AlignHCenter ? (i->width() - cw) / 2 : h == Qt::AlignRight ? i->width() - cw : 0;
                const qreal y = v == Qt::AlignVCenter ? (i->height() - ch) / 2 : v == Qt::AlignBottom ? i->height() - ch : 0;
                const QRectF painted = i->mapRectToScene(QRectF(x, y, cw, ch)).adjusted(1, 1, -1, -1);
                if (const QQuickItem *box = boxOf(i)) {
                    const QRectF edge = box->mapRectToScene(QRectF(0, 0, box->width(), box->height())).adjusted(-2, -2, 2, 2);
                    // In a scrolled list only sideways (rows go past the bottom while scrolling).
                    const bool sideways = painted.left() < edge.left() || painted.right() > edge.right();
                    const bool upright = painted.top() < edge.top() || painted.bottom() > edge.bottom();
                    if (sideways || (upright && !scrolls(i)))
                        out.push_back({u"outside"_s, page, text});
                }
                if (!scrolls(i) && (painted.left() < -2 || painted.right() > window->width() + 2))
                    out.push_back({u"offscreen"_s, page, text});
            }
        }
        auditTexts(i, window, page, out);
    }
}

} // namespace

TEST_CASE("Audit: words inside their buttons on every page, size, style and language", "[.audit]")
{
    const QByteArray reportFile = qgetenv("VTM_AUDIT");
    struct Setup { int w, h; const char *form; };
    const Setup sizes[] = {{1600, 900, "standard"}, {1280, 800, "standard"}, {1024, 600, "standard"},
                           {800, 480, "standard"}, {540, 1080, "phone"}};
    QFile f(QStringLiteral(VTM_SEED_DIR "/themes/modern.json"));
    REQUIRE(f.open(QIODevice::ReadOnly));
    const auto modernTheme = layout::Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object());
    i18n::install();

    // finding -> where (style, size, language)
    std::map<std::tuple<QString, QString, QString>, std::set<QString>> found;
    g_warnings.clear();
    for (const char *style : {"classic", "modern"}) {
        for (const Setup &size : sizes) {
            for (const char *lang : {"en", "es", "fr", "pt"}) {
                auto layout = vt::test::loadTestLayout();
                REQUIRE(layout);
                if (QByteArray(style) == "modern")
                    REQUIRE(layout->useStyle(u"modern"_s, modernTheme));
                QStringList pageIds;
                for (const auto &p : layout->pages)
                    if (p.kind != u"template" && p.kind != u"library" && p.variantOf.isEmpty())
                        pageIds << p.id;
                test::RecordingSink sink;
                app::PosService pos(test::seedPosData(), &sink);
                LayoutController c(std::move(*layout));
                c.setFormFactorOverride(QString::fromLatin1(size.form));
                c.setMealPeriod(u"lunch"_s);
                c.setPos(&pos);
                REQUIRE(pos.loginWithPin(u"1234"_s));
                pos.addItem(u"classic-burger"_s);
                pos.addItem(u"caesar"_s);
                pos.addItem(u"soda"_s);
                i18n::setLanguage(QString::fromLatin1(lang));

                QQmlApplicationEngine engine;
                engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                             {u"width"_s, size.w}, {u"height"_s, size.h}});
                const QtMessageHandler previous = qInstallMessageHandler(collect);
                engine.loadFromModule("ViewTouch", "Main");
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                REQUIRE(window);
                REQUIRE(QTest::qWaitForWindowExposed(window));
                engine.retranslate();
                const QString where = u"%1 %2x%3 %4"_s.arg(QLatin1StringView(style)).arg(size.w).arg(size.h).arg(QLatin1StringView(lang));
                for (const QString &id : pageIds) {
                    if (!c.showPage(id))
                        continue;
                    QTest::qWait(30);
                    std::vector<Finding> out;
                    auditTexts(window->contentItem(), window, id, out);
                    bool fresh = false;
                    for (const Finding &x : out) {
                        auto &seen = found[{x.kind, x.page, x.text}];
                        fresh = fresh || seen.empty();
                        seen.insert(where);
                    }
                    // VTM_AUDIT_SHOTS=<dir>: a picture of each page the first time it shows something
                    // (and of the pages in VTM_AUDIT_PAGES, "reports,admin-terminals", always).
                    if (qEnvironmentVariable("VTM_AUDIT_PAGES").split(u',').contains(id))
                        fresh = true;
                    if (const QByteArray shots = qgetenv("VTM_AUDIT_SHOTS"); fresh && !shots.isEmpty())
                        window->grabWindow().save(QString::fromLocal8Bit(shots) + u'/' + id + u' ' + where + u".png"_s);
                }
                qInstallMessageHandler(previous);
            }
        }
    }
    i18n::setLanguage(u"en"_s);

    QStringList lines;
    for (const QString &warning : QSet<QString>(g_warnings.begin(), g_warnings.end()))
        lines << u"warning\t\t"_s + warning.left(200);
    for (const auto &[key, wheres] : found) {
        const auto &[kind, page, text] = key;
        QStringList w(wheres.begin(), wheres.end());
        lines << u"%1\t%2\t%3\t%4 setups: %5"_s.arg(kind, page, text).arg(w.size()).arg(w.mid(0, 4).join(u", "_s));
    }
    lines.sort();
    if (!reportFile.isEmpty()) {
        QFile out(QString::fromLocal8Bit(reportFile));
        REQUIRE(out.open(QIODevice::WriteOnly | QIODevice::Text));
        out.write(lines.join(u'\n').toUtf8());
    }
    UNSCOPED_INFO(lines.size() << " findings");
    CHECK(true);
}

TEST_CASE("Audit: dialogs, forms, the setup guide, the kiosk and the customer display", "[.audit]")
{
    const QByteArray reportFile = qgetenv("VTM_AUDIT_DIALOGS");
    struct Setup { int w, h; const char *form; };
    const Setup sizes[] = {{1600, 900, "standard"}, {1024, 600, "standard"}, {800, 480, "standard"}, {540, 1080, "phone"}};
    QFile f(QStringLiteral(VTM_SEED_DIR "/themes/modern.json"));
    REQUIRE(f.open(QIODevice::ReadOnly));
    const auto modernTheme = layout::Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object());
    i18n::install();
    const std::function<QQuickItem *(QQuickItem *, const QString &)> named = [&](QQuickItem *r, const QString &name) -> QQuickItem * {
        for (QQuickItem *i : r->childItems()) {
            if (i->isVisible() && i->objectName() == name) return i;
            if (QQuickItem *h = named(i, name)) return h;
        }
        return nullptr;
    };
    const std::function<QQuickItem *(QQuickItem *, const char *)> having = [&](QQuickItem *r, const char *property) -> QQuickItem * {
        for (QQuickItem *i : r->childItems()) {
            if (i->property(property).isValid()) return i;
            if (QQuickItem *h = having(i, property)) return h;
        }
        return nullptr;
    };

    std::map<std::tuple<QString, QString, QString>, std::set<QString>> found;
    g_warnings.clear();
    for (const char *style : {"classic", "modern"}) {
        for (const Setup &size : sizes) {
            for (const char *lang : {"en", "es", "fr", "pt"}) {
                const QString where = u"%1 %2x%3 %4"_s.arg(QLatin1StringView(style)).arg(size.w).arg(size.h).arg(QLatin1StringView(lang));
                std::set<QString> shot;
                const auto look = [&](QQuickWindow *window, const QString &what) {
                    QTest::qWait(40);
                    std::vector<Finding> out;
                    auditTexts(window->contentItem(), window, what, out);
                    bool fresh = false;
                    for (const Finding &x : out) {
                        auto &seen = found[{x.kind, x.page, x.text}];
                        fresh = fresh || seen.empty();
                        seen.insert(where);
                    }
                    if (qEnvironmentVariable("VTM_AUDIT_PAGES").split(u',').contains(what))
                        fresh = true;
                    if (const QByteArray shots = qgetenv("VTM_AUDIT_SHOTS"); fresh && !shots.isEmpty())
                        window->grabWindow().save(QString::fromLocal8Bit(shots) + u'/' + what + u' ' + where + u".png"_s);
                };
                auto layout = vt::test::loadTestLayout();
                REQUIRE(layout);
                if (QByteArray(style) == "modern")
                    REQUIRE(layout->useStyle(u"modern"_s, modernTheme));
                QStringList adminPages;
                for (const auto &p : layout->pages)
                    if (p.variantOf.isEmpty() && std::ranges::any_of(p.zones, [](const layout::Zone &z) { return z.kind == u"adminPanel"; }))
                        adminPages << p.id;
                test::RecordingSink sink;
                app::PosData data = test::seedPosData();
                data.settings.setupDone = false;
                app::PosService pos(std::move(data), &sink);
                LayoutController c(std::move(*layout));
                c.setFormFactorOverride(QString::fromLatin1(size.form));
                c.setMealPeriod(u"lunch"_s);
                c.setPos(&pos);
                i18n::setLanguage(QString::fromLatin1(lang));
                QQmlApplicationEngine engine;
                engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                             {u"width"_s, size.w}, {u"height"_s, size.h}});
                const QtMessageHandler previous = qInstallMessageHandler(collect);
                engine.loadFromModule("ViewTouch", "Main");
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
                REQUIRE(window);
                REQUIRE(QTest::qWaitForWindowExposed(window));
                engine.retranslate();

                // The setup guide, each step (a manager's first login opens it).
                REQUIRE(pos.loginWithPin(u"1234"_s));
                QTest::qWait(60);
                if (QQuickItem *guide = having(window->contentItem(), "steps")) {
                    const int steps = guide->property("steps").toList().size();
                    for (int i = 0; i < steps; ++i) {
                        guide->setProperty("step", i);
                        look(window, u"setup-guide-"_s + guide->property("steps").toList().value(i).toString());
                    }
                }
                c.closeSetup();
                pos.shared()->settings.setupDone = true;

                // Each Manager list's first record, opened.
                for (const QString &id : adminPages) {
                    if (!c.showPage(id))
                        continue;
                    QTest::qWait(40);
                    QQuickItem *list = named(window->contentItem(), u"adminList"_s);
                    if (!list)
                        continue;
                    QQuickItem *row = nullptr;
                    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0));
                    if (!row)
                        continue;
                    QTest::mouseClick(window, Qt::LeftButton, {}, row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
                    look(window, u"form-"_s + id);
                }

                // Manager approval: a server voids a sent item.
                pos.logout();
                REQUIRE(pos.loginWithPin(u"1111"_s));
                REQUIRE(c.showPage(u"index-lunch"_s));
                pos.addItem(u"caesar"_s);
                pos.sendOrder();
                pos.voidItem();
                look(window, u"approval"_s);
                pos.cancelApproval();
                pos.releaseCheck();

                // A food truck: the order screen with a name and a pager, and Orders.
                pos.logout();
                pos.shared()->settings.serviceMode = "foodTruck";
                emit pos.shared()->adminChanged();
                pos.addItem(u"caesar"_s);
                pos.setCustomer({{u"name"_s, u"Alexandria"_s}});
                pos.setPager(12);
                pos.sendOrder();
                look(window, u"truck-order"_s);
                pos.releaseCheck();
                REQUIRE(c.showPage(u"truck-orders"_s));
                look(window, u"truck-orders"_s);
                pos.shared()->settings.serviceMode.clear();
                emit pos.shared()->adminChanged();

                // The self-order kiosk: welcome, the menu, the order, the number.
                REQUIRE(pos.loginWithPin(u"1234"_s));
                pos.enableSelfOrder();
                QTest::qWait(60);
                look(window, u"kiosk-welcome"_s);
                pos.kioskStart(false);
                pos.kioskAdd(u"caesar"_s);
                pos.kioskAdd(u"soda"_s);
                look(window, u"kiosk-menu"_s);
                pos.kioskFinish({{u"name"_s, u"Alexandria"_s}});
                look(window, u"kiosk-number"_s);
                qInstallMessageHandler(previous);

                // The customer display: between guests, an order, the tip, thank you.
                {
                    app::PosService guest(test::seedPosData(), nullptr);
                    REQUIRE(guest.loginWithPin(u"2222"_s));
                    QQmlApplicationEngine display;
                    display.setInitialProperties({{u"pos"_s, QVariant::fromValue(static_cast<app::PosSession *>(&guest))}});
                    display.loadFromModule("ViewTouch", "CustomerDisplayWindow");
                    auto *dw = qobject_cast<QQuickWindow *>(display.rootObjects().value(0));
                    REQUIRE(dw);
                    dw->resize(size.w, size.h);
                    REQUIRE(QTest::qWaitForWindowExposed(dw));
                    display.retranslate();
                    look(dw, u"display-idle"_s);
                    guest.addItem(u"classic-burger"_s);
                    guest.addItem(u"caesar"_s);
                    look(dw, u"display-order"_s);
                    guest.askForTip();
                    look(dw, u"display-tip"_s);
                    guest.openDrawerSession();
                    guest.tender(u"cash"_s);
                    guest.closeCheck();
                    look(dw, u"display-thanks"_s);
                }
            }
        }
    }
    i18n::setLanguage(u"en"_s);

    QStringList lines;
    for (const QString &warning : QSet<QString>(g_warnings.begin(), g_warnings.end()))
        lines << u"warning\t\t"_s + warning.left(200);
    for (const auto &[key, wheres] : found) {
        const auto &[kind, page, text] = key;
        QStringList w(wheres.begin(), wheres.end());
        lines << u"%1\t%2\t%3\t%4 setups: %5"_s.arg(kind, page, text).arg(w.size()).arg(w.mid(0, 4).join(u", "_s));
    }
    lines.sort();
    if (!reportFile.isEmpty()) {
        QFile out(QString::fromLocal8Bit(reportFile));
        REQUIRE(out.open(QIODevice::WriteOnly | QIODevice::Text));
        out.write(lines.join(u'\n').toUtf8());
    }
    UNSCOPED_INFO(lines.size() << " findings");
    CHECK(true);
}
