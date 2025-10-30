/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
#include "fntrace.hh"
#include "safe_string_utils.hh"

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
#include <algorithm> // std::replace

#ifdef DMALLOC
#include <dmalloc.h>
#endif

char* progname = nullptr;
int   progname_maxlen = 0;

void vt_init_setproctitle(int argc, char* argv[])
{
#ifdef BSD
    progname = nullptr;
    progname_maxlen = 0;
#else
    // This is mostly for Linux, maybe others.
    // clear out the arguments (1..N) so we get a nice process list
    for (int idx = 1; idx < argc; ++idx)
    {
        for (int idx2 = 0; argv[idx][idx2] != '\0'; ++idx2)
        {
            argv[idx][idx2] = '\0';
        }
    }
    progname = argv[0];
    progname_maxlen = static_cast<int>(strlen(progname)) - 1;
#endif
}

/****
 * vt_setproctitle:  Returns 1 on error, 0 otherwise.
 ****/
int vt_setproctitle(const char* title)
{
    FnTrace("setproctitle()");
    if (!title) {
        return 1; // invalid parameter
    }
    
    int retval = 1;

#ifdef BSD
    setproctitle("%s", title);
    retval = 0;
#else
    if (progname != nullptr)
    {
        // This isn't intended to set an arbitrary length title.  It's
        // intended to set a short title after a long title (AKA,
        // right now, vt_main starts off with a title set by the OS of
        // "/usr/viewtouch/bin/vt_main"; I change that to "vt_main
        // pri" or "vt_main dns").  If you need to set a title longer
        // than the starting title, you'll need to rewrite this (save
        // off the environment, etc.).
        vt_safe_string::safe_copy(progname, progname_maxlen + 1, title);
        progname[progname_maxlen] = '\0';
        retval = 0;
    }
#endif

    return retval;
}


/**** Str Class ****/
// Constructors

//temp for testing
//int Str::nAllocated = 0;

Str::Str()
{}

Str::Str(const std::string &str)
{
    data = str;
}

Str::Str(const Str &s)
{
    data = s.Value();
}

// Destructor
Str::~Str()
{}

// Member Functions
int Str::Clear()
{
    FnTrace("Str::Clear()");
    data.clear();
    return 0;
}

bool Str::Set(const char *str)
{
    FnTrace("Str::Set(const char *)");
    if (str) {
        data = str;
    } else {
        data.clear();
    }
    return true;
}

bool Str::Set(const std::string &str)
{
    FnTrace("Str::Set(const std::string &)");
    data = str;
    return true;
}

bool Str::Set(const int val)
{
    FnTrace("Str::Set(int)");
    data = std::to_string(val);
    return true;
}

bool Str::Set(const Flt val)
{
    FnTrace("Str::(Flt)");
    data = std::to_string(val);
    return true;
}

void Str::ChangeAtoB(const char a, const char b)
{
    FnTrace("Str::ChangeAtoB()");
    std::replace(data.begin(), data.end(), a, b);
}

int Str::IntValue() const
{
    FnTrace("Str::IntValue()");
    if (!data.empty()) {
        try {
            return std::stoi(data);
        } catch (const std::exception&) {
            return 0;
        }
    }
    return 0;
}

Flt Str::FltValue() const
{
    FnTrace("Str::FltValue()");
    if (!data.empty()) {
        try {
            return static_cast<Flt>(std::stod(data));
        } catch (const std::exception&) {
            return 0.0;
        }
    }
    return 0.0;
}

const char* Str::Value() const noexcept
{
    FnTrace("Str::Value()");
    return data.c_str();
}

const char* Str::c_str() const noexcept
{
    FnTrace("Str::c_str()");
    return data.c_str();
}

