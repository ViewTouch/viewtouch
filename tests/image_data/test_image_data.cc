#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "image_data.hh"


TEST_CASE("number_of_all_colors", "[image_data]")
{
    CHECK(ImageColorsUsed() == 1729);
}

TEST_CASE("resolution_image_sand", "[image_data]")
{
    // test multiple times
    CHECK(ImageWidth(IMAGE_SAND) == 256);
    CHECK(ImageWidth(IMAGE_SAND) == 256);
    // test multiple times
    CHECK(ImageHeight(IMAGE_SAND) == 256);
    CHECK(ImageHeight(IMAGE_SAND) == 256);
}
