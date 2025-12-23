/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * error_handler.cc - Implementation of unified error handling framework
 */

#include "error_handler.hh"
#include "string_utils.hh"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <shared_mutex>
#include <cstddef>

namespace vt_error {

    std::unique_ptr<ErrorHandler> ErrorHandler::instance_ = nullptr;
    std::once_flag ErrorHandler::instance_flag_;

    ErrorInfo::ErrorInfo(std::string_view msg, Severity sev, Category cat,
                        std::string_view file_name, int line_num,
                        std::string_view func_name, int code,
                        std::string_view ctx)
        : message(msg), severity(sev), category(cat), file(file_name),
          line(line_num), function(func_name), 
          timestamp(std::chrono::system_clock::now()),
          error_code(code), context(ctx) {
    }

    ErrorHandler& ErrorHandler::getInstance() noexcept {
        std::call_once(instance_flag_, []() {
            instance_ = std::make_unique<ErrorHandler>();
            // Set default log file path
            instance_->log_file_path_ = "/tmp/viewtouch_errors.log";
            instance_->console_output_ = true;
            instance_->min_log_level_ = Severity::INFO;
        });
        return *instance_;
    }

    void ErrorHandler::setLogFile(std::string_view path) {
        std::unique_lock<std::shared_mutex> lock(history_mutex_);
        
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        log_file_path_ = path;
        log_file_.open(log_file_path_, std::ios::app);
        
        if (!log_file_.is_open()) {
            std::cerr << "Error: Could not open log file: " << log_file_path_ << '\n';
        }
    }

    void ErrorHandler::setConsoleOutput(bool enabled) noexcept {
        console_output_ = enabled;
    }

    void ErrorHandler::setMinLogLevel(Severity level) noexcept {
        min_log_level_ = level;
    }

    void ErrorHandler::reportError(std::string_view message, Severity severity,
                                  Category category, int error_code,
                                  std::string_view context, std::string_view file,
                                  int line, std::string_view function) {
        if (severity < min_log_level_) {
            return;
        }

        ErrorInfo error(message, severity, category, file, line, function, error_code, context);
        
        {
            std::unique_lock<std::shared_mutex> lock(history_mutex_);
            error_history_.push_back(std::move(error));
            
            // Keep only the last 10000 errors to prevent memory bloat
            if (error_history_.size() > 10000) {
                error_history_.erase(error_history_.begin(), 
                                   error_history_.begin() + 5000);
            }
        }

        logToFile(error);
        if (console_output_) {
            logToConsole(error);
        }
        notifyCallbacks(error);
    }

    void ErrorHandler::debug(std::string_view message, Category category,
                            std::string_view context, std::string_view file,
                            int line, std::string_view function) {
        reportError(message, Severity::VT_DEBUG, category, 0, context, file, line, function);
    }

    void ErrorHandler::info(std::string_view message, Category category,
                           std::string_view context, std::string_view file,
                           int line, std::string_view function) {
        reportError(message, Severity::INFO, category, 0, context, file, line, function);
    }

    void ErrorHandler::warning(std::string_view message, Category category,
                              std::string_view context, std::string_view file,
                              int line, std::string_view function) {
        reportError(message, Severity::WARNING, category, 0, context, file, line, function);
    }

    void ErrorHandler::error(std::string_view message, Category category,
                            int error_code, std::string_view context,
                            std::string_view file, int line, std::string_view function) {
        reportError(message, Severity::ERROR, category, error_code, context, file, line, function);
    }

    void ErrorHandler::critical(std::string_view message, Category category,
                               int error_code, std::string_view context,
                               std::string_view file, int line, std::string_view function) {
        reportError(message, Severity::CRITICAL, category, error_code, context, file, line, function);
    }

    std::vector<ErrorInfo> ErrorHandler::getErrorHistory(size_t max_entries) const {
        std::shared_lock<std::shared_mutex> lock(history_mutex_);
        
        size_t start_idx = error_history_.size() > max_entries ? 
                          error_history_.size() - max_entries : 0;
        
        return std::vector<ErrorInfo>(
            error_history_.begin() + static_cast<std::ptrdiff_t>(start_idx),
            error_history_.end());
    }

    std::vector<ErrorInfo> ErrorHandler::getErrorsByCategory(Category category, size_t max_entries) const {
        std::shared_lock<std::shared_mutex> lock(history_mutex_);
        
        std::vector<ErrorInfo> filtered;
        filtered.reserve(max_entries);
        
        for (auto it = error_history_.rbegin(); 
             it != error_history_.rend() && filtered.size() < max_entries; ++it) {
            if (it->category == category) {
                filtered.push_back(*it);
            }
        }
        
        std::reverse(filtered.begin(), filtered.end());
        return filtered;
    }

