#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include "time_info.hh"

// external system libraries
#include "date/tz.h"

// standard libraries
#include <chrono>

TEST_CASE("addition_seconds_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_seconds_wrapping_minute", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(60*3 + 15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  3);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_seconds_wrapping_hour", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(60*60*3 + 15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  3);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_seconds_wrapping_day", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(60*60*24*3 + 15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  4);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_seconds_wrapping_month", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(60*60*24*31 + 15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  2);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_seconds_wrapping_year", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::seconds(60*60*24*365 + 15);
    CHECK(ti.Year() ==  2019);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  15);
}
TEST_CASE("addition_minutes_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::minutes(3);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  3);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_minutes_wrapping_hour", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::minutes(60*3);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  3);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_minutes_wrapping_day", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::minutes(60*24*3);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  4);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_hours_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::hours(15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  15);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_hours_wrapping_day", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += std::chrono::hours(24 + 15);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  2);
    CHECK(ti.Hour() ==  15);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_days_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += date::days(3);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  4);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_days_wrapping_month", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += date::days(31);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  2);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_month_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += date::months(2);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  3);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_month_wrapping_year", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += date::months(12);
    CHECK(ti.Year() ==  2019);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}
TEST_CASE("addition_year_basic", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // 2018-01-01 00:00::00
    ti += date::years(2);
    CHECK(ti.Year() ==  2020);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}

TEST_CASE("set_year_results_in_jan_first_midnight", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018);
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Sec() ==  0);
}

TEST_CASE("set_yesterday_night_00_00", "[time_info]")
{
    TimeInfo ti;
    CHECK_FALSE(ti.IsSet());
    // set date to yesterday morning, 00:00
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==  4);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  30);
    REQUIRE(ti.Sec() == 24);
    ti -= date::days(1);
    REQUIRE(ti.Day() == 3); // 2018-01-03 00:30:24
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Min() ==  30);
    REQUIRE(ti.Sec() == 24);
    ti.Floor<date::days>(); // 2018-01-03 00:00:00
    REQUIRE(ti.Day() == 3);
    REQUIRE(ti.Hour() == 0);
    REQUIRE(ti.Min() ==  0);
    REQUIRE(ti.Sec() ==  0);
}
TEST_CASE("set_yesterday_night_23_59", "[time_info]")
{
    TimeInfo ti;
    CHECK_FALSE(ti.IsSet());
    // set date to last night, 23:59
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    ti.Floor<date::days>(); // 2018-01-04 00:00:00
    ti -= std::chrono::seconds(1); // 2018-01-03 23:59:59
    CHECK(ti.Day() ==    3);
    CHECK(ti.Hour() ==  23);
    CHECK(ti.Min() ==   59);
    CHECK(ti.Sec() ==   59);
}

TEST_CASE("adjust_month_doesnt_change_day_and_time", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    ti += date::months(1);
    CHECK(ti.Month() ==  2);
    CHECK(ti.Day() ==    4);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}
TEST_CASE("adjust_year_doesnt_change_day_and_time", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    ti += date::years(1);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==    4);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}
TEST_CASE("same_time_after_substracting_and_adding_durations", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    ti -= std::chrono::seconds(1);
    ti -= std::chrono::minutes(1);
    ti -= std::chrono::hours(1);
    ti -= date::days(1);
    ti -= date::months(1);
    ti -= date::years(1);
    ti += date::years(1);
    ti += date::months(1);
    ti += date::days(1);
    ti += std::chrono::hours(1);
    ti += std::chrono::minutes(1);
    ti += std::chrono::seconds(1);

    // we should be when we started
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==    4);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}


