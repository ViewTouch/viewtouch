#include <catch2/catch_all.hpp>
#include "src/utils/safe_math.hh"
#include <limits>

TEST_CASE("Safe multiplication helpers", "[safe_math]")
{
    SECTION("small multiplication succeeds")
    {
        size_t out = 0;
        bool ok = vt::safe_math::safe_mul_size_t(100, 200, out);
        REQUIRE(ok == true);
        REQUIRE(out == 20000);
    }

    SECTION("multiplication overflow detected")
    {
        size_t out = 0;
        size_t max = std::numeric_limits<size_t>::max();
        // Choose operands such that their product would exceed size_t
        size_t a = max / 2 + 1;
        size_t b = 3;
        bool ok = vt::safe_math::safe_mul_size_t(a, b, out);
        REQUIRE(ok == false);
    }

    SECTION("three-operand multiplication succeeds and fails appropriately")
    {
        size_t out = 0;
        bool ok = vt::safe_math::safe_mul3_size_t(10, 20, 30, out);
        REQUIRE(ok == true);
        REQUIRE(out == 6000);

        size_t max = std::numeric_limits<size_t>::max();
        size_t a = max / 100 + 1;
        size_t b = 200;
        size_t c = 300;
        ok = vt::safe_math::safe_mul3_size_t(a, b, c, out);
        REQUIRE(ok == false);
    }
}
