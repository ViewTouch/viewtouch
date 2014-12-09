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

#include "manager.hh"
#include "utility.hh"

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

char* progname = NULL;
int   progname_maxlen = 0;

#ifdef DEBUG
// BackTrace
char BT_Stack[STRLENGTH][STRLONG];
int  BT_Depth = 0;
int  BT_Track = 0;
//int  BT_Track = 1;
//int  BT_Depth = 1;

void FnPrintTrace(void)
{
    fprintf(stdout, "Stack Trace (%d):\n", BT_Depth);
    for (int i = 0; i < BT_Depth; ++i)
        fprintf(stdout, "    (%d) %s\n", i + 1, BT_Stack[i]);
    fprintf(stdout, "\n");
}
void FnPrintLast(int depth)
{
    // depth == 0 means print the current function
    // depth == 1 means print the parent function
    // depth == 5 means print the last five functions on the stack if there are that many
    depth = BT_Depth - depth - 1;
    if (depth < 0)
        depth = 0;
    fprintf(stderr, "Stack Trace (%d of %d):\n", BT_Depth - depth, BT_Depth);
    for (int i = depth; i < BT_Depth; ++i )
        fprintf(stderr, "    (%d) %s\n", i + 1, BT_Stack[i]);
    fprintf(stderr, "\n");
}
const char* FnReturnLast()
{
    static char last[STRLENGTH];

    last[0] = '\0';
    if (BT_Depth == 1)
        strcpy(last, BT_Stack[0]);
    else if (BT_Depth > 1)
        strcpy(last, BT_Stack[BT_Depth - 2]);

    return last;
}
int debug_mode = 1;
#else
int debug_mode = 0;
#endif

void vt_init_setproctitle(int argc, char* argv[])
{
#ifdef BSD
    progname = NULL;
    progname_maxlen = 0;
#else
    // This is mostly for Linux, maybe others.
    // clear out the arguments (1..N) so we get a nice process list
    int idx = 1;
    int idx2 = 0;
    while (idx < argc)
    {
        idx2 = 0;
        while (argv[idx][idx2] != '\0')
        {
            argv[idx][idx2] = '\0';
            idx2 += 1;
        }
        idx += 1;
    }
    progname = argv[0];
    progname_maxlen = strlen(progname) - 1;
#endif
}

/****
 * vt_setproctitle:  Returns 1 on error, 0 otherwise.
 ****/
int vt_setproctitle(const char* title)
{
    FnTrace("setproctitle()");
    int retval = 1;

#ifdef BSD
    setproctitle("%s", title);
    retval = 0;
#else
    if (progname != NULL)
    {
        // This isn't intended to set an arbitrary length title.  It's
        // intended to set a short title after a long title (AKA,
        // right now, vt_main starts off with a title set by the OS of
        // "/usr/viewtouch/bin/vt_main"; I change that to "vt_main
        // pri" or "vt_main dns").  If you need to set a title longer
        // than the starting title, you'll need to rewrite this (save
        // off the environment, etc.).
        strncpy(progname, title, progname_maxlen);
        progname[progname_maxlen] = '\0';
        retval = 0;
    }
#endif

    return retval;
}

/**** Global Variables ****/
TimeInfo SystemTime;


/**** Definitions ****/
#define MIN_STRING_SIZE 8
#define VERY_LARGE (1<<30)-1


/**** Str Class ****/
// Constructors

//temp for testing
//int Str::nAllocated = 0;

Str::Str()
{
    next       = NULL;
    fore       = NULL;
    length     = 0;
    buffer_len = 0;
    data       = (char* ) NULL;
}

Str::Str(const char* str)
{
    next       = NULL;
    fore       = NULL;
    length     = 0;
    buffer_len = 0;
    data     = (char* ) NULL;
    Set(str);
}

Str::Str(Str &s)
{
    next       = NULL;
    fore       = NULL;
    length     = 0;
    buffer_len = 0;
    data     = (char* ) NULL;
    Set(s);
}

// Destructor
Str::~Str()
{
    if (data)
	{
        delete [] data;
	}
}

// Member Functions
int Str::Clear()
{
    FnTrace("Str::Clear()");
	if (data)
	{
		delete [] data;
		data = (char* ) NULL;
	}

    buffer_len = 0;
    length     = 0;
    return 0;
}

int Str::Set(const char* str)
{
    FnTrace("Str::Set(const char* )");
	int len = 0;
	if (str)
		len = strlen(str);

	if (len <= 0)
		Clear();
	else if (buffer_len > len)
	{
		length = len;
		strncpy(data, str, buffer_len);
	}
	else
	{
		int   nlength;
        int   nbuffer_len;
		char* nstring;

		nlength = len;
		nbuffer_len = nlength + 1;
		nstring = new char[nbuffer_len];
		if (nstring == NULL)
			return 1;  // error - can't change data

		length     = nlength;
		buffer_len = nbuffer_len;

		if (data)
			delete [] data;

		data     = nstring;
		strncpy(data, str, buffer_len);
	}
	return 0;
}

