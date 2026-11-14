#include <catch2/catch_test_macros.hpp>

#include "layout/floor_builder.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
namespace fp = vt::layout::floorplan;

// Tables from a few answers (the Tables walk-through).

namespace {

QVariantMap room(const char *id, int two, int four, int six, int seats = 0)
{
    return {{u"id"_s, QString::fromLatin1(id)}, {u"two"_s, two}, {u"four"_s, four}, {u"six"_s, six}, {u"seats"_s, seats}};
}

QStringList tablesOn(const layout::Layout &l, const QString &page)
{
    QStringList out;
    for (const layout::Zone &z : l.page(page)->zones)
        if (z.kind == u"table")
            out << z.label;
    return out;
}

QStringList idsOn(const layout::Layout &l, const QString &page)
{
    QStringList out;
    for (const layout::Zone &z : l.page(page)->zones)
        out << z.id;
    return out;
}

} // namespace

TEST_CASE("Floor builder: names, one count or a letter for each room", "[floorbuilder]")
{
    const QVariantList rooms{room("patio", 0, 2, 0), room("dining", 1, 2, 0), room("bar", 1, 0, 0, 3), room("roof", 0, 9, 0)};
    // Rooms in their own order (dining, bar, patio), an unknown one left out.
    const fp::Answers plain = fp::fromMap({{u"rooms"_s, rooms}});
    REQUIRE(plain.rooms.size() == 3);
    CHECK(plain.rooms[0].id == u"dining"_s);
    CHECK(fp::names(plain) == QStringList{u"T1"_s, u"T2"_s, u"T3"_s, u"T4"_s, u"T5"_s, u"T6"_s,
                                             u"Bar 1"_s, u"Bar 2"_s, u"Bar 3"_s});
    const fp::Answers byRoom = fp::fromMap({{u"rooms"_s, rooms}, {u"byRoom"_s, true}});
    CHECK(fp::names(byRoom) == QStringList{u"D1"_s, u"D2"_s, u"D3"_s, u"B1"_s, u"P1"_s, u"P2"_s,
                                              u"Bar 1"_s, u"Bar 2"_s, u"Bar 3"_s});
    // Counts kept sensible; a room with nothing in it isn't one.
    const fp::Answers odd = fp::fromMap({{u"rooms"_s, QVariantList{room("dining", -3, 500, 0), room("patio", 0, 0, 0)}}});
    REQUIRE(odd.rooms.size() == 1);
    CHECK(odd.rooms[0].four == 60);
    CHECK(odd.rooms[0].two == 0);
}

TEST_CASE("Floor builder: every table in its room, none overlapping", "[floorbuilder]")
{
    const fp::Answers a = fp::fromMap({{u"rooms"_s, QVariantList{room("dining", 2, 6, 1), room("bar", 2, 0, 0, 6),
                                                                       room("patio", 0, 4, 0)}}});
    const fp::Plan all = fp::build(a);
    QList<QRect> tables;
    QMap<QString, QRect> areas;
    for (const layout::Zone &z : all.zones) {
        CHECK(QRect(QPoint(0, 0), all.size).contains(z.rect));
        if (z.kind == u"table")
            tables << z.rect;
        else if (z.props.value(u"decor"_s) == u"area"_s)
            areas.insert(z.id, z.rect);
    }
    CHECK(tables.size() == fp::tableCount(a));
    CHECK(tables.size() == 21);
    for (int i = 0; i < tables.size(); ++i)
        for (int j = i + 1; j < tables.size(); ++j)
            CHECK_FALSE(tables[i].intersects(tables[j]));
    REQUIRE(areas.size() == 3);
    CHECK(areas[u"area-patio"_s].top() > areas[u"area-dining"_s].bottom());   // outside, below
    CHECK(areas[u"area-bar"_s].left() > areas[u"area-dining"_s].right());      // beside the dining room
    // One room alone: its own tables, named as on Main.
    const fp::Plan bar = fp::build(a, u"bar"_s);
    QStringList barNames;
    for (const layout::Zone &z : bar.zones)
        if (z.kind == u"table")
            barNames << z.label;
    CHECK(barNames == QStringList{u"Bar 1"_s, u"Bar 2"_s, u"Bar 3"_s, u"Bar 4"_s, u"Bar 5"_s, u"Bar 6"_s, u"T10"_s, u"T11"_s});
    // Fitted into a box: inside it, and a small plan not blown up past 1.5x.
    const QRect box(16, 104, 1440, 864);
    for (const layout::Zone &z : fp::fit(all, box))
        CHECK(box.adjusted(-1, -1, 1, 1).contains(z.rect));
    const fp::Plan two = fp::build(fp::fromMap({{u"rooms"_s, QVariantList{room("dining", 2, 0, 0)}}}));
    for (const layout::Zone &z : fp::fit(two, box))
        if (z.kind == u"table")
            CHECK(z.rect.width() <= 120 * 1.5 + 1);
}

