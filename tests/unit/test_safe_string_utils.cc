#include <catch2/catch_all.hpp>
#include "src/utils/safe_string_utils.hh"
#include <cstring>
#include <string>

TEST_CASE("Safe String Copy Operations", "[safe_string]")
{
    SECTION("Basic safe_copy functionality")
    {
        char buffer[20];

        // Test successful copy
        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), "Hello World");
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Hello World") == 0);
    }

    SECTION("Copy with truncation")
    {
        char buffer[10];

        // Test truncation (source longer than destination)
        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), "This is a very long string");
        REQUIRE(result == false); // Should return false for truncation
        REQUIRE(std::strlen(buffer) == 9); // Null terminator takes one char
        REQUIRE(buffer[9] == '\0'); // Null terminated
    }

    SECTION("Copy empty string")
    {
        char buffer[20];

        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), "");
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "") == 0);
    }

    SECTION("Copy with std::string")
    {
        char buffer[20];
        std::string source = "Hello World";

        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), source);
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Hello World") == 0);
    }

    SECTION("Null pointer protection")
    {
        char buffer[20];

        // Test with null destination
        bool result = vt_safe_string::safe_copy(nullptr, sizeof(buffer), "test");
        REQUIRE(result == false);

        // Test with null source
        result = vt_safe_string::safe_copy(buffer, sizeof(buffer), nullptr);
        REQUIRE(result == false);

        // Test with zero size
        result = vt_safe_string::safe_copy(buffer, 0, "test");
        REQUIRE(result == false);
    }
}

TEST_CASE("Safe String Concatenation", "[safe_string]")
{
    SECTION("Basic concatenation")
    {
        char buffer[20] = "Hello";

        bool result = vt_safe_string::safe_concat(buffer, sizeof(buffer), " World");
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Hello World") == 0);
    }

    SECTION("Concatenation with truncation")
    {
        char buffer[15] = "Hello";

        bool result = vt_safe_string::safe_concat(buffer, sizeof(buffer), " World this is long");
        REQUIRE(result == false); // Should be truncated
        REQUIRE(std::strlen(buffer) == 14); // Full buffer used
        REQUIRE(buffer[14] == '\0'); // Null terminated
    }

    SECTION("Concatenation with std::string")
    {
        char buffer[20] = "Hello";

        bool result = vt_safe_string::safe_concat(buffer, sizeof(buffer), std::string(" World"));
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Hello World") == 0);
    }
}

TEST_CASE("Safe String Formatting", "[safe_string]")
{
    SECTION("Basic formatting")
    {
        char buffer[50];

        bool result = vt_safe_string::safe_format(buffer, sizeof(buffer), "Value: %d, Text: %s", 42, "test");
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Value: 42, Text: test") == 0);
    }

    SECTION("Formatting with truncation")
    {
        char buffer[20];

        bool result = vt_safe_string::safe_format(buffer, sizeof(buffer), "This is a very long formatted string: %d", 12345);
        REQUIRE(result == false); // Should be truncated
        REQUIRE(std::strlen(buffer) == 19); // Buffer fully used minus null
        REQUIRE(buffer[19] == '\0'); // Null terminated
    }

    SECTION("Safe format string (automatic sizing)")
    {
        std::string result = vt_safe_string::safe_format_string("Value: %d, Text: %s", 42, "test");
        REQUIRE(result == "Value: 42, Text: test");

        // Test with std::string format
        std::string format_str = "Count: %d";
        result = vt_safe_string::safe_format_string(format_str, 100);
        REQUIRE(result == "Count: 100");
    }

    SECTION("Format error handling")
    {
        char buffer[20];

        // Test with null format
        bool result = vt_safe_string::safe_format(buffer, sizeof(buffer), nullptr, 42);
        REQUIRE(result == false);
    }
}

TEST_CASE("Safe Numeric Conversion", "[safe_string]")
{
    SECTION("Integer conversion")
    {
        int result;

        bool success = vt_safe_string::safe_numeric_convert("123", result);
        REQUIRE(success == true);
        REQUIRE(result == 123);

        success = vt_safe_string::safe_numeric_convert("abc", result);
        REQUIRE(success == false);

        success = vt_safe_string::safe_numeric_convert(nullptr, result);
        REQUIRE(success == false);
    }

    SECTION("Float conversion")
    {
        float result;

        bool success = vt_safe_string::safe_numeric_convert("123.45", result);
        REQUIRE(success == true);
        REQUIRE(result == 123.45f);

        success = vt_safe_string::safe_numeric_convert("not_a_number", result);
        REQUIRE(success == false);
    }
}

