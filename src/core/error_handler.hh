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
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <fstream>
#include <chrono>

namespace vt_error {

    enum class Severity {
        VT_DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    enum class Category {
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
        
        ErrorInfo(const std::string& msg, Severity sev, Category cat, 
                 const std::string& file_name, int line_num, 
                 const std::string& func_name, int code = 0, 
                 const std::string& ctx = "");
    };

    class ErrorHandler {
    private:
        static std::unique_ptr<ErrorHandler> instance_;
        static std::mutex instance_mutex_;
        
        std::vector<ErrorInfo> error_history_;
        mutable std::mutex history_mutex_;
        
        std::ofstream log_file_;
        std::string log_file_path_;
        bool console_output_;
        Severity min_log_level_;
        
        // Error notification callbacks
        std::vector<std::function<void(const ErrorInfo&)>> callbacks_;
        mutable std::mutex callbacks_mutex_;
        
        ErrorHandler();
        
    public:
        static ErrorHandler& getInstance();
        ~ErrorHandler();
        
        // Configuration
        void setLogFile(const std::string& path);
        void setConsoleOutput(bool enabled);
        void setMinLogLevel(Severity level);
        
        // Error reporting
        void reportError(const std::string& message, Severity severity = Severity::ERROR,
                        Category category = Category::GENERAL, int error_code = 0,
                        const std::string& context = "", const std::string& file = "",
                        int line = 0, const std::string& function = "");
        
        // Convenience methods
        void debug(const std::string& message, Category category = Category::GENERAL,
                  const std::string& context = "", const std::string& file = "",
                  int line = 0, const std::string& function = "");
        
        void info(const std::string& message, Category category = Category::GENERAL,
                 const std::string& context = "", const std::string& file = "",
                 int line = 0, const std::string& function = "");
        
        void warning(const std::string& message, Category category = Category::GENERAL,
                    const std::string& context = "", const std::string& file = "",
                    int line = 0, const std::string& function = "");
        
        void error(const std::string& message, Category category = Category::GENERAL,
                  int error_code = 0, const std::string& context = "",
                  const std::string& file = "", int line = 0, const std::string& function = "");
        
        void critical(const std::string& message, Category category = Category::GENERAL,
                     int error_code = 0, const std::string& context = "",
                     const std::string& file = "", int line = 0, const std::string& function = "");
        
        // Error history and retrieval
        std::vector<ErrorInfo> getErrorHistory(size_t max_entries = 100) const;
        std::vector<ErrorInfo> getErrorsByCategory(Category category, size_t max_entries = 100) const;
        std::vector<ErrorInfo> getErrorsBySeverity(Severity severity, size_t max_entries = 100) const;
        void clearErrorHistory();
        
        // Callback registration for error notifications
        void registerCallback(std::function<void(const ErrorInfo&)> callback);
        void clearCallbacks();
        
        // Utility functions
        static std::string severityToString(Severity severity);
        static std::string categoryToString(Category category);
        static Severity stringToSeverity(const std::string& str);
        static Category stringToCategory(const std::string& str);
        
    private:
        void logToFile(const ErrorInfo& error);
        void logToConsole(const ErrorInfo& error);
        void notifyCallbacks(const ErrorInfo& error);
        std::string formatLogEntry(const ErrorInfo& error) const;
    };

    // Convenience macros for easier error reporting
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

    // Legacy compatibility function
    int ReportError(const std::string& message);

} // namespace vt_error

#endif // VT_ERROR_HANDLER_HH
