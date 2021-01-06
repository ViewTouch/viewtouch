#include "catch2/catch.hpp"
#include "report.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan, std::isinf
#include <locale>

TEST_CASE("report: ReportEntry: pass nullptr for text sets draw_a_line")
{
    int color = 0;
    int align = 0;
    int mode = 0;
    ReportEntry re(nullptr,color, align, mode);
    CHECK(re.text.empty());
    CHECK(re.draw_a_line == true);
    CHECK(re.color == color);
    CHECK(re.align == align);
    CHECK(re.mode  == mode);
}
TEST_CASE("report: ReportEntry: pass char nullptr as text sets draw_line")
{
    const char *text = nullptr;
    int color = 0;
    int align = 0;
    int mode = 0;
    REQUIRE_NOTHROW(ReportEntry(text, color, align, mode));
    ReportEntry re(text,color, align, mode);
    CHECK(re.text.empty());
    CHECK(re.draw_a_line == true);
    CHECK(re.color == color);
    CHECK(re.align == align);
    CHECK(re.mode  == mode);
}

TEST_CASE("report: ReportEntry: empty char string does not set draw_a_line")
{
    const char *text = "";
    int color = 0;
    int align = 0;
    int mode = 0;
    REQUIRE_NOTHROW(ReportEntry(text, color, align, mode));
    ReportEntry re(text,color, align, mode);
    CHECK(re.text.empty());
    CHECK(re.draw_a_line == false);
}
TEST_CASE("report: ReportEntry: empty std::string does not set draw_a_line")
{
    const std::string text = "";
    int color = 0;
    int align = 0;
    int mode = 0;
    REQUIRE_NOTHROW(ReportEntry(text, color, align, mode));
    ReportEntry re(text,color, align, mode);
    CHECK(re.text.empty());
    CHECK(re.draw_a_line == false);
}

TEST_CASE("report: ReportEntry: char entries creating no line")
{
    auto text = GENERATE(as<const char *>{},
        "a",
        "12",
        "11144",
        "loads of text with spaces",
        "nothing to see here!");
    int color = 0;
    int align = 0;
    int mode = 0;
    ReportEntry re(text,color, align, mode);
    CHECK(re.text.empty() == false);
    CHECK(re.draw_a_line == false);
    CHECK(re.color == color);
    CHECK(re.align == align);
    CHECK(re.mode  == mode);
}
TEST_CASE("report: ReportEntry: std::string entries creating no line")
{
    auto text = GENERATE(as<std::string>{},
        "a",
        "12",
        "11144",
        "loads of text with spaces",
        "nothing to see here!");
    int color = 0;
    int align = 0;
    int mode = 0;
    ReportEntry re(text,color, align, mode);
    CHECK(re.text.empty() == false);
    CHECK(re.draw_a_line == false);
    CHECK(re.color == color);
    CHECK(re.align == align);
    CHECK(re.mode  == mode);
}