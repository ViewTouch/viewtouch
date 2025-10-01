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
 * string_utils.cc - Implementation of modern string processing utilities
 */

#include "string_utils.hh"
#include <regex>
#include <filesystem>

namespace vt_string {

    std::string to_upper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    std::string to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    std::string to_title_case(const std::string& str) {
        std::string result = str;
        bool capitalize_next = true;
        
        for (char& c : result) {
            if (std::isspace(c) || std::ispunct(c)) {
                capitalize_next = true;
            } else if (capitalize_next) {
                c = std::toupper(c);
                capitalize_next = false;
            } else {
                c = std::tolower(c);
            }
        }
        return result;
    }

    std::string trim_left(const std::string& str) {
        auto start = str.find_first_not_of(" \t\r\n");
        return (start == std::string::npos) ? "" : str.substr(start);
    }

    std::string trim_right(const std::string& str) {
        auto end = str.find_last_not_of(" \t\r\n");
        return (end == std::string::npos) ? "" : str.substr(0, end + 1);
    }

    std::string trim(const std::string& str) {
        return trim_left(trim_right(str));
    }

    std::string normalize_spaces(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        
        bool in_space = false;
        for (char c : str) {
            if (std::isspace(c)) {
                if (!in_space && !result.empty()) {
                    result.push_back(' ');
                    in_space = true;
                }
            } else {
                result.push_back(c);
                in_space = false;
            }
        }
        
        return result;
    }

    bool contains(const std::string& haystack, const std::string& needle, bool case_sensitive) {
        if (case_sensitive) {
            return haystack.find(needle) != std::string::npos;
        } else {
            return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
        }
    }

    bool starts_with(const std::string& str, const std::string& prefix) {
        return str.length() >= prefix.length() && 
               str.compare(0, prefix.length(), prefix) == 0;
    }

    bool ends_with(const std::string& str, const std::string& suffix) {
        return str.length() >= suffix.length() && 
               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    int compare_ignore_case(const std::string& str1, const std::string& str2) {
        return to_lower(str1).compare(to_lower(str2));
    }

    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        
        while (std::getline(ss, item, delimiter)) {
            result.push_back(item);
        }
        
        return result;
    }

    std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = str.find(delimiter);
        
        while (end != std::string::npos) {
            result.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }
        
        result.push_back(str.substr(start));
        return result;
    }

    std::string join(const std::vector<std::string>& strings, const std::string& delimiter) {
        if (strings.empty()) return "";
        
        std::string result = strings[0];
        for (size_t i = 1; i < strings.size(); ++i) {
            result += delimiter + strings[i];
        }
        
        return result;
    }

    std::string replace_all(const std::string& str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        
        std::string result = str;
        size_t pos = 0;
        
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        
        return result;
    }

    std::string replace_first(const std::string& str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        
        std::string result = str;
        size_t pos = result.find(from);
        
        if (pos != std::string::npos) {
            result.replace(pos, from.length(), to);
        }
        
        return result;
    }

    std::string format_price(int price_cents, bool show_sign, bool use_comma) {
        bool negative = price_cents < 0;
        if (negative) price_cents = -price_cents;
        
        int dollars = price_cents / 100;
        int cents = price_cents % 100;
        
        std::string result;
        if (show_sign) {
            result += negative ? "-$" : "$";
        } else {
            result += "$";
        }
        
        std::string dollar_str = std::to_string(dollars);
        if (use_comma && dollars >= 1000) {
            // Add commas for thousands
            std::string formatted;
            int count = 0;
            for (auto it = dollar_str.rbegin(); it != dollar_str.rend(); ++it) {
                if (count > 0 && count % 3 == 0) {
                    formatted = "," + formatted;
                }
                formatted = *it + formatted;
                count++;
            }
            result += formatted;
        } else {
            result += dollar_str;
        }
        
        result += format(".%02d", cents);
        return result;
    }

    bool parse_price(const std::string& str, int& price_cents) {
        std::string cleaned = trim(str);
        if (cleaned.empty()) return false;
        
        // Extract sign information regardless of whether the currency symbol
        // appears before or after it (e.g. "$-12.34" or "-$12.34").
        bool negative = false;

        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '$'), cleaned.end());
        cleaned = trim(cleaned);

        if (!cleaned.empty() && (cleaned[0] == '+' || cleaned[0] == '-')) {
            negative = cleaned[0] == '-';
            cleaned = trim(cleaned.substr(1));
        }
        
        // Remove commas
        cleaned = replace_all(cleaned, ",", "");
        
        // Find decimal point
        size_t decimal_pos = cleaned.find('.');
        
        int dollars = 0;
        int cents = 0;
        
        if (decimal_pos == std::string::npos) {
            // No decimal point - assume whole dollars
            if (!try_parse(cleaned, dollars)) return false;
        } else {
            // Has decimal point
            std::string dollar_part = cleaned.substr(0, decimal_pos);
            std::string cent_part = cleaned.substr(decimal_pos + 1);
            
            if (!dollar_part.empty() && !try_parse(dollar_part, dollars)) return false;
            
            if (cent_part.length() > 2) return false; // Too many decimal places
            if (cent_part.length() == 1) cent_part += "0"; // Pad single digit
            if (!cent_part.empty() && !try_parse(cent_part, cents)) return false;
        }
        
        price_cents = dollars * 100 + cents;
        if (negative) price_cents = -price_cents;
        
        return true;
    }

    std::string get_filename(const std::string& path) {
        return std::filesystem::path(path).filename().string();
    }

    std::string get_directory(const std::string& path) {
        return std::filesystem::path(path).parent_path().string();
    }

    std::string get_extension(const std::string& path) {
        return std::filesystem::path(path).extension().string();
    }

    std::string combine_paths(const std::string& path1, const std::string& path2) {
        return (std::filesystem::path(path1) / path2).string();
    }

    bool is_numeric(const std::string& str) {
        if (str.empty()) return false;
        
        auto start = str.begin();
        if (*start == '+' || *start == '-') ++start;
        
        return std::all_of(start, str.end(), [](char c) { return std::isdigit(c); });
    }

    bool is_alpha(const std::string& str) {
        return !str.empty() && std::all_of(str.begin(), str.end(), 
                                          [](char c) { return std::isalpha(c); });
    }

    bool is_alphanumeric(const std::string& str) {
        return !str.empty() && std::all_of(str.begin(), str.end(), 
                                          [](char c) { return std::isalnum(c); });
    }

    bool is_email(const std::string& str) {
        const std::regex email_pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
        return std::regex_match(str, email_pattern);
    }

} // namespace vt_string
