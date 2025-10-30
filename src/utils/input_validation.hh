#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <limits>
#include <regex>

/**
 * @brief Input Validation and Sanitization Utilities
 *
 * This header provides comprehensive input validation for ViewTouch,
 * including network data, user input, business logic, and configuration validation.
 */

namespace vt_input_validation {

// Forward declarations for ViewTouch types
class Check;
class Order;
class Payment;
class Terminal;
class Settings;

/**
 * @brief Validation result structure
 */
struct ValidationResult {
    bool is_valid = true;
    std::string error_message;
    std::string sanitized_value;

    ValidationResult() = default;
    ValidationResult(bool valid, const std::string& error = "", const std::string& sanitized = "")
        : is_valid(valid), error_message(error), sanitized_value(sanitized) {}
};

/**
 * @brief Severity levels for validation errors
 */
enum class ValidationSeverity {
    INFO,      // Informational, non-blocking
    WARNING,   // Warning, but allow processing
    ERROR,     // Error, block processing
    CRITICAL   // Critical error, terminate operation
};

/**
 * @brief Network Data Validation
 *
 * Validates network protocol data, socket inputs, and communication buffers
 */
class NetworkValidator {
public:
    /**
     * @brief Validate hostname/IP address
     */
    static ValidationResult validate_hostname(const std::string& hostname);

    /**
     * @brief Validate port number
     */
    static ValidationResult validate_port(int port);
    static ValidationResult validate_port(const std::string& port_str);

    /**
     * @brief Validate network buffer size
     */
    static ValidationResult validate_buffer_size(size_t size, size_t max_size = 65536);

    /**
     * @brief Validate socket data (generic buffer validation)
     */
    static ValidationResult validate_socket_data(const char* data, size_t length);

    /**
     * @brief Validate credit card data format (without storing sensitive data)
     */
    static ValidationResult validate_card_number_format(const std::string& card_data);

    /**
     * @brief Validate protocol messages
     */
    static ValidationResult validate_protocol_message(const std::string& message, size_t max_length = 4096);
};

/**
 * @brief Business Logic Validation
 *
 * Validates business rules, prices, quantities, and financial data
 */
class BusinessValidator {
public:
    /**
     * @brief Validate monetary amount (cents)
     */
    static ValidationResult validate_price(int amount_cents);
    static ValidationResult validate_price(const std::string& price_str);

    /**
     * @brief Validate quantity
     */
    static ValidationResult validate_quantity(int quantity);
    static ValidationResult validate_quantity(const std::string& qty_str);

    /**
     * @brief Validate discount percentage
     */
    static ValidationResult validate_discount_percent(float percent);
    static ValidationResult validate_discount_percent(const std::string& percent_str);

    /**
     * @brief Validate tax rate
     */
    static ValidationResult validate_tax_rate(float rate);
    static ValidationResult validate_tax_rate(const std::string& rate_str);

    /**
     * @brief Validate check total
     */
    static ValidationResult validate_check_total(int total_cents);

    /**
     * @brief Validate employee ID
     */
    static ValidationResult validate_employee_id(int id);
    static ValidationResult validate_employee_id(const std::string& id_str);

    /**
     * @brief Validate table number
     */
    static ValidationResult validate_table_number(int table_num);
    static ValidationResult validate_table_number(const std::string& table_str);
};

/**
 * @brief User Input Validation
 *
 * Validates and sanitizes user interface inputs
 */
class UserInputValidator {
public:
    /**
     * @brief Sanitize and validate text input
     */
    static ValidationResult validate_text_input(const std::string& input,
                                              size_t max_length = 255,
                                              bool allow_special_chars = false);

    /**
     * @brief Validate and sanitize names
     */
    static ValidationResult validate_name(const std::string& name);

    /**
     * @brief Validate email address
     */
    static ValidationResult validate_email(const std::string& email);

    /**
     * @brief Validate phone number
     */
    static ValidationResult validate_phone(const std::string& phone);

    /**
     * @brief Validate address
     */
    static ValidationResult validate_address(const std::string& address);

    /**
     * @brief Validate password strength
     */
    static ValidationResult validate_password(const std::string& password);

    /**
     * @brief Validate username
     */
    static ValidationResult validate_username(const std::string& username);

    /**
     * @brief Sanitize HTML/script content (XSS prevention)
     */
    static std::string sanitize_html(const std::string& input);
};

/**
 * @brief Configuration Validation
 *
 * Validates configuration files and settings
 */
class ConfigValidator {
public:
    /**
     * @brief Validate configuration file path
     */
    static ValidationResult validate_config_path(const std::string& path);

    /**
     * @brief Validate database connection string
     */
    static ValidationResult validate_db_connection(const std::string& conn_str);

