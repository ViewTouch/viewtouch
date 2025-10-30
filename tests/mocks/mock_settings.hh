#pragma once

#include <string>

// Simplified Mock Settings for testing - standalone class
class MockSettings {
public:
    MockSettings();

    // Mock methods for testing
    int Load(const std::string& path);
    int Save(const std::string& path);

    // Test-specific methods
    void SetTestValues();
    void SetTaxRate(int index, int rate);
    void SetDrawerMode(int mode);

    // Public members for testing (simplified)
    float tax_food = 0.0f;
    float tax_alcohol = 0.0f;
    int drawer_mode = 0;
    int receipt_print = 1;
    int time_format = 0;
    int date_format = 0;
};
