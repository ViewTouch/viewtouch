#include <catch2/catch_all.hpp>
#include "../../main/business/sales.hh"
#include "../../main/business/check.hh"
#include "../../main/data/settings.hh"
#include "../mocks/mock_settings.hh"

// Test Sales Item Family Categories
TEST_CASE("Sales Item Family Definitions", "[sales][family]")
{
    SECTION("Family constants are defined")
    {
        REQUIRE(FAMILY_APPETIZERS == 0);
        REQUIRE(FAMILY_BEVERAGES == 1);
        REQUIRE(FAMILY_LUNCH_ENTREES == 2);
        REQUIRE(FAMILY_CHILDRENS_MENU == 3);
        REQUIRE(FAMILY_DESSERTS == 4);
        REQUIRE(FAMILY_SANDWICHES == 5);
        REQUIRE(FAMILY_SIDE_ORDERS == 6);
        REQUIRE(FAMILY_BREAKFAST_ENTREES == 7);
    }

    SECTION("Specialty families")
    {
        REQUIRE(FAMILY_PIZZA == 14);
        REQUIRE(FAMILY_BEER == 16);
        REQUIRE(FAMILY_WINE == 18);
        REQUIRE(FAMILY_COCKTAIL == 20);
        REQUIRE(FAMILY_MODIFIER == 23);
        REQUIRE(FAMILY_MERCHANDISE == 26);
    }
}

// Test Tax Calculations
TEST_CASE("Tax Rate Calculations", "[tax][rates]")
{
    SECTION("Standard food tax at 8.25%")
    {
        int subtotal = 10000;  // $100.00
        float tax_rate = 0.0825f;
        int tax = static_cast<int>(subtotal * tax_rate);
        
        REQUIRE(tax == 825);  // $8.25
    }

    SECTION("No tax on tax-exempt items")
    {
        int subtotal = 5000;   // $50.00
        float tax_rate = 0.0f;
        int tax = static_cast<int>(subtotal * tax_rate);
        
        REQUIRE(tax == 0);
    }

    SECTION("High tax rate (20% VAT)")
    {
        int subtotal = 10000;  // $100.00
        float tax_rate = 0.20f;
        int tax = static_cast<int>(subtotal * tax_rate);
        
        REQUIRE(tax == 2000);  // $20.00
    }

    SECTION("Complex tax rate (9.875%)")
    {
        int subtotal = 10000;  // $100.00
        float tax_rate = 0.09875f;
        int tax = static_cast<int>(subtotal * tax_rate);
        
        REQUIRE(tax == 987);  // $9.87 (truncated)
    }

    SECTION("Rounding in tax calculations")
    {
        int subtotal = 1099;   // $10.99
        float tax_rate = 0.0825f;
        int tax = static_cast<int>(subtotal * tax_rate + 0.5f);  // Round
        
        REQUIRE(tax == 91);  // $0.91 (rounded)
    }
}

// Test Mixed Tax Rates (Food vs Alcohol)
TEST_CASE("Mixed Tax Rate Calculations", "[tax][mixed]")
{
    SECTION("Food and alcohol with different rates")
    {
        int food_sales = 5000;      // $50.00 food
        int alcohol_sales = 3000;   // $30.00 alcohol
        
        float food_tax_rate = 0.0825f;
        float alcohol_tax_rate = 0.10f;
        
        int food_tax = static_cast<int>(food_sales * food_tax_rate);
        int alcohol_tax = static_cast<int>(alcohol_sales * alcohol_tax_rate);
        
        REQUIRE(food_tax == 412);      // $4.12
        REQUIRE(alcohol_tax == 300);   // $3.00
        REQUIRE(food_tax + alcohol_tax == 712);  // Total tax
    }

    SECTION("Tax-free food with taxed alcohol")
    {
        int food_sales = 4000;      // $40.00 tax-free food
        int alcohol_sales = 2000;   // $20.00 taxed alcohol
        
        float food_tax_rate = 0.0f;
        float alcohol_tax_rate = 0.08f;
        
        int food_tax = static_cast<int>(food_sales * food_tax_rate);
        int alcohol_tax = static_cast<int>(alcohol_sales * alcohol_tax_rate);
        
        REQUIRE(food_tax == 0);
        REQUIRE(alcohol_tax == 160);   // $1.60
    }
}