    /**
     * @brief Validate time format string
     */
    static ValidationResult validate_time_format(const std::string& format);

    /**
     * @brief Validate date format string
     */
    static ValidationResult validate_date_format(const std::string& format);

    /**
     * @brief Validate file permissions
     */
    static ValidationResult validate_file_permissions(const std::string& path, int required_perms);

    /**
     * @brief Validate directory path
     */
    static ValidationResult validate_directory_path(const std::string& path);
};

/**
 * @brief Security Validation
 *
 * Validates inputs for security vulnerabilities
 */
class SecurityValidator {
public:
    /**
     * @brief Check for SQL injection patterns
     */
    static ValidationResult check_sql_injection(const std::string& input);

    /**
     * @brief Check for command injection patterns
     */
    static ValidationResult check_command_injection(const std::string& input);

    /**
     * @brief Check for path traversal attacks
     */
    static ValidationResult check_path_traversal(const std::string& path);

    /**
     * @brief Check for buffer overflow attempts
     */
    static ValidationResult check_buffer_overflow(const std::string& input, size_t max_length);

    /**
     * @brief Validate file extension for security
     */
    static ValidationResult validate_file_extension(const std::string& filename,
                                                  const std::vector<std::string>& allowed_extensions);

    /**
     * @brief Check for suspicious patterns in network data
     */
    static ValidationResult check_suspicious_patterns(const std::string& data);
};

/**
 * @brief Validation Context
 *
 * Manages validation state and provides context for validation operations
 */
class ValidationContext {
public:
    ValidationContext() = default;

    void set_severity_level(ValidationSeverity level) { severity_ = level; }
    ValidationSeverity get_severity_level() const { return severity_; }

    void add_error(const std::string& error) { errors_.push_back(error); }
    void add_warning(const std::string& warning) { warnings_.push_back(warning); }

    const std::vector<std::string>& get_errors() const { return errors_; }
    const std::vector<std::string>& get_warnings() const { return warnings_; }

    bool has_errors() const { return !errors_.empty(); }
    bool has_warnings() const { return !warnings_.empty(); }

    void clear() {
        errors_.clear();
        warnings_.clear();
    }

private:
    ValidationSeverity severity_ = ValidationSeverity::ERROR;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};

/**
 * @brief Validation utilities
 */
class ValidationUtils {
public:
    /**
     * @brief Check if string contains only alphanumeric characters
     */
    static bool is_alphanumeric(const std::string& str);

    /**
     * @brief Check if string contains only digits
     */
    static bool is_numeric(const std::string& str);

    /**
     * @brief Check if string is a valid identifier
     */
    static bool is_valid_identifier(const std::string& str);

    /**
     * @brief Trim whitespace from string
     */
    static std::string trim(const std::string& str);

    /**
     * @brief Check string length constraints
     */
    static bool is_length_valid(const std::string& str, size_t min_len, size_t max_len);

    /**
     * @brief Escape special characters for safe display
     */
    static std::string escape_special_chars(const std::string& str);

    /**
     * @brief Validate UTF-8 encoding
     */
    static ValidationResult validate_utf8(const std::string& str);
};

/**
 * @brief Input Sanitization Functions
 */
class Sanitizer {
public:
    /**
     * @brief Remove null bytes and control characters
     */
    static std::string remove_null_bytes(const std::string& input);

    /**
     * @brief Remove control characters (except safe ones like \n, \t)
     */
    static std::string remove_control_chars(const std::string& input);

    /**
     * @brief Normalize line endings
     */
    static std::string normalize_line_endings(const std::string& input);

    /**
     * @brief Remove potentially dangerous characters
     */
    static std::string remove_dangerous_chars(const std::string& input);

    /**
     * @brief Sanitize for SQL queries
     */
    static std::string sanitize_for_sql(const std::string& input);

    /**
     * @brief Sanitize for shell commands
     */
    static std::string sanitize_for_shell(const std::string& input);
};

/**
 * @brief Validation Macros for Convenience
 */
#define VT_VALIDATE_OR_RETURN(condition, error_msg) \
    if (!(condition)) { \
        return ValidationResult(false, error_msg); \
    }

#define VT_VALIDATE_OR_RETURN_SANITIZED(condition, error_msg, sanitized) \
    if (!(condition)) { \
        return ValidationResult(false, error_msg, sanitized); \
    }

#define VT_SANITIZE_OR_RETURN(input, sanitizer_func) \
    { \
        auto sanitized = sanitizer_func(input); \
        if (sanitized != input) { \
            return ValidationResult(true, "", sanitized); \
        } \
    }

} // namespace vt_input_validation
