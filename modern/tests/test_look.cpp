#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The look of the screens for whoever is here: their text size and text
// effect (Aa), over the screen's and the store's.

TEST_CASE("Aa: anyone sets their own text size and effect, no manager needed", "[look]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));   // a server
    REQUIRE_FALSE(pos.can(u"manager"_s));
    REQUIRE(pos.setMyLook(150, u"embossed"_s));
    CHECK(pos.userPrefs()[u"textSize"_s].toInt() == 150);
    CHECK(pos.userPrefs()[u"textEffect"_s] == u"embossed"_s);
    CHECK(pos.userPrefs()[u"myTextEffect"_s] == u"embossed"_s);
    CHECK_FALSE(pos.setMyLook(170, QString()));          // not one of the sizes
    CHECK_FALSE(pos.setMyLook(100, u"sparkles"_s));      // not an effect
    REQUIRE(pos.setMyLook(90, QString()));
    CHECK(pos.userPrefs()[u"textSize"_s].toInt() == 90);

    // Kept: theirs on the next login, on any screen; nobody else's changed.
    pos.logout();
    PosService other(pos.shared(), u"Bar"_s);
    REQUIRE(other.loginWithPin(u"1111"_s));
    CHECK(other.userPrefs()[u"textSize"_s].toInt() == 90);
    CHECK(std::ranges::count(pos.shared()->employees, 90, &core::Employee::textSize) == 1);

    // Nobody logged in: nothing to set.
    other.logout();
    CHECK_FALSE(other.setMyLook(115, QString()));
}

TEST_CASE("Text effect: the person's, else the screen's, else the store's", "[look]")
{
    app::PosData d = test::seedPosData();
    d.settings.textEffect = "outline";
    core::TerminalConfig t;
    t.id = "t1";
    t.name = "Bar";
    t.textEffect = "embossed";
    d.settings.terminals.push_back(t);
    app::PosShared shared(d, nullptr);
    PosService floor(&shared, u"Floor"_s);
    PosService bar(&shared, u"Bar"_s);
    CHECK(floor.userPrefs()[u"textEffect"_s] == u"outline"_s);   // nobody here: the store's
    CHECK(bar.userPrefs()[u"textEffect"_s] == u"embossed"_s);    // the screen's own
    REQUIRE(bar.loginWithPin(u"1111"_s));
    CHECK(bar.userPrefs()[u"textEffect"_s] == u"embossed"_s);
    REQUIRE(bar.setMyLook(100, u"none"_s));                      // theirs: plain
    CHECK(bar.userPrefs()[u"textEffect"_s].toString().isEmpty());

    // Kept with the settings, the screen and the person.
    const core::PosSettings saved = app::settingsFromJson(app::toJson(shared.settings));
    CHECK(saved.textEffect == "outline");
    CHECK(saved.terminals.back().textEffect == "embossed");
    CHECK(app::textEffectFrom(u"bogus"_s).empty());
}