int Str::Set(int val)
{
    FnTrace("Str::Set(int)");
    genericChar buffer[32];
    sprintf(buffer, "%d", val);
    return Set(buffer);
}

int Str::Set(Flt val)
{
    FnTrace("Str::(Flt)");
    genericChar buffer[32];
    sprintf(buffer, "%g", val);
    return Set(buffer);
}

int Str::ChangeAtoB(genericChar a, genericChar b)
{
    FnTrace("Str::ChangeAtoB()");
    if (data == NULL)
        return 1;

    genericChar* c = data;
    while (*c)
    {
        if (*c == a)
            *c = b;
        ++c;
    }
    return 0;
}

int Str::IntValue()
{
    FnTrace("Str::IntValue()");
    if (data)
        return atoi(data);
    else
        return 0;
}

Flt Str::FltValue()
{
    FnTrace("Str::FltValue()");
    if (data)
        return atof(data);
    else
        return 0.0;
}

const char* Str::Value() const
{
    FnTrace("Str::Value()");
    static genericChar blank_string[] = "";
    if (data)
        return data;
    else
        return blank_string;
}

char* Str::Value()
{
    FnTrace("Str::Value()");
    static genericChar blank_string[] = "";
    if (data)
        return data;
    else
        return blank_string;
}

/****
 * ValueSet:  Always return the new value of the variable, but
 *  only set it if set is non-NULL.
 ****/
const char* Str::ValueSet(const char* set)
{
    FnTrace("Str::ValueSet()");
    if (set)
        Set(set);
    return Value();
}

int Str::operator > (Str &s)
{
    FnTrace("Str::operator >()");
    const genericChar* s1 = Value();
    const genericChar* s2 = s.Value();

    while (*s1 && *s2)
    {
        if (*s1 > *s2)
            return 1;
        else if (*s1 < *s2)
            return 0;

        ++s1; ++s2;
    }
    return (*s1 != '\0');
}

int Str::operator < (Str &s)
{
    FnTrace("Str::operator <()");
    const genericChar* s1 = Value();
    const genericChar* s2 = s.Value();

    while (*s1 && *s2)
    {
        if (*s1 < *s2)
            return 1;
        else if (*s1 > *s2)
            return 0;

        ++s1; ++s2;
    }
    return (*s2 != '\0');
}

int Str::operator == (Str &s)
{
    FnTrace("Str::opterator ==()");
    if (length != s.length)
        return 0;

    const genericChar* s1 = Value();
    const genericChar* s2 = s.Value();

    while (*s1)
    {
        if (*s1 != *s2)
            return 0;
        ++s1; ++s2;
    }
    return 1;
}

int Str::operator != (Str &s)
{
    FnTrace("Str::operator !=()");
    if (length != s.length)
        return 1;

    const genericChar* s1 = Value();
    const genericChar* s2 = s.Value();

    while (*s1)
    {
        if (*s1 != *s2)
            return 1;
        ++s1; ++s2;
    }
    return 0;
}


/**** TimeInfo Class ****/
// Constructor
TimeInfo::TimeInfo()
{
    Clear();
}

// Member Functions
int TimeInfo::Set()
{
    FnTrace("TimeInfo::Set()");
    time_t     t;
    struct tm *td;

    t  = time((time_t *) NULL);
    td = localtime(&t);

    sec   = td->tm_sec;
    min   = td->tm_min;
    hour  = td->tm_hour;
    wday  = td->tm_wday;
    mday  = td->tm_mday;
    month = td->tm_mon  + 1;
    year  = td->tm_year + 1900;
    return 0;
}

int TimeInfo::Set(int s, int y)
{
    FnTrace("TimeInfo::Set(int, int)");

    sec    = (s % 60);
    min    = ((s / 60) % 60);
    hour   = ((s / 3600) % 24);
    mday   = ((s / 86400)   % 31) + 1;
    month  = ((s / 2678400) % 12) + 1;
    wday   = DayOfTheWeek(mday, month, y);
    year   = y;
    return 0;
}

int TimeInfo::Set(time_t t)
{
    FnTrace("TimeInfo::Set(time_t)");
    struct tm *td;

    td    = localtime(&t);
    sec   = td->tm_sec;
    min   = td->tm_min;
    hour  = td->tm_hour;
    wday  = td->tm_wday;
    mday  = td->tm_mday;
    month = td->tm_mon  + 1;
    year  = td->tm_year + 1900;
    return 0;
}

/****
 * Set:  Parses date strings.  Acceptible formats:
 *   "DD/MM/YY,HH:MM"   in 24hour format
 *   "DD/MM/YYYY,HH:MM" in 24hour format
 *  Returns 1 on error, 0 on success.
 ****/
