/*
 * Unit tests for Error Handler - error reporting and management
 */

#include <catch2/catch_test_macros.hpp>
#include "error_handler.hh"
#include <thread>
#include <chrono>

using namespace vt_error;

TEST_CASE("ErrorInfo Construction", "[error][info]")
{
    SECTION("Create basic error info")
    {
        ErrorInfo info("Test error", Severity::ERROR, Category::GENERAL);
        REQUIRE(info.message == "Test error");
        REQUIRE(info.severity == Severity::ERROR);
        REQUIRE(info.category == Category::GENERAL);
    }
    
    SECTION("Create error with file and line info")
    {
        ErrorInfo info("File error", Severity::CRITICAL, Category::FILE_IO, 
                      "test.cc", 42, "test_function");
        REQUIRE(info.message == "File error");
        REQUIRE(info.file == "test.cc");
        REQUIRE(info.line == 42);
        REQUIRE(info.function == "test_function");
    }
    
    SECTION("Create error with context")
    {
        ErrorInfo info("Context error", Severity::WARNING, Category::NETWORK,
                      "", 0, "", 0, "During connection attempt");
        REQUIRE(info.context == "During connection attempt");
    }
    
    SECTION("Error with error code")
    {
        ErrorInfo info("System error", Severity::ERROR, Category::SYSTEM,
                      "", 0, "", 404);
        REQUIRE(info.error_code == 404);
    }
}

TEST_CASE("ErrorInfo Copy and Move", "[error][info][memory]")
{
    SECTION("Copy constructor")
    {
        ErrorInfo original("Original", Severity::INFO, Category::UI);
        ErrorInfo copy(original);
        
        REQUIRE(copy.message == original.message);
        REQUIRE(copy.severity == original.severity);
        REQUIRE(copy.category == original.category);
    }
    
    SECTION("Move constructor")
    {
        ErrorInfo original("Move me", Severity::WARNING, Category::MEMORY);
        std::string orig_msg = original.message;
        
        ErrorInfo moved(std::move(original));
        REQUIRE(moved.message == orig_msg);
    }
    
    SECTION("Copy assignment")
    {
        ErrorInfo e1("First", Severity::ERROR, Category::GENERAL);
        ErrorInfo e2("Second", Severity::INFO, Category::UI);
        
        e2 = e1;
        REQUIRE(e2.message == "First");
        REQUIRE(e2.severity == Severity::ERROR);
    }
    
    SECTION("Move assignment")
    {
        ErrorInfo e1("Move", Severity::CRITICAL, Category::DATABASE);
        ErrorInfo e2("Target", Severity::INFO, Category::GENERAL);
        
        e2 = std::move(e1);
        REQUIRE(e2.message == "Move");
        REQUIRE(e2.severity == Severity::CRITICAL);
    }
}

TEST_CASE("Severity Levels", "[error][severity]")
{
    SECTION("All severity levels are distinct")
    {
        REQUIRE(Severity::VT_DEBUG != Severity::INFO);
        REQUIRE(Severity::INFO != Severity::WARNING);
        REQUIRE(Severity::WARNING != Severity::ERROR);
        REQUIRE(Severity::ERROR != Severity::CRITICAL);
    }
    
    SECTION("Severity ordering")
    {
        REQUIRE(static_cast<int>(Severity::VT_DEBUG) < static_cast<int>(Severity::INFO));
        REQUIRE(static_cast<int>(Severity::INFO) < static_cast<int>(Severity::WARNING));
        REQUIRE(static_cast<int>(Severity::WARNING) < static_cast<int>(Severity::ERROR));
        REQUIRE(static_cast<int>(Severity::ERROR) < static_cast<int>(Severity::CRITICAL));
    }
}

