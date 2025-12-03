/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
 * 
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
 * crash_report.cc - Implementation of automatic crash report generation
 */

#include "crash_report.hh"
#include "fntrace.hh"
#include "version/vt_version_info.hh"

// VIEWTOUCH_PATH is defined in CMakeLists.txt, use default if not available
#ifndef VIEWTOUCH_PATH
#define VIEWTOUCH_PATH "/usr/viewtouch"
#endif

// execinfo.h is a GNU extension, available on Linux and some BSD systems
#ifdef __linux__
#include <execinfo.h>
#endif
#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <memory>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ucontext.h>
#include <fcntl.h>

namespace vt_crash {

    // Global crash report directory
    static std::string g_crash_report_dir = "/usr/viewtouch/dat/crashreports";

    /**
     * Ensure crash report directory exists, creating it if necessary
     */
    static bool EnsureCrashReportDirectory(const std::string& dir) {
        struct stat st;
        if (stat(dir.c_str(), &st) == 0) {
            // Directory exists
            if (S_ISDIR(st.st_mode)) {
                return true;
            } else {
                std::cerr << "ERROR: Crash report path exists but is not a directory: " << dir << std::endl;
                return false;
            }
        }
        
        // Directory doesn't exist, create it
        // Create parent directories first
        size_t pos = dir.find_last_of('/');
        if (pos != std::string::npos && pos > 0) {
            std::string parent = dir.substr(0, pos);
            if (!EnsureCrashReportDirectory(parent)) {
                return false;
            }
        }
        
        // Create the directory with permissions 0777
        if (mkdir(dir.c_str(), 0777) == 0) {
            return true;
        } else if (errno == EEXIST) {
            // Directory was created by another process
            return true;
        } else {
            std::cerr << "ERROR: Failed to create crash report directory: " << dir 
                      << " (errno: " << errno << ")" << std::endl;
            return false;
        }
    }

    void InitializeCrashReporting(const std::string& crash_report_dir) {
        // Store crash report directory
        g_crash_report_dir = crash_report_dir;
        
        // Create crash report directory if it doesn't exist
        if (!EnsureCrashReportDirectory(crash_report_dir)) {
            std::cerr << "WARNING: Could not create crash report directory: " << crash_report_dir << std::endl;
            std::cerr << "Crash reports may not be saved to disk." << std::endl;
        } else {
            std::cerr << "Crash reporting initialized - reports will be saved to: " << crash_report_dir << std::endl;
        }
        
        // Note: We don't set up signal handlers here because they are set up
        // in manager.cc's Terminate() function. The Terminate() function will
        // call GenerateCrashReport() when a fatal signal is received.
    }

    std::string GetSignalName(int signal_num) {
        switch (signal_num) {
            case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
            case SIGABRT: return "SIGABRT (Abort)";
            case SIGBUS:  return "SIGBUS (Bus Error)";
            case SIGFPE:  return "SIGFPE (Floating Point Exception)";
            case SIGILL:  return "SIGILL (Illegal Instruction)";
            case SIGINT:  return "SIGINT (Interrupt)";
            case SIGQUIT: return "SIGQUIT (Quit)";
            case SIGTERM: return "SIGTERM (Termination)";
            case SIGPIPE: return "SIGPIPE (Broken Pipe)";
            default: {
                std::ostringstream oss;
                oss << "Unknown Signal (" << signal_num << ")";
                return oss.str();
            }
        }
    }

