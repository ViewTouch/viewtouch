/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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

#pragma GCC diagnostic ignored "-Wnull-dereference"

#include "basic.hh"
#include "fntrace.hh"
#include "time_info.hh"

#include <string>
#include <sys/stat.h>  // for mode_t

extern int debug_mode;

// Modern C++ constants instead of macros
inline constexpr const char* LOCK_RUNNING = VIEWTOUCH_PATH "/bin/.vt_is_running";
inline constexpr mode_t DIR_PERMISSIONS = 0777;

// Note: Use Max() from basic.hh or std::max from <algorithm> instead of MAX macro
// Note: Use std::isdigit() from <cctype> instead of ISDIGIT macro
// Note: STRSHORT, STRLENGTH, and STRLONG are now defined in basic.hh

void vt_init_setproctitle(int argc, char* argv[]);
int  vt_setproctitle(const char* title);


/**** Types ****/
class InputDataFile;
class OutputDataFile;

enum RenderResult : std::uint8_t
{
    RENDER_OKAY,  // render okay
    RENDER_ERROR // error in rendering
};

enum SignalResult : std::int8_t
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
    [[nodiscard]] int   IntValue() const;
    [[nodiscard]] Flt   FltValue() const;
    [[nodiscard]] const char *Value() const noexcept;
    [[nodiscard]] const char *c_str() const noexcept;
    [[nodiscard]] std::string str() const noexcept;
    const char* ValueSet(const char* set = nullptr);

    [[nodiscard]] bool   empty() const noexcept;
    [[nodiscard]] size_t size() const noexcept;

    Str & operator =  (const char* s) { Set(s); return *this; }
    Str & operator =  (const Str  &s) {
        if (this == &s) {
            return *this;
        }
        Set(s);
        return *this;
    }
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
    short x{0};
    short y{0};
    short w{0};
    short h{0};

    // Constructors
    RegionInfo() = default;
    RegionInfo(const RegionInfo &r) = default;
    RegionInfo(const RegionInfo *r);
    RegionInfo(int rx, int ry, int rw, int rh);
    
    // Assignment operator
    RegionInfo& operator=(const RegionInfo&) = default;
    
    // Destructor
    virtual ~RegionInfo();

    // Member Functions
    [[nodiscard]] constexpr int SetRegion(const RegionInfo &r) noexcept {
        return SetRegion(r.x, r.y, r.w, r.h); }
    [[nodiscard]] int SetRegion(const RegionInfo *r) noexcept {
        return SetRegion(r->x, r->y, r->w, r->h); }
    [[nodiscard]] constexpr int SetRegion(int rx, int ry, int rw, int rh) noexcept {
        x = static_cast<short>(rx); 
        y = static_cast<short>(ry); 
        w = static_cast<short>(rw); 
        h = static_cast<short>(rh); 
        return 0; }
    
    [[nodiscard]] constexpr int GetRegion(RegionInfo &r) const noexcept {
        r.x = x; r.y = y; r.w = w; r.h = h; return 0; }
    [[nodiscard]] int GetRegion(RegionInfo *r) const noexcept {
        r->x = x; r->y = y; r->w = w; r->h = h; return 0; }
    [[nodiscard]] constexpr int GetRegion(int &rx, int &ry, int &rw, int &rh) const noexcept {
        rx = x; ry = y; rw = w; rh = h; return 0; }

    [[nodiscard]] constexpr bool IsSet() const noexcept {
        return (w > 0) && (h > 0); }
    [[nodiscard]] virtual constexpr bool IsPointIn(int px, int py) const noexcept {
        return px >= x && py >= y && px < (x + w) && py < (y + h); }
    [[nodiscard]] constexpr bool Overlap(int rx, int ry, int rw, int rh) const noexcept {
        return rx < (x + w) && ry < (y + h) && (rx + rw) > x && (ry + rh) > y; }
    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return (w >= 0) && (h >= 0); }

    int Fit(int rx, int ry, int rw, int rh);
    int Fit(const RegionInfo &r) {
        return Fit(r.x, r.y, r.w, r.h); }
    int Fit(const RegionInfo *r) {
        return Fit(r->x, r->y, r->w, r->h); }

    int Intersect(int rx, int ry, int rw, int rh);
    int Intersect(const RegionInfo &r) {
        return Intersect(r.x, r.y, r.w, r.h); }
    int Intersect(const RegionInfo *r) {
        return Intersect(r->x, r->y, r->w, r->h); }

    [[nodiscard]] constexpr int Left() const noexcept   { return x; }
    [[nodiscard]] constexpr int Top() const noexcept    { return y; }
    [[nodiscard]] constexpr int Right() const noexcept  { return x + w - 1; }
    [[nodiscard]] constexpr int Bottom() const noexcept { return y + h - 1; }
    [[nodiscard]] constexpr int Width() const noexcept  { return w; }
    [[nodiscard]] constexpr int Height() const noexcept { return h; }
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

// compare case independent string
// len ... length of string to compare until, full string comparison if set to -1
// for example StringCompare("ab", "abcd", 2)==0
// for example StringCompare("ab", "abcd")!=0
int StringCompare(const std::string &str1, const std::string &str2, int len=-1);
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
// compares val with each item in list (nullptr terminated or -1 terminated)
// returns index of match or 'unknown' for no match.

int CompareListN(const genericChar* list[], const genericChar* str, int unknown = -1);
// automatically checks string length for each comparison, so
// str="hello" will match list[0]="hello world".

const char* FindStringByValue(int val, int val_list[], const genericChar* str_list[],
                        const genericChar* unknown = nullptr);
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
