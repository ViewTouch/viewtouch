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
 * vt_enum_utils.hh - Enum utilities using magic_enum
 * Provides automatic enum-to-string conversions and reflection
 */

#ifndef VT_ENUM_UTILS_HH
#define VT_ENUM_UTILS_HH

#include <magic_enum/magic_enum.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace vt {

/**
 * @brief Enum utilities wrapper around magic_enum
 * 
 * Provides convenient functions for enum reflection:
 * - Convert enum to string
 * - Parse string to enum
 * - Get all enum values
 * - Get enum count
 * 
 * Usage:
 *   enum class ButtonType { Normal, Zone, Goto, Item };
 *   
 *   auto name = EnumToString(ButtonType::Normal);  // "Normal"
 *   auto val = StringToEnum<ButtonType>("Zone");   // ButtonType::Zone
 *   auto all = GetEnumValues<ButtonType>();        // vector of all values
 */

/**
 * @brief Convert enum value to string name
 * @tparam E Enum type
 * @param value Enum value
 * @return String representation or empty string if invalid
 */
template<typename E>
std::string_view EnumToString(E value) {
    return magic_enum::enum_name(value);
}

/**
 * @brief Convert string to enum value
 * @tparam E Enum type
 * @param name String name of enum value
 * @return Optional enum value (nullopt if not found)
 */
template<typename E>
std::optional<E> StringToEnum(std::string_view name) {
    return magic_enum::enum_cast<E>(name);
}

/**
 * @brief Get all possible values for an enum
 * @tparam E Enum type
 * @return Array view of all enum values
 */
template<typename E>
constexpr auto GetEnumValues() {
    return magic_enum::enum_values<E>();
}

/**
 * @brief Get all names for an enum
 * @tparam E Enum type
 * @return Array view of all enum names
 */
template<typename E>
constexpr auto GetEnumNames() {
    return magic_enum::enum_names<E>();
}

/**
 * @brief Get count of enum values
 * @tparam E Enum type
 * @return Number of enum values
 */
template<typename E>
constexpr std::size_t GetEnumCount() {
    return magic_enum::enum_count<E>();
}

/**
 * @brief Check if a value is valid for an enum
 * @tparam E Enum type
 * @param value Value to check
 * @return true if value is a valid enum value
 */
template<typename E>
constexpr bool IsValidEnum(E value) {
    return magic_enum::enum_contains(value);
}

/**
 * @brief Get enum value from integer
 * @tparam E Enum type
 * @param value Integer value
 * @return Optional enum value
 */
template<typename E>
std::optional<E> IntToEnum(int value) {
    return magic_enum::enum_cast<E>(value);
}

/**
 * @brief Convert enum to integer
 * @tparam E Enum type
 * @param value Enum value
 * @return Integer representation
 */
template<typename E>
constexpr auto EnumToInt(E value) {
    return magic_enum::enum_integer(value);
}

/**
 * @brief Helper to create name-value pairs for UI dropdowns
 * @tparam E Enum type
 * @return Vector of pairs (name, value)
 */
template<typename E>
std::vector<std::pair<std::string, E>> GetEnumPairs() {
    std::vector<std::pair<std::string, E>> result;
    auto entries = magic_enum::enum_entries<E>();
    result.reserve(entries.size());
    
    for (const auto& [value, name] : entries) {
        result.emplace_back(std::string(name), value);
    }
    
    return result;
}

/**
 * @brief Format enum for display (converts underscores to spaces, capitalizes)
 * @tparam E Enum type
 * @param value Enum value
 * @return Formatted string
 */
template<typename E>
std::string EnumToDisplayString(E value) {
    std::string name(magic_enum::enum_name(value));
    
    // Convert to display format: MY_ENUM_VALUE -> My Enum Value
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] == '_') {
            name[i] = ' ';
            continue; // underscores fully handled
        }

        const bool word_start = (i == 0 || name[i - 1] == ' ');
        name[i] = word_start
            ? static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])))
            : static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
    }
    
    return name;
}

/**
 * @brief Create C-style array of enum names (for backwards compatibility)
 * Note: This creates a static array that lasts for program lifetime
 * @tparam E Enum type
 * @return Pointer to null-terminated array of C strings
 */
template<typename E>
const char** GetEnumNamesArray() {
    static std::vector<const char*> names;
    
    if (names.empty()) {
        auto enum_names = magic_enum::enum_names<E>();
        names.reserve(enum_names.size() + 1);
        
        for (const auto& name : enum_names) {
            names.push_back(name.data());
        }
        names.push_back(nullptr); // Null terminator
    }
    
    return names.data();
}

} // namespace vt

// Sales group type definition (moved from sales.hh for utilities)
enum SalesGroupType {
    SalesGroupUnused = 0,      // Don't use family
    SalesGroupFood = 1,
    SalesGroupBeverage = 2,
    SalesGroupBeer = 3,
    SalesGroupWine = 4,
    SalesGroupAlcohol = 5,
    SalesGroupMerchandise = 6,
    SalesGroupRoom = 7
};

// Sales group specific utilities (for backward compatibility)
namespace vt {
    /**
     * @brief Get display name for sales group type
     * @param group Sales group enum value
     * @return Translated display name
     */
    std::string GetSalesGroupDisplayName(SalesGroupType group);

    /**
     * @brief Get short name for sales group type
     * @param group Sales group enum value
     * @return Short display name
     */
    std::string GetSalesGroupShortName(SalesGroupType group);
}

// Convenience macros for common operations
#define VT_ENUM_TO_STRING(e) vt::EnumToString(e)
#define VT_STRING_TO_ENUM(T, s) vt::StringToEnum<T>(s)
#define VT_ENUM_COUNT(T) vt::GetEnumCount<T>()
#define VT_ENUM_VALUES(T) vt::GetEnumValues<T>()

#endif // VT_ENUM_UTILS_HH

