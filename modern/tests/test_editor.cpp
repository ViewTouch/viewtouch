#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "app/layout_editor.hh"
#include "layout/json_path.hh"
#include "layout/schema.hh"
#include "qt_catch.hh"

#include <QJsonArray>

using namespace Qt::StringLiterals;
using vt::app::LayoutEditor;
using namespace vt::layout;

namespace {

Layout seed()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

QRect rectOf(const LayoutEditor &e, const QString &page, const QString &zone)
{
    const Zone *z = e.layout().page(page)->zone(zone);
    REQUIRE(z);
    return z->rect;
}

} // namespace

TEST_CASE("jsonSet / jsonGet with nested paths", "[editor]")
{
    QJsonObject o;
    jsonSet(o, u"style.normal.fill", u"#fff"_s);
    jsonSet(o, u"style.normal.fontSize", 30);
    CHECK(jsonGet(o, u"style.normal.fill").toString() == u"#fff"_s);
    CHECK(jsonGet(o, u"style.selected.fill").isUndefined());
    CHECK(jsonGet(o, u"missing.deep.path").isUndefined());

    jsonSet(o, u"style.normal.fill", QJsonValue(QJsonValue::Undefined));
    jsonSet(o, u"style.normal.fontSize", QJsonValue(QJsonValue::Undefined));
    CHECK(o.isEmpty());   // empty parents are pruned
}

TEST_CASE("Every zone kind has a schema with geometry fields", "[editor]")
{
    for (const QString &kind : schema::allKinds()) {
        INFO(kind.toStdString());
        const QJsonArray fields = schema::zoneFields(kind);
        QStringList paths;
        for (const QJsonValue &f : fields)
            paths << f[u"path"].toString();
        CHECK(paths.contains(u"rect.x"_s));
        CHECK(paths.contains(u"style.normal.fill"_s));
        CHECK(paths.contains(u"actions"_s) == (kind == u"button" || kind == u"image"));
    }
}

TEST_CASE("Add, undo, redo", "[editor]")
{
    LayoutEditor e(seed());
    int changes = 0;
    e.setChangedCallback([&] { ++changes; });
    CHECK_FALSE(e.isDirty());

    const QString id = e.addZone(u"login"_s, u"button"_s);
    REQUIRE(id == u"button"_s);
    CHECK(e.isDirty());
    CHECK(changes == 1);
    const Zone *z = e.layout().page(u"login"_s)->zone(id);
    REQUIRE(z);
    CHECK(z->label == u"New Button"_s);
    CHECK(z->rect.size() == QSize(240, 120));

    // A second one does not land on the first.
    const QString id2 = e.addZone(u"login"_s, u"button"_s);
    CHECK(id2 == u"button-2"_s);
    CHECK(rectOf(e, u"login"_s, id2).topLeft() != rectOf(e, u"login"_s, id).topLeft());

    e.undoStack()->undo();
    e.undoStack()->undo();
    CHECK_FALSE(e.layout().page(u"login"_s)->zone(id));
    CHECK_FALSE(e.isDirty());
    e.undoStack()->redo();
    CHECK(e.layout().page(u"login"_s)->zone(id));
    CHECK(changes == 5);
}

TEST_CASE("No-op edits do not create undo steps", "[editor]")
{
    LayoutEditor e(seed());
    const QRect r = rectOf(e, u"login"_s, u"start"_s);
    CHECK_FALSE(e.setZoneRects(u"login"_s, {{u"start"_s, r}}));
    CHECK_FALSE(e.setZoneField(u"login"_s, {u"start"_s}, u"label"_s, u"Log In"_s));
    CHECK(e.undoStack()->count() == 0);
}

TEST_CASE("Rects are clamped to the canvas and a minimum size", "[editor]")
{
    LayoutEditor e(seed());
    e.setZoneRects(u"login"_s, {{u"start"_s, QRect(1800, -50, 400, 4)}});
    const QRect r = rectOf(e, u"login"_s, u"start"_s);
    CHECK(r == QRect(1520, 0, 400, 16));
}

TEST_CASE("Nudges merge into one undo step", "[editor]")
{
    LayoutEditor e(seed());
    const QRect before = rectOf(e, u"login"_s, u"start"_s);
    for (int i = 0; i < 5; ++i)
        e.nudgeZones(u"login"_s, {u"start"_s}, 8, 0);
    CHECK(rectOf(e, u"login"_s, u"start"_s) == before.translated(40, 0));
    CHECK(e.undoStack()->count() == 1);
    e.undoStack()->undo();
    CHECK(rectOf(e, u"login"_s, u"start"_s) == before);
}

