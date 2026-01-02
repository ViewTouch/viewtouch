/*
 * Copyright ViewTouch, Inc., 2025
 *
 * cpp23_examples.cc - Examples of C++23 usage in ViewTouch
 * 
 * This file demonstrates how to use the modern C++23 features provided
 * by cpp23_utils.hh throughout the ViewTouch codebase.
 */

#include "cpp23_utils.hh"
#include "settings_enums.hh"
#include <iostream>
#include <optional>

using namespace vt::cpp23;

// ============================================================================
// Example 1: Enum to Underlying Type Conversion
// ============================================================================

// OLD WAY (C++17/C++20):
void old_enum_conversion() {
    DrawerModeType mode = DrawerModeType::Server;
    int mode_value = static_cast<int>(mode);  // Verbose and error-prone
    
    ReceiptPrintType print = ReceiptPrintType::Full;
    auto print_value = static_cast<std::underlying_type_t<ReceiptPrintType>>(print);
}

// NEW WAY (C++23):
void modern_enum_conversion() {
    DrawerModeType mode = DrawerModeType::Server;
    auto mode_value = to_underlying(mode);  // Clean, type-safe, readable
    
    ReceiptPrintType print = ReceiptPrintType::Full;
    auto print_value = to_underlying(print);  // Works with any enum type
}

// ============================================================================
// Example 2: String Formatting
// ============================================================================

// OLD WAY (using snprintf):
void old_string_formatting() {
    char buffer[256];
    int account_no = 42;
    int total = 100;
    
    snprintf(buffer, sizeof(buffer), "Account %d of %d", account_no, total);
    // Problems: buffer overflow risk, no type safety, hard to read
    
    double price = 19.99;
    snprintf(buffer, sizeof(buffer), "Price: $%.2f", price);
}

// NEW WAY (C++23 std::format):
void modern_string_formatting() {
    int account_no = 42;
    int total = 100;
    
    // Compile-time format checking, automatic memory management
    auto message = format("Account {} of {}", account_no, total);
    
    double price = 19.99;
    auto price_str = format("Price: ${:.2f}", price);
    
    // For performance-critical code, use stack buffer:
    char buffer[256];
    format_to_buffer(buffer, sizeof(buffer), "Account {} of {}", account_no, total);
}

// ============================================================================
// Example 3: Error Handling with std::expected
// ============================================================================

// OLD WAY (using return codes or exceptions):
int old_parse_account_number(const char* str) {
    if (!str) return -1;  // Error code
    if (strlen(str) == 0) return -1;
    
    // Parse...
    int value = atoi(str);
    if (value == 0 && str[0] != '0') return -1;  // Parse failed
    
    return value;
}

// NEW WAY (C++23 std::expected):
Result<int> parse_account_number(const char* str) {
    if (!str) 
        return Error<int>("Account number cannot be null");
    if (strlen(str) == 0) 
        return Error<int>("Account number cannot be empty");
    
    char* endptr;
    long value = strtol(str, &endptr, 10);
    
    if (endptr == str)
        return Error<int>("Invalid account number format: '{}'", str);
    if (value < 0 || value > 999999)
        return Error<int>("Account number {} out of range (0-999999)", value);
    
    return static_cast<int>(value);
}

// Usage example:
void use_expected_result() {
    auto result = parse_account_number("12345");
    
    if (result) {
        // Success path
        int account_num = *result;
        std::cout << "Parsed account: " << account_num << "\n";
    } else {
        // Error path
        std::cout << "Error: " << result.error() << "\n";
    }
    
    // Can also use value_or for fallback
    int account = result.value_or(0);
    
    // Or transform the value if present (monadic operations)
    auto doubled = result.transform([](int x) { return x * 2; });
}

// ============================================================================
// Example 4: Unreachable Code Paths
// ============================================================================