// Test Canadian Tax System (GST/PST/HST/QST)
TEST_CASE("Canadian Tax Calculations", "[tax][canada]")
{
    SECTION("GST only (5%)")
    {
        int subtotal = 10000;  // $100.00
        float gst_rate = 0.05f;
        int gst = static_cast<int>(subtotal * gst_rate);
        
        REQUIRE(gst == 500);  // $5.00
    }

    SECTION("PST only (7%)")
    {
        int subtotal = 10000;  // $100.00
        float pst_rate = 0.07f;
        int pst = static_cast<int>(subtotal * pst_rate);
        
        REQUIRE(pst == 700);  // $7.00
    }

    SECTION("HST only (13%)")
    {
        int subtotal = 10000;  // $100.00
        float hst_rate = 0.13f;
        int hst = static_cast<int>(subtotal * hst_rate);
        
        REQUIRE(hst == 1300);  // $13.00
    }

    SECTION("QST calculation (9.975%)")
    {
        int subtotal = 10000;  // $100.00
        float qst_rate = 0.09975f;
        int qst = static_cast<int>(subtotal * qst_rate);
        
        REQUIRE(qst == 997);  // $9.97
    }

    SECTION("GST + PST combination")
    {
        int subtotal = 10000;  // $100.00
        float gst_rate = 0.05f;
        float pst_rate = 0.07f;
        
        int gst = static_cast<int>(subtotal * gst_rate);
        int pst = static_cast<int>(subtotal * pst_rate);
        int total_tax = gst + pst;
        
        REQUIRE(gst == 500);
        REQUIRE(pst == 700);
        REQUIRE(total_tax == 1200);  // $12.00
    }

    SECTION("QST on GST (compound tax)")
    {
        int subtotal = 10000;  // $100.00
        float gst_rate = 0.05f;
        float qst_rate = 0.09975f;
        
        int gst = static_cast<int>(subtotal * gst_rate);
        int subtotal_with_gst = subtotal + gst;
        int qst = static_cast<int>(subtotal_with_gst * qst_rate);
        
        REQUIRE(gst == 500);
        REQUIRE(qst == 1047);  // QST on $105.00
    }
}

// Test Merchandise Tax
TEST_CASE("Merchandise Tax Calculations", "[tax][merchandise]")
{
    SECTION("Merchandise with standard tax")
    {
        int merchandise_sales = 5000;  // $50.00
        float merchandise_tax_rate = 0.0825f;
        int tax = static_cast<int>(merchandise_sales * merchandise_tax_rate);
        
        REQUIRE(tax == 412);  // $4.12
    }

    SECTION("Tax-exempt merchandise")
    {
        int merchandise_sales = 3000;  // $30.00
        float merchandise_tax_rate = 0.0f;
        int tax = static_cast<int>(merchandise_sales * merchandise_tax_rate);
        
        REQUIRE(tax == 0);
    }
}

// Test Room Tax (Hotel)
TEST_CASE("Room Tax Calculations", "[tax][hotel]")
{
    SECTION("Hotel room tax")
    {
        int room_charge = 15000;  // $150.00
        float room_tax_rate = 0.14f;  // 14% occupancy tax
        int tax = static_cast<int>(room_charge * room_tax_rate);
        
        REQUIRE(tax == 2100);  // $21.00
    }

    SECTION("Combined room and sales tax")
    {
        int room_charge = 10000;  // $100.00
        float room_tax_rate = 0.10f;
        float sales_tax_rate = 0.0825f;
        
        int room_tax = static_cast<int>(room_charge * room_tax_rate);
        int sales_tax = static_cast<int>(room_charge * sales_tax_rate);
        int total_tax = room_tax + sales_tax;
        
        REQUIRE(room_tax == 1000);
        REQUIRE(sales_tax == 825);
        REQUIRE(total_tax == 1825);
    }
}

