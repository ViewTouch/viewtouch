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
 * vt_logger.cc - Modern logging implementation using spdlog
 */

#include "vt_logger.hh"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/sinks/basic_file_sink.h> // For structured JSON logs
#include <filesystem>
#include <vector>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstdarg>

namespace vt {

// Static member initialization
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
std::shared_ptr<spdlog::logger> Logger::structured_logger_ = nullptr;
bool Logger::initialized_ = false;
thread_local std::optional<BusinessContext> Logger::current_business_context_;

// Performance monitoring data
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> active_timers;
static std::mutex performance_mutex;
static std::unordered_map<std::string, double> performance_metrics;
static std::unordered_map<std::string, int64_t> performance_counters;

void Logger::Initialize(
    std::string_view log_dir,
    std::string_view log_level,
    bool enable_console,
    bool enable_syslog
) {
    if (initialized_) {
        return; // Already initialized
    }

    try {
        // Create log directory if it doesn't exist
        std::filesystem::path log_path(log_dir);
        if (!std::filesystem::exists(log_path)) {
            std::filesystem::create_directories(log_path);
        }

        // Create multiple sinks for different outputs
        std::vector<spdlog::sink_ptr> sinks;

        std::string logdir_str(log_dir);
        bool test_logs = (logdir_str.find("viewtouch_test_logs") != std::string::npos);

        // 1. File sink
        spdlog::sink_ptr file_sink;
        if (test_logs) {
            // Use basic file sink in tests for simplicity
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                logdir_str + "/viewtouch.log", true /* truncate */);
        } else {
            // Rotating file sink (10MB files, max 5 files)
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logdir_str + "/viewtouch.log",
                1024 * 1024 * 10, // 10MB per file
                5                  // Keep 5 files max
            );
        }
        file_sink->set_level(spdlog::level::trace); // Capture everything in file
        sinks.push_back(file_sink);

        // 2. Structured JSON log file for analysis
        auto json_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logdir_str + "/viewtouch_structured.log", false /* don't truncate - append instead */
        );
        json_sink->set_level(spdlog::level::info);
        // JSON pattern for structured logging - just output the raw message
        json_sink->set_pattern("%v");
        sinks.push_back(json_sink);

        // 3. Console sink (colored output)
        if (enable_console) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef DEBUG
            console_sink->set_level(spdlog::level::debug);
#else
            console_sink->set_level(spdlog::level::info);
#endif
            sinks.push_back(console_sink);
        }

        // 4. Syslog sink (for compatibility with existing deployments)
        if (enable_syslog) {
            try {
                auto syslog_sink = std::make_shared<spdlog::sinks::syslog_sink_mt>(
                    "ViewTouch",
                    LOG_PID,
                    LOG_USER,
                    false // Don't use syslog for formatting
                );
                syslog_sink->set_level(spdlog::level::info);
                sinks.push_back(syslog_sink);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not initialize syslog sink: " << e.what() << '\n';
            }
        }

        // Create loggers: use synchronous mode for test log directory to avoid async timing issues
        bool use_async = (test_logs == false);
        
        // Main logger (all sinks except JSON)
        std::vector<spdlog::sink_ptr> main_sinks;
        for (auto& sink : sinks) {
            if (sink != json_sink) {
                main_sinks.push_back(sink);
            }
        }
        
        if (use_async) {
            spdlog::init_thread_pool(8192, 1);
            logger_ = std::make_shared<spdlog::async_logger>(
                "ViewTouch",
                main_sinks.begin(),
                main_sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            logger_ = std::make_shared<spdlog::logger>("ViewTouch", main_sinks.begin(), main_sinks.end());
        }
        
        // Structured logger (JSON sink only) - always synchronous for tests, async for production
        if (use_async) {
            structured_logger_ = std::make_shared<spdlog::async_logger>(
                "ViewTouch_Structured",
                json_sink,
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            structured_logger_ = std::make_shared<spdlog::logger>("ViewTouch_Structured", json_sink);
        }
        structured_logger_->set_level(spdlog::level::info);
        structured_logger_->flush_on(spdlog::level::info); // Flush immediately for test readers

        // Set log pattern: [2025-01-20 14:30:45.123] [info] [pid:12345] Message
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [pid:%P] %v");

        // Set log level
        if (log_level == "trace") {
            logger_->set_level(spdlog::level::trace);
        } else if (log_level == "debug") {
            logger_->set_level(spdlog::level::debug);
        } else if (log_level == "info") {
            logger_->set_level(spdlog::level::info);
        } else if (log_level == "warn") {
            logger_->set_level(spdlog::level::warn);
        } else if (log_level == "error") {
            logger_->set_level(spdlog::level::err);
        } else if (log_level == "critical") {
            logger_->set_level(spdlog::level::critical);
        } else {
            logger_->set_level(spdlog::level::info); // Default
        }

        // Register as default logger
        spdlog::set_default_logger(logger_);

        // Flush on info or higher so structured JSON messages are immediately
        // available to external readers (tests read files immediately).
        logger_->flush_on(spdlog::level::info);

        initialized_ = true;

        logger_->info("ViewTouch logging system initialized");
        logger_->info("Log directory: {}", log_dir);
        logger_->info("Log level: {}", log_level);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging system: " << e.what() << '\n';
        // Fall back to basic console logger
        logger_ = spdlog::stdout_color_mt("ViewTouch");
        initialized_ = true;
    }
}

