#include "input_validation.hh"
#include "safe_string_utils.hh"
#include <regex>
#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace vt_input_validation {

// NetworkValidator implementation
ValidationResult NetworkValidator::validate_hostname(const std::string& hostname) {
    if (hostname.empty()) {
        return ValidationResult(false, "Hostname cannot be empty");
    }

    if (hostname.length() > 253) {  // RFC 1035 limit
        return ValidationResult(false, "Hostname too long (max 253 characters)");
    }

    // Basic hostname validation (allow alphanumeric, dots, hyphens)
    std::regex hostname_regex("^[a-zA-Z0-9]([a-zA-Z0-9\\-\\.]*[a-zA-Z0-9])?$");
    if (!std::regex_match(hostname, hostname_regex)) {
        return ValidationResult(false, "Invalid hostname format");
    }

    // Check for consecutive dots
    if (hostname.find("..") != std::string::npos) {
        return ValidationResult(false, "Hostname cannot contain consecutive dots");
    }

    return ValidationResult(true, "", hostname);
}

ValidationResult NetworkValidator::validate_port(int port) {
    if (port < 1 || port > 65535) {
        return ValidationResult(false, "Port must be between 1 and 65535");
    }
    return ValidationResult(true, "", std::to_string(port));
}

ValidationResult NetworkValidator::validate_port(const std::string& port_str) {
    if (port_str.empty()) {
        return ValidationResult(false, "Port string cannot be empty");
    }

    int port;
    if (!vt_safe_string::safe_numeric_convert(port_str, port)) {
        return ValidationResult(false, "Port must be a valid number");
    }

    return validate_port(port);
}

ValidationResult NetworkValidator::validate_buffer_size(size_t size, size_t max_size) {
    if (size > max_size) {
        return ValidationResult(false, "Buffer size exceeds maximum allowed size");
    }
    return ValidationResult(true);
}

ValidationResult NetworkValidator::validate_socket_data(const char* data, size_t length) {
    if (!data && length > 0) {
        return ValidationResult(false, "Null data pointer with non-zero length");
    }

    // Check for embedded null bytes (potential security issue)
    for (size_t i = 0; i < length; ++i) {
        if (data[i] == '\0' && i < length - 1) {
            return ValidationResult(false, "Embedded null byte in data stream");
        }
    }

    return ValidationResult(true);
}

ValidationResult NetworkValidator::validate_card_number_format(const std::string& card_data) {
    // Remove spaces and dashes for validation
    std::string cleaned;
    for (char c : card_data) {
        if (std::isdigit(c)) {
            cleaned += c;
        }
    }

    if (cleaned.length() < 13 || cleaned.length() > 19) {
        return ValidationResult(false, "Card number length invalid");
    }

    // Basic Luhn algorithm check (without storing the actual number)
    int sum = 0;
    bool alternate = false;
    for (int i = static_cast<int>(cleaned.length()) - 1; i >= 0; --i) {
        int digit = cleaned[i] - '0';
        if (alternate) {
            digit *= 2;
            if (digit > 9) {
                digit -= 9;
            }
        }
        sum += digit;
        alternate = !alternate;
    }

    if (sum % 10 != 0) {
        return ValidationResult(false, "Invalid card number checksum");
    }

    return ValidationResult(true, "", std::string(card_data.length(), '*')); // Mask the data
}

ValidationResult NetworkValidator::validate_protocol_message(const std::string& message, size_t max_length) {
    if (message.length() > max_length) {
        return ValidationResult(false, "Protocol message too long");
    }

    // Check for protocol-specific validation
    if (message.find('\0') != std::string::npos) {
        return ValidationResult(false, "Protocol message contains null bytes");
    }

    return ValidationResult(true, "", message);
}

// BusinessValidator implementation
ValidationResult BusinessValidator::validate_price(int amount_cents) {
    // Allow negative prices for discounts/comp adjustments, but reasonable bounds
    if (amount_cents < -1000000 || amount_cents > 1000000) {  // -$10,000 to $10,000
        return ValidationResult(false, "Price amount out of reasonable bounds");
    }
    return ValidationResult(true, "", std::to_string(amount_cents));
}

