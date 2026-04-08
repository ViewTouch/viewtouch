#include "fntrace.hh"
#include "cpp23_utils.hh"

#include <unistd.h>
#include <ctime>
#include <errno.h>
#include <string>
#include <cstring>
#include <cctype>
#include <iostream>
#include <cstdio>
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

size_t BackTraceFunction::get_current_memory_usage() noexcept {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024; // Convert to bytes
    }
    return 0;
}

void FnPrintTrace(bool include_timing, bool include_memory)
{
    std::lock_guard<std::mutex> lock(BT_Mutex);
    fprintf(stdout, "Stack Trace (%d):\n", BT_Depth.load());
    const int depth_snapshot = BT_Depth.load();
    for (int i = 0; i < depth_snapshot; ++i) {
        const auto& entry = BT_Stack[i];
        fprintf(stdout, "    (%d) %s (%s:%d)", i + 1, entry.function, entry.file, entry.line);
        
        if (include_timing) {
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - entry.timestamp).count();
            fprintf(stdout, " [%lld ms]", duration);
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
    const int current_depth = BT_Depth.load();
    depth = current_depth - depth - 1;
    if (depth < 0)
        depth = 0;
        
    fprintf(stderr, "Stack Trace (%d of %d):\n", current_depth - depth, current_depth);
    
    for (int i = depth; i < current_depth; ++i) {
        const auto& entry = BT_Stack[i];
        fprintf(stderr, "    (%d) %s (%s:%d)", i + 1, entry.function, entry.file, entry.line);
        
        if (include_timing) {
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - entry.timestamp).count();
            fprintf(stderr, " [%lld ms]", duration);
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
    const int depth_snapshot = BT_Depth.load();
    if (depth_snapshot == 1) {
        vt::cpp23::format_to_buffer(last, STRLENGTH, "{}", BT_Stack[0].function);
    } else if (depth_snapshot > 1) {
        vt::cpp23::format_to_buffer(last, STRLENGTH, "{}", BT_Stack[depth_snapshot - 2].function);
    }

    return last;
}

int debug_mode = 1;
#else
int debug_mode = 0;
#endif
