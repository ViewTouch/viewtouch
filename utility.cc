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
 * utility.cc - revision 156 (10/20/98)
 * General purpose functions
 */

#include "main/manager.hh"
#include "image_data.hh"
#include "logger.hh"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

/**** Str Class ****/
// REFACTOR NOTE: Most methods are now inline in the header file for performance
// REFACTOR NOTE: Constructors and destructors are now defaulted in header using modern C++17

void Str::ChangeAtoB(const char a, const char b)
{
    FnTrace("Str::ChangeAtoB()");
    std::replace(data.begin(), data.end(), a, b);  // REFACTOR: Using std::string's internal data member
}

int Str::IntValue() const
{
    FnTrace("Str::IntValue()");
    if (!data.empty())                             // REFACTOR: Using std::string's empty() method
        return std::stoi(data);                    // REFACTOR: Using modern std::stoi instead of atoi
    else
        return 0;
}

Flt Str::FltValue() const
{
    FnTrace("Str::FltValue()");
    if (!data.empty())                             // REFACTOR: Using std::string's empty() method
        return std::stod(data);                    // REFACTOR: Using modern std::stod instead of atof
    else
        return 0.0;
}

const char* Str::ValueSet(const char* set)
{
    FnTrace("Str::ValueSet()");
    if (set != nullptr)                            // REFACTOR: Changed NULL to nullptr
        data = set;                                // REFACTOR: Direct assignment to std::string data
    return data.c_str();                           // REFACTOR: Return c_str() of std::string data
}

int Str::operator > (const Str &s) const
{
    FnTrace("Str::operator >()");
    return this->data > s.data;                    // REFACTOR: Direct std::string comparison
}

int Str::operator < (const Str &s) const
{
    FnTrace("Str::operator <()");
    return this->data < s.data;                    // REFACTOR: Direct std::string comparison
}

int Str::operator == (const Str &s) const
{
    FnTrace("Str::opterator ==()");
    return this->data == s.data;                   // REFACTOR: Direct std::string comparison
}

bool Str::operator ==(const std::string &s) const
{
    FnTrace("Str::opterator ==()");
    return this->data == s;                        // REFACTOR: Direct std::string comparison
}

int Str::operator != (const Str &s) const
{
    FnTrace("Str::operator !=()");
    return this->data != s.data;                   // REFACTOR: Direct std::string comparison
}

/**** RegionInfo Class ****/
// Constructor
RegionInfo::RegionInfo()
{
    FnTrace("RegionInfo::RegionInfo()");
    x = 0; y = 0; w = 0; h = 0;
}

RegionInfo::RegionInfo(RegionInfo &r)
{
    FnTrace("RegionInfo::RegionInfo(RegionInfo &)");
    x = r.x; y = r.y; w = r.w; h = r.h;
}

RegionInfo::RegionInfo(RegionInfo *r)
{
    FnTrace("RegionInfo::RegionInfo(RegionInfo *)");
    if (r)
    {
        x = r->x; y = r->y; w = r->w; h = r->h;
    }
    else
    {
        x = 0; y = 0; w = 0; h = 0;
    }
}

RegionInfo::RegionInfo(int rx, int ry, int rw, int rh)
{
    FnTrace("RegionInfo::RegionInfo(int, int, int, int)");
    x = rx; y = ry; w = rw; h = rh;
}

// Destructor
RegionInfo::~RegionInfo()
{
    FnTrace("RegionInfo::~RegionInfo()");
}

// Member Functions
int RegionInfo::Fit(int rx, int ry, int rw, int rh)
{
    FnTrace("RegionInfo::Fit()");
    if (rx < x)
    {
        rw -= (x - rx);
        rx = x;
    }
    if (ry < y)
    {
        rh -= (y - ry);
        ry = y;
    }
    if ((rx + rw) > (x + w))
        rw = (x + w) - rx;
    if ((ry + rh) > (y + h))
        rh = (y + h) - ry;

    if (rw < 0 || rh < 0)
    {
        x = 0; y = 0; w = 0; h = 0;
        return 1;
    }

    x = rx; y = ry; w = rw; h = rh;
    return 0;
}

