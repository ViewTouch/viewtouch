/*
 * Unit tests for Report generation and formatting
 * Note: These test the ReportEntry class which is header-only
 */

#include <catch2/catch_test_macros.hpp>
#include "report.hh"
#include <string>

TEST_CASE("ReportEntry Construction", "[report][entry]")
{
    SECTION("Create entry with const char*")
    {
        ReportEntry entry("Test", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.text == "Test");
        REQUIRE(entry.color == COLOR_DEFAULT);
        REQUIRE(entry.align == ALIGN_LEFT);
        REQUIRE(entry.mode == 0);
    }
    
    SECTION("Create entry with std::string")
    {
        std::string text = "String entry";
        ReportEntry entry(text, COLOR_BLUE, ALIGN_CENTER, PRINT_BOLD);
        REQUIRE(entry.text == "String entry");
        REQUIRE(entry.color == COLOR_BLUE);
        REQUIRE(entry.align == ALIGN_CENTER);
        REQUIRE(entry.mode == PRINT_BOLD);
    }
    
    SECTION("Empty entry")
    {
        ReportEntry entry("", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.text.empty());
    }
    
    SECTION("Entry with different colors")
    {
        ReportEntry red("Red", COLOR_RED, ALIGN_LEFT, 0);
        ReportEntry blue("Blue", COLOR_BLUE, ALIGN_LEFT, 0);
        ReportEntry white("White", COLOR_WHITE, ALIGN_LEFT, 0);
        
        REQUIRE(red.color == COLOR_RED);
        REQUIRE(blue.color == COLOR_BLUE);
        REQUIRE(white.color == COLOR_WHITE);
    }
    
    SECTION("Entry with different alignments")
    {
        ReportEntry left("Left", COLOR_DEFAULT, ALIGN_LEFT, 0);
        ReportEntry center("Center", COLOR_DEFAULT, ALIGN_CENTER, 0);
        ReportEntry right("Right", COLOR_DEFAULT, ALIGN_RIGHT, 0);
        
        REQUIRE(left.align == ALIGN_LEFT);
        REQUIRE(center.align == ALIGN_CENTER);
        REQUIRE(right.align == ALIGN_RIGHT);
    }
    
    SECTION("Entry with different modes")
    {
        ReportEntry bold("Bold", COLOR_DEFAULT, ALIGN_LEFT, PRINT_BOLD);
        ReportEntry underline("Under", COLOR_DEFAULT, ALIGN_LEFT, PRINT_UNDERLINE);
        ReportEntry wide("Wide", COLOR_DEFAULT, ALIGN_LEFT, PRINT_WIDE);
        
        REQUIRE(bold.mode == PRINT_BOLD);
        REQUIRE(underline.mode == PRINT_UNDERLINE);
        REQUIRE(wide.mode == PRINT_WIDE);
    }
}

