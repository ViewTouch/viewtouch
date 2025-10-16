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
#include <filesystem>
#include <vector>
#include <iostream>

namespace vt {

// Static member initialization
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
bool Logger::initialized_ = false;

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

        // 1. Rotating file sink (10MB files, max 5 files)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            std::string(log_dir) + "/viewtouch.log",
            1024 * 1024 * 10, // 10MB per file
            5                  // Keep 5 files max
        );
        file_sink->set_level(spdlog::level::trace); // Capture everything in file
        sinks.push_back(file_sink);

        // 2. Console sink (colored output)
        if (enable_console) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef DEBUG
            console_sink->set_level(spdlog::level::debug);
#else
            console_sink->set_level(spdlog::level::info);
#endif
            sinks.push_back(console_sink);
        }

        // 3. Syslog sink (for compatibility with existing deployments)
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
                std::cerr << "Warning: Could not initialize syslog sink: " << e.what() << std::endl;
            }
        }

        // Create async logger with thread pool
        spdlog::init_thread_pool(8192, 1); // Queue size, thread count
        logger_ = std::make_shared<spdlog::async_logger>(
            "ViewTouch",
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block
        );

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

        // Flush on warning or higher
        logger_->flush_on(spdlog::level::warn);

        initialized_ = true;

        logger_->info("ViewTouch logging system initialized");
        logger_->info("Log directory: {}", log_dir);
        logger_->info("Log level: {}", log_level);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging system: " << e.what() << std::endl;
        // Fall back to basic console logger
        logger_ = spdlog::stdout_color_mt("ViewTouch");
        initialized_ = true;
    }
}

void Logger::Shutdown() {
    if (logger_) {
        logger_->info("Shutting down logging system");
        logger_->flush();
        spdlog::shutdown();
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
    }
}

std::shared_ptr<spdlog::logger> Logger::GetLogger() {
    if (!initialized_) {
        // Auto-initialize with defaults
        Initialize();
    }
    return logger_;
}

} // namespace vt