    /**
     * Read a single line from a file, returning the value after a colon
     */
    static std::string ReadProcFileValue(const std::string& filename, const std::string& key) {
        FILE* f = fopen(filename.c_str(), "r");
        if (!f) return "N/A";
        
        char line[512];
        std::string result = "N/A";
        while (fgets(line, sizeof(line), f)) {
            std::string line_str(line);
            size_t colon_pos = line_str.find(':');
            if (colon_pos != std::string::npos) {
                std::string file_key = line_str.substr(0, colon_pos);
                // Trim whitespace
                while (!file_key.empty() && (file_key[0] == ' ' || file_key[0] == '\t')) {
                    file_key = file_key.substr(1);
                }
                if (file_key == key) {
                    result = line_str.substr(colon_pos + 1);
                    // Trim whitespace and newlines
                    while (!result.empty() && (result[0] == ' ' || result[0] == '\t')) {
                        result = result.substr(1);
                    }
                    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                        result.pop_back();
                    }
                    break;
                }
            }
        }
        fclose(f);
        return result;
    }

    std::string GetSystemInfo() {
        std::ostringstream oss;
        
        // Get system information from uname
        struct utsname sys_info;
        if (uname(&sys_info) == 0) {
            oss << "Operating System:\n";
            oss << "  System Name: " << sys_info.sysname << "\n";
            oss << "  Release: " << sys_info.release << "\n";
            oss << "  Version: " << sys_info.version << "\n";
            oss << "  Machine/Architecture: " << sys_info.machine << "\n";
            oss << "  Node Name: " << sys_info.nodename << "\n";
        }
        
        // Get CPU information from /proc/cpuinfo
        oss << "\nCPU Information:\n";
        FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo) {
            char line[512];
            bool found_model = false;
            int cpu_count = 0;
            std::string model_name;
            std::string cpu_freq;
            
            while (fgets(line, sizeof(line), cpuinfo)) {
                std::string line_str(line);
                if (line_str.find("model name") != std::string::npos && !found_model) {
                    size_t colon = line_str.find(':');
                    if (colon != std::string::npos) {
                        model_name = line_str.substr(colon + 1);
                        // Trim whitespace
                        while (!model_name.empty() && (model_name[0] == ' ' || model_name[0] == '\t')) {
                            model_name = model_name.substr(1);
                        }
                        while (!model_name.empty() && (model_name.back() == '\n' || model_name.back() == '\r')) {
                            model_name.pop_back();
                        }
                        oss << "  Model: " << model_name << "\n";
                        found_model = true;
                    }
                } else if (line_str.find("cpu MHz") != std::string::npos || line_str.find("BogoMIPS") != std::string::npos) {
                    size_t colon = line_str.find(':');
                    if (colon != std::string::npos) {
                        std::string value = line_str.substr(colon + 1);
                        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                            value = value.substr(1);
                        }
                        while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                            value.pop_back();
                        }
                        if (line_str.find("cpu MHz") != std::string::npos) {
                            oss << "  Frequency: " << value << " MHz\n";
                        } else {
                            oss << "  BogoMIPS: " << value << "\n";
                        }
                    }
                } else if (line_str.find("processor") != std::string::npos) {
                    cpu_count++;
                } else if (line_str.find("Hardware") != std::string::npos && !found_model) {
                    size_t colon = line_str.find(':');
                    if (colon != std::string::npos) {
                        std::string hardware = line_str.substr(colon + 1);
                        while (!hardware.empty() && (hardware[0] == ' ' || hardware[0] == '\t')) {
                            hardware = hardware.substr(1);
                        }
                        while (!hardware.empty() && (hardware.back() == '\n' || hardware.back() == '\r')) {
                            hardware.pop_back();
                        }
                        oss << "  Hardware: " << hardware << "\n";
                    }
                }
            }
            fclose(cpuinfo);
            
            if (cpu_count > 0) {
                oss << "  CPU Cores: " << cpu_count << "\n";
            } else {
                // Try to get from /proc/stat or use nproc
                FILE* nproc = popen("nproc 2>/dev/null", "r");
                if (nproc) {
                    char buffer[32];
                    if (fgets(buffer, sizeof(buffer), nproc)) {
                        std::string cores(buffer);
                        while (!cores.empty() && (cores.back() == '\n' || cores.back() == '\r')) {
                            cores.pop_back();
                        }
                        oss << "  CPU Cores: " << cores << "\n";
                    }
                    pclose(nproc);
                }
            }
        } else {
            oss << "  (CPU info not available)\n";
        }
        
        // Get memory information from /proc/meminfo
        oss << "\nMemory Information:\n";
        std::string mem_total = ReadProcFileValue("/proc/meminfo", "MemTotal");
        std::string mem_free = ReadProcFileValue("/proc/meminfo", "MemAvailable");
        if (mem_free == "N/A") {
            mem_free = ReadProcFileValue("/proc/meminfo", "MemFree");
        }
        oss << "  Total Memory: " << mem_total << "\n";
        oss << "  Available Memory: " << mem_free << "\n";
        
        // Get process information
        oss << "\nProcess Information:\n";
        oss << "  Process ID: " << getpid() << "\n";
        oss << "  Parent Process ID: " << getppid() << "\n";
        
        // Get user information
        uid_t uid = getuid();
        gid_t gid = getgid();
        oss << "  User ID: " << uid << "\n";
        oss << "  Group ID: " << gid << "\n";
        
        // Get working directory
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            oss << "  Working Directory: " << cwd << "\n";
        }
        
        return oss.str();
    }

    std::string GetMemoryInfo() {
        std::ostringstream oss;
        
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            oss << "Memory Usage:\n";
            oss << "  Max RSS: " << (usage.ru_maxrss * 1024) << " bytes\n";
            oss << "  Shared Memory: " << (usage.ru_ixrss * 1024) << " bytes\n";
            oss << "  Unshared Data: " << (usage.ru_idrss * 1024) << " bytes\n";
            oss << "  Unshared Stack: " << (usage.ru_isrss * 1024) << " bytes\n";
        }
        
        return oss.str();
    }

    /**
     * Demangle C++ function names
     */
    static std::string Demangle(const char* mangled) {
        if (mangled == nullptr) {
            return "<unknown>";
        }
        
        int status = 0;
        std::unique_ptr<char, void(*)(void*)> demangled(
            abi::__cxa_demangle(mangled, nullptr, nullptr, &status),
            std::free
        );
        
        if (status == 0 && demangled) {
            return std::string(demangled.get());
        }
        
        return std::string(mangled);
    }

    /**
     * Decode address to function name and line number using addr2line
     * Returns "function_name (file:line)" or empty string on failure
     */
    static std::string DecodeAddress(void* addr, const std::string& executable_path) {
        if (addr == nullptr || executable_path.empty()) {
            return "";
        }
        
        // Check if executable exists and is readable
        if (access(executable_path.c_str(), R_OK) != 0) {
            return "";
        }
        
        // Build addr2line command - use absolute path to addr2line if possible
        std::ostringstream cmd;
        // Try to find addr2line in common locations, fall back to just "addr2line"
        std::string addr2line_cmd = "addr2line";
        if (access("/usr/bin/addr2line", X_OK) == 0) {
            addr2line_cmd = "/usr/bin/addr2line";
        } else if (access("/bin/addr2line", X_OK) == 0) {
            addr2line_cmd = "/bin/addr2line";
        }
        
        cmd << addr2line_cmd << " -e " << executable_path 
            << " -f -C -p 0x" << std::hex << reinterpret_cast<uintptr_t>(addr) << std::dec
            << " 2>/dev/null";
        
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            return "";
        }
        
        char buffer[1024];
        std::string result;
        // Read first line (function name and file:line)
        if (fgets(buffer, sizeof(buffer), pipe)) {
            result = buffer;
            // Remove trailing newline
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            // If result is "??" or empty, addr2line couldn't decode it
            if (result == "??" || result.empty() || result.find("??") == 0) {
                result.clear();
            }
        }
        
        int status = pclose(pipe);
        // If addr2line failed (non-zero exit), return empty
        if (status != 0) {
            result.clear();
        }
        
        return result;
    }

    /**
     * Get executable path from /proc/self/exe
     */
    static std::string GetExecutablePath() {
        char exe_path[4096];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            return std::string(exe_path);
        }
        return "";
    }

    /**
     * Try to get function info using dladdr
     */
    static std::string GetFunctionInfo(void* addr) {
        Dl_info info;
        if (dladdr(addr, &info) != 0 && info.dli_sname != nullptr) {
            std::string func_name = Demangle(info.dli_sname);
            if (info.dli_fname) {
                return func_name + " (" + std::string(info.dli_fname) + ")";
            }
            return func_name;
        }
        return "";
    }

    std::string GetStackTrace(int max_frames) {
        std::ostringstream oss;
        
        #ifdef __linux__
        void* buffer[max_frames];
        int num_frames = backtrace(buffer, max_frames);
        
        if (num_frames == 0) {
            oss << "No stack trace available\n";
            return oss.str();
        }
        
        // Get symbols
        char** symbols = backtrace_symbols(buffer, num_frames);
        if (symbols == nullptr) {
            oss << "Failed to get stack trace symbols\n";
            return oss.str();
        }
        
        // Get executable path for addr2line
        std::string exe_path = GetExecutablePath();
        bool use_addr2line = !exe_path.empty() && access(exe_path.c_str(), R_OK) == 0;
        
        oss << "Stack Trace (" << num_frames << " frames):\n";
        oss << "===========================================\n";
        
        for (int i = 0; i < num_frames; ++i) {
            oss << "  #" << std::setw(2) << i << " ";
            
            // Try to decode address using addr2line first (most detailed)
            std::string decoded_info;
            if (use_addr2line) {
                decoded_info = DecodeAddress(buffer[i], exe_path);
            }
            
            if (!decoded_info.empty()) {
                // addr2line gave us function name and file:line
                oss << decoded_info;
            } else {
                // Fall back to backtrace_symbols and dladdr
                std::string symbol_str = symbols[i];
                
                // Try dladdr for better function info
                std::string func_info = GetFunctionInfo(buffer[i]);
                if (!func_info.empty()) {
                    // Extract address from symbol_str and combine with function info
                    size_t addr_start = symbol_str.find('[');
                    if (addr_start != std::string::npos) {
                        std::string addr_part = symbol_str.substr(addr_start);
                        oss << func_info << " " << addr_part;
                    } else {
                        oss << func_info;
                    }
                } else {
                    // Use backtrace_symbols output, but demangle function names
                    // Format is typically: ./program(function+offset) [address]
                    size_t func_start = symbol_str.find('(');
                    size_t func_end = symbol_str.find('+', func_start);
                    
                    if (func_start != std::string::npos && func_end != std::string::npos) {
                        std::string func_name = symbol_str.substr(func_start + 1, func_end - func_start - 1);
                        std::string demangled = Demangle(func_name.c_str());
                        
                        // Replace mangled name with demangled name
                        symbol_str.replace(func_start + 1, func_end - func_start - 1, demangled);
                    }
                    
                    oss << symbol_str;
                }
            }
            
            oss << "\n";
        }
        
        free(symbols);
        #else
        oss << "Stack trace not available on this platform\n";
        oss << "(execinfo.h is a GNU/Linux extension)\n";
        #endif
        
        return oss.str();
    }

    /**
     * Get detailed signal code description
     */
    static std::string GetSignalCodeDescription(int signal_num, int si_code) {
        std::ostringstream oss;
        
        switch (signal_num) {
            case SIGSEGV:
                switch (si_code) {
                    case SEGV_MAPERR: oss << "Address not mapped to object (invalid address)"; break;
                    case SEGV_ACCERR: oss << "Invalid permissions for mapped object (access violation)"; break;
                    case SEGV_BNDERR: oss << "Failed address bound checks"; break;
                    case SEGV_PKUERR: oss << "Access was denied by memory protection keys"; break;
                    default: oss << "Unknown segmentation fault reason (code: " << si_code << ")"; break;
                }
                break;
            case SIGBUS:
                switch (si_code) {
                    case BUS_ADRALN: oss << "Invalid address alignment"; break;
                    case BUS_ADRERR: oss << "Non-existent physical address"; break;
                    case BUS_OBJERR: oss << "Object specific hardware error"; break;
                    case BUS_MCEERR_AR: oss << "Hardware memory error: action required"; break;
                    case BUS_MCEERR_AO: oss << "Hardware memory error: action optional"; break;
                    default: oss << "Unknown bus error reason (code: " << si_code << ")"; break;
                }
                break;
            case SIGFPE:
                switch (si_code) {
                    case FPE_INTDIV: oss << "Integer divide by zero"; break;
                    case FPE_INTOVF: oss << "Integer overflow"; break;
                    case FPE_FLTDIV: oss << "Floating point divide by zero"; break;
                    case FPE_FLTOVF: oss << "Floating point overflow"; break;
                    case FPE_FLTUND: oss << "Floating point underflow"; break;
                    case FPE_FLTRES: oss << "Floating point inexact result"; break;
                    case FPE_FLTINV: oss << "Invalid floating point operation"; break;
                    case FPE_FLTSUB: oss << "Subscript out of range"; break;
                    default: oss << "Unknown floating point error (code: " << si_code << ")"; break;
                }
                break;
            case SIGILL:
                switch (si_code) {
                    case ILL_ILLOPC: oss << "Illegal opcode"; break;
                    case ILL_ILLOPN: oss << "Illegal operand"; break;
                    case ILL_ILLADR: oss << "Illegal addressing mode"; break;
                    case ILL_ILLTRP: oss << "Illegal trap"; break;
                    case ILL_PRVOPC: oss << "Privileged opcode"; break;
                    case ILL_PRVREG: oss << "Privileged register"; break;
                    case ILL_COPROC: oss << "Coprocessor error"; break;
                    case ILL_BADSTK: oss << "Internal stack error"; break;
                    default: oss << "Unknown illegal instruction error (code: " << si_code << ")"; break;
                }
                break;
            default:
                oss << "Signal code: " << si_code;
                break;
        }
        
        return oss.str();
    }

    /**
     * Get memory mapping information for an address
     */
    static std::string GetMemoryMappingInfo(void* addr) {
        if (addr == nullptr) {
            return "Address is NULL";
        }
        
        std::ostringstream oss;
        FILE* maps = fopen("/proc/self/maps", "r");
        if (maps) {
            char line[512];
            uintptr_t target_addr = reinterpret_cast<uintptr_t>(addr);
            bool found = false;
            
            while (fgets(line, sizeof(line), maps)) {
                uintptr_t start, end;
                char perms[5] = "";
                char offset[17] = "";
                char dev[6] = "";
                char inode[17] = "";
                char path[256] = "";
                
                // Parse /proc/self/maps format: start-end perms offset dev inode [path]
                // We need at least 6 fields (start, end, perms, offset, dev, inode)
                // Path is optional (7th field) - anonymous mappings don't have paths
                int fields_parsed = sscanf(line, "%lx-%lx %4s %16s %5s %16s %255s", 
                                          &start, &end, perms, offset, dev, inode, path);
                
                if (fields_parsed >= 6) {
                    if (target_addr >= start && target_addr < end) {
                        oss << "  Memory Region: 0x" << std::hex << start << "-0x" << end << std::dec << "\n";
                        oss << "  Permissions: " << perms << "\n";
                        oss << "  Offset: " << offset << "\n";
                        if (fields_parsed >= 7 && path[0] != '\0') {
                            oss << "  File/Path: " << path << "\n";
                        } else {
                            oss << "  Type: Anonymous mapping\n";
                        }
                        found = true;
                        break;
                    }
                }
            }
            fclose(maps);
            
            if (!found) {
                oss << "  Address 0x" << std::hex << target_addr << std::dec 
                    << " not found in memory maps (invalid/unmapped address)\n";
            }
        } else {
            oss << "  Could not read /proc/self/maps\n";
        }
        
        return oss.str();
    }

    /**
     * Get recent error log entries
     */
    static std::string GetRecentErrorLog() {
        std::ostringstream oss;
        
        // Try to read from error_log.txt
        // We try common locations since we can't access MasterSystem here
        std::string error_log_path = VIEWTOUCH_PATH "/dat/error_log.txt";
        
        // Also try /usr/viewtouch/dat/error_log.txt as fallback
        if (access(error_log_path.c_str(), R_OK) != 0) {
            error_log_path = "/usr/viewtouch/dat/error_log.txt";
        }
        
        FILE* err_log = fopen(error_log_path.c_str(), "r");
        if (err_log) {
            // Read last 20 lines
            std::vector<std::string> lines;
            char line[512];
            
            while (fgets(line, sizeof(line), err_log)) {
                lines.push_back(line);
                if (lines.size() > 20) {
                    lines.erase(lines.begin());
                }
            }
            fclose(err_log);
            
            if (!lines.empty()) {
                oss << "Recent Error Log Entries (last " << lines.size() << "):\n";
                for (const auto& log_line : lines) {
                    oss << "  " << log_line;
                }
            } else {
                oss << "Error log is empty\n";
            }
        } else {
            oss << "Could not read error log: " << error_log_path << "\n";
        }
        
        return oss.str();
    }

    std::string GenerateCrashReport(int signal_num, const std::string& crash_report_dir, void* siginfo_ptr) {
        // Use provided directory or fall back to global default
        std::string report_dir = crash_report_dir.empty() ? g_crash_report_dir : crash_report_dir;
        
        // Ensure directory exists before trying to write
        if (!EnsureCrashReportDirectory(report_dir)) {
            std::cerr << "ERROR: Cannot create crash report directory: " << report_dir << std::endl;
            // Try to write to /tmp as fallback
            std::string fallback_dir = "/tmp";
            if (EnsureCrashReportDirectory(fallback_dir)) {
                report_dir = fallback_dir;
                std::cerr << "Using fallback directory: " << report_dir << std::endl;
            } else {
                std::cerr << "ERROR: Cannot create fallback directory either!" << std::endl;
            }
        }
        
        std::ostringstream report;
        
        // Header
        report << "========================================\n";
        report << "ViewTouch Crash Report\n";
        report << "========================================\n\n";
        
        // Timestamp
        auto now = std::time(nullptr);
        char time_str[64];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", std::localtime(&now));
        report << "Crash Time: " << time_str << "\n\n";
        
        // Signal information with detailed crash analysis
        report << "Signal Information:\n";
        report << "  Signal: " << GetSignalName(signal_num) << "\n";
        report << "  Signal Number: " << signal_num << "\n";
        
        // Parse siginfo if available
        if (siginfo_ptr != nullptr) {
            siginfo_t* si = static_cast<siginfo_t*>(siginfo_ptr);
            report << "  Signal Code: " << si->si_code << "\n";
            report << "  Crash Reason: " << GetSignalCodeDescription(signal_num, si->si_code) << "\n";
            
            // For SIGSEGV and SIGBUS, show the faulting address
            if (signal_num == SIGSEGV || signal_num == SIGBUS) {
                void* fault_addr = si->si_addr;
                report << "  Faulting Address: 0x" << std::hex << reinterpret_cast<uintptr_t>(fault_addr) << std::dec << "\n";
                
                // Get memory mapping information
                report << "\nMemory Mapping Information:\n";
                report << GetMemoryMappingInfo(fault_addr);
                
                // Additional analysis
                if (fault_addr == nullptr) {
                    report << "\n  Analysis: NULL pointer dereference - attempting to access memory at address 0x0\n";
                } else if (reinterpret_cast<uintptr_t>(fault_addr) < 0x1000) {
                    report << "\n  Analysis: Very low address (likely NULL pointer or uninitialized pointer)\n";
                } else {
                    report << "\n  Analysis: Invalid memory access - address may be:\n";
                    report << "    - Freed memory (use-after-free)\n";
                    report << "    - Uninitialized pointer\n";
                    report << "    - Out of bounds array access\n";
                    report << "    - Stack overflow\n";
                }
            }
            
            // For SIGFPE, show more details
            if (signal_num == SIGFPE) {
                report << "  Faulting Address: 0x" << std::hex << reinterpret_cast<uintptr_t>(si->si_addr) << std::dec << "\n";
            }
        } else {
            report << "  (Detailed signal context not available)\n";
        }
        report << "\n";
        
        // Recent error log entries
        report << "Recent Error Log:\n";
        report << "===========================================\n";
        report << GetRecentErrorLog() << "\n";
        
        // Crash Analysis Section
        report << "Crash Analysis:\n";
        report << "===========================================\n";
        if (signal_num == SIGSEGV) {
            report << "Segmentation Fault Analysis:\n";
            report << "  A segmentation fault typically indicates:\n";
            report << "  - NULL pointer dereference (accessing memory at address 0x0)\n";
            report << "  - Use-after-free (accessing freed memory)\n";
            report << "  - Buffer overflow (accessing memory outside allocated bounds)\n";
            report << "  - Stack overflow (exceeding stack size limits)\n";
            report << "  - Uninitialized pointer (pointer not set before use)\n";
            if (siginfo_ptr != nullptr) {
                siginfo_t* si = static_cast<siginfo_t*>(siginfo_ptr);
                void* fault_addr = si->si_addr;
                if (fault_addr == nullptr) {
                    report << "\n  Most Likely Cause: NULL pointer dereference\n";
                } else if (reinterpret_cast<uintptr_t>(fault_addr) < 0x1000) {
                    report << "\n  Most Likely Cause: NULL or uninitialized pointer (very low address)\n";
                } else {
                    report << "\n  Faulting Address: 0x" << std::hex << reinterpret_cast<uintptr_t>(fault_addr) << std::dec << "\n";
                    report << "  Check the stack trace above to identify the function causing the crash.\n";
                }
            } else {
                report << "\n  Check the stack trace above to identify the function causing the crash.\n";
                report << "  Look for functions that:\n";
                report << "    - Dereference pointers without NULL checks\n";
                report << "    - Access array elements without bounds checking\n";
                report << "    - Use freed memory\n";
            }
        } else if (signal_num == SIGABRT) {
            report << "Abort Signal Analysis:\n";
            report << "  An abort signal typically indicates:\n";
            report << "  - assert() failure (assertion failed)\n";
            report << "  - abort() called explicitly\n";
            report << "  - Memory corruption detected by runtime\n";
            report << "  - Double free or invalid free() call\n";
            report << "\n  Check the stack trace to find where abort() was called.\n";
        } else if (signal_num == SIGFPE) {
            report << "Floating Point Exception Analysis:\n";
            report << "  A floating point exception typically indicates:\n";
            report << "  - Division by zero (integer or floating point)\n";
            report << "  - Floating point overflow or underflow\n";
            report << "  - Invalid floating point operation\n";
            report << "\n  Check the stack trace to find the division or math operation causing the crash.\n";
        } else if (signal_num == SIGBUS) {
            report << "Bus Error Analysis:\n";
            report << "  A bus error typically indicates:\n";
            report << "  - Invalid memory alignment (unaligned memory access)\n";
            report << "  - Access to non-existent physical memory\n";
            report << "  - Hardware memory error\n";
            report << "\n  This often occurs with:\n";
            report << "    - Misaligned pointer arithmetic\n";
            report << "    - Corrupted memory structures\n";
        } else if (signal_num == SIGILL) {
            report << "Illegal Instruction Analysis:\n";
            report << "  An illegal instruction error typically indicates:\n";
            report << "  - Corrupted code or data\n";
            report << "  - Attempting to execute data as code\n";
            report << "  - CPU architecture mismatch\n";
            report << "  - Memory corruption affecting code section\n";
        }
        report << "\n";
        
        // System information
        report << "System Information:\n";
        report << GetSystemInfo() << "\n";
        
        // Memory information
        report << GetMemoryInfo() << "\n";
        
        // Stack trace
        report << GetStackTrace() << "\n";
        
        // Function trace (if available in DEBUG mode)
        #ifdef DEBUG
        report << "Function Trace (from FnTrace):\n";
        report << "===========================================\n";
        try {
            FnPrintTrace(true, true);
            // Note: FnPrintTrace prints to stdout, we need to capture it
            // For now, we'll just note that it's available
            report << "(Function trace available in debug output)\n\n";
        } catch (...) {
            report << "(Function trace unavailable)\n\n";
        }
        #endif
        
        // Environment variables (limited set)
        report << "Environment Variables:\n";
        report << "  PATH: " << (getenv("PATH") ? getenv("PATH") : "not set") << "\n";
        report << "  HOME: " << (getenv("HOME") ? getenv("HOME") : "not set") << "\n";
        report << "  USER: " << (getenv("USER") ? getenv("USER") : "not set") << "\n";
        report << "  DISPLAY: " << (getenv("DISPLAY") ? getenv("DISPLAY") : "not set") << "\n\n";
        
        // Build information
        report << "Build Information:\n";
        report << "  Project: " << viewtouch::get_project_name() << "\n";
        report << "  Version: " << viewtouch::get_version_short() << "\n";
        report << "  Full Version: " << viewtouch::get_version_long() << "\n";
        report << "  Build Timestamp: " << viewtouch::get_version_timestamp() << "\n";
        
        #ifdef DEBUG
        report << "  Build Type: DEBUG\n";
        #else
        report << "  Build Type: RELEASE\n";
        #endif
        
        #ifdef __GNUC__
        report << "  Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\n";
        #endif
        
        #ifdef __clang__
        report << "  Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__ << "\n";
        #endif
        
        report << "\n";
        report << "========================================\n";
        report << "End of Crash Report\n";
        report << "========================================\n";
        
        // Generate filename with timestamp
        std::ostringstream filename;
        filename << crash_report_dir << "/crash_report_";
        filename << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
        filename << "_" << getpid() << ".txt";
        
        std::string crash_file = filename.str();
        
        // Write to file
        std::ofstream out(crash_file, std::ios::out | std::ios::trunc);
        if (out.is_open()) {
            out << report.str();
            out.flush();
            out.close();
            
            // Verify file was written
            struct stat file_stat;
            if (stat(crash_file.c_str(), &file_stat) == 0 && file_stat.st_size > 0) {
                std::cerr << "Crash report successfully written to: " << crash_file << " (" << file_stat.st_size << " bytes)\n";
            } else {
                std::cerr << "WARNING: Crash report file created but appears empty: " << crash_file << "\n";
            }
            
            // Also write to stderr for immediate visibility (truncated for readability)
            std::cerr << "\n=== CRASH REPORT SUMMARY ===\n";
            std::cerr << "Signal: " << GetSignalName(signal_num) << "\n";
            std::cerr << "File: " << crash_file << "\n";
            std::cerr << "Full report saved to file above.\n";
            std::cerr << "============================\n\n";
            std::cerr.flush();
            
            return crash_file;
        } else {
            // If we can't write to the file, at least print to stderr
            std::cerr << "\n=== CRASH REPORT (NOT SAVED TO FILE) ===\n";
            std::cerr << report.str() << "\n";
            std::cerr << "ERROR: Could not write crash report to file: " << crash_file << "\n";
            std::cerr << "Error details: " << strerror(errno) << "\n";
            struct stat dir_stat;
            std::cerr << "Directory exists: " << (stat(report_dir.c_str(), &dir_stat) == 0 ? "yes" : "no") << "\n";
            std::cerr << "==========================================\n";
            std::cerr.flush();
            return "";
        }
    }

    void TriggerTestCrash(const std::string& signal_type) {
        std::cerr << "\n*** TEST CRASH TRIGGERED ***\n";
        std::cerr << "Signal type: " << signal_type << "\n";
        std::cerr << "Crash report directory: " << g_crash_report_dir << "\n";
        std::cerr << "Generating crash report...\n\n";
        std::cerr.flush();
        
        // Generate crash report first (before crashing)
        // Note: siginfo_ptr is nullptr for test crashes since we're triggering manually
        std::string crash_file = GenerateCrashReport(
            signal_type == "abort" ? SIGABRT :
            signal_type == "fpe" ? SIGFPE :
            signal_type == "bus" ? SIGBUS :
            signal_type == "ill" ? SIGILL :
            SIGSEGV,  // default to segfault
            g_crash_report_dir,
            nullptr  // No siginfo for manual test crashes
        );
        
        if (!crash_file.empty()) {
            std::cerr << "Crash report generated: " << crash_file << "\n";
        } else {
            std::cerr << "WARNING: Crash report generation returned empty path\n";
        }
        std::cerr.flush();
        
        // Small delay to ensure file is written
        usleep(100000); // 100ms
        
        // Now trigger the actual crash
        if (signal_type == "segfault" || signal_type == "null") {
            // Trigger segmentation fault by dereferencing null pointer
            // Note: This is intentional for testing crash reporting
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wnull-dereference"
            int* p = nullptr;
            *p = 42;  // This will cause SIGSEGV
            #pragma GCC diagnostic pop
        } else if (signal_type == "abort") {
            // Trigger abort
            raise(SIGABRT);
        } else if (signal_type == "fpe") {
            // Trigger floating point exception
            raise(SIGFPE);
        } else if (signal_type == "bus") {
            // Trigger bus error
            raise(SIGBUS);
        } else if (signal_type == "ill") {
            // Trigger illegal instruction
            raise(SIGILL);
        } else {
            // Default to segfault
            // Note: This is intentional for testing crash reporting
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wnull-dereference"
            int* p = nullptr;
            *p = 42;
            #pragma GCC diagnostic pop
        }
    }

} // namespace vt_crash
