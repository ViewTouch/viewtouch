#include "mock_settings.hh"

MockSettings::MockSettings()
{
    // Initialize with reasonable defaults for testing
    SetTestValues();
}

int MockSettings::Load(const std::string& path)
{
    // Mock implementation - always succeed
    (void)path; // Suppress unused parameter warning
    return 0;
}

int MockSettings::Save(const std::string& path)
{
    // Mock implementation - always succeed
    (void)path; // Suppress unused parameter warning
    return 0;
}

void MockSettings::SetTestValues()
{
    // Set reasonable test defaults
    tax_food = 0.0825f;        // 8.25%
    tax_alcohol = 0.0f;        // No alcohol tax

    drawer_mode = 0;  // Trusted mode
    receipt_print = 1;  // On finalize
    time_format = 0;   // 12-hour
    date_format = 0;   // MMDDYY
}

void MockSettings::SetTaxRate(int index, int rate)
{
    // Convert percentage (825 = 8.25%) to float (0.0825)
    float tax_rate = static_cast<float>(rate) / 10000.0f;

    switch (index) {
        case 0: tax_food = tax_rate; break;
        case 1: tax_alcohol = tax_rate; break;
        default: break;
    }
}

void MockSettings::SetDrawerMode(int mode)
{
    drawer_mode = mode;
}