TEST_CASE("ReportEntry Position and Formatting", "[report][entry][format]")
{
    SECTION("Default position")
    {
        ReportEntry entry("Test", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.pos == 0.0f);
    }
    
    SECTION("Max length default")
    {
        ReportEntry entry("Test", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.max_len == 256);
    }
    
    SECTION("New lines default")
    {
        ReportEntry entry("Test", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.new_lines == 0);
    }
    
    SECTION("Line drawing flag")
    {
        ReportEntry entry("Test", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE_FALSE(entry.draw_a_line);
        
        entry.draw_a_line = true;
        REQUIRE(entry.draw_a_line);
    }
}

TEST_CASE("ReportEntry Long Text", "[report][entry][text]")
{
    SECTION("Very long text")
    {
        std::string long_text(1000, 'A');
        ReportEntry entry(long_text, COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.text.length() == 1000);
    }
    
    SECTION("Text with special characters")
    {
        ReportEntry entry("Special: \n\t\"chars\"", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE(entry.text.find('\n') != std::string::npos);
        REQUIRE(entry.text.find('\t') != std::string::npos);
    }
    
    SECTION("Unicode text")
    {
        ReportEntry entry("€£¥", COLOR_DEFAULT, ALIGN_LEFT, 0);
        REQUIRE_FALSE(entry.text.empty());
    }
}

TEST_CASE("ReportEntry Combined Modes", "[report][entry][modes]")
{
    SECTION("Bold and underline")
    {
        ReportEntry entry("Bold+Under", COLOR_DEFAULT, ALIGN_LEFT, 
                         PRINT_BOLD | PRINT_UNDERLINE);
        REQUIRE((entry.mode & PRINT_BOLD) != 0);
        REQUIRE((entry.mode & PRINT_UNDERLINE) != 0);
    }
    
    SECTION("Large mode (wide + tall)")
    {
        ReportEntry entry("Large", COLOR_DEFAULT, ALIGN_CENTER, PRINT_LARGE);
        REQUIRE(entry.mode == PRINT_LARGE);
    }
    
    SECTION("Multiple flags")
    {
        int combined = PRINT_BOLD | PRINT_RED | PRINT_UNDERLINE;
        ReportEntry entry("Multi", COLOR_DEFAULT, ALIGN_LEFT, combined);
        REQUIRE((entry.mode & PRINT_BOLD) != 0);
        REQUIRE((entry.mode & PRINT_RED) != 0);
        REQUIRE((entry.mode & PRINT_UNDERLINE) != 0);
    }
}

TEST_CASE("Report Type Constants", "[report][types]")
{
    SECTION("Report type definitions exist")
    {
        // Just verify the constants are defined
        int types[] = {
            REPORT_DRAWER,
            REPORT_CLOSEDCHECK,
            REPORT_SERVERLABOR,
            REPORT_CHECK,
            REPORT_SERVER,
            REPORT_SALES,
            REPORT_BALANCE,
            REPORT_DEPOSIT,
            REPORT_EXPENSES,
            REPORT_CREDITCARD
        };
        
        // All should be non-zero
        for (int type : types) {
            REQUIRE(type != 0);
        }
    }
    
    SECTION("Check order constants")
    {
        REQUIRE(CHECK_ORDER_NEWOLD == 0);
        REQUIRE(CHECK_ORDER_OLDNEW == 1);
    }
    
    SECTION("Print options")
    {
        REQUIRE(RP_NO_PRINT == 0);
        REQUIRE(RP_PRINT_LOCAL == 1);
        REQUIRE(RP_PRINT_REPORT == 2);
        REQUIRE(RP_ASK == 3);
    }
}

TEST_CASE("Report Real-World Entry Scenarios", "[report][scenarios]")
{
    SECTION("Sales report header")
    {
        ReportEntry title("DAILY SALES REPORT", COLOR_DEFAULT, 
                         ALIGN_CENTER, PRINT_BOLD | PRINT_LARGE);
        REQUIRE(title.text == "DAILY SALES REPORT");
        REQUIRE(title.align == ALIGN_CENTER);
        REQUIRE((title.mode & PRINT_BOLD) != 0);
    }
    
    SECTION("Line item with price")
    {
        ReportEntry item("Burger Combo", COLOR_DEFAULT, ALIGN_LEFT, 0);
        ReportEntry price("$12.99", COLOR_DEFAULT, ALIGN_RIGHT, 0);
        
        REQUIRE(item.align == ALIGN_LEFT);
        REQUIRE(price.align == ALIGN_RIGHT);
    }
    
    SECTION("Total line emphasized")
    {
        ReportEntry total("TOTAL:", COLOR_DEFAULT, ALIGN_LEFT, 
                         PRINT_BOLD | PRINT_UNDERLINE);
        REQUIRE((total.mode & PRINT_BOLD) != 0);
        REQUIRE((total.mode & PRINT_UNDERLINE) != 0);
    }
    
    SECTION("Error message in red")
    {
        ReportEntry error("ERROR: Payment declined", COLOR_RED, 
                         ALIGN_CENTER, PRINT_BOLD | PRINT_RED);
        REQUIRE(error.color == COLOR_RED);
        REQUIRE((error.mode & PRINT_RED) != 0);
    }
}
