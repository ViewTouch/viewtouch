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
 * utility.cc - revision 124 (10/6/98)
 * Implementation of utility module
 */

#include "time_info.hh"
#include "fntrace.hh"

#include <unistd.h>
#include <ctime>
#include <errno.h>
#include <string>
#include <cstring>
#include <cctype>
#include <iostream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Global Variables ****/
TimeInfo SystemTime;

/**** TimeInfo Class ****/
// Constructor
TimeInfo::TimeInfo()
{
    Clear();
}

TimeInfo::TimeInfo(const TimeInfo &other)
{
    this->t_ = other.t_;
    this->is_valid_ = other.is_valid_;
}

const date::local_time<std::chrono::seconds> &TimeInfo::get_local_time() const
{
    return t_;
}

// Member Functions
int TimeInfo::Set()
{
    FnTrace("TimeInfo::Set()");

    using namespace date;
    auto zt = make_zoned(
                current_zone(),
                floor<std::chrono::seconds>(std::chrono::system_clock::now()));

    t_ = zt.get_local_time();
    is_valid_ = true;

    return 0;
}

int TimeInfo::Set(int s, int y)
{
    FnTrace("TimeInfo::Set(int, int)");

    using namespace date;
    t_ = local_days{date::year(y) / January / 1} + std::chrono::seconds{s};
    is_valid_ = true;

    return 0;
}

/****
 * Set:  Parses date strings.  Acceptible formats:
 *   "DD/MM/YY,HH:MM"   in 24hour format
 *   "DD/MM/YYYY,HH:MM" in 24hour format
 *  Returns 1 on error, 0 on success.
 ****/
int TimeInfo::Set(const std::string &date_string)
{
    FnTrace("TimeInfo::Set(const std::string& )");
    using namespace date;

    std::istringstream ss(date_string);
    sys_time<std::chrono::milliseconds> t;
    ss >> date::parse("%d/%m/%y,%h:%m", t);
    if (ss.fail())
    {
        ss >> date::parse("%d/%m/%Y,%h:%m", t);
        if (ss.fail())
        {
            Clear();
            return false;
        }
    }
    is_valid_ = true;
    return true;
}

int TimeInfo::Set(const TimeInfo *other)
{
    FnTrace("TimeInfo::Set(TimeInfo *)");
    if (other)
    {
        this->Set(*other);
    } else
    {
        Clear();
    }
    return 0;
}

int TimeInfo::Set(const TimeInfo &other)
{
    FnTrace("TimeInfo::Set(TimeInfo &)");
    this->is_valid_ = other.is_valid_;
    this->t_ = other.t_;
    return 0;
}

int TimeInfo::Clear()
{
    FnTrace("TimeInfo::Clear()");
    this->is_valid_ = false;
    return 0;
}

bool TimeInfo::IsSet() const
{
    return is_valid_;
}