ValidationResult BusinessValidator::validate_price(const std::string& price_str) {
    // Handle various price formats: "10.99", "1099", etc.
    std::string cleaned;
    bool has_decimal = false;

    for (char c : price_str) {
        if (std::isdigit(c)) {
            cleaned += c;
        } else if (c == '.' && !has_decimal) {
            has_decimal = true;
            cleaned += c;
        } else if (c == '-') {
            cleaned = "-" + cleaned;  // Handle negative at start
        }
    }

    if (cleaned.empty()) {
        return ValidationResult(false, "Invalid price format");
    }

    // Convert to cents
    double dollar_amount;
    if (!vt_safe_string::safe_numeric_convert(cleaned, dollar_amount)) {
        return ValidationResult(false, "Price must be a valid number");
    }

    int cents = static_cast<int>(dollar_amount * 100 + (dollar_amount >= 0 ? 0.5 : -0.5));
    return validate_price(cents);
}

ValidationResult BusinessValidator::validate_quantity(int quantity) {
    if (quantity < 0 || quantity > 10000) {  // Reasonable quantity bounds
        return ValidationResult(false, "Quantity must be between 0 and 10000");
    }
    return ValidationResult(true, "", std::to_string(quantity));
}

ValidationResult BusinessValidator::validate_quantity(const std::string& qty_str) {
    int quantity;
    if (!vt_safe_string::safe_numeric_convert(qty_str, quantity)) {
        return ValidationResult(false, "Quantity must be a valid number");
    }
    return validate_quantity(quantity);
}

ValidationResult BusinessValidator::validate_discount_percent(float percent) {
    if (percent < 0.0f || percent > 100.0f) {
        return ValidationResult(false, "Discount percentage must be between 0% and 100%");
    }
    return ValidationResult(true, "", std::to_string(percent));
}

ValidationResult BusinessValidator::validate_discount_percent(const std::string& percent_str) {
    float percent;
    if (!vt_safe_string::safe_numeric_convert(percent_str, percent)) {
        return ValidationResult(false, "Discount percentage must be a valid number");
    }
    return validate_discount_percent(percent);
}

ValidationResult BusinessValidator::validate_tax_rate(float rate) {
    if (rate < 0.0f || rate > 0.5f) {  // Max 50% tax rate
        return ValidationResult(false, "Tax rate must be between 0% and 50%");
    }
    return ValidationResult(true, "", std::to_string(rate));
}

ValidationResult BusinessValidator::validate_tax_rate(const std::string& rate_str) {
    float rate;
    if (!vt_safe_string::safe_numeric_convert(rate_str, rate)) {
        return ValidationResult(false, "Tax rate must be a valid number");
    }
    return validate_tax_rate(rate);
}

ValidationResult BusinessValidator::validate_check_total(int total_cents) {
    if (total_cents < 0 || total_cents > 10000000) {  // Max $100,000 check
        return ValidationResult(false, "Check total out of valid range");
    }
    return ValidationResult(true, "", std::to_string(total_cents));
}

ValidationResult BusinessValidator::validate_employee_id(int id) {
    if (id < 1 || id > 999999) {  // Reasonable employee ID range
        return ValidationResult(false, "Invalid employee ID range");
    }
    return ValidationResult(true, "", std::to_string(id));
}

ValidationResult BusinessValidator::validate_employee_id(const std::string& id_str) {
    int id;
    if (!vt_safe_string::safe_numeric_convert(id_str, id)) {
        return ValidationResult(false, "Employee ID must be a valid number");
    }
    return validate_employee_id(id);
}

ValidationResult BusinessValidator::validate_table_number(int table_num) {
    if (table_num < 1 || table_num > 999) {  // Reasonable table number range
        return ValidationResult(false, "Table number must be between 1 and 999");
    }
    return ValidationResult(true, "", std::to_string(table_num));
}

ValidationResult BusinessValidator::validate_table_number(const std::string& table_str) {
    int table_num;
    if (!vt_safe_string::safe_numeric_convert(table_str, table_num)) {
        return ValidationResult(false, "Table number must be a valid number");
    }
    return validate_table_number(table_num);
}

// UserInputValidator implementation
ValidationResult UserInputValidator::validate_text_input(const std::string& input,
                                                       size_t max_length,
                                                       bool allow_special_chars) {
    if (input.length() > max_length) {
        return ValidationResult(false, "Input exceeds maximum length");
    }

    // Check for control characters
    for (char c : input) {
        if (std::iscntrl(c) && c != '\t' && c != '\n' && c != '\r') {
            return ValidationResult(false, "Input contains invalid control characters");
        }
    }

    if (!allow_special_chars) {
        // Check for potentially dangerous characters
        for (char c : input) {
            if (!std::isalnum(c) && !std::isspace(c) && c != '-' && c != '_' && c != '.') {
                return ValidationResult(false, "Input contains invalid special characters");
            }
        }
    }

    return ValidationResult(true, "", input);
}

