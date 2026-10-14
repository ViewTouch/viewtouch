#include <catch2/catch_test_macros.hpp>

#include "layout/layout.hh"
#include "qt_catch.hh"

#include <QJsonArray>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt::layout;

namespace {

QJsonObject jsonOf(const char *text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

Zone button(const QString &id, QRect r = {10, 10, 100, 60})
{
    Zone z;
    z.id = id;
    z.label = id;
    z.rect = r;
    return z;
}

Page makePage(const QString &id, const QString &templateId = {})
{
    Page p;
    p.id = id;
    p.name = id;
    p.templateId = templateId;
    return p;
}

// base <- order <- items ; login stands alone
Layout sampleLayout()
{
    Layout l;
    l.theme.style.normal = jsonOf(R"({"fill":"#111","fontSize":30,"frame":"raised"})");
    l.theme.style.selected = jsonOf(R"({"fill":"#00f"})");
    l.theme.kinds.insert(u"label"_s, Style{jsonOf(R"({"frame":"none"})"), {}, {}, {}});

    Page base = makePage(u"base"_s);
    base.kind = u"template"_s;
    base.style.normal = jsonOf(R"({"fontSize":24})");
    base.background = jsonOf(R"({"fill":"#222","texture":"sand"})");
    base.zones = {button(u"b1"_s)};

    Page order = makePage(u"order"_s, u"base"_s);
    order.kind = u"template"_s;
    order.zones = {button(u"o1"_s)};

    Page items = makePage(u"items"_s, u"order"_s);
    items.kind = u"items"_s;
    items.background = jsonOf(R"({"fill":"#333"})");
    Zone top = button(u"top"_s);
    top.z = 5;
    Zone i1 = button(u"i1"_s);
    i1.style.normal = jsonOf(R"({"fill":"#f00"})");
    items.zones = {top, i1};

    Page login = makePage(u"login"_s);
    login.role = u"login"_s;
    login.zones = {button(u"go"_s)};

    l.pages = {base, order, items, login};
    return l;
}

} // namespace

TEST_CASE("Zone JSON round-trips, keeping unknown keys", "[layout]")
{
    const QJsonObject in = jsonOf(R"({
        "id": "z1", "kind": "button", "label": "Send", "rect": {"x": 8, "y": 16, "w": 200, "h": 80},
        "z": 2, "shape": "hexagon", "behavior": "double", "hotkey": "s", "group": "g",
        "style": {"normal": {"fill": "#0a0", "futureKey": 1}, "hover": {"fill": "#fff"}},
        "actions": [{"type": "command", "name": "sendOrder"}, {"type": "futureAction", "x": 1}],
        "props": {"columns": 3},
        "futureField": {"nested": true}
    })");

    const Zone z = Zone::fromJson(in);
    CHECK(z.id == u"z1"_s);
    CHECK(z.rect == QRect(8, 16, 200, 80));
    CHECK(z.shape == u"hexagon"_s);
    CHECK(z.actions.size() == 2);
    CHECK(z.actions[0].type() == u"command"_s);
    CHECK(z.extra.contains(u"futureField"_s));
    CHECK(z.style.extra.contains(u"hover"_s));

    CHECK(z.toJson() == in);
    CHECK(Zone::fromJson(z.toJson()) == z);
}

TEST_CASE("Zone defaults are applied and omitted on write", "[layout]")
{
    const Zone z = Zone::fromJson(jsonOf(R"({"id":"a","rect":{"x":0,"y":0,"w":50,"h":50}})"));
    CHECK(z.kind == u"button"_s);
    CHECK(z.shape == u"rect"_s);
    CHECK(z.behavior == u"blink"_s);
    CHECK(z.enabled);

    const QJsonObject out = z.toJson();
    CHECK_FALSE(out.contains(u"enabled"_s));
    CHECK_FALSE(out.contains(u"actions"_s));
    CHECK_FALSE(out.contains(u"style"_s));
}

TEST_CASE("Layout JSON and directory round-trip", "[layout]")
{
    Layout l = sampleLayout();
    l.pages[0].extra.insert(u"futurePageField"_s, 42);

    auto fromJson = Layout::fromJson(l.toJson());
    REQUIRE(fromJson);
    CHECK(*fromJson == l);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QStringList errors;
    REQUIRE(l.saveDirectory(dir.path(), &errors));
    CHECK(errors.isEmpty());
    auto loaded = Layout::loadDirectory(dir.path(), &errors);
    REQUIRE(loaded);
    CHECK(errors.isEmpty());
    // Directory order is by file name, so compare page by page.
    CHECK(loaded->theme == l.theme);
    REQUIRE(loaded->pages.size() == l.pages.size());
    for (const Page &p : l.pages) {
        REQUIRE(loaded->page(p.id));
        CHECK(*loaded->page(p.id) == p);
    }

    // Removing a page removes its file on the next save.
    l.pages.removeLast();
    REQUIRE(l.saveDirectory(dir.path(), &errors));
    CHECK(Layout::loadDirectory(dir.path())->pages.size() == l.pages.size());
}

TEST_CASE("Layout rejects a newer schema version", "[layout]")
{
    QJsonObject o = sampleLayout().toJson();
    o.insert(u"schemaVersion"_s, Layout::SchemaVersion + 1);
    QStringList errors;
    CHECK_FALSE(Layout::fromJson(o, &errors));
    CHECK(errors.size() == 1);
}

TEST_CASE("Template chain and effective zones", "[layout]")
{
    const Layout l = sampleLayout();

    const auto chain = l.templateChain(u"items"_s);
    REQUIRE(chain.size() == 3);
    CHECK(chain[0]->id == u"items"_s);
    CHECK(chain[2]->id == u"base"_s);

    const auto zones = l.effectiveZones(u"items"_s);
    REQUIRE(zones.size() == 4);
    // deepest template first, own zones last, own zones sorted by z
    CHECK(zones[0].zone->id == u"b1"_s);
    CHECK(zones[0].inherited);
    CHECK(zones[1].zone->id == u"o1"_s);
    CHECK(zones[2].zone->id == u"i1"_s);
    CHECK_FALSE(zones[2].inherited);
    CHECK(zones[3].zone->id == u"top"_s);
}

TEST_CASE("Template cycles terminate and are reported", "[layout]")
{
    Layout l = sampleLayout();
    l.page(u"base"_s)->templateId = u"items"_s;   // items -> order -> base -> items

    CHECK(l.templateChain(u"items"_s).size() == 3);
    CHECK(l.effectiveZones(u"items"_s).size() == 4);

    const QStringList issues = l.validate();
    CHECK(issues.filter(u"loops"_s).size() == 3);
}

TEST_CASE("Style resolves zone -> page chain -> theme kind -> theme", "[layout]")
{
    const Layout l = sampleLayout();
    const Page &items = *l.page(u"items"_s);
    const Zone &i1 = *items.zone(u"i1"_s);
    const Zone &top = *items.zone(u"top"_s);

    const QJsonObject n = l.resolveStyle(i1, u"items"_s, ZoneState::Normal);
    CHECK(n.value(u"fill").toString() == u"#f00"_s);     // zone
    CHECK(n.value(u"fontSize").toInt() == 24);          // template 'base'
    CHECK(n.value(u"frame").toString() == u"raised"_s); // theme

    // Selected: theme's selected fill beats the zone's normal fill,
    // everything else falls back to the resolved normal state.
    const QJsonObject s = l.resolveStyle(top, u"items"_s, ZoneState::Selected);
    CHECK(s.value(u"fill").toString() == u"#00f"_s);
    CHECK(s.value(u"fontSize").toInt() == 24);

    Zone label = top;
    label.kind = u"label"_s;
    CHECK(l.resolveStyle(label, u"items"_s, ZoneState::Normal).value(u"frame").toString() == u"none"_s);

    // Same zone seen on a page outside the chain uses only theme defaults.
    CHECK(l.resolveStyle(top, u"login"_s, ZoneState::Normal).value(u"fontSize").toInt() == 30);
}

TEST_CASE("Background inherits through templates", "[layout]")
{
    const Layout l = sampleLayout();
    const QJsonObject bg = l.resolveBackground(u"items"_s);
    CHECK(bg.value(u"fill").toString() == u"#333"_s);
    CHECK(bg.value(u"texture").toString() == u"sand"_s);
}

TEST_CASE("Validation reports broken references", "[layout]")
{
    Layout l = sampleLayout();
    CHECK(l.validate().isEmpty());

    Zone bad = button(u"bad"_s);
    bad.actions = {Action{jsonOf(R"({"type":"jump","page":"nowhere"})")},
                   Action{jsonOf(R"({"type":"jump","mode":"sideways"})")},
                   Action{jsonOf(R"({"type":"addItem","item":"X","modifierSequence":["ghost"]})")}};
    Zone offCanvas = button(u"off"_s, {1900, 1000, 100, 100});
    Zone dup = button(u"go"_s);
    l.page(u"login"_s)->zones.append({bad, offCanvas, dup});
    l.page(u"order"_s)->templateId = u"missing"_s;

    const QStringList issues = l.validate();
    CHECK(issues.filter(u"jump target does not resolve"_s).size() == 1);
    CHECK(issues.filter(u"unknown jump mode"_s).size() == 1);
    CHECK(issues.filter(u"modifier page 'ghost'"_s).size() == 1);
    CHECK(issues.filter(u"outside the"_s).size() == 1);
    CHECK(issues.filter(u"duplicate zone id"_s).size() == 1);
    CHECK(issues.filter(u"template 'missing'"_s).size() == 1);

    l.page(u"login"_s)->role.clear();
    CHECK(l.validate().filter(u"required role 'login'"_s).size() == 1);
}

TEST_CASE("An old tableMap floor plan loads as separate table zones", "[layout][tables]")
{
    const QJsonObject json{
        {u"id"_s, u"floor"_s}, {u"name"_s, u"Floor"_s}, {u"kind"_s, u"tables"_s},
        {u"zones"_s, QJsonArray{
            QJsonObject{{u"id"_s, u"title"_s}, {u"kind"_s, u"label"_s}, {u"rect"_s, QJsonObject{{u"x"_s, 0}, {u"y"_s, 0}, {u"w"_s, 100}, {u"h"_s, 50}}}},
            QJsonObject{{u"id"_s, u"map"_s}, {u"kind"_s, u"tableMap"_s}, {u"z"_s, 2},
                        {u"rect"_s, QJsonObject{{u"x"_s, 16}, {u"y"_s, 16}, {u"w"_s, 1400}, {u"h"_s, 1000}}},
                        {u"props"_s, QJsonObject{{u"tables"_s, QJsonArray{
                            QJsonObject{{u"label"_s, u"T1"_s}, {u"x"_s, 80}, {u"y"_s, 80}, {u"w"_s, 200}, {u"h"_s, 200},
                                        {u"shape"_s, u"circle"_s}, {u"seats"_s, 2}},
                            QJsonObject{{u"label"_s, u"Bar 1"_s}, {u"x"_s, 80}, {u"y"_s, 760}, {u"w"_s, 240}, {u"h"_s, 160}}}}}}},
            QJsonObject{{u"id"_s, u"after"_s}, {u"kind"_s, u"button"_s}, {u"rect"_s, QJsonObject{{u"x"_s, 1500}, {u"y"_s, 0}, {u"w"_s, 100}, {u"h"_s, 50}}}},
        }},
    };
    const Page p = Page::fromJson(json);
    REQUIRE(p.zones.size() == 4);   // the map became two tables, in its place
    CHECK(p.zones[0].id == u"title"_s);
    const Zone &t1 = p.zones[1];
    CHECK(t1.kind == u"table"_s);
    CHECK(t1.id == u"table-t1"_s);
    CHECK(t1.label == u"T1"_s);
    CHECK(t1.rect == QRect(96, 96, 200, 200));
    CHECK(t1.shape == u"circle"_s);
    CHECK(t1.z == 2);
    CHECK(t1.props.value(u"seats"_s).toInt() == 2);
    CHECK(p.zones[2].id == u"table-bar-1"_s);
    CHECK(p.zones[2].rect == QRect(96, 776, 240, 160));
    CHECK_FALSE(p.zones[2].props.contains(u"seats"_s));
    CHECK(p.zones[3].id == u"after"_s);
}

TEST_CASE("Starter pages: new ones added, unedited ones updated, edits kept", "[layout][upgrade]")
{
    auto makePage = [](const QString &id, const QString &label) {
        Page p;
        p.id = id;
        p.name = id;
        Zone z;
        z.id = u"b"_s;
        z.label = label;
        z.rect = QRect(0, 0, 100, 100);
        p.zones.append(z);
        return p;
    };
    // What an older version installed: manager, tables, extra.
    Layout saved;
    saved.pages = {makePage(u"manager"_s, u"v1"_s), makePage(u"tables"_s, u"v1"_s), makePage(u"legacy"_s, u"v1"_s)};
    QHash<QString, QString> installed{{u"manager"_s, Layout::fingerprint(saved.pages[0])},
                                      {u"tables"_s, Layout::fingerprint(saved.pages[1])}};
    saved.pages[1].zones[0].label = u"my floor"_s;   // the store edited its tables page

    // The new version: all three changed, plus two new pages; "gone" was deleted here on purpose.
    Layout starter;
    starter.pages = {makePage(u"manager"_s, u"v2"_s), makePage(u"tables"_s, u"v2"_s), makePage(u"legacy"_s, u"v2"_s),
                     makePage(u"tables-phone"_s, u"v2"_s), makePage(u"gone"_s, u"v2"_s)};
    const QStringList seen{u"manager"_s, u"tables"_s, u"legacy"_s, u"gone"_s};

    const auto update = saved.updateFromStarter(starter, seen, installed);
    CHECK(update.added == QStringList{u"tables-phone"_s});
    CHECK(update.updated == QStringList{u"manager"_s});
    CHECK(saved.page(u"manager"_s)->zones[0].label == u"v2"_s);         // never edited: updated
    CHECK(saved.page(u"tables"_s)->zones[0].label == u"my floor"_s);    // edited: kept
    CHECK(saved.page(u"legacy"_s)->zones[0].label == u"v1"_s);          // no fingerprint: left alone
    CHECK_FALSE(saved.page(u"gone"_s));                                  // deleted on purpose: stays gone
    CHECK(saved.page(u"tables-phone"_s));
    // Fingerprints to keep: the new versions where installed, the old one where edited.
    CHECK(update.installed.value(u"manager"_s) == Layout::fingerprint(*starter.page(u"manager"_s)));
    CHECK(update.installed.value(u"tables"_s) == installed.value(u"tables"_s));
    CHECK(update.installed.contains(u"tables-phone"_s));
    CHECK_FALSE(update.installed.contains(u"legacy"_s));

    // Running it again changes nothing.
    const auto again = saved.updateFromStarter(starter, seen + update.added, update.installed);
    CHECK(again.added.isEmpty());
    CHECK(again.updated.isEmpty());
}
