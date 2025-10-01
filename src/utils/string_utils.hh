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
 * string_utils.hh - Modern string processing utilities
 * Consolidated and modernized string functions
 */

#ifndef VT_STRING_UTILS_HH
#define VT_STRING_UTILS_HH

#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace vt_string {

    // Safe string formatting (replacement for sprintf)
    template<typename... Args>
    std::string format(const std::string& format, Args... args) {
        constexpr size_t buffer_size = 1024;
        std::array<char, buffer_size> buffer{};
        
        int result = std::snprintf(buffer.data(), buffer.size(), format.c_str(), args...);
        if (result < 0) {
            return {};
        }
        
        if (static_cast<size_t>(result) < buffer.size()) {
            return std::string(buffer.data(), result);
        }
        
        // Buffer was too small, use dynamic allocation
        std::vector<char> dynamic_buffer(result + 1);
        std::snprintf(dynamic_buffer.data(), dynamic_buffer.size(), format.c_str(), args...);
        return std::string(dynamic_buffer.data(), result);
    }

    // String case conversion
    std::string to_upper(const std::string& str);
    std::string to_lower(const std::string& str);
    std::string to_title_case(const std::string& str);

    // String trimming and whitespace handling
    std::string trim_left(const std::string& str);
    std::string trim_right(const std::string& str);
    std::string trim(const std::string& str);
    std::string normalize_spaces(const std::string& str);

    // String searching and comparison
    bool contains(const std::string& haystack, const std::string& needle, bool case_sensitive = true);
    bool starts_with(const std::string& str, const std::string& prefix);
    bool ends_with(const std::string& str, const std::string& suffix);
    int compare_ignore_case(const std::string& str1, const std::string& str2);

    // String splitting and joining
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

    // String replacement
    std::string replace_all(const std::string& str, const std::string& from, const std::string& to);
    std::string replace_first(const std::string& str, const std::string& from, const std::string& to);

    // Numeric conversions with error checking
    template<typename T>
    bool try_parse(const std::string& str, T& result) {
        std::istringstream iss(str);
        iss >> result;
        return !iss.fail() && iss.eof();
    }

    // Price formatting (ViewTouch specific)
    std::string format_price(int price_cents, bool show_sign = false, bool use_comma = true);
    bool parse_price(const std::string& str, int& price_cents);

    // File path utilities
    std::string get_filename(const std::string& path);
    std::string get_directory(const std::string& path);
    std::string get_extension(const std::string& path);
    std::string combine_paths(const std::string& path1, const std::string& path2);

    // Validation helpers
    bool is_numeric(const std::string& str);
    bool is_alpha(const std::string& str);
    bool is_alphanumeric(const std::string& str);
    bool is_email(const std::string& str);

} // namespace vt_string

#endif // VT_STRING_UTILS_HH