int TimeInfo::Set(const genericChar* date_string)
{
    FnTrace("TimeInfo::Set(const genericChar* )");
    int idx = 0;
    int buffer;
    int retval = 0;

    if (NextInteger(&buffer, date_string, '/', &idx))
    {
        mday = (Uchar) buffer;
        if (NextInteger(&buffer, date_string, '/', &idx))
        {
            month = (Uchar) buffer;
            if (NextInteger(&buffer, date_string, ',', &idx))
            {
                year = (short) buffer;
                if (year < 70)
                    year += 2000;
                else if (year < 1900)
                    year += 1900;
                if (NextInteger(&buffer, date_string, ':', &idx))
                {
                    hour = (Uchar) buffer;
                    if (NextInteger(&buffer, date_string, ':', &idx))
                        min = (Uchar) buffer;
                }
            }
        }
    }

    if (retval)
        Clear();
    return retval;
}

int TimeInfo::Set(TimeInfo *timevar)
{
    FnTrace("TimeInfo::Set(TimeInfo *)");
    if (timevar)
        *this = *timevar;
    else
        Clear();
    return 0;
}

int TimeInfo::Set(TimeInfo &timevar)
{
    FnTrace("TimeInfo::Set(TimeInfo &)");
    *this = timevar;
    return 0;
}

TimeInfo *TimeInfo::ValueSet(TimeInfo *timevar)
{
    TimeInfo *retval = NULL;

    if (timevar != NULL)
        Set(timevar);
    if (IsSet())
    {
        retval = this;
    }
    return retval;
}

int TimeInfo::Clear()
{
    FnTrace("TimeInfo::Clear()");
    sec   = 0;
    min   = 0;
    hour  = 0;
    wday  = -1;
    mday  = 1;
    month = 1;
    year  = 0;
    return 0;
}

int TimeInfo::IsSet()
{
    return (year > 0);
}

int TimeInfo::AdjustMinutes(int amount)
{
	char str[64] = {0};
	sprintf (str, "TimeInfo::AdjustMinutes(%d)\n", amount);
    FnTrace(str);
    //FnTrace("TimeInfo::AdjustMinutes()");
    if (year <= 0)
        return 1;

    int d_adjust = 0;
    int m = min + amount;
    int h = hour;

    // Fast enough for adjusting small amounts
    if (amount > 0)
    {
        while (m >= 60)
        {
            m -= 60;
            ++h;
        }

        while (h >= 24)
        {
            h -= 24;
            ++d_adjust;
        }
    }
    else if (amount < 0)
    {
        while (m < 0)
        {
            m += 60;
            --h;
        }

        while (h < 0)
        {
            h += 24;
            --d_adjust;
        }
    }

    min  = (Uchar) m;
    hour = (Uchar) h;
    if (d_adjust != 0)
        AdjustDays(d_adjust);

    return 0;
}

int TimeInfo::AdjustDays(int amount)
{
	char str[64] = {0};
	sprintf (str, "TimeInfo::AdjustDays(%d)\n", amount);
	FnTrace(str);
    
    //FnTrace("TimeInfo::AdjustDays()");
    int y = year;
    if (y <= 1)
        return 1;

    int d = mday + amount;
    int w = wday + amount;
    int m = month;

    // Fast enough for adjusting small amounts
    if (amount > 0)
    {
        while (d > DaysInMonth(m, y))
        {
            d -= DaysInMonth(m, y);
            ++m;

            while (m > 12)
            {
                m -= 12;
                ++y;
            }
        }
        wday = (Uchar) (w % 7);
    }
    else if (amount < 0)
    {
        while (d < 1)
        {
            --m;
            while (m < 1)
            {
                m += 12;
                --y;
            }
            d += DaysInMonth(m, y);
        }

        w = w % 7;
        if (w < 0)
            w += 7;
        wday = (Uchar) w;
    }

    mday  = (Uchar) d;
    month = (Uchar) m;
    year  = (short) y;
    return 0;
}

int TimeInfo::AdjustMonths(int amount)
{
	char str[64] = {0};
	sprintf (str, "TimeInfo::Months(%d)\n", amount);
	FnTrace(str);
//     FnTrace("TimeInfo::AdjustMonths()");
    int y = year;
    if (y <= 1)
        return 1;

    int m = month + amount;
    while (m > 12)
    {
        m -= 12;
        ++y;
    }
    while (m < 1)
    {
        m += 12;
        --y;
    }

    if (y < 1)
        y = 1;

    int d = DaysInMonth(m, y);
    if (mday > d)
        mday = (Uchar) d;

    wday  = DayOfTheWeek((int) mday, m, y);
    month = (Uchar) m;
    year  = (short) y;
    return 0;
}

int TimeInfo::AdjustYears(int amount)
{
	char str[64] = {0};
	sprintf (str, "TimeInfo::AdjustYears(%d)\n", amount);
	FnTrace(str);
//     FnTrace("TimeInfo::AdjustYears()");
    if (year <= 1)
        return 1;

    year += amount;
    if (year < 1)
        year = 1;

    int d = DaysInMonth(month, year);
    if (mday > d)
        mday = (Uchar) d;

    wday = DayOfTheWeek((int) mday, (int) month, year);
    return 0;
}

genericChar* TimeInfo::DebugPrint(genericChar* dest)
{
    FnTrace("TimeInfo::DebugPrint()");
    static genericChar buffer[STRLONG];
    if (dest == NULL)
        dest = buffer;
    snprintf(dest, STRLONG, "%d %d, %d  %d:%02d", month, mday, year, hour, min);
    return dest;
}

