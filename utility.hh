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

#ifndef _UTILITY_HH
#define _UTILITY_HH

#include "basic.hh"
#include <time.h>

extern int debug_mode;

#define LOCK_RUNNING VIEWTOUCH_PATH "/bin/.vt_is_running"
#define DIR_PERMISSIONS  0777

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef ISDIGIT
#define ISDIGIT(x) (((x) >= '0') && ((x) <= '9'))
#endif

#define STRSHORT    64
#define STRLENGTH   512   // constant to set the length of a string
#define STRLONG     2048  // 2K string

#ifdef DEBUG
// BackTrace Functions/Data
extern genericChar BT_Stack[STRLENGTH][STRLONG];
extern int  BT_Depth;
extern int  BT_Track;

class BackTraceFunction
{
public:
    // Constructor
    BackTraceFunction(char *str, ...)
    {
        if (BT_Track)
            printf("Entering %s\n", str);
        sprintf(BT_Stack[BT_Depth++], str);
    }
    // Destructor
    virtual ~BackTraceFunction()
    {
        --BT_Depth;
    }
};

#define FnTrace(x) BackTraceFunction _fn_start(x)
#define FnTraceEnable(x) (BT_Track = (x))
void FnPrintTrace(void);
void FnPrintLast(int depth);
char *FnReturnLast();
#define LINE() printf("%s:  Got to line %d\n", __FILE__, __LINE__)
#else
#define FnTrace(x)
#define FnTraceEnable(x)
#define FnPrintTrace()
#define FnPrintLast(x)
#define FnReturnLast() ""
#define LINE()
#endif

void vt_init_setproctitle(int argc, char *argv[]);
int  vt_setproctitle(char *title);


/**** Types ****/
class InputDataFile;
class OutputDataFile;

enum RenderResult
{
    RENDER_OKAY,  // render okay
    RENDER_ERROR // error in rendering
};

enum SignalResult
{
    SIGNAL_ERROR = -1, // error in processing signal
    SIGNAL_OKAY,       // signal received
    SIGNAL_IGNORED,    // signal not useful
    SIGNAL_END,        // signal received - don't send it to anyone else
    SIGNAL_TERMINATE   // signal received - terminate me
};

// Dynamic string storage class
class Str
{
    genericChar *data;
    short buffer_len;

public:
    Str *next;
    Str *fore;

    short length;

    // Constructors
    Str();
    Str(char *str);
    Str(Str &s);
    // Destructor
    ~Str();

    // Member Functions
    int   Clear();
    int   Set(char *str);
    int   Set(int val);
    int   Set(Flt val);
    int   Set(Str &s) { return Set(s.Value()); }
    int   Set(Str *s) { return Set(s->Value()); }
    int   ChangeAtoB(char a, genericChar b);  // character replace
    int   IntValue();
    Flt   FltValue();
    genericChar *Value();
    genericChar *ValueSet(char *set = NULL);

    Str & operator =  (char *s) { Set(s); return *this; }
    Str & operator =  (Str  &s) { Set(s); return *this; }
    Str & operator =  (int   s) { Set(s); return *this; }
    Str & operator =  (Flt   s) { Set(s); return *this; }
    int   operator >  (Str  &s);
    int   operator <  (Str  &s);
    int   operator == (Str  &s);
    int   operator != (Str  &s);
};

// Time & Date (second accuracy) storage class
class TimeInfo
{
    short year;
    Uchar sec;
    Uchar min;
    Uchar hour;
    Uchar mday;
    Uchar month;
    Schar wday;  // can be -1 (unknown)

public:
    // Constructor
    TimeInfo();

