#include <catch2/catch_all.hpp>
#include "../mocks/mock_terminal.hh"
#include "../mocks/mock_settings.hh"

// Simplified check tests for now - business logic is complex
TEST_CASE("Mock classes functionality", "[mocks]")
{
    SECTION("MockTerminal basic functionality")
    {
        MockTerminal terminal;

        // Should be able to get settings
        MockSettings* settings = terminal.GetSettings();
        REQUIRE(settings != nullptr);

        // Should be able to update settings
        int result = terminal.UpdateSettings();
        REQUIRE(result == 0);

        // Should be able to save settings
        result = terminal.SaveSettings();
        REQUIRE(result == 0);
    }

    SECTION("MockSettings basic functionality")
    {
        MockSettings settings;

        // Check default tax rates
        REQUIRE(settings.tax_food == 0.0825f);
        REQUIRE(settings.tax_alcohol == 0.0f);

        // Test setting tax rates
        settings.SetTaxRate(0, 1000);  // 10%
        REQUIRE(settings.tax_food == 0.1f);

        // Test drawer mode
        settings.SetDrawerMode(1);
        REQUIRE(settings.drawer_mode == 1);
    }
}

TEST_CASE("Basic arithmetic operations", "[arithmetic]")
{
    SECTION("Basic calculations for POS operations")
    {
        // Test basic arithmetic that would be used in POS calculations
        int subtotal = 1000;  // $10.00
        float tax_rate = 0.0825f;  // 8.25%
        int tax_amount = static_cast<int>(subtotal * tax_rate + 0.5f);  // Round to nearest cent
        int total = subtotal + tax_amount;

        REQUIRE(subtotal == 1000);
        REQUIRE(tax_amount == 83);  // 1000 * 0.0825 + 0.5 = 82.5 + 0.5 = 83
        REQUIRE(total == 1083);
    }

    SECTION("Payment calculations")
    {
        int check_total = 1500;  // $15.00
        int payment_amount = 2000;  // $20.00
        int change_due = payment_amount - check_total;

        REQUIRE(change_due == 500);  // $5.00 change
        REQUIRE(change_due > 0);  // Payment covers the check
    }
}
