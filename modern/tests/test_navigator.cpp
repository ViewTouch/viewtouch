#include <catch2/catch_test_macros.hpp>

#include "app/navigator.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using vt::app::Navigator;
using Mode = Navigator::Mode;

namespace {

vt::layout::Layout pages()
{
    vt::layout::Layout l;
    for (const auto &[id, kind] : {std::pair{"login", "login"}, {"tables", "tables"},
                                   {"lunch", "index"}, {"dinner", "index"}, {"burgers", "items"},
                                   {"temp", "modifier"}, {"side", "modifier"}, {"settle", "settle"}}) {
        vt::layout::Page p;
        p.id = QString::fromLatin1(id);
        p.kind = QString::fromLatin1(kind);
        l.pages.append(p);
    }
    return l;
}

} // namespace

TEST_CASE("parseMode", "[nav]")
{
    CHECK(Navigator::parseMode({}) == Mode::Push);
    CHECK(Navigator::parseMode(u"replace"_s) == Mode::Replace);
    CHECK(Navigator::parseMode(u"sequence"_s) == Mode::Sequence);
    CHECK_FALSE(Navigator::parseMode(u"sideways"_s));
}

TEST_CASE("push, replace, back, home", "[nav]")
{
    const auto l = pages();
    Navigator nav(l);
    nav.reset(u"login"_s);
    CHECK(nav.current() == u"login"_s);
    CHECK_FALSE(nav.canGoBack());

    CHECK(nav.jump(Mode::Push, u"tables"_s));
    CHECK(nav.jump(Mode::Push, u"lunch"_s));
    CHECK(nav.jump(Mode::Replace, u"burgers"_s));   // replace does not remember lunch
    CHECK(nav.depth() == 2);

    CHECK(nav.jump(Mode::Back));
    CHECK(nav.current() == u"tables"_s);
    CHECK(nav.jump(Mode::Back));
    CHECK(nav.current() == u"login"_s);

    // Back on an empty stack goes home (legacy PopPage -> login).
    nav.jump(Mode::Push, u"settle"_s);
    nav.jump(Mode::Home);
    CHECK(nav.current() == u"login"_s);
    CHECK(nav.depth() == 0);
    nav.jump(Mode::Replace, u"settle"_s);
    CHECK(nav.jump(Mode::Back));
    CHECK(nav.current() == u"login"_s);
}

TEST_CASE("jumps to missing pages are ignored", "[nav]")
{
    const auto l = pages();
    Navigator nav(l);
    nav.reset(u"login"_s);
    CHECK_FALSE(nav.jump(Mode::Push, u"nowhere"_s));
    CHECK(nav.current() == u"login"_s);
    CHECK(nav.depth() == 0);
}

TEST_CASE("index returns to the last index page visited", "[nav]")
{
    const auto l = pages();
    Navigator nav(l);
    nav.reset(u"login"_s);

    // Nothing visited yet: first index page.
    CHECK(nav.jump(Mode::Index));
    CHECK(nav.current() == u"lunch"_s);

    nav.jump(Mode::Replace, u"dinner"_s);
    nav.jump(Mode::Replace, u"burgers"_s);
    CHECK(nav.lastIndex() == u"dinner"_s);
    CHECK(nav.jump(Mode::Index));
    CHECK(nav.current() == u"dinner"_s);
}

TEST_CASE("modifier sequence visits each page then returns", "[nav]")
{
    const auto l = pages();
    Navigator nav(l);
    nav.reset(u"login"_s);
    nav.jump(Mode::Push, u"tables"_s);
    nav.jump(Mode::Replace, u"burgers"_s);

    REQUIRE(nav.startSequence({u"temp"_s, u"ghost"_s, u"side"_s}));   // missing pages skipped
    CHECK(nav.current() == u"temp"_s);
    nav.jump(Mode::Sequence);
    CHECK(nav.current() == u"side"_s);
    nav.jump(Mode::Sequence);
    CHECK(nav.current() == u"burgers"_s);
    nav.jump(Mode::Back);
    CHECK(nav.current() == u"login"_s);

    CHECK_FALSE(nav.startSequence({u"ghost"_s}));
    CHECK(nav.current() == u"login"_s);
}

TEST_CASE("stack depth is capped", "[nav]")
{
    const auto l = pages();
    Navigator nav(l);
    nav.reset(u"login"_s);
    for (int i = 0; i < 100; ++i)
        nav.jump(Mode::Push, i % 2 ? u"tables"_s : u"settle"_s);
    CHECK(nav.depth() == Navigator::MaxDepth);
}

TEST_CASE("index prefers the current meal period when none was visited", "[nav]")
{
    auto l = pages();
    l.page(u"lunch"_s)->mealPeriod = u"lunch"_s;
    l.page(u"dinner"_s)->mealPeriod = u"dinner"_s;
    Navigator nav(l);
    nav.setMealPeriod(u"dinner"_s);
    nav.reset(u"login"_s);

    CHECK(nav.jump(Mode::Index));
    CHECK(nav.current() == u"dinner"_s);

    // Once an index page was visited, it wins over the meal period.
    nav.jump(Mode::Replace, u"lunch"_s);
    nav.jump(Mode::Replace, u"burgers"_s);
    CHECK(nav.jump(Mode::Index));
    CHECK(nav.current() == u"lunch"_s);
}