    // Member Functions
    int   Set();                          // Sets time values to current time
    int   Set(int s, int y);
    int   Set(time_t t);
    int   Set(genericChar *date_string);
    int   Set(TimeInfo *timevar);         // Copies time value or clears if NULL
    int   Set(TimeInfo &timevar);         // Copies time value
    TimeInfo *ValueSet(TimeInfo *timevar = NULL);
    int   Clear();                        // Erases time value
    int   IsSet();                        // boolean - has time been set?
    int   AdjustMinutes(int amount);      // Adds 'amount' to current minute
    int   AdjustDays(int amount);         // Adds 'amount' to current day
    int   AdjustMonths(int amount);       // Adds 'amount' to current month
    int   AdjustYears(int amount);        // Adds 'amount' to current year
    genericChar *DebugPrint(genericChar *dest = NULL);  // Just prints the time value for debugging
    genericChar *ToString(genericChar *dest = NULL);
    genericChar *Date(genericChar *dest = NULL);
    genericChar *Time(genericChar *dest = NULL);

    TimeInfo & operator =  (TimeInfo &t);
    int        operator >  (TimeInfo &t);
    int        operator >= (TimeInfo &t);
    int        operator <  (TimeInfo &t);
    int        operator <= (TimeInfo &t);
    int        operator == (TimeInfo &t);
    int        operator != (TimeInfo &t);

    int Sec()          { return (int) sec; }
    int Sec(int s)     { sec = s; return s; }
    int Min()          { return (int) min; }
    int Min(int m)     { min = m; return m; }
    int Hour()         { return (int) hour; }
    int Hour(int h)    { hour = h; return h; }
    int WeekDay();  // always calculate it; otherwise adjustments will skew it and we won't know
    int WeekDay(int d) { wday = d; return d; }
    int Day()          { return (int) mday; }
    int Day(int d)     { mday = d; return d; }
    int Month()        { return month; }
    int Month(int m)   { month = m; return m; }
    int Year()         { return year; }
    int Year(int y)    { year = y; return y; }
};

// Rectangular Region class
class RegionInfo
{
public:
    short x;
    short y;
    short w;
    short h;

    // Constructors
    RegionInfo();
    RegionInfo(RegionInfo &r);
    RegionInfo(RegionInfo *r);
    RegionInfo(int rx, int ry, int rw, int rh);
    
    // Destructor
    virtual ~RegionInfo();

    // Member Functions
    int SetRegion(RegionInfo &r) {
        return SetRegion(r.x, r.y, r.w, r.h); }
    int SetRegion(RegionInfo *r) {
        return SetRegion(r->x, r->y, r->w, r->h); }
    int SetRegion(int rx, int ry, int rw, int rh) {
        x = rx; y = ry; w = rw; h = rh; return 0; }
    int GetRegion(RegionInfo &r) {
        r.x = x; r.y = y; r.w = w; r.h = h; return 0; }
    int GetRegion(RegionInfo *r) {
        r->x = x; r->y = y; r->w = w; r->h = h; return 0; }
    int GetRegion(int &rx, int &ry, int &rw, int &rh) {
        rx = x; ry = y; rw = w; rh = h; return 0; }

    int IsSet() {
        return (w > 0) && (h > 0); }
    virtual int IsPointIn(int px, int py) {
        return px >= x && py >= y && px < (x + w) && py < (y + h); }
    int Overlap(int rx, int ry, int rw, int rh) {
        return rx < (x + w) && ry < (y + h) && (rx + rw) > x && (ry + rh) > y; }
    int IsValid() {
        return (w >= 0) && (h >= 0); }

    int Fit(int rx, int ry, int rw, int rh);
    int Fit(RegionInfo &r) {
        return Fit(r.x, r.y, r.w, r.h); }
    int Fit(RegionInfo *r) {
        return Fit(r->x, r->y, r->w, r->h); }

    int Intersect(int rx, int ry, int rw, int rh);
    int Intersect(RegionInfo &r) {
        return Intersect(r.x, r.y, r.w, r.h); }
    int Intersect(RegionInfo *r) {
        return Intersect(r->x, r->y, r->w, r->h); }