/****
 * ToString:  converts the TimeInfo to a string that can later be
 *  parsed by Set(const char* ): "DD/MM/YY,HH:MM" in 24hour format
 ****/
genericChar* TimeInfo::ToString(genericChar* dest)
{
    FnTrace("TimeInfo::ToString()");
    static char buffer[STRLONG];
    if (dest == NULL)
        dest = buffer;

    snprintf(dest, 20, "%02d/%02d/%04d,%02d:%02d", mday, month, year, hour, min);
    return dest;
}

TimeInfo &TimeInfo::operator= (TimeInfo &t)
{
    FnTrace("TimeInfo::operator =()");
    sec   = t.sec;
    min   = t.min;
    hour  = t.hour;
    wday  = t.wday;
    mday  = t.mday;
    month = t.month;
    year  = t.year;
    return *this;
}

int TimeInfo::operator> (TimeInfo &t)
{
    FnTrace("TimeInfo::operator >()");
    if      (year  > t.year)  return 1;
    else if (year  < t.year)  return 0;
    else if (month > t.month) return 1;
    else if (month < t.month) return 0;
    else if (mday  > t.mday)  return 1;
    else if (mday  < t.mday)  return 0;
    else if (hour  > t.hour)  return 1;
    else if (hour  < t.hour)  return 0;
    else if (min   > t.min)   return 1;
    else if (min   < t.min)   return 0;
    else if (sec   > t.sec)   return 1;
    else                      return 0; // both are equal
}

int TimeInfo::operator>= (TimeInfo &t)
{
    FnTrace("TimeInfo::operator >=()");
    if      (year  > t.year)  return 1;
    else if (year  < t.year)  return 0;
    else if (month > t.month) return 1;
    else if (month < t.month) return 0;
    else if (mday  > t.mday)  return 1;
    else if (mday  < t.mday)  return 0;
    else if (hour  > t.hour)  return 1;
    else if (hour  < t.hour)  return 0;
    else if (min   > t.min)   return 1;
    else if (min   < t.min)   return 0;
    else if (sec   > t.sec)   return 1;
    else                      return 1; // both are equal
}

int TimeInfo::operator< (TimeInfo &t)
{
    FnTrace("TimeInfo::operator <()");
    if      (year  < t.year)  return 1;
    else if (year  > t.year)  return 0;
    else if (month < t.month) return 1;
    else if (month > t.month) return 0;
    else if (mday  < t.mday)  return 1;
    else if (mday  > t.mday)  return 0;
    else if (hour  < t.hour)  return 1;
    else if (hour  > t.hour)  return 0;
    else if (min   < t.min)   return 1;
    else if (min   > t.min)   return 0;
    else if (sec   < t.sec)   return 1;
    else                      return 0; // both are equal
}

int TimeInfo::operator<= (TimeInfo &t)
{
    FnTrace("TimeInfo::operator <=()");
    if      (year  < t.year)  return 1;
    else if (year  > t.year)  return 0;
    else if (month < t.month) return 1;
    else if (month > t.month) return 0;
    else if (mday  < t.mday)  return 1;
    else if (mday  > t.mday)  return 0;
    else if (hour  < t.hour)  return 1;
    else if (hour  > t.hour)  return 0;
    else if (min   < t.min)   return 1;
    else if (min   > t.min)   return 0;
    else if (sec   < t.sec)   return 1;
    else                      return 1; // both are equal
}

int TimeInfo::operator== (TimeInfo &t)
{
    FnTrace("TimeInfo::operator ==()");
    return (year == t.year && month == t.month && mday == t.mday &&
            hour == t.hour && min == t.min && sec == t.sec);
}

int TimeInfo::operator!= (TimeInfo &t)
{
    FnTrace("TimeInfo:: operator !=()");
    return (sec != t.sec || min != t.min || hour != t.hour || mday != t.mday ||
            month != t.month || year != t.year);
}

int TimeInfo::WeekDay()
{
    FnTrace("TimeInfo::WeekDay()");
    return (int)DayOfTheWeek(mday, month, year);
}

char* TimeInfo::Date(genericChar* dest)
{
    FnTrace("TimeInfo::Date()");
    static char buffer[STRLONG] = "";

    if (dest == NULL)
        dest = buffer;
    
    sprintf(dest, "%02d/%02d/%04d", month, mday, year);

    return dest;
}

char* TimeInfo::Time(genericChar* dest)
{
    FnTrace("TimeInfo::Time()");
    static char buffer[STRLONG] = "";

    if (dest == NULL)
        dest = buffer;

    if (hour == 0)
        sprintf(dest, "12:%d AM", min);
    else if (hour < 13)
        sprintf(dest, "%d:%d AM", hour, min);
    else
        sprintf(dest, "%d:%d PM", hour - 12, min);

    return dest;
}


/**** Region Class ****/
// Constructors
RegionInfo::RegionInfo()
{
    x = 0;
    y = 0;
    w = 0;
    h = 0;
}

