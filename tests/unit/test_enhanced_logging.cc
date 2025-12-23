#include <catch2/catch_all.hpp>
#include "src/utils/vt_logger.hh"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <variant>

namespace fs = std::filesystem;

TEST_CASE("Logger Initialization", "[enhanced_logging][init]")
{
    // Clean up any existing log files
    fs::remove_all("/tmp/viewtouch_test_logs");

    SECTION("Default initialization")
    {
        vt::Logger::Initialize("/tmp/viewtouch_test_logs");
        REQUIRE(vt::Logger::GetLogger() != nullptr);

        // Test basic logging
        vt::Logger::info("Test message");
        vt::Logger::warn("Test warning");
        vt::Logger::error("Test error");

        vt::Logger::Shutdown();

        // Check that log files were created
        REQUIRE(fs::exists("/tmp/viewtouch_test_logs/viewtouch.log"));
        REQUIRE(fs::exists("/tmp/viewtouch_test_logs/viewtouch_structured.log"));
    }

    SECTION("Custom log level")
    {
        vt::Logger::Initialize("/tmp/viewtouch_test_logs", "debug");
        auto logger = vt::Logger::GetLogger();
        REQUIRE(logger != nullptr);
        REQUIRE(logger->level() == spdlog::level::debug);
        vt::Logger::Shutdown();
    }
}