int TimeInfo::AdjustSeconds(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustSeconds(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += std::chrono::seconds(amount);
    return 0;
}

int TimeInfo::AdjustMinutes(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustMinutes(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += std::chrono::minutes(amount);
    return 0;
}

int TimeInfo::AdjustDays(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustDays(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += date::days(amount);
    return 0;
}

int TimeInfo::AdjustWeeks(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustWeeks(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += date::days(amount*7);
    return 0;
}

int TimeInfo::AdjustMonths(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustMonths(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += date::months(amount);
    return 0;
}

int TimeInfo::AdjustYears(int amount)
{
    const std::string msg = std::string("TimeInfo::AdjustYears(")
            + std::to_string(amount) + ")";
    FnTrace(msg.c_str());
    *this += date::years(amount);
    return 0;
}

void TimeInfo::half_month_jump(const int sign, const int d_low, const int d_high)
{
    assert(sign == 1 || sign == -1);
    assert(d_low < d_high);
    const int cur_day = this->Day();
    if (sign == 1)
    {
        if (cur_day < d_low)
        {
            // x  < 11 --> 26 prev month
            *this -= date::months(1);
            *this += date::days(d_high - cur_day);
        } else if(cur_day == d_low)
        {
            // x == 11 --> 26 this month
            *this += date::days(d_high - cur_day);
        } else if(cur_day < d_high)
        {
            // x  < 26 --> 11 this month
            *this += date::days(d_low - cur_day);
        } else if(cur_day == d_high)
        {
            // x == 26 --> 11 next month
            *this += date::months(1);
            *this += date::days(d_low - cur_day);
        } else if(cur_day > d_high)
        {
            // x  > 26 --> 11 next month
            *this += date::days(d_high - cur_day);
        } else
        {
            // this should not happen
            assert(false);
        }
    } else
    {
        if (cur_day > d_high)
        {
            // x  > 26 --> 11 next month
            *this += date::months(1);
            *this += date::days(d_low - cur_day);
        } else if (cur_day == d_high)
        {
            // x == 26 --> 11 this month
            *this += date::days(d_low - cur_day);
        } else if (cur_day > d_low)
        {
            // x  > 11 --> 26 this month
            *this += date::days(d_high - cur_day);
        } else if (cur_day == d_low)
        {
            // x == 11 --> 26 prev month
            *this -= date::months(1);
            *this += date::days(d_high - cur_day);
        } else if (cur_day < d_low)
        {
            // x  < 11 --> 11 this month
            *this += date::days(d_low - cur_day);
        } else
        {
            // this should not happen
            assert(false);
        }
    }
}

std::string TimeInfo::DebugPrint() const
{
    FnTrace("TimeInfo::DebugPrint()");
    using namespace date;
    std::ostringstream ss;
    date::zoned_seconds zt = date::make_zoned(
                current_zone(),
                date::floor<std::chrono::minutes>(t_));
    ss << zt;
    return ss.str();
}

/****
 * ToString:  converts the TimeInfo to a string that can later be
 *  parsed by Set(const char* ): "DD/MM/YY,HH:MM" in 24hour format
 ****/
std::string TimeInfo::to_string() const
{
    FnTrace("TimeInfo::ToString()");
    using namespace date;
    std::ostringstream ss;
    date::zoned_seconds zt = date::make_zoned(
                current_zone(),
                t_);
    ss << zt;
    return ss.str();
}

void TimeInfo::throw_if_uninitialized(const std::string &op_name) const
{
    if (!is_valid_)
    {
        throw std::runtime_error(op_name + ": object is not initialized");
    }
}

TimeInfo &TimeInfo::operator= (const TimeInfo &other)
{
    FnTrace("TimeInfo::operator=(TimeInfo)");
    this->t_ = other.t_;
    this->is_valid_ = other.is_valid_;
    return *this;
}

TimeInfo TimeInfo::operator+(const std::chrono::seconds &ds) const
{
    const std::string op_name = "TimeInfo::operator+(seconds)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);
    ti.t_ += ds;
    return ti;
}
TimeInfo TimeInfo::operator+(const std::chrono::minutes &dm) const
{
    const std::string op_name = "TimeInfo::operator+(minutes)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);
    ti.t_ += dm;
    return ti;
}
TimeInfo TimeInfo::operator+(const std::chrono::hours &dh) const
{
    const std::string op_name = "TimeInfo::operator+(hours)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);
    ti.t_ += dh;
    return ti;
}
TimeInfo TimeInfo::operator+(const date::days &dd) const
{
    const std::string op_name = "TimeInfo::operator+(days)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);
    ti.t_ += dd;
    return ti;
}
TimeInfo TimeInfo::operator+(const date::months &dm) const
{
    const std::string op_name = "TimeInfo::operator+(months)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);

    auto td = date::floor<date::days>(this->t_);
    date::year_month_day ymd{td};
    ti.t_ = date::local_days{ymd + dm} + (t_-td);
    return ti;
}
TimeInfo TimeInfo::operator+(const date::years &dy) const
{
    const std::string op_name = "TimeInfo::operator+(years)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    TimeInfo ti(*this);

    auto td = date::floor<date::days>(this->t_);
    date::year_month_day ymd{td};
    ti.t_ = date::local_days{ymd + dy} + (t_-td);
    return ti;
}

void TimeInfo::operator+=(const std::chrono::seconds &ds)
{
    const std::string op_name = "TimeInfo::operator+=(seconds)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    this->t_ += ds;
}
void TimeInfo::operator+=(const std::chrono::minutes &dm)
{
    const std::string op_name = "TimeInfo::operator+=(minutes)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    this->t_ += dm;
}
void TimeInfo::operator+=(const std::chrono::hours &dh)
{
    const std::string op_name = "TimeInfo::operator+=(hours)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    this->t_ += dh;
}
void TimeInfo::operator+=(const date::days &dd)
{
    const std::string op_name = "TimeInfo::operator+=(days)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    this->t_ += dd;
}
void TimeInfo::operator+=(const date::months &dm)
{
    const std::string op_name = "TimeInfo::operator+=(months)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    auto td = date::floor<date::days>(this->t_);
    date::year_month_day ymd{td};
    this->t_ = date::local_days{ymd + dm} + (t_-td);
}
void TimeInfo::operator+=(const date::years &dy)
{
    const std::string op_name = "TimeInfo::operator+=(years)";
    FnTrace(op_name.c_str());
    throw_if_uninitialized(op_name);
    auto td = date::floor<date::days>(this->t_);
    date::year_month_day ymd{td};
    this->t_ = date::local_days{ymd + dy} + (t_-td);
}



bool TimeInfo::operator> (const TimeInfo &other) const
{
    FnTrace("TimeInfo::operator >()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ > other.t_);
}

bool TimeInfo::operator>= (const TimeInfo &other) const
{
    FnTrace("TimeInfo::operator >=()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ >= other.t_);
}

bool TimeInfo::operator< (const TimeInfo &other) const
{
    FnTrace("TimeInfo::operator <()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ < other.t_);
}

bool TimeInfo::operator<= (const TimeInfo &other) const
{
    FnTrace("TimeInfo::operator <=()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ <= other.t_);
}

bool TimeInfo::operator== (const TimeInfo &other) const
{
    FnTrace("TimeInfo::operator ==()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ == other.t_);
}

bool TimeInfo::operator!= (const TimeInfo &other) const
{
    FnTrace("TimeInfo:: operator !=()");
    assert(this->is_valid_);
    assert(other.is_valid_);
    return (this->t_ != other.t_);
}

std::chrono::seconds TimeInfo::operator -(const TimeInfo &other) const
{
    assert(this->is_valid_);
    assert(other.is_valid_);
    auto diff = std::chrono::seconds{this->t_ - other.t_};
    return diff;
}

int TimeInfo::Sec() const
{
    return this->get<std::chrono::seconds, std::chrono::minutes>();
}

int TimeInfo::Min() const
{
    return this->get<std::chrono::minutes, std::chrono::hours>();
}

int TimeInfo::Hour() const
{
    return this->get<std::chrono::hours, date::days>();
}

int TimeInfo::Day() const
{
    auto ld = date::floor<date::days>(t_);
    date::year_month_day ymd{ld};
    auto d = ymd.day();
    return static_cast<int>(static_cast<unsigned int>(d));
}

int TimeInfo::Month() const
{
    auto ld = date::floor<date::days>(t_);
    date::year_month_day ymd{ld};
    auto m = ymd.month();
    return static_cast<int>(static_cast<unsigned int>(m));
}

int TimeInfo::Year() const
{
    auto ld = date::floor<date::days>(t_);
    date::year_month_day ymd{ld};
    auto y = ymd.year();
    return static_cast<int>(y);
}

int TimeInfo::WeekDay() const
{
    FnTrace("TimeInfo::WeekDay()");
    date::weekday wd{date::floor<date::days>(t_)};
    // Workaround for missing unsigned operator to get stored weekday index
    if (wd == date::Sunday)
    {
        return 0;
    } else if (wd == date::Monday)
    {
        return 1;
    } else if (wd == date::Tuesday)
    {
        return 2;
    } else if (wd == date::Wednesday)
    {
        return 3;
    } else if (wd == date::Thursday)
    {
        return 4;
    } else if (wd == date::Friday)
    {
        return 5;
    } else if (wd == date::Saturday)
    {
        return 6;
    } else
    {
        std::cout << wd << std::endl;
        throw std::runtime_error("TimeInfo::Weekday(): Unknown Weekday value");
    }
    // function using unsigned operator as defined in C++20
    // https://en.cppreference.com/w/cpp/chrono/weekday/operator_unsigned
    //unsigned wd_idx = static_cast<unsigned>(wd);
    //return static_cast<int>(wd_idx);
}

int TimeInfo::DaysInMonth() const
{
    using namespace date;
    date::year_month_day ymd{date::floor<date::days>(t_)};
    date::year_month_day ymd_first(ymd.year(), ymd.month(), 1_d);
    date::year_month_day_last ymd_last{ymd.year() / ymd.month() / date::last};

    auto month_days = date::local_days{ymd_last} - date::local_days{ymd_first};
    return month_days.count();
}

std::string TimeInfo::Date() const
{
    FnTrace("TimeInfo::Date()");
    std::ostringstream ss;
    date::to_stream(ss, "%x", t_); // local date representation
    return ss.str();
}

std::string TimeInfo::Time() const
{
    FnTrace("TimeInfo::Time()");
    std::ostringstream ss;
    date::to_stream(ss, "%X", t_); // local time representation
    return ss.str();
}


/**** Other Functions ****/
int DaysInYear(int year)
{
    FnTrace("DaysInYear()");
    if (year <= 0)
        return -1;  // error
    else if (((year % 400) == 0) || (((year % 100) != 0) && ((year % 4) == 0)))
        return 366;  // leap year
    else
        return 365;
}

int DaysInMonth(int month, int year)
{
    FnTrace("DaysInMonth()");
    static int days[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (year <= 0 || month < 1 || month > 12)
        return -1;  // error
    else if (month == 2 &&
             (((year % 400) == 0) || (((year % 100) != 0) && ((year % 4) == 0))))
        return 29;  // Feb & leap year
    else
        return days[month];
}

int DayOfTheWeek(int mday, int month, int year)
{
    FnTrace("DayOfTheWeek()");
    date::year_month_day ymdl{date::year(year) / date::month(month) / date::day(mday)};
    if (!ymdl.ok())
    {
        return -1;
    }
    date::weekday wd{ymdl};
    auto wdi = wd[0];
    return static_cast<int>(wdi.index());
}


int StringElapsedToNow(char* dest, int maxlen, TimeInfo &t1)
{
    FnTrace("StringElapsedToNow()");
    TimeInfo now;
    int seconds;
    int minutes;
    
    now.Set();
    seconds = SecondsElapsed(t1, now);
    minutes = seconds / 60;
    seconds = seconds % 60;
    snprintf(dest, maxlen, "%d:%02d", minutes, seconds);
    return 1;
}

int SecondsToString(char* dest, int maxlen, int seconds)
{
    FnTrace("SecondsToString()");
    int minutes;

    minutes = seconds / 60;
    seconds = seconds % 60;
    snprintf(dest, maxlen, "%d:%02d", minutes, seconds);
    return 1;
}

int SecondsElapsedToNow(const TimeInfo &t1)
{
    FnTrace("SecondsElapsedToNow()");
    TimeInfo now;
    now.Set();
    return SecondsElapsed(t1, now);
}

int SecondsElapsed(const TimeInfo &t1, const TimeInfo &t2)
{
    FnTrace("SecondsElapsed()");
    if (!t1.IsSet())
    {
        throw std::invalid_argument("SecondsElapsed(): t1 is not valid");
    }
    if (!t2.IsSet())
    {
        throw std::invalid_argument("SecondsElapsed(): t2 is not valid");
    }

    if (t1>=t2)
    {
        return static_cast<int>(std::chrono::seconds{t1-t2}.count());
    } else
    {
        return static_cast<int>(std::chrono::seconds{t2-t1}.count());
    }
}

int MinutesElapsedToNow(const TimeInfo &t1)
{
    FnTrace("MinutesElapsedToNow()");
    TimeInfo now;

    now.Set();
    return MinutesElapsed(t1, now);
}

int MinutesElapsed(const TimeInfo &t1, const TimeInfo &t2)
{
    FnTrace("MinutesElapsed()");
    return SecondsElapsed(t1, t2) / 60;
}