void Logger::Shutdown() {
    if (logger_) {
        logger_->info("Shutting down logging system");
        logger_->flush();
        if (structured_logger_) {
            structured_logger_->flush();
        }
        // Drop all loggers to release file handles
        spdlog::drop_all();
        logger_.reset();
        structured_logger_.reset();
    }
    initialized_ = false;
}

void Logger::SetLevel(std::string_view level) {
    if (!logger_) return;

    if (level == "trace") {
        logger_->set_level(spdlog::level::trace);
    } else if (level == "debug") {
        logger_->set_level(spdlog::level::debug);
    } else if (level == "info") {
        logger_->set_level(spdlog::level::info);
    } else if (level == "warn") {
        logger_->set_level(spdlog::level::warn);
    } else if (level == "error") {
        logger_->set_level(spdlog::level::err);
    } else if (level == "critical") {
        logger_->set_level(spdlog::level::critical);
    }
}

void Logger::Flush() {
    if (logger_) {
        logger_->flush();
        for (auto &s : logger_->sinks()) {
            s->flush();
        }
    }
    if (structured_logger_) {
        structured_logger_->flush();
        for (auto &s : structured_logger_->sinks()) {
            s->flush();
        }
    }
}

std::shared_ptr<spdlog::logger> Logger::GetLogger() {
    if (!initialized_) {
        // Auto-initialize with defaults
        Initialize();
    }
    return logger_;
}

// BusinessContext implementation
nlohmann::json BusinessContext::to_json() const {
    nlohmann::json j;
    if (user_id) j["user_id"] = *user_id;
    if (employee_id) j["employee_id"] = *employee_id;
    if (check_id) j["check_id"] = *check_id;
    if (table_number) j["table_number"] = *table_number;
    if (session_id) j["session_id"] = *session_id;
    if (terminal_id) j["terminal_id"] = *terminal_id;
    if (start_time) {
        j["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            start_time->time_since_epoch()).count();
    }
    return j;
}

// LogEvent implementation
LogEvent::LogEvent(std::string_view type, std::string_view msg, spdlog::level::level_enum lvl)
    : event_type(type), message(msg), level(lvl), timestamp(std::chrono::system_clock::now()) {}

LogEvent& LogEvent::add(std::string_view key, std::string_view value) {
    metadata[std::string(key)] = std::string(value);
    return *this;
}

LogEvent& LogEvent::add(std::string_view key, const char* value) {
    if (value)
        metadata[std::string(key)] = std::string(value);
    else
        metadata[std::string(key)] = std::string("");
    return *this;
}

LogEvent& LogEvent::add(std::string_view key, int value) {
    metadata[std::string(key)] = value;
    return *this;
}

LogEvent& LogEvent::add(std::string_view key, double value) {
    metadata[std::string(key)] = value;
    return *this;
}

LogEvent& LogEvent::add(std::string_view key, bool value) {
    metadata[std::string(key)] = value;
    return *this;
}

LogEvent& LogEvent::with_context(const BusinessContext& ctx) {
    business_context = ctx;
    return *this;
}

nlohmann::json LogEvent::to_json() const {
    nlohmann::json j;
    j["event_type"] = event_type;
    j["message"] = message;
    {
        auto sv = spdlog::level::to_string_view(level);
        j["level"] = std::string(sv.data(), sv.size());
    }
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    if (!metadata.empty()) {
        j["metadata"] = nlohmann::json::object();
        for (const auto& [key, value] : metadata) {
            std::visit([&](const auto& v) { j["metadata"][key] = v; }, value);
        }
    }

    if (business_context) {
        j["business_context"] = business_context->to_json();
    }

    return j;
}

// Structured logging implementation
void Logger::log_event(const LogEvent& event) {
    auto logger = GetLogger();
    if (!logger) return;

    // Log structured JSON to dedicated JSON logger
    if (structured_logger_) {
        structured_logger_->log(event.level, "{}", event.to_json().dump());
        structured_logger_->flush();
    }

    // Also log human-readable version to regular logs
    std::string readable_msg = event.event_type;
    if (!event.message.empty()) {
        readable_msg += ": " + event.message;
    }

    // Add metadata to readable message
    if (!event.metadata.empty()) {
        readable_msg += " [";
        bool first = true;
        for (const auto& [key, value] : event.metadata) {
            if (!first) readable_msg += ", ";
            readable_msg += key + "=";
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    readable_msg += v;
                } else {
                    readable_msg += std::to_string(v);
                }
            }, value);
            first = false;
        }
        readable_msg += "]";
    }

    // Add business context
    if (event.business_context) {
        if (event.business_context->check_id) {
            readable_msg += " (Check #" + std::to_string(*event.business_context->check_id) + ")";
        }
        if (event.business_context->table_number) {
            readable_msg += " (Table " + std::to_string(*event.business_context->table_number) + ")";
        }
    }

    logger->log(event.level, "{}", readable_msg);
}