TEST_CASE("Business Context Management", "[enhanced_logging][context]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Set and get business context")
    {
        vt::BusinessContext ctx;
        ctx.user_id = 123;
        ctx.employee_id = 456;
        ctx.check_id = 789;
        ctx.table_number = 10;
        ctx.session_id = "session_123";
        ctx.terminal_id = "term_001";

        vt::Logger::set_business_context(ctx);
        auto retrieved = vt::Logger::get_business_context();
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->user_id == 123);
        REQUIRE(retrieved->employee_id == 456);
        REQUIRE(retrieved->check_id == 789);
        REQUIRE(retrieved->table_number == 10);
        REQUIRE(retrieved->session_id == "session_123");
        REQUIRE(retrieved->terminal_id == "term_001");
    }

    SECTION("Clear business context")
    {
        vt::BusinessContext ctx;
        ctx.user_id = 999;
        vt::Logger::set_business_context(ctx);

        auto retrieved = vt::Logger::get_business_context();
        REQUIRE(retrieved.has_value());

        vt::Logger::clear_business_context();
        retrieved = vt::Logger::get_business_context();
        REQUIRE(!retrieved.has_value());
    }

    SECTION("Context JSON serialization")
    {
        vt::BusinessContext ctx;
        ctx.user_id = 123;
        ctx.check_id = 456;
        ctx.start_time = std::chrono::system_clock::now();

        nlohmann::json j = ctx.to_json();
        REQUIRE(j.contains("user_id"));
        REQUIRE(j.contains("check_id"));
        REQUIRE(j.contains("start_time"));
        REQUIRE(j["user_id"] == 123);
        REQUIRE(j["check_id"] == 456);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("User Session Tracking", "[enhanced_logging][session]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Start user session with auto-generated ID")
    {
        vt::Logger::start_user_session(123);

        auto ctx = vt::Logger::get_business_context();
        REQUIRE(ctx.has_value());
        REQUIRE(ctx->user_id == 123);
        REQUIRE(ctx->session_id.has_value());
        REQUIRE(ctx->session_id->find("session_123_") == 0);
        REQUIRE(ctx->start_time.has_value());
    }

    SECTION("Start user session with custom ID")
    {
        vt::Logger::start_user_session(456, "custom_session_123");

        auto ctx = vt::Logger::get_business_context();
        REQUIRE(ctx.has_value());
        REQUIRE(ctx->user_id == 456);
        REQUIRE(ctx->session_id == "custom_session_123");
    }

    SECTION("Update session context")
    {
        vt::Logger::start_user_session(789);
        vt::Logger::update_session_context(111, 5, 222);

        auto ctx = vt::Logger::get_business_context();
        REQUIRE(ctx.has_value());
        REQUIRE(ctx->check_id == 111);
        REQUIRE(ctx->table_number == 5);
        REQUIRE(ctx->employee_id == 222);
    }

    SECTION("End user session")
    {
        vt::Logger::start_user_session(999);
        auto ctx_before = vt::Logger::get_business_context();
        REQUIRE(ctx_before.has_value());

        vt::Logger::end_user_session();
        auto ctx_after = vt::Logger::get_business_context();
        REQUIRE(!ctx_after.has_value());
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Structured Log Events", "[enhanced_logging][events]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Create and log simple event")
    {
        vt::LogEvent event("test_event", "Test message occurred");
        vt::Logger::log_event(event);

        // Event should be logged successfully
        REQUIRE(true); // If we get here, logging worked
    }

    SECTION("Event with metadata")
    {
        vt::LogEvent event("payment_processed", "Payment completed", spdlog::level::info);
        event.add("amount", 25.99)
             .add("payment_type", "credit_card")
             .add("transaction_id", 12345)
             .add("approved", true);

        vt::Logger::log_event(event);
        REQUIRE(true);
    }

    SECTION("Event with business context")
    {
        vt::BusinessContext ctx;
        ctx.user_id = 123;
        ctx.check_id = 456;

        vt::LogEvent event("check_created", "New check opened");
        event.with_context(ctx);

        vt::Logger::log_event(event);
        REQUIRE(true);
    }

    SECTION("Event JSON serialization")
    {
        vt::LogEvent event("test", "message");
        event.add("key1", std::string("value1"));
        event.add("key2", 42);
        event.add("key3", true);

        nlohmann::json j = event.to_json();
        REQUIRE(j["event_type"] == "test");
        REQUIRE(j["message"] == "message");
        REQUIRE(j.contains("metadata"));
        REQUIRE(j["metadata"]["key2"] == 42);
        REQUIRE(j["metadata"]["key3"] == true);
        REQUIRE(j.contains("timestamp"));

        // Verify string handling works (key1 should be present)
        REQUIRE(j["metadata"].contains("key1"));
        REQUIRE(j["metadata"]["key1"].is_string());
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Business Event Macros", "[enhanced_logging][macros]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Simple business event")
    {
        vt::Logger::business_event("item_added", {
            {"item_id", std::string("burger_001")},
            {"quantity", 2},
            {"price", 9.99}
        });
        REQUIRE(true);
    }

    SECTION("Complex business event with session context")
    {
        vt::Logger::start_user_session(123);
        vt::Logger::update_session_context(456, 10, 789);

        vt::Logger::business_event("order_placed", {
            {"order_total", 45.67},
            {"item_count", 3}
        });
        vt::Logger::end_user_session();

        REQUIRE(true);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Performance Monitoring", "[enhanced_logging][performance]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Timer operations")
    {
        VT_PERFORMANCE_START("database_query");

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        VT_PERFORMANCE_END("database_query");

        REQUIRE(true);
    }

    SECTION("Performance event logging")
    {
        using namespace std::chrono;
        vt::Logger::performance_event("test_operation", microseconds(1500));
        REQUIRE(true);
    }

    SECTION("Performance monitor methods")
    {
        vt::PerformanceMonitor::record_metric("memory_usage", 85.5);
        vt::PerformanceMonitor::record_counter("requests_served", 1);
        vt::PerformanceMonitor::record_counter("requests_served", 2);

        REQUIRE(true);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Log File Output", "[enhanced_logging][files]")
{
    // Clean up
    fs::remove_all("/tmp/viewtouch_test_logs");
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Structured JSON logging")
    {
        vt::LogEvent event("test_event", "Structured log test");
        event.add("test_key", "test_value");
        vt::Logger::log_event(event);

        // Give logging time to flush
        vt::Logger::Flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check structured log file
        REQUIRE(fs::exists("/tmp/viewtouch_test_logs/viewtouch_structured.log"));

        std::ifstream structured_log("/tmp/viewtouch_test_logs/viewtouch_structured.log");
        std::string line;
        bool found_json = false;
        while (std::getline(structured_log, line)) {
            if (line.find("test_event") != std::string::npos &&
                line.find("test_key") != std::string::npos) {
                found_json = true;
                break;
            }
        }
        REQUIRE(found_json);
    }

    SECTION("Human-readable logging")
    {
        vt::Logger::info("Human readable test message");

        vt::Logger::Flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check regular log file
        REQUIRE(fs::exists("/tmp/viewtouch_test_logs/viewtouch.log"));

        std::ifstream regular_log("/tmp/viewtouch_test_logs/viewtouch.log");
        std::string line;
        bool found_message = false;
        while (std::getline(regular_log, line)) {
            if (line.find("Human readable test message") != std::string::npos) {
                found_message = true;
                break;
            }
        }
        REQUIRE(found_message);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Thread Safety", "[enhanced_logging][threading]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Concurrent logging")
    {
        auto logging_thread = [](int thread_id) {
            for (int i = 0; i < 10; ++i) {
                vt::Logger::info("Thread {} message {}", thread_id, i);

                vt::LogEvent event("thread_event", "Concurrent event");
                event.add("thread_id", thread_id);
                event.add("message_num", i);
                vt::Logger::log_event(event);
            }
        };

        std::thread t1(logging_thread, 1);
        std::thread t2(logging_thread, 2);
        std::thread t3(logging_thread, 3);

        t1.join();
        t2.join();
        t3.join();

        REQUIRE(true);
    }

    SECTION("Thread-local business context")
    {
        auto context_thread = [](int user_id) {
            vt::Logger::start_user_session(user_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            auto ctx = vt::Logger::get_business_context();
            REQUIRE(ctx.has_value());
            REQUIRE(ctx->user_id == user_id);

            vt::Logger::end_user_session();
        };

        std::thread t1(context_thread, 111);
        std::thread t2(context_thread, 222);

        t1.join();
        t2.join();

        REQUIRE(true);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Legacy Compatibility", "[enhanced_logging][legacy]")
{
    vt::Logger::Initialize("/tmp/viewtouch_test_logs");

    SECTION("Legacy syslog-style logging")
    {
        // This tests the bridge function
        vt::Logger::log_legacy_error(LOG_INFO, "Legacy message: %s %d", "test", 123);
        vt::Logger::log_legacy_error(LOG_ERR, "Legacy error: %s", "error message");

        vt::Logger::Flush();
        REQUIRE(true);
    }

    vt::Logger::Shutdown();
}

TEST_CASE("Error Handling", "[enhanced_logging][error]")
{
    SECTION("Logger operations without initialization")
    {
        // These should not crash
        vt::Logger::info("Message before init");
        vt::Logger::Flush();

        // Auto-initialization should work
        auto logger = vt::Logger::GetLogger();
        REQUIRE(logger != nullptr);

        vt::Logger::Shutdown();
    }

    SECTION("Invalid log levels")
    {
        vt::Logger::Initialize("/tmp/viewtouch_test_logs", "invalid_level");
        // Should default to info level
        auto logger = vt::Logger::GetLogger();
        REQUIRE(logger->level() == spdlog::level::info);

        vt::Logger::Shutdown();
    }

    SECTION("Performance monitor edge cases")
    {
        // End timer without starting should not crash
        vt::PerformanceMonitor::end_timer("nonexistent_timer");

        // Multiple starts should work
        vt::PerformanceMonitor::start_timer("test_timer");
        vt::PerformanceMonitor::start_timer("test_timer"); // Overwrite

        REQUIRE(true);
    }
}

// Cleanup after all tests
TEST_CASE("Cleanup", "[enhanced_logging]")
{
    vt::Logger::Shutdown();
    fs::remove_all("/tmp/viewtouch_test_logs");
}
