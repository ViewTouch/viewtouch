#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "net/build_info.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "print/raster.hh"
#include "editorcontroller.hh"
#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QRegularExpression>
#include <QPainter>
#include <QTemporaryDir>
#include <QFontMetricsF>
#include "menuprint.hh"
#include "app/i18n.hh"
#include "language.hh"
#include "fake_stripe.hh"
#include "pos_fixture.hh"
#include "storage/pos_store.hh"
#include "qt_catch.hh"

#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDate>
#include <QPointer>
#include <QSignalSpy>
#include <QDir>
#include <QQmlProperty>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::layout::Layout;

// M3 acceptance: log in -> table -> order with modifiers -> send -> pay ->
// close, through the real pages and zone actions.

namespace {

Layout seedLayout()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

struct Session {
    test::RecordingSink sink;
    app::PosService pos{test::seedPosData(true), &sink};
    LayoutController c{seedLayout()};

    Session()
    {
        c.setMealPeriod(u"lunch"_s);
        c.setPos(&pos);
    }

    void pin(const char *digits)
    {
        for (const char *d = digits; *d; ++d)
            pos.pinKey(QString(QChar(*d)));
    }

    bool zoneCurrent(const QString &zoneId)
    {
        ZoneModel *m = c.zones();
        for (int r = 0; r < m->rowCount(); ++r) {
            if (m->data(m->index(r), ZoneModel::ZoneIdRole).toString() == zoneId)
                return m->data(m->index(r), ZoneModel::CurrentRole).toBool();
        }
        FAIL("no zone " << zoneId.toStdString());
        return false;
    }
};

} // namespace

TEST_CASE("Flow: login, table, order with modifiers, send, pay, close", "[flow]")
{
    Session s;
    CHECK(s.c.pageId() == u"login"_s);

    // Nothing opens before logging in.
    CHECK_FALSE(s.c.showPage(u"tables"_s));
    CHECK(s.c.statusText().contains(u"Log in"_s));

    s.pin("1111");
    s.c.activate(u"start"_s);                        // "Log In"
    REQUIRE(s.pos.loggedIn());
    CHECK(s.c.pageId() == u"tables"_s);
    CHECK_FALSE(s.c.canGoBack());                    // tables is home while logged in

    s.c.selectTable(u"T3"_s);
    CHECK(s.c.pageId() == u"guest-count"_s);
    s.pos.entryKey(u"2"_s);
    s.c.activate(u"start"_s);                        // startCheck + go to menu
    CHECK(s.c.pageId() == u"index-lunch"_s);
    REQUIRE(s.pos.hasCheck());
    CHECK(s.pos.checkInfo()[u"label"_s].toString() == u"T3"_s);
    CHECK(s.pos.checkInfo()[u"guests"_s].toInt() == 2);

    REQUIRE(s.c.jumpTo(u"items-burgers"_s));          // a page of buttons placed by hand (a fixture)
    s.c.activate(u"item-1"_s);                       // Classic Burger: its choices
    CHECK(s.c.pageId() == u"modifiers"_s);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1)); // Medium Rare
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));        // Onion Rings (+1.00)
    s.c.finishChoosing();
    CHECK(s.c.pageId() == u"items-burgers"_s);

    // Qualifier button stays lit while pending, clears after use.
    s.c.activate(u"flow-no"_s);
    CHECK(s.zoneCurrent(u"flow-no"_s));
    CHECK_FALSE(s.zoneCurrent(u"flow-extra"_s));
    s.c.activate(u"flow-no"_s);                      // touching again cancels it
    CHECK_FALSE(s.zoneCurrent(u"flow-no"_s));

    const QVariantList lines = s.pos.lines();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].toMap()[u"modifiers"_s].toList().size() == 2);
    CHECK(lines[0].toMap()[u"price"_s].toString() == u"$12.50"_s);

    s.c.activate(u"flow-send"_s);
    CHECK(s.pos.lines()[0].toMap()[u"sent"_s].toBool());

    REQUIRE(s.pos.openDrawerSession());              // cash needs an open drawer
    s.c.activate(u"flow-pay"_s);
    CHECK(s.c.pageId() == u"settle"_s);
    // 12.50 + 8.25% (1.03125 -> 1.03) = 13.53; pay $20 cash.
    CHECK(s.pos.totals()[u"total"_s].toString() == u"$13.53"_s);
    s.pos.entryKey(u"2000"_s);
    s.c.activate(u"tender-cash"_s);
    CHECK(s.pos.totals()[u"change"_s].toString() == u"$6.47"_s);

    s.c.activate(u"close"_s);
    CHECK_FALSE(s.pos.hasCheck());
    CHECK(s.c.pageId() == u"tables"_s);              // back to the floor
    CHECK_FALSE(s.pos.tableStatus(u"T3"_s)[u"open"_s].toBool());

    REQUIRE(s.sink.checks.size() == 1);
    const core::Check &saved = s.sink.checks.begin()->second;
    CHECK(saved.status == core::CheckStatus::Closed);
    CHECK(saved.label == "T3");
    CHECK(saved.lines[0].modifiers.size() == 2);
}

TEST_CASE("Flow: a failed step stops the rest of a button's actions", "[flow]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    // Guest count "Start Order" without choosing a table: startCheck fails,
    // so the jump to the menu must not happen.
    REQUIRE(s.c.showPage(u"guest-count"_s));
    s.c.activate(u"start"_s);
    CHECK(s.c.pageId() == u"guest-count"_s);
    CHECK_FALSE(s.pos.hasCheck());
}

TEST_CASE("Flow: permissions guard pages, voids and editing", "[flow]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));   // server
    CHECK(s.c.pageId() == u"tables"_s);

    s.c.activate(u"manager"_s);
    CHECK(s.c.pageId() == u"tables"_s);
    CHECK(s.c.statusText().contains(u"cannot open"_s));

    CHECK_FALSE(s.c.requestEditMode());
    CHECK_FALSE(s.c.editing());

    s.c.activate(u"logout"_s);                // to the logout page
    CHECK(s.c.pageId() == u"logout"_s);
    s.c.activate(u"logout"_s);                // the Log Out command
    CHECK_FALSE(s.pos.loggedIn());
    CHECK(s.c.pageId() == u"login"_s);

    REQUIRE(s.pos.loginWithPin(u"1234"_s));   // manager
    s.c.activate(u"manager"_s);
    CHECK(s.c.pageId() == u"manager"_s);
    s.c.activate(u"edit-pages"_s);
    CHECK(s.c.editing());
}

TEST_CASE("Flow: quick order from the floor, open checks list", "[flow]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"2222"_s));
    s.c.activate(u"quick"_s);
    CHECK(s.c.pageId() == u"index-lunch"_s);
    REQUIRE(s.pos.hasCheck());
    REQUIRE(s.c.openCategory(u"drinks"_s));   // the Drinks button: the menu on Drinks
    CHECK(s.c.pageId() == u"menu-all"_s);
    s.c.orderItem(u"draft-beer"_s);
    s.c.activate(u"flow-tables"_s);           // release and go back to the floor
    CHECK(s.c.pageId() == u"tables"_s);
    CHECK_FALSE(s.pos.hasCheck());

    s.c.activate(u"checks"_s);
    CHECK(s.c.pageId() == u"check-list"_s);
    REQUIRE(s.pos.openChecks().size() == 1);
    s.c.openCheck(s.pos.openChecks()[0].toMap()[u"id"_s].toLongLong());
    CHECK(s.c.pageId() == u"index-lunch"_s);
    CHECK(s.pos.lines().size() == 1);
}

// --- through the real widgets -------------------------------------------------

namespace {

struct Screen : Session {
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;

    explicit Screen(bool touchKeyboard = false, int width = 1600, int height = 900, const QString &formFactor = {})
    {
        if (!formFactor.isEmpty())
            c.setFormFactorOverride(formFactor);
        engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)},
                                     {u"width"_s, width}, {u"height"_s, height}, {u"touchKeyboard"_s, touchKeyboard}});
        engine.loadFromModule("ViewTouch", "Main");
        REQUIRE_FALSE(engine.rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        REQUIRE(window);
        REQUIRE(QTest::qWaitForWindowExposed(window));
        QTest::qWait(50);
    }

    QPoint at(qreal cx, qreal cy)
    {
        auto *surface = window->findChild<QQuickItem *>(u"pageSurface"_s);
        REQUIRE(surface);
        return surface->mapToScene(QPointF(cx, cy)).toPoint();
    }

    void tapCanvas(qreal cx, qreal cy)
    {
        QTest::mouseClick(window, Qt::LeftButton, {}, at(cx, cy));
        QTest::qWait(30);
    }

    // Set VTM_SHOTS=<dir> to save screenshots of the flow for review.
    void shot(const char *name)
    {
        const QByteArray dir = qgetenv("VTM_SHOTS");
        if (!dir.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(dir) + u'/' + QString::fromLatin1(name) + u".png"_s);
    }

    // Depth-first over the visual tree (Repeater items are not QObject children).
    static QQuickItem *findKey(QQuickItem *root, const QString &text)
    {
        for (QQuickItem *item : root->childItems()) {
            if (!item->isVisible())
                continue;
            if (QByteArray(item->metaObject()->className()).startsWith("WidgetKey")
                && item->property("text").toString() == text)
                return item;
            if (QQuickItem *hit = findKey(item, text))
                return hit;
        }
        return nullptr;
    }

    // A visible item whose `property` is `value` (a Text with that text, a
    // field with that placeholder...).
    static QQuickItem *findBy(QQuickItem *root, const char *property, const QString &value)
    {
        for (QQuickItem *item : root->childItems()) {
            if (!item->isVisible())
                continue;
            if (item->property(property).toString() == value)
                return item;
            if (QQuickItem *hit = findBy(item, property, value))
                return hit;
        }
        return nullptr;
    }

    // Voids and comps ask why first (Store Settings): the first reason.
    void giveReason()
    {
        QQuickItem *r = findBy(window->contentItem(), "objectName", u"reason-0"_s);
        REQUIRE(r);
        REQUIRE(r->isVisible());
        tapItem(r);
        QTest::qWait(60);
    }

    void tapItem(QQuickItem *item)
    {
        REQUIRE(item);
        // In a list scrolled past it: scrolled to it first, as a person would.
        for (QQuickItem *p = item->parentItem(); p; p = p->parentItem()) {
            if (!p->inherits("QQuickFlickable"))
                continue;
            QQuickItem *content = qvariant_cast<QQuickItem *>(p->property("contentItem"));
            if (!content)
                break;
            const QRectF r = item->mapRectToItem(content, QRectF(0, 0, item->width(), item->height()));
            const double y = p->property("contentY").toDouble(), x = p->property("contentX").toDouble();
            const double maxY = std::max(0.0, p->property("contentHeight").toDouble() - p->height());
            const double maxX = std::max(0.0, p->property("contentWidth").toDouble() - p->width());
            if (r.top() < y || r.bottom() > y + p->height())
                p->setProperty("contentY", std::clamp(r.center().y() - p->height() / 2, 0.0, maxY));
            if (r.left() < x || r.right() > x + p->width())
                p->setProperty("contentX", std::clamp(r.center().x() - p->width() / 2, 0.0, maxX));
            QTest::qWait(30);
            break;
        }
        const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, centre.toPoint());
        QTest::qWait(30);
    }

    // The on-screen keyboard (it pops up over the screen).
    QQuickItem *keyboard()
    {
        QObject *popup = window->findChild<QObject *>(u"touchKeys"_s);
        return popup ? qvariant_cast<QQuickItem *>(popup->property("contentItem")) : nullptr;
    }
    bool keyboardShown()
    {
        QObject *popup = window->findChild<QObject *>(u"touchKeys"_s);
        return popup && popup->property("visible").toBool();
    }

    // Tap on-screen keyboard keys (their labels), one per character.
    void typeOnScreen(const QString &labels)
    {
        for (QChar ch : labels)
            tapItem(findBy(keyboard(), "text", QString(ch)));
    }

    // Tap a table on the floor plan shown, by its name.
    void tapTable(const QString &name)
    {
        QQuickItem *t = findBy(window->contentItem(), "text", name);
        while (t && !QByteArray(t->metaObject()->className()).startsWith("ZoneItem"))
            t = t->parentItem();
        tapItem(t);
    }

    // Tap a visible keypad key by its text.
    void tapKey(const QString &text)
    {
        QQuickItem *found = findKey(window->contentItem(), text);
        REQUIRE(found);
        const QPointF centre = found->mapToScene(QPointF(found->width() / 2, found->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, centre.toPoint());
        QTest::qWait(30);
    }
};

} // namespace

TEST_CASE("UI flow: keypad login, table map, guest pad, menu, pay", "[flow][ui]")
{
    Screen s;
    s.shot("1-login");
    for (const char *k : {"1", "1", "1", "1"})
        s.tapKey(QString::fromLatin1(k));
    CHECK(s.pos.pinLength() == 4);
    s.tapKey(u"Enter"_s);
    REQUIRE(s.pos.loggedIn());
    CHECK(s.c.pageId() == u"tables"_s);

    s.tapTable(u"T3"_s);
    CHECK(s.c.pageId() == u"guest-count"_s);
    s.tapKey(u"3"_s);
    CHECK(s.pos.entryGuests() == 3);
    s.tapCanvas(970 + 145, 888 + 60);         // Start Order
    CHECK(s.c.pageId() == u"index-lunch"_s);

    // The lunch categories fill themselves: Salads, then its items.
    s.tapKey(u"Salads"_s);
    CHECK(s.c.pageId() == u"menu-all"_s);
    CHECK(s.c.menuCategory() == u"salads"_s);
    QTest::qWait(60);
    const auto menuItem = [&](const char *id) {
        return Screen::findBy(s.window->contentItem(), "objectName", u"menuItem-"_s + QString::fromLatin1(id));
    };
    s.tapItem(menuItem("house-salad"));       // House Salad: it asks for a dressing
    CHECK(s.c.pageId() == u"modifiers"_s);
    s.tapKey(u"Done"_s);                      // not without the dressing
    CHECK(s.c.pageId() == u"modifiers"_s);
    s.tapKey(u"Ranch"_s);
    s.shot("2-choose");
    s.tapKey(u"Done"_s);
    CHECK(s.c.pageId() == u"menu-all"_s);
    CHECK(s.pos.lines().size() == 1);
    QTest::qWait(60);
    s.tapItem(menuItem("caesar"));            // Caesar: a protein is optional
    s.tapKey(u"Done"_s);
    CHECK(s.pos.lines().size() == 2);
    s.shot("2-order");

    // Back to the floor: T3 shows as ours; touching it reopens the check.
    s.tapCanvas(16 + 110, 980 + 42);          // Tables
    CHECK(s.c.pageId() == u"tables"_s);
    CHECK_FALSE(s.pos.hasCheck());
    s.shot("4-tables");
    s.tapTable(u"T3"_s);
    CHECK(s.c.pageId() == u"index-lunch"_s);
    CHECK(s.pos.lines().size() == 2);

    s.tapCanvas(16 + 7 * 237 + 110, 980 + 42);   // Pay
    CHECK(s.c.pageId() == u"settle"_s);
    s.tapCanvas(1468 + 218, 16 + 86 + 38);       // Credit Card, the second payment type (exact balance)
    CHECK(s.pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    s.shot("3-settle");
    s.tapCanvas(932 + 260, 788 + 60);            // Close Check
    CHECK_FALSE(s.pos.hasCheck());
    CHECK(s.c.pageId() == u"tables"_s);
    CHECK(s.sink.checks.begin()->second.status == core::CheckStatus::Closed);
}

TEST_CASE("UI: seats and courses on the order screen; Fire sends the held course", "[flow][ui][courses]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T2"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-salads"_s));
    QTest::qWait(30);

    s.tapKey(u"+"_s);                                   // seat 1
    CHECK(s.pos.checkInfo()[u"seat"_s] == 1);
    s.tapCanvas(592 + 150, 192 + 90);                   // House Salad, seat 1
    s.tapKey(u"Ranch"_s);
    s.tapKey(u"Done"_s);
    s.tapKey(u"+"_s);                                   // seat 2
    s.tapKey(u"2"_s);                                   // course 2
    s.tapCanvas(592 + 444 + 150, 192 + 90);             // Caesar, seat 2, course 2
    s.tapKey(u"Done"_s);
    const QVariantList lines = s.pos.lines();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].toMap()[u"seat"_s] == 1);
    CHECK(lines[1].toMap()[u"seat"_s] == 2);
    CHECK(lines[1].toMap()[u"held"_s].toBool());

    REQUIRE(s.pos.sendOrder());
    QTest::qWait(30);
    s.shot("5-courses");
    s.tapKey(u"Fire Course 2"_s);
    CHECK(s.pos.lines()[1].toMap()[u"sent"_s].toBool());
}

TEST_CASE("UI: the on-screen keyboard finds a customer; a gift card by its number", "[flow][ui][customers]")
{
    Screen s(true);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveCustomer({{u"name"_s, u"Dana Lee"_s}, {u"phone"_s, u"555-010-1234"_s}, {u"note"_s, u"no onions"_s}}));
    REQUIRE(s.pos.saveCustomer({{u"name"_s, u"Alex Kim"_s}, {u"phone"_s, u"555-777-0000"_s}}));
    REQUIRE(s.c.jumpTo(u"customers"_s));
    QTest::qWait(50);

    QQuickItem *keyboard = s.keyboard();
    REQUIRE(keyboard);
    CHECK_FALSE(s.keyboardShown());
    s.tapItem(Screen::findBy(s.window->contentItem(), "placeholderText", u"Phone or name…"_s));
    CHECK(s.keyboardShown());                           // a text field is being typed in
    s.typeOnScreen(u"Dan"_s);                          // capital first, then small letters
    QTest::qWait(400);
    REQUIRE(s.pos.customerResults().size() == 1);
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Dana Lee"_s));
    QTest::qWait(50);
    CHECK(s.pos.customerInfo()[u"name"_s] == u"Dana Lee"_s);
    s.shot("6-customers");

    REQUIRE(s.c.jumpTo(u"gift-card"_s));
    QTest::qWait(50);
    s.tapItem(Screen::findBy(s.window->contentItem(), "placeholderText", u"Type or swipe…"_s));
    REQUIRE(s.keyboardShown());
    CHECK(Screen::findBy(keyboard, "text", u"Done"_s));   // number fields get the number pad
    s.typeOnScreen(u"60012"_s);
    s.tapItem(Screen::findBy(keyboard, "text", u"Done"_s));   // Enter looks the card up
    CHECK(s.pos.giftCardInfo()[u"number"_s] == u"60012"_s);
    CHECK_FALSE(s.keyboardShown());
    s.shot("7-gift-card");
}

TEST_CASE("UI: the host stand seats the next party at the best free table", "[flow][ui][waitlist]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.addToWaitlist({{u"name"_s, u"Dana"_s}, {u"size"_s, 4}, {u"phone"_s, u"555-0101"_s}}) > 0);
    REQUIRE(s.pos.addToWaitlist({{u"name"_s, u"Alex"_s}, {u"size"_s, 2}, {u"note"_s, u"booth please"_s}}) > 0);
    const QString tomorrow = QDate::currentDate().addDays(1).toString(u"yyyy-MM-dd"_s) + u" 19:30"_s;
    REQUIRE(s.pos.addReservation({{u"name"_s, u"Lee"_s}, {u"size"_s, 6}, {u"at"_s, tomorrow}}) > 0);
    REQUIRE(s.c.jumpTo(u"host"_s));
    QTest::qWait(50);
    s.shot("8-host");

    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Dana  ·  4 people"_s));
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Seat Them…"_s));
    QTest::qWait(50);
    s.shot("9-host-seat");
    // Four people: the smallest free table that fits (T3, 4 seats) comes first.
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"T3\n4 seats"_s));
    CHECK(s.pos.tableStatus(u"T3"_s)[u"open"_s].toBool());
    CHECK(s.pos.waitlistInfo()[u"waiting"_s].toList().size() == 1);
}

TEST_CASE("UI: the customer display shows the order, asks for a tip, says thank you", "[flow][ui][display]")
{
    Session s;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"pos"_s, QVariant::fromValue(static_cast<app::PosSession *>(&s.pos))}});
    engine.loadFromModule("ViewTouch", "CustomerDisplayWindow");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    window->resize(1280, 720);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    const QByteArray dir = qgetenv("VTM_SHOTS");
    const auto shot = [&](const char *name) {
        QTest::qWait(60);
        if (!dir.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(dir) + u'/' + QString::fromLatin1(name) + u".png"_s);
    };
    std::function<QQuickItem *(QQuickItem *, const QString &)> find = [&](QQuickItem *root, const QString &text) -> QQuickItem * {
        for (QQuickItem *i : root->childItems()) {
            if (!i->isVisible())
                continue;
            if (i->property("text").toString() == text)
                return i;
            if (QQuickItem *hit = find(i, text))
                return hit;
        }
        return nullptr;
    };
    std::function<QQuickItem *(QQuickItem *, const QString &)> named = [&](QQuickItem *root, const QString &name) -> QQuickItem * {
        for (QQuickItem *i : root->childItems()) {
            if (i->isVisible() && i->objectName() == name)
                return i;
            if (QQuickItem *hit = named(i, name))
                return hit;
        }
        return nullptr;
    };
    const auto tap = [&](const QString &text) {
        QQuickItem *item = named(window->contentItem(), u"guestKey-"_s + text);   // a keypad key, else by its text
        if (!item)
            item = find(window->contentItem(), text);
        INFO(text.toStdString());
        REQUIRE(item);
        QTest::mouseClick(window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QTest::qWait(30);
    };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    shot("10-display-welcome");

    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"bacon-burger"_s);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));   // Medium Rare
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));          // Onion Rings (+1.00)
    REQUIRE(s.pos.finishChoosing());
    s.pos.addItem(u"house-salad"_s);
    REQUIRE(s.pos.chooseOption(u"dressing"_s, 0));
    REQUIRE(s.pos.chooseOption(u"salad-protein"_s, 0));
    REQUIRE(s.pos.finishChoosing());
    s.pos.addItem(u"water"_s);
    // The guest joins rewards with their phone number.
    tap(u"Earn rewards: touch to add your phone"_s);
    for (const char *k : {"5", "5", "5", "0", "1", "0", "1", "2", "3", "4"})
        tap(QString::fromLatin1(k));
    tap(u"OK"_s);
    CHECK(s.pos.customerPrompt()[u"loyalty"_s].toMap()[u"member"_s].toString() == u"...1234"_s);   // never the whole number
    shot("11-display-order");

    REQUIRE(s.pos.askForTip());
    shot("12-display-tip");
    tap(u"Custom amount"_s);
    for (const char *k : {"5", "0", "0"})
        tap(QString::fromLatin1(k));
    tap(u"OK"_s);
    CHECK(s.pos.customerPrompt()[u"tip"_s] == u"$5.00"_s);
    const int earning = s.pos.customerPrompt()[u"loyalty"_s].toMap()[u"earning"_s].toInt();
    CHECK(earning > 20);
    REQUIRE(s.pos.tender(u"credit"_s));
    REQUIRE(s.pos.closeCheck());
    shot("13-display-thanks");
    CHECK(find(window->contentItem(), u"You earned %1 points"_s.arg(earning)));
    tap(u"No receipt"_s);
    CHECK_FALSE(find(window->contentItem(), u"Print receipt"_s));
}

TEST_CASE("UI: the kitchen display with a rush ticket and the all-day counts", "[flow][ui][kitchen]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    const auto order = [&](const QString &table, std::initializer_list<const char *> items, bool rush = false) {
        REQUIRE(s.pos.selectTable(table) == app::PosService::TableNeedsGuests);
        REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
        for (const char *i : items) {
            s.pos.addItem(QString::fromLatin1(i));
            for (const QVariant &g : s.pos.choosingInfo()[u"groups"_s].toList())   // the first of what's required
                if (!g.toMap()[u"done"_s].toBool())
                    s.pos.chooseOption(g.toMap()[u"id"_s].toString(), 0);
            s.pos.finishChoosing();
        }
        if (rush)
            REQUIRE(s.pos.toggleFlag(u"rush"_s));
        REQUIRE(s.pos.sendOrder());
        s.pos.releaseCheck();
    };
    order(u"T1"_s, {"cobb", "caesar"});
    order(u"T2"_s, {"cobb", "cobb", "water"});
    order(u"T3"_s, {"caesar"}, true);
    order(u"T4"_s, {"bacon-burger", "cheeseburger"});
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    QTest::qWait(50);
    s.tapKey(u"All Day"_s);
    QTest::qWait(50);
    s.shot("14-kitchen");
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"RUSH"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"All day"_s));
}

TEST_CASE("UI: the week's schedule; adding a shift by touch", "[flow][ui][schedule]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const QDate today = QDate::currentDate();
    const auto day = [&](int d) { return today.addDays(d).toString(u"yyyy-MM-dd"_s); };
    REQUIRE(s.pos.addShift({{u"employeeId"_s, u"sam"_s}, {u"start"_s, day(0) + u" 16:00"_s}, {u"end"_s, day(0) + u" 22:00"_s}}));
    REQUIRE(s.pos.addShift({{u"employeeId"_s, u"riley"_s}, {u"start"_s, day(0) + u" 17:00"_s}, {u"end"_s, day(0) + u" 23:30"_s},
                            {u"note"_s, u"close"_s}}));
    REQUIRE(s.pos.addShift({{u"employeeId"_s, u"jo"_s}, {u"start"_s, day(1) + u" 18:00"_s}, {u"end"_s, day(1) + u" 01:00"_s}}));
    REQUIRE(s.c.jumpTo(u"admin-schedule"_s));
    QTest::qWait(50);
    const int before = int(s.pos.shared()->shifts.size());
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Add Shift"_s));
    CHECK(int(s.pos.shared()->shifts.size()) == before + 1);   // the first person, today, 4 - 10 PM
    QTest::qWait(50);
    s.shot("15-schedule");
}

TEST_CASE("UI: a month's report beside last year", "[flow][ui][range]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    for (const char *item : {"cobb", "caesar", "cobb"}) {
        REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
        s.pos.addItem(QString::fromLatin1(item));
        REQUIRE(s.pos.tender(u"cash"_s));
        REQUIRE(s.pos.closeCheck());
    }
    REQUIRE(s.c.jumpTo(u"reports"_s));
    QTest::qWait(50);
    s.tapKey(u"Items"_s);
    s.tapKey(u"This Month"_s);
    s.tapKey(u"vs Last Year"_s);
    for (int i = 0; i < 100 && s.pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    QTest::qWait(50);
    CHECK(s.pos.rangeReport()[u"checks"_s] == 3);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Change"_s));
    s.shot("16-range-report");
}

TEST_CASE("UI: the kiosk in the store's own look, asking only what the store wants", "[flow][ui][kiosk][kiosklook]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantMap store = s.pos.adminRecords(u"store"_s).first().toMap();
    store[u"kioskBackground"_s] = u"blue"_s;
    CHECK_FALSE(s.pos.adminSave(u"store"_s, 0, store));       // colors as #rrggbb
    store[u"kioskBackground"_s] = u"#f4efe6"_s;
    store[u"kioskCard"_s] = u"#ffffff"_s;
    store[u"kioskText"_s] = u"#2b2118"_s;
    store[u"kioskGo"_s] = u"#c0392b"_s;
    store[u"kioskWelcome"_s] = u"Tap to start your order"_s;
    store[u"kioskSizePercent"_s] = 120;
    store[u"kioskAskWhere"_s] = false;
    store[u"kioskAskName"_s] = false;
    store[u"kioskEasyReach"_s] = false;
    REQUIRE(s.pos.adminSave(u"store"_s, 0, store));
    CHECK(app::settingsFromJson(app::toJson(s.pos.shared()->settings)).kioskLook == s.pos.shared()->settings.kioskLook);
    s.pos.logout();

    s.pos.enableSelfOrder();
    QTest::qWait(80);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *kiosk = find(u"selfOrder"_s);
    REQUIRE(kiosk);
    CHECK(kiosk->property("color").value<QColor>() == QColor(u"#f4efe6"_s));
    CHECK(Screen::findBy(kiosk, "text", u"Tap to start your order"_s));
    s.shot("34-kiosk-look");

    s.tapItem(find(u"kioskAttract"_s));                   // no For Here / To Go: the menu
    QTest::qWait(60);
    REQUIRE(s.pos.selfOrderInfo()[u"ordering"_s].toBool());
    CHECK_FALSE(find(u"kioskEasyReach"_s));
    s.tapItem(Screen::findBy(kiosk, "text", u"Salads"_s));
    QTest::qWait(60);
    s.tapItem(Screen::findBy(kiosk, "text", u"Caesar"_s));  // asks for an optional protein
    QTest::qWait(60);
    s.tapItem(find(u"kioskChoicesDone"_s));
    QTest::qWait(60);
    REQUIRE(s.pos.lines().size() == 1);
    s.shot("35-kiosk-look-menu");
    s.tapItem(find(u"kioskReview"_s));
    QTest::qWait(60);
    CHECK_FALSE(find(u"kioskName"_s));                    // no name asked
    s.tapItem(find(u"kioskPlace"_s));
    QTest::qWait(60);
    QQuickItem *number = find(u"kioskNumber"_s);
    REQUIRE(number);
    CHECK(number->isVisible());
}

TEST_CASE("UI: a guest orders on the self-order kiosk", "[flow][ui][kiosk]")
{
    Screen s;
    s.pos.enableSelfOrder();
    QTest::qWait(60);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *kiosk = find(u"selfOrder"_s);
    REQUIRE(kiosk);
    CHECK(kiosk->isVisible());
    s.shot("20-kiosk-welcome");

    s.tapItem(find(u"kioskAttract"_s));                  // "Touch to Order"
    QTest::qWait(60);
    s.tapItem(find(u"kioskForHere"_s));
    REQUIRE(s.pos.selfOrderInfo()[u"ordering"_s].toBool());
    QTest::qWait(60);
    // A burger asks how it's cooked and for a side; the guest touches them.
    const auto burger = [&](const QString &name, const QString &temperature, const QString &side) {
        s.tapItem(Screen::findBy(kiosk, "text", name));   // the kiosk's card, not the page behind
        QTest::qWait(60);
        QQuickItem *done = find(u"kioskChoicesDone"_s);
        REQUIRE(done);
        CHECK(done->isVisible());
        CHECK_FALSE(done->isEnabled());                   // nothing chosen yet
        s.tapItem(Screen::findBy(kiosk, "text", temperature));
        s.tapItem(Screen::findBy(kiosk, "text", side));
        QTest::qWait(30);
        CHECK(done->isEnabled());
        s.tapItem(done);
    };
    burger(u"Classic Burger"_s, u"Medium Rare"_s, u"Fries"_s);
    s.tapItem(Screen::findBy(kiosk, "text", u"Cheeseburger"_s));
    QTest::qWait(60);
    s.tapItem(Screen::findBy(kiosk, "text", u"Well Done"_s));
    s.shot("21b-kiosk-choices");
    s.tapItem(Screen::findBy(kiosk, "text", u"Onion Rings\n+$1.00"_s));
    s.tapItem(find(u"kioskChoicesDone"_s));
    REQUIRE(s.pos.lines().size() == 2);
    CHECK(s.pos.lines()[0].toMap()[u"modifiers"_s].toList().size() == 2);   // Medium Rare, Fries
    s.shot("21-kiosk-menu");
    s.tapItem(find(u"kioskRemove"_s));                   // changed their mind
    REQUIRE(s.pos.lines().size() == 1);

    s.tapItem(find(u"kioskReview"_s));
    QTest::qWait(60);
    for (const char ch : {'L', 'e', 'e'})
        QTest::keyClick(s.window, ch);
    QTest::qWait(30);
    s.shot("22-kiosk-name");
    const qint64 id = s.pos.checkInfo()[u"id"_s].toLongLong();
    s.tapItem(find(u"kioskPlace"_s));
    QTest::qWait(60);
    QQuickItem *number = find(u"kioskNumber"_s);
    REQUIRE(number);
    CHECK(number->isVisible());
    CHECK(number->property("text").toString() == QString::number(id));
    CHECK(s.pos.shared()->open.at(id).customer.name == "Lee");
    s.shot("23-kiosk-number");

    // Staff: hold the top-left corner, then a manager's PIN.
    QTest::mousePress(s.window, Qt::LeftButton, {}, QPoint(10, 10));
    QTest::qWait(3300);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, QPoint(10, 10));
    QTest::qWait(50);
    for (const char *key : {"approvalKey-1", "approvalKey-2", "approvalKey-3", "approvalKey-4", "approvalKey-OK"})
        s.tapItem(find(QString::fromLatin1(key)));
    QTest::qWait(50);
    CHECK_FALSE(s.pos.selfOrderInfo()[u"on"_s].toBool());
    CHECK_FALSE(kiosk->isVisible());
}

TEST_CASE("UI: switch user keeps each person's check for when they're back", "[flow][ui][switch]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"water"_s);
    // Mid-order: Check… -> Switch User.
    REQUIRE(s.c.jumpTo(u"check-options"_s));
    REQUIRE(s.pos.lines().size() == 1);
    s.shot("52-switch-user");
    s.c.activate(u"switch"_s);
    QTest::qWait(60);
    CHECK_FALSE(s.pos.loggedIn());
    CHECK(s.c.pageId() == u"login"_s);

    // Someone else in and out: no check of theirs.
    REQUIRE(s.pos.loginWithPin(u"2222"_s));
    QTest::qWait(30);
    CHECK_FALSE(s.pos.hasCheck());
    s.pos.logout();

    // The first one back: their takeout, on its order screen.
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(60);
    REQUIRE(s.pos.hasCheck());
    CHECK(s.pos.lines().size() == 1);
    CHECK(s.c.pageId() != u"tables"_s);
    // Once only: logging out from the floor without a check forgets it.
    s.pos.releaseCheck();
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(s.pos.hasCheck());
}

TEST_CASE("UI: each person's text size, left hand and start screen; a start screen per job", "[flow][ui][prefs]")
{
    Screen s;
    const auto rectOf = [&](const QString &id) {
        ZoneModel *m = s.c.zones();
        for (int r = 0; r < m->rowCount(); ++r)
            if (const QModelIndex i = m->index(r); m->data(i, ZoneModel::ZoneIdRole).toString() == id)
                return QRect(m->data(i, ZoneModel::ZoneXRole).toInt(), m->data(i, ZoneModel::ZoneYRole).toInt(),
                             m->data(i, ZoneModel::ZoneWRole).toInt(), m->data(i, ZoneModel::ZoneHRole).toInt());
        return QRect();
    };
    const auto save = [&](const QString &panel, const QString &name, const QVariantMap &changes) {
        const QVariantList records = s.pos.adminRecords(panel);
        for (int i = 0; i < records.size(); ++i) {
            QVariantMap r = records[i].toMap();
            if (!name.isEmpty() && r[u"name"_s] != name)
                continue;
            r.insert(changes);
            return s.pos.adminSave(panel, i, r);
        }
        return false;
    };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    // Servers start on the whole menu (Store settings).
    REQUIRE(save(u"store"_s, {}, {{u"startPage.server"_s, u"menu-all"_s}}));
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));   // Sam, a server
    QTest::qWait(30);
    CHECK(s.c.pageId() == u"menu-all"_s);
    CHECK_FALSE(s.pos.userPrefs().value(u"leftHanded"_s).toBool());
    const QRect normal = rectOf(u"order-list"_s);
    CHECK(normal.x() < 960);
    s.pos.logout();

    // Sam's own: bigger text, left-handed, starting on Lunch.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(save(u"employees"_s, u"Sam"_s, {{u"textSize"_s, u"130"_s}, {u"leftHanded"_s, true},
                                           {u"startPage"_s, u"index-lunch"_s}}));
    CHECK(s.c.pageId() != u"index-lunch"_s);   // the manager's own screen didn't change
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"index-lunch"_s);
    const QRect mirrored = rectOf(u"order-list"_s);
    CHECK(mirrored.x() == 1920 - normal.x() - normal.width());   // the check on the right
    CHECK(rectOf(u"tab-breakfast"_s).x() < rectOf(u"tab-lunch"_s).x());   // rows keep their order
    CHECK(rectOf(u"tab-breakfast"_s).x() == normal.x());
    CHECK(mirrored.width() == normal.width());
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"water"_s);
    QTest::qWait(60);
    s.shot("53-left-handed");
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(40);
    s.shot("54-left-handed-pay");
    CHECK(s.pos.userPrefs().value(u"textSize"_s).toInt() == 130);
    // The floor plan isn't mirrored.
    s.pos.releaseCheck();
    REQUIRE(s.c.jumpTo(u"tables"_s));
    CHECK(rectOf(u"quick"_s).x() > 960);
    s.pos.logout();
    CHECK(s.pos.userPrefs().isEmpty());
}

TEST_CASE("UI: after Send a server can't void or change items without a manager's PIN", "[flow][ui][approval]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                       // Sam, a server
    REQUIRE(s.pos.selectTable(u"T2"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.pos.addItem(u"water"_s);
    s.pos.addItem(u"cobb"_s);
    s.pos.finishChoosing();
    REQUIRE(s.pos.sendOrder());
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const qint64 cobb = s.pos.lines().value(1).toMap()[u"id"_s].toLongLong();
    s.pos.selectLine(cobb);
    QTest::qWait(40);

    // Sent: no − / + and no choices to change; more is a new line (Again).
    CHECK_FALSE(find(u"lineMore"_s));
    CHECK_FALSE(find(u"lineLess"_s));
    CHECK_FALSE(s.pos.setLineQuantity(cobb, 2));
    CHECK_FALSE(s.pos.lines().value(1).toMap()[u"choices"_s].toBool());

    // Void asks for a manager: a server's own PIN doesn't do it.
    s.c.activate(u"flow-void"_s);
    QTest::qWait(80);
    s.shot("66-void-reason");
    s.giveReason();
    REQUIRE(find(u"approvalKey-OK"_s));
    s.shot("64-manager-approval");
    const auto type = [&](const QString &pin) {
        for (const QChar ch : pin) {
            QQuickItem *k = find(u"approvalKey-"_s + ch);
            s.tapItem(k);
            QTest::qWait(30);
        }
        s.tapItem(find(u"approvalKey-OK"_s));
        QTest::qWait(60);
    };
    QTest::mouseClick(s.window, Qt::LeftButton, {}, QPoint(1450, 240));   // Mushroom Swiss, behind the dimmed pad
    QTest::qWait(60);
    type(u"1111"_s);
    CHECK_FALSE(s.pos.lines().value(1).toMap()[u"voided"_s].toBool());
    CHECK(s.pos.lines().size() == 2);                          // the pad's keys don't reach the menu behind it
    REQUIRE(find(u"approvalKey-OK"_s));                         // still waiting
    type(u"1234"_s);                                            // Morgan, a manager
    CHECK(s.pos.lines().value(1).toMap()[u"voided"_s].toBool());
    CHECK_FALSE(find(u"approvalKey-OK"_s));
    bool noted = false;
    for (const QVariant &e : s.pos.checkHistory())
        noted = noted || e.toMap().value(u"what"_s).toString().contains(u"approved by Morgan"_s);
    CHECK(noted);                                               // who approved it is on the check's history
}

TEST_CASE("UI: discounts are a manager's: $ Off and % Off of the amount typed", "[flow][ui][approval][discounts]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                       // Sam, a server
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"cobb"_s);                                   // $12.50
    s.pos.finishChoosing();
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto type = [&](const QString &pin) {
        for (const QChar ch : pin) {
            s.tapItem(find(u"approvalKey-"_s + ch));
            QTest::qWait(30);
        }
        s.tapItem(find(u"approvalKey-OK"_s));
        QTest::qWait(60);
    };
    const auto discounts = [&] { return s.pos.totals()[u"discounts"_s].toString(); };

    // A comp from the server: a manager's PIN first.
    s.c.activate(u"tender-comp"_s);
    QTest::qWait(60);
    s.giveReason();
    REQUIRE(find(u"approvalKey-OK"_s));
    s.tapItem(find(u"approvalKey-Cancel"_s));
    QTest::qWait(40);
    CHECK(s.pos.payments().isEmpty());

    // $5.00 off, typed: approved by Morgan.
    s.pos.entryKey(u"500"_s);
    s.c.activate(u"off-amount"_s);
    QTest::qWait(60);
    s.giveReason();
    REQUIRE(find(u"approvalKey-OK"_s));
    type(u"1234"_s);
    CHECK(discounts() == u"-$5.00"_s);
    CHECK(s.pos.payments().value(0).toMap()[u"name"_s].toString() == u"$5.00 off"_s);
    s.shot("65-custom-discount");

    // A manager's own % Off: no PIN asked. 20% of what's left of the cobb.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.openCheck(s.pos.openChecks().value(0).toMap()[u"id"_s].toLongLong()));
    REQUIRE(s.c.jumpTo(u"settle"_s));
    s.pos.entryKey(u"20"_s);
    s.c.activate(u"off-percent"_s);
    QTest::qWait(60);
    s.giveReason();
    CHECK_FALSE(find(u"approvalKey-OK"_s));
    CHECK(discounts() == u"-$7.50"_s);                          // 5.00 + 20% of 12.50
    CHECK_FALSE(s.pos.customDiscount(true));                    // nothing typed
    s.pos.entryKey(u"150"_s);
    CHECK_FALSE(s.pos.customDiscount(true));                    // over 100%
}

TEST_CASE("UI: a manager fixes time punches, with a reason that goes on the Labor report", "[flow][ui][punches]")
{
    Screen s;
    const auto stamp = [](std::int64_t ms) { return QDateTime::fromMSecsSinceEpoch(ms).toString(u"yyyy-MM-dd HH:mm"_s); };
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::int64_t hour = 3'600'000;
    // Sam forgot to clock out: still on the clock since 5 hours ago.
    s.pos.shared()->punches.push_back({901, "sam", now - 5 * hour, 0, {}, "server", vt::Money::fromCents(1200)});
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const auto records = [&] { return s.pos.adminRecords(u"punches"_s); };
    const auto indexOf = [&](qint64 id) {
        const QVariantList r = records();
        for (int i = 0; i < r.size(); ++i)
            if (r[i].toMap()[u"id"_s].toLongLong() == id)
                return i;
        return -1;
    };
    int i = indexOf(901);
    REQUIRE(i >= 0);
    QVariantMap sam = records()[i].toMap();
    CHECK(sam[u"clockOut"_s].toString().isEmpty());
    CHECK(sam[u"_detail"_s].toString().contains(u"still clocked in"_s));

    // Clocked out 1 hour ago, with a 15-minute break: a reason first.
    sam[u"clockOut"_s] = stamp(now - hour);
    sam[u"breaks"_s] = QDateTime::fromMSecsSinceEpoch(now - 3 * hour).toString(u"HH:mm"_s) + u"-"_s
                       + QDateTime::fromMSecsSinceEpoch(now - 3 * hour + 15 * 60'000).toString(u"HH:mm"_s);
    CHECK_FALSE(s.pos.adminSave(u"punches"_s, i, sam));
    sam[u"reason"_s] = u"forgot to clock out"_s;
    REQUIRE(s.pos.adminSave(u"punches"_s, i, sam));
    const core::TimePunch *fixed = nullptr;
    for (const core::TimePunch &p : s.pos.shared()->punches)
        if (p.id == 901)
            fixed = &p;
    REQUIRE(fixed);
    CHECK_FALSE(fixed->open());
    CHECK(fixed->breaks.size() == 1);
    REQUIRE(s.pos.shared()->settings.punchChanges.size() == 1);
    CHECK(s.pos.shared()->settings.punchChanges[0].why == "forgot to clock out");
    CHECK(s.pos.shared()->settings.punchChanges[0].by == "Morgan (Manager)");

    // A missed punch for Casey yesterday; one overlapping it is refused.
    QVariantMap casey = s.pos.adminNewRecord(u"punches"_s);
    casey[u"employeeId"_s] = u"casey"_s;
    casey[u"clockIn"_s] = stamp(now - 30 * hour);
    casey[u"clockOut"_s] = stamp(now - 24 * hour);
    casey[u"reason"_s] = u"tablet was down"_s;
    REQUIRE(s.pos.adminSave(u"punches"_s, -1, casey));
    casey[u"clockIn"_s] = stamp(now - 26 * hour);
    casey[u"clockOut"_s] = stamp(now - 25 * hour);
    CHECK_FALSE(s.pos.adminSave(u"punches"_s, -1, casey));
    casey[u"clockIn"_s] = stamp(now + hour);                     // the future
    CHECK_FALSE(s.pos.adminSave(u"punches"_s, -1, casey));

    // Removing needs a reason too (the Remove button just says how).
    i = indexOf(901);
    CHECK_FALSE(s.pos.adminDelete(u"punches"_s, i));

    // The Labor report marks it and lists the change and why.
    const core::Report labor = s.pos.buildReport(u"labor"_s);
    bool marked = false, listed = false;
    for (const core::ReportRow &row : labor.rows) {
        marked = marked || (!row.cells.empty() && row.cells[0] == "Sam *");
        listed = listed || (row.cells.size() == 5 && row.cells[4] == "forgot to clock out");
    }
    CHECK(marked);
    CHECK(listed);

    // On screen: Schedule -> Time Punches.
    REQUIRE(s.c.jumpTo(u"admin-schedule"_s));
    s.c.activate(u"punches"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"admin-punches"_s);
    s.shot("67-time-punches");

    QVariantMap gone = records()[indexOf(901)].toMap();
    gone[u"remove"_s] = true;
    gone[u"reason"_s] = u"duplicate"_s;
    REQUIRE(s.pos.adminSave(u"punches"_s, indexOf(901), gone));
    CHECK(indexOf(901) < 0);
    CHECK(s.pos.shared()->settings.punchChanges.size() == 3);
}

TEST_CASE("Layout files carry their pictures and fonts to another store", "[flow][ui][bundle]")
{
    QTemporaryDir dir;
    QImage image(40, 20, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    QFile font(u"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"_s);
    const bool haveFont = font.open(QIODevice::ReadOnly);
    const QByteArray fontBytes = haveFont ? font.readAll() : QByteArray();
    const QUrl withPicture = QUrl::fromLocalFile(dir.filePath(u"with-picture.vtlayout.json"_s));
    const QUrl exported = QUrl::fromLocalFile(dir.filePath(u"bundle.vtlayout.json"_s));
    {
        // Store A: a picture on the login page, and a font.
        Screen a;
        REQUIRE(a.pos.loginWithPin(u"1234"_s));
        REQUIRE(a.pos.addStoreImage(u"menu-board.png"_s, QString::fromLatin1(png.toBase64())));
        if (haveFont)
            REQUIRE(a.pos.addStoreImage(u"House Font.ttf"_s, QString::fromLatin1(fontBytes.toBase64())));
        QJsonObject layout = a.c.layout().toJson();
        QJsonArray pages = layout.value(u"pages"_s).toArray();
        for (int i = 0; i < pages.size(); ++i) {
            QJsonObject page = pages[i].toObject();
            if (page.value(u"role"_s).toString() != u"login")
                continue;
            QJsonArray zones = page.value(u"zones"_s).toArray();
            for (int z = 0; z < zones.size(); ++z)
                if (zones[z].toObject().value(u"id"_s).toString() == u"logo") {
                    QJsonObject zone = zones[z].toObject();
                    zone.insert(u"imagePath"_s, u"store:menu-board.png"_s);
                    zones[z] = zone;
                }
            page.insert(u"zones"_s, zones);
            pages[i] = page;
        }
        layout.insert(u"pages"_s, pages);
        QFile f(withPicture.toLocalFile());
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(layout).toJson());
        f.close();
        a.c.enterEditMode();
        REQUIRE(a.c.editor()->importLayout(withPicture));
        REQUIRE(a.c.editor()->exportLayout(exported));
        QFile out(exported.toLocalFile());
        REQUIRE(out.open(QIODevice::ReadOnly));
        const QJsonObject images = QJsonDocument::fromJson(out.readAll()).object().value(u"images"_s).toObject();
        CHECK(images.contains(u"menu-board.png"_s));
        CHECK(images.size() == (haveFont ? 2 : 1));              // only what's used, and the fonts
        a.c.leaveEditMode(false);
    }
    // Store B: importing brings them; the saved pages don't carry the pictures.
    Screen b;
    REQUIRE(b.pos.loginWithPin(u"1234"_s));
    const auto names = [&] {
        QStringList out;
        for (const QVariant &v : b.pos.storeImages())
            out << v.toMap()[u"name"_s].toString();
        return out;
    };
    CHECK_FALSE(names().contains(u"menu-board.png"_s));
    b.c.enterEditMode();
    REQUIRE(b.c.editor()->importLayout(exported));
    CHECK(names().contains(u"menu-board.png"_s));
    if (haveFont)
        CHECK(names().join(u","_s).contains(u".ttf"_s, Qt::CaseInsensitive));
    CHECK_FALSE(b.c.editor()->editor().layout().toJson().contains(u"images"_s));
}

TEST_CASE("UI: a manager arranges the self-filling menu by touch", "[flow][ui][arrange]")
{
    Screen s;
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = find(name);
        INFO("looking for " << name.toStdString());
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    const auto indexOf = [&](const QString &id) {
        const QVariantList items = s.pos.menuItems();
        for (int i = 0; i < items.size(); ++i)
            if (items[i].toMap()[u"id"_s] == id)
                return i;
        return -1;
    };
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    QTest::qWait(60);
    CHECK_FALSE(find(u"menuArrange"_s));                        // servers don't
    s.pos.releaseCheck();
    s.pos.logout();

    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);   // whatever meal it is now
    QTest::qWait(60);
    REQUIRE(indexOf(u"classic-burger"_s) < indexOf(u"cheeseburger"_s));
    tap(u"menuArrange"_s);
    tap(u"menuItem-cheeseburger"_s);
    CHECK(s.pos.lines().isEmpty());                            // picked, not ordered
    tap(u"arrangeEarlier"_s);
    CHECK(indexOf(u"cheeseburger"_s) < indexOf(u"classic-burger"_s));
    tap(u"arrangeColor"_s);
    s.shot("78-arrange-color");
    tap(u"color-13"_s);                                        // green; still arranging after the change
    CHECK(s.pos.menuItems()[indexOf(u"cheeseburger"_s)].toMap()[u"buttonColor"_s] == u"#1f8a4c"_s);
    REQUIRE(find(u"arrangeBar"_s));
    s.shot("78-arrange-menu");
    tap(u"arrangeDone"_s);
    CHECK_FALSE(find(u"arrangeBar"_s));
    tap(u"menuItem-cheeseburger"_s);                           // orders again
    CHECK(s.pos.lines().size() == 1);
}

TEST_CASE("UI: colors: the store's, picked by touch; one mixed is kept for the store", "[flow][ui][colors]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    s.tapItem(by(u"builderCategory-burgers"_s));
    s.tapItem(by(u"builderEditCategory"_s));
    QTest::qWait(60);
    QQuickItem *picker = by(u"builderCategoryColor"_s);
    REQUIRE(picker);
    // Thirty to choose from: a light blue.
    QQuickItem *lightBlue = Screen::findBy(picker, "objectName", u"color-25"_s);
    REQUIRE(lightBlue);
    s.tapItem(lightBlue);
    QTest::qWait(30);
    CHECK(shown(u"builderNotSaved"_s));
    s.tapItem(by(u"builderSaveCategory"_s));
    QTest::qWait(60);
    for (const QVariant &c : s.pos.menuCategories())
        if (c.toMap()[u"id"_s] == u"burgers"_s)
            CHECK(c.toMap()[u"color"_s] == u"#6c9ce0"_s);

    // Mixed: typed in, used, and kept with the store's colors.
    CHECK(s.pos.customColors().isEmpty());
    s.tapItem(Screen::findBy(picker, "objectName", u"colorMix"_s));
    QTest::qWait(100);
    REQUIRE(shown(u"colorHex"_s));
    QQuickItem *hex = by(u"colorHex"_s);
    s.tapItem(hex);
    QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
    for (const char *ch = "#123abc"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(30);
    if (qEnvironmentVariableIsSet("VTM_SHOTS"))
        s.window->grabWindow().save(qEnvironmentVariable("VTM_SHOTS") + u"/color-mixer.png"_s);
    s.tapItem(by(u"colorUse"_s));
    QTest::qWait(60);
    CHECK(s.pos.customColors() == QStringList{u"#123abc"_s});
    CHECK_FALSE(shown(u"colorUse"_s));
    s.tapItem(by(u"builderSaveCategory"_s));
    QTest::qWait(60);
    for (const QVariant &c : s.pos.menuCategories())
        if (c.toMap()[u"id"_s] == u"burgers"_s)
            CHECK(c.toMap()[u"color"_s] == u"#123abc"_s);
    if (qEnvironmentVariableIsSet("VTM_SHOTS"))
        s.window->grabWindow().save(qEnvironmentVariable("VTM_SHOTS") + u"/color-category.png"_s);
}

TEST_CASE("UI: a category's buttons on the order screen: small or large, prices or not", "[flow][ui][categorylook]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(80);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto widthOf = [&] {
        QQuickItem *b = find(u"menuItem-classic-burger"_s);
        return b ? b->width() : 0.0;
    };
    const auto look = [&](const QString &size, bool hidePrice) {
        REQUIRE(s.pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"buttonSize"_s, size},
                                    {u"hidePrice"_s, hidePrice}}));
        QTest::qWait(80);
    };
    look(u"small"_s, false);
    const double small = widthOf();
    REQUIRE(find(u"menuPrice-classic-burger"_s));
    look(u"large"_s, true);
    const double large = widthOf();
    CHECK(small > 0);
    CHECK(large > small * 1.6);
    CHECK_FALSE(find(u"menuPrice-classic-burger"_s));      // no price on it
    s.shot("79-category-large");
    // Sold out still says so.
    REQUIRE(s.pos.setAvailable(u"classic-burger"_s, false));
    QTest::qWait(60);
    CHECK(find(u"menuPrice-classic-burger"_s));
}

TEST_CASE("UI: a manager changes or adds an item right on the order screen", "[flow][ui][quickedit]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(80);
    QQuickItem *root = s.window->contentItem();
    const auto find = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = find(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    const auto type = [&](const QString &field, const char *text) {
        tap(field);
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
        QTest::qWait(30);
    };
    const auto hold = [&](const QString &name) {
        QQuickItem *it = find(name);
        REQUIRE(it);
        const QPoint at = it->mapToScene(QPointF(it->width() / 2, it->height() / 2)).toPoint();
        QTest::mousePress(s.window, Qt::LeftButton, {}, at);
        QTest::qWait(900);
        QTest::mouseRelease(s.window, Qt::LeftButton, {}, at);
        QTest::qWait(60);
    };

    // Held: its card, not an order.
    hold(u"menuItem-classic-burger"_s);
    CHECK(s.pos.lines().isEmpty());
    REQUIRE(find(u"quickPrice"_s));
    CHECK(find(u"quickPrice"_s)->property("text").toString() == u"11.50"_s);
    type(u"quickPrice"_s, "12.25");
    QQuickItem *picker = find(u"quickColor"_s);
    REQUIRE(picker);
    s.tapItem(Screen::findBy(picker, "objectName", u"color-0"_s));
    s.shot("80-quick-edit");
    tap(u"quickSave"_s);
    CHECK_FALSE(find(u"quickSave"_s));
    CHECK(s.pos.findItem(u"classic-burger"_s)->price.cents() == 1225);
    CHECK(s.pos.findItem(u"classic-burger"_s)->buttonColor == "#7a1f1f");
    // Sold out from here, and back.
    hold(u"menuItem-bacon-burger"_s);
    tap(u"quickSoldOut"_s);
    tap(u"quickSave"_s);
    CHECK_FALSE(s.pos.findItem(u"bacon-burger"_s)->available);
    hold(u"menuItem-bacon-burger"_s);                            // held, though it can't be ordered
    tap(u"quickForSale"_s);
    tap(u"quickSave"_s);
    CHECK(s.pos.findItem(u"bacon-burger"_s)->available);
    tap(u"menuItem-classic-burger"_s);                          // a touch still orders
    CHECK(s.pos.lines().size() == 1);

    // A server holding one: nothing.
    s.pos.releaseCheck();
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"2222"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(80);
    hold(u"menuItem-cheeseburger"_s);
    CHECK_FALSE(find(u"quickPrice"_s));
    s.pos.releaseCheck();
    s.pos.logout();

    // Arrange -> + New Item: added to the category shown.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(80);
    tap(u"menuArrange"_s);
    tap(u"arrangeAdd"_s);
    REQUIRE(find(u"quickName"_s));
    CHECK_FALSE(find(u"quickSave"_s)->isEnabled());             // a name and a price first
    type(u"quickName"_s, "Patty Melt");
    type(u"quickPrice"_s, "13");
    tap(u"quickSave"_s);
    const core::MenuItem *melt = nullptr;
    for (const core::MenuItem &m : s.pos.shared()->menu)
        if (m.name == "Patty Melt")
            melt = &m;
    REQUIRE(melt);
    CHECK(melt->family == "burgers");
    CHECK(melt->price.cents() == 1300);
    CHECK(find(u"menuItem-"_s + QString::fromStdString(melt->id)));

    // + New Item with sizes, a choice and what's on it, in one go.
    tap(u"arrangeAdd"_s);
    type(u"quickName"_s, "Slider");
    tap(u"quickAddSize"_s);                                     // Small and Large
    CHECK_FALSE(find(u"quickPrice"_s));                          // by size instead
    CHECK_FALSE(find(u"quickSave"_s)->isEnabled());             // their prices first
    type(u"quickSizePrice-0"_s, "4");
    type(u"quickSizePrice-1"_s, "6.50");
    type(u"quickOnIt"_s, "pickles, onion");
    type(u"quickKitchenName"_s, "SLD");
    for (const QVariant &g : s.pos.choiceGroups())
        if (!g.toMap()[u"own"_s].toBool()) {
            tap(u"quickGroup-"_s + g.toMap()[u"id"_s].toString());
            break;
        }
    s.shot("81-quick-card-more");
    REQUIRE(find(u"quickSave"_s)->isEnabled());
    tap(u"quickSave"_s);
    const core::MenuItem *slider = nullptr;
    for (const core::MenuItem &m : s.pos.shared()->menu)
        if (m.name == "Slider")
            slider = &m;
    REQUIRE(slider);
    CHECK(slider->price.cents() == 400);
    CHECK(slider->kitchenName == "SLD");
    const QVariantMap sliderRow = [&] {
        for (const QVariant &v : s.pos.menuItems())
            if (v.toMap()[u"name"_s] == u"Slider"_s)
                return v.toMap();
        return QVariantMap();
    }();
    CHECK(sliderRow[u"sizes"_s].toList().size() == 2);
    CHECK(sliderRow[u"onIt"_s].toStringList() == QStringList{u"pickles"_s, u"onion"_s});
    CHECK(sliderRow[u"groups"_s].toStringList().size() == 1);
    CHECK(s.pos.menuUndoText() == u"Slider"_s);                  // one Undo for all of it

    // Picked, Edit…, All Settings…: the Menu Builder with it open.
    tap(u"menuItem-cheeseburger"_s);
    tap(u"arrangeEdit"_s);
    tap(u"quickAll"_s);
    QTest::qWait(150);
    CHECK(s.c.pageId() == u"menu-builder"_s);
    root = s.window->contentItem();
    REQUIRE(find(u"builderName"_s));
    CHECK(find(u"builderName"_s)->property("text").toString() == u"Cheeseburger"_s);
}

TEST_CASE("UI: Menu Builder: several items at once; an item dragged onto another category", "[flow][ui][menubuild][several]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    tap(u"builderCategory-burgers"_s);
    tap(u"builderSelect"_s);
    tap(u"builderItem-classic-burger"_s);
    tap(u"builderItem-bacon-burger"_s);
    CHECK_FALSE(by(u"builderName"_s));                         // choosing, not opening
    s.shot("81-builder-select");
    tap(u"builderSelectColor"_s);
    tap(u"color-15"_s);
    CHECK(s.pos.findItem(u"bacon-burger"_s)->buttonColor == "#2b62b0");
    CHECK(s.pos.findItem(u"classic-burger"_s)->buttonColor == "#2b62b0");
    CHECK(s.pos.findItem(u"cheeseburger"_s)->buttonColor.empty());
    tap(u"builderSelectMove"_s);
    tap(u"builderMoveTo-salads"_s);
    CHECK(s.pos.findItem(u"bacon-burger"_s)->family == "salads");
    CHECK(s.pos.findItem(u"classic-burger"_s)->family == "salads");
    tap(u"builderSelect"_s);                                   // Done
    CHECK_FALSE(by(u"builderSelectBar"_s));

    // Held, dragged onto Breakfast on the left.
    QQuickItem *tile = by(u"builderItem-cheeseburger"_s);
    QQuickItem *plates = by(u"builderCategory-breakfast"_s);
    REQUIRE(tile);
    REQUIRE(plates);
    const QPoint from = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2)).toPoint();
    const QPoint to = plates->mapToScene(QPointF(plates->width() / 2, plates->height() / 2)).toPoint();
    QTest::mousePress(s.window, Qt::LeftButton, {}, from);
    QTest::qWait(500);
    for (int i = 1; i <= 10; ++i) {
        QTest::mouseMove(s.window, from + (to - from) * i / 10);
        QTest::qWait(15);
    }
    CHECK(by(u"builderDropOn-breakfast"_s));
    s.shot("82-builder-drag-category");
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, to);
    QTest::qWait(80);
    CHECK(s.pos.findItem(u"cheeseburger"_s)->family == "breakfast");
    CHECK_FALSE(by(u"builderGhost"_s));
}

TEST_CASE("UI: Menu Builder: a new item like another", "[flow][ui][menubuild][like]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"cheeseburger"_s}, {u"name"_s, u"Cheeseburger"_s}, {u"allergens"_s, QStringList{u"dairy"_s}},
                                    {u"onIt"_s, u"lettuce, tomato"_s}, {u"prepMinutes"_s, u"9"_s}}));
    const core::MenuItem cheese = *s.pos.findItem(u"cheeseburger"_s);
    REQUIRE_FALSE(cheese.modifierGroups.empty());
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    tap(u"builderCategory-burgers"_s);
    tap(u"builderAddItem"_s);
    tap(u"builderLike"_s);
    tap(u"builderLike-cheeseburger"_s);
    tap(u"builderName"_s);
    for (const char *ch = "Patty Melt"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    tap(u"builderPrice"_s);
    for (const char *ch = "13.50"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    QTest::qWait(30);
    tap(u"builderSave"_s);
    QTest::qWait(60);
    const core::MenuItem *melt = s.pos.findItem(u"Patty Melt"_s);
    REQUIRE(melt);
    CHECK(melt->price.cents() == 1350);
    CHECK(melt->allergens == cheese.allergens);
    CHECK(melt->prepMinutes == 9);
    CHECK(melt->printer == cheese.printer);
    // The same choices, and its own What's on it (lettuce, tomato).
    CHECK(melt->modifierGroups.size() == cheese.modifierGroups.size());
    QStringList onIt;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Patty Melt"_s)
            onIt = m.toMap()[u"onIt"_s].toStringList();
    CHECK(onIt == QStringList{u"lettuce"_s, u"tomato"_s});
}

TEST_CASE("UI: the order screen: sections, a new row, shades; the Menu Builder's preview", "[flow][ui][sections]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    for (const char *id : {"classic-burger", "cheeseburger", "bacon-burger"})
        REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, QString::fromLatin1(id)}, {u"name"_s, QString::fromStdString(s.pos.findItem(QString::fromLatin1(id))->name)},
                                        {u"section"_s, u"Beef"_s}}));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"veggie-burger"_s}, {u"name"_s, u"Veggie Burger"_s}, {u"section"_s, u"Meatless"_s}}));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"bacon-burger"_s}, {u"name"_s, u"Bacon Burger"_s}, {u"breakBefore"_s, u"row"_s}}));
    REQUIRE(s.pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"shades"_s, true}}));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto find = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    REQUIRE(find(u"menuSection-Beef"_s));
    REQUIRE(find(u"menuSection-Meatless"_s));
    const auto at = [&](const QString &id) {
        QQuickItem *b = find(u"menuItem-"_s + id);
        REQUIRE(b);
        return b->mapToScene(QPointF(0, 0));
    };
    // Beef's under its heading, Meatless under its own, after.
    CHECK(at(u"veggie-burger"_s).y() > find(u"menuSection-Meatless"_s)->mapToScene(QPointF(0, 0)).y());
    CHECK(at(u"veggie-burger"_s).y() > at(u"cheeseburger"_s).y());
    // Bacon Burger starts a row: at the left, below the first two.
    CHECK(at(u"bacon-burger"_s).x() == Catch::Approx(at(u"classic-burger"_s).x()));
    CHECK(at(u"bacon-burger"_s).y() > at(u"classic-burger"_s).y());
    // Shades: the same category, not quite the same color.
    const QColor a = find(u"menuItem-classic-burger"_s)->property("color").value<QColor>();
    const QColor b = find(u"menuItem-cheeseburger"_s)->property("color").value<QColor>();
    CHECK(a != b);
    s.shot("83-menu-sections");
    s.pos.releaseCheck();

    // The Menu Builder: the same, small; touching one opens its card.
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    root = s.window->contentItem();
    s.tapItem(find(u"builderCategory-burgers"_s));
    QTest::qWait(60);
    s.tapItem(find(u"builderPreview"_s));
    QTest::qWait(80);
    QQuickItem *pane = find(u"builderPreviewPane"_s);
    REQUIRE(pane);
    QQuickItem *veggie = Screen::findBy(pane, "objectName", u"menuItem-veggie-burger"_s);
    REQUIRE(veggie);
    REQUIRE(Screen::findBy(pane, "objectName", u"menuSection-Meatless"_s));
    s.shot("84-builder-preview");
    s.tapItem(veggie);
    QTest::qWait(60);
    REQUIRE(find(u"builderName"_s));
    CHECK(find(u"builderName"_s)->property("text").toString() == u"Veggie Burger"_s);
}

TEST_CASE("UI: Menu Builder: a color theme for every category", "[flow][ui][themes]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    tap(u"builderThemes"_s);
    CHECK_FALSE(by(u"builderThemeUse"_s)->isEnabled());       // a theme first
    tap(u"builderTheme-bright"_s);
    s.shot("85-builder-themes");
    tap(u"builderThemeUse"_s);
    const QVariantList cats = s.pos.menuCategories();
    REQUIRE(cats.size() >= 2);
    CHECK(cats[0].toMap()[u"color"_s] == u"#e53935"_s);
    CHECK(cats[1].toMap()[u"color"_s] == u"#fb8c00"_s);
    CHECK(s.pos.menuUndoText() == u"Category colors"_s);
}

TEST_CASE("Printed menu: pictures, sizes, page breaks", "[printmenu]")
{
    MenuPrint m;
    m.title = u"Tacos"_s;
    QImage red(200, 120, QImage::Format_RGB32);
    red.fill(Qt::red);
    MenuPrintCategory c;
    c.name = u"Tacos"_s;
    c.items << MenuPrintItem{u"Al Pastor"_s, u"Pork, pineapple"_s, QString(), {u"Small $3.00"_s, u"Large $5.00"_s}, {}, red};
    m.categories << c;
    const QImage page = menuPreview(m, 600);
    int reds = 0;
    for (int y = 0; y < page.height(); y += 2)
        for (int x = 0; x < page.width(); x += 2)
            reds += page.pixel(x, y) == qRgb(255, 0, 0);
    CHECK(reds > 100);
    m.pictures = false;
    const QImage plain = menuPreview(m, 600);
    reds = 0;
    for (int y = 0; y < plain.height(); y += 2)
        for (int x = 0; x < plain.width(); x += 2)
            reds += plain.pixel(x, y) == qRgb(255, 0, 0);
    CHECK(reds == 0);
    // Many items: more pages.
    for (int i = 0; i < 200; ++i)
        m.categories[0].items << MenuPrintItem{u"Taco %1"_s.arg(i), {}, u"$3.00"_s, {}, {}, {}};
    CHECK(menuPageCount(m) > 1);
}

TEST_CASE("UI: Menu Builder: a printed menu, as a PDF", "[flow][ui][printmenu]")
{
    Screen s(false, 1280, 800);
    QTemporaryDir dir;
    s.c.setExportDirectory(dir.path());
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    s.shot("89-builder-print-key");
    tap(u"builderPrintMenu"_s);
    QQuickItem *preview = nullptr;
    for (int i = 0; i < 100 && !(preview && !preview->property("source").toString().isEmpty()
                                 && preview->property("status").toInt() == 1); ++i) {   // Image.Ready
        QTest::qWait(30);
        preview = by(u"printPreview"_s);
    }
    REQUIRE(preview);
    CHECK(preview->property("status").toInt() == 1);
    s.shot("88-printed-menu");
    tap(u"printOneColumn"_s);
    CHECK(by(u"printOneColumn"_s)->property("text").toString().startsWith(u"✓"_s));
    CHECK_FALSE(by(u"printTwoColumns"_s)->property("text").toString().startsWith(u"✓"_s));
    tap(u"printOneColumn"_s);                                   // still chosen
    CHECK(by(u"printOneColumn"_s)->property("text").toString().startsWith(u"✓"_s));
    tap(u"printTwoColumns"_s);

    // One column takes more pages than two.
    const int two = s.c.menuPages({{u"columns"_s, 2}});
    CHECK(two >= 1);
    CHECK(s.c.menuPages({{u"columns"_s, 1}}) >= two);
    // Only one category.
    const QString first = s.pos.menuCategories()[0].toMap()[u"id"_s].toString();
    CHECK(s.c.menuPages({{u"categories"_s, QStringList{first}}}) == 1);

    tap(u"printSave"_s);
    const QStringList files = QDir(dir.path()).entryList({u"menu-*.pdf"_s});
    REQUIRE(files.size() == 1);
    QFile pdf(QDir(dir.path()).filePath(files[0]));
    REQUIRE(pdf.open(QIODevice::ReadOnly));
    CHECK(pdf.read(5) == "%PDF-");
    CHECK(pdf.size() > 2000);
}

TEST_CASE("UI: Menu Builder: an item's sizes", "[flow][ui][sizes]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    const auto type = [&](const QString &field, const char *text) {
        tap(field);
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
        QTest::qWait(30);
    };
    tap(u"builderCategory-burgers"_s);
    tap(u"builderItem-kids-burger"_s);
    CHECK(by(u"builderSizesText"_s)->property("text").toString() == u"One size"_s);
    tap(u"builderSizes"_s);
    REQUIRE(shown(u"builderSizesSave"_s));
    REQUIRE(by(u"builderSizeName-1"_s));                       // Small and Large to start
    type(u"builderSizePrice-0"_s, "7.00");
    type(u"builderSizePrice-1"_s, "9.25");
    s.shot("86-builder-sizes");
    tap(u"builderSizesSave"_s);
    QTest::qWait(60);
    CHECK(s.pos.findItem(u"kids-burger"_s)->price.cents() == 700);
    CHECK(by(u"builderSizesText"_s)->property("text").toString() == u"Small 7.00  ·  Large 9.25"_s);
    CHECK(by(u"builderPrice"_s)->property("text").toString() == u"7.00"_s);
    CHECK_FALSE(shown(u"builderNotSaved"_s));
}

TEST_CASE("UI: Menu Builder: a ready-made picture for an item, on the order screen", "[flow][ui][readypicture]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    tap(u"builderCategory-burgers"_s);
    tap(u"builderItem-classic-burger"_s);
    tap(u"builderReadyPicture"_s);
    s.shot("87-builder-pictures");
    tap(u"builderPicture-4"_s);                                // 🍔
    tap(u"builderSave"_s);
    QTest::qWait(60);
    const std::string ref = s.pos.findItem(u"classic-burger"_s)->image;
    CHECK(ref == "store:emoji-1f354.png");
    // Drawn: in color, not blank.
    REQUIRE(s.pos.shared()->images.contains("emoji-1f354.png"));
    const QImage img = QImage::fromData(s.pos.shared()->images.at("emoji-1f354.png"));
    REQUIRE_FALSE(img.isNull());
    QSet<QRgb> colors;
    int drawn = 0;
    for (int y = 0; y < img.height(); y += 4)
        for (int x = 0; x < img.width(); x += 4) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() > 200) {
                ++drawn;
                colors.insert(qRgb(c.red() / 32, c.green() / 32, c.blue() / 32));
            }
        }
    CHECK(drawn > 400);
    CHECK(colors.size() > 4);

    // Photos on for Burgers: on the order screen.
    REQUIRE(s.pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"photos"_s, true}}));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"burgers"_s);
    QTest::qWait(300);
    s.shot("88-menu-pictures");
}

TEST_CASE("UI: clocking out, asked for the cash tips kept", "[flow][ui][cashtips]")
{
    Screen s;
    s.pos.shared()->settings.declareCashTips = true;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.clockInEmployee(u"jo"_s));
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"4444"_s));
    REQUIRE(s.pos.clockOut());
    QTest::qWait(80);
    QQuickItem *root = s.window->contentItem();
    const auto find = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = find(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(30);
    };
    REQUIRE(find(u"cashTipsSave"_s));
    for (const char *k : {"1", "2", ".", "5", "0", "0"})                     // the last 0: two decimals at most
        tap(u"cashTipsKey-"_s + QLatin1String(k));
    CHECK(find(u"cashTipsAmount"_s)->property("text").toString() == u"$12.50"_s);
    s.shot("89-cash-tips");
    tap(u"cashTipsSave"_s);
    QTest::qWait(60);
    CHECK_FALSE(find(u"cashTipsSave"_s));
    const auto p = std::ranges::find_if(s.pos.shared()->punches, [](const core::TimePunch &x) { return x.employeeId == "jo"; });
    REQUIRE(p != s.pos.shared()->punches.end());
    CHECK(p->cashTipsDeclared);
    CHECK(p->cashTips.cents() == 1250);
}

TEST_CASE("UI: Menu Builder: prices set to change tomorrow", "[flow][ui][scheduledprices]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    const qint64 classic = s.pos.findItem(u"classic-burger"_s)->price.cents();
    tap(u"builderCategory-burgers"_s);
    CHECK_FALSE(by(u"builderPriceChanges"_s));
    tap(u"builderPrices"_s);
    tap(u"builderPricesAmount"_s);
    QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
    for (const char *ch = "10"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    QTest::qWait(30);
    tap(u"builderPricesLater"_s);
    tap(u"builderPricesDay-1"_s);                                   // tomorrow, 6:00 AM
    s.shot("90-prices-later");
    tap(u"builderPricesApply"_s);
    QTest::qWait(60);
    CHECK(s.pos.findItem(u"classic-burger"_s)->price.cents() == classic);   // not yet
    REQUIRE(s.pos.priceChanges().size() == 1);
    const QVariantMap c = s.pos.priceChanges()[0].toMap();
    CHECK(c[u"label"_s] == u"Burgers +10%"_s);
    CHECK(QDateTime::fromMSecsSinceEpoch(c[u"at"_s].toLongLong()) == QDateTime(QDate::currentDate().addDays(1), QTime(6, 0)));
    REQUIRE(by(u"builderPriceChanges"_s));
    // Canceled from the list.
    tap(u"builderPriceChanges"_s);
    tap(u"builderPriceChangeCancel-"_s + QString::number(c[u"id"_s].toLongLong()));
    CHECK(s.pos.priceChanges().isEmpty());
}

TEST_CASE("UI: Menu Builder: a choice priced by size", "[flow][ui][sizeprices]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.setItemSizes(u"kids-burger"_s, {QVariantMap{{u"name"_s, u"Small"_s}, {u"price"_s, u"7.50"_s}},
                                                 QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, u"9.50"_s}}}));
    REQUIRE(s.pos.saveChoiceGroup({{u"name"_s, u"Add"_s}, {u"min"_s, 0}, {u"max"_s, 0},
                                   {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"Cheese"_s}, {u"price"_s, u"1.00"_s}}}}}));
    QString add;
    for (const QVariant &g : s.pos.choiceGroups())
        if (g.toMap()[u"name"_s] == u"Add"_s)
            add = g.toMap()[u"id"_s].toString();
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto tap = [&](const QString &name) {
        QQuickItem *it = by(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(60);
    };
    tap(u"builderModeChoices"_s);
    QQuickItem *groups = by(u"builderGroups"_s);
    REQUIRE(groups);
    groups->setProperty("contentY", groups->property("contentHeight").toReal() - groups->height());
    QTest::qWait(60);
    tap(u"builderGroupRow-"_s + add);
    QQuickItem *large = by(u"builderSizePrice-0-Large"_s);
    REQUIRE(large);
    QQuickItem *card = by(u"builderCard"_s);
    card->setProperty("contentY", card->property("contentHeight").toReal() - card->height());
    QTest::qWait(30);
    s.tapItem(large);
    for (const char *ch = "1.75"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    QTest::qWait(30);
    s.shot("91-choice-size-prices");
    tap(u"builderSaveGroup"_s);
    QTest::qWait(60);
    CHECK(s.pos.shared()->settings.modifierGroup(add.toStdString())->options[0].sizePrices.at("large").cents() == 175);
}

TEST_CASE("UI: a terminal's own look", "[flow][ui][terminallook]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    const auto fillOf = [&](const QString &id) {
        ZoneModel *m = s.c.zones();
        for (int r = 0; r < m->rowCount(); ++r)
            if (m->data(m->index(r), ZoneModel::ZoneIdRole).toString() == id)
                return m->data(m->index(r), ZoneModel::StyleNormalRole).toMap().value(u"fill"_s).toString();
        return QString();
    };
    const QString before = fillOf(u"checks"_s);
    const QVariantList looks = s.c.looks();
    REQUIRE(looks.size() >= 2);
    QString lookId;
    for (const QVariant &v : looks)
        if (v.toMap()[u"colors"_s].toStringList().value(1) != before)
            lookId = v.toMap()[u"id"_s].toString();
    REQUIRE_FALSE(lookId.isEmpty());

    QVariantMap t = s.pos.adminNewRecord(u"terminals"_s);
    t[u"name"_s] = s.pos.terminalName();
    t[u"look"_s] = lookId;
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, t));
    QTest::qWait(60);
    CHECK(s.pos.terminalLook() == lookId);
    CHECK(fillOf(u"checks"_s) != before);                      // this screen, recolored
    s.shot("77-terminal-look");

    // The store's look is untouched; clearing goes back to it.
    t = s.pos.adminRecords(u"terminals"_s).last().toMap();
    t[u"look"_s] = QString();
    REQUIRE(s.pos.adminSave(u"terminals"_s, s.pos.adminRecords(u"terminals"_s).size() - 1, t));
    QTest::qWait(60);
    CHECK(fillOf(u"checks"_s) == before);
}

TEST_CASE("UI: course pacing: fire the next course in 10 minutes", "[flow][ui][pacing]")
{
    Screen s;
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"house-salad"_s);                            // course 1
    REQUIRE(s.pos.chooseOption(u"dressing"_s, 0));
    REQUIRE(s.pos.chooseOption(u"salad-protein"_s, 0));
    s.pos.finishChoosing();
    s.pos.selectLine(0);                                         // course 2 for what's ordered next
    s.pos.setCourse(2);
    s.pos.addItem(u"cobb"_s);                                   // course 2: held
    REQUIRE(s.pos.chooseOption(u"salad-protein"_s, 0));
    s.pos.finishChoosing();
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(60);

    s.tapItem(find(u"fireLater"_s));
    QTest::qWait(40);
    REQUIRE(find(u"fireIn-10"_s));
    s.shot("76-fire-in");
    s.tapItem(find(u"fireIn-10"_s));
    QTest::qWait(40);
    QVariantMap check = s.pos.checkInfo();
    CHECK_FALSE(check[u"firesAt"_s].toString().isEmpty());
    CHECK(check[u"heldCount"_s].toInt() == 1);
    CHECK(s.pos.fireDueOrders() == 0);                          // not yet

    // Ten minutes later (the store's 30-second check): it fires by itself.
    for (auto &[id, c] : s.pos.shared()->open)
        if (c.fireAt > 0)
            c.fireAt = QDateTime::currentMSecsSinceEpoch() - 1000;
    CHECK(s.pos.fireDueOrders() == 1);
    check = s.pos.checkInfo();
    CHECK(check[u"heldCount"_s].toInt() == 0);
    CHECK(check[u"firesAt"_s].toString().isEmpty());

    // Changed one's mind: back to firing by hand.
    s.pos.selectLine(0);
    s.pos.setCourse(3);
    s.pos.addItem(u"water"_s);
    CHECK_FALSE(s.pos.sendOrder());                              // held: "fire it when it's time"
    REQUIRE(s.pos.fireCourseIn(15));
    REQUIRE(s.pos.fireCourseIn(-1));
    CHECK(s.pos.checkInfo()[u"firesAt"_s].toString().isEmpty());
}

TEST_CASE("UI: kitchen tickets are late past what their items usually take", "[flow][ui][preptimes]")
{
    Screen s;
    s.pos.shared()->settings.prepSeconds["cobb"] = 600;          // usually 10 minutes
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"cobb"_s);
    s.pos.finishChoosing();
    REQUIRE(s.pos.sendOrder());
    QVariantMap ticket = s.pos.kitchenTickets().value(0).toMap();
    CHECK(ticket[u"targetMinutes"_s].toInt() == 10);
    CHECK(ticket[u"lateMinutes"_s].toInt() == 12);
    CHECK(ticket[u"warnMinutes"_s].toInt() == 7);

    // Twenty minutes ago: LATE on the kitchen screen.
    for (auto &[id, c] : s.pos.shared()->open)
        for (core::OrderLine &l : c.lines)
            l.sentAt -= 20 * 60'000;
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    QTest::qWait(1100);
    bool late = false;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *it) {
        if (it->isVisible() && it->property("text").toString().startsWith(u"LATE"_s))
            late = true;
        for (QQuickItem *c : it->childItems())
            walk(c);
    };
    walk(s.window->contentItem());
    CHECK(late);
    s.shot("75-kitchen-late");

    // Bumped: what it took (20 minutes) moves the usual time up.
    ticket = s.pos.kitchenTickets().value(0).toMap();
    REQUIRE(s.pos.bumpTicket(ticket[u"checkId"_s].toLongLong(), ticket[u"sentAt"_s].toLongLong(), {}));
    CHECK(s.pos.shared()->settings.prepSeconds["cobb"] == 720);   // 0.8 x 600 + 0.2 x 1200

    // A manager's own target wins.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const QVariantList menu = s.pos.adminRecords(u"menu"_s);
    for (int i = 0; i < menu.size(); ++i)
        if (menu[i].toMap()[u"id"_s] == u"cobb"_s) {
            QVariantMap r = menu[i].toMap();
            r[u"prepMinutes"_s] = 5;
            REQUIRE(s.pos.adminSave(u"menu"_s, i, r));
        }
    CHECK(s.pos.prepMinutesFor("cobb") == 5);
}

TEST_CASE("UI: opening and closing checklists, ticked by whoever does them", "[flow][ui][checklists]")
{
    Screen s;
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.c.jumpTo(u"checklists"_s));
    QTest::qWait(60);
    s.tapItem(find(u"task-opening-0"_s));
    QTest::qWait(40);
    QVariantList opening = s.pos.checklists()[u"opening"_s].toList();
    REQUIRE(opening.size() == 5);
    CHECK(opening[0].toMap()[u"done"_s].toBool());
    CHECK(opening[0].toMap()[u"by"_s] == u"Sam"_s);
    s.tapItem(find(u"task-opening-0"_s));                      // touched again: undone
    QTest::qWait(40);
    CHECK_FALSE(s.pos.checklists()[u"opening"_s].toList()[0].toMap()[u"done"_s].toBool());
    s.tapItem(find(u"task-opening-0"_s));
    s.tapItem(find(u"task-closing-0"_s));
    s.tapItem(find(u"task-closing-1"_s));
    QTest::qWait(40);
    CHECK(s.pos.checklists()[u"closingDone"_s].toInt() == 2);
    s.shot("74-checklists");

    // End of Day shows what's left; the report lists who did what.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    CHECK(s.pos.dayInfo()[u"closingLeft"_s].toInt() == 3);
    REQUIRE(s.c.jumpTo(u"end-of-day"_s));
    QTest::qWait(60);
    CHECK(find(u"eodChecklist"_s));
    const core::Report r = s.pos.buildReport(u"checklists"_s);
    int notDone = 0, bySam = 0;
    for (const core::ReportRow &row : r.rows)
        if (row.cells.size() == 3) {
            notDone += row.cells[1] == "NOT DONE";
            bySam += row.cells[1] == "Sam";
        }
    CHECK(notDone == 7);
    CHECK(bySam == 3);
}

TEST_CASE("UI: time off and shift swaps from the Time Clock, decided by a manager", "[flow][ui][requests]")
{
    Screen s;
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    // Sam works tomorrow 4-10 PM.
    const QDateTime tomorrow(QDate::currentDate().addDays(1), QTime(16, 0));
    core::Shift shift;
    shift.id = 501;
    shift.employeeId = "sam";
    shift.start = tomorrow.toMSecsSinceEpoch();
    shift.end = tomorrow.addSecs(6 * 3600).toMSecsSinceEpoch();
    s.pos.shared()->shifts.push_back(shift);
    REQUIRE(s.c.jumpTo(u"time-clock"_s));

    // Sam gives it away, and asks for a day off three days out.
    REQUIRE(s.pos.timeClockStart(u"1111"_s));
    QTest::qWait(60);
    s.tapItem(find(u"giveAway-501"_s));
    QTest::qWait(40);
    CHECK_FALSE(find(u"giveAway-501"_s));                       // now "up for grabs"
    s.tapItem(find(u"timeOff"_s));
    QTest::qWait(40);
    REQUIRE(find(u"offPicker"_s));
    s.tapItem(find(u"offDay-3"_s));
    s.shot("72-time-off");
    s.tapItem(find(u"askOff"_s));
    QTest::qWait(40);
    QVariantList mine = s.pos.timeClock()[u"requests"_s].toMap()[u"mine"_s].toList();
    REQUIRE(mine.size() == 2);
    CHECK_FALSE(s.pos.timeClockRequestOff(QDate::currentDate().addDays(3).toString(u"yyyy-MM-dd"_s), {}));   // again
    s.pos.timeClockDone();

    // Casey sees it up for grabs and takes it.
    REQUIRE(s.pos.timeClockStart(u"2222"_s));
    QTest::qWait(60);
    const QVariantList grabs = s.pos.timeClock()[u"requests"_s].toMap()[u"upForGrabs"_s].toList();
    REQUIRE(grabs.size() == 1);
    const qint64 swapId = grabs[0].toMap()[u"id"_s].toLongLong();
    s.shot("73-up-for-grabs");
    s.tapItem(find(u"take-"_s + QString::number(swapId)));
    QTest::qWait(40);
    CHECK(s.pos.timeClock()[u"requests"_s].toMap()[u"upForGrabs"_s].toList().isEmpty());
    s.pos.timeClockDone();

    // The manager: two waiting (on the dashboard too); the swap approved, the day off not.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    CHECK(s.pos.dashboard()[u"requestsWaiting"_s].toInt() == 2);
    QVariantList records = s.pos.adminRecords(u"requests"_s);
    REQUIRE(records.size() == 2);
    for (int i = 0; i < records.size(); ++i) {
        QVariantMap r = records[i].toMap();
        r[u"status"_s] = r[u"_title"_s].toString().contains(u"Give away"_s) ? u"approved"_s : u"denied"_s;
        REQUIRE(s.pos.adminSave(u"requests"_s, i, r));
        records = s.pos.adminRecords(u"requests"_s);   // re-sorted: waiting first
        i = -1;
        if (std::ranges::none_of(records, [](const QVariant &v) { return v.toMap()[u"status"_s] == u"pending"_s; }))
            break;
    }
    CHECK(s.pos.shared()->shifts.back().employeeId == "casey");
    CHECK(s.pos.dashboard()[u"requestsWaiting"_s].toInt() == 0);
    REQUIRE(s.c.jumpTo(u"admin-schedule"_s));
    s.c.activate(u"requests"_s);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"admin-requests"_s);
    s.pos.logout();

    // Sam sees how both went.
    REQUIRE(s.pos.timeClockStart(u"1111"_s));
    mine = s.pos.timeClock()[u"requests"_s].toMap()[u"mine"_s].toList();
    QStringList statuses;
    for (const QVariant &v : mine)
        statuses << v.toMap()[u"status"_s].toString();
    statuses.sort();
    CHECK(statuses == QStringList{u"approved"_s, u"denied"_s});
}

TEST_CASE("UI: overtime warnings on the Time Clock, at clock-in and on the dashboard", "[flow][ui][overtime]")
{
    Screen s;
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::int64_t hour = 3'600'000;
    // An 8-hour pay week that started yesterday; Sam already worked 6.5 h of it.
    core::PosSettings &st = s.pos.shared()->settings;
    st.overtimeWeeklyHours = 8;
    st.overtimeDailyHours = 0;
    st.weekStartsOn = QDate::currentDate().addDays(-1).dayOfWeek() % 7;
    s.pos.shared()->punches.push_back({931, "sam", now - 8 * hour, now - 3 * hour / 2, {}, "server", vt::Money::fromCents(1200)});
    QStringList notices;
    QObject::connect(&s.pos, &app::PosSession::notice, [&](const QString &t) { notices << t; });

    REQUIRE(s.pos.timeClockStart(u"1111"_s));
    QVariantMap ot = s.pos.timeClock()[u"overtime"_s].toMap();
    CHECK(ot[u"state"_s] == u"soon"_s);
    CHECK(ot[u"left"_s] == u"1.5"_s);
    REQUIRE(s.pos.timeClockAct(u"in"_s));
    CHECK(notices.join(u"|"_s).contains(u"reaches overtime in 1.5 h"_s));
    REQUIRE(s.c.jumpTo(u"time-clock"_s));
    QTest::qWait(60);
    REQUIRE(Screen::findBy(s.window->contentItem(), "objectName", u"clockOvertime"_s));
    s.shot("71-overtime-soon");
    s.pos.timeClockDone();

    // The dashboard flags it too.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    bool flagged = false;
    for (const QVariant &v : s.pos.dashboard()[u"labor"_s].toMap()[u"onClock"_s].toList())
        flagged = flagged || (v.toMap()[u"name"_s] == u"Sam"_s && v.toMap()[u"overtime"_s] == u"soon"_s);
    CHECK(flagged);

    // Past it: "over".
    st.overtimeWeeklyHours = 6;
    s.pos.logout();
    REQUIRE(s.pos.timeClockStart(u"1111"_s));
    CHECK(s.pos.timeClock()[u"overtime"_s].toMap()[u"state"_s] == u"over"_s);
}

TEST_CASE("UI: ring items in by number (keyboard, or Find)", "[flow][ui][plu]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    QTest::qWait(40);
    // 203 Enter: Cobb (salads start at 201).
    for (const char *k : {"2", "0", "3"})
        QTest::keyClick(s.window, *k);
    CHECK(s.c.statusText().contains(u"#203"_s));
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(60);
    REQUIRE(s.pos.lines().size() == 1);
    CHECK(s.pos.lines()[0].toMap()[u"name"_s] == u"Cobb"_s);
    s.c.finishChoosing();
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    for (const char *k : {"9", "9", "9"})
        QTest::keyClick(s.window, *k);
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(40);
    CHECK(s.c.statusText().contains(u"No item has number 999"_s));
    CHECK(s.pos.lines().size() == 1);

    // Find: "30" lists the drinks by number.
    s.c.activate(u"tab-find"_s);
    QTest::qWait(60);
    s.pos.textKey(u"3"_s);
    s.pos.textKey(u"0"_s);
    s.pos.textKey(u"5"_s);
    QTest::qWait(60);
    CHECK(Screen::findBy(s.window->contentItem(), "objectName", u"menuItem-water"_s));   // 305
    CHECK_FALSE(Screen::findBy(s.window->contentItem(), "objectName", u"menuItem-coffee"_s));

    // Numbers are each item's own.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const QVariantList menu = s.pos.adminRecords(u"menu"_s);
    for (int i = 0; i < menu.size(); ++i)
        if (menu[i].toMap()[u"id"_s] == u"caesar"_s) {
            QVariantMap r = menu[i].toMap();
            r[u"number"_s] = u"203"_s;
            CHECK_FALSE(s.pos.adminSave(u"menu"_s, i, r));
            r[u"number"_s] = u"12a"_s;
            CHECK_FALSE(s.pos.adminSave(u"menu"_s, i, r));
            r[u"number"_s] = u"250"_s;
            CHECK(s.pos.adminSave(u"menu"_s, i, r));
        }
}

TEST_CASE("UI: the manager's dashboard: today so far", "[flow][ui][dashboard]")
{
    Screen s;
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::int64_t hour = 3'600'000;
    // Sam on the clock 4 hours at $12; buns running low.
    s.pos.shared()->punches.push_back({921, "sam", now - 4 * hour, 0, {}, "server", vt::Money::fromCents(1200)});
    for (core::Ingredient &g : s.pos.shared()->ingredients)
        if (g.id == "bun")
            g.onHand = 4;
    // A cobb sold ($12.50), and a check still open.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(s.pos.dashboard().isEmpty());
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    s.pos.addItem(u"cobb"_s);
    s.pos.finishChoosing();
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.pos.tender(u"credit"_s));
    REQUIRE(s.pos.closeCheck());
    // Last week by this time: half as much.
    core::Check lastWeek = s.pos.shared()->closedToday.back();
    lastWeek.payments.clear();
    for (core::OrderLine &l : lastWeek.lines)
        l.unitPrice = vt::Money::fromCents(625);
    s.pos.shared()->history = [lastWeek](std::int64_t, std::int64_t) { return std::vector<core::Check>{lastWeek}; };
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"water"_s);
    s.pos.releaseCheck();

    const QVariantMap d = s.pos.dashboard();
    const QVariantMap sales = d[u"sales"_s].toMap();
    CHECK(sales[u"net"_s] == u"$12.50"_s);
    CHECK(sales[u"checks"_s].toInt() == 1);
    CHECK(sales[u"change"_s].toInt() == 100);                    // twice last week's
    const QVariantMap labor = d[u"labor"_s].toMap();
    CHECK(labor[u"cost"_s] == u"$48.00"_s);                      // 4 h x $12
    CHECK(labor[u"percent"_s].toInt() == 384);
    CHECK(labor[u"onClock"_s].toList().size() == 1);
    CHECK(d[u"open"_s].toMap()[u"count"_s].toInt() == 1);
    CHECK(d[u"top"_s].toList().value(0).toMap()[u"name"_s] == u"Cobb"_s);
    bool bun = false;
    for (const QVariant &v : d[u"low"_s].toList())
        bun = bun || v.toMap()[u"name"_s].toString().contains(u"un"_s);
    CHECK(bun);

    // Manager -> Dashboard; not for servers.
    REQUIRE(s.c.jumpTo(u"manager"_s));
    s.c.activate(u"dashboard"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"dashboard"_s);
    REQUIRE(Screen::findBy(s.window->contentItem(), "objectName", u"dashSales"_s));
    s.shot("70-dashboard");
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK(s.pos.dashboard().isEmpty());
}

TEST_CASE("UI: dishes running low show how many are left", "[flow][ui][stock]")
{
    Screen s;
    for (core::Ingredient &g : s.pos.shared()->ingredients)
        if (g.id == "bun")
            g.onHand = 4;                                       // low at 12
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QVariantMap left = s.pos.stockLeft();
    CHECK(left.value(u"classic-burger"_s).toInt() == 4);
    CHECK(left.value(u"cheeseburger"_s).toInt() == 4);
    CHECK_FALSE(left.contains(u"cobb"_s));                      // no buns in it
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(60);
    REQUIRE(Screen::findBy(s.window->contentItem(), "text", u"4 left"_s));

    // One sells: 3 left.
    s.pos.addItem(u"classic-burger"_s);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(60);
    CHECK(s.pos.stockLeft().value(u"classic-burger"_s).toInt() == 3);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"3 left"_s));
    s.shot("69-stock-left");
}

TEST_CASE("Time Punches reaches a month back; the dashboard refreshes", "[flow][punches][older]")
{
    Screen s;
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::int64_t day = 24 * 3'600'000;
    // In the store (not loaded at start): Sam, 20 days ago, and 40 days ago.
    std::vector<core::TimePunch> stored{
        {951, "sam", now - 20 * day, now - 20 * day + 8 * 3'600'000, {}, "server", vt::Money::fromCents(1200)},
        {952, "sam", now - 40 * day, now - 40 * day + 8 * 3'600'000, {}, "server", vt::Money::fromCents(1200)}};
    s.pos.shared()->punchHistory = [stored](std::int64_t from, std::int64_t to) {
        std::vector<core::TimePunch> out;
        for (const core::TimePunch &p : stored)
            if (p.clockIn >= from && p.clockIn < to)
                out.push_back(p);
        return out;
    };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantList records = s.pos.adminRecords(u"punches"_s);
    int i = -1;
    for (int r = 0; r < records.size(); ++r) {
        CHECK(records[r].toMap()[u"id"_s].toLongLong() != 952);   // over a month: not listed
        if (records[r].toMap()[u"id"_s].toLongLong() == 951)
            i = r;
    }
    REQUIRE(i >= 0);
    QVariantMap r = records[i].toMap();
    r[u"clockOut"_s] = QDateTime::fromMSecsSinceEpoch(now - 20 * day + 9 * 3'600'000).toString(u"yyyy-MM-dd HH:mm"_s);
    r[u"reason"_s] = u"stayed late"_s;
    REQUIRE(s.pos.adminSave(u"punches"_s, i, r));
    CHECK(s.pos.shared()->settings.punchChanges.back().why == "stayed late");

    QSignalSpy dayChanged(&s.pos, &app::PosSession::dayChanged);
    s.pos.refreshDay();
    CHECK(dayChanged.count() >= 1);
}

TEST_CASE("UI: End of Day lists who's still clocked in, and clocks them out", "[flow][ui][punches][endofday]")
{
    Screen s;
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::int64_t hour = 3'600'000;
    s.pos.shared()->punches.push_back({911, "sam", now - 14 * hour, 0, {}, "server", vt::Money::fromCents(1200)});   // forgot
    s.pos.shared()->punches.push_back({912, "casey", now - 2 * hour, 0, {}, "cashier", vt::Money::fromCents(1100)});
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"end-of-day"_s));
    QTest::qWait(80);
    const QVariantList in = s.pos.dayInfo()[u"clockedIn"_s].toList();
    REQUIRE(in.size() == 2);
    bool samLong = false, caseyLong = true;
    for (const QVariant &v : in) {
        if (v.toMap()[u"name"_s] == u"Sam"_s) samLong = v.toMap()[u"overTwelve"_s].toBool();
        if (v.toMap()[u"name"_s] == u"Casey"_s) caseyLong = v.toMap()[u"overTwelve"_s].toBool();
    }
    CHECK(samLong);
    CHECK_FALSE(caseyLong);
    QQuickItem *out = Screen::findBy(s.window->contentItem(), "objectName", u"eodClockOut-911"_s);
    REQUIRE(out);
    s.shot("68-end-of-day-clocked-in");
    s.tapItem(out);
    QTest::qWait(60);
    CHECK(s.pos.dayInfo()[u"clockedIn"_s].toList().size() == 1);
    REQUIRE(s.pos.shared()->settings.punchChanges.size() == 1);
    CHECK(s.pos.shared()->settings.punchChanges[0].employee == "Sam");
}

TEST_CASE("UI: the Time Clock: clock in and out, breaks and the schedule, by PIN alone", "[flow][ui][timeclock]")
{
    Screen s;
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto pin = [&](const QString &digits) {
        for (const QChar ch : digits) {
            s.tapItem(find(u"clockKey-"_s + ch));
            QTest::qWait(20);
        }
        s.tapItem(find(u"clockKey-OK"_s));
        QTest::qWait(60);
    };
    // Sam works today and the day after tomorrow; it's noon (whenever this runs).
    const QDateTime today(QDate::currentDate(), QTime(0, 0));
    s.pos.setClock([today] { return today.addSecs(12 * 3600).toMSecsSinceEpoch(); });
    for (const int day : {0, 2}) {
        core::Shift shift;
        shift.id = 100 + day;
        shift.employeeId = "sam";
        shift.start = today.addDays(day).addSecs(6 * 3600).toMSecsSinceEpoch();
        shift.end = today.addDays(day).addSecs(23 * 3600 + 59 * 60).toMSecsSinceEpoch();
        shift.note = day == 2 ? "patio" : "";
        s.pos.shared()->shifts.push_back(shift);
    }

    // From the login page.
    s.c.activate(u"time-clock"_s);
    QTest::qWait(60);
    REQUIRE(s.c.pageId() == u"time-clock"_s);
    pin(u"9999"_s);
    CHECK(s.pos.timeClock().isEmpty());                         // not a PIN
    pin(u"1111"_s);
    QVariantMap info = s.pos.timeClock();
    REQUIRE(info[u"name"_s] == u"Sam"_s);
    CHECK_FALSE(s.pos.loggedIn());                              // never logged in to the register
    CHECK(info[u"status"_s] == u"out"_s);
    const QVariantList shifts = info[u"shifts"_s].toList();
    REQUIRE(shifts.size() == 2);
    CHECK(shifts[0].toMap()[u"day"_s] == u"Today"_s);
    CHECK(shifts[0].toMap()[u"now"_s].toBool());
    CHECK(shifts[1].toMap()[u"note"_s] == u"patio"_s);
    REQUIRE(find(u"clockIn"_s));

    s.tapItem(find(u"clockIn"_s));
    QTest::qWait(60);
    CHECK(s.pos.timeClock()[u"status"_s] == u"in"_s);
    s.shot("66-time-clock");
    s.tapItem(find(u"clockBreak"_s));
    QTest::qWait(40);
    CHECK(s.pos.timeClock()[u"status"_s] == u"break"_s);
    s.tapItem(find(u"clockBreak"_s));
    QTest::qWait(40);
    CHECK(s.pos.timeClock()[u"status"_s] == u"in"_s);
    s.tapItem(find(u"clockOut"_s));
    QTest::qWait(40);
    CHECK(s.pos.timeClock()[u"status"_s] == u"out"_s);
    CHECK(s.pos.timeClock()[u"punches"_s].toList().size() == 1);
    s.tapItem(find(u"clockDone"_s));
    QTest::qWait(40);
    CHECK(s.pos.timeClock().isEmpty());
    REQUIRE(find(u"clockKey-OK"_s));
    CHECK_FALSE(s.pos.loggedIn());

    // A terminal set to Time Clock rests there: after a logout too.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantMap t = s.pos.adminNewRecord(u"terminals"_s);
    t[u"name"_s] = s.pos.terminalName();
    t[u"screen"_s] = u"timeClock"_s;
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, t));
    s.pos.logout();
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"time-clock"_s);
    // "Log In to the Register…" is still there for a manager.
    s.c.activate(u"register"_s);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"login"_s);
}

TEST_CASE("UI: holding a button explains it instead of pressing it", "[flow][ui][explain]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *send = Screen::findBy(s.window->contentItem(), "text", u"Send"_s);
    REQUIRE(send);
    // Held: nothing sent, the card says what it does.
    const QPoint at = send->mapToScene(QPointF(send->width() / 2, send->height() / 2)).toPoint();
    QTest::mousePress(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(1000);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(60);
    REQUIRE(find(u"explainCard"_s));
    CHECK(s.c.explanation().value(u"text"_s).toString().contains(u"kitchen"_s));
    s.shot("63-explain");
    CHECK(s.pos.lines().isEmpty());

    // An item button names the item and its price; a page button the page.
    s.c.explain(u"item-1"_s);
    CHECK(s.c.explanation().value(u"text"_s).toString().startsWith(u"Adds Classic Burger ($"_s));
    s.c.explain(u"tab-find"_s);
    CHECK(s.c.explanation().value(u"text"_s).toString() != QString());

    // A touch closes it.
    s.tapItem(find(u"explainCard"_s));
    QTest::qWait(40);
    CHECK_FALSE(find(u"explainCard"_s));
}

TEST_CASE("UI: the Popular page fills itself with today's best sellers", "[flow][ui][popular]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK(s.pos.popularItems().isEmpty());
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"water"_s);
    s.pos.addItem(u"cobb"_s);
    s.pos.finishChoosing();
    REQUIRE(s.pos.setLineQuantity(0, 3));      // 3 Cobbs: first
    s.pos.addItem(u"water"_s);                // 2 waters
    const QStringList top = s.pos.popularItems();
    REQUIRE(top.size() == 2);
    CHECK(top[0] == u"cobb"_s);
    CHECK(top[1] == u"water"_s);

    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    s.c.activate(u"cat-menu-popular"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"menu-popular"_s);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *cobb = find(u"menuItem-cobb"_s);
    QQuickItem *water = find(u"menuItem-water"_s);
    REQUIRE(cobb);
    REQUIRE(water);
    CHECK(cobb->mapToScene({0, 0}).x() < water->mapToScene({0, 0}).x());   // most sold first
    CHECK_FALSE(find(u"menuItem-caesar"_s));
    s.shot("62-popular");
}

TEST_CASE("UI: Another Round orders the drinks sent last again", "[flow][ui][round]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T3"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-drinks"_s));
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto names = [&] {
        QStringList out;
        for (const QVariant &v : s.pos.lines())
            if (!v.toMap()[u"sent"_s].toBool())
                out << v.toMap()[u"name"_s].toString();
        return out;
    };
    CHECK_FALSE(s.pos.anotherRound());                 // nothing sent yet
    s.pos.addItem(u"soda"_s);
    REQUIRE(s.pos.chooseOption(u"drink-size"_s, 1));
    REQUIRE(s.pos.finishChoosing());
    s.pos.addItem(u"draft-beer"_s);
    REQUIRE(s.pos.chooseOption(u"draft"_s, 0));
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.pos.setLineQuantity(0, 2));              // two beers
    s.pos.addItem(u"bacon-burger"_s);                  // not a drink
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.c.jumpTo(u"items-drinks"_s));
    QTest::qWait(60);

    QQuickItem *round = find(u"anotherRound"_s);
    REQUIRE(round);
    s.shot("61-another-round");
    s.tapItem(round);
    QTest::qWait(60);
    CHECK(names().size() == 2);                        // the soda and the beers, not the burger
    const QVariantList lines = s.pos.lines();
    CHECK(lines.last().toMap()[u"quantity"_s].toInt() == 2);
    CHECK_FALSE(lines.last().toMap()[u"modifiers"_s].toList().isEmpty());   // same pour

    // The next round is the drinks of the latest Send only.
    REQUIRE(s.pos.sendOrder());
    s.pos.addItem(u"water"_s);
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.pos.anotherRound());
    CHECK(names() == QStringList{u"Water"_s});
}

TEST_CASE("UI: Undo puts back the item just removed", "[flow][ui][undo]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.pos.addItem(u"water"_s);
    s.pos.addItem(u"bacon-burger"_s);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.pos.addItem(u"water"_s);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    CHECK_FALSE(find(u"undoBar"_s));

    // Take the burger off: the bar offers it back, in its place, choices and all.
    const QVariantMap burger = s.pos.lines().value(1).toMap();
    s.pos.selectLine(burger[u"id"_s].toLongLong());
    REQUIRE(s.pos.voidItem());
    QTest::qWait(60);
    REQUIRE(s.pos.lines().size() == 2);
    REQUIRE(find(u"undoBar"_s));
    CHECK(s.pos.undoText() == u"Removed Bacon Burger"_s);
    s.shot("60-undo");
    s.tapItem(find(u"undoLast"_s));
    QTest::qWait(60);
    REQUIRE(s.pos.lines().size() == 3);
    CHECK(s.pos.lines().value(1).toMap()[u"name"_s] == burger[u"name"_s]);
    CHECK(s.pos.lines().value(1).toMap()[u"modifiers"_s].toList().size() == burger[u"modifiers"_s].toList().size());
    CHECK_FALSE(find(u"undoBar"_s));
    CHECK_FALSE(s.pos.undoLast());                 // once

    // Fewer: Undo puts the number back.
    s.pos.selectLine(burger[u"id"_s].toLongLong());
    REQUIRE(s.pos.setLineQuantity(0, 3));
    REQUIRE(s.pos.changeLineQuantity(0, -1));
    CHECK(s.pos.undoText().contains(u"2 instead of 3"_s));
    REQUIRE(s.pos.undoLast());
    CHECK(s.pos.lines().value(1).toMap()[u"quantity"_s].toInt() == 3);

    // Only on the check it happened on.
    REQUIRE(s.pos.voidItem());
    s.pos.releaseCheck();
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    CHECK(s.pos.undoText().isEmpty());
}

TEST_CASE("UI: − 2 + and Again on a touched line", "[flow][ui][quantity]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.pos.addItem(u"bacon-burger"_s);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));   // Medium Rare
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));          // Onion Rings (+1.00)
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto line = [&](int i) { return s.pos.lines().value(i).toMap(); };
    const qint64 one = std::llround(line(0)[u"price"_s].toString().remove(u'$').toDouble() * 100);

    // The new line is the touched one: + + makes it 3.
    QQuickItem *more = find(u"lineMore"_s);
    REQUIRE(more);
    s.tapItem(more);
    QTest::qWait(40);
    s.tapItem(find(u"lineMore"_s));
    QTest::qWait(40);
    REQUIRE(s.pos.lines().size() == 1);
    CHECK(line(0)[u"quantity"_s].toInt() == 3);
    CHECK(std::llround(line(0)[u"price"_s].toString().remove(u'$').toDouble() * 100) == one * 3);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"3 × Bacon Burger"_s));
    s.shot("57-quantity");
    s.tapItem(find(u"lineLess"_s));
    QTest::qWait(40);
    CHECK(line(0)[u"quantity"_s].toInt() == 2);

    // Again: one more the same way, choices and all, as its own line.
    s.tapItem(find(u"lineAgain"_s));
    QTest::qWait(40);
    REQUIRE(s.pos.lines().size() == 2);
    CHECK(line(1)[u"quantity"_s].toInt() == 1);
    CHECK(line(1)[u"modifiers"_s].toList().size() == line(0)[u"modifiers"_s].toList().size());
    CHECK(line(1)[u"selected"_s].toBool());

    // − on the last one takes it off the check.
    s.tapItem(find(u"lineLess"_s));
    QTest::qWait(40);
    CHECK(s.pos.lines().size() == 1);

    // Sent: no − / +, only Again (a new line, sent with the next Send).
    REQUIRE(s.pos.sendOrder());
    s.pos.selectLine(line(0)[u"id"_s].toLongLong());
    QTest::qWait(40);
    CHECK_FALSE(find(u"lineMore"_s));
    CHECK_FALSE(s.pos.setLineQuantity(line(0)[u"id"_s].toLongLong(), 5));
    s.tapItem(find(u"lineAgain"_s));
    QTest::qWait(40);
    REQUIRE(s.pos.lines().size() == 2);
    CHECK_FALSE(line(1)[u"sent"_s].toBool());
    CHECK(line(0)[u"quantity"_s].toInt() == 2);
}

TEST_CASE("UI: a table split by seat, a line moved, and back together", "[flow][ui][tablechecks]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T6"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    for (int seat = 1; seat <= 3; ++seat) {
        s.pos.setSeat(seat);
        s.pos.addItem(u"water"_s);
    }
    s.pos.addItem(u"water"_s);   // seat 3 too
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto tapName = [&](const QString &name) {
        QQuickItem *it = find(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(50);
    };

    tapName(u"tableCheck-more"_s);
    s.shot("58-table-tools");
    tapName(u"splitBySeat"_s);
    REQUIRE(s.pos.tableChecks().size() == 3);
    CHECK(s.pos.lines().size() == 1);          // seat 1 stayed here
    CHECK_FALSE(find(u"orderSheet"_s));        // closed

    // Seat 1's rings go to check 3 instead (touch, Move…, Check 3).
    s.pos.selectLine(s.pos.lines().value(0).toMap().value(u"id"_s).toLongLong());
    QTest::qWait(40);
    tapName(u"lineMove"_s);
    s.shot("59-move-line");
    tapName(u"moveTo-3"_s);
    CHECK(s.pos.lines().isEmpty());
    tapName(u"tableCheck-3"_s);
    CHECK(s.pos.lines().size() == 3);          // seat 3's two and the moved one

    // Back to one check.
    tapName(u"tableCheck-more"_s);
    tapName(u"combineTableChecks"_s);
    CHECK(s.pos.tableChecks().size() == 1);
    CHECK(s.pos.lines().size() == 4);

    // Nothing has a seat: split by seat says so.
    REQUIRE(s.pos.selectTable(u"T7"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"water"_s);
    CHECK_FALSE(s.pos.splitBySeat());
}

TEST_CASE("UI: split check one at a time, a shared item, evenly", "[flow][ui][split]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T6"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"cobb"_s);
    const qint64 cobb = s.pos.lines()[0].toMap()[u"id"_s].toLongLong();
    REQUIRE(s.pos.setLineQuantity(cobb, 2));
    s.pos.addItem(u"soda"_s);
    REQUIRE(s.c.jumpTo(u"split"_s));
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto tapName = [&](const QString &name) {
        QQuickItem *it = find(name);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(50);
    };
    const auto tapText = [&](const QString &text) {
        QQuickItem *it = Screen::findBy(s.window->contentItem(), "text", text);
        REQUIRE(it);
        s.tapItem(it);
        QTest::qWait(50);
    };

    s.pos.selectLine(cobb);
    QTest::qWait(40);
    QQuickItem *howMany = find(u"splitHowMany"_s);
    REQUIRE(howMany);
    CHECK(howMany->isVisible());
    s.shot("60-split-one-or-all");
    tapText(u"+ New check"_s);                    // one of the two
    CHECK(s.pos.lines()[0].toMap()[u"quantity"_s].toInt() == 1);
    CHECK_FALSE(howMany->isVisible());

    // The soda, shared 2 ways.
    s.pos.selectLine(s.pos.lines()[1].toMap()[u"id"_s].toLongLong());
    QTest::qWait(40);
    tapName(u"splitShare"_s);
    tapName(u"splitWays-2"_s);
    CHECK(s.pos.lines().size() == 3);
    CHECK(s.pos.lines()[2].toMap()[u"shared"_s].toBool());
    s.shot("61-split-shared");
    tapName(u"splitPutBack"_s);
    CHECK(s.pos.lines().size() == 2);

    // Evenly, 2 ways.
    tapName(u"splitEvenly"_s);
    tapName(u"splitWays-2"_s);
    CHECK(s.pos.tableChecks().size() == 3);
}

TEST_CASE("UI: separate checks at one table, switched on the order screen", "[flow][ui][tablechecks]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T5"_s) == app::PosSession::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.pos.addItem(u"water"_s);
    QTest::qWait(60);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(s.pos.tableChecks().size() == 1);
    QQuickItem *plus = find(u"tableCheck-new"_s);
    REQUIRE(plus);
    const qint64 first = s.pos.checkInfo().value(u"id"_s).toLongLong();

    // Guest 2 on their own check, without leaving the page.
    s.tapItem(plus);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"items-burgers"_s);
    REQUIRE(s.pos.tableChecks().size() == 2);
    CHECK(s.pos.checkInfo().value(u"label"_s).toString() == u"T5"_s);
    CHECK(s.pos.checkInfo().value(u"id"_s).toLongLong() != first);
    CHECK(s.pos.lines().isEmpty());
    s.pos.addItem(u"water"_s);
    s.pos.addItem(u"water"_s);
    s.tapItem(find(u"tableCheck-new"_s));
    QTest::qWait(60);
    REQUIRE(s.pos.tableChecks().size() == 3);
    for (int i = 0; i < 3; ++i) {   // a party of six
        s.tapItem(find(u"tableCheck-new"_s));
        QTest::qWait(40);
    }
    REQUIRE(s.pos.tableChecks().size() == 6);
    s.shot("55-table-checks");

    // Twenty: still one line; the open one shown, the others a page away.
    for (int i = 0; i < 14; ++i) {
        s.tapItem(find(u"tableCheck-new"_s));
        QTest::qWait(30);
    }
    REQUIRE(s.pos.tableChecks().size() == 20);
    QTest::qWait(60);
    CHECK(find(u"tableCheck-20"_s));            // the new one, open
    CHECK_FALSE(find(u"tableCheck-1"_s));       // a page back
    QQuickItem *prev = find(u"tableCheck-prev"_s);
    REQUIRE(prev);
    const QQuickItem *plus20 = find(u"tableCheck-new"_s);
    CHECK(plus20->height() == find(u"tableCheck-20"_s)->height());
    s.shot("56-twenty-checks");
    for (int i = 0; i < 5 && !find(u"tableCheck-1"_s); ++i) {
        s.tapItem(prev);
        QTest::qWait(40);
    }
    REQUIRE(find(u"tableCheck-1"_s));

    // Back to check 1: its own line.
    QQuickItem *one = find(u"tableCheck-1"_s);
    REQUIRE(one);
    s.tapItem(one);
    QTest::qWait(60);
    CHECK(s.pos.checkInfo().value(u"id"_s).toLongLong() == first);
    CHECK(s.pos.lines().size() == 1);
    CHECK(s.c.pageId() == u"items-burgers"_s);

    // The table now asks which check.
    s.pos.releaseCheck();
    CHECK(s.pos.selectTable(u"T5"_s) == app::PosSession::TableChooseCheck);
}

TEST_CASE("UI: find an item by typing part of its name", "[flow][ui][find]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    s.pos.textKey(u"x"_s);                                   // left over from somewhere
    s.c.activate(u"tab-find"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"find-item"_s);
    CHECK(s.pos.textEntry().isEmpty());                       // Find starts clean
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    for (const QChar ch : u"cob"_s)
        s.pos.textKey(QString(ch));
    QTest::qWait(60);
    QQuickItem *cobb = find(u"menuItem-cobb"_s);
    REQUIRE(cobb);
    CHECK_FALSE(find(u"menuItem-caesar"_s));                  // only what matches
    s.shot("51-find-item");
    s.tapItem(cobb);
    QTest::qWait(60);
    CHECK(s.pos.lines().size() == 1);
    CHECK(s.pos.textEntry().isEmpty());                       // ready for the next one
    s.c.finishChoosing();                                     // Cobb's protein (optional)
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"find-item"_s);

    // A word inside the name counts; nothing found says so.
    for (const QChar ch : u"bur"_s)
        s.pos.textKey(QString(ch));
    QTest::qWait(60);
    CHECK(find(u"menuItem-classic-burger"_s));                // "Classic Burger": a word starts with it
    s.pos.textKey(u"clear"_s);
    for (const QChar ch : u"zzz"_s)
        s.pos.textKey(QString(ch));
    QTest::qWait(60);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Nothing on the menu has \"zzz\"."_s));
}

TEST_CASE("UI: Preview shows a page on a phone, a tablet, a terminal and a kiosk", "[flow][ui][preview]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    QVariantMap info = e->previewInfo();
    CHECK(info[u"canvasW"_s].toInt() == 1920);
    CHECK(info[u"smallest"_s].toInt() > 0);
    CHECK(info[u"phone"_s].toString().contains(u"phone"_s, Qt::CaseInsensitive));   // Tables has a phone version
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(50);
    QObject *preview = s.window->findChild<QObject *>(u"pagePreview"_s);
    REQUIRE(preview);
    QMetaObject::invokeMethod(preview, "open");
    QTest::qWait(300);
    auto *content = preview->property("contentItem").value<QQuickItem *>();
    REQUIRE(content);
    for (const char *id : {"phone", "tablet", "terminal", "kiosk"})
        CHECK(Screen::findBy(content, "objectName", u"preview-"_s + QLatin1StringView(id)));
    QQuickItem *phone = Screen::findBy(content, "objectName", u"previewSize-phone"_s);
    QQuickItem *terminal = Screen::findBy(content, "objectName", u"previewSize-terminal"_s);
    REQUIRE(phone);
    REQUIRE(terminal);
    CHECK(phone->property("text").toString().contains(u"mm"_s));
    // The same buttons are smaller on a phone than on a terminal.
    const auto mm = [](QQuickItem *label) {
        static const QRegularExpression n(u"([0-9]+\\.[0-9])"_s);
        return n.match(label->property("text").toString()).captured(1).toDouble();
    };
    CHECK(mm(phone) < mm(terminal));
    s.shot("79-preview");
    QMetaObject::invokeMethod(preview, "close");
    s.c.leaveEditMode(false);
}

TEST_CASE("UI: ready-made layouts for each screen, and page files", "[flow][ui][layouts]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const auto rectOf = [&](const QString &page, const QString &zone) {
        const vt::layout::Page *p = s.c.editor() ? s.c.editor()->editor().layout().page(page)
                                                 : s.c.activeLayout().page(page);
        return p && p->zone(zone) ? p->zone(zone)->rect : QRect();
    };
    QTemporaryDir dir;

    // Every main screen has five.
    for (const QString &page : {u"login"_s, u"tables"_s, u"settle"_s, u"kitchen"_s, u"index-lunch"_s}) {
        REQUIRE(s.c.jumpTo(page));
        s.c.enterEditMode();
        CHECK(s.c.editor()->arrangements().size() == 5);
        REQUIRE(s.c.leaveEditMode(false));
    }

    // The gallery on the Tables page: Buttons left.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const QRect quick = rectOf(u"tables"_s, u"quick"_s);
    CHECK(Screen::findBy(s.window->contentItem(), "objectName", u"layoutsButton"_s));   // Layouts… in the toolbar
    // The earlier edit sessions' toolbars are gone first (deleted later).
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(50);
    QObject *gallery = s.window->findChild<QObject *>(u"layoutGallery"_s);
    REQUIRE(gallery);
    QMetaObject::invokeMethod(gallery, "open");                                       // what it does
    QTest::qWait(250);
    CHECK(gallery->property("visible").toBool());
    auto *galleryItem = gallery->property("contentItem").value<QQuickItem *>();
    REQUIRE(galleryItem);
    QQuickItem *left = Screen::findBy(galleryItem, "objectName", u"layout-buttons-left"_s);
    REQUIRE(left);
    s.shot("47-layout-gallery");
    s.tapItem(left);
    QTest::qWait(80);
    CHECK(rectOf(u"tables"_s, u"quick"_s).x() == 16);
    e->undo();
    CHECK(rectOf(u"tables"_s, u"quick"_s) == quick);
    REQUIRE(e->useArrangement(u"buttons-left"_s));

    // A menu page: the order screen around it (its template).
    REQUIRE(e->exportPage(QUrl::fromLocalFile(dir.filePath(u"tables.vtpage.json"_s))));
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(80);
    s.shot("48-tables-buttons-left");
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    s.c.enterEditMode();
    e = s.c.editor();
    REQUIRE(e->useArrangement(u"big-send-pay"_s));
    CHECK(rectOf(u"order-template"_s, u"flow-pay"_s).width() == 334);
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(80);
    s.shot("49-order-big-send-pay");

    // Another store's page used for this one, then the whole restaurant from a file.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    e = s.c.editor();
    REQUIRE(e->useArrangement(u"classic"_s));
    CHECK(rectOf(u"tables"_s, u"quick"_s) == quick);
    REQUIRE(e->importPageHere(QUrl::fromLocalFile(dir.filePath(u"tables.vtpage.json"_s))));
    CHECK(rectOf(u"tables"_s, u"quick"_s).x() == 16);              // the file's arrangement
    CHECK(e->editor().layout().page(u"tables"_s)->role == u"tables"_s);   // still the Tables page
    REQUIRE(e->exportLayout(QUrl::fromLocalFile(dir.filePath(u"store.vtlayout.json"_s))));
    REQUIRE(e->useArrangement(u"counter"_s));
    REQUIRE(e->importLayout(QUrl::fromLocalFile(dir.filePath(u"store.vtlayout.json"_s))));
    CHECK(rectOf(u"tables"_s, u"quick"_s).x() == 16);
    REQUIRE(s.c.leaveEditMode(false));

    // Every layout of every screen, for a look (only when taking screenshots).
    if (qEnvironmentVariableIsEmpty("VTM_SHOTS"))
        return;
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"caesar"_s);
    s.pos.finishChoosing();
    REQUIRE(s.pos.sendOrder());
    for (const QString &page : {u"login"_s, u"tables"_s, u"index-lunch"_s, u"settle"_s, u"kitchen"_s}) {
        REQUIRE(s.c.jumpTo(page));
        s.c.enterEditMode();
        QStringList ids;
        for (const QVariant &v : s.c.editor()->arrangements())
            ids << v.toMap()[u"id"_s].toString();
        REQUIRE(s.c.leaveEditMode(false));
        for (const QString &id : ids) {
            REQUIRE(s.c.jumpTo(page));
            s.c.enterEditMode();
            REQUIRE(s.c.editor()->useArrangement(id));
            REQUIRE(s.c.leaveEditMode(true));
            REQUIRE(s.c.jumpTo(page));
            QTest::qWait(120);
            s.shot(qPrintable(u"50-%1-%2"_s.arg(page, id)));
        }
    }
}

TEST_CASE("UI: the setup guide opens for the first manager and walks through the store", "[flow][ui][setup]")
{
    Screen s;
    s.pos.shared()->settings.setupDone = false;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                      // a server: no guide
    QTest::qWait(60);
    CHECK_FALSE(s.c.setupOpen());
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QTest::qWait(120);
    REQUIRE(s.c.setupOpen());
    const auto find = [&](const char *name) {
        return Screen::findBy(s.window->contentItem(), "objectName", QString::fromLatin1(name));
    };
    QQuickItem *guide = find("setupGuide");
    REQUIRE(guide);
    s.shot("46-setup-welcome");
    const auto next = [&] {
        s.tapItem(find("setupNext"));
        QTest::qWait(80);
    };
    next();                                                      // your store
    guide->setProperty("storeName", u"Taco Loco"_s);
    guide->setProperty("receiptLines", u"123 Main St · 555-0100"_s);
    s.shot("46-setup-store");
    next();                                                      // logo
    CHECK(s.pos.shared()->settings.storeName == "Taco Loco");
    QTemporaryDir dir;
    QImage logo(240, 240, QImage::Format_ARGB32);
    logo.fill(Qt::transparent);
    {
        QPainter p(&logo);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(u"#e4572e"_s));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(10, 10, 220, 220));
        p.setPen(Qt::white);
        p.setFont(QFont(u"DejaVu Sans"_s, 40, QFont::Bold));
        p.drawText(QRectF(10, 10, 220, 220), Qt::AlignCenter, u"TL"_s);
    }
    REQUIRE(logo.save(dir.filePath(u"taco.png"_s)));
    guide->setProperty("logoRef", s.pos.addImageFile(dir.filePath(u"taco.png"_s)));
    guide->setProperty("receiptLogo", true);
    QTest::qWait(80);
    s.shot("46-setup-logo");
    next();                                                      // look
    CHECK(s.pos.shared()->settings.displayLogo == "store:taco.png");
    QTest::qWait(80);
    QQuickItem *fromLogo = find("setupLook-logo-dark");
    REQUIRE(fromLogo);
    s.tapItem(fromLogo);
    QTest::qWait(80);
    CHECK(s.c.activeLayout().theme.name == u"From the logo (dark)"_s);
    s.shot("46-setup-look");
    next();                                                      // taxes
    guide->setProperty("foodTax", u"8.25"_s);
    next();                                                      // menu
    CHECK(s.pos.shared()->settings.tax.foodPpm == 82500);
    s.pos.setupAddItem(u"Fish Tacos"_s, 12.5, u"tacos"_s);
    QTest::qWait(60);
    s.shot("46-setup-menu");
    next();                                                      // staff
    s.pos.setupAddEmployee(u"Ana Ruiz"_s, u"manager"_s, u"8642"_s);
    QTest::qWait(80);
    REQUIRE(find("setupRetire"));
    s.shot("46-setup-staff");
    s.tapItem(find("setupRetire"));
    QTest::qWait(80);
    CHECK(s.pos.setupInfo()[u"samples"_s].toInt() == 0);
    next();                                                      // done
    s.shot("46-setup-done");
    next();                                                      // Finish
    CHECK_FALSE(s.c.setupOpen());
    CHECK(s.pos.shared()->settings.setupDone);
    CHECK_FALSE(s.pos.loggedIn());                               // Morgan (a sample) is off now
    CHECK(s.pos.loginWithPin(u"8642"_s));                        // Ana, the owner
}

TEST_CASE("UI: ready-made looks recolor every screen; one comes from the logo", "[flow][ui][looks]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                 // a server can't
    CHECK_FALSE(s.c.applyLook(u"light"_s));
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QStringList ids;
    for (const QVariant &v : s.c.looks())
        ids << v.toMap()[u"id"_s].toString();
    CHECK(ids.contains(u"light"_s));
    CHECK_FALSE(ids.contains(u"logo-dark"_s));             // no logo yet

    const auto tour = [&](const QString &look) {
        REQUIRE(s.c.applyLook(look));
        REQUIRE(s.c.jumpTo(u"tables"_s));
        QTest::qWait(80);
        s.shot(qPrintable(u"44-look-%1-tables"_s.arg(look)));
        if (!s.pos.hasCheck()) {
            REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
            s.pos.addItem(u"caesar"_s);
            s.pos.finishChoosing();
            s.pos.addItem(u"soda"_s);
            s.pos.chooseOption(u"drink-size"_s, 1);
            s.pos.finishChoosing();
        }
        REQUIRE(s.c.jumpTo(u"index-lunch"_s));
        QTest::qWait(80);
        s.shot(qPrintable(u"44-look-%1-order"_s.arg(look)));
        REQUIRE(s.c.jumpTo(u"settle"_s));
        QTest::qWait(80);
        s.shot(qPrintable(u"44-look-%1-settle"_s.arg(look)));
    };
    tour(u"light"_s);
    CHECK(s.c.activeLayout().theme.background.value(u"fill"_s).toString() == u"#e9edf2"_s);
    tour(u"contrast"_s);
    tour(u"cafe"_s);

    // With a logo: looks in its colors.
    QImage logo(200, 200, QImage::Format_ARGB32);
    logo.fill(Qt::transparent);
    for (int y = 40; y < 160; ++y)
        for (int x = 40; x < 160; ++x)
            logo.setPixel(x, y, x < 100 ? qRgb(0x0b, 0x6e, 0x4f) : qRgb(0xf2, 0xa5, 0x41));
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    logo.save(&buffer, "PNG");
    REQUIRE(s.pos.addStoreImage(u"logo.png"_s, QString::fromLatin1(png.toBase64())));
    s.pos.shared()->settings.displayLogo = "store:logo.png";
    ids.clear();
    for (const QVariant &v : s.c.looks())
        ids << v.toMap()[u"id"_s].toString();
    REQUIRE(ids.contains(u"logo-dark"_s));
    tour(u"logo-dark"_s);
    tour(u"logo-light"_s);

    // In the editor: Theme tab -> Looks; one undo step.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Theme"_s));
    QTest::qWait(120);
    QQuickItem *ocean = Screen::findBy(s.window->contentItem(), "objectName", u"look-ocean"_s);
    REQUIRE(ocean);
    s.shot("45-looks-gallery");
    s.tapItem(ocean);
    QTest::qWait(60);
    CHECK(e->editor().layout().theme.name == u"Ocean"_s);
    e->undo();
    CHECK(e->editor().layout().theme.name == u"From the logo (light)"_s);
    REQUIRE(s.c.leaveEditMode(false));
}

TEST_CASE("UI: the store's own font: added once, in every font list, on buttons", "[flow][ui][fonts]")
{
    Screen s;
    const QString garamond = QStringLiteral(VTM_SEED_DIR) + u"/../../fonts/ebgaramond/EBGaramond-Regular.ttf"_s;
    REQUIRE(QFile::exists(garamond));
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(s.c.fontFamilies().contains(u"EB Garamond"_s));   // not on this computer
    CHECK(s.pos.addImageFile(garamond) == u"store:ebgaramond-regular.ttf"_s);
    QTest::qWait(50);
    CHECK(s.c.fontFamilies().contains(u"EB Garamond"_s));          // installed for this screen
    bool listed = false;
    for (const QVariant &v : s.pos.storeImages())
        listed = listed || v.toMap()[u"kind"_s] == u"font"_s;
    CHECK(listed);

    // On a button, from the editor's font list.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    e->selectOnly({u"quick"_s});
    REQUIRE(e->setField(u"zone"_s, u"style.normal.font"_s, u"EB Garamond"_s));
    QTest::qWait(150);
    QQuickItem *choice = Screen::findBy(s.window->contentItem(), "objectName", u"fontChoice"_s);
    REQUIRE(choice);
    CHECK(choice->property("currentText").toString() == u"EB Garamond"_s);
    CHECK(Screen::findBy(s.window->contentItem(), "objectName", u"fontAdd"_s));
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(80);
    QQuickItem *quick = Screen::findBy(s.window->contentItem(), "zoneId", u"quick"_s);
    REQUIRE(quick);
    CHECK(quick->property("st").toMap()[u"font"_s] == u"EB Garamond"_s);
    s.shot("43-store-font");
}

TEST_CASE("UI: the menu laid out by itself: families, items, new items with no editing", "[flow][ui][menugrid]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    s.c.activate(u"cat-menu-all"_s);
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"menu-all"_s);
    const auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(find(u"menuFamily-salads"_s));
    s.tapItem(find(u"menuFamily-salads"_s));
    QTest::qWait(60);
    REQUIRE(find(u"menuItem-cobb"_s));
    s.tapItem(find(u"menuItem-cobb"_s));
    QTest::qWait(60);
    CHECK(s.pos.lines().size() == 1);
    CHECK(s.c.pageId() == u"modifiers"_s);             // its choices, as its own button would
    s.c.finishChoosing();                               // Done: back to the menu
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"menu-all"_s);

    // A new salad from Manager -> Menu: its button is there, no page editing.
    QVariantMap item = s.pos.adminNewRecord(u"menu"_s);
    item[u"name"_s] = u"Soup of the Day"_s;
    item[u"price"_s] = 6.5;
    item[u"family"_s] = u"salads"_s;
    REQUIRE(s.pos.adminSave(u"menu"_s, -1, item));
    QTest::qWait(80);
    QQuickItem *soup = find(u"menuItem-soup-of-the-day"_s);
    REQUIRE(soup);
    s.shot("42-menu-grid");
    s.tapItem(soup);
    QTest::qWait(60);
    CHECK(s.pos.lines().size() == 2);

    // A burger asks how it's cooked, as its own button would.
    s.tapItem(find(u"menuFamily-burgers"_s));
    QTest::qWait(60);
    s.tapItem(find(u"menuItem-classic-burger"_s));
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"modifiers"_s);
}

TEST_CASE("UI: the store's pictures: a logo on the login page and screen saver, on buttons and backgrounds",
          "[flow][ui][pictures]")
{
    Screen s;
    QTemporaryDir dir;
    s.pos.shared()->imageCacheDir = dir.filePath(u"cache"_s);
    const auto picture = [&](const QString &name, QColor color, int w, int h) {
        QImage img(w, h, QImage::Format_RGB32);
        img.fill(color);
        const QString path = dir.filePath(name);
        REQUIRE(img.save(path));
        return path;
    };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    // Add Picture… on this computer: the store keeps it, under a plain name.
    // A logo: a red badge with the café's name, on a transparent background.
    const QString logoFile = dir.filePath(u"Cafe Logo.png"_s);
    {
        QImage img(360, 360, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(u"#c0392b"_s));
        p.setPen(QPen(Qt::white, 10));
        p.drawEllipse(QRectF(10, 10, 340, 340));
        p.setPen(Qt::white);
        QFont f(u"DejaVu Sans"_s, 34, QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(10, 90, 340, 90), Qt::AlignCenter, u"ViewTouch"_s);
        f.setPointSize(56);
        p.setFont(f);
        p.drawText(QRectF(10, 180, 340, 110), Qt::AlignCenter, u"Café"_s);
        p.end();
        REQUIRE(img.save(logoFile));
    }
    // What the receipt printer makes of it (Print the logo on receipts).
    if (const QByteArray dir = qgetenv("VTM_SHOTS"); !dir.isEmpty()) {
        QFile lf(logoFile);
        REQUIRE(lf.open(QIODevice::ReadOnly));
        const auto dots = print::rasterize(lf.readAll(), 576 * 3 / 4, 200);
        REQUIRE(dots);
        QImage paper(dots->width + 80, dots->height + 40, QImage::Format_RGB32);
        paper.fill(Qt::white);
        for (int y = 0; y < dots->height; ++y)
            for (int x = 0; x < dots->width; ++x)
                if (dots->dot(x, y))
                    paper.setPixel(x + 40, y + 20, qRgb(0, 0, 0));
        paper.save(QString::fromLocal8Bit(dir) + u"/41-receipt-logo-dots.png"_s);
    }
    const QString ref = s.pos.addImageFile(QUrl::fromLocalFile(logoFile).toString());
    CHECK(ref == u"store:cafe-logo.png"_s);
    QTest::qWait(30);
    REQUIRE(s.pos.storeImages().size() == 1);
    QVariantMap store = s.pos.adminRecords(u"store"_s).first().toMap();
    store[u"displayLogo"_s] = ref;
    REQUIRE(s.pos.adminSave(u"store"_s, 0, store));

    // On the login page.
    s.pos.logout();
    REQUIRE(s.c.jumpTo(u"login"_s));
    QTest::qWait(150);
    QQuickItem *logoZone = Screen::findBy(s.window->contentItem(), "zoneId", u"logo"_s);
    REQUIRE(logoZone);
    CHECK(logoZone->property("pictureUrl").toString().startsWith(u"file:"_s));
    s.shot("37-login-logo");

    // On the screen saver.
    s.c.setScreenSaverForTesting(100);
    QTest::qWait(400);
    QQuickItem *saverLogo = s.window->findChild<QQuickItem *>(u"screenSaverLogo"_s);
    REQUIRE(saverLogo);
    CHECK(saverLogo->isVisible());
    s.shot("38-screen-saver-logo");
    s.c.setScreenSaverForTesting(3'600'000);
    s.c.wake();

    // A picture on a button, and behind the Tables page.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.addImageFile(picture(u"patio.png"_s, QColor(u"#2e7d32"_s), 320, 180));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    e->selectOnly({u"host"_s});
    REQUIRE(e->setField(u"zone"_s, u"imagePath"_s, u"store:patio.png"_s));
    QTest::qWait(150);
    QQuickItem *choice = Screen::findBy(s.window->contentItem(), "objectName", u"imageChoice"_s);   // the Picture field
    REQUIRE(choice);
    CHECK(choice->property("currentText").toString() == u"patio.png"_s);
    CHECK(Screen::findBy(s.window->contentItem(), "objectName", u"imageAdd"_s));
    s.shot("40-picture-field");
    e->clearSelection();
    REQUIRE(e->setField(u"page"_s, u"background.image"_s, u"store:patio.png"_s));
    REQUIRE(e->setField(u"page"_s, u"background.imageFit"_s, u"cover"_s));
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(150);
    QQuickItem *host = Screen::findBy(s.window->contentItem(), "zoneId", u"host"_s);
    REQUIRE(host);
    CHECK(host->property("pictureUrl").toString().startsWith(u"file:"_s));
    QQuickItem *bg = s.window->findChild<QQuickItem *>(u"pagePicture"_s);
    REQUIRE(bg);
    CHECK(bg->isVisible());
    s.shot("39-pictures");
}

TEST_CASE("UI: brisket by the pound: the Weigh page asks how much", "[flow][ui][weight]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.c.activate(u"brisket"_s);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"weigh"_s);
    for (const QString &k : {u"4"_s, u"5"_s, u"8"_s})   // keys only the keypad has (Course has 1 2 3)
        s.tapKey(k);
    QTest::qWait(40);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"4.58 lb  ·  $100.76"_s));
    s.shot("36-weigh");
    s.c.activate(u"add"_s);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"items-burgers"_s);
    REQUIRE(s.pos.lines().size() == 1);
    CHECK(s.pos.lines()[0].toMap()[u"name"_s].toString().contains(u"4.58 lb"_s));
}

TEST_CASE("UI: the theme's status colors: a table with my check", "[flow][ui][statuscolors]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    REQUIRE(s.c.editor()->setField(u"theme"_s, u"status.tableMine"_s, u"#7a1fa2"_s));
    REQUIRE(s.c.leaveEditMode(true));
    CHECK(s.c.statusColors().value(u"tableMine"_s) == u"#7a1fa2"_s);

    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"cobb"_s);
    s.pos.releaseCheck();
    QTest::qWait(80);
    QQuickItem *t4 = Screen::findBy(s.window->contentItem(), "name", u"T4"_s);
    REQUIRE(t4);
    CHECK(t4->property("tint").value<QColor>() == QColor(u"#7a1fa2"_s));
}

TEST_CASE("UI: zones shown only when their rules hold; live text in labels", "[flow][ui][rules]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));               // a manager arranges the page
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const auto add = [&](const QString &label, const QString &rule, const QString &value) {
        const QString id = e->addZone(u"button"_s);
        e->selectOnly({id});
        REQUIRE(e->setField(u"zone"_s, u"label"_s, label));
        if (!rule.isEmpty())
            REQUIRE(e->setField(u"zone"_s, u"showWhen."_s + rule, value));
        return id;
    };
    const QString due = add(u"Due {check.balance} · {user.name}"_s, u"check"_s, u"open"_s);
    const QString boss = add(u"Boss Button"_s, u"login"_s, u"manager"_s);
    REQUIRE(e->setField(u"zone"_s, u"hotkey"_s, u"m"_s));
    const QString lunch = add(u"Lunch Special"_s, u"mealPeriod"_s, u"lunch"_s);
    const QString takeout = add(u"Takeout {check.label}"_s, u"checkType"_s, u"takeout"_s);
    REQUIRE(s.c.leaveEditMode(true));
    s.c.setMealPeriod(u"lunch"_s);
    QTest::qWait(60);

    const auto shown = [&](const QString &text) {
        return Screen::findBy(s.window->contentItem(), "text", text) != nullptr;
    };
    CHECK(shown(u"Boss Button"_s));
    CHECK(shown(u"Lunch Special"_s));
    CHECK_FALSE(shown(u"Due  · Morgan (Manager)"_s));   // no check open

    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                // Sam, a server
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(60);
    CHECK_FALSE(shown(u"Boss Button"_s));
    CHECK_FALSE(s.c.triggerHotkey(u"m"_s));                 // hidden: its key does nothing either

    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"cobb"_s);
    QTest::qWait(60);
    const QString balance = s.pos.totals()[u"balance"_s].toString();
    CHECK(shown(u"Due %1 · Sam"_s.arg(balance)));
    CHECK(shown(u"Takeout %1"_s.arg(s.pos.checkInfo()[u"label"_s].toString())));
    s.shot("33-rules");

    s.c.setMealPeriod(u"dinner"_s);
    QTest::qWait(30);
    CHECK_FALSE(shown(u"Lunch Special"_s));
    Q_UNUSED(due); Q_UNUSED(boss); Q_UNUSED(lunch); Q_UNUSED(takeout);
}

TEST_CASE("UI: a widget's own buttons hidden, renamed, restyled; their commands on buttons of your own",
          "[flow][ui][builtins]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    REQUIRE(e);
    e->selectOnly({u"tickets"_s});
    REQUIRE(e->setField(u"zone"_s, u"props.buttons.recall.hide"_s, true));
    REQUIRE(e->setField(u"zone"_s, u"props.buttons.allDay.label"_s, u"Everything"_s));
    REQUIRE(e->setField(u"zone"_s, u"props.buttons.allDay.order"_s, 1));   // first in the row
    REQUIRE(e->setField(u"zone"_s, u"style.normal.keyFill"_s, u"#aa2200"_s));
    QTest::qWait(150);
    s.shot("32-inspector");
    // A button of our own, anywhere: it does what All Day does.
    const QString mine = e->addZone(u"button"_s);
    REQUIRE_FALSE(mine.isEmpty());
    e->selectOnly({mine});
    e->setActions({QVariantMap{{u"type"_s, u"command"_s}, {u"name"_s, u"kitchenAllDay"_s}}});
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(100);

    const auto find = [&](const char *name) {
        return Screen::findBy(s.window->contentItem(), "objectName", QString::fromLatin1(name));
    };
    CHECK_FALSE(find("kdsRecall"));                     // hidden (only shown items are found)
    QQuickItem *allDay = find("kdsAllDay");
    REQUIRE(allDay);
    CHECK(allDay->property("text").toString() == u"Everything"_s);
    QQuickItem *station = find("kdsStation");
    REQUIRE(station);
    CHECK(allDay->mapToScene({0, 0}).x() < station->mapToScene({0, 0}).x());
    CHECK(allDay->property("color").value<QColor>() == QColor(u"#aa2200"_s));
    s.shot("31-builtins");

    s.c.activate(mine);
    QTest::qWait(50);
    CHECK(find("kdsAllDay")->property("text").toString() == u"Hide All Day"_s);

    // Seats, courses and guests from buttons too.
    REQUIRE(s.pos.selectTable(u"T3"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    s.c.enterEditMode();
    e = s.c.editor();
    const auto button = [&](const char *command) {
        const QString id = e->addZone(u"button"_s);
        e->selectOnly({id});
        e->setActions({QVariantMap{{u"type"_s, u"command"_s}, {u"name"_s, QString::fromLatin1(command)}}});
        return id;
    };
    const QString seat = button("seatNext"), course = button("courseNext"), guests = button("guestsMore");
    REQUIRE(s.c.leaveEditMode(true));
    s.c.activate(seat);
    s.c.activate(seat);
    QTest::qWait(30);
    CHECK(s.pos.checkInfo()[u"seat"_s] == 2);
    s.c.activate(course);
    QTest::qWait(30);
    CHECK(s.pos.checkInfo()[u"course"_s] == 2);
    const int entered = s.pos.property("entryGuests").toInt();   // the guest count page's number
    s.c.activate(guests);
    QTest::qWait(30);
    CHECK(s.pos.property("entryGuests").toInt() == entered + 1);
}

TEST_CASE("UI: a kitchen screen picks its station; the fries show at the fryer", "[flow][ui][stations]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.selectTable(u"T2"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"burger-combo"_s);
    s.pos.chooseOption(u"temperature"_s, 2);
    s.pos.chooseOption(u"side"_s, 0);
    s.pos.chooseOption(u"combo-drink"_s, 0);
    s.pos.finishChoosing();
    s.pos.addItem(u"caesar"_s);
    REQUIRE(s.pos.sendOrder());
    s.pos.releaseCheck();
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    QTest::qWait(150);
    const auto shows = [&](const QString &text) {
        return Screen::findBy(s.window->contentItem(), "text", text) != nullptr;
    };
    // Found again each time: a settings change rebuilds the page.
    const auto station = [&] {
        QQuickItem *k = Screen::findBy(s.window->contentItem(), "objectName", u"kdsStation"_s);
        REQUIRE(k);
        return k;
    };
    s.tapItem(station());                                   // Grill
    QTest::qWait(150);
    CHECK(s.pos.kitchenStation() == u"grill"_s);
    s.shot("29-station-grill");
    s.tapItem(station());                                   // Fryer
    QTest::qWait(150);
    CHECK(s.pos.kitchenStation() == u"fryer"_s);
    CHECK(shows(u"1  Fries"_s));
    CHECK_FALSE(shows(u"1  COMBO BGR"_s));
    s.shot("30-station-fryer");
    s.tapItem(station());                                   // Cold Line
    QTest::qWait(150);
    s.tapItem(station());                                   // back to the page's own: the kitchen
    QTest::qWait(150);
    CHECK(s.pos.kitchenStation().isEmpty());
    CHECK(shows(u"1  COMBO BGR"_s));
}

TEST_CASE("UI: a takeout ready later, picked by day, hour and minutes", "[flow][ui][later]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"takeout"_s);
    CHECK(s.c.pageId() == u"index-lunch"_s);                  // straight to the menu
    QTest::qWait(50);
    const auto key = [&](const QString &name) {
        QQuickItem *k = Screen::findBy(s.window->contentItem(), "objectName", name);
        REQUIRE(k);
        return k;
    };
    s.tapItem(key(u"who-later"_s));                          // Ready Later… on the check
    CHECK(s.c.pageId() == u"order-later"_s);
    QTest::qWait(50);
    s.tapItem(key(u"laterDay-1"_s));      // tomorrow
    s.tapItem(key(u"laterHour-18"_s));    // 6 PM
    s.tapItem(key(u"laterMinute-30"_s));
    const QDateTime due = QDateTime::fromMSecsSinceEpoch(s.pos.checkInfo()[u"dueAt"_s].toLongLong());
    CHECK(due == QDateTime(QDate::currentDate().addDays(1), QTime(18, 30)));
    QTest::qWait(50);
    CHECK(key(u"laterDue"_s)->property("text").toString().contains(u"tomorrow"_s));
    s.shot("28-order-later");
    s.tapItem(key(u"laterAsap"_s));
    CHECK(s.pos.checkInfo()[u"dueAt"_s].toLongLong() == 0);
}

TEST_CASE("UI: opening a bar tab and finding it on the tabs screen", "[flow][ui][tabs]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"4444"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"tabs"_s);
    CHECK(s.c.pageId() == u"tabs"_s);
    s.c.activate(u"new"_s);
    CHECK(s.c.pageId() == u"new-tab"_s);
    for (const char ch : {'M', 'i', 'k', 'e'})
        s.pos.textKey(QString(QChar(ch)));
    s.c.activate(u"open"_s);                          // Open Tab, then the menu
    CHECK(s.c.pageId() == u"index-lunch"_s);
    CHECK(s.pos.checkInfo()[u"label"_s] == u"Mike"_s);
    s.c.activate(u"flow-tables"_s);
    REQUIRE(s.pos.openTab(u"Ana"_s));
    s.pos.releaseCheck();
    REQUIRE(s.c.jumpTo(u"tabs"_s));
    QTest::qWait(60);
    s.shot("27-bar-tabs");
    QQuickItem *ana = Screen::findBy(s.window->contentItem(), "text", u"Ana"_s);
    REQUIRE(ana);
    s.tapItem(ana);
    CHECK(s.pos.checkInfo()[u"label"_s] == u"Ana"_s);
}

TEST_CASE("UI: the screen dims when untouched; the first touch only wakes it", "[flow][ui][saver]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.setScreenSaverForTesting(200);
    QTest::qWait(400);
    REQUIRE(s.c.asleep());
    QQuickItem *saver = Screen::findBy(s.window->contentItem(), "objectName", u"screenSaver"_s);
    REQUIRE(saver);
    CHECK(saver->isVisible());
    s.shot("26-screen-saver");

    // A touch on Quick Order while dim: awake, and no order started.
    s.c.setScreenSaverForTesting(60'000);
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Quick Order"_s));
    CHECK_FALSE(s.c.asleep());
    CHECK_FALSE(saver->isVisible());
    CHECK_FALSE(s.pos.hasCheck());
    CHECK(s.c.pageId() == u"tables"_s);
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Quick Order"_s));   // now it works
    CHECK(s.pos.hasCheck());

    // Kitchen screens stay on.
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    s.c.setScreenSaverForTesting(150);
    QTest::qWait(400);
    CHECK_FALSE(s.c.asleep());
}

TEST_CASE("UI: receiving a delivery by touch", "[flow][ui][vendors]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"receive-delivery"_s));
    QTest::qWait(80);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    s.tapItem(find(u"vendor-city-bakery"_s));
    QTest::qWait(60);
    CHECK_FALSE(find(u"qty-lettuce"_s));                 // only City Bakery's items
    s.tapItem(find(u"qty-bun"_s));
    for (const char ch : {'2', '4'})
        QTest::keyClick(s.window, ch);
    s.tapItem(find(u"cost-bun"_s));
    for (const char ch : {'0', '.', '4', '2'})
        QTest::keyClick(s.window, ch);
    QTest::qWait(60);
    s.shot("25-receive-delivery");
    const double before = s.pos.shared()->ingredient("bun")->onHand;
    s.tapItem(find(u"receiveDelivery"_s));
    QTest::qWait(80);
    CHECK(s.pos.shared()->ingredient("bun")->onHand == before + 24);
    CHECK(s.pos.shared()->ingredient("bun")->cost.cents() == 42);
    CHECK(find(u"qty-bun"_s)->property("text").toString().isEmpty());   // ready for the next one
}

TEST_CASE("UI: counting stock by touch, and the Stock Count report", "[flow][ui][stockCount]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    // A few buns went missing since the last delivery.
    REQUIRE(s.c.jumpTo(u"admin-inventory"_s));
    QTest::qWait(60);
    REQUIRE(Screen::findBy(s.window->contentItem(), "text", u"Count Stock…"_s));
    s.c.activate(u"count"_s);
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"stock-count"_s);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    s.tapItem(find(u"countVendor-city-bakery"_s));
    QTest::qWait(60);
    CHECK_FALSE(find(u"counted-lettuce"_s));               // only City Bakery's shelf
    const double buns = s.pos.shared()->ingredient("bun")->onHand;
    s.tapItem(find(u"counted-bun"_s));
    for (const char ch : {'4', '5'})
        QTest::keyClick(s.window, ch);
    QTest::qWait(60);
    CHECK(find(u"countedHowMany"_s)->property("text").toString().startsWith(u"1 of"_s));
    s.shot("stock-count");
    s.tapItem(find(u"saveStockCount"_s));
    QTest::qWait(80);
    CHECK(s.pos.shared()->ingredient("bun")->onHand == 45);
    CHECK(find(u"counted-bun"_s)->property("text").toString().isEmpty());   // ready for the next one
    REQUIRE(s.c.jumpTo(u"reports"_s));
    QTest::qWait(50);
    s.tapKey(u"Stock"_s);
    QTest::qWait(80);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Burger Buns (each)"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", QString::number(45 - buns)));
    s.shot("stock-count-report");
}

TEST_CASE("UI: logging waste by touch", "[flow][ui][waste]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(60);
    s.c.activate(u"waste-log"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"waste-log"_s);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const double buns = s.pos.shared()->ingredient("bun")->onHand;
    s.tapItem(find(u"waste-classic-burger"_s));
    s.tapItem(find(u"wasteMore"_s));                       // two of them
    s.tapItem(find(u"wasteReason-0"_s));
    QTest::qWait(60);
    s.shot("waste-log");
    s.tapItem(find(u"logWaste"_s));
    QTest::qWait(80);
    CHECK(s.pos.shared()->ingredient("bun")->onHand == buns - 2);
    REQUIRE(s.pos.wasteEntries().size() == 1);
    CHECK(s.pos.wasteEntries().back().qty == 2);
    CHECK(find(u"wasteQty"_s)->property("text").toString() == u"1"_s);   // ready for the next one
    // An ingredient, by its unit.
    s.tapItem(find(u"wasteIngredients"_s));
    QTest::qWait(40);
    s.tapItem(find(u"waste-lettuce"_s));
    s.tapItem(find(u"wasteQty"_s));
    QTest::keyClick(s.window, Qt::Key_End);
    QTest::keyClick(s.window, Qt::Key_Backspace);
    QTest::keyClick(s.window, '6');
    s.tapItem(find(u"wasteReason-1"_s));
    s.tapItem(find(u"logWaste"_s));
    QTest::qWait(80);
    REQUIRE(s.pos.wasteEntries().size() == 2);
    CHECK(s.pos.wasteEntries().back().qty == 6);
    CHECK(s.pos.wasteEntries().back().unit == "oz");
    s.shot("waste-logged");
}

TEST_CASE("UI: Store Check: ready to open, and problems marked on the Manager page", "[flow][ui][storeCheck]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    core::PrinterConfig &printer = s.pos.shared()->settings.printers.front();
    printer.watch = true;
    s.pos.shared()->setPrinterProblem(printer.id, "coverOpen");
    s.pos.shared()->noteProblem("backup", {}, "the backup drive is full");
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(80);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Store Check ⚠ 2"_s));
    s.c.activate(u"store-check"_s);
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"store-check"_s);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(find(u"readyToOpen"_s));
    CHECK(find(u"readyToOpen"_s)->property("text").toString().startsWith(u"⚠"_s));
    s.shot("store-check");
    s.tapItem(find(u"problemsSeen"_s));
    QTest::qWait(60);
    CHECK(s.pos.storeCheck()[u"unseen"_s].toInt() == 0);
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(60);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Store Check "_s));
}

TEST_CASE("UI: the kitchen screen's Labels and Waste; a prep label by touch", "[flow][ui][labels]")
{
    Screen s;
    core::PrinterConfig zebra;
    zebra.id = "prep";
    zebra.name = "Prep Labels";
    zebra.type = "file";
    zebra.path = "labels.zpl";
    zebra.format = "zpl";
    s.pos.shared()->settings.printers.push_back(zebra);
    REQUIRE(s.c.jumpTo(u"kitchen"_s));   // nobody logged in
    QTest::qWait(80);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    REQUIRE(find(u"kdsLabels"_s));
    s.tapItem(find(u"kdsLabels"_s));
    QTest::qWait(80);
    REQUIRE(s.c.pageId() == u"prep-labels"_s);
    s.tapItem(find(u"label-lettuce"_s));
    s.tapItem(find(u"keeps-48"_s));
    s.tapItem(find(u"labelsMore"_s));
    QTest::qWait(60);
    CHECK(find(u"labelCount"_s)->property("text").toString() == u"2"_s);
    CHECK(find(u"printLabels"_s)->isEnabled());
    s.shot("prep-labels");
    // Waste from there too.
    s.c.activate(u"waste"_s);
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"waste-log"_s);
    REQUIRE(s.c.jumpTo(u"kitchen"_s));
    QTest::qWait(60);
    s.tapItem(find(u"kdsWaste"_s));
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"waste-log"_s);
}

TEST_CASE("UI: a person with High Contrast and the largest text", "[flow][ui][looks]")
{
    Screen s;
    for (core::Employee &e : s.pos.shared()->employees)
        if (e.name == "Sam") {
            e.look = "contrast";
            e.textSize = 150;
        }
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(80);
    CHECK(s.pos.terminalLook() == u"contrast"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    s.pos.addItem(u"coffee"_s);
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    QTest::qWait(120);
    s.shot("high-contrast");
    // Big text still fits the check: the price and the line's keys inside it.
    QQuickItem *price = Screen::findBy(s.window->contentItem(), "text", u"$2.75"_s);
    QQuickItem *again = Screen::findBy(s.window->contentItem(), "objectName", u"lineAgain"_s);
    REQUIRE(price);
    REQUIRE(again);
    QQuickItem *list = price;
    while (list && !list->inherits("QQuickListView"))
        list = list->parentItem();
    REQUIRE(list);
    const QRectF inside = list->mapRectToScene(QRectF(0, 0, list->width(), list->height()));
    CHECK(inside.contains(price->mapRectToScene(QRectF(0, 0, price->width(), price->height()))));
    CHECK(inside.contains(again->mapRectToScene(QRectF(0, 0, again->width(), again->height()))));
}

TEST_CASE("UI: clocking in with two jobs asks which one", "[flow][ui][pay]")
{
    Screen s;
    for (const char *k : {"4", "4", "4", "4"})   // Jo: bartender, or server
        s.pos.pinKey(QString::fromLatin1(k));
    REQUIRE(s.pos.clockIn());
    QTest::qWait(60);
    QPointer<QQuickItem> server = Screen::findBy(s.window->contentItem(), "objectName", u"jobKey-server"_s);
    REQUIRE(server);
    CHECK(server->isVisible());
    s.shot("24-which-job");
    s.tapItem(server);
    QTest::qWait(60);
    CHECK(s.pos.clockInJobs().isEmpty());
    CHECK((!server || !server->isVisible()));   // the choice is gone
    REQUIRE_FALSE(s.pos.shared()->punches.empty());
    CHECK(s.pos.shared()->punches.back().job == "server");
}

// Every page and its dialogs, in English and Spanish: the labels that don't
// fit (a key's words wider than the key, a text cut off or shrunk a lot) and,
// in Spanish, words still in English. An audit, not run with the rest:
//   VTM_FIT_OUT=<dir> vtm_tests "[.fit]"   (writes en.txt, es.txt and a picture of each)
namespace {
// English phrases with a Spanish translation (set for the Spanish pass).
QSet<QString> gEnglishPhrases;

QStringList labelsCutOff(Screen &s, const QString &where)
{
    QStringList out;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
        if (!item->isVisible() || item->opacity() == 0)
            return;
        const QMetaObject *mo = item->metaObject();
        const QString text = item->property("text").toString();
        if (gEnglishPhrases.contains(text.trimmed()))
            out << u"%1 | still English | %2"_s.arg(where, text.left(80));
        if (mo->indexOfProperty("placeholderText") >= 0 && gEnglishPhrases.contains(item->property("placeholderText").toString().trimmed()))
            out << u"%1 | still English | %2"_s.arg(where, item->property("placeholderText").toString().left(80));
        if (!text.trimmed().isEmpty()) {
            if (mo->indexOfProperty("implicitContentWidth") >= 0 && mo->indexOfProperty("availableWidth") >= 0
                && item->inherits("QQuickAbstractButton")) {
                const qreal need = item->property("implicitContentWidth").toReal();
                const qreal room = item->property("availableWidth").toReal();
                if (room > 0 && need > room + 1)
                    out << u"%1 | key | %2 | %3 > %4"_s.arg(where, text).arg(qRound(need)).arg(qRound(room));
            } else if (item->inherits("QQuickText")) {
                const qreal w = item->width();
                const QFont font = item->property("font").value<QFont>();
                const qreal wide = QFontMetricsF(font).horizontalAdvance(text);
                const bool oneLine = item->property("wrapMode").toInt() == 0 && !text.contains(u'\n');
                if (item->property("truncated").toBool())
                    out << u"%1 | text cut | %2"_s.arg(where, text.left(80));
                else if (oneLine && item->property("fontSizeMode").toInt() != 0 && w > 0 && wide > w * 1.25)
                    // Shrunk to fit: to less than 80% of its size.
                    out << u"%1 | text shrunk to %2% | %3"_s.arg(where).arg(qRound(100 * w / wide)).arg(text.left(80));
                else if (oneLine && item->property("elide").toInt() == 0 && item->property("fontSizeMode").toInt() == 0
                         && w > 0 && item->property("contentWidth").toReal() > w + 2)
                    out << u"%1 | text spills over | %2"_s.arg(where, text.left(80));
            }
        }
        for (QQuickItem *child : item->childItems())
            walk(child);
    };
    walk(s.window->contentItem());
    return out;
}
} // namespace

TEST_CASE("Audit: labels that don't fit, every page and dialog, in every language", "[.fit]")
{
    const QString outDir = qEnvironmentVariable("VTM_FIT_OUT");
    QSet<QString> english;
    {
        QFile f(QStringLiteral(VTM_SEED_DIR "/../i18n/es.json"));
        REQUIRE(f.open(QIODevice::ReadOnly));
        const QJsonObject es = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = es.begin(); it != es.end(); ++it)
            if (it.value().isString() && it.value().toString() != it.key() && it.key().contains(QRegularExpression(u"[A-Za-z]{3}"_s)))
                english.insert(it.key());
    }
    for (const char *lang : {"en", "es", "fr", "pt", "zh"}) {
        gEnglishPhrases = std::string(lang) == "es" ? english : QSet<QString>();
        Screen s(false, 1280, 800);
        vt::i18n::install();
        vt::ui::followLanguage(&s.engine, &s.c);
        for (core::Employee &e : s.pos.shared()->employees)
            e.language = lang;
        REQUIRE(s.pos.loginWithPin(u"1234"_s));
        s.pos.clockIn();
        REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
        s.pos.addItem(u"cobb"_s);
        s.pos.addItem(u"soda"_s);
        QStringList found;
        for (const QVariant &v : s.c.pageChoices()) {
            const QString page = v.toMap()[u"value"_s].toString();
            if (page.isEmpty() || !s.c.jumpTo(page))
                continue;
            QTest::qWait(120);
            const QStringList onPage = labelsCutOff(s, page);
            found << onPage;
            if (!outDir.isEmpty())
                s.window->grabWindow().save(u"%1/%2-%3.png"_s.arg(outDir, QString::fromLatin1(lang), page));
            // Its dialogs, one at a time.
            // Dialogs belong to the items they're declared in (not always
            // found through QObject children from the window).
            QList<QObject *> popups;
            std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
                if (!item->isVisible())
                    return;
                for (QObject *o : item->children())
                    if (o->inherits("QQuickPopup"))
                        popups << o;
                for (QQuickItem *child : item->childItems())
                    collect(child);
            };
            collect(s.window->contentItem());
            for (QObject *root : s.engine.rootObjects())
                for (QObject *o : root->children())
                    if (o->inherits("QQuickPopup"))
                        popups << o;
            std::set<QObject *> seen;
            for (QObject *o : popups) {
                if (!seen.insert(o).second)
                    continue;
                if (!o->inherits("QQuickPopup") || o->property("opened").toBool())
                    continue;
                // Only the dialogs of what's on screen.
                QObject *owner = o->parent();
                while (owner && !qobject_cast<QQuickItem *>(owner))
                    owner = owner->parent();
                if (!owner || !qobject_cast<QQuickItem *>(owner)->isVisible())
                    continue;
                QMetaObject::invokeMethod(o, "open");
                QTest::qWait(60);
                if (o->property("opened").toBool() || o->property("visible").toBool()) {
                    const QString name = o->objectName().isEmpty() ? QString::fromLatin1(o->metaObject()->className()) : o->objectName();
                    for (const QString &x : labelsCutOff(s, page + u" / "_s + name))
                        if (!onPage.contains(QString(x).replace(u" / "_s + name, QString())))
                            found << x;
                    if (!outDir.isEmpty() && !o->objectName().isEmpty())
                        s.window->grabWindow().save(u"%1/%2-%3--%4.png"_s.arg(outDir, QString::fromLatin1(lang), page, o->objectName()));
                }
                QMetaObject::invokeMethod(o, "close");
                QTest::qWait(30);
            }
        }
        found.removeDuplicates();
        if (!outDir.isEmpty()) {
            QFile f(u"%1/%2.txt"_s.arg(outDir, QString::fromLatin1(lang)));
            REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write(found.join(u'\n').toUtf8() + '\n');
        }
        WARN(lang << ": " << found.size() << " labels don't fit");
        vt::i18n::setLanguage(u"en"_s);
    }
}

TEST_CASE("UI: Menu Builder keys fit, in English and Spanish", "[flow][ui][i18n][printmenu]")
{
    for (const char *lang : {"en", "es"}) {
        Screen s(false, 1280, 800);
        vt::i18n::install();
        vt::ui::followLanguage(&s.engine, &s.c);
        for (core::Employee &e : s.pos.shared()->employees)
            e.language = lang;
        REQUIRE(s.pos.loginWithPin(u"1234"_s));
        REQUIRE(s.c.jumpTo(u"menu-builder"_s));
        QTest::qWait(150);
        QQuickItem *root = s.window->contentItem();
        if (std::string(lang) == "es")
            CHECK(Screen::findBy(root, "text", u"Imprimir…"_s));
        for (const QString name : {u"builderImport"_s, u"builderExport"_s, u"builderTemplates"_s, u"builderPrintMenu"_s}) {
            QQuickItem *key = Screen::findBy(root, "objectName", name);
            REQUIRE(key);
            INFO(lang << ": " << key->property("text").toString().toStdString());
            // The label's whole width, inside the key's padding.
            CHECK(key->property("implicitContentWidth").toReal() <= key->property("availableWidth").toReal() + 0.5);
        }
        s.shot(std::string(lang) == "es" ? "89-builder-keys-es" : "89-builder-keys-en");
        vt::i18n::setLanguage(u"en"_s);
    }
}

TEST_CASE("UI: the screen speaks the language of whoever logs in", "[flow][ui][i18n]")
{
    Screen s;
    vt::i18n::install();
    vt::ui::followLanguage(&s.engine, &s.c);
    auto shows = [&](const QString &text) { return Screen::findBy(s.window->contentItem(), "text", text) != nullptr; };
    CHECK(shows(u"Clock In"_s));                          // the store's: English
    REQUIRE(s.pos.loginWithPin(u"5555"_s));               // Rosa
    QTest::qWait(50);
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(50);
    CHECK(shows(u"Orden rápida"_s));                      // page buttons
    CHECK(shows(u"Cuentas abiertas"_s));
    s.shot("19-spanish-tables");
    s.pos.logout();
    QTest::qWait(50);
    CHECK(s.c.pageId() == u"login"_s);
    CHECK(shows(u"Clock In"_s));                          // back to the store's
    vt::i18n::setLanguage(u"en"_s);
}

TEST_CASE("UI: Manager -> Network shows the standby, the screens and the printers", "[flow][ui][network]")
{
    Screen s;
    QVariant standby;
    s.pos.shared()->network = [&] {
        return QVariantMap{{u"role"_s, u"main"_s}, {u"machine"_s, u"office-pc"_s}, {u"standby"_s, standby},
                           {u"terminals"_s, QVariantList{QVariantMap{{u"name"_s, u"Bar"_s}, {u"address"_s, u"10.0.0.7"_s},
                                                                     {u"user"_s, u"Sam"_s}, {u"since"_s, qint64(1'700'000'000'000)}}}},
                           {u"printers"_s, QVariantList{QVariantMap{{u"name"_s, u"Kitchen"_s}, {u"type"_s, u"network"_s},
                                                                    {u"where"_s, u"10.0.0.50:9100"_s}, {u"status"_s, u"failed"_s},
                                                                    {u"error"_s, u"No answer"_s}, {u"at"_s, qint64(1'700'000'300'000)}}}}};
    };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(30);
    s.c.activate(u"network"_s);                          // Manager -> Network…
    QTest::qWait(30);
    REQUIRE(s.c.pageId() == u"network"_s);
    auto standbyText = [&] {
        QQuickItem *t = Screen::findBy(s.window->contentItem(), "objectName", u"standbyText"_s);
        REQUIRE(t);
        return t->property("text").toString();
    };
    CHECK(standbyText().startsWith(u"No standby"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Bar"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Sam is logged in"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Kitchen"_s));

    standby = QVariantMap{{u"address"_s, u"10.0.0.9"_s}, {u"since"_s, qint64(1'700'000'000'000)}, {u"standby"_s, true}};
    emit s.pos.shared()->networkChanged();
    QTest::qWait(30);
    CHECK(standbyText().startsWith(u"In sync: 10.0.0.9"_s));
    s.shot("18-network");

    // Not for staff without manager pages.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK(s.pos.networkInfo().isEmpty());
}

TEST_CASE("UI: Manager -> Factory Reset asks for RESET before it does anything", "[flow][ui][reset]")
{
    Screen s;
    int asked = 0;
    s.pos.shared()->requestFactoryReset = [&] { ++asked; return true; };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(30);
    s.c.activate(u"factory-reset"_s);                    // Manager -> Factory Reset…
    QTest::qWait(30);
    REQUIRE(s.c.pageId() == u"factory-reset"_s);
    QQuickItem *button = Screen::findBy(s.window->contentItem(), "text", u"Back Up and Reset Everything"_s);
    REQUIRE(button);
    CHECK_FALSE(button->isEnabled());
    s.tapItem(Screen::findBy(s.window->contentItem(), "placeholderText", u"RESET"_s));
    for (const char ch : {'R', 'E', 'S', 'E', 'T'})
        QTest::keyClick(s.window, ch);
    QTest::qWait(30);
    CHECK(button->isEnabled());
    s.shot("17-factory-reset");
    s.tapItem(button);
    CHECK(asked == 1);
}

TEST_CASE("UI: the expo screen - kitchen names, what's made, ready in blue", "[flow][ui][expo]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    const auto order = [&](const QString &table, std::initializer_list<const char *> items) {
        REQUIRE(s.pos.selectTable(table) == app::PosService::TableNeedsGuests);
        REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
        for (const char *i : items) {
            s.pos.addItem(QString::fromLatin1(i));
            for (const QVariant &g : s.pos.choosingInfo()[u"groups"_s].toList())   // the first of what's required
                if (!g.toMap()[u"done"_s].toBool())
                    s.pos.chooseOption(g.toMap()[u"id"_s].toString(), 0);
            s.pos.finishChoosing();
        }
        REQUIRE(s.pos.sendOrder());
        s.pos.releaseCheck();
    };
    order(u"T1"_s, {"bacon-burger", "draft-beer"});
    order(u"T2"_s, {"cobb", "kids-burger"});
    const QVariantMap t2 = s.pos.expoTickets().last().toMap();
    REQUIRE(s.pos.bumpTicket(t2[u"checkId"_s].toLongLong(), t2[u"sentAt"_s].toLongLong(), {}));
    const QVariantMap t1 = s.pos.expoTickets().first().toMap();
    REQUIRE(s.pos.bumpTicket(t1[u"checkId"_s].toLongLong(), t1[u"sentAt"_s].toLongLong(), u"kitchen"_s));
    REQUIRE(s.c.jumpTo(u"expo"_s));
    QTest::qWait(60);
    s.shot("18-expo");
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"READY"_s));
}

// --- touch targets ------------------------------------------------------------------------
// Every page on a 1280x800 screen: lists what a finger can touch that is
// smaller than 40 px (about 7 mm on a 10" screen; widgets aim for 46).

namespace {

QString describe(QQuickItem *item)
{
    for (QQuickItem *i = item; i; i = i->parentItem()) {
        for (const char *p : {"text", "placeholderText", "objectName"}) {
            const QString v = i->property(p).toString();
            if (!v.isEmpty())
                return QString::fromLatin1(item->metaObject()->className()) + u" \""_s + v.left(40) + u'"';
        }
    }
    return QString::fromLatin1(item->metaObject()->className());
}

bool touchable(QQuickItem *item)
{
    if (item->inherits("QQuickAbstractButton") || item->inherits("QQuickMouseArea") || item->inherits("QQuickComboBox")
        || item->inherits("QQuickTextField") || item->inherits("QQuickSpinBox") || item->inherits("QQuickScrollBar"))
        return true;
    for (QObject *child : item->children()) {
        if ((child->inherits("QQuickTapHandler") || child->inherits("QQuickDragHandler"))
            && child->property("enabled").toBool())   // a label's handler is off
            return true;
    }
    return false;
}

void smallTargets(QQuickItem *item, QQuickWindow *window, QStringList *out)
{
    if (!item->isVisible() || item->opacity() < 0.05)
        return;
    if (touchable(item) && item->isEnabled()) {
        // The item's box (a text field's boundingRect is just its text).
        const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        const QRectF shown = r.intersected(QRectF(QPointF(), window->size()));
        if (!shown.isEmpty() && (r.width() < 40 || r.height() < 40))
            out->append(u"%1 (%2x%3)"_s.arg(describe(item)).arg(qRound(r.width())).arg(qRound(r.height())));
    }
    for (QQuickItem *child : item->childItems())
        smallTargets(child, window, out);
}

} // namespace

TEST_CASE("Touch: every control on every page is big enough for a finger", "[ui][touch]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QStringList report;
    QDir pages(QStringLiteral(VTM_SEED_DIR "/pages"));
    for (const QString &file : pages.entryList({u"*.json"_s})) {
        const QString id = file.chopped(5);
        if (id.endsWith(u"-phone"_s) || !s.c.jumpTo(id))
            continue;
        QTest::qWait(60);
        s.shot(qPrintable(u"touch-"_s + id));
        QStringList small;
        smallTargets(s.window->contentItem(), s.window, &small);
        small.removeDuplicates();
        for (const QString &x : small)
            report << id + u": "_s + x;
    }
    WARN(report.join(u'\n').toStdString());
    CHECK(report.isEmpty());
}

TEST_CASE("Touch: the phone pages on a phone", "[ui][touch]")
{
    Screen s(false, 412, 915, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QStringList report;
    QDir pages(QStringLiteral(VTM_SEED_DIR "/pages"));
    for (const QString &file : pages.entryList({u"*-phone.json"_s})) {
        const QString id = file.chopped(5).chopped(6);   // the base page; the phone variant shows
        if (!s.c.jumpTo(id))
            continue;
        QTest::qWait(60);
        s.shot(qPrintable(u"touch-phone-"_s + id));
        QStringList small;
        smallTargets(s.window->contentItem(), s.window, &small);
        small.removeDuplicates();
        for (const QString &x : small)
            report << id + u": "_s + x;
    }
    WARN(report.join(u'\n').toStdString());
    CHECK(report.isEmpty());
}

// --- the user manual's screenshots (hidden: run with "[.manual]" and VTM_SHOTS) ---------------

#include "app/pos_demo.hh"
#include "app/pos_json.hh"

TEST_CASE("Manual: a screenshot of every screen", "[.manual]")
{
    Screen s;
    // A lived-in store: two months of demo service, promotions and texting on.
    s.pos.shared()->settings.promotions =
        app::settingsFromJson(test::readSeed("pos/settings.json").object()).promotions;
    s.pos.shared()->settings.textWebhook = "https://example.invalid/sms";
    s.pos.shared()->sendText = [](const QString &, const QString &) {};
    // Without the screen watching: it would redraw after every one of thousands of changes.
    s.c.setPos(nullptr);
    REQUIRE(app::fillDemoData(s.pos, QDateTime::currentMSecsSinceEpoch()).startsWith(u"Added"_s));
    // The demo's note to everyone ("86 Smoked Brisket tonight") would be across the top of every picture.
    s.pos.shared()->settings.notices.clear();
    s.c.setPos(&s.pos);
    // Reports over a range read back what was saved (here: what the sink recorded).
    s.pos.shared()->history = [&s](std::int64_t from, std::int64_t to) {
        std::vector<core::Check> out;
        for (const auto &[id, c] : s.sink.checks)
            if (c.status == core::CheckStatus::Closed && c.closedAt >= from && c.closedAt < to)
                out.push_back(c);
        return out;
    };
    // A clean picture: no passing message on top.
    const auto snap = [&](const char *name) {
        if (QObject *toast = s.window->findChild<QObject *>(u"toast"_s))
            toast->setProperty("opacity", 0);
        QTest::qWait(450);                                      // it fades out
        s.shot(name);
    };
    const auto go = [&](const char *page, const char *name) {
        s.c.jumpTo(QString::fromLatin1(page));
        QTest::qWait(120);
        snap(name);
    };
    const auto chooseRequired = [&] {
        for (const QVariant &g : s.pos.choosingInfo()[u"groups"_s].toList())
            if (!g.toMap()[u"done"_s].toBool())
                s.pos.chooseOption(g.toMap()[u"id"_s].toString(), 0);
        s.pos.finishChoosing();
    };

    // Logging in, the floor.
    s.pos.logout();
    go("login", "m01-login");
    REQUIRE(s.pos.loginWithPin(u"1111"_s));                     // Sam, a server
    s.pos.clockIn();
    for (const char *table : {"T2", "T5"}) {                    // a couple of tables already going
        // (The demo's service may have it going already, by the time of day.)
        const int r = s.pos.selectTable(QString::fromLatin1(table));
        if (r == app::PosService::TableOpened) {
            s.pos.releaseCheck();
            continue;
        }
        REQUIRE(r == app::PosService::TableNeedsGuests);
        s.pos.entryKey(u"4"_s);
        REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
        s.pos.addItem(u"cobb"_s);
        s.pos.addItem(u"classic-burger"_s);
        chooseRequired();                                       // its temperature and side
        s.pos.sendOrder();
        s.pos.releaseCheck();
    }
    go("tables", "m02-tables");
    REQUIRE(s.pos.selectTable(u"T3"_s) == app::PosService::TableNeedsGuests);
    go("guest-count", "m03-guest-count");
    s.pos.entryKey(u"3"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    go("index-lunch", "m04-menu-index");
    s.pos.addItem(u"bacon-burger"_s);
    s.pos.chooseOption(u"temperature"_s, 1);                    // Medium Rare
    s.pos.chooseOption(u"side"_s, 0);                           // Fries
    s.pos.finishChoosing();
    go("items-burgers", "m05-items");
    s.c.activate(u"item-1"_s);                                  // Classic Burger -> its choices
    QTest::qWait(120);
    snap("m06-modifier-page");
    s.c.jumpTo(u"items-salads"_s);
    s.pos.addItem(u"house-salad"_s);
    s.c.jumpTo(u"modifiers"_s);
    QTest::qWait(120);
    snap("m07-choose");
    chooseRequired();
    s.pos.setSeat(2);
    s.pos.setCourse(2);
    s.pos.addItem(u"caesar"_s);
    s.pos.setCourse(1);
    go("items-drinks", "m08-order-seats-courses");
    go("note", "m09-note");
    go("check-options", "m10-check-options");
    go("transfer", "m11-transfer");
    go("move-table", "m12-move-table");
    go("merge", "m13-merge");
    s.pos.sendOrder();
    go("split", "m14-split");

    // A gift card with money on it (the demo ones are spent by now).
    s.pos.releaseCheck();
    REQUIRE(s.pos.sellGiftCard(u"6000 2026 1001"_s, 5000));
    REQUIRE(s.pos.tender(u"cash"_s, 5000));
    REQUIRE(s.pos.closeCheck());

    // Takeout, customers, gift cards.
    s.pos.releaseCheck();
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    go("customer", "m15-takeout-customer");
    s.pos.findCustomers(u"Dana"_s);
    const QVariantList found = s.pos.customerResults();
    if (!found.isEmpty())
        s.pos.selectCustomer(found.first().toMap()[u"id"_s].toString());
    s.pos.useCustomer();
    go("customers", "m16-customers");
    s.pos.lookupGiftCard(u"600020261001"_s);
    go("gift-card", "m17-gift-card");
    s.pos.addItem(u"cobb"_s);
    s.pos.addItem(u"soda"_s);
    chooseRequired();
    go("settle", "m18-settle");
    s.pos.releaseCheck();

    // Drinks for the bar.
    REQUIRE(s.pos.selectTable(u"Bar 1"_s) == app::PosService::TableNeedsGuests);
    s.pos.entryKey(u"2"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    for (const char *drink : {"draft-beer", "house-wine", "soda"}) {
        s.pos.addItem(QString::fromLatin1(drink));
        chooseRequired();
    }
    s.pos.sendOrder();
    s.pos.releaseCheck();

    // The host stand and the kitchen.
    go("host", "m19-host-stand");
    go("kitchen", "m20-kitchen");
    s.tapKey(u"All Day"_s);
    QTest::qWait(60);
    snap("m21-kitchen-all-day");
    go("bar-display", "m22-bar-display");
    go("expo", "m23-expo");
    go("logout", "m24-logout");

    // Jo bartends, and serves some nights: clocking in asks which.
    s.pos.logout();
    const auto pin = [&](const char *digits) {
        for (const char *d = digits; *d; ++d)
            s.pos.pinKey(QString(QChar(*d)));
    };
    pin("4444");
    s.pos.clockOut();
    pin("4444");
    REQUIRE(s.pos.clockIn());
    go("login", "m78-which-job");
    REQUIRE(s.pos.clockInAs(u"bartender"_s));
    REQUIRE(s.pos.loginWithPin(u"1111"_s));

    // Manager.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.clockIn();
    go("manager", "m30-manager");
    const auto admin = [&](const char *page, const char *name, bool pickFirst) {
        s.c.jumpTo(QString::fromLatin1(page));
        QTest::qWait(120);
        if (pickFirst) {
            // The first record; the form is zoomed for fingers (ZoneItem.formZoom).
            const double zoom = std::min(46.0 / (25 * std::min(1600.0 / 1920, 900.0 / 1080)), 1888.0 / 680);
            s.tapCanvas(16 + 60 * zoom, 112 + 30 * zoom);
            QTest::qWait(80);
        }
        snap(name);
    };
    admin("admin-menu", "m31-menu-items", true);
    admin("admin-modifier-groups", "m32-modifier-groups", true);
    admin("admin-employees", "m33-employees", true);
    admin("admin-store", "m34-store-settings", false);
    {
        // Store Settings -> About, as a store's own computer shows it.
        const vt::net::AppBuild me = vt::net::appBuild();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        s.pos.shared()->settings.builds = {{"0.9.1270", 1270, "b0b0b0b", now - 3 * 24 * 3600 * 1000LL},
                                           {me.version.toStdString(), me.number, me.commit.toStdString(), now - 9 * 3600 * 1000LL}};
        s.pos.shared()->network = [me, now] {
            return QVariantMap{{u"role"_s, u"single"_s}, {u"machine"_s, u"cafe-counter"_s},
                               {u"build"_s, vt::net::buildText(me.version, me.number, me.commit, me.modified)},
                               {u"platform"_s, me.platform}, {u"qt"_s, QString::fromLatin1(qVersion())},
                               {u"database"_s, u"/home/cafe/.local/share/ViewTouch/viewtouch.db"_s},
                               {u"databaseBytes"_s, qint64(18'400'000)},
                               {u"backups"_s, u"/home/cafe/.local/share/ViewTouch/backups"_s},
                               {u"startedAt"_s, now - 9 * 3600 * 1000LL}, {u"lastBackup"_s, now - 3600 * 1000LL},
                               {u"history"_s, vt::net::appHistory()}};
        };
        s.c.jumpTo(u"manager"_s);   // Store Settings opened afresh, with these
        QTest::qWait(80);
        s.c.jumpTo(u"admin-store"_s);
        QTest::qWait(120);
        QQuickItem *list = Screen::findBy(s.window->contentItem(), "objectName", u"adminList"_s);
        QMetaObject::invokeMethod(list, "positionViewAtEnd");
        QTest::qWait(40);
        s.tapItem(Screen::findBy(s.window->contentItem(), "objectName", u"settingsSection-about"_s));
        QTest::qWait(80);
        snap("m34b-about");
        // Further down: the updates this store has had, and what the builds changed.
        if (QQuickItem *bar = Screen::findBy(s.window->contentItem(), "objectName", u"adminFormScroll"_s)) {
            QQuickItem *updates = Screen::findBy(s.window->contentItem(), "text", u"Updates on this store"_s);
            QQuickItem *flick = updates;
            while (flick && !flick->inherits("QQuickFlickable"))
                flick = flick->parentItem();
            if (flick && updates)
                flick->setProperty("contentY", updates->parentItem()->parentItem()->y() - 10);
            QTest::qWait(60);
            snap("m34c-about-history");
        }
        s.pos.shared()->network = nullptr;
    }
    admin("admin-taxes", "m35-taxes", false);
    admin("admin-tenders", "m36-payment-types", true);
    admin("admin-printers", "m37-printers", true);
    admin("admin-terminals", "m38-terminals", false);
    admin("admin-meal-periods", "m39-meal-periods", true);
    admin("admin-inventory", "m40-inventory", true);
    admin("admin-promotions", "m41-promotions", true);
    go("admin-schedule", "m42-schedule");
    go("network", "m42b-network");
    go("sold-out", "m43-sold-out");
    go("drawer", "m44-drawer");
    go("end-of-day", "m45-end-of-day");
    // A delivery received and a pay out for ice, for the Purchases and Expenses reports.
    REQUIRE(s.pos.receiveDelivery({{u"vendor"_s, u"valley-foods"_s}, {u"invoice"_s, u"VF-20931"_s},
                                   {u"lines"_s, QVariantList{
                                        QVariantMap{{u"ingredient"_s, u"patty"_s}, {u"qty"_s, 40}, {u"cost"_s, 1.65}},
                                        QVariantMap{{u"ingredient"_s, u"cheese"_s}, {u"qty"_s, 80}},
                                        QVariantMap{{u"ingredient"_s, u"eggs"_s}, {u"qty"_s, 90}}}}}));
    if (s.pos.drawerInfo()[u"open"_s].toBool()) {
        s.pos.entryKey(u"1800"_s);
        s.pos.setExpenseCategory(u"Ice"_s);
        s.pos.payout(core::CashMovement::Kind::Payout);
    }
    go("receive-delivery", "m76-receive-delivery");
    REQUIRE(s.pos.searchChecks(u"cobb"_s));
    for (int i = 0; i < 200 && s.pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    if (const QVariantList found = s.pos.checkSearch()[u"results"_s].toList(); !found.isEmpty())
        s.pos.selectFoundCheck(found.first().toMap()[u"id"_s].toLongLong());
    go("find-check", "m77-find-check");

    // A combo for tomorrow evening, two bar tabs, and the screen dimmed.
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"burger-combo"_s);
    s.pos.chooseOption(u"temperature"_s, 2);
    s.pos.chooseOption(u"side"_s, 2);
    s.pos.chooseOption(u"combo-drink"_s, 2);
    go("modifiers", "m79-combo");
    s.pos.finishChoosing();
    REQUIRE(s.pos.setDueAt(QDateTime(QDate::currentDate().addDays(1), QTime(18, 30)).toMSecsSinceEpoch()));
    go("order-later", "m80-order-later");
    s.pos.sendOrder();
    s.pos.releaseCheck();
    for (const QString &who : {u"Mike"_s, u"Ana, red jacket"_s, u"Table of 4 by the TV"_s}) {
        REQUIRE(s.pos.openTab(who));
        s.pos.addItem(u"draft-beer"_s);
        chooseRequired();
        s.pos.sendOrder();
        s.pos.releaseCheck();
    }
    go("tabs", "m81-bar-tabs");
    go("tables", "m81-tables");
    REQUIRE(s.pos.setKitchenStation(u"fryer"_s));
    go("kitchen", "m83-station-fryer");
    REQUIRE(s.pos.setKitchenStation(QString()));
    {   // The kitchen screen's own buttons: red, Recall hidden, All Day renamed.
        s.c.enterEditMode();
        EditorController *e = s.c.editor();
        e->selectOnly({u"tickets"_s});
        e->setField(u"zone"_s, u"props.buttons.recall.hide"_s, true);
        e->setField(u"zone"_s, u"props.buttons.allDay.label"_s, u"Everything"_s);
        e->setField(u"zone"_s, u"style.normal.keyFill"_s, u"#aa2200"_s);
        QTest::qWait(150);
        snap("m84-builtins");
        REQUIRE(s.c.leaveEditMode(false));
    }
    s.c.setScreenSaverForTesting(100);
    QTest::qWait(300);
    snap("m82-screen-saver");
    s.c.setScreenSaverForTesting(3'600'000);
    s.c.wake();
    go("reports", "m46-report-sales");
    s.tapKey(u"Items"_s);
    s.tapKey(u"Last Month"_s);                                  // both years have the whole month
    s.tapKey(u"vs Last Year"_s);
    for (int i = 0; i < 200 && s.pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    QTest::qWait(80);
    snap("m47-report-month-vs-last-year");
    s.tapKey(u"Day"_s);
    for (const char *r : {"Labor", "Kitchen", "Food Cost", "Menu Mix", "Tips", "Gift Cards", "Expenses", "Purchases", "Exceptions", "Deposit", "Customers", "Royalty", "Accounting"}) {
        s.tapKey(QString::fromLatin1(r));
        QTest::qWait(80);
        snap(QString(u"m48-report-%1"_s).arg(QString::fromLatin1(r).toLower().replace(u' ', u'-')).toLatin1().constData());
    }
    go("closed-checks", "m49-closed-checks");
    go("factory-reset", "m50-factory-reset");

    // Network, as it looks with a standby server and a few screens.
    s.pos.shared()->network = [] {
        const qint64 since = QDateTime::currentMSecsSinceEpoch() - 3 * 3600 * 1000;
        QVariantList terminals;
        for (const auto &[name, address, user] : {std::tuple{"Bar", "192.168.1.21", "Jo"}, {"Patio Tablet", "192.168.1.34", "Sam"},
                                                 {"Kitchen", "192.168.1.40", ""}})
            terminals << QVariantMap{{u"name"_s, QString::fromLatin1(name)}, {u"address"_s, QString::fromLatin1(address)},
                                     {u"user"_s, QString::fromLatin1(user)}, {u"since"_s, since}};
        return QVariantMap{{u"role"_s, u"main"_s}, {u"machine"_s, u"office-pc"_s}, {u"terminals"_s, terminals},
                           {u"standby"_s, QVariantMap{{u"address"_s, u"192.168.1.11"_s}, {u"since"_s, since}}},
                           {u"printers"_s, QVariantList{
                               QVariantMap{{u"name"_s, u"Kitchen"_s}, {u"where"_s, u"192.168.1.50:9100"_s}, {u"status"_s, u"ok"_s},
                                           {u"at"_s, since + 3 * 3600 * 1000 - 120000}},
                               QVariantMap{{u"name"_s, u"Receipt"_s}, {u"where"_s, u"192.168.1.51:9100"_s}, {u"status"_s, u"failed"_s},
                                           {u"error"_s, u"No answer (is it on?)"_s}, {u"at"_s, since + 3 * 3600 * 1000 - 60000}}}}};
    };
    emit s.pos.shared()->networkChanged();
    go("network", "m52-network-standby");
    s.pos.shared()->network = nullptr;

    // Spanish: Rosa's screens.
    vt::i18n::install();
    vt::ui::followLanguage(&s.engine, &s.c);
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"5555"_s));
    go("tables", "m53-spanish-tables");
    REQUIRE(s.pos.selectTable(u"T6"_s) == app::PosService::TableNeedsGuests);
    s.pos.entryKey(u"2"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"bacon-burger"_s);
    go("modifiers", "m54-spanish-choices");
    s.pos.cancelChoosing();
    s.pos.releaseCheck();
    s.pos.logout();
    vt::i18n::setLanguage(u"en"_s);
    s.engine.retranslate();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));

    // The self-order kiosk, as a guest sees it.
    s.pos.logout();
    s.pos.enableSelfOrder();
    QTest::qWait(150);
    snap("m55-kiosk-welcome");
    REQUIRE(s.pos.kioskStart(false));
    REQUIRE(s.pos.kioskAdd(u"cheeseburger"_s));
    s.pos.finishChoosing();
    REQUIRE(s.pos.kioskAdd(u"classic-burger"_s));
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));
    QTest::qWait(150);
    snap("m56-kiosk-choices");
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));
    REQUIRE(s.pos.finishChoosing());
    REQUIRE(s.pos.kioskRemove(s.pos.lines().value(0).toMap()[u"id"_s].toLongLong()));   // the first, unfinished
    REQUIRE(s.pos.kioskAdd(u"lemonade"_s));
    REQUIRE(s.pos.chooseOption(u"drink-size"_s, 1));
    REQUIRE(s.pos.finishChoosing());
    QTest::qWait(150);
    snap("m57-kiosk-order");
    QQuickItem *review = Screen::findBy(s.window->contentItem(), "objectName", u"kioskReview"_s);
    REQUIRE(review);
    s.tapItem(review);
    QTest::qWait(120);
    for (const char ch : {'M', 'a', 'r', 'i', 'a'})
        QTest::keyClick(s.window, ch);
    snap("m58-kiosk-name");
    REQUIRE(s.pos.kioskFinish({{u"name"_s, u"Maria"_s}}));
    QTest::qWait(150);
    snap("m59-kiosk-number");
    REQUIRE(s.pos.leaveSelfOrder(u"1234"_s));

    // Editing pages.
    s.c.jumpTo(u"tables"_s);
    REQUIRE(s.c.requestEditMode());
    QTest::qWait(150);
    snap("m51-edit-mode");
}

TEST_CASE("Manual: the phone screens", "[.manual]")
{
    Screen s(false, 540, 1080, u"phone"_s);
    QTest::qWait(300);
    const auto snap = [&](const char *page, const char *name) {
        s.c.jumpTo(QString::fromLatin1(page));
        QTest::qWait(200);
        if (QObject *toast = s.window->findChild<QObject *>(u"toast"_s))
            toast->setProperty("opacity", 0);
        QTest::qWait(450);                                      // it fades out
        s.shot(name);
    };
    s.pos.logout();
    snap("login", "m60-phone-login");
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    for (const char *table : {"T2", "T5"}) {
        REQUIRE(s.pos.selectTable(QString::fromLatin1(table)) == app::PosService::TableNeedsGuests);
        s.pos.entryKey(u"2"_s);
        REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
        s.pos.addItem(u"cobb"_s);
        s.pos.sendOrder();
        s.pos.releaseCheck();
    }
    snap("tables", "m61-phone-tables");
    REQUIRE(s.pos.selectTable(u"T7"_s) == app::PosService::TableNeedsGuests);
    s.pos.entryKey(u"2"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"cobb"_s);
    s.pos.addItem(u"caesar"_s);
    snap("index-lunch", "m62-phone-order");
    snap("settle", "m63-phone-settle");
}

TEST_CASE("Memory: a screen through a long service stays flat", "[.memory]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.c.jumpTo(u"tables"_s));
    const auto rss = [] {
        QFile f(u"/proc/self/status"_s);
        f.open(QIODevice::ReadOnly);
        for (const QByteArray &line : f.readAll().split('\n'))
            if (line.startsWith("VmRSS:"))
                return line.mid(6).trimmed().split(' ').first().toLong();
        return 0L;
    };
    const auto serve = [&](int n) {
        for (int i = 0; i < n; ++i) {
            REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
            s.pos.addItem(u"cobb"_s);
            s.pos.addItem(u"water"_s);
            s.pos.sendOrder();
            s.pos.tender(u"cash"_s);
            REQUIRE(s.pos.closeCheck());
            if (i % 10 == 0)
                QTest::qWait(1);   // let the screen redraw, as it would
        }
    };
    serve(100);
    QTest::qWait(200);
    const long after100 = rss();
    serve(300);
    QTest::qWait(200);
    const long after400 = rss();
    WARN("RSS after 100 checks " << after100 << " kB, after 400 " << after400 << " kB, per check "
         << (after400 - after100) / 300.0 << " kB");
}

TEST_CASE("UI: a server's void waits for a manager's PIN on the same screen", "[flow][ui][approval]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"cobb"_s);
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.c.jumpTo(u"items-salads"_s));
    QTest::qWait(50);
    s.c.activate(u"flow-void"_s);                                // Void, as Sam
    QTest::qWait(80);
    s.giveReason();
    CHECK(s.pos.approvalInfo()[u"needed"_s].toBool());
    s.shot("19-approval");
    std::function<QQuickItem *(QQuickItem *, const QString &)> named = [&](QQuickItem *root, const QString &name) -> QQuickItem * {
        for (QQuickItem *i : root->childItems()) {
            if (i->isVisible() && i->objectName() == name)
                return i;
            if (QQuickItem *hit = named(i, name))
                return hit;
        }
        return nullptr;
    };
    for (const char *k : {"1", "2", "3", "4", "OK"})
        s.tapItem(named(s.window->contentItem(), u"approvalKey-"_s + QString::fromLatin1(k)));
    QTest::qWait(50);
    CHECK_FALSE(s.pos.approvalInfo()[u"needed"_s].toBool());
    CHECK(s.pos.lines().first().toMap()[u"voided"_s].toBool());
}

TEST_CASE("UI: idle screens log out; messages show where they're meant to", "[flow][ui][messages]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    // Another screen (the kitchen) sends to the floor; this is a floor screen.
    app::PosService kitchen(s.pos.shared(), u"Kitchen"_s);
    REQUIRE(kitchen.sendMessage(u"floor"_s, u"86 salmon - out until tomorrow"_s));
    QTest::qWait(80);
    QQuickItem *ok = s.window->findChild<QQuickItem *>(u"messageOk"_s);
    REQUIRE(ok);
    CHECK(ok->isVisible());
    s.shot("20-message");
    s.tapItem(ok);
    CHECK_FALSE(ok->isVisible());
    // A message for the kitchen screens doesn't show on the floor.
    REQUIRE(kitchen.sendMessage(u"kitchen"_s, u"Order up"_s));
    QTest::qWait(50);
    CHECK_FALSE(ok->isVisible());

    // Posted until tonight: it comes back for the next person who logs in.
    REQUIRE(kitchen.sendMessage(u"all"_s, u"Wine dinner Friday: sell tickets"_s,
                                QDateTime(QDate::currentDate(), QTime(23, 59)).toMSecsSinceEpoch()));
    QTest::qWait(50);
    CHECK(ok->isVisible());
    s.tapItem(ok);
    CHECK_FALSE(ok->isVisible());
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"4444"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(50);
    CHECK(ok->isVisible());
    s.tapItem(ok);

    // Idle: logged out by itself (a short time for the test).
    s.c.setIdleTimeoutForTesting(150);
    QTest::qWait(400);
    CHECK_FALSE(s.pos.loggedIn());
    CHECK(s.c.pageId() == u"login"_s);
}

TEST_CASE("Kiosk on a 1080x1920 portrait screen (Chipsee KIOSK-CM4-215)", "[flow][ui][kiosk][portrait]")
{
    // A 21.5" floor-standing kiosk: tall, guests reaching up to it.
    Screen s(false, 1080, 1920);
    s.pos.enableSelfOrder();
    QTest::qWait(100);
    auto find = [&](const QString &name) {
        QQuickItem *kiosk = Screen::findBy(s.window->contentItem(), "objectName", u"selfOrder"_s);
        REQUIRE(kiosk);
        // The visible one (landscape and portrait share some names).
        std::function<QQuickItem *(QQuickItem *)> look = [&](QQuickItem *item) -> QQuickItem * {
            if (!item->isVisible())
                return nullptr;
            if (item->objectName() == name)
                return item;
            for (QQuickItem *c : item->childItems())
                if (QQuickItem *f = look(c))
                    return f;
            return nullptr;
        };
        return look(kiosk);
    };
    // In reach: what a guest touches sits in the lower two-thirds.
    const auto lowerTwoThirds = [&](QQuickItem *item) {
        REQUIRE(item);
        return item->mapToScene(QPointF(0, 0)).y() >= 1920.0 / 3;
    };
    s.shot("p1-attract");
    s.tapItem(find(u"kioskAttract"_s));
    QTest::qWait(60);
    CHECK(lowerTwoThirds(find(u"kioskForHere"_s)));
    s.shot("p2-where");
    s.tapItem(find(u"kioskForHere"_s));
    REQUIRE(s.pos.selfOrderInfo()[u"ordering"_s].toBool());
    QTest::qWait(60);
    for (const char *name : {"kioskReview", "kioskOrderBar", "kioskStartOver", "kioskEasyReach"})
        CHECK(lowerTwoThirds(find(QString::fromLatin1(name))));

    // A burger: its choices come up from the bottom.
    REQUIRE(s.pos.kioskAdd(u"classic-burger"_s));
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 1));
    QTest::qWait(100);
    CHECK(lowerTwoThirds(find(u"kioskChoicesDone"_s)));
    s.shot("p3-choices");
    REQUIRE(s.pos.chooseOption(u"side"_s, 2));
    s.tapItem(find(u"kioskChoicesDone"_s));
    REQUIRE(s.pos.kioskAdd(u"lemonade"_s));
    REQUIRE(s.pos.chooseOption(u"drink-size"_s, 1));
    REQUIRE(s.pos.finishChoosing());
    QTest::qWait(100);
    s.shot("p4-menu");
    CHECK(find(u"kioskOrderBar"_s)->property("text").toString().startsWith(u"2 items"_s));

    // Easy Reach: everything in the lower part of the screen.
    s.tapItem(find(u"kioskEasyReach"_s));
    QTest::qWait(400);
    QQuickItem *items = find(u"kioskItems"_s);
    REQUIRE(items);
    CHECK(items->mapToScene(QPointF(0, 0)).y() >= 1920 * 0.4 - 1);
    s.shot("p5-easy-reach");
    s.tapItem(find(u"kioskEasyReach"_s));
    QTest::qWait(400);
    CHECK(find(u"kioskItems"_s)->mapToScene(QPointF(0, 0)).y() < 1920 * 0.2);   // full screen again

    // The order sheet, then a name and the number.
    s.tapItem(find(u"kioskOrderBar"_s));
    QTest::qWait(100);
    s.shot("p6-order-sheet");
    s.tapItem(Screen::findBy(s.window->contentItem(), "text", u"Keep Ordering"_s));
    s.tapItem(find(u"kioskReview"_s));
    QTest::qWait(100);
    CHECK(lowerTwoThirds(find(u"kioskName"_s)));
    for (const char ch : {'A', 'n', 'a'})
        QTest::keyClick(s.window, ch);
    s.shot("p7-name");
    s.tapItem(find(u"kioskPlace"_s));
    QTest::qWait(100);
    REQUIRE(find(u"kioskNumber"_s));
    s.shot("p8-number");
}

// Every page on a phone held upright, as screenshots (VTM_SHOTS): run by hand
// with [phoneaudit] to look them over.
TEST_CASE("Phone audit: every page in portrait", "[.][phoneaudit]")
{
    Screen s(false, 412, 870, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"cobb"_s);
    s.c.finishChoosing();
    s.pos.addItem(u"water"_s);
    QStringList ids;
    for (const layout::Page &p : s.c.layout().pages)
        if (p.formFactor != u"phone" && p.kind != u"template" && p.kind != u"library")
            ids << p.id;
    ids.sort();
    for (const QString &id : ids) {
        if (!s.c.jumpTo(id))
            continue;
        QTest::qWait(60);
        s.shot((u"phone-"_s + id).toUtf8().constData());
    }
    QTest::qWait(30);
}

TEST_CASE("Host stand screen: pick a party, push tables together, seat; bussing", "[ui][host]")
{
    for (const bool phone : {false, true}) {
        Screen s(false, phone ? 412 : 1600, phone ? 870 : 900, phone ? u"phone"_s : QString());
        REQUIRE(s.pos.loginWithPin(u"1234"_s));
        const qint64 lee = s.pos.addReservation({{u"name"_s, u"Lee"_s}, {u"size"_s, 8},
                                                 {u"at"_s, QDateTime::currentMSecsSinceEpoch() + 3'600'000}});
        REQUIRE(lee > 0);
        REQUIRE(s.pos.addToWaitlist({{u"name"_s, u"Kim"_s}, {u"size"_s, 2}}) > 0);
        REQUIRE(s.pos.setTableState(u"T3"_s, u"dirty"_s));
        REQUIRE(s.c.jumpTo(u"seating"_s));
        QTest::qWait(60);
        QQuickItem *root = s.window->contentItem();
        const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };

        // Hold T5 + T6 for Lee.
        s.tapItem(by(u"hostBooked"_s));
        s.tapItem(by(u"hostParty-Lee"_s));
        s.tapItem(by(u"host-T5"_s));
        s.tapItem(by(u"host-T6"_s));
        s.shot(phone ? "host-phone-picked" : "host-picked");
        CHECK(by(u"hostHint"_s)->property("text").toString().startsWith(u"T5 + T6 for 8 guests"_s));
        s.tapItem(by(u"hostHold"_s));
        CHECK(s.pos.floor().value(u"T6"_s).toMap().value(u"state"_s) == u"reserved"_s);

        // Seat Kim at T1, and a walk-in of 3 at T2.
        s.tapItem(by(u"hostWaiting"_s));
        s.tapItem(by(u"hostParty-Kim"_s));
        s.tapItem(by(u"host-T1"_s));
        s.tapItem(by(u"hostSeat"_s));
        CHECK(s.pos.floor().value(u"T1"_s).toMap().value(u"state"_s) == u"seated"_s);
        s.tapItem(by(u"hostWalkIn"_s));
        s.tapItem(by(u"host-T2"_s));
        s.tapItem(by(u"hostSeat"_s));
        CHECK(s.pos.floor().value(u"T2"_s).toMap().value(u"guests"_s) == 2);

        // T3 bussed.
        s.tapItem(by(u"host-T3"_s));
        s.tapItem(by(u"hostClean"_s));
        CHECK_FALSE(s.pos.floor().contains(u"T3"_s));
        s.tapItem(by(u"host-T4"_s));
        s.tapItem(by(u"hostDirty"_s));
        CHECK(s.pos.floor().value(u"T4"_s).toMap().value(u"state"_s) == u"dirty"_s);
        QTest::qWait(30);
        s.shot(phone ? "host-phone" : "host");
        CHECK_FALSE(s.pos.hasCheck());   // the host doesn't keep the tables' checks
    }
}

TEST_CASE("Phone orders: name, phone and address right on the check", "[flow][ui][who]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveCustomer({{u"name"_s, u"Dana Ruiz"_s}, {u"phone"_s, u"555-0142"_s},
                                {u"address"_s, u"12 Oak St"_s}}));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"delivery"_s);
    CHECK(s.c.pageId() == u"index-lunch"_s);
    QTest::qWait(50);
    const auto key = [&](const QString &name) {
        QQuickItem *k = Screen::findBy(s.window->contentItem(), "objectName", name);
        REQUIRE(k);
        return k;
    };
    CHECK(key(u"who-name"_s)->property("text") == u"+ Name"_s);
    CHECK(key(u"who-address"_s)->property("text") == u"+ Address"_s);
    s.shot("who-1-empty");

    // Type a name.
    s.tapItem(key(u"who-name"_s));
    for (char ch : std::string("Sam"))
        QTest::keyClick(s.window, ch);
    s.shot("who-2-typing");
    s.tapItem(key(u"whoSave"_s));
    CHECK(s.pos.checkInfo()[u"customer"_s].toMap()[u"name"_s] == u"Sam"_s);
    CHECK(key(u"who-name"_s)->property("text") == u"Sam"_s);

    // A phone number that matches a regular: touch them, and everything fills in.
    s.tapItem(key(u"who-phone"_s));
    for (char ch : std::string("0142"))
        QTest::keyClick(s.window, ch);
    QTest::qWait(30);
    QQuickItem *regular = nullptr;
    for (const QVariant &v : s.pos.customerResults())
        if (v.toMap()[u"name"_s] == u"Dana Ruiz"_s)
            regular = key(u"regular-"_s + v.toMap()[u"id"_s].toString());
    s.shot("who-3-regular");
    s.tapItem(regular);
    const QVariantMap who = s.pos.checkInfo()[u"customer"_s].toMap();
    CHECK(who[u"name"_s] == u"Dana Ruiz"_s);
    CHECK(who[u"address"_s] == u"12 Oak St"_s);
    CHECK(key(u"who-address"_s)->property("text") == u"12 Oak St"_s);

    // Change just the address: Enter saves.
    s.tapItem(key(u"who-address"_s));
    QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
    for (char ch : std::string("9 Elm Ave"))
        QTest::keyClick(s.window, ch);
    QTest::keyClick(s.window, Qt::Key_Return);
    CHECK(s.pos.checkInfo()[u"customer"_s].toMap()[u"address"_s] == u"9 Elm Ave"_s);
    CHECK(s.pos.checkInfo()[u"customer"_s].toMap()[u"phone"_s] == u"555-0142"_s);
    QTest::qWait(30);
    s.shot("who-4-done");
}

TEST_CASE("On-screen keyboard: on by default, over the screen, lifts a covered field, off by setting", "[ui][keyboard]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QQmlApplicationEngine engine;
    // No --touch-keyboard: the terminal's setting decides (on when not set).
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&s.c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    CHECK(window->property("touchKeyboard").toBool());
    QQuickItem *root = window->contentItem();
    const auto by = [&](QQuickItem *in, const QString &name) { return Screen::findBy(in, "objectName", name); };
    const auto tap = [&](QQuickItem *item) {
        REQUIRE(item);
        QTest::mouseClick(window, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QTest::qWait(30);
    };
    // A key of the keyboard: its label's parent.
    const auto key = [&](QQuickItem *keyboard, const QString &label) {
        QQuickItem *text = Screen::findBy(keyboard, "text", label);
        REQUIRE(text);
        return text->parentItem();
    };

    // A takeout's name, typed on the screen's keyboard: over the page, which keeps its size.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"takeout"_s);
    QTest::qWait(50);
    QQuickItem *page = window->findChild<QQuickItem *>(u"pageSurface"_s);
    REQUIRE(page);
    const QSizeF pageSize(page->width() * page->scale(), page->height() * page->scale());
    tap(by(root, u"who-name"_s));
    QObject *popup = window->findChild<QObject *>(u"touchKeys"_s);
    REQUIRE(popup);
    CHECK(popup->property("visible").toBool());
    auto *docked = qvariant_cast<QQuickItem *>(popup->property("contentItem"));
    REQUIRE(docked);
    QTest::qWait(200);
    CHECK(QSizeF(page->width() * page->scale(), page->height() * page->scale()) == pageSize);
    CHECK(window->property("lift").toReal() == 0);                // the field is above the keyboard
    for (const char *k : {"S", "a", "m"})                         // a capital to start
        tap(key(docked, QString::fromLatin1(k)));
    CHECK(by(root, u"whoField"_s)->property("text") == u"Sam"_s);
    tap(key(docked, u"⏎"_s));                                    // Enter saves it
    CHECK(s.pos.checkInfo()[u"customer"_s].toMap()[u"name"_s] == u"Sam"_s);
    const auto shot = [&](const char *name) {
        if (const QByteArray dir = qgetenv("VTM_SHOTS"); !dir.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(dir) + u'/' + QString::fromLatin1(name) + u".png"_s);
    };

    // A phone number: the number pad.
    tap(by(root, u"who-phone"_s));
    QTest::qWait(30);
    CHECK(Screen::findBy(docked, "text", u"q"_s) == nullptr);
    for (const char *k : {"5", "5", "5"})
        tap(key(docked, QString::fromLatin1(k)));
    CHECK(by(root, u"whoField"_s)->property("text") == u"555"_s);
    shot("kb-1-numbers");
    tap(by(root, u"whoSave"_s));

    // A field low on the screen: the page slides up to keep it in view, and back after.
    REQUIRE(s.c.jumpTo(u"customer"_s));
    QTest::qWait(80);
    QQuickItem *lowest = nullptr;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
        if (!item->isVisible())
            return;
        if ((item->inherits("QQuickTextField") || item->inherits("QQuickTextArea"))
            && (!lowest || item->mapToScene(QPointF(0, 0)).y() > lowest->mapToScene(QPointF(0, 0)).y()))
            lowest = item;
        for (QQuickItem *child : item->childItems())
            walk(child);
    };
    walk(page);
    REQUIRE(lowest);
    tap(lowest);
    QTest::qWait(300);
    CHECK(window->property("lift").toReal() > 0);
    const qreal keysTop = window->height() - docked->height();
    CHECK(lowest->mapToScene(QPointF(0, lowest->height())).y() <= keysTop);
    shot("kb-0-lifted");
    tap(key(docked, u"⌨▾"_s));                                   // put away: the page comes back
    QTest::qWait(300);
    CHECK(window->property("lift").toReal() == 0);
    s.pos.releaseCheck();

    // The setup guide covers the page: the keyboard floats over it.
    s.c.openSetup();
    QTest::qWait(80);
    tap(by(root, u"setupNext"_s));                                // past the welcome
    QTest::qWait(80);
    QQuickItem *storeName = by(root, u"setupStoreName"_s);
    REQUIRE(storeName);
    tap(storeName);
    CHECK(popup->property("visible").toBool());
    auto *keys = qvariant_cast<QQuickItem *>(popup->property("contentItem"));
    REQUIRE(keys);
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    for (const char *k : {"b", "o", "b"})
        tap(key(keys, QString::fromLatin1(k)));
    CHECK(storeName->property("text").toString().endsWith(u"bob"_s));
    shot("kb-2-setup");

    // Off for this terminal (it has a keyboard): none shows.
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, {{u"name"_s, s.pos.terminalName()}, {u"keyboard"_s, u"off"_s}}));
    QTest::qWait(30);
    CHECK_FALSE(window->property("touchKeyboard").toBool());
    CHECK_FALSE(popup->property("visible").toBool());
}

TEST_CASE("Order screen: swipe a line, same as last time, deliveries board", "[ui][swipe][phoneorders]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    // Drag across a line by dx (a swipe).
    const auto swipe = [&](const QString &name, qreal dx) {
        QQuickItem *text = Screen::findBy(root, "text", name);
        REQUIRE(text);
        const QPoint from = text->mapToScene(QPointF(text->width() / 2, text->height() / 2)).toPoint();
        QTest::mousePress(s.window, Qt::LeftButton, {}, from);
        for (int i = 1; i <= 10; ++i) {
            QTest::mouseMove(s.window, from + QPoint(int(dx * i / 10), 0));
            QTest::qWait(10);
        }
        if (dx < 0)
            s.shot("phone-0-swipe");
        QTest::mouseRelease(s.window, Qt::LeftButton, {}, from + QPoint(int(dx), 0));
        QTest::qWait(50);
    };

    // A regular's takeout.
    REQUIRE(s.pos.saveCustomer({{u"name"_s, u"Dana Ruiz"_s}, {u"phone"_s, u"555-0142"_s}}));
    QString dana;
    for (const core::CustomerRecord &c : s.pos.shared()->customers)
        dana = QString::fromStdString(c.id);
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"takeout"_s);
    REQUIRE(s.pos.useCustomer(dana));
    s.pos.addItem(u"coffee"_s);
    s.pos.addItem(u"water"_s);
    QTest::qWait(50);
    CHECK(by(u"readyQuote"_s)->property("text").toString().startsWith(u"Ready in about"_s));

    // Swipe right: one more. Swipe left: off.
    swipe(u"Coffee"_s, 300);
    CHECK(s.pos.lines()[0].toMap()[u"quantity"_s] == 2);
    swipe(u"Water"_s, -160);
    CHECK(s.pos.lines().size() == 1);
    swipe(u"2 × Coffee"_s, 40);                                  // a short drag does nothing
    CHECK(s.pos.lines()[0].toMap()[u"quantity"_s] == 2);
    REQUIRE(s.pos.sendOrder());
    REQUIRE(s.pos.tender(u"cash"_s));
    REQUIRE(s.pos.closeCheck());

    // Next time: Same as Last Time.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"takeout"_s);
    REQUIRE(s.pos.useCustomer(dana));
    QTest::qWait(50);
    QQuickItem *same = by(u"sameAsLastTime"_s);
    REQUIRE(same);
    CHECK(same->property("text") == u"Same as Last Time: 2 × Coffee"_s);
    s.shot("phone-1-same");
    s.tapItem(same);
    CHECK(s.pos.lines().size() == 1);
    CHECK_FALSE(by(u"sameAsLastTime"_s));                       // gone once there's an order
    s.pos.releaseCheck();

    // Deliveries: one sent, one not.
    QVariantMap e = s.pos.adminNewRecord(u"employees"_s);
    e[u"name"_s] = u"Lou"_s;
    e[u"role"_s] = u"driver"_s;
    e[u"pin"_s] = u"7777"_s;
    REQUIRE(s.pos.adminSave(u"employees"_s, -1, e));
    for (const auto &[name, address] : {std::pair{u"Ana"_s, u"12 Oak St"_s}, std::pair{u"Bo"_s, u"3 Pine Ave, apt 2"_s}}) {
        REQUIRE(s.pos.startCheck(core::CheckType::Delivery));
        s.pos.addItem(u"coffee"_s);
        REQUIRE(s.pos.setCustomer({{u"name"_s, name}, {u"address"_s, address}}));
        if (name == u"Ana"_s)
            REQUIRE(s.pos.sendOrder());
        s.pos.releaseCheck();
    }
    REQUIRE(s.c.jumpTo(u"deliveries"_s));
    QTest::qWait(80);
    const auto row = [&](const QString &name) {
        for (const QVariant &v : s.pos.deliveries())
            if (v.toMap()[u"name"_s] == name)
                return v.toMap();
        return QVariantMap();
    };
    const qint64 ana = row(u"Ana"_s)[u"id"_s].toLongLong();
    s.tapItem(by(u"delivery-"_s + QString::number(ana)));
    s.tapItem(by(u"driver-Lou"_s));
    s.shot("phone-2-deliveries");
    s.tapItem(by(u"deliverySendOut"_s));
    CHECK(row(u"Ana"_s)[u"state"_s] == u"out"_s);
    CHECK(row(u"Bo"_s)[u"state"_s] == u"new"_s);
    s.tapItem(by(u"delivery-"_s + QString::number(ana)));
    s.tapItem(by(u"deliveryBack"_s));
    CHECK(row(u"Ana"_s)[u"state"_s] == u"back"_s);
    QTest::qWait(50);
    s.shot("phone-3-back");
    s.tapItem(by(u"delivery-"_s + QString::number(ana)));
    s.tapItem(by(u"deliveryOpen"_s));
    QTest::qWait(50);
    CHECK(s.pos.checkInfo()[u"id"_s].toLongLong() == ana);
}

TEST_CASE("Qualifiers: No opens the item's choices; hold a choice for Lite / Extra / Side", "[ui][qualifiers]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.c.orderItem(u"classic-burger"_s);
    QTest::qWait(80);
    REQUIRE(s.pos.chooseOption(u"temperature"_s, 2));
    REQUIRE(s.pos.chooseOption(u"side"_s, 0));
    s.c.finishChoosing();
    QTest::qWait(80);
    const QString menuPage = s.c.pageId();
    CHECK(menuPage == u"items-burgers"_s);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };

    // No: the burger's choices open, waiting for what to leave off.
    s.c.activate(u"flow-no"_s);
    QTest::qWait(120);
    CHECK(s.pos.pendingQualifier() == u"no"_s);
    CHECK(s.c.pageId() != menuPage);
    QQuickItem *onion = by(u"option-Onion"_s);
    REQUIRE(onion);
    s.tapItem(onion);
    QTest::qWait(50);
    CHECK(by(u"option-Onion"_s)->property("text").toString().startsWith(u"No Onion"_s));

    // Held: how to have it.
    QQuickItem *bacon = by(u"option-Tomato"_s);   // in view, beside the onion
    REQUIRE(bacon);
    const QPoint at = bacon->mapToScene(QPointF(bacon->width() / 2, bacon->height() / 2)).toPoint();
    QTest::mousePress(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(700);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(50);
    REQUIRE(by(u"qualifierSheet"_s));
    s.shot("qual-1-hold");
    s.tapItem(by(u"how-extra"_s));
    QTest::qWait(50);
    CHECK_FALSE(by(u"qualifierSheet"_s));
    CHECK(by(u"option-Tomato"_s)->property("text").toString().startsWith(u"Extra Tomato"_s));
    s.shot("qual-2-chosen");
    QStringList mods;
    for (const QVariant &m : s.pos.lines().last().toMap()[u"modifiers"_s].toList())
        mods << m.toMap()[u"name"_s].toString();
    CHECK(mods.contains(u"No Onion"_s));
    CHECK(mods.contains(u"Extra Tomato"_s));
}

TEST_CASE("Qualifiers: a finger on No, Extra... or Cancel presses nothing behind them", "[ui][qualifiers][touch]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.c.orderItem(u"classic-burger"_s);
    QTest::qWait(80);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    static QPointingDevice *finger = QTest::createTouchDevice();
    const auto touch = [&](QQuickItem *item) {
        REQUIRE(item);
        const QPoint at = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        QTest::touchEvent(s.window, finger).press(0, at, s.window);
        QTest::qWait(60);
        QTest::touchEvent(s.window, finger).release(0, at, s.window);
        QTest::qWait(80);
    };
    const auto chosen = [&] {
        QStringList names;
        for (const QVariant &g : s.pos.choosingInfo()[u"groups"_s].toList())
            for (const QVariant &o : g.toMap()[u"options"_s].toList())
                if (o.toMap()[u"chosen"_s].toBool())
                    names << o.toMap()[u"qualifier"_s].toString() + o.toMap()[u"name"_s].toString();
        names.sort();
        return names;
    };
    const QStringList before = chosen();
    // Each answer, and Cancel: only the ingredient asked about changes.
    for (const char *answer : {"how-no", "how-extra", "how-side", "how-lite"}) {
        touch(by(u"option-Tomato"_s));   // comes on it: asks how
        REQUIRE(by(u"qualifierSheet"_s));
        QQuickItem *key = by(QString::fromLatin1(answer));
        REQUIRE(key);
        const QPoint where = key->mapToScene(QPointF(key->width() / 2, key->height() / 2)).toPoint();
        touch(key);
        CHECK_FALSE(by(u"qualifierSheet"_s));   // found only while shown
        // A late click where the answer was (touch screens make one) presses nothing behind it.
        QTest::mouseClick(s.window, Qt::LeftButton, {}, where);
        QTest::qWait(400);
        QStringList now = chosen();
        now.removeIf([](const QString &n) { return n.endsWith(u"Tomato"_s); });
        CHECK(now == before);
        // Back to as it came, for the next answer.
        REQUIRE(by(u"option-Tomato"_s));
        if (by(u"option-Tomato"_s)->property("text").toString() != u"Tomato"_s)
            touch(by(u"option-Tomato"_s));
    }
    touch(by(u"option-Tomato"_s));
    REQUIRE(by(u"qualifierSheet"_s));
    const QStringList beforeCancel = chosen();
    QQuickItem *cancel = nullptr;
    for (QQuickItem *k : by(u"qualifierSheet"_s)->findChildren<QQuickItem *>())
        if (k->property("text").toString() == u"Cancel"_s)
            cancel = k;
    touch(cancel);
    CHECK_FALSE(by(u"qualifierSheet"_s));
    QTest::qWait(400);
    CHECK(chosen() == beforeCancel);
}

TEST_CASE("Setup guide: every step fits a phone", "[ui][setup][phone]")
{
    Screen s(false, 412, 915, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.c.openSetup();
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const QRectF screen(QPointF(), s.window->size());
    const auto onScreen = [&](const QString &name) {
        QQuickItem *it = by(name);
        INFO(name.toStdString());
        REQUIRE(it);
        const QRectF r = it->mapRectToScene(QRectF(0, 0, it->width(), it->height()));
        INFO(r.x() << "," << r.y() << " " << r.width() << "x" << r.height());
        CHECK(screen.contains(r));
    };
    const QList<QStringList> fields = {{}, {u"setupStoreName"_s, u"setupReceipt"_s}, {u"setupLogoAdd"_s},
                                       {}, {u"setupFood"_s, u"setupAlcohol"_s},
                                       {u"setupItemName"_s, u"setupItemPrice"_s, u"setupItemFamily"_s, u"setupItemAdd"_s},
                                       {u"setupStaffName"_s, u"setupStaffRole"_s, u"setupStaffPin"_s, u"setupStaffAdd"_s}, {}};
    for (int step = 0; step < fields.size(); ++step) {
        s.shot(qPrintable(u"setup-phone-%1"_s.arg(step)));
        onScreen(u"setupNext"_s);
        onScreen(u"setupLater"_s);
        for (const QString &f : fields[step])
            onScreen(f);
        if (step + 1 < fields.size()) {
            s.tapItem(by(u"setupNext"_s));
            QTest::qWait(80);
        }
    }
}

TEST_CASE("Phone audit: every page on a phone (pictures, small controls)", "[.][phoneaudit]")
{
    Screen s(false, 412, 915, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.setupFinish(true);
    s.c.closeSetup();
    QStringList report;
    QDir pages(QStringLiteral(VTM_SEED_DIR "/pages"));
    for (const QString &file : pages.entryList({u"*.json"_s})) {
        const QString id = file.chopped(5);
        if (!s.c.jumpTo(id))
            continue;
        QTest::qWait(80);
        s.shot(qPrintable(u"phone-"_s + id));
        if (qEnvironmentVariableIsSet("VTM_BARS")) {
            std::function<void(QQuickItem *)> walk = [&](QQuickItem *it) {
                if (it->inherits("QQuickScrollBar") && it->isVisible())
                    qWarning().noquote() << id << "bar" << it->width() << "drawn"
                                         << it->mapRectToScene(QRectF(0, 0, it->width(), 1)).width()
                                         << "scale" << it->property("drawnScale").toDouble() << "phone" << it->property("onPhone").toBool();
                for (QQuickItem *c : it->childItems())
                    walk(c);
            };
            walk(s.window->contentItem());
        }
        QStringList small;
        smallTargets(s.window->contentItem(), s.window, &small);
        small.removeDuplicates();
        for (const QString &x : small)
            report << id + u": "_s + x;
    }
    WARN(report.join(u'\n').toStdString());
}

TEST_CASE("Phones don't edit pages: they're made on a bigger screen", "[ui][phone][edit]")
{
    Screen s(false, 412, 915, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.setupFinish(true);
    s.c.closeSetup();
    REQUIRE(s.c.jumpTo(u"manager"_s));
    CHECK_FALSE(s.c.requestEditMode());
    CHECK_FALSE(s.c.editing());
    CHECK(s.c.statusText().contains(u"bigger screen"_s));
}

TEST_CASE("Handhelds: the screen sleeps; off long enough, it asks for the PIN and the check comes back", "[ui][phone][phonelock]")
{
    Screen s(false, 412, 915, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(s.c.screenStaysOn());
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    s.pos.addItem(u"cobb"_s);
    const qint64 check = s.pos.checkInfo().value(u"id"_s).toLongLong();
    REQUIRE(check != 0);

    // Off for a moment (under the store's 30 seconds): still them, still their check.
    s.c.screenOff();
    s.c.screenOn();
    CHECK(s.pos.loggedIn());
    CHECK(s.pos.hasCheck());

    // The store asks every time.
    s.pos.logout();                                // their check is kept for them
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantMap store = s.pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"phoneLockSeconds"_s] = 0;
    REQUIRE(s.pos.adminSave(u"store"_s, 0, store));
    CHECK(s.pos.phoneLockSeconds() == 0);
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(50);
    CHECK(s.pos.hasCheck());                       // picked up again
    s.c.screenOff();
    s.c.screenOn();
    CHECK_FALSE(s.pos.loggedIn());
    CHECK(s.c.statusText().contains(u"PIN"_s));
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(50);
    CHECK(s.pos.hasCheck());
    CHECK(s.pos.checkInfo().value(u"id"_s).toLongLong() == check);

    // A tablet's screen stays on, and it doesn't lock.
    Screen t(false, 1280, 800);
    CHECK(t.c.screenStaysOn());
    REQUIRE(t.pos.loginWithPin(u"1111"_s));
    t.c.screenOff();
    t.c.screenOn();
    CHECK(t.pos.loggedIn());
}

TEST_CASE("Host stand: a host takes a busy server's table's payment (Store Settings)", "[ui][host][hostpay]")
{
    Screen s;
    core::Employee host;
    host.id = "host-test";
    host.name = "Hana";
    host.role = "host";
    host.pinSalt = app::newSalt();
    host.pinHash = app::hashPin(u"7777"_s, host.pinSalt);
    s.pos.shared()->employees.push_back(host);

    // Sam's table.
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"coffee"_s);
    const qint64 check = s.pos.checkInfo().value(u"id"_s).toLongLong();
    s.pos.logout();

    // Not the store's way yet: a host can't take payments.
    REQUIRE(s.pos.loginWithPin(u"7777"_s));
    CHECK_FALSE(s.pos.can(u"check.settle"_s));
    s.pos.logout();

    // Hosts take payments.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantMap store = s.pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"hostsTakePayments"_s] = true;
    REQUIRE(s.pos.adminSave(u"store"_s, 0, store));
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"7777"_s));
    CHECK(s.pos.can(u"check.settle"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());                   // their bank, for cash
    REQUIRE(s.c.jumpTo(u"seating"_s));
    QTest::qWait(60);
    QQuickItem *pay = Screen::findBy(s.window->contentItem(), "objectName", u"hostPay"_s);
    REQUIRE(pay);                                         // on the host stand
    s.shot("host-pay");
    s.c.payTable(u"T4"_s);
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"settle"_s);
    REQUIRE(s.pos.hasCheck());
    CHECK(s.pos.checkInfo().value(u"id"_s).toLongLong() == check);
    REQUIRE(s.pos.tender(u"cash"_s));                     // the whole balance
    if (s.pos.hasCheck())
        REQUIRE(s.pos.closeCheck());
    CHECK_FALSE(s.pos.shared()->open.contains(check));    // paid and closed
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"seating"_s);                   // back at the host stand
    s.pos.logout();

    // A host who may not (Employees): still can't.
    for (core::Employee &e : s.pos.shared()->employees)
        if (e.id == "host-test")
            e.deny.insert(core::perm::Settle);
    REQUIRE(s.pos.loginWithPin(u"7777"_s));
    CHECK_FALSE(s.pos.can(u"check.settle"_s));
}

TEST_CASE("Phone audit: typing on a phone", "[.][phonekeys]")
{
    Screen s(true, 412, 870, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.shot("pk-0-login");
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.activate(u"takeout"_s);
    QTest::qWait(80);
    QQuickItem *root = s.window->contentItem();
    s.tapItem(Screen::findBy(root, "objectName", u"who-name"_s));
    QTest::qWait(300);
    s.shot("pk-1-name");
    s.tapItem(Screen::findBy(root, "objectName", u"whoSave"_s));
    s.tapItem(Screen::findBy(root, "objectName", u"who-phone"_s));
    QTest::qWait(300);
    s.shot("pk-2-phone");
    s.tapItem(Screen::findBy(root, "objectName", u"whoSave"_s));
    s.pos.releaseCheck();
    REQUIRE(s.c.jumpTo(u"customer"_s));
    QTest::qWait(80);
    QQuickItem *lowest = nullptr;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
        if (!item->isVisible())
            return;
        if ((item->inherits("QQuickTextField") || item->inherits("QQuickTextArea"))
            && (!lowest || item->mapToScene(QPointF(0, 0)).y() > lowest->mapToScene(QPointF(0, 0)).y()))
            lowest = item;
        for (QQuickItem *child : item->childItems())
            walk(child);
    };
    walk(root);
    if (lowest) {
        s.tapItem(lowest);
        QTest::qWait(300);
        s.shot("pk-3-lifted");
    }
}

TEST_CASE("Card reader (simulated): Pay with a card, declined, canceled", "[ui][cards]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    // This screen has a (simulated) card reader.
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, {{u"name"_s, s.pos.terminalName()}, {u"cardReader"_s, u"simulated"_s}}));
    CardReader *reader = s.c.cardReader();
    CHECK(reader->kind() == u"simulated"_s);
    reader->setSimulatedDelay(300);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };

    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.pos.addItem(u"coffee"_s));                       // $2.98
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(50);

    // Declined: cents ending in 05.
    for (const char *k : {"1", "0", "5"})
        s.pos.entryKey(QString::fromLatin1(k));
    s.c.activate(u"tender-credit"_s);
    QTest::qWait(60);
    REQUIRE(by(u"cardWait"_s));
    s.shot("card-1-waiting");
    QTest::qWait(500);
    CHECK_FALSE(by(u"cardWait"_s));
    CHECK(s.pos.payments().isEmpty());
    CHECK(s.c.statusText().contains(u"declined"_s));

    // Canceled.
    s.c.activate(u"tender-credit"_s);
    QTest::qWait(60);
    s.tapItem(by(u"cardCancel"_s));
    QTest::qWait(50);
    CHECK_FALSE(by(u"cardWait"_s));
    CHECK(s.pos.payments().isEmpty());

    // Approved: the whole balance, on the check with the card.
    s.pos.entryKey(u"clear"_s);                                // the $1.05 stays typed after a decline
    s.c.activate(u"tender-credit"_s);
    QTest::qWait(500);
    REQUIRE(s.pos.payments().size() == 1);
    CHECK(s.pos.payments()[0].toMap()[u"name"_s].toString().contains(u"Visa •••• 4242"_s));
    CHECK(s.pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    s.shot("card-2-approved");

    // No reader: Credit Card is typed in as before.
    REQUIRE(s.pos.adminSave(u"terminals"_s, 0, {{u"name"_s, s.pos.terminalName()}, {u"cardReader"_s, u""_s}}));
    CHECK(reader->kind().isEmpty());
}

TEST_CASE("Countertop reader: Pay, the reader waits, a test card is tapped", "[ui][cards][counter]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    test::FakeStripe stripe;
    stripe.install(s.pos);
    s.pos.shared()->settings.stripeSecretKey = "sk_test_x";
    s.pos.shared()->settings.stripeReaders.push_back({"tmr_1", "Front counter", "simulated_wisepos_e"});
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, {{u"name"_s, s.pos.terminalName()}, {u"cardReader"_s, u"counter:tmr_1"_s}}));
    CHECK(s.c.cardReader()->kind() == u"counter"_s);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };

    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.pos.addItem(u"coffee"_s));
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(50);
    s.c.activate(u"tender-credit"_s);
    QTest::qWait(80);
    REQUIRE(by(u"cardWait"_s));
    REQUIRE(by(u"testCardApprove"_s));                      // Stripe test mode
    s.shot("counter-1-waiting");
    s.tapItem(by(u"testCardApprove"_s));
    test::waitFor([&] { return !s.pos.payments().isEmpty(); });
    QTest::qWait(60);
    CHECK_FALSE(by(u"cardWait"_s));
    REQUIRE(s.pos.payments().size() == 1);
    CHECK(s.pos.payments()[0].toMap()[u"name"_s].toString().contains(u"Mastercard •••• 4444"_s));
    s.shot("counter-2-paid");
}

TEST_CASE("Find a Check: show all, a check's history, refund part of it", "[ui][refunds]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    s.pos.shared()->settings.stripeSecretKey = "sk_test_x";
    s.pos.shared()->stripeRefund = [](const QString &, std::int64_t, auto done) { done(u"re_ui"_s, {}); };
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.pos.addItem(u"coffee"_s));
    const qint64 id = s.pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(s.pos.recordCardPayment({{u"reference"_s, u"pi_ui"_s}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
                                     {u"amountCents"_s, 298}, {u"checkId"_s, id}, {u"tenderId"_s, u"credit"_s},
                                     {u"processor"_s, u"stripe"_s}}));
    REQUIRE(s.pos.closeCheck());

    REQUIRE(s.c.jumpTo(u"find-check"_s));
    QTest::qWait(60);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    s.tapItem(by(u"range-0"_s));                           // all time
    s.tapItem(by(u"searchAll"_s));
    for (int i = 0; i < 100 && s.pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    QTest::qWait(60);
    s.tapItem(by(u"found-%1"_s.arg(id)));
    QTest::qWait(60);
    s.shot("find-1-check");
    const qint64 payment = s.pos.checkSearch()[u"selected"_s].toMap()[u"payments"_s].toList()[0].toMap()[u"id"_s].toLongLong();
    QMetaObject::invokeMethod(by(u"checkDetail"_s), "positionViewAtEnd");   // the payments, the history
    QTest::qWait(40);
    s.tapItem(by(u"refund-%1"_s.arg(payment)));
    QTest::qWait(40);
    REQUIRE(by(u"refundSheet"_s));
    QQuickItem *amount = by(u"refundAmount"_s);
    CHECK(amount->property("text") == u"2.98"_s);
    amount->setProperty("text", u"1.00"_s);
    by(u"refundReason"_s)->setProperty("text", u"Wrong item"_s);
    QTest::qWait(30);
    s.shot("find-2-refund");
    s.tapItem(by(u"refundGo"_s));
    QTest::qWait(60);
    CHECK_FALSE(by(u"refundSheet"_s));
    REQUIRE(s.pos.shared()->refundsToday.size() == 1);
    CHECK(s.pos.shared()->refundsToday[0].amount.cents() == 100);
    QTest::qWait(60);
    s.shot("find-3-refunded");
}

TEST_CASE("UI: a handheld's receipt: choose the printer, or no receipt", "[flow][ui][receipts]")
{
    Screen s(false, 540, 960, u"phone"_s);
    struct Printed : app::PosPrinter {
        std::vector<std::string> printers;
        void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
        void printReceipt(const core::PosSettings &, const core::Check &, const std::string &p) override { printers.push_back(p); }
        void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
        void openDrawer(const core::PosSettings &, const std::string &) override {}
    } printed;
    auto &settings = s.pos.shared()->settings;
    core::PrinterConfig host;
    host.id = "host";
    host.name = "Host Stand";
    host.type = "file";
    host.receipts = true;
    settings.printers.push_back(host);
    core::TerminalConfig t;
    t.name = s.pos.terminalName().toStdString();
    t.receiptPrinter = "ask";
    t.afterPaying = "ask";
    settings.terminals.push_back(t);
    s.pos.shared()->printer = &printed;

    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.pos.addItem(u"coffee"_s));
    REQUIRE(s.pos.tender(u"cash"_s));
    REQUIRE(s.pos.closeCheck());
    QTest::qWait(50);
    auto *sheet = s.window->findChild<QQuickItem *>(u"receiptSheet"_s);
    REQUIRE(sheet);
    CHECK(sheet->isVisible());
    s.shot("receipt-handheld");
    s.tapKey(u"Host Stand"_s);
    CHECK(printed.printers == std::vector<std::string>{"host"});
    CHECK_FALSE(sheet->isVisible());

    REQUIRE(s.pos.printReceipt());   // the last check again: asked where
    QTest::qWait(30);
    CHECK(sheet->isVisible());
    s.tapKey(u"No Receipt"_s);
    CHECK_FALSE(sheet->isVisible());
    CHECK(printed.printers.size() == 1);
}

namespace {

// A visible item of a class (a QML type) whose `text` is `value`, searched
// from the window's root (popups and menus included).
QQuickItem *findTyped(QQuickItem *root, const char *cls, const QString &value)
{
    for (QQuickItem *item : root->childItems()) {
        if (!item->isVisible())
            continue;
        if (QByteArray(item->metaObject()->className()).contains(cls) && item->property("text").toString() == value)
            return item;
        if (QQuickItem *hit = findTyped(item, cls, value))
            return hit;
    }
    return nullptr;
}

// Scrolls the nearest Flickable so `item` is in view.
void scrollTo(QQuickItem *item)
{
    for (QQuickItem *p = item->parentItem(); p; p = p->parentItem()) {
        if (QByteArray(p->metaObject()->className()).contains("Flickable")) {
            const qreal y = item->mapToItem(p->property("contentItem").value<QQuickItem *>(), QPointF(0, 0)).y();
            p->setProperty("contentY", std::max<qreal>(0, y - p->height() / 2));
            QTest::qWait(60);
            return;
        }
    }
}

} // namespace

TEST_CASE("UI editor: + Add action shows each new action, the menu opens under its button", "[flow][ui][editorui]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    REQUIRE(e);
    const QString mine = e->addZone(u"button"_s);
    e->selectOnly({mine});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();   // menus and popups are in its overlay
    const QStringList picks = {u"Command"_s, u"Go to page"_s, u"Qualifier"_s};
    for (int round = 0; round < 3; ++round) {
        INFO("round " << round);
        QQuickItem *add = findTyped(root, "Button", u"+ Add action"_s);
        REQUIRE(add);
        scrollTo(add);
        s.tapItem(add);
        QTest::qWait(150);
        QQuickItem *item = findTyped(root, "MenuItem", picks[round]);
        REQUIRE(item);
        // Under the button, not in a corner.
        const QPointF b = add->mapToScene(QPointF(0, add->height()));
        const QPointF m = item->mapToScene(QPointF(0, 0));
        CHECK(std::abs(m.x() - b.x()) < 40);
        CHECK(m.y() >= b.y() - 4);
        s.tapItem(item);
        QTest::qWait(150);
        REQUIRE(e->actions().size() == round + 1);
        CHECK(findTyped(root, "Label", QString::number(round + 1) + u"."_s));   // its card
        CHECK_FALSE(findTyped(root, "Label", u"Nothing happens when this is touched."_s));
    }
    QStringList types;
    for (const QVariant &a : e->actions())
        types << a.toMap()[u"type"_s].toString();
    CHECK(types == QStringList{u"command"_s, u"jump"_s, u"qualifier"_s});
    s.shot("ed-actions");

    // Removing one: the cards follow.
    QQuickItem *remove = findTyped(root, "ToolButton", u"✕"_s);
    REQUIRE(remove);
    s.tapItem(remove);
    QTest::qWait(120);
    CHECK(e->actions().size() == 2);
    CHECK_FALSE(findTyped(root, "Label", u"3."_s));

    // Undo brings it back, in the panel too.
    e->undo();
    QTest::qWait(120);
    CHECK(e->actions().size() == 3);
    CHECK(findTyped(root, "Label", u"3."_s));
}

TEST_CASE("UI editor: the inspector shows a change made elsewhere", "[flow][ui][editorui]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const QString mine = e->addZone(u"button"_s);
    e->selectOnly({mine});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();   // menus and popups are in its overlay
    REQUIRE(e->setField(u"zone"_s, u"label"_s, u"Happy Hour"_s));
    QTest::qWait(120);
    CHECK(Screen::findBy(root, "text", u"Happy Hour"_s));       // the Text field and the button
    e->undo();
    QTest::qWait(120);
    CHECK_FALSE(Screen::findBy(root, "text", u"Happy Hour"_s));
}

TEST_CASE("UI editor: holding a zone opens its menu (a touchscreen has no right button)", "[flow][ui][editorui]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const QString mine = e->addZone(u"button"_s);
    e->selectOnly({mine});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();   // menus and popups are in its overlay
    QQuickItem *zone = Screen::findBy(root, "text", u"New Button"_s);
    REQUIRE(zone);
    const QPoint at = zone->mapToScene(QPointF(zone->width() / 2, zone->height() / 2)).toPoint();
    QTest::mousePress(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(1200);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, at);
    QTest::qWait(150);
    QQuickItem *copy = findTyped(root, "MenuItem", u"Copy"_s);
    REQUIRE(copy);
    const QPointF m = copy->mapToScene(QPointF(0, 0));
    CHECK(std::abs(m.x() - at.x()) < 60);
    CHECK(std::abs(m.y() - at.y()) < 60);
    CHECK(e->selection() == QStringList{mine});
}

TEST_CASE("UI editor: typing in the inspector edits the button; undo shows in the field", "[flow][ui][editorui]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const QString mine = e->addZone(u"button"_s);
    e->selectOnly({mine});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    // The Text box (the canvas's button has that text too).
    std::function<QQuickItem *(QQuickItem *)> textBox = [&](QQuickItem *it) -> QQuickItem * {
        for (QQuickItem *c : it->childItems()) {
            const QByteArray cls = c->metaObject()->className();
            if (c->isVisible() && (cls.contains("TextArea") || cls.contains("TextField"))
                && c->property("text").toString() == u"New Button"_s)
                return c;
            if (QQuickItem *hit = textBox(c))
                return hit;
        }
        return nullptr;
    };
    QQuickItem *box = textBox(root);
    REQUIRE(box);
    const auto type = [&](const char *text) {
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    };
    s.tapItem(box);
    QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
    type("Happy Hour");
    box->setFocus(false);   // leaving the box saves it
    QTest::qWait(120);
    CHECK(e->fieldInfo(u"zone"_s, u"label"_s)[u"value"_s].toString() == u"Happy Hour"_s);
    CHECK(textBox(root) == nullptr);   // the button on the canvas says it now
    CHECK(box->property("text").toString() == u"Happy Hour"_s);
    // Undo shows in the field.
    e->undo();
    QTest::qWait(120);
    CHECK(box->property("text").toString() == u"New Button"_s);
}

TEST_CASE("UI editor: toolbar menus open under their buttons; the toolbar fits a small screen", "[flow][ui][editorui]")
{
    Screen s(false, 1024, 600);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    e->selectOnly({e->addZone(u"button"_s)});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    s.shot("ed-1024");
    // Save and Done are always on screen.
    for (const QString &t : {u"Save"_s, u"Done"_s}) {
        QQuickItem *k = findTyped(root, "Button", t);
        REQUIRE(k);
        CHECK(k->mapToScene(QPointF(k->width(), 0)).x() <= s.window->width());
    }
    for (const auto &[button, item] : {std::pair{u"+ Panel ▾"_s, u"Order list"_s}, std::pair{u"Arrange ▾"_s, u"Bring to front"_s},
                                       std::pair{u"File ▾"_s, u"Export this page…"_s}}) {
        INFO(button.toStdString());
        QQuickItem *b = Screen::findBy(root, "text", button);
        REQUIRE(b);
        // Swiped into view when the screen is narrow.
        auto *tools = Screen::findBy(root, "objectName", u"editorTools"_s);
        REQUIRE(tools);
        const qreal x = b->mapToItem(tools->property("contentItem").value<QQuickItem *>(), QPointF(0, 0)).x();
        const qreal most = std::max<qreal>(0, tools->property("contentWidth").toReal() - tools->width());
        tools->setProperty("contentX", std::clamp<qreal>(x - tools->width() / 2, 0, most));
        QTest::qWait(60);
        CHECK(b->mapToScene(QPointF(b->width(), 0)).x() <= s.window->width());   // on screen
        s.tapItem(b);
        QTest::qWait(150);
        QQuickItem *m = findTyped(root, "MenuItem", item);
        REQUIRE(m);
        const QPointF under = b->mapToScene(QPointF(0, b->height()));
        CHECK(m->mapToScene(QPointF(0, 0)).y() >= under.y() - 4);
        CHECK(std::abs(m->mapToScene(QPointF(0, 0)).x() - under.x()) < 120);
        QTest::keyClick(s.window, Qt::Key_Escape);
        QTest::qWait(80);
    }
}

TEST_CASE("UI editor: Standard | Phone edits either screen's version; buttons stay shared", "[flow][ui][editorui][phoneVersion]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    QQuickItem *root = s.window->contentItem();
    const auto tap = [&](const QString &name) {
        QQuickItem *it = Screen::findBy(root, "objectName", name);
        REQUIRE(it);
        REQUIRE(it->isVisible());
        s.tapItem(it);
        QTest::qWait(150);
    };

    REQUIRE(s.c.showPage(u"settle"_s));
    QTest::qWait(150);
    tap(u"screenPhone"_s);
    CHECK(e->pageId() == u"settle-phone"_s);
    e->selectOnly({u"tender-cash"_s});
    QTest::qWait(150);
    QQuickItem *box = Screen::findBy(root, "objectName", u"phoneShareBox"_s);
    REQUIRE(box);
    CHECK(box->isVisible());
    s.shot("phone-version-edit");
    tap(u"ownLabel"_s);
    CHECK(e->phoneShare()[u"own"_s].toStringList() == QStringList{u"label"_s});

    // Where the phone differs from the page: Split's own words; the same again.
    tap(u"phoneMenuButton"_s);
    s.shot("phone-version-differences");
    tap(u"followPage-split"_s);
    CHECK(e->layout().page(u"settle-phone"_s)->zone(u"split"_s)->label == u"Split"_s);
    QQuickItem *screens = Screen::findBy(root, "objectName", u"phoneScreens"_s);
    REQUIRE(screens);
    QMetaObject::invokeMethod(screens, "closeTray");
    QTest::qWait(150);
    tap(u"screenStandard"_s);
    CHECK(e->pageId() == u"settle"_s);

    // Show on: standard screens only.
    e->selectOnly({u"coupon"_s});
    QTest::qWait(150);
    tap(u"showOn-standard"_s);
    CHECK(e->fieldInfo(u"zone"_s, u"showWhen.screen"_s)[u"value"_s].toString() == u"standard"_s);
    tap(u"showOn-both"_s);
    CHECK(e->fieldInfo(u"zone"_s, u"showWhen.screen"_s)[u"value"_s].toString().isEmpty());

    // A page without one: made from what phones show, then back to automatic.
    REQUIRE(s.c.showPage(u"drawer"_s));
    QTest::qWait(150);
    tap(u"screenPhone"_s);
    tap(u"makePhoneVersion"_s);
    CHECK(e->pageId() == u"drawer-phone"_s);
    tap(u"phoneMenuButton"_s);
    s.shot("phone-version-tray");
    tap(u"removePhoneVersion"_s);
    QQuickItem *yes = findTyped(root, "Button", u"Yes"_s);
    REQUIRE(yes);
    s.tapItem(yes);
    QTest::qWait(150);
    CHECK(e->pageId() == u"drawer"_s);
    CHECK(!e->layout().variantFor(u"drawer"_s, u"phone"_s));
}

TEST_CASE("UI: Count Drawer counts bill by bill on the keypad", "[flow][ui][drawerCount]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.c.jumpTo(u"drawer"_s));
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    const auto tap = [&](const QString &name) {
        QQuickItem *it = Screen::findBy(root, "objectName", name);
        REQUIRE(it);
        REQUIRE(it->isVisible());
        s.tapItem(it);
        QTest::qWait(60);
        INFO(name.toStdString());
        CHECK(s.pos.entry().isEmpty());   // not the keypad on the page behind
    };
    QQuickItem *count = Screen::findBy(root, "text", u"Count Drawer"_s);
    REQUIRE(count);
    s.tapItem(count);
    QTest::qWait(150);
    QQuickItem *sheet = Screen::findBy(root, "objectName", u"drawerCount"_s);
    REQUIRE(sheet);
    CHECK(sheet->isVisible());

    // 4 × $20, 2 × $10, then 3 quarters.
    tap(u"countRow-2000"_s);
    tap(u"countKey-4"_s);
    tap(u"countKey-next"_s);
    tap(u"countKey-2"_s);
    tap(u"countRow-25"_s);
    tap(u"countKey-3"_s);
    QQuickItem *total = Screen::findBy(root, "objectName", u"countTotal"_s);
    REQUIRE(total);
    CHECK(total->property("text").toString() == u"$100.75"_s);
    s.shot("drawer-count");
    tap(u"countDone"_s);
    CHECK_FALSE(sheet->isVisible());
    const QVariantMap d = s.pos.drawerInfo();
    CHECK_FALSE(d[u"open"_s].toBool());
    CHECK(d[u"counted"_s].toString() == u"$100.75"_s);
    CHECK(d[u"countedPieces"_s].toList().size() == 3);
    QTest::qWait(100);
    s.shot("drawer-counted");
}

TEST_CASE("UI: counting a drawer on a phone", "[flow][ui][drawerCount][phone]")
{
    Screen s(false, 412, 892, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.c.jumpTo(u"drawer"_s));
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    QQuickItem *count = Screen::findBy(root, "text", u"Count Drawer"_s);
    REQUIRE(count);
    s.tapItem(count);
    QTest::qWait(150);
    QQuickItem *key = Screen::findBy(root, "objectName", u"countKey-7"_s);
    REQUIRE(key);
    s.tapItem(key);
    QTest::qWait(60);
    QQuickItem *done = Screen::findBy(root, "objectName", u"countDone"_s);
    REQUIRE(done);
    CHECK(QRectF(0, 0, 412, 892).contains(done->mapRectToScene(QRectF(0, 0, done->width(), done->height()))));
    CHECK(key->height() >= 44);
    s.shot("drawer-count-phone");
    s.tapItem(done);
    QTest::qWait(60);
    CHECK(s.pos.drawerInfo()[u"counted"_s].toString() == u"$700.00"_s);
}

TEST_CASE("UI editor: a page made now is offered to Go to page right away", "[flow][ui][editorui]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    const QString button = e->addZone(u"button"_s);
    e->selectOnly({button});
    e->setActions({QVariantMap{{u"type"_s, u"jump"_s}, {u"mode"_s, u"push"_s}}});
    QTest::qWait(150);
    QQuickItem *root = s.window->contentItem();
    const auto offered = [&](const QString &name) {
        std::function<bool(QQuickItem *)> look = [&](QQuickItem *it) {
            for (QQuickItem *c : it->childItems()) {
                if (c->isVisible() && QByteArray(c->metaObject()->className()).contains("ComboBox")) {
                    const QVariant model = c->property("model");
                    for (const QVariant &o : model.toList())
                        if (o.toMap()[u"text"_s].toString().startsWith(name))
                            return true;
                }
                if (look(c))
                    return true;
            }
            return false;
        };
        return look(root);
    };
    CHECK_FALSE(offered(u"Happy Hour"_s));
    const QString page = e->newPage(u"Happy Hour"_s, u"custom"_s, {});
    REQUIRE_FALSE(page.isEmpty());
    REQUIRE(s.c.jumpTo(u"tables"_s));   // back to the button
    QTest::qWait(100);
    e->selectOnly({button});
    QTest::qWait(150);
    CHECK(offered(u"Happy Hour"_s));
}

// A binding that refreshes when a value changes must use the value: a read
// that is thrown away ("x.revision", "void x", "(x, y)") is compiled out
// of compiled QML, and the screen then never refreshes.
TEST_CASE("Screens never depend on a value they throw away", "[ui][lint]")
{
    const QDir qml(QStringLiteral(VTM_SEED_DIR) + u"/../ui/qml"_s);
    REQUIRE(qml.exists());
    const QRegularExpression bare(uR"(^\s*[A-Za-z_]\w*(\.\w+)+;?\s*$)"_s);
    const QRegularExpression voided(uR"(\bvoid [A-Za-z_])"_s);
    const QRegularExpression comma(uR"(\([A-Za-z_][\w.]*(Revision|revision|tick)\w*, )"_s);
    // "{ pos ? pos.openChecks : null; return ..." or a line of its own.
    const QRegularExpression ternary(uR"((^|\{)\s*[A-Za-z_][\w.]*\s*\?\s*[\w.]+\s*:\s*(null|undefined|\[\]|\{\})\s*(;|$))"_s);
    int files = 0;
    for (const QString &name : qml.entryList({u"*.qml"_s})) {
        QFile f(qml.filePath(name));
        REQUIRE(f.open(QIODevice::ReadOnly));
        ++files;
        int n = 0;
        for (const QString &line : QString::fromUtf8(f.readAll()).split(u'\n')) {
            ++n;
            const QString code = line.section(u"//"_s, 0, 0);
            INFO(name.toStdString() << ":" << n << ": " << line.trimmed().toStdString());
            CHECK_FALSE(bare.match(code).hasMatch());
            CHECK_FALSE(voided.match(code).hasMatch());
            CHECK_FALSE(comma.match(code).hasMatch());
            CHECK_FALSE(ternary.match(code).hasMatch());
        }
    }
    CHECK(files > 50);
}

TEST_CASE("UI: the kiosk's own keyboard types a name with a space", "[flow][ui][kiosk][kioskkeys]")
{
    for (const QSize size : {QSize(1600, 900), QSize(1080, 1920)}) {
        INFO(size.width() << "x" << size.height());
        Screen s(false, size.width(), size.height());
        REQUIRE(s.pos.loginWithPin(u"1234"_s));
        REQUIRE(s.c.jumpTo(u"tables"_s));   // the store's tables behind the kiosk
        s.pos.enableSelfOrder();
        QTest::qWait(60);
        auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
        QQuickItem *kiosk = find(u"selfOrder"_s);
        REQUIRE(kiosk);
        s.tapItem(find(u"kioskAttract"_s));
        QTest::qWait(60);
        s.tapItem(find(u"kioskForHere"_s));
        QTest::qWait(60);
        s.tapItem(Screen::findBy(kiosk, "text", u"Cheeseburger"_s));
        QTest::qWait(60);
        s.tapItem(Screen::findBy(kiosk, "text", u"Well Done"_s));
        s.tapItem(Screen::findBy(kiosk, "text", u"Fries"_s));
        s.tapItem(find(u"kioskChoicesDone"_s));
        QTest::qWait(60);
        REQUIRE(s.pos.lines().size() == 1);
        s.tapItem(find(u"kioskReview"_s));
        QTest::qWait(80);
        QQuickItem *name = find(u"kioskName"_s);
        REQUIRE(name);
        REQUIRE(name->isVisible());
        for (const QString &key : {u"L"_s, u"e"_s, u"e"_s, u"space"_s, u"k"_s}) {
            INFO(key.toStdString());
            QQuickItem *k = Screen::findBy(kiosk, "text", key);
            REQUIRE(k);
            s.tapItem(k);
            QTest::qWait(40);
            CHECK(s.pos.selfOrderInfo()[u"ordering"_s].toBool());   // the order is still there
            CHECK(name->isVisible());                                 // still on the name
        }
        CHECK(name->property("text").toString() == u"Lee k"_s);
        CHECK(s.pos.lines().size() == 1);
    }
}

TEST_CASE("UI: a touch on a sheet or the screen saver never presses the page behind it", "[flow][ui][kiosk][kioskkeys]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(60);
    QQuickItem *page = Screen::findBy(s.window->contentItem(), "objectName", u"pageSurface"_s);
    REQUIRE(page);
    CHECK(page->isEnabled());
    s.pos.enableSelfOrder();
    QTest::qWait(60);
    CHECK_FALSE(page->isEnabled());   // under the kiosk
}

TEST_CASE("UI: the pop-up keyboard's keys never press the page behind them", "[flow][ui][kioskkeys][popkeys]")
{
    Screen s(true);   // touch keyboard on
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"find-check"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    std::function<QQuickItem *(QQuickItem *)> field = [&](QQuickItem *it) -> QQuickItem * {
        for (QQuickItem *c : it->childItems()) {
            if (c->isVisible() && QByteArray(c->metaObject()->className()).contains("TextField"))
                return c;
            if (QQuickItem *hit = field(c))
                return hit;
        }
        return nullptr;
    };
    QQuickItem *box = field(root);
    REQUIRE(box);
    s.tapItem(box);
    QTest::qWait(100);
    QQuickItem *kb = nullptr;
    std::function<void(QQuickItem *)> findKb = [&](QQuickItem *it) {
        for (QQuickItem *c : it->childItems()) {
            if (QByteArray(c->metaObject()->className()).contains("TouchKeyboard"))
                kb = c;
            findKb(c);
        }
    };
    findKb(root);
    REQUIRE(kb);
    REQUIRE(kb->isVisible());
    // The keys of the current layout (read again each time: the rows are rebuilt).
    const auto keys = [&] {
        std::vector<std::pair<QString, QPointF>> out;
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *it) {
            for (QQuickItem *c : it->childItems()) {
                if (c->isVisible() && c->parentItem() && QByteArray(c->parentItem()->metaObject()->className()).contains("RowLayout")) {
                    QString label;
                    for (QQuickItem *t : c->childItems())
                        label += t->property("text").toString();
                    out.emplace_back(label, c->mapToScene(QPointF(c->width() / 2, c->height() / 2)));
                }
                walk(c);
            }
        };
        walk(kb);
        return out;
    };
    const QSet<QString> modes = {u"⇧"_s, u"⌫"_s, u"⏎"_s, u"⌨▾"_s, u"?123"_s, u"abc"_s};
    int pressed = 0;
    for (std::size_t i = 0; i < keys().size(); ++i) {
        const auto [label, at] = keys()[i];
        if (label.isEmpty() || modes.contains(label))
            continue;   // a row's Repeater, or a key that changes the layout
        INFO("key '" << label.toStdString() << "' at " << at.x() << "," << at.y());
        QTest::mouseClick(s.window, Qt::LeftButton, {}, at.toPoint());
        QTest::qWait(20);
        ++pressed;
        CHECK(s.c.pageId() == u"find-check"_s);
        REQUIRE(kb->isVisible());
    }
    CHECK(pressed > 25);
    CHECK(box->property("text").toString().size() == pressed);   // every key typed into the field
}

// Qt offers a touch to the tap handlers under whatever took it: every page
// control's tap handler ignores a press on the pop-up keyboard (TouchGuard).
TEST_CASE("Every tap handler on a page ignores the keyboard's touches", "[ui][lint]")
{
    const QDir qml(QStringLiteral(VTM_SEED_DIR) + u"/../ui/qml"_s);
    QStringList files = qml.entryList({u"Widget*.qml"_s});
    files << u"ZoneItem.qml"_s << u"FloorPlanView.qml"_s;
    const QRegularExpression handler(uR"(\bon(Tapped|LongPressed|DoubleTapped)\s*:)"_s);
    int handlers = 0;
    for (const QString &name : files) {
        QFile f(qml.filePath(name));
        REQUIRE(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        const int n = int(text.count(handler));
        handlers += n;
        INFO(name.toStdString());
        CHECK(int(text.count(u"TouchGuard.covered("_s)) >= n);
    }
    CHECK(handlers >= 18);
}

TEST_CASE("UI: after the kiosk is used, staff can open its orders and they stay", "[flow][ui][kiosk][kioskstaff]")
{
    Screen s;
    s.pos.shared()->settings.kioskIdleSeconds = 1;   // the "still there?" wait, short for the test
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.enableSelfOrder();
    QTest::qWait(60);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *kiosk = find(u"selfOrder"_s);
    s.tapItem(find(u"kioskAttract"_s));
    QTest::qWait(60);
    s.tapItem(find(u"kioskForHere"_s));
    QTest::qWait(60);
    s.tapItem(Screen::findBy(kiosk, "text", u"Cheeseburger"_s));
    QTest::qWait(60);
    s.tapItem(Screen::findBy(kiosk, "text", u"Well Done"_s));
    s.tapItem(Screen::findBy(kiosk, "text", u"Fries"_s));
    s.tapItem(find(u"kioskChoicesDone"_s));
    QTest::qWait(60);
    const qint64 id = s.pos.checkInfo()[u"id"_s].toLongLong();
    s.tapItem(find(u"kioskReview"_s));
    QTest::qWait(60);
    for (const char ch : {'L', 'e', 'e'})
        QTest::keyClick(s.window, ch);
    s.tapItem(find(u"kioskPlace"_s));
    QTest::qWait(100);
    // A manager ends kiosk mode: hold the corner, a PIN.
    QTest::mousePress(s.window, Qt::LeftButton, {}, QPoint(10, 10));
    QTest::qWait(3300);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, QPoint(10, 10));
    QTest::qWait(50);
    for (const char *key : {"approvalKey-1", "approvalKey-2", "approvalKey-3", "approvalKey-4", "approvalKey-OK"})
        s.tapItem(find(QString::fromLatin1(key)));
    QTest::qWait(100);
    REQUIRE_FALSE(s.pos.selfOrderInfo()[u"on"_s].toBool());
    // A server opens the order, and keeps it open past every kiosk timer.
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    s.c.openCheck(id);
    QTest::qWait(100);
    REQUIRE(s.pos.hasCheck());
    QTest::qWait(19000);   // the 1 s wait, the 15 s countdown, more
    REQUIRE(s.pos.shared()->open.contains(id));
    CHECK(s.pos.shared()->open.at(id).lines.size() == 1);
    CHECK(s.pos.hasCheck());
}

TEST_CASE("UI: Open Checks shows yours; All Checks shows everyone's, others' locked", "[flow][ui][whose]")
{
    Screen s;
    const auto start = [&](const char *pin) {
        REQUIRE(s.pos.loginWithPin(QString::fromLatin1(pin)));
        REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
        REQUIRE(s.pos.addItem(u"coffee"_s));
        const QString label = s.pos.checkInfo()[u"label"_s].toString();
        s.pos.releaseCheck();
        s.pos.logout();
        return label;
    };
    const QString sams = start("1111");
    const QString rosas = start("5555");
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    s.c.activate(u"checks"_s);
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    CHECK(Screen::findBy(root, "text", sams));
    CHECK_FALSE(Screen::findBy(root, "text", rosas));        // not his
    s.tapItem(Screen::findBy(root, "objectName", u"allChecks"_s));
    QTest::qWait(100);
    QQuickItem *hers = Screen::findBy(root, "text", rosas);
    REQUIRE(hers);
    CHECK(Screen::findBy(root, "text", u"🔒"_s));             // locked for him
    s.shot("open-checks-all");
    s.tapItem(hers);                                          // a manager's PIN is asked
    QTest::qWait(100);
    CHECK(s.pos.approvalInfo()[u"needed"_s].toBool());
    CHECK_FALSE(s.pos.hasCheck());
}

TEST_CASE("UI: a printer out of paper shows on every staff screen, not on the kiosk", "[flow][ui][status]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    QTest::qWait(60);
    QQuickItem *root = s.window->contentItem();
    QQuickItem *banner = Screen::findBy(root, "objectName", u"printerAlerts"_s);
    CHECK_FALSE(banner);   // nothing wrong: not shown
    s.pos.shared()->setPrinterProblem("kitchen", "paperOut");
    QTest::qWait(60);
    REQUIRE(Screen::findBy(root, "objectName", u"printerAlerts"_s));
    CHECK(Screen::findBy(root, "text", u"🖨  Kitchen: out of paper"_s));
    s.shot("printer-alert");
    s.pos.enableSelfOrder();   // guests don't need to know
    QTest::qWait(60);
    CHECK_FALSE(Screen::findBy(root, "objectName", u"printerAlerts"_s));
}

TEST_CASE("UI: on the kiosk, a guest has no onion and mayo on the side", "[flow][ui][kiosk][how]")
{
    Screen s(false, 1080, 1920);
    s.pos.enableSelfOrder();
    QTest::qWait(60);
    auto find = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *kiosk = find(u"selfOrder"_s);
    s.tapItem(find(u"kioskAttract"_s));
    QTest::qWait(60);
    s.tapItem(find(u"kioskForHere"_s));
    QTest::qWait(60);
    s.tapItem(Screen::findBy(kiosk, "text", u"Classic Burger"_s));
    QTest::qWait(100);
    REQUIRE(find(u"kioskHow-Onion-no"_s));
    s.tapItem(find(u"kioskHow-Onion-no"_s));
    s.tapItem(find(u"kioskHow-Mayo-side"_s));
    QTest::qWait(60);
    s.shot("kiosk-how");
    const QVariantList mods = s.pos.lines()[0].toMap()[u"modifiers"_s].toList();
    QStringList names;
    for (const QVariant &m : mods)
        names << m.toString() + m.toMap()[u"name"_s].toString();
    CHECK(names.join(u' ').contains(u"No Onion"_s));
    CHECK(names.join(u' ').contains(u"Side of Mayo"_s));
}

TEST_CASE("UI: choosing No Onion on the Choose page keeps the page where it was scrolled", "[flow][ui][choosescroll]")
{
    Screen s(false, 1024, 600);
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Takeout));
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    s.c.orderItem(u"classic-burger"_s);   // its choices open
    QTest::qWait(150);
    QQuickItem *picker = nullptr;
    std::function<void(QQuickItem *)> findPicker = [&](QQuickItem *it) {
        for (QQuickItem *c : it->childItems()) {
            if (c->isVisible() && QByteArray(c->metaObject()->className()).contains("WidgetModifierPicker"))
                picker = c;
            findPicker(c);
        }
    };
    findPicker(s.window->contentItem());
    REQUIRE(picker);
    QQuickItem *flick = nullptr;
    for (QQuickItem *c : picker->childItems())
        if (QByteArray(c->metaObject()->className()).contains("Flickable"))
            flick = c;
    REQUIRE(flick);
    const qreal most = flick->property("contentHeight").toReal() - flick->height();
    REQUIRE(most > 50);   // it scrolls here
    flick->setProperty("contentY", most);
    QTest::qWait(50);
    // The No key, then Onion (as the screen does it).
    int onion = -1;
    for (const QVariant &g : s.pos.choosingInfo()[u"groups"_s].toList())
        if (g.toMap()[u"id"_s] == u"toppings"_s)
            for (const QVariant &o : g.toMap()[u"options"_s].toList())
                if (o.toMap()[u"name"_s] == u"Onion"_s)
                    onion = o.toMap()[u"index"_s].toInt();
    REQUIRE(onion >= 0);
    QQuickItem *key = Screen::findBy(picker, "objectName", u"option-Onion"_s);
    REQUIRE(key);
    QPointer<QQuickItem> before = picker, beforeKey = key;
    const QString page = s.c.pageId();
    s.pos.invoke(u"chooseOptionAs"_s, {u"toppings"_s, onion, u"no"_s});
    QTest::qWait(100);
    REQUIRE(before);       // the page's panels weren't rebuilt
    REQUIRE(beforeKey);
    CHECK(s.c.pageId() == page);
    CHECK(flick->property("contentY").toReal() == Catch::Approx(most));   // still down there
    CHECK(key == Screen::findBy(picker, "objectName", u"option-Onion"_s));   // the same button, updated
    CHECK(key->property("text").toString().startsWith(u"No Onion"_s));
}

TEST_CASE("UI: a new item shows up on the menu screen by itself", "[flow][ui][menubuild]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.adminSave(u"menu"_s, -1, {{u"name"_s, u"Fish Tacos"_s}, {u"price"_s, 13.5}, {u"family"_s, u"plates"_s},
                                             {u"taxClass"_s, u"food"_s}, {u"printer"_s, u"kitchen"_s}, {u"available"_s, true}}));
    s.c.setMealPeriod(u"lunch"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.c.jumpTo(u"index-lunch"_s));
    QTest::qWait(60);
    // Lunch's categories, in the Menu Builder's order; breakfast's aren't there.
    QQuickItem *root = s.window->contentItem();
    CHECK(Screen::findBy(root, "objectName", u"category-burgers"_s));
    CHECK(Screen::findBy(root, "objectName", u"category-plates"_s));
    CHECK_FALSE(Screen::findBy(root, "objectName", u"category-breakfast"_s));
    s.shot("menu-categories");
    s.tapItem(Screen::findBy(root, "objectName", u"category-plates"_s));
    QTest::qWait(60);
    CHECK(s.c.pageId() == u"menu-all"_s);
    QQuickItem *tacos = nullptr;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Fish Tacos"_s)
            tacos = Screen::findBy(root, "objectName", u"menuItem-"_s + m.toMap()[u"id"_s].toString());
    REQUIRE(tacos);   // on the screen with no page editing
    s.shot("menu-plates");
    s.tapItem(tacos);
    QTest::qWait(60);
    REQUIRE(s.pos.lines().size() == 1);
    CHECK(s.pos.lines()[0].toMap()[u"name"_s].toString() == u"Fish Tacos"_s);
}

TEST_CASE("UI: the Menu Builder adds an item, with what's on it, and a category", "[flow][ui][menubuild]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto type = [&](const QString &field, const char *text) {
        QQuickItem *f = by(field);
        REQUIRE(f);
        scrollTo(f);
        s.tapItem(f);
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    };
    REQUIRE(by(u"builderCategory-burgers"_s));
    s.tapItem(by(u"builderCategory-burgers"_s));
    QTest::qWait(60);
    REQUIRE(by(u"builderItem-classic-burger"_s));
    s.tapItem(by(u"builderAddItem"_s));
    QTest::qWait(60);
    type(u"builderName"_s, "Patty Melt");
    type(u"builderPrice"_s, "12.75");
    type(u"builderOnIt"_s, "rye, grilled onion, swiss");
    scrollTo(by(u"builderGroup-temperature"_s));   // down the card
    s.tapItem(by(u"builderGroup-temperature"_s));
    s.shot("builder-new-item");
    s.tapItem(by(u"builderSave"_s));
    QTest::qWait(150);
    QVariantMap melt;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Patty Melt"_s)
            melt = m.toMap();
    REQUIRE_FALSE(melt.isEmpty());
    CHECK(melt[u"price"_s] == u"$12.75"_s);
    CHECK(melt[u"family"_s] == u"burgers"_s);
    CHECK(melt[u"station"_s] == u"grill"_s);   // the Burgers category's
    CHECK(melt[u"onIt"_s].toStringList() == QStringList{u"rye"_s, u"grilled onion"_s, u"swiss"_s});
    CHECK(melt[u"groups"_s].toStringList() == QStringList{u"temperature"_s});
    CHECK(by(u"builderItem-"_s + melt[u"id"_s].toString()));   // its tile, and its card open

    // A new category.
    s.tapItem(by(u"builderAddCategory"_s));
    QTest::qWait(60);
    type(u"builderCategoryName"_s, "Tacos");
    s.tapItem(by(u"builderSaveCategory"_s));
    QTest::qWait(100);
    CHECK(by(u"builderCategory-tacos"_s));
    CHECK(by(u"builderAddItem"_s));   // Tacos is the one shown, ready for its items
    s.shot("builder");
}

TEST_CASE("UI: the Menu Builder makes a choice group, row by row", "[flow][ui][menubuild]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto type = [&](const QString &field, const char *text) {
        QQuickItem *f = by(field);
        REQUIRE(f);
        scrollTo(f);
        s.tapItem(f);
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    };
    s.tapItem(by(u"builderModeChoices"_s));
    QTest::qWait(60);
    REQUIRE(by(u"builderGroupRow-temperature"_s));
    s.tapItem(by(u"builderAddGroup"_s));
    QTest::qWait(60);
    type(u"builderGroupName"_s, "Salsa");
    type(u"builderOption-0"_s, "Pico de gallo");
    type(u"builderOption-1"_s, "Salsa Verde");
    type(u"builderOptionPrice-1"_s, "0.50");
    scrollTo(by(u"builderAddOption"_s));
    s.tapItem(by(u"builderAddOption"_s));
    QTest::qWait(60);
    type(u"builderOption-2"_s, "Habanero");
    s.shot("builder-group");
    s.tapItem(by(u"builderSaveGroup"_s));
    QTest::qWait(150);
    QVariantMap salsa;
    for (const QVariant &g : s.pos.choiceGroups())
        if (g.toMap()[u"name"_s] == u"Salsa"_s)
            salsa = g.toMap();
    REQUIRE_FALSE(salsa.isEmpty());
    CHECK(salsa[u"rule"_s] == u"Pick 1"_s);   // one, required: the starting rule
    CHECK(salsa[u"options"_s].toList().size() == 3);
    CHECK(by(u"builderGroupRow-"_s + salsa[u"id"_s].toString()));
}

TEST_CASE("UI: the Menu Builder adds several at once, and duplicates one", "[flow][ui][menubuild]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    // Down the categories list, as a finger would scroll it.
    QQuickItem *plates = by(u"builderCategory-plates"_s);
    REQUIRE(plates);
    QQuickItem *list = plates->parentItem()->parentItem();   // the ListView
    QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, 3), Q_ARG(int, 2 /* Contain */));
    QTest::qWait(60);
    s.tapItem(by(u"builderCategory-plates"_s));
    QTest::qWait(60);
    s.tapItem(by(u"builderAddSeveral"_s));
    QTest::qWait(100);
    QQuickItem *text = by(u"builderSeveralText"_s);
    REQUIRE(text);
    text->setProperty("text", u"Fish Tacos 13.50\nShrimp Tacos 14,50\nDrinks: Horchata 2.75"_s);
    QTest::qWait(60);
    s.shot("builder-several");
    QQuickItem *add = by(u"builderSeveralAdd"_s);
    REQUIRE(add);
    CHECK(add->property("text").toString() == u"Add 3 items"_s);
    s.tapItem(add);
    QTest::qWait(150);
    QStringList names;
    for (const QVariant &m : s.pos.menuItems())
        names << m.toMap()[u"name"_s].toString() + u'/' + m.toMap()[u"family"_s].toString();
    CHECK(names.contains(u"Fish Tacos/plates"_s));
    CHECK(names.contains(u"Shrimp Tacos/plates"_s));
    CHECK(names.contains(u"Horchata/drinks"_s));

    // Duplicate Fish Tacos: the copy opens, ready to rename.
    QString fish;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Fish Tacos"_s)
            fish = m.toMap()[u"id"_s].toString();
    s.tapItem(by(u"builderItem-"_s + fish));
    QTest::qWait(60);
    s.tapItem(by(u"builderDuplicate"_s));
    QTest::qWait(150);
    QQuickItem *name = by(u"builderName"_s);
    REQUIRE(name);
    CHECK(name->property("text").toString() == u"Fish Tacos 2"_s);
}

TEST_CASE("UI: an older store switches its menu pages to self-filling, then saves", "[flow][ui][menuscreens]")
{
    Screen s(false, 1280, 800);
    // Lunch as older stores have it: category buttons placed by hand.
    vt::layout::Layout old = s.c.activeLayout();
    vt::layout::Page *lunch = old.page(u"index-lunch"_s);
    REQUIRE(lunch);
    lunch->zones.removeIf([](const vt::layout::Zone &z) { return z.kind == u"menuCategories"_s; });
    for (const auto &[id, target, x] : {std::tuple{u"cat-burgers"_s, u"items-burgers"_s, 592}, std::tuple{u"cat-salads"_s, u"items-salads"_s, 1036}}) {
        vt::layout::Zone z;
        z.id = id;
        z.label = id;
        z.rect = QRect(x, 192, 424, 240);
        vt::layout::Action a;
        a.data = {{u"type"_s, u"jump"_s}, {u"page"_s, target}};
        z.actions.append(a);
        lunch->zones.append(z);
    }
    s.c.replaceLayout(old);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    REQUIRE(by(u"builderHandBuilt"_s));                      // the offer
    s.shot("builder-handbuilt");
    s.tapItem(by(u"builderSwitchScreens"_s));
    QTest::qWait(150);
    REQUIRE(s.c.editing());                                  // to look over
    CHECK(s.c.pageId() == u"index-lunch"_s);
    s.shot("builder-switched");
    REQUIRE(s.c.leaveEditMode(true));                        // Save
    CHECK_FALSE(s.c.menuScreensHandBuilt());
    CHECK(s.c.activeLayout().page(u"index-lunch"_s)->zone(u"categories"_s));
    CHECK_FALSE(s.c.activeLayout().page(u"index-lunch"_s)->zone(u"cat-burgers"_s));
    QTest::qWait(60);
    CHECK_FALSE(by(u"builderHandBuilt"_s));                  // nothing more to offer
}

TEST_CASE("UI: the menu screen's buttons fit their category, and Favorites come first", "[flow][ui][menufit]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, u"Cobb Salad"_s}, {u"favorite"_s, true}}));
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    QQuickItem *root = s.window->contentItem();
    QQuickItem *grid = nullptr;
    const auto findGrid = [&] {
        grid = Screen::findBy(root, "objectName", u"menuButtons"_s);
        return grid;
    };
    // Every category's items on the screen at once, as big as fits.
    for (const QVariant &c : s.pos.menuCategories()) {
        if (c.toMap()[u"count"_s].toInt() == 0)
            continue;
        REQUIRE(s.c.openCategory(c.toMap()[u"id"_s].toString()));
        QTest::qWait(80);
        REQUIRE(findGrid());
        INFO(c.toMap()[u"name"_s].toString().toStdString());
        CHECK(grid->property("contentHeight").toReal() <= grid->height() + 1);   // no scrolling
        CHECK(grid->property("cellHeight").toReal() >= 90);                     // big enough to touch
    }
    REQUIRE(s.c.openCategory(u"burgers"_s));
    QTest::qWait(80);
    s.shot("menu-fit-burgers");

    // ★ Favorites: first, with the one marked.
    QQuickItem *fav = Screen::findBy(root, "objectName", u"menuFamily-★"_s);
    REQUIRE(fav);
    s.tapItem(fav);
    QTest::qWait(80);
    CHECK(s.c.menuCategory() == u"★"_s);
    CHECK(Screen::findBy(root, "objectName", u"menuItem-cobb"_s));
    s.shot("menu-favorites");
}

TEST_CASE("UI: in the Menu Builder, hold an item and drag it to its new place", "[flow][ui][menubuild]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    s.tapItem(by(u"builderCategory-burgers"_s));
    QTest::qWait(60);
    QQuickItem *from = by(u"builderItem-classic-burger"_s), *to = by(u"builderItem-veggie-burger"_s);
    REQUIRE(from);
    REQUIRE(to);
    const QPoint a = from->mapToScene(QPointF(from->width() / 2, from->height() / 2)).toPoint();
    const QPoint b = to->mapToScene(QPointF(to->width() / 2, to->height() / 2)).toPoint();
    QTest::mousePress(s.window, Qt::LeftButton, {}, a);
    QTest::qWait(500);                                      // held
    for (int i = 1; i <= 10; ++i) {
        QTest::mouseMove(s.window, a + (b - a) * i / 10);
        QTest::qWait(15);
    }
    REQUIRE(by(u"builderGhost"_s));                         // under the finger
    s.shot("builder-drag");
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, b);
    QTest::qWait(120);
    QStringList ids;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"family"_s] == u"burgers"_s)
            ids << m.toMap()[u"id"_s].toString();
    // Veggie Burger was 5th; Classic is there now.
    CHECK(ids.indexOf(u"classic-burger"_s) == 4);
    CHECK(ids.first() == u"cheeseburger"_s);
    CHECK_FALSE(by(u"builderGhost"_s));
}

TEST_CASE("Flow: the bar's screen starts orders on Drinks; ‹ Menu still shows the meal's", "[flow][startcategory]")
{
    Session s;
    core::TerminalConfig bar;
    bar.name = s.pos.terminalName().toStdString();
    bar.startCategory = "drinks";
    s.pos.shared()->settings.terminals.push_back(bar);
    REQUIRE(s.pos.loginWithPin(u"4444"_s));            // Jo, the bartender
    s.c.activate(u"quick"_s);
    CHECK(s.c.pageId() == u"menu-all"_s);
    CHECK(s.c.menuCategory() == u"drinks"_s);
    s.c.activate(u"tab-categories"_s);                 // ‹ Menu
    CHECK(s.c.pageId() == u"index-lunch"_s);
    // Another screen: the meal's page, as before.
    s.pos.shared()->settings.terminals.back().startCategory.clear();
    s.c.activate(u"flow-tables"_s);
    s.c.activate(u"quick"_s);
    CHECK(s.c.pageId() == u"index-lunch"_s);
}

TEST_CASE("Flow: a device's own start screen: the patio handheld opens on the patio, whoever logs in", "[flow][startpage]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1111"_s));            // Sam, a server: the floor plan
    const QString usual = s.c.pageId();
    s.pos.logout();
    core::TerminalConfig patio;
    patio.name = s.pos.terminalName().toStdString();
    patio.startPage = "floor-patio";
    s.pos.shared()->settings.terminals.push_back(patio);
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK(s.c.pageId() == u"floor-patio"_s);
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"4444"_s));            // Jo, the bartender, too
    CHECK(s.c.pageId() == u"floor-patio"_s);
    s.pos.logout();
    // Saved on the Terminals screen, and offered among the start pages.
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const QVariantList records = s.pos.adminRecords(u"terminals"_s);
    REQUIRE_FALSE(records.isEmpty());
    QVariantMap me = records.last().toMap();
    CHECK(me[u"startPage"_s] == u"floor-patio"_s);
    me[u"startPage"_s] = u"check-list"_s;
    REQUIRE(s.pos.adminSave(u"terminals"_s, int(records.size()) - 1, me));
    CHECK(s.pos.shared()->settings.terminals.back().startPage == "check-list");
    QStringList offered;
    for (const QVariant &v : s.c.pageChoices(u"start"_s))
        offered << v.toMap()[u"value"_s].toString();
    CHECK(offered.contains(u"floor-patio"_s));
    CHECK(offered.contains(u"check-list"_s));
    CHECK(offered.contains(u"menu-all"_s));
    CHECK_FALSE(offered.contains(u"admin-store"_s));
    s.pos.logout();
    REQUIRE(s.pos.loginWithPin(u"1111"_s));
    CHECK(s.c.pageId() == u"check-list"_s);
    CHECK(usual != u"check-list"_s);
}

TEST_CASE("Flow: an item's button added to a page built by hand, in its next free place", "[flow][alsoon]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveMenuItemCard({{u"name"_s, u"Patty Melt"_s}, {u"price"_s, u"12.75"_s}, {u"family"_s, u"burgers"_s}}));
    QString melt;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Patty Melt"_s)
            melt = m.toMap()[u"id"_s].toString();
    QStringList pages;
    for (const QVariant &p : s.c.handBuiltPages(melt))
        pages << p.toMap()[u"id"_s].toString();
    CHECK(pages.contains(u"items-burgers"_s));      // a page of buttons placed by hand
    CHECK_FALSE(pages.contains(u"menu-all"_s));     // self-filling: no need
    const bool added = s.c.addItemButton(u"items-burgers"_s, melt, u"Patty Melt"_s);
    INFO(s.c.statusText().toStdString());
    REQUIRE(added);
    const vt::layout::Page *burgers = s.c.activeLayout().page(u"items-burgers"_s);
    REQUIRE(burgers);
    const vt::layout::Zone *button = burgers->zone(u"item-"_s + melt);
    REQUIRE(button);
    CHECK(button->label == u"Patty Melt"_s);
    CHECK(button->rect.size() == burgers->zone(u"item-1"_s)->rect.size());   // like the others
    for (const vt::layout::Zone &z : burgers->zones)
        if (z.id != button->id)
            CHECK_FALSE(z.rect.intersects(button->rect));                     // in a free place
    for (const QVariant &p : s.c.handBuiltPages(melt))
        if (p.toMap()[u"id"_s] == u"items-burgers"_s)
            CHECK(p.toMap()[u"has"_s].toBool());
    // It orders.
    REQUIRE(s.c.jumpTo(u"items-burgers"_s));
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    s.c.activate(button->id);
    CHECK(s.pos.lines().size() == 1);
}

TEST_CASE("UI: the Menu Builder asks before dropping changes; a new choice group comes back to its item", "[flow][ui][menubuild][unsaved]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    const auto type = [&](const QString &field, const char *text) {
        QQuickItem *f = by(field);
        REQUIRE(f);
        scrollTo(f);
        s.tapItem(f);
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    };
    const auto priceOf = [&](const QString &id) {
        for (const QVariant &m : s.pos.menuItems())
            if (m.toMap()[u"id"_s] == id)
                return m.toMap()[u"price"_s].toString();
        return QString();
    };
    s.tapItem(by(u"builderCategory-burgers"_s));
    QTest::qWait(60);
    s.tapItem(by(u"builderItem-classic-burger"_s));
    QTest::qWait(60);
    const QString before = priceOf(u"classic-burger"_s);

    // Nothing changed: straight to the next one.
    s.tapItem(by(u"builderItem-classic-burger"_s));
    QTest::qWait(60);
    CHECK_FALSE(shown(u"builderLeaveSave"_s));

    // Changed, then another item: asked; Don't Save drops it.
    type(u"builderPrice"_s, "99");
    QQuickItem *other = by(u"builderAddItem"_s);
    s.tapItem(other);
    QTest::qWait(100);
    REQUIRE(shown(u"builderLeaveSave"_s));
    s.shot("builder-unsaved");
    s.tapItem(by(u"builderLeaveDiscard"_s));
    QTest::qWait(100);
    CHECK(priceOf(u"classic-burger"_s) == before);
    CHECK(by(u"builderName"_s)->property("text").toString().isEmpty());   // the new item's card

    // Changed again: Save keeps it, then goes on.
    s.tapItem(by(u"builderItem-classic-burger"_s));
    QTest::qWait(60);
    type(u"builderPrice"_s, "13.25");
    s.tapItem(by(u"builderAddItem"_s));
    QTest::qWait(100);
    REQUIRE(shown(u"builderLeaveSave"_s));
    s.tapItem(by(u"builderLeaveSave"_s));
    QTest::qWait(150);
    CHECK(priceOf(u"classic-burger"_s) == u"$13.25"_s);
    CHECK_FALSE(shown(u"builderLeaveSave"_s));

    // + New Choice Group from an item: made, then back to the item with it chosen,
    // the item's unsaved name kept.
    s.tapItem(by(u"builderItem-classic-burger"_s));
    QTest::qWait(60);
    type(u"builderName"_s, "Classic Burger Deluxe");
    scrollTo(by(u"builderNewGroup"_s));
    s.tapItem(by(u"builderNewGroup"_s));
    QTest::qWait(100);
    CHECK_FALSE(shown(u"builderLeaveSave"_s));     // nothing is dropped
    CHECK(shown(u"builderBackToItem"_s));
    type(u"builderGroupName"_s, "Bun");
    type(u"builderOption-0"_s, "Brioche");
    type(u"builderOption-1"_s, "Pretzel");
    s.tapItem(by(u"builderSaveGroup"_s));
    QTest::qWait(150);
    QString bun;
    for (const QVariant &g : s.pos.choiceGroups())
        if (g.toMap()[u"name"_s] == u"Bun"_s)
            bun = g.toMap()[u"id"_s].toString();
    REQUIRE_FALSE(bun.isEmpty());
    REQUIRE(shown(u"builderName"_s));
    CHECK(by(u"builderName"_s)->property("text").toString() == u"Classic Burger Deluxe"_s);
    REQUIRE(by(u"builderGroup-"_s + bun));
    CHECK(by(u"builderGroup-"_s + bun)->property("checked").toBool());
    s.tapItem(by(u"builderSave"_s));
    QTest::qWait(150);
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"id"_s] == u"classic-burger"_s) {
            CHECK(m.toMap()[u"name"_s] == u"Classic Burger Deluxe"_s);
            CHECK(m.toMap()[u"groups"_s].toStringList().contains(bun));
        }
}

TEST_CASE("Flow: a menu exported at one store, imported at another", "[flow][menucopy]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // Store A: the taqueria starter menu on top of its own.
    Session a;
    REQUIRE(a.pos.loginWithPin(u"1234"_s));
    REQUIRE(a.pos.applyMenuTemplate(u"taqueria"_s));
    a.c.setExportDirectory(dir.path());
    a.c.exportMenu();
    const QStringList files = QDir(dir.path()).entryList({u"*.vtmenu.json"_s});
    REQUIRE(files.size() == 1);
    const QUrl file = QUrl::fromLocalFile(QDir(dir.path()).filePath(files.front()));

    // Store B: the same demo menu, without the taqueria; a price of its own.
    Session b;
    REQUIRE(b.pos.loginWithPin(u"1234"_s));
    auto *classic = const_cast<core::MenuItem *>(b.pos.findItem(u"classic-burger"_s));
    classic->price = vt::Money::fromCents(999);
    const int before = int(b.pos.shared()->menu.size());
    const QVariantMap read = b.c.readMenuFile(file);
    REQUIRE(read.contains(u"menuFile"_s));
    CHECK_FALSE(read.contains(u"error"_s));
    bool sawTaco = false;
    for (const QVariant &i : read[u"items"_s].toList())
        if (i.toMap()[u"name"_s] == u"Carne Asada Taco"_s) {
            sawTaco = true;
            CHECK(i.toMap()[u"price"_s].toDouble() == 3.5);
            CHECK(i.toMap()[u"category"_s] == u"Tacos"_s);
        }
    CHECK(sawTaco);

    // The taqueria's items B doesn't have by name.
    int expected = 0;
    for (const QVariant &v : read[u"menuFile"_s].toMap()[u"items"_s].toList()) {
        const QString name = v.toMap()[u"name"_s].toString();
        expected += std::ranges::none_of(b.pos.shared()->menu, [&](const core::MenuItem &m) {
            return QString::fromStdString(m.name).compare(name, Qt::CaseInsensitive) == 0;
        });
    }
    REQUIRE(expected >= 10);
    const int added = b.pos.importMenuFile(read[u"menuFile"_s].toMap());
    CHECK(added == expected);
    CHECK(int(b.pos.shared()->menu.size()) == before + expected);
    CHECK(b.pos.findItem(u"classic-burger"_s)->price.cents() == 999);   // B's own unchanged
    // The taco, in a Tacos category, with Salsa and Tortilla and what's on it.
    const core::MenuItem *taco = nullptr;
    for (const core::MenuItem &m : b.pos.shared()->menu)
        if (m.name == "Carne Asada Taco")
            taco = &m;
    REQUIRE(taco);
    CHECK(taco->price.cents() == 350);
    const auto cats = b.pos.shared()->categories();
    CHECK(std::ranges::any_of(cats, [&](const core::MenuCategory &c) { return c.id == taco->family && c.name == "Tacos"; }));
    QStringList groupNames;
    for (const std::string &g : taco->modifierGroups)
        if (const core::ModifierGroup *mg = b.pos.shared()->settings.modifierGroup(g))
            groupNames << QString::fromStdString(mg->name);
    CHECK(groupNames.size() == 3);
    CHECK(groupNames.contains(u"Salsa"_s));
    CHECK(groupNames.contains(u"Tortilla"_s));
    const core::ModifierGroup *onIt = b.pos.shared()->settings.modifierGroup("on-" + taco->id);
    REQUIRE(onIt);
    CHECK(onIt->options.size() == 2);                           // onion, cilantro
    // The check finds nothing wrong with what came in.
    for (const QVariant &p : b.pos.menuProblems())
        CHECK_FALSE(p.toMap()[u"serious"_s].toBool());
    // Again: nothing doubles.
    CHECK(b.pos.importMenuFile(read[u"menuFile"_s].toMap()) == 0);
    CHECK(int(b.pos.shared()->menu.size()) == before + expected);
    // Not a menu.
    CHECK(b.c.readMenuFile(QUrl::fromLocalFile(dir.filePath(u"x.json"_s))).contains(u"error"_s));
}

TEST_CASE("UI: a card not saved is kept while the Menu Builder is left", "[flow][ui][menubuild][keptcard]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    s.tapItem(by(u"builderCategory-burgers"_s));
    QTest::qWait(60);
    s.tapItem(by(u"builderItem-classic-burger"_s));
    QTest::qWait(60);
    CHECK_FALSE(shown(u"builderNotSaved"_s));
    QQuickItem *price = by(u"builderPrice"_s);
    s.tapItem(price);
    QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
    for (const char *ch = "15.5"; *ch; ++ch)
        QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
    QTest::qWait(60);
    CHECK(shown(u"builderNotSaved"_s));
    // Off to another page, then back.
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(100);
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(150);
    root = s.window->contentItem();
    REQUIRE(by(u"builderPrice"_s));
    CHECK(by(u"builderPrice"_s)->property("text").toString() == u"15.5"_s);
    CHECK(shown(u"builderNotSaved"_s));
    s.tapItem(by(u"builderSave"_s));
    QTest::qWait(150);
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"id"_s] == u"classic-burger"_s)
            CHECK(m.toMap()[u"price"_s] == u"$15.50"_s);
    CHECK_FALSE(shown(u"builderNotSaved"_s));
    // Saved: nothing kept the next time.
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(100);
    CHECK_FALSE(s.c.take(u"menuBuilder"_s).isValid());
}

TEST_CASE("UI: Menu Builder: find an item, its More settings, prices up for a category", "[flow][ui][menubuild][cardmore]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    // Prices by meal: opening it is not a change.
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"cheeseburger"_s}, {u"name"_s, u"Cheeseburger"_s},
                                    {u"periodPrices"_s, QVariantMap{{u"dinner"_s, u"15.00"_s}}}, {u"number"_s, u"212"_s}}));
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    const auto type = [&](const char *text) {
        QTest::keyClick(s.window, Qt::Key_A, Qt::ControlModifier);
        for (const char *ch = text; *ch; ++ch)
            QTest::sendKeyEvent(QTest::Click, s.window, Qt::Key_unknown, *ch, Qt::NoModifier);
        QTest::qWait(60);
    };

    // Found by its number, wherever it is.
    s.tapItem(by(u"builderSearch"_s));
    type("212");
    CHECK(shown(u"builderFound-cheeseburger"_s));
    CHECK_FALSE(shown(u"builderCategory-burgers"_s));
    s.tapItem(by(u"builderFound-cheeseburger"_s));
    QTest::qWait(60);
    REQUIRE(by(u"builderName"_s));
    CHECK(by(u"builderName"_s)->property("text").toString() == u"Cheeseburger"_s);
    CHECK_FALSE(shown(u"builderNotSaved"_s));
    // More: folded until wanted; a kitchen name typed is a change.
    CHECK_FALSE(shown(u"builderKitchenName"_s));
    QQuickItem *card = by(u"builderCard"_s);
    REQUIRE(card);
    card->setProperty("contentY", card->property("contentHeight").toReal() - card->height());
    QTest::qWait(30);
    s.tapItem(by(u"builderMore"_s));
    QTest::qWait(60);
    REQUIRE(shown(u"builderKitchenName"_s));
    if (qEnvironmentVariableIsSet("VTM_SHOTS"))
        s.window->grabWindow().save(qEnvironmentVariable("VTM_SHOTS") + u"/builder-more.png"_s);
    CHECK(by(u"builderPeriodPrice-dinner"_s)->property("text").toString() == u"15.00"_s);
    s.tapItem(by(u"builderKitchenName"_s));
    type("CHZ");
    CHECK(shown(u"builderNotSaved"_s));
    s.tapItem(by(u"builderSave"_s));
    QTest::qWait(100);
    CHECK(s.pos.findItem(u"cheeseburger"_s)->kitchenName == "CHZ");
    CHECK(s.pos.findItem(u"cheeseburger"_s)->periodPrices.at("dinner").cents() == 1500);
    CHECK_FALSE(shown(u"builderNotSaved"_s));

    // Prices…: burgers up 10%, to the nickel; shown, then changed.
    std::map<std::string, qint64> before;
    for (const core::MenuItem &m : s.pos.shared()->menu)
        before[m.id] = m.price.cents();
    s.tapItem(by(u"builderPrices"_s));
    QTest::qWait(100);
    REQUIRE(shown(u"builderPricesApply"_s));
    s.tapItem(by(u"builderPricesAmount"_s));
    type("10");
    if (qEnvironmentVariableIsSet("VTM_SHOTS"))
        s.window->grabWindow().save(qEnvironmentVariable("VTM_SHOTS") + u"/builder-prices.png"_s);
    QQuickItem *preview = by(u"builderPricesPreview"_s);
    REQUIRE(preview);
    CHECK(preview->property("count").toInt() > 1);
    s.tapItem(by(u"builderPricesApply"_s));
    QTest::qWait(100);
    for (const core::MenuItem &m : s.pos.shared()->menu) {
        const qint64 was = before[m.id];
        if (m.family == "burgers" && !m.isModifier && was > 0) {
            CHECK(m.price.cents() == (was * 110 + 250) / 500 * 5);
        } else {
            CHECK(m.price.cents() == was);
        }
    }
    CHECK(s.pos.findItem(u"cheeseburger"_s)->periodPrices.at("dinner").cents() == 1650);   // its dinner price too
    CHECK(s.pos.menuUndoText().startsWith(u"Change "_s));
    // The open card shows the new price, nothing unsaved.
    CHECK(by(u"builderPrice"_s)->property("text").toString()
          == QString::number(double(s.pos.findItem(u"cheeseburger"_s)->price.cents()) / 100.0, 'f', 2));
    CHECK_FALSE(shown(u"builderNotSaved"_s));

    // More… with a change not saved: asked first.
    s.tapItem(by(u"builderKitchenName"_s));
    type("CB");
    s.tapItem(Screen::findBy(root, "text", u"More…"_s));
    QTest::qWait(100);
    CHECK(shown(u"builderLeaveSave"_s));
    CHECK(s.c.pageId() == u"menu-builder"_s);
}

TEST_CASE("Flow: an item taken off the menu takes its hand-placed buttons; a category gone, the screen's start too", "[flow][leftbehind]")
{
    Session s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveMenuItemCard({{u"name"_s, u"Patty Melt"_s}, {u"price"_s, u"12.75"_s}, {u"family"_s, u"burgers"_s}}));
    QString melt;
    for (const QVariant &m : s.pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Patty Melt"_s)
            melt = m.toMap()[u"id"_s].toString();
    REQUIRE(s.c.addItemButton(u"items-burgers"_s, melt, u"Patty Melt"_s));
    REQUIRE(s.c.activeLayout().page(u"items-burgers"_s)->zone(u"item-"_s + melt));
    REQUIRE(s.pos.deleteMenuItemCard(melt));
    CHECK(s.c.removeItemButtons(melt) == 1);
    CHECK_FALSE(s.c.activeLayout().page(u"items-burgers"_s)->zone(u"item-"_s + melt));
    CHECK(s.c.activeLayout().page(u"items-burgers"_s)->zone(u"item-1"_s));   // the others stay

    REQUIRE(s.pos.saveCategory({{u"name"_s, u"Desserts"_s}}));
    core::TerminalConfig t;
    t.name = s.pos.terminalName().toStdString();
    t.startCategory = "desserts";
    s.pos.shared()->settings.terminals.push_back(t);
    REQUIRE(s.pos.deleteCategory(u"desserts"_s));
    CHECK(s.pos.shared()->settings.terminals.back().startCategory.empty());
}

TEST_CASE("Flow: photos from a spreadsheet's Photo column, and with a menu to another store", "[flow][menuphotos]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto picture = [&](const QString &name, QColor color) {
        QImage img(40, 30, QImage::Format_RGB32);
        img.fill(color);
        REQUIRE(img.save(dir.filePath(name)));
    };
    picture(u"Fish Tacos.PNG"_s, Qt::red);
    picture(u"churros.png"_s, Qt::yellow);
    QFile csv(dir.filePath(u"menu.csv"_s));
    REQUIRE(csv.open(QIODevice::WriteOnly));
    csv.write("Name,Price,Category,Foto\n"
              "Fish Tacos,4.50,Tacos,fish tacos.png\n"     // another case: found anyway
              "Churros,3.00,Desserts,churros.png\n"
              "Flan,3.50,Desserts,flan.jpg\n");           // not there
    csv.close();

    Session a;
    REQUIRE(a.pos.loginWithPin(u"1234"_s));
    const QVariantMap read = a.c.readMenuFile(QUrl::fromLocalFile(csv.fileName()));
    CHECK(read[u"photos"_s].toInt() == 2);
    CHECK(read[u"problems"_s].toStringList().filter(u"flan.jpg"_s).size() == 1);
    CHECK(read[u"items"_s].toList().size() == 3);          // Flan comes in, without a photo
    // A different picture already called fish-tacos.png: kept; the new one gets its own name.
    a.pos.shared()->images["fish-tacos.png"] = QByteArray("not the same");
    CHECK(a.pos.importMenuRows(read[u"items"_s].toList(), {}, false) == 3);
    const auto imageOf = [](app::PosService &pos, const char *name) {
        for (const core::MenuItem &m : pos.shared()->menu)
            if (m.name == name)
                return QString::fromStdString(m.image);
        return u"?"_s;
    };
    CHECK(imageOf(a.pos, "Fish Tacos") == u"store:fish-tacos-2.png"_s);
    CHECK(imageOf(a.pos, "Churros") == u"store:churros.png"_s);
    CHECK(imageOf(a.pos, "Flan").isEmpty());
    CHECK(a.pos.shared()->images.at("fish-tacos.png") == QByteArray("not the same"));
    CHECK_FALSE(QImage::fromData(a.pos.shared()->images.at("churros.png")).isNull());

    // To another store: the photos go along.
    a.c.setExportDirectory(dir.filePath(u"out"_s));
    a.c.exportMenu();
    const QStringList files = QDir(dir.filePath(u"out"_s)).entryList({u"*.vtmenu.json"_s});
    REQUIRE(files.size() == 1);
    Session b;
    REQUIRE(b.pos.loginWithPin(u"1234"_s));
    const QVariantMap menu = b.c.readMenuFile(QUrl::fromLocalFile(QDir(dir.filePath(u"out"_s)).filePath(files.front())));
    bool churrosPhoto = false;
    for (const QVariant &i : menu[u"items"_s].toList())
        churrosPhoto |= i.toMap()[u"name"_s] == u"Churros"_s && i.toMap()[u"photo"_s].toBool();
    CHECK(churrosPhoto);
    REQUIRE(b.pos.importMenuFile(menu[u"menuFile"_s].toMap()) >= 3);
    CHECK(imageOf(b.pos, "Churros") == u"store:churros.png"_s);
    CHECK(b.pos.shared()->images.at("churros.png") == a.pos.shared()->images.at("churros.png"));
    CHECK(imageOf(b.pos, "Flan").isEmpty());
}

TEST_CASE("UI: allergies: set on the check, marked on the menu; contained, on the item's card; avoided on the kiosk", "[flow][ui][allergy]")
{
    Screen s(false, 1280, 800);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    const auto shown = [&](const QString &name) { QQuickItem *i = by(name); return i && i->isVisible(); };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    // The Menu Builder: the Cobb contains egg and dairy.
    REQUIRE(s.c.jumpTo(u"menu-builder"_s));
    QTest::qWait(100);
    s.tapItem(by(u"builderCategory-salads"_s));
    QTest::qWait(60);
    REQUIRE(by(u"builderItem-cobb"_s));
    s.tapItem(by(u"builderItem-cobb"_s));
    QTest::qWait(60);
    for (const char *a : {"egg", "dairy"}) {
        QQuickItem *chip = by(u"builderAllergen-"_s + QLatin1String(a));
        REQUIRE(chip);
        scrollTo(chip);
        s.tapItem(chip);
        QTest::qWait(30);
    }
    s.tapItem(by(u"builderSave"_s));
    QTest::qWait(100);
    CHECK(s.pos.findItem(u"cobb"_s)->allergens == std::vector<std::string>{"dairy", "egg"});

    // A guest allergic to egg: Check Options -> Allergy...
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.c.jumpTo(u"check-options"_s));
    QTest::qWait(60);
    s.c.activate(u"allergies"_s);
    QTest::qWait(100);
    REQUIRE(shown(u"allergySheet"_s));
    s.tapItem(by(u"allergy-egg"_s));
    QTest::qWait(60);
    CHECK(s.pos.checkInfo()[u"allergies"_s].toStringList() == QStringList{u"egg"_s});
    s.shot("allergy-sheet");
    s.tapItem(by(u"allergyDone"_s));
    QTest::qWait(60);
    CHECK_FALSE(shown(u"allergySheet"_s));
    // The menu: the Cobb is marked; something without egg isn't.
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    s.c.setMenuCategory(u"salads"_s);
    QTest::qWait(100);
    root = s.window->contentItem();
    CHECK(shown(u"allergyMark-cobb"_s));
    CHECK_FALSE(shown(u"allergyMark-house-salad"_s));
    s.shot("allergy-menu");
    s.pos.releaseCheck();

    // The kiosk: avoiding egg, the Cobb isn't offered.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.pos.enableSelfOrder();
    QTest::qWait(60);
    s.tapItem(by(u"kioskAttract"_s));
    QTest::qWait(60);
    s.tapItem(by(u"kioskForHere"_s));
    QTest::qWait(100);
    QQuickItem *kiosk = by(u"selfOrder"_s);
    REQUIRE(kiosk);
    const auto offered = [&](const char *id) {
        for (const QVariant &i : kiosk->property("menu").toMap()[u"items"_s].toList())
            if (i.toMap()[u"id"_s] == QLatin1String(id))
                for (const QVariant &v : QQmlProperty::read(kiosk, u"avoid"_s).toList())
                    if (i.toMap()[u"allergens"_s].toStringList().contains(v.toString()))
                        return false;
        return true;
    };
    CHECK(offered("cobb"));
    s.tapItem(by(u"kioskAllergies"_s));
    QTest::qWait(60);
    REQUIRE(shown(u"kioskAvoidSheet"_s));
    s.tapItem(by(u"kioskAvoid-egg"_s));
    QTest::qWait(60);
    s.shot("kiosk-avoid");
    s.tapItem(by(u"kioskAvoidDone"_s));
    QTest::qWait(60);
    CHECK(QQmlProperty::read(kiosk, u"avoid"_s).toStringList() == QStringList{u"egg"_s});
    CHECK_FALSE(offered("cobb"));
}

TEST_CASE("Card reader (simulated), the internet down: cards taken offline within the limits, sent or declined later", "[ui][cards][offlinecards]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, {{u"name"_s, s.pos.terminalName()}, {u"cardReader"_s, u"simulated"_s}}));
    CardReader *reader = s.c.cardReader();
    reader->setSimulatedDelay(100);
    reader->setSimulatedOffline(true);
    const auto pay = [&](const char *item) {
        REQUIRE(s.pos.startCheck(core::CheckType::Quick));
        REQUIRE(s.pos.addItem(QString::fromLatin1(item)));
        REQUIRE(s.c.jumpTo(u"settle"_s));
        s.c.activate(u"tender-credit"_s);
        QTest::qWait(300);
    };
    const auto lastCheck = [&]() -> core::Check & {
        auto &today = s.pos.shared()->closedToday;
        REQUIRE_FALSE(today.empty());
        return today.back();
    };

    // Not allowed (the store's setting): the card needs the internet.
    pay("coffee");
    CHECK(s.pos.payments().isEmpty());
    CHECK(s.c.statusText().contains(u"internet"_s));
    s.pos.releaseCheck();

    // Allowed, up to $50 a card.
    QVariantMap store = s.pos.adminRecords(u"store"_s).value(0).toMap();
    store[u"offlineCards"_s] = true;
    store[u"offlineCardMax"_s] = 50.0;
    store[u"offlineCardTotal"_s] = 500.0;
    const bool saved = s.pos.adminSave(u"store"_s, 0, store);
    INFO(s.c.statusText().toStdString());
    REQUIRE(saved);
    CHECK(s.pos.shared()->settings.offlineCards);
    pay("coffee");
    REQUIRE(s.pos.payments().size() == 1);
    REQUIRE(s.pos.closeCheck());
    core::Payment first = lastCheck().payments.back();
    CHECK(first.offline == "waiting");
    CHECK(QString::fromStdString(first.reference).startsWith(u"offline:"_s));
    // No refund before Stripe has it.
    CHECK_FALSE(s.pos.refundPayment(lastCheck().id, first.id, 0, u"wrong order"_s));
    CHECK(s.c.statusText().contains(u"hasn't reached Stripe"_s));

    // Over the limit: refused.
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    for (int i = 0; i < 4; ++i)
        REQUIRE(s.pos.addItem(u"burger-of-the-day"_s));   // $56 and tax: over $50
    REQUIRE(s.c.jumpTo(u"settle"_s));
    s.c.activate(u"tender-credit"_s);
    QTest::qWait(300);
    CHECK(s.pos.payments().isEmpty());
    s.pos.releaseCheck();

    // A second one, then the internet is back: the first reaches Stripe, the second is declined.
    pay("coffee");
    REQUIRE(s.pos.closeCheck());
    const core::Payment second = lastCheck().payments.back();
    const qint64 secondCheck = lastCheck().id;
    reader->simulateForwarded(QString::fromStdString(first.reference).mid(8), true);
    reader->simulateForwarded(QString::fromStdString(second.reference).mid(8), false);
    QTest::qWait(50);
    for (const core::Check &c : s.pos.shared()->closedToday)
        for (const core::Payment &p : c.payments) {
            if (p.id == first.id && c.id != secondCheck) {
                CHECK(p.offline.empty());
                CHECK(QString::fromStdString(p.reference).startsWith(u"pi_sim_"_s));
            }
            if (c.id == secondCheck)
                CHECK(p.offline == "declined");
        }
    const QVariantList alerts = s.pos.cardAlerts();
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].toMap()[u"checkId"_s].toLongLong() == secondCheck);
    QQuickItem *root = s.window->contentItem();
    QTest::qWait(60);
    QQuickItem *banner = Screen::findBy(root, "objectName", u"cardAlerts"_s);
    REQUIRE(banner);
    CHECK(banner->isVisible());
    s.shot("offline-card-declined");
    REQUIRE(s.pos.seeCardAlert(alerts[0].toMap()[u"ref"_s].toString()));
    CHECK(s.pos.cardAlerts().isEmpty());
}

TEST_CASE("UI: a coupon on the Pay screen: typed or scanned, shown, taken off", "[flow][ui][coupons]")
{
    Screen s(false, 1280, 800);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    core::PosSettings::Promotion coupon;
    coupon.id = "five-off";
    coupon.name = "Five Off";
    coupon.code = "SAVE5";
    coupon.amount = Money::fromCents(500);
    s.pos.shared()->settings.promotions.push_back(coupon);
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    s.pos.addItem(u"cobb"_s);
    s.pos.addItem(u"cobb"_s);
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(100);
    s.c.activate(u"coupon"_s);
    QTest::qWait(100);
    REQUIRE(by(u"couponSheet"_s));
    REQUIRE(by(u"couponSheet"_s)->isVisible());
    // A scanner types the code and presses Enter, into the field that has the focus.
    const auto typeKeys = [&](const char *text) { for (const char *k = text; *k; ++k) QTest::keyClick(s.window, *k); };
    typeKeys("save5");
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(100);
    CHECK(s.pos.totals()[u"subtotal"_s].toString() == u"$20.00"_s);
    REQUIRE(by(u"couponRemove-five-off"_s));
    s.shot("coupon-sheet");
    s.tapItem(by(u"couponRemove-five-off"_s));
    QTest::qWait(100);
    CHECK(s.pos.totals()[u"subtotal"_s].toString() == u"$25.00"_s);
    // Typed on the screen's keys, then Apply.
    typeKeys("SAVE5");
    s.tapItem(by(u"couponApply"_s));
    QTest::qWait(100);
    CHECK(s.pos.totals()[u"subtotal"_s].toString() == u"$20.00"_s);
    s.tapItem(by(u"couponDone"_s));
    QTest::qWait(60);
    CHECK_FALSE(by(u"couponSheet"_s));   // hidden
}

TEST_CASE("UI: a barcode scanner: an item rung in on the menu; a coupon on Pay; typing by hand isn't a scan", "[flow][ui][barcode]")
{
    Screen s(false, 1280, 800);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"draft-beer"_s}, {u"name"_s, u"Draft Beer"_s}, {u"barcodes"_s, u"012345678905"_s}}));
    core::PosSettings::Promotion coupon;
    coupon.id = "five-off";
    coupon.name = "Five Off";
    coupon.code = "SAVE5";
    coupon.amount = Money::fromCents(500);
    s.pos.shared()->settings.promotions.push_back(coupon);
    REQUIRE(s.pos.startCheck(core::CheckType::Quick));
    REQUIRE(s.c.jumpTo(u"menu-all"_s));
    QTest::qWait(100);
    const auto lines = [&] { return s.pos.lines().size(); };
    // A scanner: every key at once, then Enter.
    const auto scan = [&](const char *code) {
        for (const char *k = code; *k; ++k)
            QTest::keyClick(s.window, *k);
        QTest::keyClick(s.window, Qt::Key_Return);
        QTest::qWait(100);
    };
    scan("012345678905");
    REQUIRE(lines() == 1);
    CHECK(s.pos.lines()[0].toMap()[u"name"_s].toString().contains(u"Draft Beer"_s));
    // A person typing the same: not a scan (and too long for an item's number).
    for (const char *k = "012345678905"; *k; ++k) {
        QTest::keyClick(s.window, *k);
        QTest::qWait(80);
    }
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(100);
    CHECK(lines() == 1);
    // On Pay: a coupon, without opening anything.
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(100);
    scan("SAVE5");
    CHECK(s.pos.totals()[u"hasDiscount"_s] == true);
    // Something nobody has: said.
    QStringList said;
    QObject::connect(&s.pos, &app::PosService::notice, [&](const QString &m) { said << m; });
    scan("77777777");
    CHECK(said.join(u" "_s).contains(u"77777777"_s));
}

TEST_CASE("UI: a bar tab: opened holding the card; charged with a tip on Pay", "[ui][cards][hold]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    s.pos.entryKey(u"10000"_s);
    REQUIRE(s.pos.openDrawerSession());
    REQUIRE(s.pos.adminSave(u"terminals"_s, -1, {{u"name"_s, s.pos.terminalName()}, {u"cardReader"_s, u"simulated"_s}}));
    CardReader *reader = s.c.cardReader();
    reader->setSimulatedDelay(300);
    QQuickItem *root = s.window->contentItem();
    const auto by = [&](const QString &name) { return Screen::findBy(root, "objectName", name); };

    // New Tab: the name, then Open Tab & Hold Card: the reader takes the card.
    REQUIRE(s.c.jumpTo(u"new-tab"_s));
    QTest::qWait(60);
    for (const QString &k : {u"M"_s, u"i"_s, u"k"_s, u"e"_s})
        s.pos.textKey(k);
    QStringList said;
    QObject::connect(&s.pos, &app::PosService::notice, [&](const QString &m) { said << m; });
    s.c.activate(u"open-hold"_s);
    QTest::qWait(80);
    INFO(said.join(u" | "_s).toStdString());
    REQUIRE(by(u"cardWait"_s));
    CHECK(reader->amount() == u"Hold $50.00"_s);
    s.shot("tab-hold-waiting");
    QTest::qWait(600);
    CHECK_FALSE(by(u"cardWait"_s));
    CHECK(s.pos.checkInfo()[u"label"_s] == u"Mike"_s);
    CHECK(s.pos.checkInfo()[u"hold"_s].toMap()[u"card"_s] == u"Visa •••• 4242"_s);
    CHECK(s.pos.payments().isEmpty());

    // Drinks, then Pay: the panel says the card is held; Card on File… charges it.
    REQUIRE(s.pos.addItem(u"coffee"_s));
    REQUIRE(s.pos.addItem(u"coffee"_s));
    REQUIRE(s.c.jumpTo(u"settle"_s));
    QTest::qWait(80);
    REQUIRE(by(u"cardOnFileKey"_s));
    s.tapItem(by(u"cardOnFileKey"_s));
    QTest::qWait(80);
    REQUIRE(by(u"holdSheet"_s));
    s.shot("tab-hold-sheet");
    REQUIRE(by(u"holdTip-18"_s));
    s.tapItem(by(u"holdTip-18"_s));
    QTest::qWait(100);
    CHECK_FALSE(by(u"holdSheet"_s));
    CHECK(s.pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    REQUIRE(s.pos.payments().size() == 1);
    CHECK(s.pos.payments()[0].toMap()[u"tip"_s].toString() != QString());
    CHECK(s.pos.checkInfo()[u"hold"_s].toMap().isEmpty());
    REQUIRE(s.pos.closeCheck());
}

TEST_CASE("UI: Reports -> Forecast: a coming day, chosen by its key", "[flow][ui][forecast]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const QDate today = QDateTime::fromMSecsSinceEpoch(s.pos.shared()->now()).date();
    std::vector<core::Check> past;
    qint64 id = 9000;
    for (int week = 1; week <= 4; ++week) {
        for (int hour : {12, 13, 18, 19, 19, 20}) {
            core::Check c;
            c.id = ++id;
            c.status = core::CheckStatus::Closed;
            c.guests = 2;
            c.openedAt = QDateTime(today.addDays(2 - 7 * week), QTime(hour, 10)).toMSecsSinceEpoch();
            c.closedAt = c.openedAt + 3600 * 1000;
            for (const char *item : {"classic-burger", "cobb", "coffee"})
                c.addItem(*s.pos.findItem(QString::fromLatin1(item)));
            past.push_back(c);
        }
    }
    s.pos.shared()->history = [&](std::int64_t, std::int64_t) { return past; };
    REQUIRE(s.c.jumpTo(u"reports"_s));
    QTest::qWait(50);
    s.tapKey(u"Forecast"_s);
    QTest::qWait(50);
    s.tapItem(Screen::findBy(s.window->contentItem(), "objectName", u"forecastDay-2"_s));
    for (int i = 0; i < 100 && s.pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    QTest::qWait(80);
    const QVariantMap report = s.pos.rangeReport()[u"report"_s].toMap();
    CHECK(report[u"title"_s] == u"Forecast"_s);
    CHECK(report[u"rows"_s].toList().value(0).toMap()[u"cells"_s].toStringList().value(0).contains(u"4 past weeks"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Classic Burger"_s));
    s.shot("forecast");
}

TEST_CASE("UI: the Changes report: who changed what", "[flow][ui][changes]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    QVariantMap store = s.pos.adminRecords(u"store"_s)[0].toMap();
    store[u"storeName"_s] = u"Casa Ariel"_s;
    store[u"askReasons"_s] = false;
    REQUIRE(s.pos.adminSave(u"store"_s, 0, store));
    REQUIRE(s.pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, u"Cobb"_s}, {u"price"_s, u"13.25"_s}}));
    REQUIRE(s.pos.logPageEdits({}, {}, {u"Settle"_s}));
    REQUIRE(s.c.jumpTo(u"reports"_s));
    QTest::qWait(50);
    s.tapKey(u"Changes"_s);
    QTest::qWait(80);
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"Casa Ariel"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "text", u"$13.25"_s));
    s.shot("changes-report");
}

TEST_CASE("UI: Store Settings in sections: found by search, changes marked, kept when the page is left", "[flow][ui][settings]")
{
    Screen s;
    s.pos.shared()->settings.loyaltyEnabled = false;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    // The first section, not ninety settings at once.
    CHECK(text(u"Store name"_s));
    CHECK_FALSE(text(u"Kitchen stations"_s));
    s.shot("settings-sections");

    // Loyalty: "points per dollar" only once loyalty is on.
    QMetaObject::invokeMethod(by(u"adminList"_s), "positionViewAtEnd");
    QTest::qWait(60);
    s.tapItem(by(u"settingsSection-loyalty"_s));
    QTest::qWait(60);
    CHECK_FALSE(text(u"Store name"_s));
    REQUIRE(text(u"Loyalty points"_s));
    CHECK_FALSE(text(u"…points per dollar"_s));
    QQuickItem *loyalty = text(u"Loyalty points"_s)->parentItem()->parentItem();
    s.tapItem(Screen::findBy(loyalty, "objectName", u"onOff"_s));
    QTest::qWait(60);
    CHECK(text(u"…points per dollar"_s));
    CHECK(text(u"changed"_s));
    CHECK(text(u"1 changed"_s));                    // beside Loyalty in the sections
    CHECK(text(u"Save 1 change"_s));
    CHECK_FALSE(s.pos.shared()->settings.loyaltyEnabled);
    s.shot("settings-changed");

    // Left for another page and back: the change is still there to save.
    REQUIRE(s.c.jumpTo(u"manager"_s));
    QTest::qWait(80);
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    CHECK(text(u"Loyalty points"_s));               // the section it was in
    CHECK(text(u"Save 1 change"_s));
    s.tapItem(by(u"adminSave"_s));
    QTest::qWait(80);
    CHECK(s.pos.shared()->settings.loyaltyEnabled);
    CHECK_FALSE(text(u"changed"_s));

    // Search: across the sections, each found setting with its section's name.
    s.tapItem(by(u"settingsSearch"_s));
    for (const char *k = "kiosk color"; *k; ++k)
        QTest::keyClick(s.window, *k);
    QTest::qWait(60);
    CHECK(text(u"Background color"_s));
    CHECK(text(u"Text color"_s));
    CHECK(text(u"Self-order kiosk"_s));
    CHECK_FALSE(text(u"Loyalty points"_s));
    s.shot("settings-search");
    s.tapItem(by(u"settingsSearchClear"_s));
    QTest::qWait(60);
    CHECK(text(u"Loyalty points"_s));
}

TEST_CASE("UI: Store Settings on a phone: the sections, then one section's settings", "[flow][ui][settings]")
{
    Screen s(false, 390, 844, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    REQUIRE(by(u"settingsSection-tips"_s));
    CHECK_FALSE(text(u"Store name"_s));
    s.shot("settings-phone-sections");
    s.tapItem(by(u"settingsSection-tips"_s));
    QTest::qWait(60);
    CHECK(text(u"Tip-outs"_s));
    CHECK_FALSE(by(u"settingsSection-tips"_s));
    s.shot("settings-phone-tips");
    s.tapItem(by(u"adminBackToSections"_s));
    QTest::qWait(60);
    CHECK(by(u"settingsSection-tips"_s));
}

TEST_CASE("UI: Store Settings by touch: numbers with - and +, switches, lists as rows", "[flow][ui][settings]")
{
    Screen s;
    s.pos.shared()->settings.screenSaverMinutes = 0;
    s.pos.shared()->settings.tipOuts.clear();
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    // The setting whose name is `label` (its FieldEditor).
    const auto setting = [&](const QString &label) {
        QQuickItem *l = text(label);
        REQUIRE(l);
        return l->parentItem()->parentItem();
    };

    // A number: 0 has a name; + counts up.
    QMetaObject::invokeMethod(by(u"adminList"_s), "positionViewAtIndex", Q_ARG(int, 10), Q_ARG(int, 1));
    QTest::qWait(40);
    s.tapItem(by(u"settingsSection-screens"_s));
    QTest::qWait(60);
    QQuickItem *dim = setting(u"Dim the screen after (minutes)"_s);
    CHECK(Screen::findBy(dim, "objectName", u"stepValue"_s)->property("text").toString() == u"Never"_s);
    s.tapItem(Screen::findBy(dim, "objectName", u"stepUp"_s));
    s.tapItem(Screen::findBy(dim, "objectName", u"stepUp"_s));
    QTest::qWait(40);
    CHECK(Screen::findBy(dim, "objectName", u"stepValue"_s)->property("text").toString() == u"2"_s);

    // A list as rows: a tip-out to the bussers, 15% of tips.
    QMetaObject::invokeMethod(by(u"adminList"_s), "positionViewAtIndex", Q_ARG(int, 5), Q_ARG(int, 1));
    QTest::qWait(40);
    s.tapItem(by(u"settingsSection-tips"_s));
    QTest::qWait(60);
    QQuickItem *tipOuts = setting(u"Tip-outs"_s);
    // Scrolled to the tip-outs (the form's Flickable holds the column they're in).
    QQuickItem *flick = tipOuts;
    while (flick && !flick->inherits("QQuickFlickable"))
        flick = flick->parentItem();
    REQUIRE(flick);
    flick->setProperty("contentY", tipOuts->y());
    QTest::qWait(60);
    s.tapItem(Screen::findBy(tipOuts, "objectName", u"rowAdd"_s));
    QTest::qWait(60);
    s.shot("settings-rows-added");
    QQuickItem *percent = Screen::findBy(tipOuts, "objectName", u"rowCell"_s);
    REQUIRE(percent);
    s.tapItem(percent);
    for (const char *k = "15"; *k; ++k)
        QTest::keyClick(s.window, *k);
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(60);
    CHECK(text(u"Save 2 changes"_s));
    s.shot("settings-rows");
    s.tapItem(by(u"adminSave"_s));
    QTest::qWait(80);
    CHECK(s.pos.shared()->settings.screenSaverMinutes == 2);
    REQUIRE(s.pos.shared()->settings.tipOuts.size() == 1);
    CHECK(s.pos.shared()->settings.tipOuts[0].role == "busser");
    CHECK(s.pos.shared()->settings.tipOuts[0].percentBp == 1500);
    CHECK(s.pos.shared()->settings.tipOuts[0].basis == "tips");
}

TEST_CASE("UI: the tip pool is one table: a row a job, its share", "[flow][ui][settings]")
{
    Screen s;
    s.pos.shared()->settings.tipPool = {{"server", 100}, {"busser", 50}};
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QMetaObject::invokeMethod(by(u"adminList"_s), "positionViewAtIndex", Q_ARG(int, 5), Q_ARG(int, 1));
    QTest::qWait(40);
    s.tapItem(by(u"settingsSection-tips"_s));
    QTest::qWait(60);
    QQuickItem *pool = Screen::findBy(s.window->contentItem(), "text", u"Tip pool"_s)->parentItem()->parentItem();
    QQuickItem *flick = pool;
    while (flick && !flick->inherits("QQuickFlickable"))
        flick = flick->parentItem();
    REQUIRE(flick);
    flick->setProperty("contentY", pool->y());
    QTest::qWait(60);
    // Servers 100, bussers 50: in the jobs' usual order.
    const auto cells = [&] {
        std::vector<QQuickItem *> out;
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *i) {
            for (QQuickItem *c : i->childItems()) {
                if (!c->isVisible())
                    continue;
                if (c->objectName() == u"rowCell"_s)
                    out.push_back(c);
                walk(c);
            }
        };
        walk(pool);
        return out;
    };
    REQUIRE(cells().size() == 2);
    CHECK(cells()[0]->property("text").toString() == u"100"_s);
    CHECK(cells()[1]->property("text").toString() == u"50"_s);
    // Bartenders join at a full share: the new row starts on a job not in the pool.
    s.tapItem(Screen::findBy(pool, "objectName", u"rowAdd"_s));
    QTest::qWait(60);
    REQUIRE(cells().size() == 3);
    s.tapItem(cells()[2]);
    for (const char *k = "100"; *k; ++k)
        QTest::keyClick(s.window, *k);
    QTest::keyClick(s.window, Qt::Key_Return);
    QTest::qWait(60);
    s.shot("settings-tip-pool");
    s.tapItem(by(u"adminSave"_s));
    QTest::qWait(80);
    const std::map<std::string, int> want{{"server", 100}, {"bartender", 100}, {"busser", 50}};
    CHECK(s.pos.shared()->settings.tipPool == want);
}

TEST_CASE("UI: manager pages one after another: each opens as it should", "[flow][ui][settings]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    const auto go = [&](const char *page) {
        REQUIRE(s.c.jumpTo(QString::fromLatin1(page)));
        QTest::qWait(120);
    };
    go("admin-employees");
    CHECK(text(u"Choose something on the left, or Add."_s));
    go("admin-store");                    // one record: open at once
    CHECK(text(u"Store name"_s));
    CHECK_FALSE(text(u"Choose something on the left, or Add."_s));
    go("admin-taxes");
    CHECK_FALSE(text(u"Choose something on the left, or Add."_s));
    go("admin-tenders");                  // a list: nothing chosen yet
    CHECK(text(u"Choose something on the left, or Add."_s));
}

TEST_CASE("Store Settings: every setting is in a section", "[admin][settings]")
{
    Session s;
    for (const QVariant &f : s.pos.adminFields(u"store"_s))
        CHECK(f.toMap()[u"groupKey"_s].toString() != u"more"_s);
}

// Every Store Settings section, top and bottom, for review (hidden: "[.settingsTour]" with VTM_SHOTS).
TEST_CASE("Store Settings: a picture of every section", "[.settingsTour]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    QQuickItem *list = by(u"adminList"_s);
    REQUIRE(list);
    int i = 0;
    for (const QVariant &sec : list->property("model").toList()) {
        const QString key = sec.toMap()[u"key"_s].toString();
        QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, i), Q_ARG(int, 1 /* ListView.Beginning */));
        QTest::qWait(40);
        s.tapItem(by(u"settingsSection-"_s + key));
        QTest::qWait(80);
        const QByteArray n = QStringLiteral("tour-%1-%2").arg(i, 2, 10, QChar(u'0')).arg(key).toLatin1();
        s.shot(n.constData());
        if (QQuickItem *bar = by(u"adminFormScroll"_s)) {
            bar->setProperty("position", 1.0 - bar->property("size").toDouble());
            QTest::qWait(60);
            s.shot((n + "-end").constData());
        }
        ++i;
    }
}

TEST_CASE("UI: start pages are chosen from the floor plans", "[flow][ui][settings]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const auto names = [&](const QVariantList &choices) {
        QStringList out;
        for (const QVariant &c : choices)
            out << c.toMap()[u"text"_s].toString();
        return out;
    };
    // The tables, and the host's seating screen; not their phone versions.
    QStringList seed = names(s.c.pageChoices(u"floor"_s));
    seed.sort();
    CHECK(seed == QStringList{QString(), u"Bar"_s, u"Dining Area"_s, u"Main"_s, u"Patio"_s, u"Seating"_s});
    CHECK(names(s.c.pageChoices()).contains(u"Manager"_s));   // other page fields: every page

    // Another room the store draws itself: a page with a table map.
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    REQUIRE_FALSE(e->newPage(u"Terrace"_s, u"custom"_s, QString()).isEmpty());
    REQUIRE_FALSE(e->addZone(u"tableMap"_s).isEmpty());
    REQUIRE(s.c.leaveEditMode(true));
    QTest::qWait(60);
    const QStringList floors = names(s.c.pageChoices(u"floor"_s));
    CHECK(floors.contains(u"Terrace"_s));
    CHECK_FALSE(floors.contains(u"Tables (phone)"_s));
    CHECK_FALSE(floors.contains(u"Manager"_s));

    // Store Settings -> Start pages: the floor plans to choose from.
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    s.tapItem(Screen::findBy(s.window->contentItem(), "objectName", u"settingsSection-startPages"_s));
    QTest::qWait(60);
    QQuickItem *server = Screen::findBy(s.window->contentItem(), "text", u"Servers start on"_s)->parentItem()->parentItem();
    QQuickItem *combo = nullptr;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *i) {
        for (QQuickItem *c : i->childItems()) {
            if (!combo && c->inherits("QQuickComboBox"))
                combo = c;
            walk(c);
        }
    };
    walk(server);
    REQUIRE(combo);
    CHECK(combo->property("count").toInt() == 7);   // (the floor plan), Main, Dining Area, Bar, Patio, Seating, Terrace
}

TEST_CASE("UI: floor plans: Main and each room, tabs between them, the same table on two", "[flow][ui][floorplans]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    const auto table = [&](const QString &name) {
        // The table zone itself (its name is drawn inside it).
        QQuickItem *t = text(name);
        while (t && !QByteArray(t->metaObject()->className()).startsWith("ZoneItem"))
            t = t->parentItem();
        return t;
    };
    // T1 open on Main: open on the Dining Area plan too.
    REQUIRE(s.pos.selectTable(u"T1"_s) == app::PosService::TableNeedsGuests);
    s.pos.entryKey(u"2"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"coffee"_s);
    s.pos.releaseCheck();
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(150);
    for (const char *name : {"T1", "T7", "Bar 1", "H1", "P1", "Main", "Dining Area", "Bar", "Patio", "Entrance"})
        CHECK(text(QString::fromUtf8(name)));
    CHECK(table(u"P1"_s)->property("rotation").toDouble() == 45);
    s.shot("floor-main");

    // The tabs: Dining Area.
    s.tapItem(Screen::findBy(s.window->contentItem(), "zoneId", u"plan-dining"_s));
    QTest::qWait(150);
    CHECK(s.c.pageId() == u"floor-dining"_s);
    CHECK(text(u"T1"_s));
    CHECK_FALSE(text(u"P1"_s));
    CHECK(Screen::findBy(s.window->contentItem(), "zoneId", u"plan-dining"_s)->property("current").toBool());
    CHECK(text(u"1 check"_s) == nullptr);   // one check: the server's name shows instead
    s.shot("floor-dining");
    // Touching T1 here opens the same check.
    s.tapItem(table(u"T1"_s));
    QTest::qWait(150);
    CHECK(s.pos.hasCheck());
    CHECK(s.pos.checkInfo()[u"label"_s].toString().contains(u"T1"_s));
    s.pos.releaseCheck();

    for (const auto &[page, shot, has] : {std::tuple{"floor-bar", "floor-bar", "Bar 1"}, {"floor-patio", "floor-patio", "P4"}}) {
        REQUIRE(s.c.jumpTo(QString::fromLatin1(page)));
        QTest::qWait(150);
        CHECK(text(QString::fromLatin1(has)));
        s.shot(shot);
    }
    // Every table once (the host stand, the phone's list...).
    QStringList names;
    for (const QVariant &t : s.c.tables())
        names << t.toMap()[u"name"_s].toString();
    CHECK(names.size() == 16);
    CHECK(names.removeDuplicates() == 0);
}

TEST_CASE("UI: drawing a floor plan: a wall, turned by the menu and by its handle; a table pasted onto another plan", "[flow][ui][floorplans]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"floor-patio"_s));
    s.c.enterEditMode();
    QTest::qWait(80);
    EditorController *e = s.c.editor();
    const auto rot = [&](const QString &id) {
        for (const QVariant &g : e->geometry())
            if (g.toMap()[u"id"_s] == id)
                return g.toMap()[u"rot"_s].toInt();
        return -1;
    };
    const auto rectOf = [&](const QString &id) {
        for (const QVariant &g : e->geometry())
            if (g.toMap()[u"id"_s] == id)
                return g.toMap();
        return QVariantMap();
    };

    // A wall from + Floor Plan: under the tables, turned 90 then back 15.
    const QString wall = e->addZone(u"decor:wall"_s);
    REQUIRE_FALSE(wall.isEmpty());
    // Over the other pieces (the patio's floor, its fence...), under the tables.
    const QVariantList order = e->geometry();
    const auto at = [&](const QString &id) {
        for (int i = 0; i < order.size(); ++i)
            if (order[i].toMap()[u"id"_s] == id)
                return i;
        return -1;
    };
    CHECK(at(wall) > at(u"area-1"_s));
    CHECK(at(wall) < at(u"table-p1"_s));
    e->rotateSelection(90);
    CHECK(rot(wall) == 90);
    e->rotateSelection(-15);
    CHECK(rot(wall) == 75);
    e->undo();
    CHECK(rot(wall) == 90);
    e->setRotation(0);
    CHECK(rot(wall) == 0);
    // Between the plants and P2, where turning it up and down crosses no table.
    e->commitRects({{wall, QVariantMap{{u"x"_s, 560}, {u"y"_s, 600}, {u"w"_s, 480}, {u"h"_s, 24}}}});

    // Its round handle: dragged to the right of its middle, it turns 90.
    QTest::qWait(60);
    QQuickItem *knob = Screen::findBy(s.window->contentItem(), "objectName", u"rotateHandle"_s);
    REQUIRE(knob);
    const QVariantMap g = rectOf(wall);
    const QPointF mid(g[u"x"_s].toDouble() + g[u"w"_s].toDouble() / 2, g[u"y"_s].toDouble() + g[u"h"_s].toDouble() / 2);
    const QPoint from = knob->mapToScene(QPointF(knob->width() / 2, knob->height() / 2)).toPoint();
    const QPoint to = s.at(mid.x() + 300, mid.y());
    QTest::mousePress(s.window, Qt::LeftButton, {}, from);
    for (int i = 1; i <= 10; ++i)
        QTest::mouseMove(s.window, from + (to - from) * i / 10);
    QTest::mouseRelease(s.window, Qt::LeftButton, {}, to);
    QTest::qWait(60);
    CHECK(rot(wall) == 90);
    s.shot("floor-wall-turned");

    // A turned piece is picked where it shows: above its middle (it runs up and down now).
    e->clearSelection();
    s.tapCanvas(mid.x(), mid.y() - 180);
    CHECK(e->selection() == QStringList{wall});

    // T1 copied from Main and pasted here: still T1 (the same table); pasted again here: a new one.
    REQUIRE(s.c.showPage(u"tables"_s));
    QTest::qWait(60);
    e->select(u"table-t1"_s, false);
    e->copy();
    REQUIRE(s.c.showPage(u"floor-patio"_s));
    QTest::qWait(60);
    e->paste();
    const QStringList pasted = e->selection();
    REQUIRE(pasted.size() == 1);
    CHECK(rectOf(pasted.first())[u"label"_s] == u"T1"_s);
    e->paste();
    CHECK(rectOf(e->selection().first())[u"label"_s] != u"T1"_s);
    REQUIRE(s.c.leaveEditMode(false));
}

// Main's ready-made arrangements, for review (hidden: "[.floorArrangements]" with VTM_SHOTS).
TEST_CASE("Floor plans: a picture of each arrangement of Main", "[.floorArrangements]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    s.c.enterEditMode();
    EditorController *e = s.c.editor();
    for (const QVariant &a : e->arrangements()) {
        const QString id = a.toMap()[u"id"_s].toString();
        REQUIRE(e->useArrangement(id));
        REQUIRE(s.c.leaveEditMode(true));
        QTest::qWait(150);
        s.shot(("arrangement-" + id).toLatin1().constData());
        s.c.enterEditMode();
        e = s.c.editor();
    }
    REQUIRE(e->useArrangement(u"classic"_s));
    REQUIRE(s.c.leaveEditMode(true));
}

TEST_CASE("Host stand: the floor plan as drawn, a tab for each, picked by touch, zoomed in", "[ui][host][floorplans]")
{
    for (const bool phone : {false, true}) {
        Screen s(false, phone ? 412 : 1600, phone ? 870 : 900, phone ? u"phone"_s : QString());
        REQUIRE(s.pos.loginWithPin(u"1234"_s));
        REQUIRE(s.c.jumpTo(u"seating"_s));
        QTest::qWait(80);
        const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
        // Main: every table where the plan has it, walls and all; T1 left of T2, the patio below.
        REQUIRE(by(u"host-T1"_s));
        REQUIRE(by(u"host-P1"_s));
        const auto at = [&](const QString &t) {
            QQuickItem *i = by(u"host-"_s + t);
            return i->mapToScene(QPointF(i->width() / 2, i->height() / 2));
        };
        CHECK(at(u"T1"_s).x() < at(u"T2"_s).x());
        CHECK(at(u"P1"_s).y() > at(u"T5"_s).y());
        CHECK(by(u"host-P1"_s)->property("rotation").toDouble() == 45);
        // The Patio tab: just its tables, bigger.
        const auto shown = [&](const QString &t) {
            QQuickItem *i = by(u"host-"_s + t);
            return i->mapRectToScene(QRectF(0, 0, i->width(), i->height())).width();
        };
        const double mainWidth = shown(u"P1"_s);
        s.tapItem(by(u"hostPlan-floor-patio"_s));
        QTest::qWait(60);
        CHECK_FALSE(by(u"host-T1"_s));
        REQUIRE(by(u"host-P4"_s));
        CHECK(shown(u"P1"_s) > mainWidth);
        s.tapItem(by(u"host-P4"_s));
        CHECK(Screen::findBy(by(u"host-P4"_s), "text", u"✓ P4"_s));
        s.shot(phone ? "host-phone-patio" : "host-patio");
        // Zoomed in (two fingers): still picked by a touch, the plan scrolled to it.
        s.tapItem(by(u"hostPlan-tables"_s));
        QTest::qWait(60);
        by(u"hostFloor"_s)->setProperty("zoom", 2.5);
        QTest::qWait(60);
        s.tapItem(by(u"host-Bar 3"_s));
        CHECK(Screen::findBy(by(u"host-Bar 3"_s), "text", u"✓ Bar 3"_s));
        // The plan picked stays: back on this screen, Main again (the last tab touched).
        REQUIRE(s.c.jumpTo(u"tables"_s));
        REQUIRE(s.c.jumpTo(u"seating"_s));
        QTest::qWait(60);
        CHECK(by(u"host-T1"_s));
    }
}

// The floor plans on a phone, for review (hidden: "[.phoneFloors]" with VTM_SHOTS).
TEST_CASE("Floor plans on a phone: a picture of each", "[.phoneFloors]")
{
    Screen s(false, 412, 870, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    for (const char *page : {"tables", "floor-dining", "floor-bar", "floor-patio"}) {
        REQUIRE(s.c.jumpTo(QString::fromLatin1(page)));
        QTest::qWait(150);
        s.shot(("phone-" + std::string(page)).c_str());
    }
}

TEST_CASE("Phone: the floor plans with tabs; a table touched opens; a room's own page starts on it; moving a check", "[ui][phone][floorplans]")
{
    Screen s(false, 412, 870, u"phone"_s);
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"tables"_s));
    QTest::qWait(100);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    // Main, turned to fill the phone: the patio (south) on the left of the dining room.
    REQUIRE(by(u"floor-T1"_s));
    REQUIRE(by(u"floor-P1"_s));
    const auto at = [&](const QString &t) {
        QQuickItem *i = by(u"floor-"_s + t);
        return i->mapToScene(QPointF(i->width() / 2, i->height() / 2));
    };
    CHECK(at(u"P1"_s).x() < at(u"T5"_s).x());
    // T2: guests, then the menu, as on a big screen.
    s.tapItem(by(u"floor-T2"_s));
    QTest::qWait(80);
    CHECK(s.c.pageId() == u"guest-count"_s);
    s.c.goBack();
    QTest::qWait(80);
    // The Bar tab: just the bar, bigger.
    s.tapItem(by(u"floorPlan-floor-bar"_s));
    QTest::qWait(60);
    CHECK_FALSE(by(u"floor-T2"_s));
    REQUIRE(by(u"floor-Bar 2"_s));

    // A server who starts on the Dining Area: its phone page, on its plan.
    REQUIRE(s.c.jumpTo(u"floor-dining"_s));
    QTest::qWait(100);
    CHECK(by(u"floor-T5"_s));
    CHECK_FALSE(by(u"floor-P1"_s));
    CHECK(by(u"floorPlan-floor-dining"_s));

    // An open check moved to P3 by touching P3 on the plan.
    REQUIRE(s.pos.selectTable(u"T4"_s) == app::PosService::TableNeedsGuests);
    s.pos.entryKey(u"2"_s);
    REQUIRE(s.pos.startCheck(core::CheckType::DineIn));
    s.pos.addItem(u"coffee"_s);
    REQUIRE(s.c.jumpTo(u"move-table"_s));
    QTest::qWait(100);
    s.tapItem(by(u"floorPlan-floor-patio"_s));
    QTest::qWait(60);
    s.tapItem(by(u"floor-P3"_s));
    QTest::qWait(80);
    CHECK(s.pos.checkInfo()[u"label"_s].toString().contains(u"P3"_s));
    s.shot("phone-moved");
}

TEST_CASE("UI: Store Settings -> About: the version, this screen, the license", "[flow][ui][settings]")
{
    Screen s;
    REQUIRE(s.pos.loginWithPin(u"1234"_s));
    REQUIRE(s.c.jumpTo(u"admin-store"_s));
    QTest::qWait(120);
    const auto by = [&](const QString &name) { return Screen::findBy(s.window->contentItem(), "objectName", name); };
    const auto text = [&](const QString &t) { return Screen::findBy(s.window->contentItem(), "text", t); };
    // Found by searching "version" too.
    s.tapItem(by(u"settingsSearch"_s));
    for (const char *k = "version"; *k; ++k)
        QTest::keyClick(s.window, *k);
    QTest::qWait(60);
    CHECK(text(u"ViewTouch version"_s));
    s.tapItem(by(u"settingsSearchClear"_s));
    QMetaObject::invokeMethod(by(u"adminList"_s), "positionViewAtEnd");
    QTest::qWait(40);
    s.tapItem(by(u"settingsSection-about"_s));
    QTest::qWait(80);
    CHECK(text(u"This screen's version"_s));
    QStringList shown;
    std::function<void(QQuickItem *)> walk = [&](QQuickItem *i) {
        for (QQuickItem *c : i->childItems()) {
            if (c->isVisible() && c->objectName() == u"infoText"_s)
                shown << c->property("text").toString();
            walk(c);
        }
    };
    walk(s.window->contentItem());
    CHECK(shown.join(u'\n').contains(vt::net::appBuild().version));     // this screen's build: 0.9.1283
    CHECK(shown.join(u'\n').contains(u"1600 × 900, standard pages"_s));
    CHECK(shown.join(u'\n').contains(u"GNU General Public License, version 3"_s));
    CHECK_FALSE(text(u"Save"_s));                                          // nothing to change here
    s.shot("settings-about");
}
