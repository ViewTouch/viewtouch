#include <catch2/catch_all.hpp>
#include "../mocks/mock_settings.hh"
#include "main/data/settings.hh"

TEST_CASE("Settings basic functionality", "[settings]")
{
    SECTION("Settings initialization")
    {
        MockSettings settings;

        // Check default values
        REQUIRE(settings.tax_food == 0.0825f);  // 8.25%
        REQUIRE(settings.drawer_mode == 0);  // Trusted
        REQUIRE(settings.receipt_print == 1);  // On finalize
        REQUIRE(settings.time_format == 0);   // 12-hour
        REQUIRE(settings.date_format == 0);   // MMDDYY
    }

    SECTION("Tax rate setting")
    {
        MockSettings settings;

        settings.SetTaxRate(0, 1000);  // 10%
        REQUIRE(settings.tax_food == 0.1f);

        settings.SetTaxRate(1, 500);   // 5%
        REQUIRE(settings.tax_alcohol == 0.05f);

        // Test bounds checking (should not crash)
        settings.SetTaxRate(-1, 100);
        settings.SetTaxRate(4, 100);
        // Values should remain unchanged
        REQUIRE(settings.tax_food == 0.1f);
        REQUIRE(settings.tax_alcohol == 0.05f);
    }

    SECTION("Drawer mode setting")
    {
        MockSettings settings;

        settings.SetDrawerMode(1);  // Assigned
        REQUIRE(settings.drawer_mode == 1);

        settings.SetDrawerMode(2);  // ServerBank
        REQUIRE(settings.drawer_mode == 2);
    }
}

TEST_CASE("Settings validation", "[settings]")
{
    SECTION("Valid tax rates")
    {
        MockSettings settings;

        // Test valid tax rates (0% to 99%)
        settings.SetTaxRate(0, 0);     // 0%
        REQUIRE(settings.tax_food == 0.0f);

        settings.SetTaxRate(0, 2500);  // 25%
        REQUIRE(settings.tax_food == 0.25f);

        settings.SetTaxRate(0, 9900);  // 99%
        REQUIRE(settings.tax_food == 0.99f);
    }

    SECTION("Invalid tax rates are handled")
    {
        MockSettings settings;

        // These should be handled gracefully (implementation dependent)
        settings.SetTaxRate(0, -100);  // Negative
        settings.SetTaxRate(0, 10000); // Over 100%

        // Settings should remain in valid state
        REQUIRE(settings.tax_food >= 0.0f);
    }
}