TEST_CASE("Safe Character Access", "[safe_string]")
{
    SECTION("C-string character access")
    {
        const char* str = "Hello World";

        char ch = vt_safe_string::safe_char_at(str, 0);
        REQUIRE(ch == 'H');

        ch = vt_safe_string::safe_char_at(str, 6);
        REQUIRE(ch == 'W');

        ch = vt_safe_string::safe_char_at(str, 100, 'X'); // Out of bounds
        REQUIRE(ch == 'X');

        ch = vt_safe_string::safe_char_at(nullptr, 0, 'Y'); // Null string
        REQUIRE(ch == 'Y');
    }

    SECTION("std::string character access")
    {
        std::string str = "Hello World";

        char ch = vt_safe_string::safe_char_at(str, 0);
        REQUIRE(ch == 'H');

        ch = vt_safe_string::safe_char_at(str, 10); // Last character
        REQUIRE(ch == 'd');

        ch = vt_safe_string::safe_char_at(str, 100, 'Z'); // Out of bounds
        REQUIRE(ch == 'Z');
    }
}

TEST_CASE("Safe Substring Operations", "[safe_string]")
{
    SECTION("C-string substring")
    {
        const char* str = "Hello World";

        std::string result = vt_safe_string::safe_substring(str, 6, 5);
        REQUIRE(result == "World");

        result = vt_safe_string::safe_substring(str, 0, 5);
        REQUIRE(result == "Hello");

        result = vt_safe_string::safe_substring(str, 100); // Out of bounds
        REQUIRE(result.empty());

        result = vt_safe_string::safe_substring(nullptr, 0); // Null string
        REQUIRE(result.empty());
    }

    SECTION("std::string substring")
    {
        std::string str = "Hello World";

        std::string result = vt_safe_string::safe_substring(str, 6, 5);
        REQUIRE(result == "World");

        result = vt_safe_string::safe_substring(str, 6); // To end
        REQUIRE(result == "World");
    }
}

TEST_CASE("Safe String Comparison", "[safe_string]")
{
    SECTION("C-string comparison")
    {
        const char* str1 = "Hello";
        const char* str2 = "Hello";
        const char* str3 = "World";

        REQUIRE(vt_safe_string::safe_equals(str1, str2) == true);
        REQUIRE(vt_safe_string::safe_equals(str1, str3) == false);
        REQUIRE(vt_safe_string::safe_equals(nullptr, nullptr) == true);
        REQUIRE(vt_safe_string::safe_equals(str1, nullptr) == false);
        REQUIRE(vt_safe_string::safe_equals(nullptr, str1) == false);
    }

    SECTION("std::string comparison")
    {
        std::string str1 = "Hello";
        std::string str2 = "Hello";
        std::string str3 = "World";

        REQUIRE(vt_safe_string::safe_equals(str1, str2) == true);
        REQUIRE(vt_safe_string::safe_equals(str1, str3) == false);
    }

    SECTION("Mixed comparison")
    {
        const char* c_str = "Hello";
        std::string cpp_str = "Hello";
        std::string diff_str = "World";

        REQUIRE(vt_safe_string::safe_equals(c_str, cpp_str) == true);
        REQUIRE(vt_safe_string::safe_equals(cpp_str, c_str) == true);
        REQUIRE(vt_safe_string::safe_equals(c_str, diff_str) == false);
        REQUIRE(vt_safe_string::safe_equals(nullptr, cpp_str) == false);
    }
}

TEST_CASE("Integration Tests", "[safe_string]")
{
    SECTION("Complex string operations")
    {
        char buffer[100];

        // Start with base string
        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), "Base");
        REQUIRE(result == true);

        // Concatenate
        result = vt_safe_string::safe_concat(buffer, sizeof(buffer), " string");
        REQUIRE(result == true);
        REQUIRE(std::strcmp(buffer, "Base string") == 0);

        // Format into another buffer
        char formatted[50];
        result = vt_safe_string::safe_format(formatted, sizeof(formatted), "Result: %s (length: %zu)",
                                           buffer, std::strlen(buffer));
        REQUIRE(result == true);
        REQUIRE(std::string(formatted).find("Result: Base string") != std::string::npos);
    }

    SECTION("Error recovery")
    {
        char buffer[10];

        // Attempt operation that would overflow
        bool result = vt_safe_string::safe_copy(buffer, sizeof(buffer), "This is way too long for this buffer");
        REQUIRE(result == false);

        // Buffer should still be null-terminated and safe to use
        REQUIRE(std::strlen(buffer) < sizeof(buffer));
        REQUIRE(buffer[sizeof(buffer) - 1] == '\0');

        // Should be able to use the truncated result safely
        std::string safe_copy = buffer;
        REQUIRE(safe_copy.length() == std::strlen(buffer));
    }
}
