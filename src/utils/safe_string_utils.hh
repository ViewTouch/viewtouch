#pragma once

#include <string>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <sstream>
#include <iostream>

/**
 * @brief Safe String Operations - Replacements for unsafe C string functions
 *
 * This header provides safe alternatives to dangerous C string functions
 * like strcpy, sprintf, etc. These functions prevent buffer overflows,
 * null pointer dereferences, and other common string-related bugs.
 */

namespace vt_safe_string {

/**
 * @brief Safe string copy with bounds checking
 *
 * Replaces strcpy/strncpy with guaranteed safety
 *
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer (including null terminator)
 * @param src Source string
 * @return true if copy succeeded, false if truncated or error
 */
inline bool safe_copy(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        return false;
    }

    size_t src_len = std::strlen(src);
    if (src_len >= dest_size) {
        // Source too long, copy what we can and null terminate
        std::memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        return false; // Truncated
    }

    std::memcpy(dest, src, src_len + 1); // Include null terminator
    return true; // Success
}

/**
 * @brief Safe string copy with bounds checking (std::string version)
 *
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string
 * @return true if copy succeeded, false if truncated
 */
inline bool safe_copy(char* dest, size_t dest_size, const std::string& src) {
    return safe_copy(dest, dest_size, src.c_str());
}

/**
 * @brief Safe string concatenation
 *
 * Replaces strcat/strncat with guaranteed safety
 *
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string to append
 * @return true if concatenation succeeded, false if truncated or error
 */
inline bool safe_concat(char* dest, size_t dest_size, const char* src) {
    if (!dest || !src || dest_size == 0) {
        return false;
    }

    size_t dest_len = std::strlen(dest);
    if (dest_len >= dest_size) {
        return false; // Destination already full
    }

    size_t remaining = dest_size - dest_len;
    return safe_copy(dest + dest_len, remaining, src);
}

/**
 * @brief Safe string concatenation (std::string version)
 *
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param src Source string to append
 * @return true if concatenation succeeded, false if truncated
 */
inline bool safe_concat(char* dest, size_t dest_size, const std::string& src) {
    return safe_concat(dest, dest_size, src.c_str());
}

/**
 * @brief Safe sprintf replacement
 *
 * Replaces sprintf with guaranteed safety using std::snprintf
 *
 * @param buffer Destination buffer
 * @param buffer_size Size of destination buffer
 * @param format Format string
 * @param args Format arguments
 * @return true if formatting succeeded, false if buffer too small or error
 */
template<typename... Args>
bool safe_format(char* buffer, size_t buffer_size, const char* format, Args... args) {
    if (!buffer || !format || buffer_size == 0) {
        return false;
    }

    int result = std::snprintf(buffer, buffer_size, format, args...);
    if (result < 0) {
        buffer[0] = '\0'; // Ensure null termination on error
        return false;
    }

    if (static_cast<size_t>(result) >= buffer_size) {
        buffer[buffer_size - 1] = '\0'; // Ensure null termination
        return false; // Buffer too small
    }

    return true;
}

/**
 * @brief Safe sprintf replacement (std::string version)
 *
 * @param buffer Destination buffer
 * @param buffer_size Size of destination buffer
 * @param format Format string
 * @param args Format arguments
 * @return true if formatting succeeded, false if buffer too small or error
 */
template<typename... Args>
bool safe_format(char* buffer, size_t buffer_size, const std::string& format, Args... args) {
    return safe_format(buffer, buffer_size, format.c_str(), args...);
}

/**
 * @brief Safe string formatting returning std::string
 *
 * Always safe, automatically allocates correct buffer size
 *
 * @param format Format string
 * @param args Format arguments
 * @return Formatted string
 * @throws std::runtime_error if formatting fails
 */
template<typename... Args>
std::string safe_format_string(const char* format, Args... args) {
    if (!format) {
        throw std::runtime_error("Format string cannot be null");
    }

    // Try with a reasonable buffer first
    constexpr size_t initial_buffer_size = 256;
    std::string result;
    result.resize(initial_buffer_size);

    int needed = std::snprintf(&result[0], result.size() + 1, format, args...);
    if (needed < 0) {
        throw std::runtime_error("String formatting failed");
    }

    if (static_cast<size_t>(needed) <= result.size()) {
        result.resize(needed); // Shrink to fit
        return result;
    }

    // Buffer was too small, resize and try again
    result.resize(needed);
    int final_result = std::snprintf(&result[0], result.size() + 1, format, args...);
    if (final_result < 0 || static_cast<size_t>(final_result) > result.size()) {
        throw std::runtime_error("String formatting failed on second attempt");
    }

    result.resize(final_result);
    return result;
}

