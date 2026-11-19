#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <utility>

using namespace Qt::StringLiterals;
using namespace vt;

// The order screen on a phone: what was just added shows on the check without
// opening it or scrolling (a Stripe S700/S710: 1080 x 1920, 360 x 640 points).

namespace {

void addSome(app::PosService &pos)
{
    for (const char *id : {"classic-burger", "fries", "cobb", "pancakes", "soda", "coffee"})
        REQUIRE(pos.addItem(QString::fromLatin1(id)));
}

// A shown item by its objectName (the page's widgets are in the item tree).
QQuickItem *shown(QQuickItem *root, const QString &name)
{
    for (QQuickItem *item : root->childItems()) {
        if (!item->isVisible())
            continue;
        if (item->objectName() == name)
            return item;
        if (QQuickItem *hit = shown(item, name))
            return hit;
    }
    return nullptr;
}

} // namespace

TEST_CASE("Phone order screen: the check shows its lines while ordering", "[ui][phone][phoneorder]")
{
    const QString style = GENERATE(u"classic"_s, u"modern"_s);
    // A Stripe S700/S710 (16:9), and a tall phone (a Galaxy A55: 1080 x 2340).
    const auto [wide, tall] = GENERATE(std::pair{360, 640}, std::pair{411, 890});
    auto layout = test::loadTestLayout();
    REQUIRE(layout);
    if (style != u"classic")   // (the starter pages are Classic)
        REQUIRE(layout->useStyle(style, layout::Theme{}));
    app::PosService pos(test::seedPosData(), nullptr);
    LayoutController c(std::move(*layout));
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    c.setFormFactorOverride(u"phone"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    addSome(pos);
    REQUIRE(c.showPage(u"index-lunch"_s));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)}, {u"width"_s, wide}, {u"height"_s, tall}});
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);
    if (const QByteArray shots = qgetenv("VTM_SHOTS"); !shots.isEmpty())
        window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/phone-order-"_s + style + u'-' + QString::number(tall) + u".png"_s);

    // The lines have room: about three a finger tall (48 each) on the screen.
    auto *lines = shown(window->contentItem(), u"checkLines"_s);
    REQUIRE(lines);
    const QRectF room = lines->mapRectToScene(QRectF(0, 0, lines->width(), lines->height()));
    INFO(style.toStdString() << ": the lines get " << room.height() << " of " << tall);
    CHECK(room.height() >= 130);
    // ...and the newest (Coffee, just added) is in view, not below.
    const double end = lines->property("originY").toDouble() + lines->property("contentHeight").toDouble() - lines->height();
    CHECK(lines->property("contentY").toDouble() >= end - 2);
    // The total is on the top row; the menu, Send and Pay are all on the screen.
    auto *total = shown(window->contentItem(), u"foldedTotal"_s);
    REQUIRE(total);
    CHECK(total->isVisible());
    CHECK(c.canvasSize().height() <= c.canvasSize().width() * tall / wide + 8);
}
