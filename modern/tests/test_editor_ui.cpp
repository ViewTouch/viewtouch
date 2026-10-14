#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "qt_catch.hh"

#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;
using vt::layout::Layout;

// Drives the real QML UI (Main.qml, EditLayer, ZoneItem) with synthetic
// mouse and keyboard input, offscreen.

namespace {

class Ui {
public:
    explicit Ui(const QString &page, bool edit = true)
        : controller(seed())
    {
        controller.showPage(page);
        if (edit)
            controller.enterEditMode();
        engine.setInitialProperties({
            {u"controller"_s, QVariant::fromValue(&controller)},
            {u"width"_s, 1600},
            {u"height"_s, 900},
        });
        engine.loadFromModule("ViewTouch", "Main");
        REQUIRE_FALSE(engine.rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        REQUIRE(window);
        REQUIRE(QTest::qWaitForWindowExposed(window));
        settle();
    }

    static Layout seed()
    {
        auto l = vt::test::loadTestLayout();
        REQUIRE(l);
        return *l;
    }

    void settle() { QTest::qWait(50); }

    // Canvas coordinates -> window coordinates, through the scaled surface.
    QPoint at(qreal cx, qreal cy)
    {
        auto *surface = window->findChild<QQuickItem *>(u"pageSurface"_s);
        REQUIRE(surface);
        return surface->mapToScene(QPointF(cx, cy)).toPoint();
    }

    void drag(QPointF from, QPointF to, Qt::KeyboardModifiers mods = {})
    {
        QTest::mousePress(window, Qt::LeftButton, mods, at(from.x(), from.y()));
        const int steps = 8;
        for (int i = 1; i <= steps; ++i) {
            const QPointF p = from + (to - from) * (qreal(i) / steps);
            QTest::mouseMove(window, at(p.x(), p.y()), 10);
        }
        QTest::mouseRelease(window, Qt::LeftButton, mods, at(to.x(), to.y()));
        settle();
    }

    void click(QPointF p, Qt::KeyboardModifiers mods = {})
    {
        QTest::mouseClick(window, Qt::LeftButton, mods, at(p.x(), p.y()));
        settle();
    }

    QRect rect(const QString &zone) const
    {
        const auto *z = controller.activeLayout().page(controller.pageId())->zone(zone);
        REQUIRE(z);
        return z->rect;
    }

    LayoutController controller;
    QQmlApplicationEngine engine;
    QQuickWindow *window = nullptr;
};

} // namespace

TEST_CASE("UI: click selects, drag moves with snapping, undo restores", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    EditorController *e = ui.controller.editor();
    const QRect before = ui.rect(u"item-1"_s);   // 592,192 316x180

    ui.click({700, 280});
    CHECK(e->selection() == QStringList{u"item-1"_s});

    ui.drag({700, 280}, {700 + 101, 280 + 37});
    const QRect after = ui.rect(u"item-1"_s);
    CHECK(after.size() == before.size());
    CHECK(std::abs(after.x() - (before.x() + 101)) <= 16);   // grid or guide snap
    CHECK(std::abs(after.y() - (before.y() + 37)) <= 16);
    CHECK(after != before);
    CHECK(e->undoText() == u"Move / resize"_s);

    e->undo();
    CHECK(ui.rect(u"item-1"_s) == before);
}

TEST_CASE("UI: a click without movement does not move the zone", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    const QRect before = ui.rect(u"item-2"_s);
    ui.click({1000, 280});
    ui.click({1001, 281});
    CHECK(ui.rect(u"item-2"_s) == before);
    CHECK_FALSE(ui.controller.editor()->canUndo());
}

TEST_CASE("UI: bottom-right handle resizes", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    ui.click({700, 280});
    const QRect before = ui.rect(u"item-1"_s);
    const QPointF corner(before.right() + 1, before.bottom() + 1);

    ui.drag(corner, corner + QPointF(100, 50));
    const QRect after = ui.rect(u"item-1"_s);
    CHECK(after.topLeft() == before.topLeft());
    CHECK(std::abs(after.width() - (before.width() + 100)) <= 16);
    CHECK(std::abs(after.height() - (before.height() + 50)) <= 16);
}

TEST_CASE("UI: rubber band selects touched zones only", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    ui.drag({1250, 182}, {1700, 400});
    QStringList sel = ui.controller.editor()->selection();
    sel.sort();
    CHECK(sel == QStringList{u"item-3"_s, u"item-4"_s});

    // Shift-click adds and removes.
    ui.click({700, 280}, Qt::ShiftModifier);
    CHECK(ui.controller.editor()->selection().size() == 3);
    ui.click({700, 280}, Qt::ShiftModifier);
    CHECK(ui.controller.editor()->selection().size() == 2);

    // Clicking empty canvas clears.
    ui.click({1250, 900});
    CHECK(ui.controller.editor()->selection().isEmpty());
}

TEST_CASE("UI: template zones are locked; double-click opens the template", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    ui.click({1780, 1020});   // "Pay" belongs to order-template
    CHECK(ui.controller.editor()->selection().isEmpty());
    CHECK(ui.controller.editor()->notice().contains(u"template"_s));

    QTest::mouseDClick(ui.window, Qt::LeftButton, {}, ui.at(1780, 1020));
    ui.settle();
    CHECK(ui.controller.pageId() == u"order-template"_s);
}

TEST_CASE("UI: keyboard shortcuts edit the selection", "[ui]")
{
    Ui ui(u"items-burgers"_s);
    EditorController *e = ui.controller.editor();
    ui.click({700, 280});
    const QRect before = ui.rect(u"item-1"_s);

    QTest::keyClick(ui.window, Qt::Key_Right);
    QTest::keyClick(ui.window, Qt::Key_Right);
    CHECK(ui.rect(u"item-1"_s) == before.translated(16, 0));
    QTest::keyClick(ui.window, Qt::Key_Down, Qt::ShiftModifier);
    CHECK(ui.rect(u"item-1"_s) == before.translated(16, 1));

    QTest::keyClick(ui.window, Qt::Key_D, Qt::ControlModifier);
    CHECK(e->selection() == QStringList{u"item-1-2"_s});

    QTest::keyClick(ui.window, Qt::Key_Delete);
    CHECK_FALSE(ui.controller.activeLayout().page(u"items-burgers"_s)->zone(u"item-1-2"_s));

    QTest::keyClick(ui.window, Qt::Key_Z, Qt::ControlModifier);
    CHECK(ui.controller.activeLayout().page(u"items-burgers"_s)->zone(u"item-1-2"_s));
}

TEST_CASE("UI: touching buttons outside edit mode still navigates", "[ui]")
{
    Ui ui(u"index-lunch"_s, false);
    ui.click({592 + 300, 812 + 76});    // "Everything"
    CHECK(ui.controller.pageId() == u"menu-all"_s);

    // F1 enters edit mode from the running app.
    QTest::keyClick(ui.window, Qt::Key_F1);
    CHECK(ui.controller.editing());
    QTest::keyClick(ui.window, Qt::Key_F1);   // nothing changed -> leaves directly
    CHECK_FALSE(ui.controller.editing());
}

TEST_CASE("UI: a finger's tap that wanders a little selects without moving; a corner is easy to grab", "[ui][touchedit]")
{
    Ui ui(u"items-burgers"_s);
    EditorController *e = ui.controller.editor();
    const QRect before = ui.rect(u"item-1"_s);
    const QPoint at = ui.at(700, 280);
    QTest::mousePress(ui.window, Qt::LeftButton, {}, at);
    QTest::mouseMove(ui.window, at + QPoint(4, 3), 10);
    QTest::mouseMove(ui.window, at + QPoint(6, -2), 10);
    QTest::mouseRelease(ui.window, Qt::LeftButton, {}, at + QPoint(6, -2));
    ui.settle();
    CHECK(e->selection() == QStringList{u"item-1"_s});
    CHECK(ui.rect(u"item-1"_s) == before);
    CHECK_FALSE(e->canUndo());

    // The bottom-right corner, touched a finger's width off (screen pixels).
    const QPoint corner = ui.at(before.right() + 1, before.bottom() + 1) + QPoint(12, 12);
    QTest::mousePress(ui.window, Qt::LeftButton, {}, corner);
    for (int i = 1; i <= 8; ++i)
        QTest::mouseMove(ui.window, corner + QPoint(6 * i, 4 * i), 10);
    QTest::mouseRelease(ui.window, Qt::LeftButton, {}, corner + QPoint(48, 32));
    ui.settle();
    const QRect after = ui.rect(u"item-1"_s);
    CHECK(after.topLeft() == before.topLeft());     // resized, not moved
    CHECK(after.width() > before.width());
    CHECK(after.height() > before.height());
}
