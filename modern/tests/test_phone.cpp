#include <catch2/catch_test_macros.hpp>

#include "editorcontroller.hh"
#include "layout_fixture.hh"
#include "app/layout_editor.hh"
#include "layout/reflow.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::layout::Zone;

// Phones: phone versions of pages, and pages laid out again for a narrow screen.

namespace {

layout::Layout seedLayout()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

Zone zoneAt(const QString &id, const QString &kind, QRect rect)
{
    Zone z;
    z.id = id;
    z.kind = kind;
    z.rect = rect;
    return z;
}

QStringList shownIds(LayoutController &c)
{
    QStringList ids;
    ZoneModel *m = c.zones();
    for (int r = 0; r < m->rowCount(); ++r)
        ids << m->data(m->index(r), ZoneModel::ZoneIdRole).toString();
    return ids;
}

QRect shownRect(LayoutController &c, const QString &id)
{
    ZoneModel *m = c.zones();
    for (int r = 0; r < m->rowCount(); ++r) {
        const QModelIndex i = m->index(r);
        if (m->data(i, ZoneModel::ZoneIdRole).toString() == id)
            return QRect(m->data(i, ZoneModel::ZoneXRole).toInt(), m->data(i, ZoneModel::ZoneYRole).toInt(),
                         m->data(i, ZoneModel::ZoneWRole).toInt(), m->data(i, ZoneModel::ZoneHRole).toInt());
    }
    return {};
}

} // namespace

TEST_CASE("Reflow: reading order, full-width labels and panels, a grid of buttons", "[phone][reflow]")
{
    // A landscape page: title, a panel, and a row of four buttons.
    const QList<Zone> page{
        zoneAt(u"b2"_s, u"button"_s, {600, 300, 200, 100}),
        zoneAt(u"title"_s, u"label"_s, {100, 20, 1700, 80}),
        zoneAt(u"b1"_s, u"button"_s, {100, 300, 200, 100}),
        zoneAt(u"note"_s, u"comment"_s, {1500, 500, 300, 100}),
        zoneAt(u"panel"_s, u"keyboard"_s, {100, 500, 1700, 400}),
        zoneAt(u"b3"_s, u"button"_s, {1000, 300, 200, 100}),
        zoneAt(u"b4"_s, u"button"_s, {1400, 300, 200, 100}),
    };
    QList<const Zone *> zones;
    for (const Zone &z : page)
        zones << &z;
    const QRect area(16, 900, 1048, 1300);
    const QList<QRect> r = layout::reflowZones(zones, area);
    REQUIRE(r.size() == zones.size());

    CHECK(r[3].isEmpty());                                   // notes are not shown
    CHECK(r[1].top() == area.top());                         // the title comes first...
    CHECK(r[1].width() == area.width());                     // ...across the whole width
    CHECK(r[2].top() > r[1].bottom());                       // then the buttons, left to right
    CHECK(r[2].left() == area.left());
    CHECK(r[0].top() == r[2].top());                          // two per row
    CHECK(r[0].left() > r[2].right());
    CHECK(r[5].top() > r[2].bottom());
    CHECK(r[4].top() > r[6].bottom());                       // the panel after the buttons
    CHECK(r[4].width() == area.width());
    for (qsizetype i = 0; i < r.size(); ++i) {
        if (!r[i].isEmpty())
            CHECK(area.contains(r[i]));
        for (qsizetype j = i + 1; j < r.size(); ++j)
            CHECK_FALSE(r[i].intersects(r[j]));
    }

    // Many buttons: 3 columns and shorter rows before running past the bottom.
    QList<Zone> many;
    for (int i = 0; i < 18; ++i)
        many << zoneAt(u"m%1"_s.arg(i), u"button"_s, {100 + (i % 6) * 250, 100 + (i / 6) * 150, 200, 100});
    QList<const Zone *> manyZones;
    for (const Zone &z : many)
        manyZones << &z;
    const QList<QRect> m = layout::reflowZones(manyZones, QRect(0, 0, 1048, 1032));
    for (const QRect &rect : m)
        CHECK(QRect(0, 0, 1048, 1032).contains(rect));
    CHECK(m[0].width() < 1048 / 2);                           // 3 across
}

