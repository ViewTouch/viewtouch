/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
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
 * crash_report.hh - Automatic crash report generation
 * Generates GDB-like crash reports when the application crashes
 */

#ifndef VT_CRASH_REPORT_HH
#define VT_CRASH_REPORT_HH

#include <string>
#include <csignal>
#include <cstddef>

namespace vt_crash {

    /**
     * Initialize crash reporting system
     * Sets up signal handlers for fatal signals and configures crash report directory
     * @param crash_report_dir Directory where crash reports will be saved (default: /usr/viewtouch/dat/crashreports)
     */
    void InitializeCrashReporting(const std::string& crash_report_dir = "/usr/viewtouch/dat/crashreports");

    /**
     * Generate a crash report for the given signal
     * This function is called automatically by signal handlers
     * @param signal_num The signal number that caused the crash
     * @param crash_report_dir Directory where crash report will be saved
     * @param siginfo Optional signal info structure with detailed crash context (si_code, si_addr, etc.)
     * @return Path to the generated crash report file, or empty string on failure
     */
    std::string GenerateCrashReport(int signal_num, const std::string& crash_report_dir = "/usr/viewtouch/dat/crashreports", void* siginfo_ptr = nullptr);

    /**
     * Get signal name from signal number
     * @param signal_num Signal number
     * @return Human-readable signal name
     */
    std::string GetSignalName(int signal_num);

    /**
     * Get system information (OS, memory, etc.)
     * @return String containing system information
     */
    std::string GetSystemInfo();

    /**
     * Get stack trace using execinfo (backtrace)
     * @param max_frames Maximum number of stack frames to capture
     * @return String containing formatted stack trace
     */
    std::string GetStackTrace(int max_frames = 50);

    /**
     * Get memory usage information
     * @return String containing memory usage details
     */
    std::string GetMemoryInfo();

    /**
     * Test function to trigger a crash for testing crash reporting
     * WARNING: This will cause the application to crash!
     * @param signal_type Type of crash to trigger: "segfault", "abort", "fpe", or "null"
     */
    void TriggerTestCrash(const std::string& signal_type = "segfault");

} // namespace vt_crash

#endif // VT_CRASH_REPORT_HH

