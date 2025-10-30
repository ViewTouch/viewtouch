#include "mock_terminal.hh"

MockTerminal::MockTerminal()
{
    // Initialize with default settings
    mock_settings_ = std::make_unique<MockSettings>();
}

MockSettings* MockTerminal::GetSettings()
{
    return mock_settings_.get();
}

int MockTerminal::UpdateSettings()
{
    // Mock implementation - just return success
    return 0;
}

int MockTerminal::SaveSettings()
{
    // Mock implementation - just return success
    return 0;
}

void MockTerminal::SetMockSettings(std::unique_ptr<MockSettings> settings)
{
    mock_settings_ = std::move(settings);
}

void MockTerminal::SetUser(User* user)
{
    mock_user_ = user;
}

void MockTerminal::SetDrawer(Drawer* drawer)
{
    mock_drawer_ = drawer;
}
