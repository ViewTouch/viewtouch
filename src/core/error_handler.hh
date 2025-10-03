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
 * error_handler.hh - Unified error handling framework
 * Centralizes error reporting, logging, and recovery mechanisms
 */

#ifndef VT_ERROR_HANDLER_HH
#define VT_ERROR_HANDLER_HH

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <chrono>
#include <optional>
#include <span>
#include <cstdint>

namespace vt_error {

    enum class Severity : std::uint8_t {
        VT_DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    enum class Category : std::uint8_t {
        GENERAL = 0,
        SYSTEM = 1,
        NETWORK = 2,
        DATABASE = 3,
        UI = 4,
        PRINTER = 5,
        CREDIT_CARD = 6,
        FILE_IO = 7,
        MEMORY = 8
    };

    struct ErrorInfo {
        std::string message;
        Severity severity;
        Category category;
        std::string file;
        int line;
        std::string function;
        std::chrono::system_clock::time_point timestamp;
        int error_code;
        std::string context;
        
        // Modern constructor with string_view and default parameters
        ErrorInfo(std::string_view msg, Severity sev, Category cat, 
                 std::string_view file_name = "", int line_num = 0, 
                 std::string_view func_name = "", int code = 0, 
                 std::string_view ctx = "");
        
        // Copy and move constructors
        ErrorInfo(const ErrorInfo&) = default;
        ErrorInfo(ErrorInfo&&) = default;
        ErrorInfo& operator=(const ErrorInfo&) = default;
        ErrorInfo& operator=(ErrorInfo&&) = default;
    };

    class ErrorHandler {
    private:
        static std::unique_ptr<ErrorHandler> instance_;
        static std::once_flag instance_flag_;
        
        std::vector<ErrorInfo> error_history_;
        mutable std::shared_mutex history_mutex_;
        
        std::ofstream log_file_;
        std::string log_file_path_;
        bool console_output_;
        Severity min_log_level_;
        
        // Error notification callbacks
        std::vector<std::function<void(const ErrorInfo&)>> callbacks_;
        mutable std::shared_mutex callbacks_mutex_;
        
    public:
        // Default constructor (needed for make_unique)
        ErrorHandler() = default;
        
        // Singleton with modern thread-safe implementation
        static ErrorHandler& getInstance() noexcept;
        
        // Rule of five - disable copy, enable move
        ErrorHandler(const ErrorHandler&) = delete;
        ErrorHandler& operator=(const ErrorHandler&) = delete;
        ErrorHandler(ErrorHandler&&) = delete;
        ErrorHandler& operator=(ErrorHandler&&) = delete;
        
        ~ErrorHandler() = default;
        
        // Configuration
        void setLogFile(std::string_view path);
        void setConsoleOutput(bool enabled) noexcept;
        void setMinLogLevel(Severity level) noexcept;
        
        // Error reporting
        void reportError(std::string_view message, Severity severity = Severity::ERROR,
                        Category category = Category::GENERAL, int error_code = 0,
                        std::string_view context = "", std::string_view file = "",
                        int line = 0, std::string_view function = "");
        
        // Convenience methods
        void debug(std::string_view message, Category category = Category::GENERAL,
                  std::string_view context = "", std::string_view file = "",
                  int line = 0, std::string_view function = "");
        
        void info(std::string_view message, Category category = Category::GENERAL,
                 std::string_view context = "", std::string_view file = "",
                 int line = 0, std::string_view function = "");
        
        void warning(std::string_view message, Category category = Category::GENERAL,
                    std::string_view context = "", std::string_view file = "",
                    int line = 0, std::string_view function = "");
        
        void error(std::string_view message, Category category = Category::GENERAL,
                  int error_code = 0, std::string_view context = "",
                  std::string_view file = "", int line = 0, std::string_view function = "");
        
        void critical(std::string_view message, Category category = Category::GENERAL,
                     int error_code = 0, std::string_view context = "",
                     std::string_view file = "", int line = 0, std::string_view function = "");
        
        // Error history and retrieval
        std::vector<ErrorInfo> getErrorHistory(size_t max_entries = 100) const;
        std::vector<ErrorInfo> getErrorsByCategory(Category category, size_t max_entries = 100) const;
        std::vector<ErrorInfo> getErrorsBySeverity(Severity severity, size_t max_entries = 100) const;
        void clearErrorHistory() noexcept;
        
        // Callback registration for error notifications
        void registerCallback(std::function<void(const ErrorInfo&)> callback);
        void clearCallbacks() noexcept;
        
        // Utility functions - constexpr where possible
        static constexpr std::string_view severityToString(Severity severity) noexcept;
        static constexpr std::string_view categoryToString(Category category) noexcept;
        static Severity stringToSeverity(std::string_view str) noexcept;
        static Category stringToCategory(std::string_view str) noexcept;
        
    private:
        void logToFile(const ErrorInfo& error);
        void logToConsole(const ErrorInfo& error);
        void notifyCallbacks(const ErrorInfo& error);
        std::string formatLogEntry(const ErrorInfo& error) const;
    };

    // Convenience macros for easier error reporting - modernized
    #define VT_ERROR(message, category, ...) \
        vt_error::ErrorHandler::getInstance().error(message, category, ##__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
    
    #define VT_WARNING(message, category, ...) \
        vt_error::ErrorHandler::getInstance().warning(message, category, ##__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
    
    #define VT_INFO(message, category, ...) \
        vt_error::ErrorHandler::getInstance().info(message, category, ##__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
    
    #define VT_DEBUG(message, category, ...) \
        vt_error::ErrorHandler::getInstance().debug(message, category, ##__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)
    
    #define VT_CRITICAL(message, category, ...) \
        vt_error::ErrorHandler::getInstance().critical(message, category, ##__VA_ARGS__, __FILE__, __LINE__, __FUNCTION__)

    // Legacy compatibility function - modernized
    int ReportError(std::string_view message) noexcept;

} // namespace vt_error

#endif // VT_ERROR_HANDLER_HH