TEST_CASE("Floor builder: the starter store's floor pages", "[floorbuilder]")
{
    auto loaded = test::loadTestLayout();
    REQUIRE(loaded);
    layout::Layout l = *loaded;
    const QStringList buttonsBefore = [&] {
        QStringList ids;
        for (const layout::Zone &z : l.page(u"tables"_s)->zones)
            if (z.kind != u"table" && z.kind != u"decor" && !z.id.startsWith(u"plan-"))
                ids << z.id;
        return ids;
    }();
    // A dining room and a bar, numbered by room; no patio.
    const fp::Answers a = fp::fromMap({{u"rooms"_s, QVariantList{room("dining", 0, 5, 0), room("bar", 0, 0, 0, 4)}},
                                             {u"byRoom"_s, true}});
    CHECK(fp::apply(l, a) == 4);   // Main, Dining Area, Bar, Patio
    CHECK(tablesOn(l, u"tables"_s) == QStringList{u"D1"_s, u"D2"_s, u"D3"_s, u"D4"_s, u"D5"_s,
                                                  u"Bar 1"_s, u"Bar 2"_s, u"Bar 3"_s, u"Bar 4"_s});
    CHECK(tablesOn(l, u"floor-dining"_s) == QStringList{u"D1"_s, u"D2"_s, u"D3"_s, u"D4"_s, u"D5"_s});
    CHECK(tablesOn(l, u"floor-bar"_s) == QStringList{u"Bar 1"_s, u"Bar 2"_s, u"Bar 3"_s, u"Bar 4"_s});
    CHECK(tablesOn(l, u"floor-patio"_s).isEmpty());
    // The buttons all stay; the Patio tab goes.
    for (const QString &id : buttonsBefore)
        CHECK(idsOn(l, u"tables"_s).contains(id));
    CHECK(idsOn(l, u"tables"_s).contains(u"plan-bar"_s));
    CHECK_FALSE(idsOn(l, u"tables"_s).contains(u"plan-patio"_s));
    CHECK(l.validate().filter(u"error"_s).isEmpty());

    // One room: no tabs at all.
    fp::apply(l, fp::fromMap({{u"rooms"_s, QVariantList{room("dining", 2, 2, 0)}}}));
    CHECK(tablesOn(l, u"tables"_s).size() == 4);
    for (const QString &id : idsOn(l, u"tables"_s))
        CHECK_FALSE(id.startsWith(u"plan-"));
}

TEST_CASE("Floor builder: from the controller, kept tables with open checks, and back", "[floorbuilder]")
{
    auto loaded = test::loadTestLayout();
    REQUIRE(loaded);
    PosService pos(test::seedPosData(), nullptr);
    LayoutController c(*loaded);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QStringList before = [&] {
        QStringList out;
        for (const QVariant &t : c.tables())
            out << t.toMap().value(u"name"_s).toString();
        return out;
    }();
    REQUIRE(before.contains(u"T3"_s));

    // T3 has an open check: a plan without a T3 is refused, and says why.
    REQUIRE(pos.selectTable(u"T3"_s) != PosService::TableFailed);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"coffee"_s);
    pos.releaseCheck();
    const QVariantMap small{{u"rooms"_s, QVariantList{room("dining", 0, 2, 0)}}};
    CHECK(c.previewFloor(small).value(u"count"_s).toInt() == 2);
    CHECK_FALSE(c.buildFloor(small));
    CHECK(c.statusText().contains(u"T3"_s));
    CHECK_FALSE(c.canUndoFloor());

    // With a T3 in it: made; then put back as it was.
    const QVariantMap more{{u"rooms"_s, QVariantList{room("dining", 2, 4, 0)}}};
    REQUIRE(c.buildFloor(more));
    QStringList now;
    for (const QVariant &t : c.tables())
        now << t.toMap().value(u"name"_s).toString();
    CHECK(now == QStringList{u"T1"_s, u"T2"_s, u"T3"_s, u"T4"_s, u"T5"_s, u"T6"_s});
    CHECK(c.canUndoFloor());
    REQUIRE(c.undoFloor());
    QStringList back;
    for (const QVariant &t : c.tables())
        back << t.toMap().value(u"name"_s).toString();
    CHECK(back == before);
}