ValidationResult UserInputValidator::validate_name(const std::string& name) {
    auto result = validate_text_input(name, 100, false);
    if (!result.is_valid) {
        return result;
    }

    // Additional name-specific validation
    if (name.empty()) {
        return ValidationResult(false, "Name cannot be empty");
    }

    // Check for reasonable name patterns
    if (name.length() < 2) {
        return ValidationResult(false, "Name too short");
    }

    return ValidationResult(true, "", name);
}

ValidationResult UserInputValidator::validate_email(const std::string& email) {
    if (email.empty()) {
        return ValidationResult(false, "Email cannot be empty");
    }

    if (email.length() > 254) {  // RFC 5321 limit
        return ValidationResult(false, "Email address too long");
    }

    // Basic email regex (simplified)
    std::regex email_regex("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    if (!std::regex_match(email, email_regex)) {
        return ValidationResult(false, "Invalid email format");
    }

    return ValidationResult(true, "", email);
}

ValidationResult UserInputValidator::validate_phone(const std::string& phone) {
    // Remove all non-digit characters for validation
    std::string digits_only;
    for (char c : phone) {
        if (std::isdigit(c)) {
            digits_only += c;
        }
    }

    if (digits_only.length() < 10 || digits_only.length() > 15) {
        return ValidationResult(false, "Phone number must have 10-15 digits");
    }

    // Basic US phone validation (optional)
    if (digits_only.length() == 10) {
        // Check area code doesn't start with 0 or 1
        if (digits_only[0] == '0' || digits_only[0] == '1') {
            return ValidationResult(false, "Invalid area code");
        }
    }

    return ValidationResult(true, "", phone);
}

ValidationResult UserInputValidator::validate_address(const std::string& address) {
    auto result = validate_text_input(address, 500, true);
    if (!result.is_valid) {
        return result;
    }

    if (address.empty()) {
        return ValidationResult(false, "Address cannot be empty");
    }

    return ValidationResult(true, "", address);
}

ValidationResult UserInputValidator::validate_password(const std::string& password) {
    if (password.length() < 8) {
        return ValidationResult(false, "Password must be at least 8 characters long");
    }

    if (password.length() > 128) {
        return ValidationResult(false, "Password too long");
    }

    // Check for at least one uppercase, one lowercase, one digit
    bool has_upper = false, has_lower = false, has_digit = false;
    for (char c : password) {
        if (std::isupper(c)) has_upper = true;
        if (std::islower(c)) has_lower = true;
        if (std::isdigit(c)) has_digit = true;
    }

    if (!has_upper || !has_lower || !has_digit) {
        return ValidationResult(false, "Password must contain uppercase, lowercase, and numeric characters");
    }

    return ValidationResult(true, "", std::string(password.length(), '*')); // Mask password
}

ValidationResult UserInputValidator::validate_username(const std::string& username) {
    auto result = validate_text_input(username, 50, false);
    if (!result.is_valid) {
        return result;
    }

    if (username.empty()) {
        return ValidationResult(false, "Username cannot be empty");
    }

    if (username.length() < 3) {
        return ValidationResult(false, "Username too short");
    }

    // Must start with letter
    if (!std::isalpha(username[0])) {
        return ValidationResult(false, "Username must start with a letter");
    }

    return ValidationResult(true, "", username);
}

std::string UserInputValidator::sanitize_html(const std::string& input) {
    std::string result = input;

    // Remove script tags
    std::regex script_regex("<script[^>]*>.*?</script>", std::regex_constants::icase);
    result = std::regex_replace(result, script_regex, "");

    // Remove other dangerous tags
    std::regex tag_regex("<[^>]+>", std::regex_constants::icase);
    result = std::regex_replace(result, tag_regex, "");

    return result;
}

// ConfigValidator implementation
ValidationResult ConfigValidator::validate_config_path(const std::string& path) {
    if (path.empty()) {
        return ValidationResult(false, "Configuration path cannot be empty");
    }

    if (path.length() > 4096) {  // PATH_MAX on most systems
        return ValidationResult(false, "Configuration path too long");
    }

    // Check for path traversal attempts
    if (path.find("..") != std::string::npos) {
        return ValidationResult(false, "Configuration path contains path traversal");
    }

    // Basic path validation
    if (path[0] != '/' && path.find("./") != 0 && path.find("../") != 0) {
        // Relative path - check if it exists
        try {
            if (!fs::exists(path)) {
                return ValidationResult(false, "Configuration file does not exist");
            }
        } catch (const fs::filesystem_error&) {
            return ValidationResult(false, "Invalid configuration path");
        }
    }

    return ValidationResult(true, "", path);
}

