#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// Global test setup
struct TestConfig {
    TestConfig() {
        // Disable any ViewTouch logging during tests
        // Set up test data directory if needed
    }

    ~TestConfig() {
        // Cleanup after tests
    }
};

TestConfig globalTestSetup;
