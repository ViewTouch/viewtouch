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
 * vt_logger.hh - Modern logging wrapper using spdlog
 * Provides a unified logging interface for ViewTouch with async performance
 */

#ifndef VT_LOGGER_HH
#define VT_LOGGER_HH

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <memory>
#include <string>
#include <string_view>

namespace vt {

/**
 * @brief Modern logging facade for ViewTouch
 * 
 * Provides high-performance, async logging with multiple sinks:
 * - Rotating file logs (automatic cleanup)
 * - Console output (colored)
 * - Syslog integration (for existing workflows)
 * 
 * Usage:
 *   vt::Logger::info("Starting ViewTouch v{}", version);
 *   vt::Logger::error("Printer error: {}", error_msg);
 *   vt::Logger::debug("Check #{} total: ${:.2f}", check_id, total);
 */
class Logger {
public:
    /**
     * @brief Initialize the logging system
     * @param log_dir Directory for log files (default: /var/log/viewtouch)
     * @param log_level Minimum log level (trace, debug, info, warn, error, critical)
     * @param enable_console Enable console output (default: true in debug builds)
     * @param enable_syslog Enable syslog output (default: true for compatibility)
     */
    static void Initialize(
        std::string_view log_dir = "/var/log/viewtouch",
        std::string_view log_level = "info",
        bool enable_console = true,
        bool enable_syslog = true
    );

    /**
     * @brief Shutdown the logging system (flushes all buffers)
     */
    static void Shutdown();

    /**
     * @brief Set the minimum log level at runtime
     */
    static void SetLevel(std::string_view level);

    /**
     * @brief Flush all log buffers immediately
     */
    static void Flush();

    // Logging methods with format string support
    template<typename... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        GetLogger()->critical(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Get the underlying spdlog logger for advanced use
     */
    static std::shared_ptr<spdlog::logger> GetLogger();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static bool initialized_;
};

} // namespace vt

// Convenience macros for compatibility with existing code
#define VT_LOG_TRACE(...)    vt::Logger::trace(__VA_ARGS__)
#define VT_LOG_DEBUG(...)    vt::Logger::debug(__VA_ARGS__)
#define VT_LOG_INFO(...)     vt::Logger::info(__VA_ARGS__)
#define VT_LOG_WARN(...)     vt::Logger::warn(__VA_ARGS__)
#define VT_LOG_ERROR(...)    vt::Logger::error(__VA_ARGS__)
#define VT_LOG_CRITICAL(...) vt::Logger::critical(__VA_ARGS__)

#endif // VT_LOGGER_HH