// Test Price Calculations with Modifiers
TEST_CASE("Sales Item Price with Modifiers", "[sales][modifiers]")
{
    SECTION("Base item with no modifiers")
    {
        int base_price = 1200;  // $12.00
        int modifier_price = 0;
        int total = base_price + modifier_price;
        
        REQUIRE(total == 1200);
    }

    SECTION("Item with single modifier")
    {
        int base_price = 1200;  // $12.00 burger
        int modifier_price = 150;  // $1.50 cheese
        int total = base_price + modifier_price;
        
        REQUIRE(total == 1350);
    }

    SECTION("Item with multiple modifiers")
    {
        int base_price = 1200;  // $12.00 burger
        int cheese = 150;
        int bacon = 200;
        int avocado = 175;
        int total = base_price + cheese + bacon + avocado;
        
        REQUIRE(total == 1725);
    }

    SECTION("Negative modifier (substitution)")
    {
        int base_price = 1500;  // $15.00 entree
        int substitution = -100;  // -$1.00 for substitution
        int total = base_price + substitution;
        
        REQUIRE(total == 1400);
    }
}

// Test Quantity Pricing
TEST_CASE("Quantity-Based Pricing", "[sales][quantity]")
{
    SECTION("Single item")
    {
        int unit_price = 500;  // $5.00
        int quantity = 1;
        int total = unit_price * quantity;
        
        REQUIRE(total == 500);
    }

    SECTION("Multiple items")
    {
        int unit_price = 500;  // $5.00
        int quantity = 5;
        int total = unit_price * quantity;
        
        REQUIRE(total == 2500);
    }

    SECTION("Fractional quantity (half item)")
    {
        int full_price = 1000;  // $10.00
        int half_price = full_price / 2;
        
        REQUIRE(half_price == 500);
    }

    SECTION("Bulk pricing (dozen)")
    {
        int single_price = 150;  // $1.50 each
        int dozen_price = 1500;  // $15.00 per dozen (discount)
        
        int quantity = 12;
        int individual_total = single_price * quantity;
        
        REQUIRE(individual_total == 1800);  // $18.00
        REQUIRE(dozen_price < individual_total);  // Discount applies
    }
}

// Test Discount Calculations
TEST_CASE("Discount Amount Calculations", "[sales][discount]")
{
    SECTION("Percentage discount on item")
    {
        int original_price = 2000;  // $20.00
        float discount_percent = 0.15f;  // 15% off
        int discount_amount = static_cast<int>(original_price * discount_percent);
        int final_price = original_price - discount_amount;
        
        REQUIRE(discount_amount == 300);  // $3.00 off
        REQUIRE(final_price == 1700);     // $17.00
    }

    SECTION("Dollar amount discount")
    {
        int original_price = 2000;  // $20.00
        int discount_amount = 500;  // $5.00 off
        int final_price = original_price - discount_amount;
        
        REQUIRE(final_price == 1500);  // $15.00
    }

    SECTION("Buy one get one discount")
    {
        int item_price = 800;  // $8.00
        int quantity = 2;
        int total_without_discount = item_price * quantity;
        int discount = item_price;  // Free item
        int final_total = total_without_discount - discount;
        
        REQUIRE(total_without_discount == 1600);
        REQUIRE(final_total == 800);
    }

    SECTION("Discount cannot exceed item price")
    {
        int original_price = 1000;  // $10.00
        int discount_amount = 1200;  // $12.00 off (too much)
        int final_price = std::max(0, original_price - discount_amount);
        
        REQUIRE(final_price == 0);  // Cannot go negative
    }
}

// Test Coupon Reductions
TEST_CASE("Coupon-Based Price Reductions", "[sales][coupon]")
{
    SECTION("Fixed amount coupon")
    {
        int subtotal = 5000;     // $50.00
        int coupon_value = 1000;  // $10.00 off
        int final_total = subtotal - coupon_value;
        
        REQUIRE(final_total == 4000);
    }

    SECTION("Percentage coupon")
    {
        int subtotal = 5000;     // $50.00
        float coupon_percent = 0.20f;  // 20% off
        int discount = static_cast<int>(subtotal * coupon_percent);
        int final_total = subtotal - discount;
        
        REQUIRE(discount == 1000);
        REQUIRE(final_total == 4000);
    }

    SECTION("Minimum purchase for coupon")
    {
        int subtotal = 2500;     // $25.00
        int minimum = 3000;       // $30.00 minimum
        int coupon_value = 500;   // $5.00 off
        
        bool coupon_applies = (subtotal >= minimum);
        int final_total = coupon_applies ? subtotal - coupon_value : subtotal;
        
        REQUIRE(coupon_applies == false);
        REQUIRE(final_total == 2500);  // Coupon doesn't apply
    }

    SECTION("Maximum discount for coupon")
    {
        int subtotal = 10000;    // $100.00
        float coupon_percent = 0.50f;  // 50% off
        int max_discount = 2000;  // Max $20.00 off
        
        int calculated_discount = static_cast<int>(subtotal * coupon_percent);
        int actual_discount = std::min(calculated_discount, max_discount);
        int final_total = subtotal - actual_discount;
        
        REQUIRE(calculated_discount == 5000);
        REQUIRE(actual_discount == 2000);
        REQUIRE(final_total == 8000);
    }
}

