/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * utility.hh - revision 106 (10/20/98)
 * General functions and data types than have no other place to go
 */

#ifndef VT_TIME_INFO_HH
#define VT_TIME_INFO_HH

#include "basic.hh"

#include "date/tz.h"

#include <time.h>
#include <string>

// Time & Date (second accuracy) storage class
class TimeInfo
{
private:
    bool is_valid_ = false;
    date::local_time<std::chrono::seconds> t_;
public:
    // Constructor
    TimeInfo();
    TimeInfo(const TimeInfo &other);

    const date::local_time<std::chrono::seconds> &get_local_time() const;
    // Member Functions
    int   Set();                          // Sets time values to current time
    int   Set(int s, int y);
    int   Set(const std::string &date_string);
    int   Set(const TimeInfo *other);           // Copies time value
    int   Set(const TimeInfo &other);           // Copies time value
    int   Clear();                        // Erases time value
    bool  IsSet() const;                  // boolean - has time been set?
    int   AdjustSeconds(int amount);      // Adds 'amount' to current second
    int   AdjustMinutes(int amount);      // Adds 'amount' to current minute
    int   AdjustDays(int amount);         // Adds 'amount' to current day
    int   AdjustWeeks(int amount);        // Adds 'amount' to current week
    int   AdjustMonths(int amount);       // Adds 'amount' to current month
    int   AdjustYears(int amount);        // Adds 'amount' to current year
    // if JUMP_DOWN if day is d_low or d_high go to next lower day, otherwise round down
    // if ROUND_DOWN if day is d_low or d_high stay at day, otherwise round down
    // if ROUND_UP if day is d_low or d_high stay at day, otherwise round up
    // if JUMP_UP if day is d_low or d_high go to next higher day, otherwise round down
    // if sign < 0 jump backward a half mont
    // d_low and d_high are the days in the month to jump to
    void half_month_jump(const int sign, const int d_low, const int d_high);
    std::string DebugPrint() const;  // Just prints the time value for debugging (minutes accuracy)
    std::string to_string() const;
    std::string Date() const;
    std::string Time()const;

    TimeInfo& operator=  (const TimeInfo &other);

    // comparisions
    bool operator >  (const TimeInfo &other) const;
    bool operator >= (const TimeInfo &other) const;
    bool operator <  (const TimeInfo &other) const;
    bool operator <= (const TimeInfo &other) const;
    bool operator == (const TimeInfo &other) const;
    bool operator != (const TimeInfo &other) const;

    // arithmetic
    std::chrono::seconds operator -(const TimeInfo &other) const;

    void throw_if_uninitialized(const std::string &op_name) const;

    TimeInfo operator+(const std::chrono::seconds& ds) const;
    TimeInfo operator+(const std::chrono::minutes& dm) const;
    TimeInfo operator+(const std::chrono::hours& dh) const;
    TimeInfo operator+(const date::days& dm) const;
    TimeInfo operator+(const date::months& dm) const;
    TimeInfo operator+(const date::years& dm) const;
    void operator+=(const std::chrono::seconds& ds);
    void operator+=(const std::chrono::minutes& dm);
    void operator+=(const std::chrono::hours& dh);
    void operator+=(const date::days& dm);
    void operator+=(const date::months& dm);
    void operator+=(const date::years& dm);

    template<typename Duration>
    TimeInfo operator-(const Duration &dur) const
    {
        return this->operator+(-dur);
    }
    template<typename Duration>
    void operator-=(const Duration &dur)
    {
        this->operator+=(-dur);
    }


    // helper to get the specified property
    template<typename Duration_return, typename Duration_floor>
    int get() const
    {
        auto t_floor = date::floor<Duration_floor>(t_);
        auto dur = std::chrono::duration_cast<Duration_return>(t_ - t_floor);
        auto dur_cnt = dur.count();
        // assert if we are bigger than int
        assert(dur_cnt <= std::numeric_limits<int>::max());
        return static_cast<int>(dur_cnt);
    }
    // getters using get()
    int Sec() const;
    int Min() const;
    int Hour() const;
    int Day() const;
    int Month() const;
    int Year() const;
    // the weekday of the TimeInfo
    int WeekDay() const;
    // get the seconds since midnight of the current year
    int SecondsInYear() const;
    // get the days in the current month
    int DaysInMonth() const;

    // round down to the precision defined by Duration
    // for example floor<std::chrono::seconds>()
    template<typename Duration>
    void Floor()
    {
        using namespace::date;
        t_ = date::floor<Duration>(t_) + std::chrono::seconds(0);
    }
};

/**** Global Variables ****/
extern TimeInfo SystemTime;
// Current system time


/**** Other Functions ****/
int DaysInYear(int year);
int DaysInMonth(int month, int year);
// Returns number of days in given year or month

int DayOfTheWeek(int mday, int month, int year);
// Returns what day of the week it is

int StringElapsedToNow(char* dest, int maxlen, TimeInfo &t1);
int SecondsToString(char* dest, int maxlen, int seconds);

int SecondsElapsedToNow(const TimeInfo &t1);
// SecondsElapsed always returns the absolute timedifference
int SecondsElapsed(const TimeInfo &t1, const TimeInfo &t2);
// Returns number of seconds between two times

int MinutesElapsedToNow(const TimeInfo &t1);
int MinutesElapsed(const TimeInfo &t1, const TimeInfo &t2);
// Returns minutes elapsed between two times (ingnoring second values)

#endif // VT_TIME_INFO_HH
