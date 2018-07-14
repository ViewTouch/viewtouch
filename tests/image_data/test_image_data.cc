#include "gtest/gtest.h"
#include "image_data.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan
#include <locale>

TEST(image_data, number_of_all_colors)
{
    EXPECT_EQ(ImageColorsUsed(), 1729);
}

TEST(image_data, resolution_image_sand)
{
    // test multiple times
    EXPECT_EQ(ImageWidth(IMAGE_SAND), 256);
    EXPECT_EQ(ImageWidth(IMAGE_SAND), 256);
    // test multiple times
    EXPECT_EQ(ImageHeight(IMAGE_SAND), 256);
    EXPECT_EQ(ImageHeight(IMAGE_SAND), 256);
}