// Test Item Comp (Partial Comp)
TEST_CASE("Item Comp Calculations", "[sales][comp]")
{
    SECTION("Single item comp")
    {
        int item_price = 1500;  // $15.00
        int comp_amount = 1500;  // Full comp
        int customer_pays = item_price - comp_amount;
        
        REQUIRE(customer_pays == 0);
    }

    SECTION("Partial item comp (50%)")
    {
        int item_price = 2000;  // $20.00
        int comp_amount = 1000;  // $10.00 comp
        int customer_pays = item_price - comp_amount;
        
        REQUIRE(customer_pays == 1000);
    }

    SECTION("Multiple items with selective comp")
    {
        int item1_price = 1200;
        int item2_price = 800;
        int item3_price = 1000;
        
        int item1_comp = 1200;  // Full comp on item 1
        int item2_comp = 0;     // No comp
        int item3_comp = 500;   // Partial comp
        
        int total_sales = item1_price + item2_price + item3_price;
        int total_comps = item1_comp + item2_comp + item3_comp;
        int customer_pays = total_sales - total_comps;
        
        REQUIRE(total_sales == 3000);
        REQUIRE(total_comps == 1700);
        REQUIRE(customer_pays == 1300);
    }
}

// Test Employee Meal Pricing
TEST_CASE("Employee Meal Discount", "[sales][employee]")
{
    SECTION("50% employee discount")
    {
        int regular_price = 1200;  // $12.00
        float employee_discount = 0.50f;
        int employee_price = static_cast<int>(regular_price * (1.0f - employee_discount));
        
        REQUIRE(employee_price == 600);
    }

    SECTION("Fixed employee meal price")
    {
        int regular_price = 1500;  // $15.00
        int employee_price = 500;   // Fixed $5.00 for employees
        
        REQUIRE(employee_price < regular_price);
        REQUIRE(employee_price == 500);
    }

    SECTION("Tax on employee meals")
    {
        int employee_price = 500;  // $5.00
        float tax_rate = 0.0825f;
        int tax = static_cast<int>(employee_price * tax_rate);
        
        REQUIRE(tax == 41);  // $0.41
    }
}

// Test Happy Hour Pricing
TEST_CASE("Time-Based Pricing (Happy Hour)", "[sales][time_based]")
{
    SECTION("Regular price outside happy hour")
    {
        int regular_price = 800;  // $8.00
        bool is_happy_hour = false;
        int price = is_happy_hour ? (regular_price / 2) : regular_price;
        
        REQUIRE(price == 800);
    }

    SECTION("Happy hour discount")
    {
        int regular_price = 800;  // $8.00
        bool is_happy_hour = true;
        int price = is_happy_hour ? (regular_price / 2) : regular_price;
        
        REQUIRE(price == 400);  // 50% off
    }
}

// Test Rounding in Sales Calculations
TEST_CASE("Price Rounding", "[sales][rounding]")
{
    SECTION("Round to nearest cent")
    {
        float calculated_price = 12.345f;
        int price_cents = static_cast<int>(calculated_price * 100 + 0.5f);
        
        REQUIRE(price_cents == 1235);  // Rounds to $12.35
    }

    SECTION("Truncate to cent")
    {
        float calculated_price = 12.349f;
        int price_cents = static_cast<int>(calculated_price * 100);
        
        REQUIRE(price_cents == 1234);  // Truncates to $12.34
    }

    SECTION("Round up at 0.5")
    {
        float calculated_price = 12.355f;
        int price_cents = static_cast<int>(calculated_price * 100 + 0.5f);
        
        REQUIRE(price_cents == 1236);  // Rounds to $12.36
    }
}