TEST_CASE("Field edits apply to every selected zone and can reset", "[editor]")
{
    LayoutEditor e(seed());
    const QStringList ids = {u"quick"_s, u"takeout"_s};
    REQUIRE(e.setZoneField(u"tables"_s, ids, u"style.normal.fill"_s, u"#123456"_s));
    for (const QString &id : ids)
        CHECK(e.zoneField(u"tables"_s, id, u"style.normal.fill"_s).toString() == u"#123456"_s);

    // Reset to inherited removes the key entirely.
    REQUIRE(e.setZoneField(u"tables"_s, ids, u"style.normal.fill"_s, QJsonValue(QJsonValue::Undefined)));
    CHECK(e.zoneField(u"tables"_s, u"takeout"_s, u"style.normal.fill"_s).isUndefined());
    CHECK(e.layout().page(u"tables"_s)->zone(u"takeout"_s)->style.isEmpty());

    // Defaults are visible to the inspector.
    CHECK(e.zoneField(u"tables"_s, u"takeout"_s, u"enabled"_s).toBool());
    CHECK(e.zoneField(u"tables"_s, u"takeout"_s, u"hotkey"_s).toString().isEmpty());

    // Kind can change; id cannot be edited through fields.
    CHECK(e.setZoneField(u"tables"_s, {u"takeout"_s}, u"kind"_s, u"label"_s));
    CHECK(e.layout().page(u"tables"_s)->zone(u"takeout"_s)->kind == u"label"_s);
    CHECK_FALSE(e.setZoneField(u"tables"_s, {u"takeout"_s}, u"id"_s, u"x"_s));
}

TEST_CASE("Duplicate, delete, z-order", "[editor]")
{
    LayoutEditor e(seed());
    const QStringList copies = e.duplicateZones(u"tables"_s, {u"quick"_s, u"takeout"_s});
    REQUIRE(copies == QStringList{u"quick-2"_s, u"takeout-2"_s});
    CHECK(rectOf(e, u"tables"_s, u"quick-2"_s) == rectOf(e, u"tables"_s, u"quick"_s).translated(16, 16));

    REQUIRE(e.bringToFront(u"tables"_s, {u"table-t1"_s}));
    CHECK(e.layout().page(u"tables"_s)->zones.last().id == u"table-t1"_s);
    REQUIRE(e.sendToBack(u"tables"_s, {u"table-t1"_s}));
    CHECK(e.layout().page(u"tables"_s)->zones.first().id == u"table-t1"_s);

    REQUIRE(e.deleteZones(u"tables"_s, copies));
    CHECK_FALSE(e.layout().page(u"tables"_s)->zone(u"quick-2"_s));
}

TEST_CASE("Align, distribute, match size", "[editor]")
{
    LayoutEditor e(seed());
    const QString p = u"tables"_s;
    const QStringList ids = {u"quick"_s, u"takeout"_s, u"checks"_s};
    e.setZoneRects(p, {{u"quick"_s, {100, 100, 200, 200}}, {u"takeout"_s, {400, 180, 100, 100}},
                       {u"checks"_s, {1000, 140, 200, 150}}});

    REQUIRE(e.align(p, ids, LayoutEditor::Align::Top));
    for (const QString &id : ids)
        CHECK(rectOf(e, p, id).top() == 100);

    REQUIRE(e.distribute(p, ids, Qt::Horizontal));
    // span 100..1200 = 1100, widths 500 -> gaps of 300
    CHECK(rectOf(e, p, u"takeout"_s).left() == 600);
    CHECK(rectOf(e, p, u"checks"_s).left() == 1000);

    REQUIRE(e.matchSize(p, ids, true, true));
    for (const QString &id : ids)
        CHECK(rectOf(e, p, id).size() == QSize(200, 200));

    CHECK_FALSE(e.distribute(p, {u"quick"_s, u"takeout"_s}, Qt::Vertical));   // needs 3
}

TEST_CASE("Copy on one page, paste on another", "[editor]")
{
    LayoutEditor e(seed());
    CHECK_FALSE(e.hasClipboard());
    e.copyZones(u"library"_s, {u"lib-send"_s, u"lib-void"_s});
    REQUIRE(e.hasClipboard());

    const QStringList pasted = e.paste(u"login"_s);
    REQUIRE(pasted == QStringList{u"lib-send"_s, u"lib-void"_s});
    const Zone *send = e.layout().page(u"login"_s)->zone(u"lib-send"_s);
    REQUIRE(send);
    CHECK(send->rect == e.layout().page(u"library"_s)->zone(u"lib-send"_s)->rect);   // same spot

    // Pasting again on the same page offsets and renames.
    const QStringList again = e.paste(u"login"_s);
    CHECK(again == QStringList{u"lib-send-2"_s, u"lib-void-2"_s});
    CHECK(rectOf(e, u"login"_s, u"lib-send-2"_s) == send->rect.translated(16, 16));
}

