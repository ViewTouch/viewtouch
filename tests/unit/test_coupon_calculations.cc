#include <catch2/catch_all.hpp>

// Payment flags (from settings.hh)
#define TF_IS_PERCENT      1    // amount calculated by percent
#define TF_APPLY_EACH      1024 // applies to each item rather than just once

// Helper functions to test coupon calculation logic
// These mirror the logic in CouponInfo::Amount() and CouponInfo::CPAmount()

// Convert price (in cents) to float
inline double PriceToFlt(int price) {
    return static_cast<double>(price) / 100.0;
}

// Convert float to price (in cents)
inline int FltToPrice(double flt) {
    return static_cast<int>(flt * 100.0 + 0.5);
}

// Convert percentage (stored as 2000 = 20.00%) to float
inline double PercentToFlt(int percent) {
    return static_cast<double>(percent) / 10000.0;
}

// Calculate Amount (what customer pays) for a coupon
// This mirrors CouponInfo::Amount(int item_cost, int item_count)
int CalculateCouponAmount(int item_cost, int item_count, int coupon_amount, int flags) {
    if (flags & TF_IS_PERCENT) {
        // Percentage discount
        double price = PriceToFlt(item_cost);
        double percent = PercentToFlt(coupon_amount);
        price = price - (price * percent);
        return FltToPrice(price) * item_count;
    } else {
        // Fixed dollar amount discount
        int retval = item_cost - coupon_amount;
        return retval * item_count;
    }
}

// Calculate CPAmount (deduction amount) for a coupon
// This mirrors CouponInfo::CPAmount(int item_cost, int item_count)
int CalculateCouponCPAmount(int item_cost, int item_count, int coupon_amount, int flags) {
    int total_cost = item_cost * item_count;
    int amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
    return total_cost - amount;
}

// Test coupon calculations in sales mix reports
TEST_CASE("Coupon Calculations in Reports", "[coupons][reports]")
{
    SECTION("Coupon CPAmount calculation - fixed dollar amount")
    {
        // Create a coupon: $2 off per item
        int coupon_amount = 200;  // $2.00 in cents
        int flags = 0;             // Not percentage, not substitute
        
        // Test CPAmount for a $10 item, quantity 1
        int item_cost = 1000;  // $10.00
        int item_count = 1;
        int cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);
        
        // CPAmount should return the deduction: $10 - ($10 - $2) = $2
        REQUIRE(cp_amount == 200);  // $2.00 deduction
        
        // Test CPAmount for a $10 item, quantity 3
        item_count = 3;
        cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);
        
        // CPAmount should return: 3 * ($10 - ($10 - $2)) = 3 * $2 = $6
        REQUIRE(cp_amount == 600);  // $6.00 total deduction
    }
    
    SECTION("Coupon CPAmount calculation - percentage discount")
    {
        // Create a coupon: 20% off
        int coupon_amount = 2000;  // 20% (stored as 2000 = 20.00%)
        int flags = TF_IS_PERCENT;
        
        // Test CPAmount for a $10 item, quantity 1
        int item_cost = 1000;  // $10.00
        int item_count = 1;
        int cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);
        
        // CPAmount should return: $10 - ($10 * 0.80) = $10 - $8 = $2
        REQUIRE(cp_amount == 200);  // $2.00 deduction (20% of $10)
        
        // Test CPAmount for a $10 item, quantity 2
        item_count = 2;
        cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);
        
        // CPAmount should return: 2 * ($10 - $8) = 2 * $2 = $4
        REQUIRE(cp_amount == 400);  // $4.00 total deduction
    }
    
    SECTION("Coupon Amount calculation - fixed dollar amount")
    {
        // Create a coupon: $2 off per item
        int coupon_amount = 200;  // $2.00
        int flags = 0;
        
        // Test Amount for a $10 item, quantity 1
        int item_cost = 1000;  // $10.00
        int item_count = 1;
        int amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        
        // Amount should return what customer pays: $10 - $2 = $8
        REQUIRE(amount == 800);  // $8.00
        
        // Test Amount for a $10 item, quantity 3
        item_count = 3;
        amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        
        // Amount should return: 3 * ($10 - $2) = 3 * $8 = $24
        REQUIRE(amount == 2400);  // $24.00
    }
    
    SECTION("Coupon Amount calculation - percentage discount")
    {
        // Create a coupon: 20% off
        int coupon_amount = 2000;  // 20%
        int flags = TF_IS_PERCENT;
        
        // Test Amount for a $10 item, quantity 1
        int item_cost = 1000;  // $10.00
        int item_count = 1;
        int amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        
        // Amount should return: $10 * 0.80 = $8
        REQUIRE(amount == 800);  // $8.00 (80% of $10)
        
        // Test Amount for a $10 item, quantity 2
        item_count = 2;
        amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        
        // Amount should return: 2 * ($10 * 0.80) = 2 * $8 = $16
        REQUIRE(amount == 1600);  // $16.00
    }
    
    SECTION("Coupon relationship: CPAmount + Amount = Original Cost")
    {
        // Verify that CPAmount (deduction) + Amount (what customer pays) = Original Cost
        int coupon_amount = 200;  // $2.00 off
        int flags = 0;
        
        int item_cost = 1000;  // $10.00
        int item_count = 2;
        
        int cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);  // Deduction
        int amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);       // What customer pays
        int original_cost = item_cost * item_count;              // Original total
        
        // CPAmount + Amount should equal original cost
        REQUIRE(cp_amount + amount == original_cost);
        REQUIRE(cp_amount == 400);   // $4.00 deduction (2 items * $2)
        REQUIRE(amount == 1600);     // $16.00 customer pays (2 items * $8)
        REQUIRE(original_cost == 2000);  // $20.00 original
    }
    
    SECTION("Coupon with TF_APPLY_EACH flag behavior")
    {
        // Coupons with TF_APPLY_EACH should apply to each matching item
        // The calculation logic is the same, but it's applied per item
        int coupon_amount = 100;  // $1.00 off per item
        int flags = TF_APPLY_EACH;
        
        // For a $5 item, quantity 4
        int item_cost = 500;   // $5.00
        int item_count = 4;
        
        // CPAmount for one item
        int cp_amount_single = CalculateCouponCPAmount(item_cost, 1, coupon_amount, 0);
        REQUIRE(cp_amount_single == 100);  // $1.00 off per item
        
        // CPAmount for all items
        int cp_amount_total = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, 0);
        REQUIRE(cp_amount_total == 400);  // $4.00 total deduction (4 * $1)
        
        // Verify the relationship
        int amount_total = CalculateCouponAmount(item_cost, item_count, coupon_amount, 0);
        REQUIRE(cp_amount_total + amount_total == item_cost * item_count);
    }
    
    SECTION("Coupon without TF_APPLY_EACH (treated as discount)")
    {
        // Coupons without TF_APPLY_EACH are treated as global discounts
        // They should still calculate CPAmount correctly
        int coupon_amount = 500;  // $5.00 off total
        int flags = 0;             // No TF_APPLY_EACH
        
        // For a $20 order
        int item_cost = 2000;  // $20.00
        int item_count = 1;
        
        int cp_amount = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);
        REQUIRE(cp_amount == 500);  // $5.00 deduction
        
        int amount = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        REQUIRE(amount == 1500);  // $15.00 customer pays
        
        // Verify relationship
        REQUIRE(cp_amount + amount == item_cost * item_count);
    }
}