// Business context management
void Logger::set_business_context(const BusinessContext& context) {
    current_business_context_ = context;
}

void Logger::clear_business_context() {
    current_business_context_.reset();
}

std::optional<BusinessContext> Logger::get_business_context() {
    return current_business_context_;
}

// User session tracking
void Logger::start_user_session(int user_id, std::string_view session_id) {
    BusinessContext ctx;
    ctx.user_id = user_id;
    ctx.start_time = std::chrono::system_clock::now();

    if (!session_id.empty()) {
        ctx.session_id = std::string(session_id);
    } else {
        // Generate a session ID
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        ctx.session_id = "session_" + std::to_string(user_id) + "_" + std::to_string(dis(gen));
    }

    set_business_context(ctx);

    LogEvent event("user_session_started", "User session started");
    event.add("user_id", user_id);
    event.add("session_id", *ctx.session_id);
    log_event(event);
}

void Logger::end_user_session() {
    if (current_business_context_) {
        LogEvent event("user_session_ended", "User session ended");
        if (current_business_context_->user_id) {
            event.add("user_id", *current_business_context_->user_id);
        }
        if (current_business_context_->session_id) {
            event.add("session_id", *current_business_context_->session_id);
        }

        // Calculate session duration
        if (current_business_context_->start_time) {
            auto duration = std::chrono::system_clock::now() - *current_business_context_->start_time;
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
            event.add("duration_minutes", static_cast<int>(minutes));
        }

        log_event(event);
    }

    clear_business_context();
}

void Logger::update_session_context(int check_id, int table_number, int employee_id) {
    if (current_business_context_) {
        if (check_id > 0) current_business_context_->check_id = check_id;
        if (table_number > 0) current_business_context_->table_number = table_number;
        if (employee_id > 0) current_business_context_->employee_id = employee_id;
    }
}

// Performance monitoring implementation
void PerformanceMonitor::start_timer(std::string_view operation) {
    std::lock_guard<std::mutex> lock(performance_mutex);
    active_timers[std::string(operation)] = std::chrono::steady_clock::now();
}

void PerformanceMonitor::end_timer(std::string_view operation) {
    std::lock_guard<std::mutex> lock(performance_mutex);
    auto it = active_timers.find(std::string(operation));
    if (it != active_timers.end()) {
        auto duration = std::chrono::steady_clock::now() - it->second;
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

        vt::Logger::performance_event(operation, std::chrono::microseconds(microseconds));

        active_timers.erase(it);
    }
}

void PerformanceMonitor::record_metric(std::string_view name, double value) {
    std::lock_guard<std::mutex> lock(performance_mutex);
    performance_metrics[std::string(name)] = value;
}

void PerformanceMonitor::record_counter(std::string_view name, int increment) {
    std::lock_guard<std::mutex> lock(performance_mutex);
    performance_counters[std::string(name)] += increment;
}

// Performance event logging
void Logger::performance_event(std::string_view operation, std::chrono::microseconds duration,
                              const std::unordered_map<std::string, std::string>& metadata) {
    LogEvent event("performance", std::string(operation) + " completed");
    event.level = spdlog::level::debug;
    event.add("operation", std::string(operation));
    event.add("duration_us", static_cast<int>(duration.count()));
    event.add("duration_ms", static_cast<double>(duration.count()) / 1000.0);

    for (const auto& [key, value] : metadata) {
        event.add(key, value);
    }

    log_event(event);
}

// Legacy compatibility - bridge to old error handler
void Logger::log_legacy_error(int priority, const char* fmt, ...) {
    if (!fmt) return;

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    spdlog::level::level_enum level = spdlog::level::info;
    switch (priority) {
        case LOG_DEBUG: level = spdlog::level::debug; break;
        case LOG_INFO: level = spdlog::level::info; break;
        case LOG_WARNING: case LOG_NOTICE: level = spdlog::level::warn; break;
        case LOG_ERR: level = spdlog::level::err; break;
        case LOG_CRIT: case LOG_ALERT: case LOG_EMERG: level = spdlog::level::critical; break;
    }

    GetLogger()->log(level, "{}", buffer);
}

} // namespace vt

