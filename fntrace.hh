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
 * fntrace.hh - Function tracing module
 */

#pragma once

#include "basic.hh"
#include <string>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstring>

extern int debug_mode;

#define STRSHORT    64
#define STRLENGTH   512   // constant to set the length of a string
#define STRLONG     2048  // 2K string

#ifdef DEBUG
// BackTrace Functions/Data
struct TraceEntry {
    char function[STRLONG];
    char file[STRLENGTH];
    int line;
    std::chrono::steady_clock::time_point timestamp;
    size_t memory_usage;
};

extern TraceEntry BT_Stack[STRLENGTH];
extern std::atomic<int> BT_Depth;
extern std::atomic<int> BT_Track;
extern std::mutex BT_Mutex;

class BackTraceFunction
{
public:
    // Constructor
    BackTraceFunction(const char* func, const char* file, int line)
    {
        if (BT_Track) {
            std::lock_guard<std::mutex> lock(BT_Mutex);
            auto& entry = BT_Stack[BT_Depth++];
            // Fixed: use strncpy for simple string copy instead of snprintf
            strncpy(entry.function, func, STRLONG);
            // Fixed: use strncpy for simple string copy instead of snprintf
            entry.function[STRLONG-1] = '\0';
            // Fixed: ensure null-termination
            strncpy(entry.file, file, STRLENGTH);
            entry.file[STRLENGTH-1] = '\0';
            entry.line = line;
            entry.timestamp = std::chrono::steady_clock::now();
            entry.memory_usage = get_current_memory_usage();
            
            printf("Entering %s (%s:%d)\n", func, file, line);
        }
    }
    
    // Destructor
    virtual ~BackTraceFunction()
    {
        if (BT_Track) {
            std::lock_guard<std::mutex> lock(BT_Mutex);
            --BT_Depth;
        }
    }

private:
    size_t get_current_memory_usage();
};

#define FnTrace(func) BackTraceFunction _fn_start(func, __FILE__, __LINE__)
#define FnTraceEnable(x) (BT_Track = (x))
void FnPrintTrace(bool include_timing = true, bool include_memory = true);
void FnPrintLast(int depth, bool include_timing = true, bool include_memory = true);
const char* FnReturnLast();
#define LINE() printf("%s:  Got to line %d\n", __FILE__, __LINE__)
#else
#define FnTrace(...)
#define FnTraceEnable(x)
#define FnPrintTrace(...)
#define FnPrintLast(...)
#define FnReturnLast() ""
#define LINE()
#endif