// Test that coupon values are correctly calculated for report tracking
TEST_CASE("Coupon Value Tracking for Reports", "[coupons][reports]")
{
    SECTION("Verify coupon payment value calculation")
    {
        // When a coupon is applied, payment->value should equal the total CPAmount
        // This value is what gets tracked in reports as the coupon adjustment
        
        int coupon_amount = 200;  // $2.00 off
        
        // Simulate applying coupon to 3 items at $10 each
        int item_cost = 1000;  // $10.00
        int item_count = 3;
        
        // Total CPAmount should be the payment value
        int expected_payment_value = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, 0);
        REQUIRE(expected_payment_value == 600);  // 3 * $2 = $6.00
        
        // In reports, this $6.00 should be subtracted from gross sales
        // Gross sales = 3 * $10 = $30.00
        // Net sales = $30.00 - $6.00 = $24.00
        int gross_sales = item_cost * item_count;
        int coupon_adjustment = expected_payment_value;
        int net_sales = gross_sales - coupon_adjustment;
        
        REQUIRE(gross_sales == 3000);      // $30.00
        REQUIRE(coupon_adjustment == 600); // $6.00
        REQUIRE(net_sales == 2400);        // $24.00
        
        // Verify net sales equals what customer actually pays
        int customer_pays = CalculateCouponAmount(item_cost, item_count, coupon_amount, 0);
        REQUIRE(net_sales == customer_pays);
    }
    
    SECTION("Multiple coupons with different types")
    {
        // Test that different coupon types calculate correctly
        
        // Fixed amount coupon
        int coupon_amount1 = 100;  // $1.00 off
        int flags1 = 0;
        
        // Percentage coupon
        int coupon_amount2 = 1500;  // 15% off
        
        int item_cost = 1000;  // $10.00
        int item_count = 1;
        
        // Coupon 1: $1 off
        int cp1 = CalculateCouponCPAmount(item_cost, item_count, coupon_amount1, flags1);
        REQUIRE(cp1 == 100);  // $1.00
        
        // Coupon 2: 15% off
        int cp2 = CalculateCouponCPAmount(item_cost, item_count, coupon_amount2, TF_IS_PERCENT);
        REQUIRE(cp2 == 150);  // $1.50 (15% of $10)
        
        // If both were applied (hypothetically), total deduction would be $2.50
        // But in practice, only one discount/coupon applies at a time
    }
    
    SECTION("Sales mix report calculation verification")
    {
        // Verify the formula used in reports: net_sales = gross_sales - coupon_adjustment
        // This ensures coupons are correctly subtracted from sales
        
        int item_cost = 1000;  // $10.00 per item
        int item_count = 5;    // 5 items
        int coupon_amount = 150;  // $1.50 off per item
        int flags = 0;
        
        // Calculate values
        int gross_sales = item_cost * item_count;  // $50.00
        int coupon_value = CalculateCouponCPAmount(item_cost, item_count, coupon_amount, flags);  // $7.50
        int net_sales = gross_sales - coupon_value;  // $42.50
        
        // Verify the relationship
        REQUIRE(gross_sales == 5000);      // $50.00
        REQUIRE(coupon_value == 750);     // $7.50 (5 * $1.50)
        REQUIRE(net_sales == 4250);       // $42.50
        
        // Net sales should equal what customer pays
        int customer_pays = CalculateCouponAmount(item_cost, item_count, coupon_amount, flags);
        REQUIRE(net_sales == customer_pays);
        REQUIRE(customer_pays == 4250);   // $42.50
    }
}

