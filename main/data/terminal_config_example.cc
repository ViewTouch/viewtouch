/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * terminal_config_example.cc - Example of using JSON config for terminals
 * Demonstrates modern configuration management with nlohmann/json
 */

#include "src/utils/vt_json_config.hh"
#include "src/utils/vt_logger.hh"
#include "src/utils/vt_enum_utils.hh"
#include "settings_enums.hh"
#include <iostream>

/**
 * @brief Example: Create a JSON configuration file for ViewTouch terminals
 * 
 * This demonstrates how to use JSON for configuration instead of custom
 * binary formats or INI files.
 */
void CreateTerminalConfigExample() {
    vt::JsonConfig cfg("/usr/viewtouch/dat/conf/terminals.json");

    // Terminal 1 - Front Counter
    cfg.Set("terminals.0.id", 1);
    cfg.Set("terminals.0.name", "Front Counter");
    cfg.Set("terminals.0.display", ":0.0");
    cfg.Set("terminals.0.type", "pos");
    cfg.Set("terminals.0.printer", std::string(vt::EnumToString(PrinterDestType::Kitchen1)));
    cfg.Set("terminals.0.drawer_mode", std::string(vt::EnumToString(DrawerModeType::Trusted)));
    cfg.Set("terminals.0.receipt_print", std::string(vt::EnumToString(ReceiptPrintType::OnFinalize)));

    // Terminal 2 - Kitchen Display
    cfg.Set("terminals.1.id", 2);
    cfg.Set("terminals.1.name", "Kitchen Display");
    cfg.Set("terminals.1.display", ":0.1");
    cfg.Set("terminals.1.type", "kitchen");
    cfg.Set("terminals.1.printer", std::string(vt::EnumToString(PrinterDestType::Kitchen1)));
    cfg.Set("terminals.1.show_images", true);

    // Global terminal settings
    cfg.Set("settings.screen_blank_time", 300);
    cfg.Set("settings.time_format", std::string(vt::EnumToString(TimeFormatType::Hour12)));
    cfg.Set("settings.date_format", std::string(vt::EnumToString(DateFormatType::MMDDYY)));
    cfg.Set("settings.number_format", std::string(vt::EnumToString(NumberFormatType::Standard)));

    // Printer network configuration
    cfg.Set("printers.kitchen1.host", "192.168.1.100");
    cfg.Set("printers.kitchen1.port", 9100);
    cfg.Set("printers.kitchen1.model", "epson");
    cfg.Set("printers.receipts.host", "192.168.1.101");
    cfg.Set("printers.receipts.port", 9100);
    cfg.Set("printers.receipts.model", "star");

    // Save with pretty printing and backup
    if (cfg.Save(true, true)) {
        vt::Logger::info("Terminal configuration saved to: {}", cfg.GetPath());
    } else {
        vt::Logger::error("Failed to save terminal configuration");
    }
}

/**
 * @brief Example: Load and parse terminal configuration
 */
void LoadTerminalConfig() {
    vt::JsonConfig cfg("/usr/viewtouch/dat/conf/terminals.json");

    if (!cfg.Load()) {
        vt::Logger::warn("Terminal config not found, using defaults");
        return;
    }

    vt::Logger::info("Loading terminal configuration...");

    // Read terminal 1 settings
    auto term1_name = cfg.Get<std::string>("terminals.0.name", "Unknown");
    auto term1_display = cfg.Get<std::string>("terminals.0.display", ":0.0");
    auto term1_type = cfg.Get<std::string>("terminals.0.type", "pos");

    // Parse enum from JSON
    auto printer_str = cfg.Get<std::string>("terminals.0.printer");
    auto printer = vt::StringToEnum<PrinterDestType>(printer_str);
    
    auto drawer_str = cfg.Get<std::string>("terminals.0.drawer_mode");
    auto drawer_mode = vt::StringToEnum<DrawerModeType>(drawer_str);

    // Log the configuration
    vt::Logger::info("Terminal 1: {}", term1_name);
    vt::Logger::info("  Display: {}", term1_display);
    vt::Logger::info("  Type: {}", term1_type);
    
    if (printer) {
        vt::Logger::info("  Printer: {}", vt::EnumToString(*printer));
    }
    
    if (drawer_mode) {
        vt::Logger::info("  Drawer Mode: {}", vt::GetDrawerModeDisplayName(*drawer_mode));
    }

    // Read global settings
    auto blank_time = cfg.Get<int>("settings.screen_blank_time", 300);
    auto time_format_str = cfg.Get<std::string>("settings.time_format");
    auto time_format = vt::StringToEnum<TimeFormatType>(time_format_str);

    vt::Logger::info("Global Settings:");
    vt::Logger::info("  Screen blank time: {}s", blank_time);
    
    if (time_format) {
        vt::Logger::info("  Time format: {}", vt::GetTimeFormatDisplayName(*time_format));
    }

    // Read printer configuration
    auto kitchen_host = cfg.Get<std::string>("printers.kitchen1.host");
    auto kitchen_port = cfg.Get<int>("printers.kitchen1.port", 9100);
    auto kitchen_model = cfg.Get<std::string>("printers.kitchen1.model");

    vt::Logger::info("Printer Configuration:");
    vt::Logger::info("  Kitchen1: {}:{} ({})", kitchen_host, kitchen_port, kitchen_model);
}