RegionInfo::RegionInfo(RegionInfo &r)
{
    x = r.x;
    y = r.y;
    w = r.w;
    h = r.h;
}

RegionInfo::RegionInfo(RegionInfo *r)
{
    x = r->x;
    y = r->y;
    w = r->w;
    h = r->h;
}

RegionInfo::RegionInfo(int rx, int ry, int rw, int rh)
{
    x = rx;
    y = ry;
    w = rw;
    h = rh;
}

//Destructor
RegionInfo::~RegionInfo()
{
}

// Member Functions
int RegionInfo::Fit(int rx, int ry, int rw, int rh)
{
    FnTrace("RegionInfo::Fit()");
    if (w == 0 && h == 0)
        return SetRegion(rx, ry, rw, rh);

    int x2  = x + w;
    int y2  = y + h;
    int rx2 = rx + rw;
    int ry2 = ry + rh;

    if (rx2 > x2) x2 = rx2;
    if (ry2 > y2) y2 = ry2;

    if (rx < x) x = rx;
    if (ry < y) y = ry;

    w = x2 - x;
    h = y2 - y;
    return 0;
}

int RegionInfo::Intersect(int rx, int ry, int rw, int rh)
{
    FnTrace("RegionInfo::Intersect()");
    int x2  = x + w;
    int y2  = y + h;
    int rx2 = rx + rw;
    int ry2 = ry + rh;

    if (rx2 < x2) x2 = rx2;
    if (ry2 < y2) y2 = ry2;

    if (rx > x) x = rx;
    if (ry > y) y = ry;

    w = x2 - x;
    h = y2 - y;
    return 0;
}

/**** Price Class ****/
// Constructor
Price::Price(int price_amount, int price_type)
{
}

// Member Functions
int Price::Read(InputDataFile &df, int version)
{
    return 1;
}

int Price::Write(OutputDataFile &df, int version)
{
    return 1;
}

const char* Price::Format(int sign)
{
    return NULL;
}

const char* Price::Format(const char* buffer, int sign)
{
    return NULL;
}

const char* Price::SimpleFormat()
{
    return NULL;
}

const char* Price::SimpleFormat(const char* buffer)
{
    return NULL;
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
    if (mday < 1 || mday > 31 || month < 1 || month > 12 ||
        year < 1970 || year > 2037)
        return -1;

    struct tm x;
    x.tm_sec   = 0;
    x.tm_min   = 0;
    x.tm_hour  = 12;
    x.tm_mday  = mday;
    x.tm_mon   = month - 1;
    x.tm_year  = year  - 1900;
    x.tm_isdst = -1;

    time_t  t_val = mktime(&x);
    struct tm *td = localtime(&t_val);

    return td->tm_wday;
}


char* StringToLower(char* str)
{
    FnTrace("StringToLower()");
    while (*str)
    {
        *str = tolower(*str);
        ++str;
    }
    return str;
}

char* StringToUpper(char* str)
{
    FnTrace("StringToUpper()");
    while (*str)
    {
        *str = toupper(*str);
        ++str;
    }
    return str;
}

/****
 * StripWhiteSpace:  remove all whitespace characters from the beginning
 *  and the end of string.
 ****/
int StripWhiteSpace(genericChar* longstr)
{
    FnTrace("StripWhiteSpace()");
    int idx = 0;
    int storeidx = 0;  // where to move characters to
    int len = strlen(longstr);
    int count = 0;

    // strip from the beginning
    while (isspace(longstr[idx]))
        idx += 1;
    count += 0;
    while (idx > 0 && idx < len)
    {
        longstr[storeidx] = longstr[idx];
        storeidx += 1;
        idx += 1;
    }
    if (storeidx > 0)
        longstr[storeidx] = '\0';

    // strip from the end
    len = strlen(longstr) - 1;
    while (len > 0 && isspace(longstr[len]))
    {
        longstr[len] = '\0';
        len -= 1;
        count += 1;
    }
    return count;
}

char* AdjustCase(char* str)
{
    FnTrace("AdjustCase()");
    genericChar* c = str;
    int capital = 1;
    while (*c)
    {
        if (isspace(*c) || ispunct(*c))
            capital = 1;
        else if (capital)
        {
            *c = toupper(*c);
            capital = 0;
        }
        else
            *c = tolower(*c);
        ++c;
    }
    return str;
}

int AdjustCaseAndSpacing(char* str)
{
    FnTrace("AdjustCaseAndSpacing()");
    genericChar* s = str, *d = str;
    int word = 0;

    while (*s)
    {
        if (isspace(*s))
        {
            if (word)
            {
                *d++ = *s;
                word = 0;
            }
        }
        else if (word)
            *d++ = tolower(*s);
        else
        {
            *d++ = toupper(*s);
            word = 1;
        }
        ++s;
    }

    if (d > str && isspace(*(d - 1)))
        --d;
    *d = '\0';
    return 0;
}

