#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "qt_catch.hh"
#include "storage/layout_store.hh"

#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using vt::layout::Layout;

namespace {

Layout seed()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

int zoneCount(LayoutController &c)
{
    return c.zones()->rowCount();
}

} // namespace

TEST_CASE("Edits stay in the draft until saved", "[editmode]")
{
    QTemporaryDir dir;
    vt::storage::LayoutStore store(dir.filePath(u"vt.db"_s));
    REQUIRE(store.open());
    REQUIRE(store.save(seed()));

    LayoutController c(*store.load());
    c.setStore(&store);
    const int before = zoneCount(c);

    c.enterEditMode();
    REQUIRE(c.editing());
    EditorController *e = c.editor();
    REQUIRE(e);
    CHECK(e->pageId() == u"login"_s);

    const QString id = e->addZone(u"button"_s);
    CHECK(e->selection() == QStringList{id});
    CHECK(zoneCount(c) == before + 1);         // screen shows the draft
    CHECK(c.layout().page(u"login"_s)->zones.size() == before);   // running layout untouched
    CHECK(e->dirty());

    // Touches do nothing while editing.
    QSignalSpy pages(&c, &LayoutController::pageChanged);
    c.activate(u"start"_s);
    CHECK(pages.isEmpty());

    REQUIRE(c.saveEdits());
    CHECK_FALSE(e->dirty());
    CHECK(c.layout().page(u"login"_s)->zone(id));
    CHECK(store.load()->page(u"login"_s)->zone(id));   // persisted

    REQUIRE(c.leaveEditMode(false));
    CHECK_FALSE(c.editing());
    CHECK(zoneCount(c) == before + 1);
}

TEST_CASE("Leaving without saving discards the draft", "[editmode]")
{
    LayoutController c(seed());
    const int before = zoneCount(c);
    c.enterEditMode();
    c.editor()->addZone(u"label"_s);
    c.editor()->newPage(u"Scratch"_s, u"custom"_s, {});
    CHECK(c.pageId() == u"scratch"_s);

    REQUIRE(c.leaveEditMode(false));
    CHECK_FALSE(c.layout().page(u"scratch"_s));
    CHECK(c.pageId() == u"login"_s);            // vanished page -> back/home
    CHECK(zoneCount(c) == before);
}

TEST_CASE("Editor follows page renames and deletions", "[editmode]")
{
    LayoutController c(seed());
    c.showPage(u"settle"_s);
    c.enterEditMode();
    EditorController *e = c.editor();

    REQUIRE(e->setField(u"page"_s, u"id"_s, u"checkout"_s));
    CHECK(c.pageId() == u"checkout"_s);
    CHECK(e->pageId() == u"checkout"_s);

    const QString dup = e->duplicatePage();
    CHECK(c.pageId() == dup);
    REQUIRE(e->deletePage());
    CHECK(c.activeLayout().page(c.pageId()));   // landed somewhere valid

    // Undo brings the page back.
    e->undo();
    CHECK(c.activeLayout().page(dup));

    REQUIRE(c.showPage(u"checkout"_s));
    CHECK_FALSE(e->setField(u"page"_s, u"role"_s, u"login"_s));
    CHECK(e->notice().contains(u"already"_s));
}

TEST_CASE("Selection survives undo only for zones that still exist", "[editmode]")
{
    LayoutController c(seed());
    c.showPage(u"tables"_s);
    c.enterEditMode();
    EditorController *e = c.editor();

    e->selectOnly({u"quick"_s, u"takeout"_s});
    e->duplicateSelection();
    const QStringList copies = e->selection();
    REQUIRE(copies.size() == 2);
    e->undo();
    CHECK(e->selection().isEmpty());

    e->selectInRect(1460, 10, 460, 330, false);   // band over Quick Order and Takeout
    CHECK(e->selection().contains(u"quick"_s));
    CHECK(e->selection().contains(u"takeout"_s));
    CHECK_FALSE(e->selection().contains(u"checks"_s));
}

TEST_CASE("Inspector field info: values, mixed, resolved", "[editmode]")
{
    LayoutController c(seed());
    c.showPage(u"items-burgers"_s);
    c.enterEditMode();
    EditorController *e = c.editor();

    e->selectOnly({u"item-1"_s});
    QVariantMap info = e->fieldInfo(u"zone"_s, u"style.normal.fill"_s);
    CHECK(info[u"value"_s].toString() == u"#a86a12"_s);
    CHECK(info[u"resolved"_s].toString() == u"#a86a12"_s);
    info = e->fieldInfo(u"zone"_s, u"style.normal.fontSize"_s);
    CHECK_FALSE(info[u"isSet"_s].toBool());
    CHECK(info[u"resolved"_s].toInt() == 30);   // from theme

    e->selectOnly({u"item-1"_s, u"title"_s});
    CHECK(e->fieldInfo(u"zone"_s, u"label"_s)[u"mixed"_s].toBool());
    CHECK(e->selectionKind().isEmpty());        // button + label

    REQUIRE(e->setField(u"zone"_s, u"style.normal.fontSize"_s, 22));
    CHECK(e->fieldInfo(u"zone"_s, u"style.normal.fontSize"_s)[u"value"_s].toInt() == 22);
    REQUIRE(e->clearField(u"zone"_s, u"style.normal.fontSize"_s));
    CHECK_FALSE(e->fieldInfo(u"zone"_s, u"style.normal.fontSize"_s)[u"isSet"_s].toBool());

    // Page background inherits the theme color.
    const QVariantMap bg = e->fieldInfo(u"page"_s, u"background.fill"_s);
    CHECK_FALSE(bg[u"isSet"_s].toBool());
    CHECK(bg[u"resolved"_s].toString() == u"#171a1f"_s);

    // Inherited zone -> its template.
    CHECK(e->templateOf(u"flow-pay"_s) == u"order-template"_s);
    CHECK(e->templateOf(u"item-1"_s).isEmpty());
}

TEST_CASE("Action list round-trips through the inspector API", "[editmode]")
{
    LayoutController c(seed());
    c.enterEditMode();
    EditorController *e = c.editor();
    e->selectOnly({u"start"_s});

    QVariantList actions = e->actions();
    REQUIRE(actions.size() == 1);
    CHECK(actions[0].toMap()[u"type"_s].toString() == u"command"_s);

    actions.append(QVariantMap{{u"type"_s, u"command"_s}, {u"name"_s, u"clockIn"_s}});
    e->setActions(actions);
    CHECK(e->actions().size() == 2);
    CHECK(c.activeLayout().validate() == QStringList{});
}

TEST_CASE("Manager 'Edit Pages' button enters edit mode", "[editmode]")
{
    LayoutController c(seed());
    c.showPage(u"manager"_s);
    c.activate(u"edit-pages"_s);
    CHECK(c.editing());
    CHECK(c.editor()->pageId() == u"manager"_s);
}
