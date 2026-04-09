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
 * cpp23_utils.hh - C++23 utility functions and modernization helpers
 * Provides convenient wrappers for C++23 features used throughout ViewTouch
 */

#ifndef VT_CPP23_UTILS_HH
#define VT_CPP23_UTILS_HH

#include <utility>      // std::to_underlying (C++23)
#include <string>
#include <string_view>
#include <system_error>
#include <stdexcept>
#include <type_traits>
#include <algorithm>    // std::clamp

// Feature detection for C++20/C++23 features
#ifdef __has_include
#  if __has_include(<format>)
#    include <format>
#    define VT_HAS_STD_FORMAT 1
#  elif __has_include(<fmt/format.h>)
#    include <fmt/format.h>
#    define VT_HAS_FMTLIB 1
#  endif
#endif

#ifdef __cpp_lib_expected
#  include <expected>
#  define VT_HAS_STD_EXPECTED 1
#endif

#ifndef VT_HAS_STD_FORMAT
#  warning "std::format is not available in this compiler/stdlib combination. This will cause compilation errors."
#  warning "ViewTouch requires C++23 with a modern standard library (GCC 14+ or Clang 18+ with libc++)."
#  warning "Please upgrade your compiler or use a newer standard library."
#endif

namespace vt::cpp23 {

// ============================================================================
// Enum Utilities
// ============================================================================

/**
 * @brief Type-safe conversion of enum to its underlying type
 * 
 * Wrapper for std::to_underlying providing semantic meaning.
 * Use this instead of static_cast<int>(enum_value).
 * 
 * @tparam E Enum type
 * @param e Enum value
 * @return Underlying value
 * 
 * Example:
 *   enum class Color : uint8_t { Red = 1, Green = 2, Blue = 3 };
 *   auto value = to_underlying(Color::Red);  // returns uint8_t(1)
 */
template<typename E>
    requires std::is_enum_v<E>
[[nodiscard]] constexpr std::underlying_type_t<E> to_underlying(E e) noexcept {
    #if __cpp_lib_to_underlying >= 202102L
        return std::to_underlying(e);
    #else
        return static_cast<std::underlying_type_t<E>>(e);
    #endif
}

// ============================================================================
// Control Flow Utilities  
// ============================================================================

/**
 * @brief Marks code path as unreachable
 * 
 * Use this in switch statements or if-else chains where a path
 * should logically never be reached. Triggers undefined behavior
 * if actually executed (compiler optimizes assuming this never happens).
 * 
 * Example:
 *   switch (value) {
 *       case 0: return "zero";
 *       case 1: return "one";
 *       default: unreachable();  // All cases handled
 *   }
 */
[[noreturn]] inline void unreachable() {
    // Use compiler builtin if available, otherwise std::unreachable in C++23
    #if defined(__GNUC__) || defined(__clang__)
        __builtin_unreachable();
    #elif defined(_MSC_VER)
        __assume(false);
    #else
        // Fallback for other compilers
        std::terminate();
    #endif
}

// ============================================================================
// String Formatting (C++20 std::format, enhanced in C++23)
// ============================================================================

// String formatting utilities: provide implementations using std::format (C++20/23)
// or fall back to {fmt} if available. If neither is present, provide a
// minimal snprintf-based fallback for simple formatting.

#if defined(VT_HAS_STD_FORMAT)

template<typename... Args>
[[nodiscard]] std::string format(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
void format_to(std::string& str, std::format_string<Args...> fmt, Args&&... args) {
    str.clear();
    std::format_to(std::back_inserter(str), fmt, std::forward<Args>(args)...);
}

template<typename... Args>
[[nodiscard]] std::size_t format_to_buffer(
    char* buffer,
    std::size_t size,
    std::format_string<Args...> fmt,
    Args&&... args)
{
    if (size == 0) return 0;

    try {
        auto result = std::format_to_n(buffer, size - 1, fmt, std::forward<Args>(args)...);
        buffer[result.size < size ? result.size : size - 1] = '\0';
        return result.size;
    } catch (...) {
        buffer[0] = '\0';
        return 0;
    }
}

#elif defined(VT_HAS_FMTLIB)

// Use {fmt} library as a compatible fallback.
template<typename... Args>
[[nodiscard]] std::string format(fmt::format_string<Args...> fmt_s, Args&&... args) {
    return fmt::format(fmt_s, std::forward<Args>(args)...);
}

template<typename... Args>
void format_to(std::string& str, fmt::format_string<Args...> fmt_s, Args&&... args) {
    str.clear();
    fmt::format_to(std::back_inserter(str), fmt_s, std::forward<Args>(args)...);
}

template<typename... Args>
[[nodiscard]] std::size_t format_to_buffer(
    char* buffer,
    std::size_t size,
    fmt::format_string<Args...> fmt_s,
    Args&&... args)
{
    if (size == 0) return 0;

    try {
        auto result = fmt::format_to_n(buffer, size - 1, fmt_s, std::forward<Args>(args)...);
        buffer[result.size < size ? result.size : size - 1] = '\0';
        return result.size;
    } catch (...) {
        buffer[0] = '\0';
        return 0;
    }
}

#else

// Minimal fallback: use snprintf-style formatting. This does NOT support
// Python-style formatting ("{}") and is a degraded fallback for older
// compilers without std::format or {fmt}. It keeps the code compiling but
// callers should prefer environments with std::format or fmt available.
#include <cstdio>

template<typename... Args>
[[nodiscard]] std::string format(const char* fmt_cstr, Args&&... args) {
    // Estimate required size
    int size = std::snprintf(nullptr, 0, fmt_cstr, std::forward<Args>(args)...);
    if (size <= 0) return std::string();
    std::string out;
    out.resize(static_cast<std::size_t>(size));
    std::snprintf(out.data(), out.size() + 1, fmt_cstr, std::forward<Args>(args)...);
    return out;
}

template<typename... Args>
void format_to(std::string& str, const char* fmt_cstr, Args&&... args) {
    str = format(fmt_cstr, std::forward<Args>(args)...);
}

template<typename... Args>
[[nodiscard]] std::size_t format_to_buffer(
    char* buffer,
    std::size_t size,
    const char* fmt_cstr,
    Args&&... args)
{
    if (size == 0) return 0;
    int n = std::snprintf(buffer, size, fmt_cstr, std::forward<Args>(args)...);
    if (n < 0) {
        buffer[0] = '\0';
        return 0;
    }
    if (static_cast<std::size_t>(n) >= size) buffer[size - 1] = '\0';
    return static_cast<std::size_t>(n);
}

#endif

// ============================================================================
// Error Handling with std::expected
// ============================================================================

#ifdef VT_HAS_STD_EXPECTED
/**
 * @brief Result type for operations that can fail
 * 
 * Modern alternative to error codes and exceptions.
 * Forces caller to check for errors before using the value.
 * 
 * Example:
 *   Result<int> parse_number(const char* str) {
 *       if (!str) return Error("null pointer");
 *       // ... parsing logic ...
 *       return value;
 *   }
 *   
 *   auto result = parse_number("42");
 *   if (result) {
 *       int value = *result;  // Access value
 *   } else {
 *       std::string err = result.error();  // Access error
 *   }
 */
template<typename T>
using Result = std::expected<T, std::string>;

/**
 * @brief Create an error result
 */
template<typename T>
[[nodiscard]] Result<T> Error(std::string message) {
    return std::unexpected(std::move(message));
}

/**
 * @brief Create an error result with formatting
 */
template<typename T, typename... Args>
[[nodiscard]] Result<T> Error(std::format_string<Args...> fmt, Args&&... args) {
    return std::unexpected(std::format(fmt, std::forward<Args>(args)...));
}

/**
 * @brief Result type using std::error_code for system errors
 */
template<typename T>
using SystemResult = std::expected<T, std::error_code>;

#endif // VT_HAS_STD_EXPECTED

// ============================================================================
// Optional Utilities (C++23 Monadic Operations)
// ============================================================================

/**
 * @brief Get value from optional or compute default
 * 
 * Example:
 *   std::optional<int> opt;
 *   int value = opt.value_or(42);  // Returns 42 if empty
 */
// Note: std::optional::value_or already exists, this is just for completeness

/**
 * @brief Transform optional value if present (monadic and_then)
 * 
 * Example:
 *   std::optional<int> opt = 42;
 *   auto result = opt.and_then([](int x) -> std::optional<int> { 
 *       return x * 2; 
 *   });
 */
// Note: C++23 adds monadic operations directly to std::optional

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Check if a value is in a range (inclusive)
 */
template<typename T>
[[nodiscard]] constexpr bool in_range(T value, T min, T max) noexcept {
    return value >= min && value <= max;
}

/**
 * @brief Clamp a value to a range
 */
template<typename T>
[[nodiscard]] constexpr T clamp(T value, T min, T max) noexcept {
    return std::clamp(value, min, max);
}

} // namespace vt::cpp23

#endif // VT_CPP23_UTILS_HH
