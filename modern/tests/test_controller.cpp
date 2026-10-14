#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "qt_catch.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

using namespace Qt::StringLiterals;

namespace {

LayoutController seedController()
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    return LayoutController(std::move(*layout));
}

// The starter pages plus the older way to ask for modifiers: the Classic
// Burger button runs a chain of pages (Temperature, then Side), as a store
// can still build in the page editor.
vt::layout::Layout withModifierPages()
{
    auto seed = vt::test::loadTestLayout();
    REQUIRE(seed);
    QJsonObject root = seed->toJson();
    QJsonArray pages = root.value(u"pages").toArray();
    auto modifierPage = [](const QString &id, const QStringList &options) {
        QJsonArray zones;
        for (int i = 0; i < options.size(); ++i) {
            const QString item = options[i].toLower().replace(u' ', u'-');
            zones.append(QJsonObject{
                {u"id"_s, u"opt-%1"_s.arg(i + 1)}, {u"kind"_s, u"button"_s}, {u"label"_s, options[i]},
                {u"rect"_s, QJsonObject{{u"x"_s, 592 + i * 260}, {u"y"_s, 192}, {u"w"_s, 240}, {u"h"_s, 180}}},
                {u"actions"_s, QJsonArray{QJsonObject{{u"type"_s, u"addItem"_s}, {u"item"_s, item}},
                                          QJsonObject{{u"type"_s, u"jump"_s}, {u"mode"_s, u"sequence"_s}}}}});
        }
        zones.append(QJsonObject{
            {u"id"_s, u"skip"_s}, {u"kind"_s, u"button"_s}, {u"label"_s, u"Skip"_s},
            {u"rect"_s, QJsonObject{{u"x"_s, 1480}, {u"y"_s, 800}, {u"w"_s, 424}, {u"h"_s, 120}}},
            {u"actions"_s, QJsonArray{QJsonObject{{u"type"_s, u"jump"_s}, {u"mode"_s, u"sequence"_s}}}}});
        return QJsonObject{{u"schemaVersion"_s, 1}, {u"id"_s, id}, {u"name"_s, id}, {u"kind"_s, u"modifier"_s},
                           {u"canvas"_s, QJsonObject{{u"w"_s, 1920}, {u"h"_s, 1080}}}, {u"grid"_s, 8},
                           {u"templateId"_s, u"order-template"_s}, {u"zones"_s, zones}};
    };
    pages.append(modifierPage(u"mod-temperature"_s, {u"Rare"_s, u"Medium Rare"_s, u"Medium"_s}));
    pages.append(modifierPage(u"mod-side"_s, {u"Fries"_s, u"Onion Rings"_s}));
    for (QJsonValueRef v : pages) {
        QJsonObject page = v.toObject();
        if (page.value(u"id").toString() != u"items-burgers")
            continue;
        QJsonArray zones = page.value(u"zones").toArray();
        for (QJsonValueRef z : zones) {
            QJsonObject zone = z.toObject();
            if (zone.value(u"id").toString() == u"item-1")
                zone[u"actions"_s] = QJsonArray{QJsonObject{
                    {u"type"_s, u"addItem"_s}, {u"item"_s, u"classic-burger"_s},
                    {u"modifierSequence"_s, QJsonArray{u"mod-temperature"_s, u"mod-side"_s}}}};
            z = zone;
        }
        page[u"zones"_s] = zones;
        v = page;
    }
    root[u"pages"_s] = pages;
    QStringList errors;
    auto layout = vt::layout::Layout::fromJson(root, &errors);
    INFO(errors.join(u'\n').toStdString());
    REQUIRE(layout);
    return *layout;
}

// Row of the zone in the controller's model, or -1.
int rowOf(LayoutController &c, const QString &zoneId)
{
    ZoneModel *m = c.zones();
    for (int r = 0; r < m->rowCount(); ++r) {
        if (m->data(m->index(r), ZoneModel::ZoneIdRole).toString() == zoneId)
            return r;
    }
    return -1;
}

QVariant role(LayoutController &c, const QString &zoneId, int role)
{
    const int r = rowOf(c, zoneId);
    REQUIRE(r >= 0);
    return c.zones()->data(c.zones()->index(r), role);
}

} // namespace