TEST_CASE("Pages: add, duplicate, delete rules", "[editor]")
{
    LayoutEditor e(seed());
    const QString id = e.addPage(u"Happy Hour"_s, u"items"_s, u"order-template"_s);
    CHECK(id == u"happy-hour"_s);
    CHECK(e.layout().page(id)->templateId == u"order-template"_s);
    CHECK(e.addPage(u"Happy Hour"_s, u"items"_s) == u"happy-hour-2"_s);

    const QString dup = e.duplicatePage(u"settle"_s);
    CHECK(dup == u"settle-copy"_s);
    CHECK(e.layout().page(dup)->role.isEmpty());
    CHECK(e.layout().page(dup)->zones.size() == e.layout().page(u"settle"_s)->zones.size());

    QString why;
    CHECK_FALSE(e.deletePage(u"order-template"_s, &why));
    CHECK(why.contains(u"template"_s));
    CHECK_FALSE(e.deletePage(u"login"_s, &why));
    CHECK(e.deletePage(dup, &why));
    CHECK_FALSE(e.layout().page(dup));
}

TEST_CASE("Renaming a page id rewrites every reference", "[editor]")
{
    LayoutEditor e(seed());
    const QStringList before = e.referencesTo(u"split"_s);
    CHECK_FALSE(before.isEmpty());

    QString why;
    REQUIRE(e.setPageField(u"split"_s, u"id"_s, u"split-check"_s, &why));
    CHECK_FALSE(e.layout().page(u"split"_s));
    CHECK(e.referencesTo(u"split-check"_s).size() == before.size());
    CHECK(e.referencesTo(u"split"_s).isEmpty());
    CHECK(e.layout().validate() == QStringList{});

    REQUIRE(e.setPageField(u"order-template"_s, u"id"_s, u"order-base"_s, &why));
    CHECK(e.layout().page(u"index-lunch"_s)->templateId == u"order-base"_s);

    CHECK_FALSE(e.setPageField(u"settle"_s, u"id"_s, u"login"_s, &why));
    CHECK(why.contains(u"already"_s));
    CHECK_FALSE(e.setPageField(u"settle"_s, u"id"_s, u"has space"_s, &why));
}

TEST_CASE("Page field rules: roles unique, no template loops", "[editor]")
{
    LayoutEditor e(seed());
    QString why;
    CHECK_FALSE(e.setPageField(u"settle"_s, u"role"_s, u"login"_s, &why));
    CHECK_FALSE(e.setPageField(u"login"_s, u"role"_s, u""_s, &why));
    CHECK_FALSE(e.setPageField(u"order-template"_s, u"templateId"_s, u"items-burgers"_s, &why));
    CHECK(why.contains(u"loop"_s));

    REQUIRE(e.setPageField(u"settle"_s, u"background.fill"_s, u"#000000"_s, &why));
    CHECK(e.pageField(u"settle"_s, u"background.fill"_s).toString() == u"#000000"_s);
    CHECK(e.pageField(u"settle"_s, u"templateId"_s).toString().isEmpty());   // default visible
}

TEST_CASE("Theme edits", "[editor]")
{
    LayoutEditor e(seed());
    REQUIRE(e.setThemeField(u"style.normal.fontSize"_s, 36));
    CHECK(e.themeField(u"style.normal.fontSize"_s).toInt() == 36);
    const Zone &z = *e.layout().page(u"login"_s)->zone(u"start"_s);
    CHECK(e.layout().resolveStyle(z, u"login"_s, ZoneState::Normal).value(u"fontSize").toInt() == 36);
}

TEST_CASE("Export and import a page", "[editor]")
{
    LayoutEditor e(seed());
    const QJsonObject exported = e.exportPage(u"settle"_s);
    CHECK(exported.value(u"schemaVersion").toInt() == Layout::SchemaVersion);

    QString why;
    const QString id = e.importPage(exported, &why);
    CHECK(id == u"settle-2"_s);
    CHECK(e.layout().page(id)->role.isEmpty());   // settle role stays with the original
    CHECK(e.layout().page(id)->zones == e.layout().page(u"settle"_s)->zones);

    QJsonObject future = exported;
    future.insert(u"schemaVersion"_s, Layout::SchemaVersion + 1);
    CHECK(e.importPage(future, &why).isEmpty());
    CHECK_FALSE(why.isEmpty());
}

TEST_CASE("slugify", "[editor]")
{
    CHECK(LayoutEditor::slugify(u"Happy Hour!"_s) == u"happy-hour"_s);
    CHECK(LayoutEditor::slugify(u"  Café Menü  "_s) == u"cafe-menu"_s);
    CHECK(LayoutEditor::slugify(u"---"_s).isEmpty());
}