    int Left()   { return x; }
    int Top()    { return y; }
    int Right()  { return x + w - 1; }
    int Bottom() { return y + h - 1; }
    int Width()  { return w; }
    int Height() { return h; }
};

class Price
{
public:
    int   amount;
    short type;
    short decimal;

    // Constructor
    Price(int price_amount, int price_type);

    // Member Functions
    int   Read(InputDataFile &df, int version);
    int   Write(OutputDataFile &df, int version);
    genericChar *Format(int sign = 0);
    genericChar *Format(char *buffer, int sign = 0);
    genericChar *SimpleFormat();
    genericChar *SimpleFormat(char *buffer);
};


/**** Global Variables ****/
extern TimeInfo SystemTime;
// Current system time


/**** Other Functions ****/
extern int ReportError(char *message);
// general error reporting function
// (the prototype is here but the implementation isn't in utility.cc)
// implementation is in main/manager.cc

int DaysInYear(int year);
int DaysInMonth(int month, int year);
// Returns number of days in given year or month

int DayOfTheWeek(int mday, int month, int year);
// Returns what day of the week it is

int StringElapsedToNow(char *dest, int maxlen, TimeInfo &t1);
int SecondsToString(char *dest, int maxlen, int seconds);

int SecondsElapsedToNow(TimeInfo &t1);
int SecondsElapsed(TimeInfo &t1, TimeInfo &t2);
// Returns number of seconds between two times

int MinutesElapsedToNow(TimeInfo &t1);
int MinutesElapsed(TimeInfo &t1, TimeInfo &t2);
// Returns minutes elapsed between two times (ingnoring second values)

int StringCompare(const genericChar *str1, const genericChar *str2, int len = -1);
// Case insesitive string compare
int StringInString(const genericChar *haystack, const genericChar *needle);

char *StringToLower(char *str);
char *StringToUpper(char *str);
// Convert string to lowercase/uppercase - returns string
int   StripWhiteSpace(genericChar *string);

char *AdjustCase(char *string);
// capitalizes 1st letter of every word in string - returns string

int AdjustCaseAndSpacing(char *str);

int CompareList(genericChar *val, genericChar *list[], int unknown = -1);
int CompareList(int   val, int   list[], int unknown = -1);
// compares val with each item in list (NULL terminated or -1 terminated)
// returns index of match or 'unknown' for no match.

int CompareListN(genericChar *list[], genericChar *str, int unknown = -1);
// automatically checks string length for each comparison, so
// str="hello" will match list[0]="hello world".

char *FindStringByValue(int val, int val_list[], genericChar *str_list[],
                        genericChar *unknown = NULL);
int   FindValueByString(genericChar *val, int val_list[], genericChar *str_list[],
                        int unknown = -1);
// finds string by finding val index
int FindIndexOfValue(int value, int val_list[], int unknown = -1);

genericChar *NextName(genericChar *name, genericChar **list);
int NextValue(int val, int *val_array);
int ForeValue(int val, int *val_array);
// Finds next/prior value in array (-1 terminated)

int NextToken(genericChar *dest, genericChar *src, genericChar sep, int *idx);
int NextInteger(int *dest, genericChar *src, genericChar sep, int *idx);

int DoesFileExist(const genericChar *filename);
// Returns 1 if file exists otherwise 0

int EnsureFileExists(const genericChar *filename);

int DeleteFile(const genericChar *filename);
// Deletes file from disk

int BackupFile(const genericChar *filename);
// Renames file to file.bak

int RestoreBackup(const genericChar *filename);
// Renames file.bak to file

int FltToPrice(Flt value);
Flt PriceToFlt(int price);
// Conversion from Flt to price and back again

Flt PercentToFlt(int percent);
int FltToPercent(Flt value);
// Conversion from Flt to percent (2 digits of accuracy) and back again

int LockDevice(genericChar *devpath);
int UnlockDevice(int id);
#endif