ValidationResult ConfigValidator::validate_time_format(const std::string& format) {
    // Allow common time formats
    std::vector<std::string> valid_formats = {
        "%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p"
    };

    for (const auto& valid_format : valid_formats) {
        if (format == valid_format) {
            return ValidationResult(true, "", format);
        }
    }

    return ValidationResult(false, "Invalid time format");
}

ValidationResult ConfigValidator::validate_date_format(const std::string& format) {
    // Allow common date formats
    std::vector<std::string> valid_formats = {
        "%m/%d/%Y", "%d/%m/%Y", "%Y-%m-%d", "%m-%d-%Y"
    };

    for (const auto& valid_format : valid_formats) {
        if (format == valid_format) {
            return ValidationResult(true, "", format);
        }
    }

    return ValidationResult(false, "Invalid date format");
}

// SecurityValidator implementation
ValidationResult SecurityValidator::check_sql_injection(const std::string& input) {
    // Common SQL injection patterns (case-insensitive)
    std::vector<std::string> sql_patterns = {
        "union", "select", "insert", "update", "delete", "drop",
        "or 1=1", "or true", "/*", "*/", "--", "#", "xp_cmdshell", "exec",
        "script", "javascript:", "vbscript:", "onload", "onerror"
    };

    std::string lower_input = input;
    std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

    for (const auto& pattern : sql_patterns) {
        if (lower_input.find(pattern) != std::string::npos) {
            return ValidationResult(false, "Potential SQL injection detected");
        }
    }

    return ValidationResult(true, "", input);
}

ValidationResult SecurityValidator::check_command_injection(const std::string& input) {
    // Common command injection patterns
    std::vector<std::string> cmd_patterns = {
        ";", "|", "&", "`", "$(", "${", ">", "<", "2>", "2>>",
        "&&", "||", ">>", "<<", "2>&1", "1>&2", "rm ", "rmdir ", "del ", "format ",
        "shutdown ", "reboot ", "halt ", "poweroff ", "mkfs", "fdisk", "dd "
    };

    for (const auto& pattern : cmd_patterns) {
        if (input.find(pattern) != std::string::npos) {
            return ValidationResult(false, "Potential command injection detected");
        }
    }

    return ValidationResult(true, "", input);
}

ValidationResult SecurityValidator::check_path_traversal(const std::string& path) {
    // Path traversal patterns
    std::vector<std::string> traversal_patterns = {
        "..", "../", "..\\", ".\\", "~/"
    };

    for (const auto& pattern : traversal_patterns) {
        if (path.find(pattern) != std::string::npos) {
            return ValidationResult(false, "Potential path traversal detected");
        }
    }

    return ValidationResult(true, "", path);
}

ValidationResult SecurityValidator::check_buffer_overflow(const std::string& input, size_t max_length) {
    if (input.length() > max_length) {
        return ValidationResult(false, "Input exceeds maximum allowed length");
    }

    // Check for extremely long lines that might indicate attack
    size_t line_length = 0;
    for (char c : input) {
        if (c == '\n' || c == '\r') {
            if (line_length > max_length / 2) {
                return ValidationResult(false, "Extremely long line detected");
            }
            line_length = 0;
        } else {
            line_length++;
        }
    }

    return ValidationResult(true, "", input);
}

ValidationResult SecurityValidator::validate_file_extension(const std::string& filename,
                                                         const std::vector<std::string>& allowed_extensions) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return ValidationResult(false, "File must have an extension");
    }

    std::string extension = filename.substr(dot_pos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    for (const auto& allowed : allowed_extensions) {
        if (extension == allowed) {
            return ValidationResult(true, "", filename);
        }
    }

    return ValidationResult(false, "File extension not allowed");
}