TEST_CASE("Meal period from time of day", "[controller]")
{
    CHECK(LayoutController::mealPeriodAt(QTime(7, 30)) == u"breakfast"_s);
    CHECK(LayoutController::mealPeriodAt(QTime(12, 0)) == u"lunch"_s);
    CHECK(LayoutController::mealPeriodAt(QTime(19, 0)) == u"dinner"_s);
}

TEST_CASE("Seed navigation without a POS session", "[controller]")
{
    // Layout-only use (no PosService): commands are no-ops, jumps still work.
    LayoutController c(withModifierPages());
    c.setMealPeriod(u"lunch"_s);
    QSignalSpy items(&c, &LayoutController::itemAdded);

    CHECK(c.pageId() == u"login"_s);
    REQUIRE(c.showPage(u"guest-count"_s));
    c.activate(u"start"_s);                     // startCheck (no-op), then index jump
    CHECK(c.pageId() == u"index-lunch"_s);

    // Inherited template zones are present and flagged; the lunch tab is lit.
    CHECK(role(c, u"order-list"_s, ZoneModel::InheritedRole).toBool());
    CHECK_FALSE(role(c, u"title"_s, ZoneModel::InheritedRole).toBool());
    CHECK(role(c, u"tab-lunch"_s, ZoneModel::CurrentRole).toBool());
    CHECK_FALSE(role(c, u"tab-dinner"_s, ZoneModel::CurrentRole).toBool());

    REQUIRE(c.jumpTo(u"items-burgers"_s));   // a page of buttons placed by hand (a fixture)
    CHECK(c.pageId() == u"items-burgers"_s);
    CHECK(role(c, u"tab-lunch"_s, ZoneModel::CurrentRole).toBool());   // via last index

    // A burger button with a page chain: Temperature -> Side -> back to the burger page.
    c.activate(u"item-1"_s);
    CHECK(c.pageId() == u"mod-temperature"_s);
    c.activate(u"opt-3"_s);                     // "Medium", then continue sequence
    CHECK(c.pageId() == u"mod-side"_s);
    c.activate(u"skip"_s);
    CHECK(c.pageId() == u"items-burgers"_s);

    REQUIRE(items.size() == 2);
    CHECK(items[0][0].toString() == u"classic-burger"_s);
    CHECK(items[1][0].toString() == u"medium"_s);

    // Hotkey on an inherited zone: "p" = Pay.
    CHECK(c.triggerHotkey(u"P"_s));
    CHECK(c.pageId() == u"settle"_s);
    c.activate(u"done"_s);
    CHECK(c.pageId() == u"items-burgers"_s);

    c.activate(u"tab-categories"_s);
    CHECK(c.pageId() == u"index-lunch"_s);

    c.goHome();
    CHECK(c.pageId() == u"login"_s);
    CHECK_FALSE(c.canGoBack());
}

TEST_CASE("Resolved styles reach the model", "[controller]")
{
    LayoutController c = seedController();
    c.jumpTo(u"items-burgers"_s);

    const QVariantMap normal = role(c, u"flow-send"_s, ZoneModel::StyleNormalRole).toMap();
    CHECK(normal.value(u"fill"_s).toString() == u"#1f8a4c"_s);            // zone
    CHECK(normal.value(u"font"_s).toString() == u"DejaVu Sans"_s);        // theme
    const QVariantMap selected = role(c, u"flow-send"_s, ZoneModel::StyleSelectedRole).toMap();
    CHECK(selected.value(u"frame"_s).toString() == u"inset"_s);
    const QVariantMap title = role(c, u"title"_s, ZoneModel::StyleNormalRole).toMap();
    CHECK(title.value(u"frame"_s).toString() == u"none"_s);               // theme kind: label
}

TEST_CASE("Unknown targets and disabled zones do nothing", "[controller]")
{
    LayoutController c = seedController();
    CHECK_FALSE(c.jumpTo(u"nowhere"_s));
    CHECK(c.pageId() == u"login"_s);

    REQUIRE(c.jumpTo(u"library"_s));
    QSignalSpy pages(&c, &LayoutController::pageChanged);
    c.activate(u"lib-disabled"_s);
    c.activate(u"no-such-zone"_s);
    CHECK(pages.isEmpty());
    CHECK(c.pageId() == u"library"_s);
}