TEST_CASE("Error Categories", "[error][category]")
{
    SECTION("All categories are distinct")
    {
        REQUIRE(Category::GENERAL != Category::SYSTEM);
        REQUIRE(Category::NETWORK != Category::DATABASE);
        REQUIRE(Category::UI != Category::PRINTER);
        REQUIRE(Category::CREDIT_CARD != Category::FILE_IO);
        REQUIRE(Category::MEMORY != Category::GENERAL);
    }
    
    SECTION("Can create errors for each category")
    {
        ErrorInfo general("msg", Severity::INFO, Category::GENERAL);
        ErrorInfo system("msg", Severity::INFO, Category::SYSTEM);
        ErrorInfo network("msg", Severity::INFO, Category::NETWORK);
        ErrorInfo database("msg", Severity::INFO, Category::DATABASE);
        ErrorInfo ui("msg", Severity::INFO, Category::UI);
        ErrorInfo printer("msg", Severity::INFO, Category::PRINTER);
        ErrorInfo credit("msg", Severity::INFO, Category::CREDIT_CARD);
        ErrorInfo fileio("msg", Severity::INFO, Category::FILE_IO);
        ErrorInfo memory("msg", Severity::INFO, Category::MEMORY);
        
        // All should construct without issues
        REQUIRE(general.category == Category::GENERAL);
        REQUIRE(printer.category == Category::PRINTER);
        REQUIRE(memory.category == Category::MEMORY);
    }
}

TEST_CASE("ErrorInfo Timestamp", "[error][timestamp]")
{
    SECTION("Timestamp is set on construction")
    {
        auto before = std::chrono::system_clock::now();
        ErrorInfo info("Timed", Severity::INFO, Category::GENERAL);
        auto after = std::chrono::system_clock::now();
        
        REQUIRE(info.timestamp >= before);
        REQUIRE(info.timestamp <= after);
    }
    
    SECTION("Different errors have different timestamps")
    {
        ErrorInfo e1("First", Severity::INFO, Category::GENERAL);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ErrorInfo e2("Second", Severity::INFO, Category::GENERAL);
        
        // e2 should have a later timestamp (or at least not earlier)
        REQUIRE(e2.timestamp >= e1.timestamp);
    }
}

TEST_CASE("Error Message Content", "[error][messages]")
{
    SECTION("Empty message is allowed")
    {
        ErrorInfo info("", Severity::INFO, Category::GENERAL);
        REQUIRE(info.message.empty());
    }
    
    SECTION("Long messages are preserved")
    {
        std::string long_msg(1000, 'x');
        ErrorInfo info(long_msg, Severity::ERROR, Category::GENERAL);
        REQUIRE(info.message.length() == 1000);
    }
    
    SECTION("Special characters in messages")
    {
        ErrorInfo info("Error: \n\t\\\"special\"", Severity::ERROR, Category::GENERAL);
        REQUIRE(info.message.find('\n') != std::string::npos);
        REQUIRE(info.message.find('\t') != std::string::npos);
    }
}

TEST_CASE("Error Context Information", "[error][context]")
{
    SECTION("File information")
    {
        ErrorInfo info("Error", Severity::ERROR, Category::GENERAL, 
                      "src/main.cc", 100);
        REQUIRE(info.file == "src/main.cc");
        REQUIRE(info.line == 100);
    }
    
    SECTION("Function information")
    {
        ErrorInfo info("Error", Severity::ERROR, Category::GENERAL,
                      "", 0, "process_payment");
        REQUIRE(info.function == "process_payment");
    }
    
    SECTION("Full context")
    {
        ErrorInfo info("Network timeout", Severity::ERROR, Category::NETWORK,
                      "network.cc", 250, "connect_to_server", 408,
                      "Attempting to connect to payment gateway");
        
        REQUIRE(info.file == "network.cc");
        REQUIRE(info.line == 250);
        REQUIRE(info.function == "connect_to_server");
        REQUIRE(info.error_code == 408);
        REQUIRE(info.context == "Attempting to connect to payment gateway");
    }
}

TEST_CASE("Error Codes", "[error][codes]")
{
    SECTION("Zero error code")
    {
        ErrorInfo info("No error code", Severity::INFO, Category::GENERAL);
        REQUIRE(info.error_code == 0);
    }
    
    SECTION("Positive error codes")
    {
        ErrorInfo info("HTTP error", Severity::ERROR, Category::NETWORK,
                      "", 0, "", 404);
        REQUIRE(info.error_code == 404);
    }
    
    SECTION("Negative error codes")
    {
        ErrorInfo info("System error", Severity::ERROR, Category::SYSTEM,
                      "", 0, "", -1);
        REQUIRE(info.error_code == -1);
    }
    
    SECTION("Large error codes")
    {
        ErrorInfo info("Custom", Severity::WARNING, Category::GENERAL,
                      "", 0, "", 99999);
        REQUIRE(info.error_code == 99999);
    }
}