const genericChar* NextName(const genericChar* name, const genericChar* *list)
{
    FnTrace("NextName()");
    int idx = 0;

    // find the current string
    while (list[idx] != NULL && (strcmp(name, list[idx]) != 0))
        idx += 1;

    // advance to next
    if (list[idx] != NULL)
        idx += 1;
    // wrap to beginning if necessary
    if (list[idx] == NULL)
        idx = 0;

    return list[idx];
}

int NextValue(int val, int *val_array)
{
    FnTrace("NextValue()");
    int idx = CompareList(val, val_array);
    ++idx;
    if (val_array[idx] < 0)
        idx = 0;
    return val_array[idx];
}

int ForeValue(int val, int *val_array)
{
    FnTrace("ForeValue()");
    int idx = CompareList(val, val_array);
    --idx;
    if (idx < 0)
    {
        idx = 0;
        while (val_array[idx] >= 0)
            ++idx;
    }
    return val_array[idx];
}

/****
 * NextToken:  similar to strtok_r, except that it returns success or
 *  failure and the destination string is passed as an argument.
 *  sep is allowed to change between calls.  The function never backtracks,
 *  so the next token, whether the same or different, will always be
 *  found (or not found) after the previous token.  idx must point to a
 *  storage space for the index.  src is NOT modified by NextToken.
 ****/
int NextToken(genericChar* dest, const genericChar* src, genericChar sep, int *idx)
{
    FnTrace("NextToken()");
    int destidx = 0;
    int retval = 0;

    if (src[*idx] != '\0')
    {
        while (src[*idx] != '\0' && src[*idx] != sep)
        {
            dest[destidx] = src[*idx];
            destidx += 1;
            *idx += 1;
        }
        dest[destidx] = '\0';
        // gobble up the token and any extra tokens
        while (src[*idx] == sep && src[*idx] != '\0')
            *idx += 1;
        retval = 1;
    }
    return retval;
}

/****
 * NextInteger:  Like NextToken, but converts the resulting string to
 *  an integer and stores that value in dest.
 ****/
int NextInteger(int *dest, const genericChar* src, genericChar sep, int *idx)
{
    FnTrace("NextInteger()");
    genericChar buffer[STRLONG];
    int retval = 0;

    if (NextToken(buffer, src, sep, idx))
    {
        *dest = atoi(buffer);
        retval = 1;
    }
    return retval;
}

int BackupFile(const genericChar* filename)
{
    FnTrace("BackupFile()");
    if (DoesFileExist(filename) == 0)
        return 1;  // No file to backup

    genericChar bak[256];
    sprintf(bak,  "%s.bak", filename);

    if (DoesFileExist(bak))
    {
        genericChar bak2[256];
        sprintf(bak2, "%s.bak2", filename);

        // delete *.bak2
        unlink(bak2);
        // move *.bak to *.bak2
        link(bak, bak2);
        unlink(bak);
    }

    // move * to *.bak
    link(filename, bak);
    unlink(filename);
    return 0;
}

int RestoreBackup(const genericChar* filename)
{
    FnTrace("RestoreBackup()");
    genericChar str[256];
    sprintf(str, "%s.bak", filename);

    if (DoesFileExist(str) == 0)
        return 1;  // No backup to restore

    sprintf(str, "/bin/cp %s.bak %s", filename, filename);
    return system(str);
}


int FltToPrice(Flt value)
{
    FnTrace("FltToPrice()");
    if (value >= 0.0)
        return (int) ((value * 100.0) + .5);
    else
        return (int) ((value * 100.0) - .5);
}

Flt PriceToFlt(int price)
{
    FnTrace("PriceToFlt()");
    return (Flt) ((Flt) price / 100.0);
}

int FltToPercent(Flt value)
{
    FnTrace("FltToPercent()");
    if (value >= 0.0)
        return (int) ((value * 10000.0) + .5);
    else
        return (int) ((value * 10000.0) - .5);
}

Flt PercentToFlt(int percent)
{
    FnTrace("PercentToFlt()");
    return (Flt) percent / 10000.0;
}


const char* FindStringByValue(int val, int val_list[], const genericChar* str_list[], const genericChar* unknown)
{
    FnTrace("FindStringByValue()");

    for (int i = 0; str_list[i] != NULL; ++i)
    {
        if (val == val_list[i]) 
			return str_list[i];
	}

    return unknown;
}

int FindValueByString(const genericChar* val, int val_list[], const genericChar* str_list[], int unknown)
{
    FnTrace("FindValueByString()");
    int retval = unknown;
    int idx = 0;

    while (val_list[idx] >= 0 && retval == unknown)
    {
        if (strcmp(val, str_list[idx]) == 0)
            retval = val_list[idx];
        idx += 1;
    }

    return retval;
}

/****
 * FindIndexOfValue:  The Name[] and Value[] arrays in labels.cc are not necessarily
 *  in order.  For example, compare the FAMILY_ constants in sales.hh to the
 *  FamilyName[] and FamilyValue[] arrays.  This function compares value to the
 *  elements of FamilyValue and returns the index of the match, or -1 if no
 *  match was found.
 ****/
