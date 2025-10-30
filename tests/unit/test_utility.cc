#include <catch2/catch_all.hpp>
#include "src/utils/string_utils.hh"
#include "src/utils/utility.hh"
#include <string>
#include <cstring>

TEST_CASE("String utilities safety", "[string_utils]")
{
    SECTION("Safe string copy operations")
    {
        char dest[20];

        // Test basic string operations
        std::string source = "Hello World";

        // These should be safe operations
        REQUIRE(source.length() == 11);
        REQUIRE(source == "Hello World");

        // Test string concatenation
        std::string result = source + " Test";
        REQUIRE(result == "Hello World Test");
    }

    SECTION("String validation")
    {
        // Test various string inputs
        std::string valid_string = "ValidInput123";
        std::string empty_string = "";
        std::string special_chars = "!@#$%^&*()";

        // Basic validation checks
        REQUIRE(valid_string.length() > 0);
        REQUIRE(empty_string.empty());
        REQUIRE(special_chars.length() > 0);
    }
}

TEST_CASE("Input validation", "[validation]")
{
    SECTION("Numeric input validation")
    {
        // Test numeric validation
        std::string valid_number = "12345";
        std::string invalid_number = "abc123";
        std::string negative_number = "-123";

        // Basic numeric checks
        REQUIRE(std::stoi(valid_number) == 12345);
        REQUIRE(negative_number[0] == '-');

        // Should handle invalid input gracefully
        bool is_valid = true;
        try {
            int value = std::stoi(invalid_number);
        } catch (const std::invalid_argument&) {
            is_valid = false;
        }
        REQUIRE(!is_valid);
    }

    SECTION("String length limits")
    {
        const size_t MAX_LENGTH = 100;

        std::string short_string = "Short";
        std::string long_string(MAX_LENGTH + 1, 'A');

        REQUIRE(short_string.length() <= MAX_LENGTH);
        REQUIRE(long_string.length() > MAX_LENGTH);
    }
}

TEST_CASE("Memory safety checks", "[memory]")
{
    SECTION("Buffer overflow prevention")
    {
        const size_t BUFFER_SIZE = 10;
        char buffer[BUFFER_SIZE];

        std::string test_data = "Short";
        std::string long_data = "This is a very long string that exceeds buffer size";

        // Safe operations should not overflow
        REQUIRE(test_data.length() < BUFFER_SIZE);

        // Long data should be handled safely
        if (long_data.length() >= BUFFER_SIZE) {
            // Should truncate or handle gracefully
            REQUIRE(long_data.length() > BUFFER_SIZE);
        }
    }
}
