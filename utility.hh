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

#ifndef VT_UTILITY_HH
#define VT_UTILITY_HH

#include "basic.hh"
#include "fntrace.hh"
#include "time_info.hh"

#include <string>

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

void vt_init_setproctitle(int argc, char* argv[]);
int  vt_setproctitle(const char* title);


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
    std::string data;

public:
    Str *next = nullptr;
    Str *fore = nullptr;

    // Constructors
    Str();
    Str(const std::string &str);
    Str(const Str &s);
    // Destructor
    ~Str();

    // Member Functions
    int   Clear();
    bool  Set(const char *str);
    bool  Set(const std::string &str);
    bool  Set(const int val);
    bool  Set(const Flt val);
    bool  Set(const Str &s) { data = s.data; return true; }
    bool  Set(const Str *s) { return Set(s->Value()); }
    void  ChangeAtoB(const char a, const char b);  // character replace
    int   IntValue() const;
    Flt   FltValue() const;
    const char *Value() const;
    const char *c_str() const;
    std::string str() const;
    const char* ValueSet(const char* set = nullptr);

    bool   empty() const;
    size_t size() const;

    Str & operator =  (const char* s) { Set(s); return *this; }
    Str & operator =  (const Str  &s) { Set(s); return *this; }
    Str & operator =  (const int   s) { Set(s); return *this; }
    Str & operator =  (const Flt   s) { Set(s); return *this; }
    int   operator >  (const Str  &s) const;
    int   operator <  (const Str  &s) const;
    int   operator == (const Str  &s) const;
    bool operator == (const std::string &s) const;
    int   operator != (const Str  &s) const;
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
    const genericChar* Format(int sign = 0);
    const genericChar* Format(const char* buffer, int sign = 0);
    const genericChar* SimpleFormat();
    const genericChar* SimpleFormat(const char* buffer);
};


/**** Other Functions ****/
extern int ReportError(const std::string &message);
// general error reporting function
// (the prototype is here but the implementation isn't in utility.cc)
// implementation is in main/manager.cc

// compare same-length part of string
// for example StringCompare("ab", "abcd")==0
int StringCompare(const std::string &str1, const std::string &str2);
// Case insesitive string compare
int StringInString(const genericChar* haystack, const genericChar* needle);

std::string StringToUpper(const std::string &str);
std::string StringToLower(const std::string &str);
// Convert string to lowercase/uppercase - returns string
int   StripWhiteSpace(genericChar* string);

// strip leading and trailing whitespace, only one space between words
std::string StringAdjustSpacing(const std::string &str);

// capitalizes 1st letter of every word in string - returns string
std::string AdjustCase(const std::string &str);

std::string AdjustCaseAndSpacing(const std::string &str);

int CompareList(const genericChar* val, const genericChar* list[], int unknown = -1);
int CompareList(int   val, int   list[], int unknown = -1);
// compares val with each item in list (NULL terminated or -1 terminated)
// returns index of match or 'unknown' for no match.

int CompareListN(const genericChar* list[], const genericChar* str, int unknown = -1);
// automatically checks string length for each comparison, so
// str="hello" will match list[0]="hello world".

const char* FindStringByValue(int val, int val_list[], const genericChar* str_list[],
                        const genericChar* unknown = NULL);
int   FindValueByString(const genericChar* val, int val_list[], const genericChar* str_list[],
                        int unknown = -1);
// finds string by finding val index
int FindIndexOfValue(int value, int val_list[], int unknown = -1);

const genericChar* NextName(const genericChar* name, const genericChar* *list);
int NextValue(int val, int *val_array);
int ForeValue(int val, int *val_array);
// Finds next/prior value in array (-1 terminated)

int NextToken(genericChar* dest, const genericChar* src, genericChar sep, int *idx);
int NextInteger(int *dest, const genericChar* src, genericChar sep, int *idx);

int DoesFileExist(const genericChar* filename);
// Returns 1 if file exists otherwise 0

int EnsureFileExists(const genericChar* filename);

int DeleteFile(const genericChar* filename);
// Deletes file from disk

int BackupFile(const genericChar* filename);
// Renames file to file.bak

int RestoreBackup(const genericChar* filename);
// Renames file.bak to file

int FltToPrice(Flt value);
Flt PriceToFlt(int price);
// Conversion from Flt to price and back again

Flt PercentToFlt(int percent);
int FltToPercent(Flt value);
// Conversion from Flt to percent (2 digits of accuracy) and back again

int LockDevice(const genericChar* devpath);
int UnlockDevice(int id);
#endif // VT_UTILITY_HH