TEST_CASE("Real-World Error Scenarios", "[error][scenarios]")
{
    SECTION("Database connection error")
    {
        ErrorInfo db_error(
            "Failed to connect to database",
            Severity::CRITICAL,
            Category::DATABASE,
            "db_connection.cc",
            45,
            "connect_to_db",
            1045, // MySQL access denied error
            "Using credentials from config file"
        );
        
        REQUIRE(db_error.severity == Severity::CRITICAL);
        REQUIRE(db_error.category == Category::DATABASE);
        REQUIRE(db_error.error_code == 1045);
    }
    
    SECTION("Printer offline error")
    {
        ErrorInfo printer_error(
            "Printer not responding",
            Severity::WARNING,
            Category::PRINTER,
            "printer_manager.cc",
            123,
            "send_to_printer",
            0,
            "Receipt printer at station 2"
        );
        
        REQUIRE(printer_error.severity == Severity::WARNING);
        REQUIRE(printer_error.category == Category::PRINTER);
    }
    
    SECTION("Credit card processing error")
    {
        ErrorInfo cc_error(
            "Card declined",
            Severity::ERROR,
            Category::CREDIT_CARD,
            "payment_processor.cc",
            300,
            "process_credit_card",
            51, // Insufficient funds
            "Transaction amount: $125.50"
        );
        
        REQUIRE(cc_error.severity == Severity::ERROR);
        REQUIRE(cc_error.category == Category::CREDIT_CARD);
        REQUIRE(cc_error.error_code == 51);
    }
    
    SECTION("Memory allocation error")
    {
        ErrorInfo mem_error(
            "Failed to allocate memory",
            Severity::CRITICAL,
            Category::MEMORY,
            "data_manager.cc",
            89,
            "allocate_buffer",
            12, // ENOMEM
            "Requested 10MB buffer"
        );
        
        REQUIRE(mem_error.severity == Severity::CRITICAL);
        REQUIRE(mem_error.category == Category::MEMORY);
    }
    
    SECTION("File I/O error")
    {
        ErrorInfo file_error(
            "Permission denied",
            Severity::ERROR,
            Category::FILE_IO,
            "file_manager.cc",
            156,
            "open_log_file",
            13, // EACCES
            "Attempting to write to /var/log/viewtouch.log"
        );
        
        REQUIRE(file_error.severity == Severity::ERROR);
        REQUIRE(file_error.category == Category::FILE_IO);
        REQUIRE(file_error.error_code == 13);
    }
}

TEST_CASE("Error Severity Use Cases", "[error][use_cases]")
{
    SECTION("Debug level for development")
    {
        ErrorInfo debug("Variable x = 42", Severity::VT_DEBUG, Category::GENERAL);
        REQUIRE(debug.severity == Severity::VT_DEBUG);
    }
    
    SECTION("Info for normal operations")
    {
        ErrorInfo info("Transaction completed", Severity::INFO, Category::GENERAL);
        REQUIRE(info.severity == Severity::INFO);
    }
    
    SECTION("Warning for potential issues")
    {
        ErrorInfo warning("Disk space low", Severity::WARNING, Category::SYSTEM);
        REQUIRE(warning.severity == Severity::WARNING);
    }
    
    SECTION("Error for failures")
    {
        ErrorInfo error("Payment failed", Severity::ERROR, Category::CREDIT_CARD);
        REQUIRE(error.severity == Severity::ERROR);
    }
    
    SECTION("Critical for system-threatening issues")
    {
        ErrorInfo critical("System crash imminent", Severity::CRITICAL, Category::SYSTEM);
        REQUIRE(critical.severity == Severity::CRITICAL);
    }
}

TEST_CASE("ErrorInfo Field Validation", "[error][validation]")
{
    SECTION("All fields are accessible")
    {
        ErrorInfo info(
            "Full info",
            Severity::ERROR,
            Category::NETWORK,
            "test.cc",
            42,
            "test_func",
            100,
            "Test context"
        );
        
        // Verify all fields are readable
        REQUIRE_FALSE(info.message.empty());
        REQUIRE(info.severity == Severity::ERROR);
        REQUIRE(info.category == Category::NETWORK);
        REQUIRE_FALSE(info.file.empty());
        REQUIRE(info.line > 0);
        REQUIRE_FALSE(info.function.empty());
        REQUIRE(info.error_code != 0);
        REQUIRE_FALSE(info.context.empty());
    }
}