TEST_CASE("Tables are zones: added, duplicated and pasted with free names", "[editor][tables]")
{
    LayoutEditor e(seed());
    const Layout &l = e.layout();
    CHECK(l.tableLabels().contains(u"T7"_s));
    CHECK(l.nextTableLabel(u"T3"_s) == u"T8"_s);        // after the highest T
    CHECK(l.nextTableLabel(u"Bar 1"_s) == u"Bar 4"_s);
    CHECK(l.nextTableLabel(u"Patio"_s) == u"Patio 2"_s);
    CHECK(l.validate().filter(u"table"_s).isEmpty());

    const QString added = e.addZone(u"tables"_s, u"table"_s);
    REQUIRE_FALSE(added.isEmpty());
    const Zone *z = e.layout().page(u"tables"_s)->zone(added);
    CHECK(z->label == u"T8"_s);
    CHECK(z->props.value(u"seats"_s).toInt() == 4);

    const QStringList copies = e.duplicateZones(u"tables"_s, {added, u"table-bar-3"_s});
    REQUIRE(copies.size() == 2);
    CHECK(e.layout().page(u"tables"_s)->zone(copies[0])->label == u"T9"_s);
    CHECK(e.layout().page(u"tables"_s)->zone(copies[1])->label == u"Bar 4"_s);

    e.copyZones(u"tables"_s, {u"table-t1"_s});
    const QStringList pasted = e.paste(u"tables"_s);
    REQUIRE(pasted.size() == 1);
    CHECK(e.layout().page(u"tables"_s)->zone(pasted[0])->label == u"T10"_s);

    // Seats are edited like any field; renaming onto a taken name is reported.
    REQUIRE(e.setZoneField(u"tables"_s, {added}, u"props.seats"_s, 6));
    CHECK(e.layout().page(u"tables"_s)->zone(added)->props.value(u"seats"_s).toInt() == 6);
    REQUIRE(e.setZoneField(u"tables"_s, {added}, u"label"_s, u"T1"_s));
    CHECK(e.layout().validate().contains(u"table 'T1' is on page 'tables' more than once"_s));
    CHECK(schema::isWidgetKind(u"table"_s));
}

TEST_CASE("Widgets: their settings, built-in buttons and button look are in the inspector", "[editor][builtins]")
{
    using namespace vt::layout;
    const auto paths = [](const QString &kind) {
        QStringList out;
        for (const QJsonValue &f : schema::zoneFields(kind))
            out << f.toObject().value(u"path").toString();
        return out;
    };
    const QStringList kds = paths(u"kitchenDisplay"_s);
    CHECK(kds.contains(u"props.mode"_s));
    CHECK(kds.contains(u"props.station"_s));
    CHECK(kds.contains(u"props.hideButtons"_s));
    CHECK(kds.contains(u"props.buttons.recall.hide"_s));
    CHECK(kds.contains(u"props.buttons.recall.label"_s));
    CHECK(kds.contains(u"style.normal.keyFill"_s));
    CHECK(paths(u"checkList"_s).contains(u"props.mode"_s));
    CHECK(paths(u"reportView"_s).contains(u"props.report"_s));
    CHECK_FALSE(paths(u"button"_s).contains(u"style.normal.keyFill"_s));   // plain buttons have no built-ins

    // Every built-in button's command is one a regular button can pick.
    QStringList commands;
    for (const QJsonValue &t : schema::actionTypes()) {
        if (t.toObject().value(u"type").toString() != u"command")
            continue;
        for (const QJsonValue &f : t.toObject().value(u"fields").toArray())
            for (const QJsonValue &o : f.toObject().value(u"options").toArray())
                commands << o.toObject().value(u"value").toString();
    }
    for (const QString &kind : schema::widgetKinds())
        for (const schema::BuiltIn &b : schema::builtInButtons(kind))
            if (!b.command.isEmpty())
                CHECK(commands.contains(b.command));
}

TEST_CASE("A new zone goes in free space, not on top of others", "[editor]")
{
    // The Bar's floor plan: room on both sides of the bar.
    LayoutEditor e(seed());
    const auto rectOf = [&](const QString &id) {
        return e.layout().page(u"floor-bar"_s)->zone(id)->rect;
    };
    for (int i = 0; i < 3; ++i) {
        const QString id = e.addZone(u"floor-bar"_s, u"button"_s);
        REQUIRE_FALSE(id.isEmpty());
        for (const auto &pz : e.layout().effectiveZones(u"floor-bar"_s))
            if (pz.zone->id != id && pz.zone->props.value(u"decor").toString() != u"area")   // a room's floor: free
                CHECK_FALSE(pz.zone->rect.intersects(rectOf(id)));
    }
}
