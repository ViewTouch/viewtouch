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
 * fntrace.cc - revision 124 (10/6/98)
 * Implementation of fntrace module
 */
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
#include <sys/resource.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG
// BackTrace
TraceEntry BT_Stack[STRLENGTH];
std::atomic<int> BT_Depth(0);
std::atomic<int> BT_Track(0);
std::mutex BT_Mutex;

size_t BackTraceFunction::get_current_memory_usage() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024; // Convert to bytes
    }
    return 0;
}

// depth == 0 means print the current function on line 55
void FnPrintTrace(bool include_timing, bool include_memory)
{
    std::lock_guard<std::mutex> lock(BT_Mutex);
    fprintf(stdout, "Stack Trace (%d):\n", BT_Depth.load());
    
    for (int i = 0; i < BT_Depth; ++i) {
        const auto& entry = BT_Stack[i];
        fprintf(stdout, "    (%d) %s (%s:%d)", i + 1, entry.function, entry.file, entry.line);
        
        if (include_timing) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - entry.timestamp).count();
            fprintf(stdout, " [%ld ms]", duration);
        }
        
        if (include_memory) {
            fprintf(stdout, " [%zu bytes]", entry.memory_usage);
        }
        
        fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
}

void FnPrintLast(int depth, bool include_timing, bool include_memory)
{
    std::lock_guard<std::mutex> lock(BT_Mutex);
    depth = BT_Depth - depth - 1;
    if (depth < 0)
        depth = 0;
        
    fprintf(stderr, "Stack Trace (%d of %d):\n", BT_Depth.load() - depth, BT_Depth.load());
    
    for (int i = depth; i < BT_Depth; ++i) {
        const auto& entry = BT_Stack[i];
        fprintf(stderr, "    (%d) %s (%s:%d)", i + 1, entry.function, entry.file, entry.line);
        
        if (include_timing) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - entry.timestamp).count();
            fprintf(stderr, " [%ld ms]", duration);
        }
        
        if (include_memory) {
            fprintf(stderr, " [%zu bytes]", entry.memory_usage);
        }
        
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}

const char* FnReturnLast()
{
    std::lock_guard<std::mutex> lock(BT_Mutex);
    static char last[STRLENGTH];

    last[0] = '\0';
    if (BT_Depth == 1)
        strcpy(last, BT_Stack[0].function);
    else if (BT_Depth > 1)
        strcpy(last, BT_Stack[BT_Depth - 2].function);

    return last;
}

int debug_mode = 1;
#else
int debug_mode = 0;
#endif