std::string Str::str() const noexcept
{
    FnTrace("Str::Value()");
    return data;
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

bool   Str::empty() const noexcept
{
    return data.empty();
}

size_t Str::size() const noexcept
{
    return data.size();
}

int Str::operator > (const Str &s) const
{
    FnTrace("Str::operator >()");
    return this->data > s.data;
}

int Str::operator < (const Str &s) const
{
    FnTrace("Str::operator <()");
    return this->data < s.data;
}

int Str::operator == (const Str &s) const
{
    FnTrace("Str::opterator ==()");
    return this->data == s.data;
}

bool Str::operator ==(const std::string &s) const
{
    FnTrace("Str::opterator ==()");
    return this->data == s;
}

int Str::operator != (const Str &s) const
{
    FnTrace("Str::operator !=()");
    return this->data != s.data;
}


/**** Region Class ****/
// Constructors (default constructor and copy constructor are defaulted in header)

RegionInfo::RegionInfo(const RegionInfo *r)
    : x(r ? r->x : 0)
    , y(r ? r->y : 0)
    , w(r ? r->w : 0)
    , h(r ? r->h : 0)
{
}

RegionInfo::RegionInfo(int rx, int ry, int rw, int rh)
    : x(static_cast<short>(rx))
    , y(static_cast<short>(ry))
    , w(static_cast<short>(rw))
    , h(static_cast<short>(rh))
{
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
std::string StringToLower(const std::string &str)
{
    FnTrace("StringToLower()");
    std::string data = str;
    for (char &c : data)
    {
        c = std::tolower(c);
    }
    return data;
}

std::string StringToUpper(const std::string &str)
{
    FnTrace("StringToUpper()");
    std::string data = str;
    for (char &c : data)
    {
        c = std::toupper(c);
    }
    return data;
}

/****
 * StripWhiteSpace:  remove all whitespace characters from the beginning
 *  and the end of string.
 ****/
int StripWhiteSpace(genericChar* longstr)
{
    FnTrace("StripWhiteSpace()");
    if (!longstr) {
        return 0; // invalid parameter
    }
    
    int idx = 0;
    int storeidx = 0;  // where to move characters to
    const int len = static_cast<int>(strlen(longstr));
    int count = 0;

    // strip from the beginning
    while (idx < len && isspace(longstr[idx]))
        ++idx;
    
    while (idx < len)
    {
        longstr[storeidx] = longstr[idx];
        ++storeidx;
        ++idx;
    }
    if (storeidx > 0)
        longstr[storeidx] = '\0';

    // strip from the end
    int end_len = static_cast<int>(strlen(longstr)) - 1;
    while (end_len > 0 && isspace(longstr[end_len]))
    {
        longstr[end_len] = '\0';
        --end_len;
        ++count;
    }
    return count;
}

std::string AdjustCase(const std::string &str)
{
    FnTrace("AdjustCase()");

    std::string data = str;
    bool capital = true;
    for (char &c : data)
    {
        if (isspace(c) || ispunct(c))
        {
            capital = true; // next character should be upper case
        } else if (capital)
        {
            c = std::toupper(c);
            capital = false; // next character should be lower case
        } else
        {
            c = std::tolower(c);
        }
    }
    return data;
}

std::string StringAdjustSpacing(const std::string &str)
{
    FnTrace("StringAdjustSpacing()");
    std::string data;
    data.reserve(str.size());

    bool space = true; // flag to shorten spaces to one digit
    for (const char &s : str)
    {
        if (std::isspace(s))
        {
            if (!space)
            {
                space = true; // add only first space
                data.push_back(' ');
            }
        }
        else if (std::isprint(s))
        {
            data.push_back(s);
            space = false;
        }
    }
    if (space && !data.empty())
    {
        data.pop_back(); // remove trailing space
    }
    return data;
}

std::string AdjustCaseAndSpacing(const std::string &str)
{
    FnTrace("AdjustCaseAndSpacing()");
    return AdjustCase(StringAdjustSpacing(str));
}

const genericChar* NextName(const genericChar* name, const genericChar* *list)
{
    FnTrace("NextName()");
    if (!name || !list) {
        return nullptr; // invalid parameters
    }
    
    int idx = 0;

    // find the current string
    while (list[idx] != nullptr && (strcmp(name, list[idx]) != 0))
        ++idx;

    // advance to next
    if (list[idx] != nullptr)
        ++idx;
    // wrap to beginning if necessary
    if (list[idx] == nullptr)
        idx = 0;

    return list[idx];
}

int NextValue(int val, int *val_array)
{
    FnTrace("NextValue()");
    if (!val_array) {
        return -1; // invalid parameter
    }
    
    int idx = CompareList(val, val_array);
    ++idx;
    if (val_array[idx] < 0)
        idx = 0;
    return val_array[idx];
}

int ForeValue(int val, int *val_array)
{
    FnTrace("ForeValue()");
    if (!val_array) {
        return -1; // invalid parameter
    }
    
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
    if (!dest || !src || !idx) {
        return 0; // invalid parameters
    }
    
    int destidx = 0;
    int retval = 0;

    if (src[*idx] != '\0')
    {
        while (src[*idx] != '\0' && src[*idx] != sep)
        {
            dest[destidx] = src[*idx];
            ++destidx;
            ++(*idx);
        }
        dest[destidx] = '\0';
        // gobble up the token and any extra tokens
        while (src[*idx] == sep && src[*idx] != '\0')
            ++(*idx);
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
    if (!dest) {
        return 0; // invalid parameter
    }
    
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
    if (!filename) {
        return 1; // invalid parameter
    }
    
    if (DoesFileExist(filename) == 0)
        return 1;  // No file to backup

    genericChar bak[256];
    snprintf(bak, sizeof(bak), "%s.bak", filename);

    if (DoesFileExist(bak))
    {
        genericChar bak2[256];
        snprintf(bak2, sizeof(bak2), "%s.bak2", filename);

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
    if (!filename) {
        return 1; // invalid parameter
    }
    
    genericChar str[256];
    snprintf(str, sizeof(str), "%s.bak", filename);

    if (DoesFileExist(str) == 0)
        return 1;  // No backup to restore

    snprintf(str, sizeof(str), "/bin/cp %s.bak %s", filename, filename);
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
    if (!val_list || !str_list) {
        return unknown; // invalid parameters
    }

    for (int i = 0; str_list[i] != nullptr; ++i)
    {
        if (val == val_list[i]) 
			return str_list[i];
	}

    return unknown;
}

int FindValueByString(const genericChar* val, int val_list[], const genericChar* str_list[], int unknown)
{
    FnTrace("FindValueByString()");
    if (!val || !val_list || !str_list) {
        return unknown; // invalid parameters
    }
    
    int retval = unknown;
    int idx = 0;

    while (val_list[idx] >= 0 && retval == unknown)
    {
        if (strcmp(val, str_list[idx]) == 0)
            retval = val_list[idx];
        ++idx;
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
    if (!val_list) {
        return unknown; // invalid parameter
    }
    
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
            ++idx;
            fvalue = val_list[idx];
        }
    }
    return retval;
}



int DoesFileExist(const genericChar* filename)
{
    FnTrace("DoesFileExist()");
    if (filename == nullptr)
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

    if (filename == nullptr)
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
    if (filename == nullptr || strlen(filename) <= 0 || access(filename, F_OK))
        return 1;
    unlink(filename);
    return 0;
}

int StringCompare(const std::string &str1, const std::string &str2, int len)
{
    FnTrace("StringCompare()");
    if (len < 0)
    {
        // compare full string
        const std::string str1_lower = StringToLower(str1);
        const std::string str2_lower = StringToLower(str2);
        return str1_lower.compare(str2_lower);
    }
    // compare up to len
    const std::string str1_lower = StringToLower(str1.substr(0, static_cast<size_t>(len)));
    const std::string str2_lower = StringToLower(str2.substr(0, static_cast<size_t>(len)));
    return str1_lower.compare(str2_lower);
}

/****
 * StringInString:  We want to find needle inside haystack, case-insensitive.
 *   Returns 1 if the string is found, 0 otherwise.
 *   NOTE:  this function does not return the position of needle in
 *   haystack, just whether or not it is there.
 ****/
int StringInString(const genericChar* haystack, const genericChar* needle)
{
    if (!haystack || !needle) {
        return 0; // invalid parameters
    }
    
    const genericChar* c1 = haystack;
    const genericChar* l1 = haystack;
    const genericChar* c2 = needle;
    int match = 0;

    while (*c1 && *c2)
    {
        if (tolower(*c1) == tolower(*c2))
        {
            if (match == 0)
                l1 = c1;
            ++c1;
            ++c2;
            match = 1;
        }
        else
        {
            if (match)
            {
                c2 = needle;
                c1 = l1;
            }
            match = 0;
            ++c1;
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
    if (!str || !list) {
        return unknown; // invalid parameters
    }
    
    for (int i = 0; list[i] != nullptr; ++i)
    {
        if (StringCompare(str, list[i]) == 0)
            return i;
    }
    return unknown;
}

int CompareList(int val, int list[], int unknown)
{
    FnTrace("CompareList(int)");
    if (!list) {
        return unknown; // invalid parameter
    }
    
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
    if (!word) {
        return 0; // invalid parameter
    }
    
    const int len = static_cast<int>(strlen(word));
    int has_space = 0;
    
    for (int idx = 0; idx < len; ++idx)
    {
        if (isspace(word[idx]))
        {
            has_space = 1;
            break;
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
    if (!list || !word) {
        return unknown; // invalid parameters
    }
    
    const int wordlen = static_cast<int>(strlen(word));
    int idx = 0;
    int itemlen;
    int search = -1;

    while (list[idx] != nullptr)
    {
        search = -1;
        itemlen = static_cast<int>(strlen(list[idx]));

        if (list[idx][itemlen - 1] == ' ')
            search = StringCompare(word, list[idx], itemlen);
        else if (itemlen == wordlen)
            search = StringCompare(word, list[idx]);

        if (search == 0)
            return idx;
        ++idx;
    }
    return unknown;
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
    if (!devpath) {
        return -1; // invalid parameter
    }
    
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
    vt_safe_string::safe_copy(buffer, sizeof(buffer), devpath);
    while (buffer[idx] != '\0')
    {
        if (buffer[idx] == '/')
            buffer[idx] = '.';
        ++idx;
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