/**
 * @brief Safe string formatting returning std::string (std::string format version)
 *
 * @param format Format string
 * @param args Format arguments
 * @return Formatted string
 * @throws std::runtime_error if formatting fails
 */
template<typename... Args>
std::string safe_format_string(const std::string& format, Args... args) {
    return safe_format_string(format.c_str(), args...);
}

/**
 * @brief Safe numeric conversion
 *
 * Replaces atoi/atof with error checking
 *
 * @param str String to convert
 * @param result Output parameter for converted value
 * @return true if conversion succeeded, false otherwise
 */
template<typename T>
bool safe_numeric_convert(const char* str, T& result) {
    if (!str) {
        return false;
    }

    // Use stringstream for safe conversion
    std::istringstream iss(str);
    iss >> result;

    // Check if conversion succeeded and consumed entire string
    return !iss.fail() && iss.eof();
}

/**
 * @brief Safe numeric conversion (std::string version)
 *
 * @param str String to convert
 * @param result Output parameter for converted value
 * @return true if conversion succeeded, false otherwise
 */
template<typename T>
bool safe_numeric_convert(const std::string& str, T& result) {
    return safe_numeric_convert(str.c_str(), result);
}

/**
 * @brief Bounds-checked array access for C-style strings
 *
 * @param str String to access
 * @param index Index to access
 * @param default_char Character to return if out of bounds
 * @return Character at index, or default_char if out of bounds
 */
inline char safe_char_at(const char* str, size_t index, char default_char = '\0') {
    if (!str) {
        return default_char;
    }

    size_t len = std::strlen(str);
    if (index >= len) {
        return default_char;
    }

    return str[index];
}

/**
 * @brief Bounds-checked array access for std::string
 *
 * @param str String to access
 * @param index Index to access
 * @param default_char Character to return if out of bounds
 * @return Character at index, or default_char if out of bounds
 */
inline char safe_char_at(const std::string& str, size_t index, char default_char = '\0') {
    if (index >= str.size()) {
        return default_char;
    }

    return str[index];
}

/**
 * @brief Safe substring extraction
 *
 * @param str Source string
 * @param start Start position
 * @param length Length to extract (0 = to end)
 * @return Extracted substring, empty if invalid parameters
 */
inline std::string safe_substring(const std::string& str, size_t start, size_t length = 0) {
    if (start >= str.size()) {
        return {};
    }

    if (length == 0) {
        return str.substr(start);
    }

    size_t end = start + length;
    if (end > str.size()) {
        end = str.size();
    }

    return str.substr(start, end - start);
}

/**
 * @brief Safe substring extraction (C-string version)
 *
 * @param str Source C-string
 * @param start Start position
 * @param length Length to extract (0 = to end)
 * @return Extracted substring, empty if invalid parameters
 */
inline std::string safe_substring(const char* str, size_t start, size_t length = 0) {
    if (!str) {
        return {};
    }

    size_t str_len = std::strlen(str);
    if (start >= str_len) {
        return {};
    }

    if (length == 0) {
        return std::string(str + start);
    }

    size_t end = start + length;
    if (end > str_len) {
        end = str_len;
    }

    return std::string(str + start, end - start);
}

/**
 * @brief Safe string comparison with null checking
 *
 * @param str1 First string
 * @param str2 Second string
 * @return true if strings are equal, false otherwise
 */
inline bool safe_equals(const char* str1, const char* str2) {
    if (!str1 || !str2) {
        return str1 == str2; // Both null = equal, one null = not equal
    }

    return std::strcmp(str1, str2) == 0;
}

/**
 * @brief Safe string comparison (std::string version)
 *
 * @param str1 First string
 * @param str2 Second string
 * @return true if strings are equal, false otherwise
 */
inline bool safe_equals(const std::string& str1, const std::string& str2) {
    return str1 == str2;
}

/**
 * @brief Safe string comparison (mixed version)
 *
 * @param str1 C-string
 * @param str2 std::string
 * @return true if strings are equal, false otherwise
 */
inline bool safe_equals(const char* str1, const std::string& str2) {
    if (!str1) {
        return false;
    }

    return str2 == str1;
}

/**
 * @brief Safe string comparison (mixed version)
 *
 * @param str1 std::string
 * @param str2 C-string
 * @return true if strings are equal, false otherwise
 */
inline bool safe_equals(const std::string& str1, const char* str2) {
    return safe_equals(str2, str1);
}

} // namespace vt_safe_string