TEST_CASE("Reflow: Show on standard screens only, a row of its own, first and last", "[phone][reflow]")
{
    QList<Zone> page{
        zoneAt(u"a"_s, u"button"_s, {100, 100, 200, 100}),
        zoneAt(u"b"_s, u"button"_s, {400, 100, 200, 100}),
        zoneAt(u"c"_s, u"button"_s, {700, 100, 200, 100}),
        zoneAt(u"d"_s, u"button"_s, {1000, 100, 200, 100}),
        zoneAt(u"e"_s, u"button"_s, {1300, 100, 200, 100}),
    };
    page[1].extra.insert(u"showWhen"_s, QJsonObject{{u"screen"_s, u"standard"_s}});
    page[2].extra.insert(u"phone"_s, QJsonObject{{u"wide"_s, true}});
    page[3].extra.insert(u"phone"_s, QJsonObject{{u"place"_s, u"first"_s}});
    page[0].extra.insert(u"phone"_s, QJsonObject{{u"place"_s, u"last"_s}});
    QList<const Zone *> zones;
    for (const Zone &z : page)
        zones << &z;
    const QRect area(0, 0, 1000, 2000);
    const QList<QRect> r = layout::reflowZones(zones, area);

    CHECK(!layout::showsOnPhones(page[1]));
    CHECK(r[1].isEmpty());                          // standard screens only
    CHECK(r[3].topLeft() == area.topLeft());        // first
    CHECK(r[2].width() == area.width());            // a row of its own
    CHECK(r[2].top() > r[3].bottom());
    CHECK(r[4].top() > r[2].bottom());              // the rest after it...
    CHECK(r[0].top() >= r[4].top());                // ...and the last one last
    CHECK((r[0].top() > r[4].top() || r[0].left() > r[4].left()));
}

TEST_CASE("Phone versions: standard-only buttons, where they differ, and a warning when they drift", "[phone][phoneVersion]")
{
    EditorController e(seedLayout());
    // A button for standard screens only isn't missing from the phone.
    e.setPageId(u"settle"_s);
    const QString id = e.addZone(u"button"_s);
    REQUIRE(!id.isEmpty());
    CHECK(e.layout().validate().filter(u"settle-phone"_s).size() == 1);
    e.selectOnly({id});
    REQUIRE(e.setField(u"zone"_s, u"showWhen.screen"_s, u"standard"_s));
    CHECK(e.layout().validate().isEmpty());

    // The phone's own words for Split: listed, with the page's.
    e.setPageId(u"settle-phone"_s);
    const auto split = [&] {
        for (const QVariant &v : e.phoneDifferences()) {
            const QVariantMap m = v.toMap();
            if (m.value(u"id"_s) == u"split"_s && m.value(u"path"_s) == u"label"_s)
                return m;
        }
        return QVariantMap();
    };
    REQUIRE(!split().isEmpty());
    CHECK(split().value(u"phone"_s) == u"Split Check"_s);
    CHECK(split().value(u"standard"_s) == u"Split"_s);

    // Changing Split's words on the standard page says the phone keeps its own.
    e.setPageId(u"settle"_s);
    e.selectOnly({u"split"_s});
    REQUIRE(e.setField(u"zone"_s, u"label"_s, u"Split…"_s));
    CHECK(e.notice().contains(u"Split Check"_s));

    // Same as the page again: no longer different.
    e.setPageId(u"settle-phone"_s);
    e.followPage(u"split"_s, u"label"_s);
    CHECK(split().isEmpty());
    CHECK(e.layout().page(u"settle-phone"_s)->zone(u"split"_s)->label == u"Split…"_s);

    // A copy made on the phone is only on the phone.
    e.selectOnly({u"tender-cash"_s});
    e.duplicateSelection();
    CHECK(std::ranges::any_of(e.phoneDifferences(), [](const QVariant &v) { return v.toMap().value(u"phoneOnly"_s).toBool(); }));

    // The "On phones" hints are for pages phones lay out by themselves.
    const auto hasHints = [&] {
        return std::ranges::any_of(e.zoneFields(u"button"_s), [](const QVariant &f) {
            return f.toMap().value(u"path"_s).toString() == u"phone.wide"_s;
        });
    };
    CHECK(!hasHints());
    e.setPageId(u"settle"_s);
    CHECK(hasHints());
}

