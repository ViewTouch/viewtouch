#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <string>
#include <vector>

TEST_CASE("Catch2 integration test", "[catch2]") {
    SECTION("Basic assertions") {
        REQUIRE(true == true);
        REQUIRE(1 + 1 == 2);
        REQUIRE(std::string("hello") == std::string("hello"));
        REQUIRE(3.14 == Catch::Approx(3.14));
    }

    SECTION("String operations") {
        std::string test = "Hello World";
        REQUIRE(test.length() == 11);
        REQUIRE(test.find("Hello") == 0);
        REQUIRE(test.find("World") == 6);
    }

    SECTION("Vector operations") {
        std::vector<int> numbers = {1, 2, 3, 4, 5};
        REQUIRE(numbers.size() == 5);
        REQUIRE(numbers[0] == 1);
        REQUIRE(numbers[4] == 5);
        REQUIRE(numbers.back() == 5);
    }

    SECTION("Floating point comparisons") {
        double pi = 3.14159;
        REQUIRE(pi == Catch::Approx(3.14159).margin(0.0001));
        REQUIRE(pi > 3.0);
        REQUIRE(pi < 4.0);
    }
} 