//FIX BAK-->This function and FindStringByValue should not be necessary.
//FIX       Should reorder the Name[] and Value[] arrays to coincide with the
//FIX       constants wherever possible.
int FindIndexOfValue(int value, int val_list[], int unknown)
{
    FnTrace("FindIndexOfValue()");
    int retval = unknown;
    int idx = 0;
    int fvalue = val_list[idx];
    while (fvalue >= 0)
    {
        if (value == fvalue)
        {
            retval = idx;
            fvalue = -1;  // exit the loop
        }
        else
        {
            idx += 1;
            fvalue = val_list[idx];
        }
    }
    return retval;
}



int DoesFileExist(const genericChar* filename)
{
    FnTrace("DoesFileExist()");
    if (filename == NULL)
        return 0;
    int status = access(filename, F_OK);
    return (status == 0);  // Return true if file exists
}

/****
 * EnsureFileExists:  Creates the file or directory if it does not
 *  already exist.  Returns 0 if the file exists or if it is successfully
 *  created.  Returns 1 on error.
 ****/
int EnsureFileExists(const genericChar* filename)
{
    FnTrace("EnsureFileExists()");
    int retval = 1;

    if (filename == NULL)
        return retval;

    if (DoesFileExist(filename) == 0)
    {
        if (mkdir(filename, 0) == 0)
        {
            if (chmod(filename, DIR_PERMISSIONS) == 0)
                retval = 0;
        }
    }
    else
        retval = 0;

    return retval;
}