    std::vector<ErrorInfo> ErrorHandler::getErrorsBySeverity(Severity severity, size_t max_entries) const {
        std::shared_lock<std::shared_mutex> lock(history_mutex_);
        
        std::vector<ErrorInfo> filtered;
        filtered.reserve(max_entries);
        
        for (auto it = error_history_.rbegin(); 
             it != error_history_.rend() && filtered.size() < max_entries; ++it) {
            if (it->severity == severity) {
                filtered.push_back(*it);
            }
        }
        
        std::reverse(filtered.begin(), filtered.end());
        return filtered;
    }

    void ErrorHandler::clearErrorHistory() noexcept {
        std::unique_lock<std::shared_mutex> lock(history_mutex_);
        error_history_.clear();
    }

    void ErrorHandler::registerCallback(std::function<void(const ErrorInfo&)> callback) {
        std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);
        callbacks_.push_back(std::move(callback));
    }

    void ErrorHandler::clearCallbacks() noexcept {
        std::unique_lock<std::shared_mutex> lock(callbacks_mutex_);
        callbacks_.clear();
    }

    constexpr std::string_view ErrorHandler::severityToString(Severity severity) noexcept {
        switch (severity) {
            case Severity::VT_DEBUG: return "DEBUG";
            case Severity::INFO: return "INFO";
            case Severity::WARNING: return "WARNING";
            case Severity::ERROR: return "ERROR";
            case Severity::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    constexpr std::string_view ErrorHandler::categoryToString(Category category) noexcept {
        switch (category) {
            case Category::GENERAL: return "GENERAL";
            case Category::SYSTEM: return "SYSTEM";
            case Category::NETWORK: return "NETWORK";
            case Category::DATABASE: return "DATABASE";
            case Category::UI: return "UI";
            case Category::PRINTER: return "PRINTER";
            case Category::CREDIT_CARD: return "CREDIT_CARD";
            case Category::FILE_IO: return "FILE_IO";
            case Category::MEMORY: return "MEMORY";
            default: return "UNKNOWN";
        }
    }

    Severity ErrorHandler::stringToSeverity(std::string_view str) noexcept {
        std::string upper = vt_string::to_upper(std::string(str));
        if (upper == "DEBUG") return Severity::VT_DEBUG;
        if (upper == "INFO") return Severity::INFO;
        if (upper == "WARNING") return Severity::WARNING;
        if (upper == "ERROR") return Severity::ERROR;
        if (upper == "CRITICAL") return Severity::CRITICAL;
        return Severity::INFO; // Default
    }

    Category ErrorHandler::stringToCategory(std::string_view str) noexcept {
        std::string upper = vt_string::to_upper(std::string(str));
        if (upper == "GENERAL") return Category::GENERAL;
        if (upper == "SYSTEM") return Category::SYSTEM;
        if (upper == "NETWORK") return Category::NETWORK;
        if (upper == "DATABASE") return Category::DATABASE;
        if (upper == "UI") return Category::UI;
        if (upper == "PRINTER") return Category::PRINTER;
        if (upper == "CREDIT_CARD") return Category::CREDIT_CARD;
        if (upper == "FILE_IO") return Category::FILE_IO;
        if (upper == "MEMORY") return Category::MEMORY;
        return Category::GENERAL; // Default
    }

    void ErrorHandler::logToFile(const ErrorInfo& error) {
        if (!log_file_.is_open()) {
            log_file_.open(log_file_path_, std::ios::app);
        }
        
        if (log_file_.is_open()) {
            log_file_ << formatLogEntry(error) << '\n';
            log_file_.flush();
        }
    }

    void ErrorHandler::logToConsole(const ErrorInfo& error) {
        std::ostream& stream = (error.severity >= Severity::ERROR) ? std::cerr : std::cout;
        stream << formatLogEntry(error) << '\n';
    }

    void ErrorHandler::notifyCallbacks(const ErrorInfo& error) {
        std::shared_lock<std::shared_mutex> lock(callbacks_mutex_);
        for (const auto& callback : callbacks_) {
            try {
                callback(error);
            } catch (const std::exception& e) {
                // Log callback error but don't let it propagate
                std::cerr << "Error in error handler callback: " << e.what() << '\n';
            }
        }
    }

    std::string ErrorHandler::formatLogEntry(const ErrorInfo& error) const {
        auto time_t = std::chrono::system_clock::to_time_t(error.timestamp);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << " [" << severityToString(error.severity) << "]"
           << " [" << categoryToString(error.category) << "]";
        
        if (!error.file.empty()) {
            ss << " (" << vt_string::get_filename(error.file);
            if (error.line > 0) {
                ss << ":" << error.line;
            }
            if (!error.function.empty()) {
                ss << " " << error.function << "()";
            }
            ss << ")";
        }
        
        ss << " " << error.message;
        
        if (error.error_code != 0) {
            ss << " [code: " << error.error_code << "]";
        }
        
        if (!error.context.empty()) {
            ss << " [context: " << error.context << "]";
        }
        
        return ss.str();
    }

    // Legacy compatibility function
    int ReportError(std::string_view message) noexcept {
        ErrorHandler::getInstance().error(message, Category::GENERAL);
        return 0; // Return success for compatibility
    }

} // namespace vt_error