TEST_CASE("Phone pages: versions, the phone frame, and back to standard", "[phone][ui]")
{
    const layout::Layout l = seedLayout();
    CHECK(l.variantFor(u"tables"_s, u"phone"_s));
    CHECK(l.validate().filter(u"version"_s).isEmpty());

    PosService pos(test::seedPosData(), nullptr);
    LayoutController c(l);
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(c.jumpTo(u"tables"_s));
    CHECK(c.formFactor() == u"standard"_s);
    CHECK(c.canvasSize() == QSize(1920, 1080));
    CHECK(shownIds(c).contains(u"table-t1"_s));

    QSignalSpy changed(&c, &LayoutController::formFactorChanged);
    c.setFormFactorOverride(u"phone"_s);
    CHECK(changed.size() == 1);
    CHECK(c.pageId() == u"tables"_s);                         // still the tables page...
    CHECK(c.canvasSize() == QSize(1080, 2280));                // ...its phone version on screen
    CHECK(shownIds(c).contains(u"tables"_s));                  // the table grid
    CHECK_FALSE(shownIds(c).contains(u"table-t1"_s));
    const QVariantList tables = c.tables();
    CHECK(tables.size() == 16);                                // every floor plan's, once each, by name
    CHECK(tables.first().toMap()[u"name"_s] == u"Bar 1"_s);
    CHECK(tables.first().toMap()[u"seats"_s] == 1);

    // A menu page with no phone version: the phone order frame around its buttons.
    REQUIRE(c.jumpTo(u"items-burgers"_s));
    const QStringList ids = shownIds(c);
    CHECK(ids.contains(u"order-list"_s));
    CHECK(ids.contains(u"flow-send"_s));
    CHECK_FALSE(ids.contains(u"tab-lunch"_s));                 // the landscape tabs are not
    const QRect burger = shownRect(c, u"item-1"_s);
    CHECK(QRect(16, 878, 1048, 1032).contains(burger));        // in the frame's content area
    CHECK(burger.width() > 400);                               // big, two across

    // Touching a laid-out button still does what it does.
    c.activate(u"item-1"_s);
    CHECK(pos.lines().size() == 1);

    // The editor always works on pages as designed.
    c.setFormFactorOverride(u"standard"_s);
    CHECK(c.canvasSize() == QSize(1920, 1080));
    CHECK(shownIds(c).contains(u"tab-lunch"_s));
}

TEST_CASE("Phone or not: forced, per terminal, or from the screen size", "[phone][ui]")
{
    layout::Layout l = seedLayout();
    app::PosData data = test::seedPosData();
    data.settings.terminals.push_back({});
    data.settings.terminals.back().name = "Handheld";
    app::PosShared shared(data, nullptr);
    PosService pos(&shared, u"Handheld"_s);
    LayoutController c(l);
    c.setPos(&pos);

    // Automatic (Android): a short side under 600 is a phone.
    c.setAutoFormFactor(true);
    c.windowResized(412, 915);
    CHECK(c.formFactor() == u"phone"_s);
    c.windowResized(915, 412);                                  // turned sideways: still a phone
    CHECK(c.formFactor() == u"phone"_s);
    c.windowResized(1280, 800);                                 // a tablet
    CHECK(c.formFactor() == u"standard"_s);

    // Manager -> Terminals: this terminal always gets phone pages.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap t = pos.adminRecords(u"terminals"_s).first().toMap();
    t[u"screen"_s] = u"phone"_s;
    REQUIRE(pos.adminSave(u"terminals"_s, 0, t));
    CHECK(pos.screenMode() == u"phone"_s);
    CHECK(c.formFactor() == u"phone"_s);
    t[u"screen"_s] = u"sideways"_s;
    CHECK_FALSE(pos.adminSave(u"terminals"_s, 0, t));

    // The command line wins over both.
    c.setFormFactorOverride(u"standard"_s);
    CHECK(c.formFactor() == u"standard"_s);
    c.setFormFactorOverride(u"auto"_s);
    CHECK(c.formFactor() == u"phone"_s);

    // Desktops don't guess from the window.
    LayoutController desktop(l);
    desktop.windowResized(412, 915);
    CHECK(desktop.formFactor() == u"standard"_s);
}