int DeleteFile(const genericChar* filename)
{
    FnTrace("DeleteFile()");
    if (filename == NULL || strlen(filename) <= 0 || access(filename, F_OK))
        return 1;
    unlink(filename);
    return 0;
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

int SecondsElapsedToNow(TimeInfo &t1)
{
    FnTrace("SecondsElapsedToNow()");
    TimeInfo now;

    now.Set();
    return SecondsElapsed(t1, now);
}

int SecondsElapsed(TimeInfo &t1, TimeInfo &t2)
{
    FnTrace("SecondsElapsed()");
    time_t time1 = 0, time2 = 0;

    if (t1.IsSet())
    {
        struct tm x;
        x.tm_sec   = t1.Sec();
        x.tm_min   = t1.Min();
        x.tm_hour  = t1.Hour();
        x.tm_mday  = t1.Day();
        x.tm_mon   = t1.Month() - 1;
        x.tm_year  = t1.Year() - 1900;
        x.tm_isdst = -1;
        time1 = mktime(&x);
    }
    else
        time1 = time((time_t *) NULL);

    if (t2.IsSet())
    {
        struct tm x;
        x.tm_sec   = t2.Sec();
        x.tm_min   = t2.Min();
        x.tm_hour  = t2.Hour();
        x.tm_mday  = t2.Day();
        x.tm_mon   = t2.Month() - 1;
        x.tm_year  = t2.Year() - 1900;
        x.tm_isdst = -1;
        time2 = mktime(&x);
    }
    else
        time2 = time((time_t *) NULL);

    if (time1 > time2)
        return (int) difftime(time1, time2);
    else
        return (int) difftime(time2, time1);
}

int MinutesElapsedToNow(TimeInfo &t1)
{
    FnTrace("MinutesElapsedToNow()");
    TimeInfo now;

    now.Set();
    return MinutesElapsed(t1, now);
}

int MinutesElapsed(TimeInfo &t1, TimeInfo &t2)
{
    FnTrace("MinutesElapsed()");
    time_t time1 = 0, time2 = 0;

    if (t1.IsSet())
    {
        struct tm x;
        x.tm_sec   = 0;
        x.tm_min   = t1.Min();
        x.tm_hour  = t1.Hour();
        x.tm_mday  = t1.Day();
        x.tm_mon   = t1.Month() - 1;
        x.tm_year  = t1.Year() - 1900;
        x.tm_isdst = -1;
        time1 = mktime(&x);
    }
    else
        time1 = time((time_t *) NULL);

    if (t2.IsSet())
    {
        struct tm x;
        x.tm_sec   = 0;
        x.tm_min   = t2.Min();
        x.tm_hour  = t2.Hour();
        x.tm_mday  = t2.Day();
        x.tm_mon   = t2.Month() - 1;
        x.tm_year  = t2.Year() - 1900;
        x.tm_isdst = -1;
        time2 = mktime(&x);
    }
    else
        time2 = time((time_t *) NULL);

    if (time1 > time2)
        return (int) (difftime(time1, time2) / 60.0);
    else
        return (int) (difftime(time2, time1) / 60.0);
}

int StringCompare(const genericChar* str1, const genericChar* str2, int len)
{
    FnTrace("StringCompare()");
	char a, b;
    while (*str1 && *str2)
    {
        a = tolower(*str1);
        b = tolower(*str2);
        if (a < b)
            return -1;
        else if (a > b)
            return 1;

        if (len > 0 && (--len == 0))
            return 0;

        ++str1;
        ++str2;
    }

    if (*str1)
        return 1;
    else if (*str2)
        return -1;
    else
        return 0;
}

/****
 * StringInString:  We want to find needle inside haystack, case-insensitive.
 *   Returns 1 if the string is found, 0 otherwise.
 *   NOTE:  this function does not return the position of needle in
 *   haystack, just whether or not it is there.
 ****/
int StringInString(const genericChar* haystack, const genericChar* needle)
{
    const genericChar* c1 = (const genericChar* )haystack;
    const genericChar* l1 = (const genericChar* )haystack;
    const genericChar* c2 = (const genericChar* )needle;
    int match = 0;

    while (*c1 && *c2)
    {
        if (tolower(*c1) == tolower(*c2))
        {
            if (match == 0)
                l1 = c1;
            c1++;
            c2++;
            match = 1;
        }
        else
        {
            if (match)
            {
                c2 = (const genericChar* )needle;
                c1 = l1;
            }
            match = 0;
            c1++;
        }
    }

    if (*c2 == '\0')
        return 1;
    else
        return 0;
}

// FIX - convert the following three functions into a single template if possible
int CompareList(const genericChar* str, const genericChar* list[], int unknown)
{
    FnTrace("CompareList(char)");
    for (int i = 0; list[i] != NULL; ++i)
    {
        if (StringCompare(str, list[i]) == 0)
            return i;
    }
    return unknown;
}

int CompareList(int val, int list[], int unknown)
{
    FnTrace("CompareList(int)");
    for (int i = 0; list[i] >= 0; ++i)
    {
        if (val == list[i])
            return i;
    }
    return unknown;
}

/****
 * HasSpace:  returns 1 if word contains a space character,
 *   0 otherwise.
 ****/
int HasSpace(const genericChar* word)
{
    FnTrace("HasSpace()");
    int len = strlen(word);
    int idx;
    int has_space = 0;
    
    for (idx = 0; idx < len; idx++)
    {
        if (isspace(word[idx]))
        {
            has_space = 1;
            idx = len;
        }
    }
    return has_space;
}

/****
 * CompareListN:  search for str in each element of list (similar to
 *   strncmp).  Returns the list[] index of the first match, or
 *   the unknown parameter if no match is found.
 *   NOTE:  There is the problem of the list having both "next"
 *   and "nextsearch", so that next always matches.  The assumption
 *   made is that if we want a truncated search ("nextsearch" matches
 *   "nextsearch WORD") there will always be a space following the
 *   keyword.  If there is no space, the two words must match
 *   exactly, in length as well as content.
 *
 * if word has a space
 *     search for item within word
 * else if word and item are equal length
 *     search for exact match
 * if match
 *     return index within list (not within string)
 ****/
int CompareListN(const genericChar* list[], const genericChar* word, int unknown)
{
    FnTrace("CompareListN()");
    int wordlen = strlen(word);
    int idx = 0;
    int itemlen;
    int search = -1;

    while (list[idx] != NULL)
    {
        search = -1;
        itemlen = strlen(list[idx]);

        if (list[idx][itemlen - 1] == ' ')
            search = StringCompare(word, list[idx], itemlen);
        else if (itemlen == wordlen)
            search = StringCompare(word, list[idx]);

        if (search == 0)
            return idx;
        idx += 1;
    }
    return -1;
}

/*********************************************************************
 * I want to control access to the /dev files, but apparently flock
 * can't touch them.  I also want to do locking uniformly and
 * transparently.  So we create two functions here.  LockDevice will
 * return a positive ID on success, a negative number on failure.
 * The returned ID must be passed to UnlockDevice to remove the lock.
 ********************************************************************/

#define LOCK_DIR VIEWTOUCH_PATH "/bin/.lock"

int LockDevice(const genericChar* devpath)
{
    FnTrace("LockDevice()");
    int retval = 0;
    genericChar lockpath[STRLONG];
    genericChar buffer[STRLONG];
    int idx = 0;
    struct stat sb;

    // make sure the lock directory exists
    if (stat(LOCK_DIR, &sb))
    {
        if (mkdir(LOCK_DIR, 0755))
            perror("Cannot create lock directory");
        chmod(LOCK_DIR, 0755);
    }
    
    // convert the file name from /dev/lpt0 to .dev.lpt0
    strcpy(buffer, devpath);
    while (buffer[idx] != '\0')
    {
        if (buffer[idx] == '/')
            buffer[idx] = '.';
        idx += 1;
    }
    snprintf(lockpath, STRLONG, "%s/%s", LOCK_DIR, buffer);

    retval = open(lockpath, O_WRONLY | O_CREAT, 0755);
    if (retval > 0)
    {
        if (flock(retval, LOCK_EX))
        {
            close(retval);
            retval = -1;
        }
    }
    else if (retval < 0)
    {
        printf("LockDevice error %d for %s", errno, devpath);
    }

    return retval;
}

int UnlockDevice(int id)
{
    FnTrace("UnlockDevice()");
    int retval = 1;

    if (id > 0)
    {
        if (flock(id, LOCK_UN) == 0)
        {
            if (close(id) == 0)
                retval = 0;
            // Should the file be deleted?
        }
    }
    return retval;
}
