/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
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
 * modern_cpp_example.cc - Demonstration of modern C++ utilities
 * Shows how to use spdlog, nlohmann/json, and magic_enum in ViewTouch
 */

#include "vt_logger.hh"
#include "vt_json_config.hh"
#include "vt_enum_utils.hh"
#include <iostream>

// Example enum for demo
enum class PaymentType {
    Cash,
    CreditCard,
    DebitCard,
    GiftCard,
    Check,
    RoomCharge
};

// Example enum class for button types
enum class ButtonType {
    Normal,
    Zone,
    Goto,
    Item,
    Modifier
};

void DemoLogging() {
    std::cout << "\n=== Logging Demo ===" << std::endl;
    
    // Initialize logger
    vt::Logger::Initialize(
        "/tmp/viewtouch_demo",  // Log directory
        "debug",                 // Log level
        true,                    // Console output
        false                    // Syslog (disabled for demo)
    );

    // Different log levels
    vt::Logger::info("ViewTouch demo started");
    vt::Logger::debug("Debug information: value={}", 42);
    vt::Logger::warn("Warning: Low memory available");
    vt::Logger::error("Error processing payment: {}", "Card declined");

    // Format strings with multiple arguments
    int check_id = 1234;
    double total = 45.67;
    vt::Logger::info("Check #{} completed - Total: ${:.2f}", check_id, total);

    // Using macros
    VT_LOG_INFO("This is a convenience macro");
    VT_LOG_DEBUG("Debug value: {}", 100);

    std::cout << "Check /tmp/viewtouch_demo/viewtouch.log for output" << std::endl;
}

void DemoJson() {
    std::cout << "\n=== JSON Config Demo ===" << std::endl;

    // Create a config file
    vt::JsonConfig cfg("/tmp/viewtouch_demo_config.json");

    // Set various values
    cfg.Set("store_name", "Demo Restaurant");
    cfg.Set("store_address", "123 Main Street");
    cfg.Set("tax.food", 0.07);
    cfg.Set("tax.alcohol", 0.09);
    cfg.Set("network.timeout", 30);
    cfg.Set("settings.use_seats", true);

    // Save to file
    if (cfg.Save()) {
        std::cout << "Config saved to: " << cfg.GetPath() << std::endl;
    }

    // Load it back
    vt::JsonConfig loaded("/tmp/viewtouch_demo_config.json");
    if (loaded.Load()) {
        auto store = loaded.Get<std::string>("store_name", "Unknown");
        auto tax = loaded.Get<double>("tax.food", 0.0);
        auto timeout = loaded.Get<int>("network.timeout", 10);
        bool use_seats = loaded.Get<bool>("settings.use_seats", false);

        std::cout << "Store: " << store << std::endl;
        std::cout << "Food tax: " << tax << std::endl;
        std::cout << "Timeout: " << timeout << "s" << std::endl;
        std::cout << "Use seats: " << (use_seats ? "yes" : "no") << std::endl;
    }

    // Create example config
    vt::JsonConfig::CreateExample("/tmp/viewtouch_example_config.json");
    std::cout << "Example config created at /tmp/viewtouch_example_config.json" << std::endl;
}

void DemoEnums() {
    std::cout << "\n=== Enum Utils Demo ===" << std::endl;

    // Enum to string
    auto payment_name = vt::EnumToString(PaymentType::CreditCard);
    std::cout << "Payment type: " << payment_name << std::endl;

    // String to enum
    auto payment = vt::StringToEnum<PaymentType>("Cash");
    if (payment) {
        std::cout << "Parsed payment type: " << vt::EnumToString(*payment) << std::endl;
    }

    // Get all values
    std::cout << "\nAll payment types:" << std::endl;
    for (auto type : vt::GetEnumValues<PaymentType>()) {
        std::cout << "  - " << vt::EnumToString(type) << std::endl;
    }

    // Get count
    std::cout << "\nTotal payment types: " << vt::GetEnumCount<PaymentType>() << std::endl;

    // Display formatting
    std::cout << "\nButton types (display format):" << std::endl;
    for (auto btn : vt::GetEnumValues<ButtonType>()) {
        std::cout << "  - " << vt::EnumToDisplayString(btn) << std::endl;
    }

    // Get pairs for UI dropdowns
    auto pairs = vt::GetEnumPairs<ButtonType>();
    std::cout << "\nButton type pairs (for UI):" << std::endl;
    for (const auto& [name, value] : pairs) {
        std::cout << "  " << name << " = " << vt::EnumToInt(value) << std::endl;
    }

    // Backwards compatibility - C-style array
    const char** payment_names = vt::GetEnumNamesArray<PaymentType>();
    std::cout << "\nC-style array (backwards compatible):" << std::endl;
    for (int i = 0; payment_names[i] != nullptr; ++i) {
        std::cout << "  [" << i << "] = " << payment_names[i] << std::endl;
    }
}

void DemoCombined() {
    std::cout << "\n=== Combined Demo ===" << std::endl;

    // Use JSON config with enums and logging
    vt::JsonConfig cfg("/tmp/viewtouch_combined_demo.json");

    // Store enum as string in JSON
    cfg.Set("default_payment", std::string(vt::EnumToString(PaymentType::CreditCard)));
    cfg.Set("button_type", std::string(vt::EnumToString(ButtonType::Normal)));

    // Save
    if (cfg.Save()) {
        vt::Logger::info("Saved combined config to: {}", cfg.GetPath());
    }

    // Load and parse
    vt::JsonConfig loaded("/tmp/viewtouch_combined_demo.json");
    if (loaded.Load()) {
        auto payment_str = loaded.Get<std::string>("default_payment");
        auto payment = vt::StringToEnum<PaymentType>(payment_str);
        
        if (payment) {
            vt::Logger::info("Loaded payment type: {}", vt::EnumToString(*payment));
            std::cout << "Successfully parsed payment type from JSON" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== ViewTouch Modern C++ Libraries Demo ===" << std::endl;
    std::cout << "Demonstrating spdlog, nlohmann/json, and magic_enum" << std::endl;

    try {
        DemoLogging();
        DemoJson();
        DemoEnums();
        DemoCombined();

        std::cout << "\n=== Demo Complete ===" << std::endl;
        std::cout << "Check the /tmp directory for generated files" << std::endl;

        // Cleanup
        vt::Logger::Shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