TEST_CASE("Page versions are saved and checked", "[phone][layout]")
{
    layout::Page p;
    p.id = u"x-phone"_s;
    p.variantOf = u"x"_s;
    p.formFactor = u"phone"_s;
    p.contentArea = QRect(16, 878, 1048, 1032);
    const layout::Page back = layout::Page::fromJson(p.toJson());
    CHECK(back.variantOf == u"x"_s);
    CHECK(back.formFactor == u"phone"_s);
    CHECK(back.contentArea == p.contentArea);

    layout::Layout l = seedLayout();
    layout::Page orphan = *l.page(u"tables-phone"_s);
    orphan.id = u"orphan"_s;
    orphan.variantOf = u"nowhere"_s;
    l.pages.append(orphan);
    layout::Page twin = *l.page(u"tables-phone"_s);
    twin.id = u"tables-phone-2"_s;
    l.pages.append(twin);
    const QStringList issues = l.validate();
    CHECK(issues.filter(u"'nowhere', which does not exist"_s).size() == 1);
    CHECK(issues.filter(u"more than one phone version"_s).size() == 1);
}

TEST_CASE("Close ViewTouch: managers only", "[ui][close]")
{
    PosService pos(test::seedPosData(), nullptr);
    LayoutController c(seedLayout());
    c.setPos(&pos);
    QSignalSpy closing(&c, &LayoutController::closeRequested);

    REQUIRE(pos.loginWithPin(u"1111"_s));                 // a server
    REQUIRE(c.jumpTo(u"library"_s));
    c.activate(u"lib-close"_s);
    CHECK(closing.isEmpty());
    CHECK(c.statusText().contains(u"needs a manager"_s));

    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));                 // a manager
    REQUIRE(c.jumpTo(u"manager"_s));
    c.activate(u"close-app"_s);
    CHECK(closing.size() == 1);
}

TEST_CASE("Phones held upright: every page fits the portrait screen", "[phone][portrait]")
{
    const layout::Layout l = seedLayout();
    PosService pos(test::seedPosData(), nullptr);
    LayoutController c(l);
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // a manager reaches every page
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    c.setFormFactorOverride(u"phone"_s);
    QStringList over, landscape;
    for (const layout::Page &p : l.pages) {
        if (p.formFactor == u"phone" || p.kind == u"template" || p.kind == u"library" || !c.jumpTo(p.id))
            continue;
        const QSize canvas = c.canvasSize();
        if (canvas.width() > canvas.height())
            landscape << p.id;   // shown sideways on a phone
        ZoneModel *m = c.zones();
        for (int r = 0; r < m->rowCount(); ++r) {
            const QModelIndex i = m->index(r);
            const QRect rect(m->data(i, ZoneModel::ZoneXRole).toInt(), m->data(i, ZoneModel::ZoneYRole).toInt(),
                             m->data(i, ZoneModel::ZoneWRole).toInt(), m->data(i, ZoneModel::ZoneHRole).toInt());
            if (!rect.isEmpty() && !QRect(QPoint(0, 0), canvas).contains(rect)) {
                over << p.id + u':' + m->data(i, ZoneModel::ZoneIdRole).toString();
                break;
            }
        }
    }
    INFO("sideways: " << landscape.join(u", "_s).toStdString());
    INFO("past the edge: " << over.join(u", "_s).toStdString());
    CHECK(landscape.isEmpty());
    CHECK(over.isEmpty());
}