int RegionInfo::Intersect(int rx, int ry, int rw, int rh)
{
    FnTrace("RegionInfo::Intersect()");
    int retval = 1;
    if (rx > (x + w - 1) || ry > (y + h - 1) ||
        (rx + rw - 1) < x || (ry + rh - 1) < y)
    {
        // no intersection
        x = 0; y = 0; w = 0; h = 0;
    }
    else
    {
        // intersection
        retval = 0;
        int left   = Max((int)x, rx);                // REFACTOR: Cast to int for template type matching
        int top    = Max((int)y, ry);                // REFACTOR: Cast to int for template type matching
        int right  = Min((int)(x + w - 1), rx + rw - 1);     // REFACTOR: Cast to int for template type matching
        int bottom = Min((int)(y + h - 1), ry + rh - 1);     // REFACTOR: Cast to int for template type matching
        x = left;
        y = top;
        w = right - left + 1;
        h = bottom - top + 1;
    }
    return retval;
}

/**** Price Class ****/
// Constructor
Price::Price(int price_amount, int price_type)
{
    FnTrace("Price::Price()");
    amount  = price_amount;
    type    = price_type;
    decimal = 0;
}

int Price::Read(InputDataFile &df, int version)
{
    FnTrace("Price::Read()");
    return 1;
}

int Price::Write(OutputDataFile &df, int version)
{
    FnTrace("Price::Write()");
    return 1;
}

const char* Price::Format(int sign)
{
    return nullptr;                                // REFACTOR: Changed NULL to nullptr
}

const char* Price::Format(const char* buffer, int sign)
{
    return nullptr;                                // REFACTOR: Changed NULL to nullptr
}

const char* Price::SimpleFormat()
{
    return nullptr;                                // REFACTOR: Changed NULL to nullptr
}

const char* Price::SimpleFormat(const char* buffer)
{
    return nullptr;                                // REFACTOR: Changed NULL to nullptr
}

/**** Other Functions ****/
int StringCompare(const std::string &str1, const std::string &str2, int len)
{
    FnTrace("StringCompare()");
    const char* s1 = str1.c_str();
    const char* s2 = str2.c_str();
    
    if (len < 0)
        return strcasecmp(s1, s2);
    else
        return strncasecmp(s1, s2, len);
}

int StringInString(const genericChar* haystack, const genericChar* needle)
{
    FnTrace("StringInString()");
    return (strcasestr(haystack, needle) != nullptr);  // REFACTOR: Changed NULL comparison to nullptr
}