ValidationResult SecurityValidator::check_suspicious_patterns(const std::string& data) {
    // Check for binary data in text streams
    int binary_chars = 0;
    for (char c : data) {
        if (static_cast<unsigned char>(c) < 32 && c != '\n' && c != '\r' && c != '\t') {
            binary_chars++;
        }
    }

    if (binary_chars > data.length() / 10) {  // More than 10% binary chars
        return ValidationResult(false, "Data contains excessive binary characters");
    }

    // Check for repeated characters (potential DoS)
    if (data.length() > 100) {
        char last_char = data[0];
        int repeat_count = 1;
        for (size_t i = 1; i < data.length(); ++i) {
            if (data[i] == last_char) {
                repeat_count++;
                if (repeat_count > 100) {  // 100+ repeated chars
                    return ValidationResult(false, "Data contains excessive character repetition");
                }
            } else {
                repeat_count = 1;
                last_char = data[i];
            }
        }
    }

    return ValidationResult(true, "", data);
}

// ValidationUtils implementation
bool ValidationUtils::is_alphanumeric(const std::string& str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c));
    });
}

bool ValidationUtils::is_numeric(const std::string& str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    });
}

bool ValidationUtils::is_valid_identifier(const std::string& str) {
    if (str.empty()) return false;

    // Must start with letter or underscore
    if (!std::isalpha(str[0]) && str[0] != '_') return false;

    // Rest must be alphanumeric or underscore
    return std::all_of(str.begin() + 1, str.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

std::string ValidationUtils::trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        ++start;
    }

    auto end = str.end();
    do {
        --end;
    } while (end != start && std::isspace(*end));

    return std::string(start, end + 1);
}

bool ValidationUtils::is_length_valid(const std::string& str, size_t min_len, size_t max_len) {
    return str.length() >= min_len && str.length() <= max_len;
}

std::string ValidationUtils::escape_special_chars(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c; break;
        }
    }
    return result;
}

ValidationResult ValidationUtils::validate_utf8(const std::string& str) {
    // Basic UTF-8 validation
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = str[i];

        if (c <= 0x7F) {
            // ASCII character
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= str.length() || (str[i + 1] & 0xC0) != 0x80) {
                return ValidationResult(false, "Invalid UTF-8 2-byte sequence");
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= str.length() ||
                (str[i + 1] & 0xC0) != 0x80 ||
                (str[i + 2] & 0xC0) != 0x80) {
                return ValidationResult(false, "Invalid UTF-8 3-byte sequence");
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= str.length() ||
                (str[i + 1] & 0xC0) != 0x80 ||
                (str[i + 2] & 0xC0) != 0x80 ||
                (str[i + 3] & 0xC0) != 0x80) {
                return ValidationResult(false, "Invalid UTF-8 4-byte sequence");
            }
            i += 4;
        } else {
            return ValidationResult(false, "Invalid UTF-8 byte sequence");
        }
    }

    return ValidationResult(true, "", str);
}

// Sanitizer implementation
std::string Sanitizer::remove_null_bytes(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c != '\0') {
            result += c;
        }
    }
    return result;
}

std::string Sanitizer::remove_control_chars(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (!std::iscntrl(c) || c == '\n' || c == '\t' || c == '\r') {
            result += c;
        }
    }
    return result;
}

std::string Sanitizer::normalize_line_endings(const std::string& input) {
    std::string result = input;
    // Convert \r\n to \n first
    size_t pos = 0;
    while ((pos = result.find("\r\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        // Don't increment pos since we just shortened the string
    }
    // Then convert remaining \r to \n
    pos = 0;
    while ((pos = result.find('\r', pos)) != std::string::npos) {
        result.replace(pos, 1, "\n");
        // Don't increment pos since we just replaced one char with one char
    }
    return result;
}

std::string Sanitizer::remove_dangerous_chars(const std::string& input) {
    std::string result;
    for (char c : input) {
        // Allow printable ASCII and safe control chars
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\t' || c == '\r') {
            result += c;
        }
    }
    return result;
}

std::string Sanitizer::sanitize_for_sql(const std::string& input) {
    std::string result = input;

    // Escape single quotes for SQL
    size_t pos = 0;
    while ((pos = result.find('\'', pos)) != std::string::npos) {
        result.replace(pos, 1, "''");
        pos += 2;  // Skip the doubled quote
    }

    return result;
}

std::string Sanitizer::sanitize_for_shell(const std::string& input) {
    std::string result;

    for (char c : input) {
        // Escape shell metacharacters
        switch (c) {
            case '|': case '&': case ';': case '(': case ')': case '<': case '>':
            case ' ': case '\t': case '\n': case '\'': case '"': case '\\':
                result += '\\';
                result += c;
                break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

} // namespace vt_input_validation
