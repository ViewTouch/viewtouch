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
#include <unordered_map>
#include <variant>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace vt {

// Forward declarations
struct BusinessContext;
struct LogEvent;

/**
 * @brief Business context for structured logging
 */
struct BusinessContext {
    std::optional<int> user_id;
    std::optional<int> employee_id;
    std::optional<int> check_id;
    std::optional<int> table_number;
    std::optional<std::string> session_id;
    std::optional<std::string> terminal_id;
    std::optional<std::chrono::system_clock::time_point> start_time;

    // Convert to JSON for structured logging
    [[nodiscard]] nlohmann::json to_json() const;
};

/**
 * @brief Structured log event
 */
struct LogEvent {
    std::string event_type;
    std::string message;
    spdlog::level::level_enum level;
    std::unordered_map<std::string, std::variant<std::string, int, double, bool>> metadata;
    std::optional<BusinessContext> business_context;
    std::chrono::system_clock::time_point timestamp;

    LogEvent(std::string_view type, std::string_view msg,
             spdlog::level::level_enum lvl = spdlog::level::info);

    // Add metadata
    LogEvent& add(std::string_view key, std::string_view value);
    LogEvent& add(std::string_view key, const char* value); // ensure const char* selects string, not bool
    LogEvent& add(std::string_view key, int value);
    LogEvent& add(std::string_view key, double value);
    LogEvent& add(std::string_view key, bool value);

    // Set business context
    LogEvent& with_context(const BusinessContext& ctx);

    // Convert to JSON
    nlohmann::json to_json() const;
};

/**
 * @brief Performance monitoring utilities
 */
class PerformanceMonitor {
public:
    static void start_timer(std::string_view operation);
    static void end_timer(std::string_view operation);
    static void record_metric(std::string_view name, double value);
    static void record_counter(std::string_view name, int increment = 1);
};

/**
 * @brief Enhanced logging facade for ViewTouch
 *
 * Provides high-performance, async logging with multiple sinks and structured data:
 * - Rotating file logs (automatic cleanup)
 * - Console output (colored)
 * - Syslog integration (for existing workflows)
 * - Structured logging with JSON support
 * - Business context tracking (users, transactions, sessions)
 * - Performance monitoring and metrics
 *
 * Usage:
 *   vt::Logger::info("Starting ViewTouch v{}", version);
 *   vt::Logger::error("Printer error: {}", error_msg);
 *   vt::Logger::debug("Check #{} total: ${:.2f}", check_id, total);
 *
 * Structured logging:
 *   vt::Logger::business_event("check_created", {
 *       {"check_id", check_id},
 *       {"table_number", table_num},
 *       {"employee_id", emp_id}
 *   });
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

    // Structured logging methods
    static void log_event(const LogEvent& event);

    // Business event with key-value pairs
    static void business_event(std::string_view event_type, std::initializer_list<std::pair<std::string_view, std::variant<std::string, int, double, bool>>> metadata) {
        LogEvent event(event_type, "");
        for (const auto& [key, value] : metadata) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    event.add(key, std::string_view(v));
                } else {
                    event.add(key, v);
                }
            }, value);
        }
        log_event(event);
    }

    // Business context management
    static void set_business_context(const BusinessContext& context);
    static void clear_business_context();
    static std::optional<BusinessContext> get_business_context();

    // User session tracking
    static void start_user_session(int user_id, std::string_view session_id = "");
    static void end_user_session();
    static void update_session_context(int check_id = 0, int table_number = 0, int employee_id = 0);

    // Performance monitoring integration
    static void performance_event(std::string_view operation, std::chrono::microseconds duration,
                                 const std::unordered_map<std::string, std::string>& metadata = {});

    // Legacy compatibility - bridge to old error handler
    static void log_legacy_error(int priority, const char* fmt, ...);

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::shared_ptr<spdlog::logger> structured_logger_;
    static bool initialized_;
    static thread_local std::optional<BusinessContext> current_business_context_;
};

} // namespace vt

// Convenience macros for compatibility with existing code
#define VT_LOG_TRACE(...)    vt::Logger::trace(__VA_ARGS__)
#define VT_LOG_DEBUG(...)    vt::Logger::debug(__VA_ARGS__)
#define VT_LOG_INFO(...)     vt::Logger::info(__VA_ARGS__)
#define VT_LOG_WARN(...)     vt::Logger::warn(__VA_ARGS__)
#define VT_LOG_ERROR(...)    vt::Logger::error(__VA_ARGS__)
#define VT_LOG_CRITICAL(...) vt::Logger::critical(__VA_ARGS__)

// Structured logging macros
#define VT_BUSINESS_EVENT(event_type, ...) \
    vt::Logger::business_event(event_type, ##__VA_ARGS__)

#define VT_PERFORMANCE_START(operation) \
    vt::PerformanceMonitor::start_timer(operation)

#define VT_PERFORMANCE_END(operation) \
    vt::PerformanceMonitor::end_timer(operation)

#define VT_SESSION_START(user_id, session_id) \
    vt::Logger::start_user_session(user_id, session_id)

#define VT_SESSION_END() \
    vt::Logger::end_user_session()

#define VT_SESSION_UPDATE(check_id, table_num, emp_id) \
    vt::Logger::update_session_context(check_id, table_num, emp_id)

#endif // VT_LOGGER_HH

