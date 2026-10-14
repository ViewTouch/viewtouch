#include <catch2/catch_test_macros.hpp>

#include "layout/menu_screens.hh"
#include "layout_fixture.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using namespace vt::layout;

namespace {

Zone button(const QString &id, QRect r, const QString &target)
{
    Zone z;
    z.id = id;
    z.label = id;
    z.rect = r;
    Action a;
    a.data = {{u"type"_s, u"jump"_s}, {u"page"_s, target}, {u"mode"_s, u"replace"_s}};
    z.actions.append(a);
    return z;
}

// A store set up before the menu screens filled themselves: the Lunch page
// with category buttons placed by hand, and an Everything page.
Layout oldStore()
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    Page *lunch = l->page(u"index-lunch"_s);
    REQUIRE(lunch);
    lunch->zones.clear();
    Zone title;
    title.id = u"title"_s;
    title.kind = u"label"_s;
    title.rect = QRect(592, 104, 1312, 72);
    lunch->zones = {title,
                    button(u"cat-burgers"_s, QRect(592, 192, 424, 240), u"items-burgers"_s),
                    button(u"cat-salads"_s, QRect(1036, 192, 424, 240), u"items-salads"_s),
                    button(u"cat-drinks"_s, QRect(1480, 192, 424, 240), u"items-drinks"_s),
                    button(u"everything"_s, QRect(592, 452, 424, 240), u"menu-all"_s)};
    l->page(u"menu-all"_s)->role.clear();
    return *l;
}

} // namespace

TEST_CASE("Old menu screens become self-filling, the rest of the page kept", "[menuscreens]")
{
    const Layout before = oldStore();
    REQUIRE(hasHandBuiltMenu(before));
    const Layout after = withSelfFillingMenu(before);
    CHECK_FALSE(hasHandBuiltMenu(after));

    const Page *lunch = after.page(u"index-lunch"_s);
    REQUIRE(lunch);
    CHECK_FALSE(lunch->zone(u"cat-burgers"_s));                 // the hand-placed buttons: gone
    CHECK(lunch->zone(u"everything"_s));                        // Everything stays (it's self-filling)
    CHECK(lunch->zone(u"title"_s));
    const Zone *panel = lunch->zone(u"categories"_s);
    REQUIRE(panel);
    CHECK(panel->kind == u"menuCategories"_s);
    CHECK(panel->rect == QRect(592, 192, 1312, 240));           // where the buttons were
    CHECK(panel->props.value(u"period"_s).toString() == u"lunch"_s);

    // The Everything page is now the menu page; no second one.
    CHECK(after.pageByRole(u"menu"_s) == after.page(u"menu-all"_s));
    CHECK(after.pages.size() == before.pages.size());
    // Hand-built item pages stay, as pages of their own.
    CHECK(after.page(u"items-burgers"_s));

    // Again: nothing more to do.
    CHECK(withSelfFillingMenu(after).pages == after.pages);
}

TEST_CASE("A store with no menu page gets one", "[menuscreens]")
{
    Layout l = oldStore();
    l.pages.removeIf([](const Page &p) { return p.id == u"menu-all"_s; });
    // Its Everything button now goes nowhere; the categories still switch.
    const Layout after = withSelfFillingMenu(l);
    const Page *menu = after.pageByRole(u"menu"_s);
    REQUIRE(menu);
    CHECK(menu->kind == u"items"_s);
    CHECK(menu->templateId == u"order-template"_s);
    REQUIRE(menu->zones.size() == 1);
    CHECK(menu->zones[0].kind == u"menuGrid"_s);
    CHECK(menu->zones[0].rect == QRect(592, 104, 1312, 328));   // the buttons' area, up to the title
}

TEST_CASE("The starter screens need no switching", "[menuscreens]")
{
    auto l = Layout::loadDirectory(QStringLiteral(VTM_SEED_DIR));
    REQUIRE(l);
    CHECK_FALSE(hasHandBuiltMenu(*l));
}
