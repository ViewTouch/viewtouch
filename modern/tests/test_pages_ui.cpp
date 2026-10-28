#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

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
