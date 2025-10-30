#pragma once

#include "mock_settings.hh"
#include <memory>

// Forward declarations
class User;
class Drawer;

// Mock Terminal for testing - simplified interface
class MockTerminal {
public:
    MockTerminal();
    ~MockTerminal() = default;

    // Key methods for testing
    MockSettings* GetSettings();
    int UpdateSettings();
    int SaveSettings();

    // Test-specific methods
    void SetMockSettings(std::unique_ptr<MockSettings> settings);
    void SetUser(User* user);
    void SetDrawer(Drawer* drawer);

private:
    std::unique_ptr<MockSettings> mock_settings_;
    User* mock_user_ = nullptr;
    Drawer* mock_drawer_ = nullptr;
};
