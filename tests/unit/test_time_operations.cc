/*
 * Unit tests for TimeInfo class - date and time operations
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "time_info.hh"

TEST_CASE("TimeInfo Construction and Initialization", "[time][construction]")
{
    SECTION("Default construction creates unset time")
    {
        TimeInfo t;
        REQUIRE_FALSE(t.IsSet());
    }

    SECTION("Copy constructor")
    {
        TimeInfo t1;
        t1.Set();
        TimeInfo t2(t1);
        REQUIRE(t2.IsSet());
        REQUIRE(t1 == t2);
    }

    SECTION("Set to current time")
    {
        TimeInfo t;
        int result = t.Set();
        REQUIRE(result == 0);
        REQUIRE(t.IsSet());
    }

    SECTION("Copy from another TimeInfo")
    {
        TimeInfo t1, t2;
        t1.Set();
        t2.Set(&t1);
        REQUIRE(t1 == t2);
        
        TimeInfo t3;
        t3.Set(t1);
        REQUIRE(t1 == t3);
    }
}

TEST_CASE("TimeInfo Comparison Operators", "[time][comparison]")
{
    TimeInfo t1, t2;
    t1.Set();
    
    SECTION("Equal times")
    {
        t2.Set(t1);
        REQUIRE(t1 == t2);
        REQUIRE_FALSE(t1 != t2);
        REQUIRE(t1 <= t2);
        REQUIRE(t1 >= t2);
    }

    SECTION("Later time comparisons")
    {
        t2.Set(t1);
        t2.AdjustSeconds(10);
        REQUIRE(t2 > t1);
        REQUIRE(t2 >= t1);
        REQUIRE(t1 < t2);
        REQUIRE(t1 <= t2);
        REQUIRE(t1 != t2);
        REQUIRE_FALSE(t1 == t2);
    }
}

TEST_CASE("TimeInfo Arithmetic - Seconds", "[time][arithmetic][seconds]")
{
    TimeInfo t;
    t.Set();
    TimeInfo original(t);

    SECTION("Add seconds")
    {
        t.AdjustSeconds(30);
        auto diff = t - original;
        REQUIRE(diff.count() == 30);
    }

    SECTION("Subtract seconds")
    {
        t.AdjustSeconds(-45);
        auto diff = original - t;
        REQUIRE(diff.count() == 45);
    }

    SECTION("Add large number of seconds (crossing minute boundary)")
    {
        t.AdjustSeconds(120);
        auto diff = t - original;
        REQUIRE(diff.count() == 120);
    }
}

TEST_CASE("TimeInfo Arithmetic - Minutes", "[time][arithmetic][minutes]")
{
    TimeInfo t;
    t.Set();
    TimeInfo original(t);

    SECTION("Add minutes")
    {
        t.AdjustMinutes(15);
        auto diff = t - original;
        REQUIRE(diff.count() == 15 * 60);
    }

    SECTION("Subtract minutes")
    {
        t.AdjustMinutes(-30);
        auto diff = original - t;
        REQUIRE(diff.count() == 30 * 60);
    }

    SECTION("Add hours worth of minutes")
    {
        t.AdjustMinutes(120);
        auto diff = t - original;
        REQUIRE(diff.count() == 120 * 60);
    }
}

TEST_CASE("TimeInfo Arithmetic - Days", "[time][arithmetic][days]")
{
    TimeInfo t;
    t.Set();
    TimeInfo original(t);

    SECTION("Add one day")
    {
        t.AdjustDays(1);
        auto diff = t - original;
        REQUIRE(diff.count() == 86400); // 24 * 60 * 60
    }

    SECTION("Subtract one day")
    {
        t.AdjustDays(-1);
        auto diff = original - t;
        REQUIRE(diff.count() == 86400);
    }

    SECTION("Add multiple days")
    {
        t.AdjustDays(7);
        auto diff = t - original;
        REQUIRE(diff.count() == 7 * 86400);
    }
}

TEST_CASE("TimeInfo Arithmetic - Weeks", "[time][arithmetic][weeks]")
{
    TimeInfo t;
    t.Set();
    TimeInfo original(t);

    SECTION("Add one week")
    {
        t.AdjustWeeks(1);
        auto diff = t - original;
        REQUIRE(diff.count() == 7 * 86400);
    }

    SECTION("Add multiple weeks")
    {
        t.AdjustWeeks(4);
        auto diff = t - original;
        REQUIRE(diff.count() == 28 * 86400);
    }
}

TEST_CASE("TimeInfo Arithmetic - Months", "[time][arithmetic][months]")
{
    TimeInfo t;
    t.Set();

    SECTION("Add one month")
    {
        TimeInfo before(t);
        t.AdjustMonths(1);
        REQUIRE(t > before);
        // Month length varies, just check it moved forward
    }

    SECTION("Subtract one month")
    {
        TimeInfo before(t);
        t.AdjustMonths(-1);
        REQUIRE(t < before);
    }

    SECTION("Add twelve months (one year)")
    {
        TimeInfo before(t);
        t.AdjustMonths(12);
        REQUIRE(t > before);
    }
}

TEST_CASE("TimeInfo Arithmetic - Years", "[time][arithmetic][years]")
{
    TimeInfo t;
    t.Set();

    SECTION("Add one year")
    {
        TimeInfo before(t);
        t.AdjustYears(1);
        REQUIRE(t > before);
        // Approximate check (leap years may affect)
        auto diff = t - before;
        REQUIRE(diff.count() >= 365 * 86400);
        REQUIRE(diff.count() <= 366 * 86400);
    }

    SECTION("Subtract one year")
    {
        TimeInfo before(t);
        t.AdjustYears(-1);
        REQUIRE(t < before);
    }

    SECTION("Add multiple years")
    {
        TimeInfo before(t);
        t.AdjustYears(5);
        REQUIRE(t > before);
    }
}

TEST_CASE("TimeInfo Operator Arithmetic", "[time][operators]")
{
    TimeInfo t;
    t.Set();
    TimeInfo original(t);

    SECTION("Add duration with operator+")
    {
        auto t2 = t + std::chrono::seconds(60);
        auto diff = t2 - t;
        REQUIRE(diff.count() == 60);
    }

    SECTION("Add minutes with operator+")
    {
        auto t2 = t + std::chrono::minutes(5);
        auto diff = t2 - t;
        REQUIRE(diff.count() == 300);
    }

    SECTION("Add hours with operator+")
    {
        auto t2 = t + std::chrono::hours(2);
        auto diff = t2 - t;
        REQUIRE(diff.count() == 7200);
    }

    SECTION("Compound assignment with seconds")
    {
        t += std::chrono::seconds(120);
        auto diff = t - original;
        REQUIRE(diff.count() == 120);
    }

    SECTION("Compound assignment with minutes")
    {
        t += std::chrono::minutes(10);
        auto diff = t - original;
        REQUIRE(diff.count() == 600);
    }
}

TEST_CASE("TimeInfo String Operations", "[time][string]")
{
    TimeInfo t;
    t.Set();

    SECTION("to_string returns non-empty string")
    {
        auto str = t.to_string();
        REQUIRE_FALSE(str.empty());
    }

    SECTION("Date returns non-empty string")
    {
        auto date_str = t.Date();
        REQUIRE_FALSE(date_str.empty());
    }

    SECTION("Time returns non-empty string")
    {
        auto time_str = t.Time();
        REQUIRE_FALSE(time_str.empty());
    }

    SECTION("DebugPrint returns non-empty string")
    {
        auto debug_str = t.DebugPrint();
        REQUIRE_FALSE(debug_str.empty());
    }
}

TEST_CASE("TimeInfo Clear and Reset", "[time][clear]")
{
    TimeInfo t;
    
    SECTION("Clear makes time unset")
    {
        t.Set();
        REQUIRE(t.IsSet());
        t.Clear();
        REQUIRE_FALSE(t.IsSet());
    }

    SECTION("Can set after clearing")
    {
        t.Set();
        t.Clear();
        t.Set();
        REQUIRE(t.IsSet());
    }
}

TEST_CASE("TimeInfo Copy and Assignment", "[time][assignment]")
{
    TimeInfo t1, t2;
    t1.Set();

    SECTION("Assignment operator")
    {
        t2 = t1;
        REQUIRE(t2.IsSet());
        REQUIRE(t1 == t2);
    }

    SECTION("Self-assignment is safe")
    {
        t1 = t1;
        REQUIRE(t1.IsSet());
    }

    SECTION("Chain assignments")
    {
        TimeInfo t3;
        t3 = t2 = t1;
        REQUIRE(t3 == t1);
        REQUIRE(t2 == t1);
    }
}

TEST_CASE("TimeInfo Business Logic - Scheduling", "[time][business]")
{
    TimeInfo shift_start, shift_end;
    shift_start.Set();
    shift_end.Set(shift_start);
    shift_end.AdjustMinutes(8 * 60); // 8 hours

    SECTION("8-hour shift duration")
    {
        auto duration = shift_end - shift_start;
        REQUIRE(duration.count() == 8 * 3600);
    }

    SECTION("Check if time is within shift")
    {
        TimeInfo during_shift(shift_start);
        during_shift.AdjustMinutes(4 * 60); // 4 hours
        
        REQUIRE(during_shift >= shift_start);
        REQUIRE(during_shift <= shift_end);
    }

    SECTION("Check if time is outside shift")
    {
        TimeInfo after_shift(shift_end);
        after_shift.AdjustMinutes(1);
        
        REQUIRE(after_shift > shift_end);
    }
}

TEST_CASE("TimeInfo Edge Cases", "[time][edge]")
{
    TimeInfo t;

    SECTION("Multiple adjustments in sequence")
    {
        t.Set();
        TimeInfo original(t);
        
        t.AdjustDays(1);
        t.AdjustMinutes(2 * 60); // 2 hours
        t.AdjustMinutes(30);
        t.AdjustSeconds(45);
        
        auto diff = t - original;
        int expected = 86400 + 7200 + 1800 + 45;
        REQUIRE(diff.count() == expected);
    }

    SECTION("Forward and backward adjustments")
    {
        t.Set();
        TimeInfo original(t);
        
        t.AdjustDays(5);
        t.AdjustDays(-3);
        
        auto diff = t - original;
        REQUIRE(diff.count() == 2 * 86400);
    }

    SECTION("Zero adjustments")
    {
        t.Set();
        TimeInfo original(t);
        
        t.AdjustSeconds(0);
        t.AdjustMinutes(0);
        t.AdjustDays(0);
        
        REQUIRE(t == original);
    }
}

TEST_CASE("TimeInfo Ordering", "[time][ordering]")
{
    TimeInfo t1, t2, t3;
    t1.Set();
    t2.Set(t1);
    t2.AdjustMinutes(30);
    t3.Set(t1);
    t3.AdjustMinutes(60); // 1 hour

    SECTION("Proper ordering")
    {
        REQUIRE(t1 < t2);
        REQUIRE(t2 < t3);
        REQUIRE(t1 < t3);
    }

    SECTION("Transitive comparisons")
    {
        REQUIRE((t1 < t2 && t2 < t3) == (t1 < t3));
    }
}