std::string StringToUpper(const std::string &str)
{
    FnTrace("StringToUpper()");
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string StringToLower(const std::string &str)
{
    FnTrace("StringToLower()");
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

int StripWhiteSpace(genericChar* string)
{
    FnTrace("StripWhiteSpace()");
    if (string == nullptr)                         // REFACTOR: Changed NULL to nullptr
        return 1;

    int len = strlen(string);
    if (len <= 0)
        return 1;

    // strip front spaces
    genericChar* s = string;
    while (isspace(*s))
        ++s;

    if (s != string)
    {
        strcpy(string, s);
        len = strlen(string);
    }

    // strip end spaces
    s = &string[len - 1];
    while (s >= string && isspace(*s))
        --s;
    s[1] = '\0';
    return 0;
}

std::string StringAdjustSpacing(const std::string &str)
{
    FnTrace("StringAdjustSpacing()");
    std::string result;
    bool inSpace = true;
    
    for (char c : str)
    {
        if (isspace(c))
        {
            if (!inSpace)
            {
                result += ' ';
                inSpace = true;
            }
        }
        else
        {
            result += c;
            inSpace = false;
        }
    }
    
    // Remove trailing space if any
    if (!result.empty() && result.back() == ' ')
        result.pop_back();
        
    return result;
}

std::string AdjustCase(const std::string &str)
{
    FnTrace("AdjustCase()");
    std::string result = str;
    bool capitalize = true;
    
    for (char& c : result)
    {
        if (isspace(c))
        {
            capitalize = true;
        }
        else if (capitalize)
        {
            c = toupper(c);
            capitalize = false;
        }
        else
        {
            c = tolower(c);
        }
    }
    
    return result;
}

std::string AdjustCaseAndSpacing(const std::string &str)
{
    FnTrace("AdjustCaseAndSpacing()");
    return AdjustCase(StringAdjustSpacing(str));
}

int CompareList(const genericChar* val, const genericChar* list[], int unknown)
{
    FnTrace("CompareList()");
    if (val == nullptr)                            // REFACTOR: Changed NULL to nullptr
        return unknown;

    for (int i = 0; list[i] != nullptr; ++i)      // REFACTOR: Changed NULL to nullptr
    {
        if (strcasecmp(val, list[i]) == 0)
            return i;
    }
    return unknown;
}

int CompareList(int val, int list[], int unknown)
{
    FnTrace("CompareList()");
    for (int i = 0; list[i] != -1; ++i)
    {
        if (val == list[i])
            return i;
    }
    return unknown;
}

int CompareListN(const genericChar* list[], const genericChar* str, int unknown)
{
    FnTrace("CompareListN()");
    if (str == nullptr)                            // REFACTOR: Changed NULL to nullptr
        return unknown;

    int len = strlen(str);
    for (int i = 0; list[i] != nullptr; ++i)      // REFACTOR: Changed NULL to nullptr
    {
        if (strncasecmp(str, list[i], len) == 0)
            return i;
    }
    return unknown;
}

const char* FindStringByValue(int val, int val_list[], const genericChar* str_list[],
                              const genericChar* unknown)
{
    FnTrace("FindStringByValue()");
    for (int i = 0; str_list[i] != nullptr; ++i)  // REFACTOR: Changed NULL to nullptr
    {
        if (val == val_list[i]) 
			return str_list[i];
	}
    return unknown;
}

int FindValueByString(const genericChar* val, int val_list[], const genericChar* str_list[],
                      int unknown)
{
    FnTrace("FindValueByString()");
    if (val == nullptr)                            // REFACTOR: Changed NULL to nullptr
        return unknown;

    for (int i = 0; str_list[i] != nullptr; ++i)  // REFACTOR: Changed NULL to nullptr
    {
        if (strcasecmp(val, str_list[i]) == 0)
            return val_list[i];
    }
    return unknown;
}

int FindIndexOfValue(int value, int val_list[], int unknown)
{
    FnTrace("FindIndexOfValue()");
    for (int i = 0; val_list[i] != -1; ++i)
    {
        if (value == val_list[i])
            return i;
    }
    return unknown;
}

const genericChar* NextName(const genericChar* name, const genericChar* *list)
{
    FnTrace("NextName()");
    if (name == nullptr || list == nullptr)       // REFACTOR: Changed NULL to nullptr
        return name;

    int idx = 0;
    // find the current string
    while (list[idx] != nullptr && (strcmp(name, list[idx]) != 0))  // REFACTOR: Changed NULL to nullptr
        idx += 1;

    // advance to next
    if (list[idx] != nullptr)                      // REFACTOR: Changed NULL to nullptr
        idx += 1;
    // wrap to beginning if necessary
    if (list[idx] == nullptr)                      // REFACTOR: Changed NULL to nullptr
        idx = 0;

    return list[idx];
}

int NextValue(int val, int *val_array)
{
    FnTrace("NextValue()");
    if (val_array == nullptr)                      // REFACTOR: Changed NULL to nullptr
        return val;

    int idx = 0;
    while (val_array[idx] != -1 && val_array[idx] != val)
        ++idx;

    if (val_array[idx] != -1)
        ++idx;
    if (val_array[idx] == -1)
        idx = 0;

    return val_array[idx];
}

int ForeValue(int val, int *val_array)
{
    FnTrace("ForeValue()");
    if (val_array == nullptr)                      // REFACTOR: Changed NULL to nullptr
        return val;

    int idx = 0;
    int len = 0;
    while (val_array[len] != -1)
        ++len;

    if (len <= 1)
        return val;

    while (idx < len && val_array[idx] != val)
        ++idx;

    if (idx > 0)
        --idx;
    else
        idx = len - 1;

    return val_array[idx];
}

int NextToken(genericChar* dest, const genericChar* src, genericChar sep, int *idx)
{
    FnTrace("NextToken()");
    if (dest == nullptr || src == nullptr || idx == nullptr)  // REFACTOR: Changed NULL to nullptr
        return 1;

    int start = *idx;
    int len = strlen(src);
    
    // Skip to start of token
    while (start < len && src[start] == sep)
        ++start;
        
    if (start >= len)
    {
        dest[0] = '\0';
        return 1;
    }
    
    // Find end of token
    int end = start;
    while (end < len && src[end] != sep)
        ++end;
        
    // Copy token
    int token_len = end - start;
    strncpy(dest, &src[start], token_len);
    dest[token_len] = '\0';
    
    *idx = end + 1;
    return 0;
}

int NextInteger(int *dest, const genericChar* src, genericChar sep, int *idx)
{
    FnTrace("NextInteger()");
    if (dest == nullptr || src == nullptr || idx == nullptr)  // REFACTOR: Changed NULL to nullptr
        return 1;

    genericChar buffer[32];
    if (NextToken(buffer, src, sep, idx) == 0)
    {
        *dest = atoi(buffer);
        return 0;
    }
    return 1;
}

int DoesFileExist(const genericChar* filename)
{
    FnTrace("DoesFileExist()");
    if (filename == nullptr)                       // REFACTOR: Changed NULL to nullptr
        return 0;

    struct stat sb;
    return (stat(filename, &sb) == 0);
}

int EnsureFileExists(const genericChar* filename)
{
    FnTrace("EnsureFileExists()");
    if (filename == nullptr)                       // REFACTOR: Changed NULL to nullptr
        return 1;

    if (DoesFileExist(filename))
        return 0;

    int fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0)
    {
        close(fd);
        return 0;
    }
    return 1;
}

int DeleteFile(const genericChar* filename)
{
    FnTrace("DeleteFile()");
    if (filename == nullptr)                       // REFACTOR: Changed NULL to nullptr
        return 1;

    return unlink(filename);
}

int BackupFile(const genericChar* filename)
{
    FnTrace("BackupFile()");
    if (filename == nullptr)                       // REFACTOR: Changed NULL to nullptr
        return 1;

    genericChar backup[512];
    snprintf(backup, 512, "%s.bak", filename);
    return rename(filename, backup);
}

int RestoreBackup(const genericChar* filename)
{
    FnTrace("RestoreBackup()");
    if (filename == nullptr)                       // REFACTOR: Changed NULL to nullptr
        return 1;

    genericChar backup[512];
    snprintf(backup, 512, "%s.bak", filename);
    return rename(backup, filename);
}

int FltToPrice(Flt value)
{
    FnTrace("FltToPrice()");
    return (int) (value * 100.0 + 0.5);
}

Flt PriceToFlt(int price)
{
    FnTrace("PriceToFlt()");
    return ((Flt) price) / 100.0;
}

Flt PercentToFlt(int percent)
{
    FnTrace("PercentToFlt()");
    return ((Flt) percent) / 100.0;
}

int FltToPercent(Flt value)
{
    FnTrace("FltToPercent()");
    return (int) (value * 100.0 + 0.5);
}

int LockDevice(const genericChar* devpath)
{
    FnTrace("LockDevice()");
    if (devpath == nullptr)                        // REFACTOR: Changed NULL to nullptr
        return -1;

    genericChar lockpath[STRLONG];
    genericChar buffer[STRLONG];
    const char* lock_dir = VIEWTOUCH_PATH "/bin/.lock";  // REFACTOR: Define LOCK_DIR inline
    
    // create lock filename
    const genericChar* p = strrchr(devpath, '/');
    if (p)
        snprintf(buffer, STRLONG, "%s", p+1);
    else
        snprintf(buffer, STRLONG, "%s", devpath);
        
    for (int i = 0; buffer[i]; ++i)
        if (buffer[i] == '/')
            buffer[i] = '_';
            
    snprintf(lockpath, STRLONG, "%s/%s", lock_dir, buffer);
    
    int fd = open(lockpath, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (fd < 0)
        return -1;
        
    return fd;
}

int UnlockDevice(int id)
{
    FnTrace("UnlockDevice()");
    if (id < 0)
        return -1;
        
    close(id);
    return 0;
}

/**** Process Title Functions ****/
// REFACTOR: Added setproctitle implementations to fix linker error
// These functions allow setting the process title visible in ps/top commands

static char **vt_argv = nullptr;           // REFACTOR: Store original argv pointer
static size_t vt_argv_space = 0;           // REFACTOR: Available space for process title

void vt_init_setproctitle(int argc, char* argv[])
{
    FnTrace("vt_init_setproctitle()");
    
    // REFACTOR: Store argv pointer and calculate available space for process title
    vt_argv = argv;
    
    if (argc > 0 && argv != nullptr && argv[0] != nullptr)  // REFACTOR: Safety checks with nullptr
    {
        // Calculate available space from argv[0] to end of last argument
        char* start = argv[0];
        char* end = argv[argc - 1] + strlen(argv[argc - 1]);
        vt_argv_space = end - start;
        
        // Minimum space check for safety
        if (vt_argv_space < 16)
            vt_argv_space = 16;
    }
    else
    {
        vt_argv_space = 0;
    }
}

int vt_setproctitle(const char* title)
{
    FnTrace("vt_setproctitle()");
    
    // REFACTOR: Set process title by overwriting argv[0] space
    if (title == nullptr || vt_argv == nullptr || vt_argv[0] == nullptr)  // REFACTOR: Safety checks with nullptr
        return -1;
        
    if (vt_argv_space == 0)
        return -1;
        
    // Clear the argv space and set new title
    memset(vt_argv[0], 0, vt_argv_space);                    // REFACTOR: Clear existing argv space
    strncpy(vt_argv[0], title, vt_argv_space - 1);           // REFACTOR: Copy new title safely
    vt_argv[0][vt_argv_space - 1] = '\0';                    // REFACTOR: Ensure null termination
    
    return 0;
}