/**
 * @brief Example: Update terminal configuration at runtime
 */
void UpdateTerminalSettings() {
    vt::JsonConfig cfg("/usr/viewtouch/dat/conf/terminals.json");

    if (!cfg.Load()) {
        vt::Logger::error("Cannot update config - file not found");
        return;
    }

    // Change drawer mode
    auto old_mode_str = cfg.Get<std::string>("terminals.0.drawer_mode");
    auto old_mode = vt::StringToEnum<DrawerModeType>(old_mode_str);
    
    DrawerModeType new_mode = DrawerModeType::Assigned;
    cfg.Set("terminals.0.drawer_mode", std::string(vt::EnumToString(new_mode)));

    // Log the change
    if (old_mode) {
        vt::Logger::info("Drawer mode changed: {} -> {}", 
                        vt::GetDrawerModeDisplayName(*old_mode),
                        vt::GetDrawerModeDisplayName(new_mode));
    }

    // Save changes
    if (cfg.Save()) {
        vt::Logger::info("Terminal configuration updated successfully");
    }
}

/**
 * @brief Example: Create dropdown options for UI
 */
void ShowDropdownExample() {
    vt::Logger::info("Available Drawer Modes for UI:");
    auto drawer_modes = vt::GetAllDrawerModes();
    for (const auto& [name, value] : drawer_modes) {
        vt::Logger::info("  {} = {} ({})", 
                        name, 
                        vt::EnumToInt(value),
                        vt::GetDrawerModeDisplayName(value));
    }

    vt::Logger::info("\nAvailable Receipt Print Options:");
    auto receipt_options = vt::GetAllReceiptPrintOptions();
    for (const auto& [name, value] : receipt_options) {
        vt::Logger::info("  {} = {} ({})", 
                        name, 
                        vt::EnumToInt(value),
                        vt::GetReceiptPrintDisplayName(value));
    }
}

/**
 * @brief Main demonstration
 */
int main() {
    // Initialize logging
    vt::Logger::Initialize("/tmp/viewtouch_terminal_config", "debug", true, false);
    vt::Logger::info("=== ViewTouch Terminal Configuration Example ===");

    try {
        // 1. Create example configuration
        vt::Logger::info("\n1. Creating terminal configuration...");
        CreateTerminalConfigExample();

        // 2. Load and display configuration
        vt::Logger::info("\n2. Loading terminal configuration...");
        LoadTerminalConfig();

        // 3. Update configuration
        vt::Logger::info("\n3. Updating terminal settings...");
        UpdateTerminalSettings();

        // 4. Show dropdown options
        vt::Logger::info("\n4. Generating UI dropdown options...");
        ShowDropdownExample();

        vt::Logger::info("\n=== Example Complete ===");
        vt::Logger::info("Check /usr/viewtouch/dat/conf/terminals.json for the generated config");

    } catch (const std::exception& e) {
        vt::Logger::error("Exception: {}", e.what());
        return 1;
    }

    // Cleanup
    vt::Logger::Shutdown();
    return 0;
}

/*
 * EXAMPLE OUTPUT JSON FILE:
 * =========================
 * 
 * {
 *     "printers": {
 *         "kitchen1": {
 *             "host": "192.168.1.100",
 *             "model": "epson",
 *             "port": 9100
 *         },
 *         "receipts": {
 *             "host": "192.168.1.101",
 *             "model": "star",
 *             "port": 9100
 *         }
 *     },
 *     "settings": {
 *         "date_format": "MMDDYY",
 *         "number_format": "Standard",
 *         "screen_blank_time": 300,
 *         "time_format": "Hour12"
 *     },
 *     "terminals": [
 *         {
 *             "display": ":0.0",
 *             "drawer_mode": "Trusted",
 *             "id": 1,
 *             "name": "Front Counter",
 *             "printer": "Kitchen1",
 *             "receipt_print": "OnFinalize",
 *             "type": "pos"
 *         },
 *         {
 *             "display": ":0.1",
 *             "id": 2,
 *             "name": "Kitchen Display",
 *             "printer": "Kitchen1",
 *             "show_images": true,
 *             "type": "kitchen"
 *         }
 *     ]
 * }
 */