const char* get_drawer_mode_name(DrawerModeType mode) {
    switch (mode) {
        case DrawerModeType::None:     return "None";
        case DrawerModeType::Server:   return "Server";
        case DrawerModeType::Assigned: return "Assigned";
        case DrawerModeType::Shared:   return "Shared";
    }
    
    // All cases handled, this path should never execute
    unreachable();  // Better than return nullptr or throw
}

// ============================================================================
// Example 5: Combining C++23 Features
// ============================================================================

// Modern function combining std::expected, std::format, and to_underlying
Result<std::string> format_drawer_config(DrawerModeType mode, int drawer_id) {
    if (drawer_id < 0 || drawer_id > 255)
        return Error<std::string>("Invalid drawer ID: {}", drawer_id);
    
    const char* mode_name = get_drawer_mode_name(mode);
    auto mode_value = to_underlying(mode);
    
    return format("Drawer #{} Mode: {} ({})", drawer_id, mode_name, mode_value);
}

// Usage:
void demonstrate_combined_features() {
    auto result = format_drawer_config(DrawerModeType::Server, 1);
    
    if (result) {
        std::cout << *result << "\n";
        // Prints: "Drawer #1 Mode: Server (1)"
    } else {
        std::cout << "Configuration error: " << result.error() << "\n";
    }
}

// ============================================================================
// Example 6: Safe Range Checking
// ============================================================================

Result<int> validate_table_number(int table_num) {
    constexpr int MIN_TABLE = 1;
    constexpr int MAX_TABLE = 100;
    
    if (!in_range(table_num, MIN_TABLE, MAX_TABLE)) {
        return Error<int>("Table number {} out of valid range ({}-{})", 
                         table_num, MIN_TABLE, MAX_TABLE);
    }
    
    return table_num;
}

// ============================================================================
// Benefits of C++23 in ViewTouch
// ============================================================================

/*
 * 1. TYPE SAFETY
 *    - std::format catches format errors at compile time
 *    - std::to_underlying prevents casting errors
 *    - std::expected makes error handling explicit
 *
 * 2. PERFORMANCE
 *    - format_to_buffer uses stack allocation (zero heap allocations)
 *    - to_underlying is constexpr (no runtime cost)
 *    - unreachable() enables better optimization
 *
 * 3. READABILITY
 *    - Format strings are clearer than printf
 *    - Expected makes error paths obvious
 *    - Intent is explicit in code
 *
 * 4. SAFETY
 *    - No buffer overflows with std::format
 *    - Forced error checking with std::expected
 *    - Compile-time validation where possible
 *
 * 5. MAINTAINABILITY
 *    - Less boilerplate code
 *    - Self-documenting error handling
 *    - Fewer bugs from manual buffer management
 */

// ============================================================================
// Migration Strategy
// ============================================================================

/*
 * PHASE 1: New code
 *   - Use cpp23_utils.hh in all new features
 *   - Establish patterns and best practices
 *
 * PHASE 2: Critical paths
 *   - Update string formatting in security-sensitive code
 *   - Add std::expected to file I/O operations
 *   - Replace manual buffer management
 *
 * PHASE 3: Gradual migration
 *   - Convert sprintf/snprintf when touching code
 *   - Add to_underlying when modifying enum code
 *   - Refactor error handling incrementally
 *
 * PHASE 4: Complete modernization
 *   - Systematic conversion of remaining code
 *   - Remove legacy patterns
 *   - Update coding standards
 */

int main() {
    std::cout << "C++23 Examples for ViewTouch\n";
    std::cout << "============================\n\n";
    
    std::cout << "Example 1: Enum conversion\n";
    modern_enum_conversion();
    
    std::cout << "\nExample 2: String formatting\n";
    modern_string_formatting();
    
    std::cout << "\nExample 3: Error handling\n";
    use_expected_result();
    
    std::cout << "\nExample 4: Combined features\n";
    demonstrate_combined_features();
    
    return 0;
}