TEST_CASE("half_month_jump_1_15_doesnt_change_time", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(1, d_low, d_high);

    // same time after half-month-jump
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_forward_snaps_backwards", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(1, d_low, d_high);

    // we should be when we started
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==    1);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_forward_on_first_jumps_to_15th", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*30 + 24, 2018); // 2018-01-01 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(1, d_low, d_high);

    // we should be when we started
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==   15);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_forward_on_15th_jumps_to_next_month_first", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*30 + 24, 2018); // 2018-01-01 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(1, d_low, d_high); // 2018-01-15 00:30:24
    ti.half_month_jump(1, d_low, d_high); // 2018-02-01 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  2);
    CHECK(ti.Day() ==    1);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_forward_snaps_backwards", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(1, d_low, d_high);

    CHECK(ti.Year() ==  2017);
    CHECK(ti.Month() ==  12);
    CHECK(ti.Day() ==    26);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_forward_on_11th_jumps_to_26th", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*10 + 60*30 + 24, 2018); // 2018-01-11 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(1, d_low, d_high); // 2018-01-26 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==   26);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_forward_on_26th_jumps_to_11th_of_next_month", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*25 + 60*30 + 24, 2018); // 2018-01-26 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(1, d_low, d_high); // 2018-02-11 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  2);
    CHECK(ti.Day() ==   11);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_backward_snaps_forwards", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(-1, d_low, d_high); // 2018-01-15 00:30:24

    // we should be when we started
    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==   15);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_backward_on_first_jumps_to_15th_of_last_month", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*30 + 24, 2018); // 2018-01-01 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(-1, d_low, d_high); // 2017-12-15 00:30:24

    // we should be when we started
    CHECK(ti.Year() ==  2017);
    CHECK(ti.Month() == 12);
    CHECK(ti.Day() ==   15);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_1_15_backward_on_15th_jumps_to_first", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*14 + 60*30 + 24, 2018); // 2018-01-15 00:30:24
    constexpr int d_low = 1;
    constexpr int d_high = 15;
    ti.half_month_jump(-1, d_low, d_high); // 2018-01-01 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==    1);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_backward_snaps_forward", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3 + 60*30 + 24, 2018); // 2018-01-04 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(-1, d_low, d_high); // 2018-01-11 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==   11);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_backward_on_11th_jumps_to_26th_of_previous_month", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*10 + 60*30 + 24, 2018); // 2018-01-11 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(-1, d_low, d_high); // 2017-12-26 00:30:24

    CHECK(ti.Year() ==  2017);
    CHECK(ti.Month() == 12);
    CHECK(ti.Day() ==   26);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}

TEST_CASE("half_month_jump_11_26_backward_on_26th_jumps_to_11th", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*25 + 60*30 + 24, 2018); // 2018-01-26 00:30:24
    constexpr int d_low = 11;
    constexpr int d_high = 26;
    ti.half_month_jump(-1, d_low, d_high); // 2018-01-11 00:30:24

    CHECK(ti.Year() ==  2018);
    CHECK(ti.Month() ==  1);
    CHECK(ti.Day() ==   11);
    CHECK(ti.Hour() ==   0);
    CHECK(ti.Min() ==   30);
    CHECK(ti.Sec() ==   24);
}


TEST_CASE("floor_days_results_in_zero_hours_mins_secs", "[time_info]")
{
    TimeInfo ti;
    CHECK_FALSE(ti.IsSet());
    // set date to last night, 23:59
    ti.Set();
    ti.Floor<date::days>();
    // expect zero time
}
// Floor

TEST_CASE("floor_years_sets_all_lower_values_to_start", "[time_info]")
{
    TimeInfo ti;
    CHECK_FALSE(ti.IsSet());
    ti.Floor<date::years>();
    CHECK(ti.Sec() ==  0);
    CHECK(ti.Min() ==  0);
    CHECK(ti.Hour() ==  0);
    CHECK(ti.Day() ==  1);
    CHECK(ti.Month() ==  1);
}

TEST_CASE("check_weekday_has_the_ctime_indizes_Monday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(0, 2018); // Mon 2018-01-01 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  1);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Tuesday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*1, 2018); // Tue 2018-01-02 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  2);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Wednesday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*2, 2018); // Tue 2018-01-03 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  3);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Thursday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*3, 2018); // Thu 2018-01-04 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  4);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Friday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*4, 2018); // Fri 2018-01-05 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  5);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Saturday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*5, 2018); // Sat 2018-01-06 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  6);
}
TEST_CASE("check_weekday_has_the_ctime_indizes_Sunday", "[time_info]")
{
    TimeInfo ti;
    ti.Set(60*60*24*6, 2018); // Sun 2018-01-07 00:00:00
    // ctime defines tm_wday as "days since Sunday in the range of 0 to 6
    CHECK(ti.WeekDay() ==  0);
}

TEST_CASE("SecondsElapsed always returns the absolute difference", "[time_info][SecondsElapsed]")
{
    TimeInfo ti1;
    TimeInfo ti2;
    ti1.Set( 0, 2018); // 2018-01-01 00:00::00
    ti2.Set(60, 2018); // 2018-01-01 00:01::00
    CHECK(SecondsElapsed(ti1, ti2) ==  60);
    CHECK(SecondsElapsed(ti2, ti1) ==  60);
}

TEST_CASE("seconds in year", "[time_info][SecondsInYear]")
{
    TimeInfo ti;
    ti.Set(600, 2018);
    CHECK(ti.SecondsInYear() == 600);
    CHECK(ti.SecondsInYear() == 600);
    CHECK(ti.Year() == 2018);
}
