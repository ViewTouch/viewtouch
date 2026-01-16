#include <catch2/catch_all.hpp>
#include "src/utils/memory_utils.hh"
#include <memory>

// Test modern memory management patterns
TEST_CASE("Modern Memory Management", "[memory]")
{
    SECTION("Smart pointer factory functions")
    {
        // Test basic unique_ptr creation
        auto int_ptr = std::make_unique<int>(42);
        REQUIRE(int_ptr != nullptr);
        REQUIRE(*int_ptr == 42);

        auto string_ptr = std::make_unique<std::string>("Hello");
        REQUIRE(string_ptr != nullptr);
        REQUIRE(*string_ptr == "Hello");
    }

    SECTION("Ownership transfer utilities")
    {
        auto ptr = std::make_unique<std::string>("Test String");

        // Transfer ownership to raw pointer
        std::string* raw_ptr = vt::transfer_ownership(ptr);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(ptr == nullptr);  // Ownership transferred
        REQUIRE(*raw_ptr == "Test String");

        // Clean up
        delete raw_ptr;
    }

    SECTION("RAII wrapper example")
    {
        // This demonstrates the concept - in practice we'd use it for C APIs
        bool cleanup_called = false;

        auto test_resource = new int(42);
        auto wrapper = vt::make_raii(test_resource, [&](int* ptr) {
            cleanup_called = true;
            delete ptr;
        });

        REQUIRE(*wrapper == 42);
        REQUIRE(!cleanup_called);

        // When wrapper goes out of scope, cleanup is called
        {
            auto temp_wrapper = std::move(wrapper);
            // cleanup_called remains false until temp_wrapper is destroyed
        }

        REQUIRE(cleanup_called);
    }
}

TEST_CASE("Memory Safety Improvements", "[safety]")
{
    SECTION("Automatic cleanup prevents leaks")
    {
        // This test demonstrates that smart pointers prevent memory leaks
        // In the old pattern, forgetting to delete would cause leaks

        int cleanup_count = 0;

        {
            auto ptr1 = std::make_unique<int>(1);
            auto ptr2 = std::make_unique<int>(2);
            auto ptr3 = std::make_unique<int>(3);

            // Simulate some work
            *ptr1 = 10;
            *ptr2 = 20;
            *ptr3 = 30;

            // Create RAII wrapper to track cleanup
            auto tracked_ptr = new int(100);
            auto wrapper = vt::make_raii(tracked_ptr, [&](int* ptr) {
                cleanup_count++;
                delete ptr;
            });

            REQUIRE(cleanup_count == 0);
            REQUIRE(*wrapper == 100);
        }

        // All smart pointers are automatically cleaned up here
        // In legacy code, this would be:
        // delete ptr1; delete ptr2; delete ptr3; delete tracked_ptr;
        // (and easy to forget!)

        REQUIRE(cleanup_count == 1);  // RAII wrapper was cleaned up
    }

    SECTION("Exception safety")
    {
        // Smart pointers provide exception safety
        bool cleanup_happened = false;

        {
            auto safe_ptr = std::make_unique<std::string>("Safe");

            // Create a resource that needs cleanup
            auto resource = new int(123);
            auto wrapper = vt::make_raii(resource, [&](int* ptr) {
                cleanup_happened = true;
                delete ptr;
            });

            REQUIRE(*safe_ptr == "Safe");
            REQUIRE(*wrapper == 123);
            REQUIRE(!cleanup_happened);

            // Cleanup happens at end of scope via RAII
        }

        REQUIRE(cleanup_happened);  // RAII wrapper was cleaned up
    }
}

TEST_CASE("Memory Management Patterns", "[patterns]")
{
    SECTION("Factory pattern with smart pointers")
    {
        // Demonstrate factory pattern that returns smart pointers
        auto create_test_object = []() -> std::unique_ptr<std::string> {
            return std::make_unique<std::string>("Factory Created");
        };

        auto obj = create_test_object();
        REQUIRE(obj != nullptr);
        REQUIRE(*obj == "Factory Created");
    }

    SECTION("Resource management with multiple owners")
    {
        // Demonstrate shared_ptr for shared ownership
        auto shared_resource = std::make_shared<std::string>("Shared");

        // Multiple owners can share the resource
        std::weak_ptr<std::string> weak_ref = shared_resource;
        REQUIRE(!weak_ref.expired());

        {
            auto another_owner = shared_resource;
            REQUIRE(another_owner != nullptr);
            REQUIRE(*another_owner == "Shared");
            REQUIRE(shared_resource.use_count() == 2);
        }

        // Reference count decreases when owners go out of scope
        REQUIRE(shared_resource.use_count() == 1);
    }

    SECTION("Avoiding dangling pointers")
    {
        // This demonstrates how smart pointers prevent dangling pointers
        std::unique_ptr<std::string> safe_ptr;

        {
            auto temp = std::make_unique<std::string>("Temporary");
            safe_ptr = std::move(temp);

            // temp is now nullptr, but safe_ptr owns the resource
            REQUIRE(temp == nullptr);
            REQUIRE(safe_ptr != nullptr);
        }

        // safe_ptr still owns the resource and cleans it up
        REQUIRE(*safe_ptr == "Temporary");
    }
}
