#include <catch2/catch_test_macros.hpp>

#include "core/money.hh"

using vt::Money;

TEST_CASE("Money arithmetic", "[money]")
{
    const Money a = Money::fromCents(1250);
    const Money b = Money::fromCents(275);

    CHECK((a + b).cents() == 1525);
    CHECK((a - b).cents() == 975);
    CHECK((b * 3).cents() == 825);
    CHECK((-a).cents() == -1250);
    CHECK(b < a);
}

TEST_CASE("Money percent rounds half away from zero", "[money]")
{
    // 8.25% of $10.00 = 82.5c -> 83c
    CHECK(Money::fromCents(1000).percent(825).cents() == 83);
    // 8.25% of $9.99 = 82.4175c -> 82c
    CHECK(Money::fromCents(999).percent(825).cents() == 82);
    // negative (refund) mirrors positive
    CHECK(Money::fromCents(-1000).percent(825).cents() == -83);
    CHECK(Money::fromCents(0).percent(825).cents() == 0);
}

TEST_CASE("Money formats as decimal string", "[money]")
{
    CHECK(Money::fromCents(1234).toString() == "12.34");
    CHECK(Money::fromCents(5).toString() == "0.05");
    CHECK(Money::fromCents(-5).toString() == "-0.05");
    CHECK(Money::fromCents(100000).toString() == "1000.00");
}
