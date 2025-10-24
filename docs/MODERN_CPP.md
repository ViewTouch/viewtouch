# ViewTouch Modern C++ Libraries - Complete Guide

**Version**: 25.02.0+
**Date**: October 24, 2025  
**Status**: ✅ **PRODUCTION READY - ACTIVELY USED**

---

## 📋 Quick Navigation

- [What's Integrated](#whats-integrated) - Overview of what's active
- [Data Formats](#data-formats-important) - What's JSON vs Binary
- [spdlog Logging](#spdlog-logging-api) - Logging API and usage
- [magic_enum](#magic_enum-api) - Enum utilities API
- [nlohmann/json](#nlohmannjson-api) - JSON API
- [Active Integrations](#active-integrations) - Where libraries are used
- [Quick Reference](#quick-reference) - Code snippets

---

## What's Integrated

### ✅ **spdlog** - 51 Active Log Points

**Active in 5 components:**
- **vtpos** (3 points) - Loader startup
- **vt_main** (18 points) - System init, sockets, shutdown
- **vt_term** (16 points) - Terminal startup, connections
- **check.cc** (9 points) - Check save/close operations
- **credit.cc** (5 points) - Payment transactions

**Example logs:**
```
[2025-10-15 23:07:25.369] [ViewTouch] [info] ViewTouch Main (vt_main) starting - Version 25.02.0
[2025-10-15 23:15:45.125] [ViewTouch] [info] Check #1234 saved successfully
[2025-10-15 23:16:30.789] [ViewTouch] [info] Credit auth finalized - Approval: ABC123, Amount: $45.67
```

### ✅ **magic_enum** - 5 Active in Settings UI

**Active in `zone/settings_zone.cc`:**
- Drawer Mode (Trusted/Assigned/ServerBank)
- Receipt Print (OnSend/OnFinalize/OnBoth/Never)
- Time Format (Hour12/Hour24)
- Date Format (MMDDYY/DDMMYY)
- Number Format (Standard/Euro)

**7 enum types defined in `main/data/settings_enums.hh`**

### ✅ **nlohmann/json** - Templates Ready

**Created templates** (examples for future use):
- `config/example_terminal_config.json` - Terminal setup
- `config/example_settings.json` - Settings template

**Note**: ViewTouch data still uses binary format - JSON is ready for NEW features.

---

## Data Formats (IMPORTANT!)

### ❌ What's NOT Using JSON (Current Formats)

ViewTouch continues to use original formats:

| Data | Format | Location | Code |
|------|--------|----------|------|
| **Checks** | Custom binary (gzip) | `/usr/viewtouch/dat/current/` | `data_file.{hh,cc}` |
| **Settings** | Custom binary (gzip) | `/usr/viewtouch/dat/conf/` | `data_file.{hh,cc}` |
| **Drawers** | Custom binary (gzip) | `/usr/viewtouch/dat/current/` | `data_file.{hh,cc}` |
| **Archives** | Custom binary (gzip) | `/usr/viewtouch/dat/archive/` | `data_file.{hh,cc}` |
| **Tax configs** | INI files | `/usr/viewtouch/dat/conf/` | `conf_file.{hh,cc}` |

**Files created**:
- `check_*` - Binary check files
- `drawer_*` - Binary drawer files
- `tax.ini`, `fees.ini` - INI config files

### ✅ JSON Templates (Examples Only)

**What we created:**
- `config/example_terminal_config.json` - **Example template** (not loaded)
- `config/example_settings.json` - **Example template** (not loaded)

**Purpose**: Show how JSON **could** be used for new features

**JSON is ready for**:
- New configuration files
- Report exports
- External API data
- User preferences
- System monitoring

**Recommendation**: Keep existing formats (they work well), use JSON for NEW features only.

---

## spdlog Logging API

### Initialize

```cpp
#include "src/utils/vt_logger.hh"

// At program startup
#ifdef DEBUG
    vt::Logger::Initialize("/var/log/viewtouch", "debug", true, true);
#else
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
#endif
```

### Logging Methods

```cpp
// Log levels (least to most severe)
vt::Logger::trace("Very detailed info");
vt::Logger::debug("Debug info: {}", value);
vt::Logger::info("General info");
vt::Logger::warn("Warning: {}", message);
vt::Logger::error("Error: {}", error);
vt::Logger::critical("Critical: {}", fatal_error);

// Format strings
vt::Logger::info("Check #{} - Total: ${:.2f}", check_id, total);
vt::Logger::debug("User: {}, Terminal: {}", username, term_id);

// Macros (shorter syntax)
VT_LOG_INFO("Starting...");
VT_LOG_ERROR("Failed: {}", error);
```

### Shutdown

```cpp
// Before program exit
vt::Logger::Shutdown();
```

### Log Files

- **Location**: `/var/log/viewtouch/viewtouch.log`
- **Rotation**: 10MB per file, keeps 5 files
- **Format**: `[2025-10-16 14:30:45.123] [info] [pid:12345] Message`
- **Fallback**: Console if no write permissions

---

## magic_enum API

### Enum to String

```cpp
#include "src/utils/vt_enum_utils.hh"
#include "main/data/settings_enums.hh"

auto name = vt::EnumToString(DrawerModeType::Trusted);  // "Trusted"
```

### String to Enum

```cpp
auto mode = vt::StringToEnum<DrawerModeType>("Assigned");
if (mode) {
    // Use mode.value()
} else {
    vt::Logger::warn("Invalid mode");
}
```

### Int to Enum

```cpp
auto mode = vt::IntToEnum<DrawerModeType>(settings->drawer_mode);
if (mode) {
    auto display = vt::GetDrawerModeDisplayName(*mode);
}
```

### Get All Values

```cpp
// Iterate all values
for (auto mode : vt::GetEnumValues<DrawerModeType>()) {
    std::cout << vt::EnumToString(mode) << std::endl;
}

// Get count
size_t count = vt::GetEnumCount<DrawerModeType>();

// Get pairs for dropdowns
auto pairs = vt::GetEnumPairs<DrawerModeType>();
for (const auto& [name, value] : pairs) {
    AddMenuItem(name, vt::EnumToInt(value));
}
```

### Available Enums

**In `main/data/settings_enums.hh`:**
```cpp
enum class DrawerModeType { Trusted, Assigned, ServerBank };
enum class ReceiptPrintType { OnSend, OnFinalize, OnBoth, Never };
enum class TimeFormatType { Hour12, Hour24 };
enum class DateFormatType { MMDDYY, DDMMYY };
enum class NumberFormatType { Standard, Euro };
enum class SalesPeriodType { None, OneWeek, TwoWeeks, FourWeeks, Month };
enum class PrinterDestType { None, Kitchen1, Kitchen2, Bar1, Bar2, ... };
```

---

## nlohmann/json API

### JsonConfig Helper

```cpp
#include "src/utils/vt_json_config.hh"

// Create/Load
vt::JsonConfig cfg("/path/to/config.json");

if (cfg.Load()) {
    // Get values with defaults
    auto name = cfg.Get<std::string>("store_name", "Default");
    auto tax = cfg.Get<double>("tax.food", 0.07);
    auto count = cfg.Get<int>("count", 0);
    bool flag = cfg.Get<bool>("enabled", false);
    
    // Nested keys (use dot notation)
    auto terminal_name = cfg.Get<std::string>("terminals.0.name");
}

// Set values
cfg.Set("option", "value");
cfg.Set("nested.option", 42);
cfg.Set("array.0.item", "first");

// Check existence
if (cfg.Has("network.timeout")) {
    auto timeout = cfg.Get<int>("network.timeout");
}

// Remove keys
cfg.Remove("old_option");

// Save
if (cfg.Save(true, true)) {  // pretty print + backup
    vt::Logger::info("Config saved");
}
```

### Direct JSON Usage

```cpp
// Create JSON object
vt::json data = {
    {"name", "ViewTouch"},
    {"version", 25},
    {"enabled", true}
};

// Nested structures
vt::json config = {
    {"store", {
        {"name", "My Restaurant"},
        {"address", "123 Main St"}
    }},
    {"terminals", vt::json::array({
        {{"id", 1}, {"name", "Counter"}},
        {{"id", 2}, {"name", "Kitchen"}}
    })}
};

// Access
std::string name = data["name"];
int version = data["version"];

// Serialize
std::string json_str = data.dump(4);  // Pretty print, 4-space indent

// Parse from string
auto j = vt::json::parse(R"({"key": "value"})");
```

### Convenience Functions

```cpp
// Load JSON file
auto data = vt::LoadJsonFile("/path/to/file.json");
if (data) {
    std::string value = (*data)["key"];
}

// Save JSON file
vt::SaveJsonFile("/path/to/file.json", json_data, true);
```

---

## Active Integrations

### vtpos (loader/loader_main.cc)

```cpp
#include "src/utils/vt_logger.hh"

int main() {
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
    vt::Logger::info("ViewTouch Loader (vtpos) starting - Version {}", 
                     viewtouch::get_version_short());
    // ...
}
```

### vt_main (main/data/manager.cc)

```cpp
#include "src/utils/vt_logger.hh"

int main() {
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
    vt::Logger::info("ViewTouch Main (vt_main) starting - Version {}", version);
    vt::Logger::critical("Can't open initial loader socket - errno: {}", errno);
    vt::Logger::debug("Loader socket opened successfully: {}", socket);
    vt::Logger::info("Connected to loader successfully");
    vt::Logger::info("Initializing ViewTouch system...");
    vt::Logger::info("Using default data path: {}", path);
    vt::Logger::info("Starting ViewTouch system (network: {})", enabled ? "enabled" : "disabled");
    vt::Logger::info("ViewTouch system shutting down...");
    vt::Logger::Shutdown();
}
```

### vt_term (term/term_main.cc)

```cpp
#include "src/utils/vt_logger.hh"

int main() {
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
    vt::Logger::info("ViewTouch Terminal (vt_term) starting - Version {}", version);
    vt::Logger::debug("Using socket file: {}", socket_file);
    vt::Logger::debug("Socket opened: {}", socket_no);
    vt::Logger::info("Connected to server successfully");
    vt::Logger::info("Opening terminal - Display: {}, Hardware: {}", display, hw);
}
```

### Check Processing (main/business/check.cc)

```cpp
#include "src/utils/vt_logger.hh"

int Check::Save() {
    vt::Logger::debug("Saving check #{} - Serial: {}, Table: {}", 
                      checknum, serial_number, Table());
    // ... save logic ...
    if (result == 0) {
        vt::Logger::info("Check #{} saved successfully", checknum);
    } else {
        vt::Logger::error("Failed to save check #{}", checknum);
    }
}

int Check::Close(Terminal *term) {
    vt::Logger::info("Closing check #{} - User: {}", checknum, 
                     term->user ? term->user->system_name.Value() : "Unknown");
    // ... close logic ...
    if (closed) {
        vt::Logger::info("Check #{} closed successfully - {} subchecks", checknum, closed);
    } else {
        vt::Logger::warn("Check #{} close failed - no drawer", checknum);
    }
}
```

### Credit Cards (main/data/credit.cc)

```cpp
#include "src/utils/vt_logger.hh"

int Credit::Finalize(Terminal *term) {
    vt::Logger::debug("Finalizing credit transaction - Amount: ${:.2f}", amount / 100.0);
    
    if (IsPreauthed()) {
        vt::Logger::info("Credit preauth finalized - Approval: {}, Amount: ${:.2f}",
                        approval, preauth_amount / 100.0);
    } else if (IsAuthed()) {
        vt::Logger::info("Credit auth finalized - Approval: {}, Amount: ${:.2f}",
                        approval, auth_amount / 100.0);
    } else if (IsVoided()) {
        vt::Logger::info("Credit void finalized - Amount: ${:.2f}", void_amount / 100.0);
    } else if (IsRefunded()) {
        vt::Logger::info("Credit refund finalized - Amount: ${:.2f}", refund_amount / 100.0);
    }
}
```

### Settings UI (zone/settings_zone.cc)

```cpp
#include "main/data/settings_enums.hh"
#include "src/utils/vt_enum_utils.hh"
#include "src/utils/vt_logger.hh"

// Drawer Mode
case SWITCH_DRAWER_MODE:
    if (auto mode = vt::IntToEnum<DrawerModeType>(settings->drawer_mode)) {
        str = vt::GetDrawerModeDisplayName(*mode);
        vt::Logger::debug("Drawer mode: {}", str);
    }
    break;

// Receipt Print
case SWITCH_RECEIPT_PRINT:
    if (auto type = vt::IntToEnum<ReceiptPrintType>(settings->receipt_print)) {
        str = vt::GetReceiptPrintDisplayName(*type);
    }
    break;

// Time Format
case SWITCH_TIME_FORMAT:
    if (auto format = vt::IntToEnum<TimeFormatType>(settings->time_format)) {
        str = vt::GetTimeFormatDisplayName(*format);
    }
    break;
```

---

## Quick Reference

### Add Logging to Any File

```cpp
// 1. Include
#include "src/utils/vt_logger.hh"

// 2. Use
vt::Logger::info("Operation completed - Value: {}", value);
vt::Logger::error("Failed: {} - Code: {}", msg, code);
vt::Logger::debug("Debug: {}", data);
```

### Use Modern Enums

```cpp
// 1. Include
#include "main/data/settings_enums.hh"
#include "src/utils/vt_enum_utils.hh"

// 2. Use
auto mode = vt::IntToEnum<DrawerModeType>(value);
auto name = vt::GetDrawerModeDisplayName(*mode);
auto str = vt::EnumToString(DrawerModeType::Trusted);
```

### Use JSON (For New Configs)

```cpp
// 1. Include
#include "src/utils/vt_json_config.hh"

// 2. Use
vt::JsonConfig cfg("/path/to/config.json");
cfg.Set("option", value);
cfg.Save(true, true);
```

---

## Config File Examples

### Terminal Configuration

**File**: `config/example_terminal_config.json` (Template)

```json
{
    "terminals": [
        {
            "id": 1,
            "name": "Front Counter",
            "display": ":0.0",
            "type": "pos",
            "drawer_mode": "Trusted",
            "receipt_print": "OnFinalize",
            "screen_blank_time": 300
        },
        {
            "id": 2,
            "name": "Kitchen Display",
            "display": ":0.1",
            "type": "kitchen"
        }
    ],
    "printers": {
        "kitchen1": {
            "host": "192.168.1.100",
            "port": 9100,
            "model": "epson",
            "enabled": true
        },
        "receipts": {
            "host": "192.168.1.102",
            "port": 9100,
            "model": "star"
        }
    },
    "logging": {
        "level": "info",
        "log_directory": "/var/log/viewtouch"
    }
}
```

### Settings Configuration

**File**: `config/example_settings.json` (Template)

```json
{
    "store": {
        "name": "My Restaurant",
        "address": "123 Main Street",
        "phone": "555-1234"
    },
    "tax": {
        "food": 0.07,
        "alcohol": 0.09,
        "merchandise": 0.065
    },
    "settings": {
        "time_format": "Hour12",
        "date_format": "MMDDYY",
        "number_format": "Standard",
        "drawer_mode": "Trusted",
        "receipt_print": "OnFinalize",
        "screen_blank_time": 300,
        "use_seats": true,
        "language": "en_US"
    }
}
```

### Usage (When Needed)

```cpp
#include "src/utils/vt_json_config.hh"

// Load terminal config
vt::JsonConfig cfg("/usr/viewtouch/dat/conf/terminals.json");
if (cfg.Load()) {
    auto name = cfg.Get<std::string>("terminals.0.name");
    auto printer_host = cfg.Get<std::string>("printers.kitchen1.host");
    vt::Logger::info("Terminal: {}, Printer: {}", name, printer_host);
}
```

---

## Where to Use Next

### Add More Logging (Easy Wins)

1. **`main/data/manager.cc`** - ReportError() function
   ```cpp
   int ReportError(const std::string &message) {
       vt::Logger::error("{}", message);
       return 0;
   }
   ```

2. **`main/hardware/printer.cc`** - Printer operations
   ```cpp
   vt::Logger::info("Printing to {}", printer_name);
   vt::Logger::error("Printer connection failed: {}", error);
   ```

3. **`main/business/sales.cc`** - Sales processing
   ```cpp
   vt::Logger::info("Daily sales total: ${:.2f}", total);
   ```

### Replace More Enum Arrays

1. **`main/data/settings.cc`** - TenderName() arrays (line 3541)
2. **`main/data/credit.cc`** - CardType arrays (line 69)
3. **`zone/report_zone.cc`** - Use enums in reports

### Use JSON For New Features

1. **Export reports**: `Report::ExportToJSON()`
2. **User preferences**: Per-user JSON configs
3. **System status**: Export for monitoring
4. **External API**: Send/receive JSON data

---

## Build & Verification

### Build Status

```
✅ vtpos   - 628 KB  - With logging
✅ vt_main - 3.0 MB  - With logging
✅ vt_term - 2.3 MB  - With logging
✅ All programs compiled successfully
✅ Zero errors
```

### Verification

```
✅ spdlog: 51 active log points across 5 files
✅ magic_enum: 5 settings actively using enums
✅ nlohmann/json: Templates valid, helper ready
✅ All programs tested and logging correctly
```

### Test Commands

```bash
# Build
cd /home/ariel/Documents/viewtouchFork/build
make -j$(nproc)

# Test
./vtpos version     # See logging
./vt_main version   # See logging
./vt_term version   # See logging

# Install
sudo make install

# Run
sudo /usr/viewtouch/bin/vtpos

# Watch logs
sudo tail -f /var/log/viewtouch/viewtouch.log
```

---

## Migration Patterns

### Old vs New

```cpp
// OLD: printf/logmsg
printf("Check #%d - Total: $%.2f\n", id, total);
logmsg(LOG_INFO, "Starting...");
ReportError("Failed");

// NEW: spdlog
vt::Logger::info("Check #{} - Total: ${:.2f}", id, total);
vt::Logger::info("Starting...");
vt::Logger::error("Failed: {}", error);
```

```cpp
// OLD: Manual enum arrays
const char* DrawerModeName[] = {"Trusted", "Assigned", NULL};
int DrawerModeValue[] = {DRAWER_NORMAL, DRAWER_ASSIGNED, -1};
str = FindStringByValue(mode, DrawerModeValue, DrawerModeName);

// NEW: magic_enum
enum class DrawerModeType { Trusted, Assigned, ServerBank };
auto mode = vt::IntToEnum<DrawerModeType>(value);
str = vt::GetDrawerModeDisplayName(*mode);
```

```cpp
// OLD: Custom binary
OutputDataFile df;
df.Write(value);

// NEW: JSON (for new configs only)
vt::JsonConfig cfg("config.json");
cfg.Set("option", value);
cfg.Save();
```

---

## Code Examples

### Demo Programs

**`src/utils/modern_cpp_example.cc`** - Shows all 3 libraries:
```bash
cd build
g++ -std=c++20 -I.. -I../src -I_deps/*/include \
    ../src/utils/modern_cpp_example.cc \
    ../src/utils/vt_logger.cc ../src/utils/vt_json_config.cc \
    _deps/spdlog-build/libspdlog.a -lpthread \
    -o modern_cpp_demo
./modern_cpp_demo
```

**`main/data/terminal_config_example.cc`** - JSON config example (271 lines)

---

## Best Practices

### Logging
1. Use appropriate log levels (trace → debug → info → warn → error → critical)
2. Format strings efficiently: `vt::Logger::info("Value: {}", v)`
3. Initialize early, shutdown late

### Enums
1. Always check `std::optional` results from string parsing
2. Use display names for UI: `vt::GetDrawerModeDisplayName()`
3. Use `EnumToString()` for internal logging

### JSON
1. Use nested keys: `cfg.Set("network.terminal.name", ...)`
2. Always provide defaults: `cfg.Get<int>("timeout", 30)`
3. Create backups: `cfg.Save(true, true)`

---

## Files Created

### Core Infrastructure:
- `src/utils/vt_logger.{hh,cc}` - spdlog wrapper
- `src/utils/vt_json_config.{hh,cc}` - JSON helper
- `src/utils/vt_enum_utils.hh` - Enum utilities
- `main/data/settings_enums.hh` - Modern enum types

### Examples:
- `src/utils/modern_cpp_example.cc` - Demo
- `main/data/terminal_config_example.cc` - JSON demo
- `config/example_terminal_config.json` - Template
- `config/example_settings.json` - Template

### Modified:
- `CMakeLists.txt` - Library linking
- `loader/loader_main.cc` - Logging
- `main/data/manager.cc` - Logging
- `term/term_main.cc` - Logging
- `main/business/check.cc` - Logging
- `main/data/credit.cc` - Logging
- `zone/settings_zone.cc` - Enums

---

## Summary

**Libraries**: 3 integrated (spdlog, magic_enum, nlohmann/json)  
**Active Uses**: 51 log points + 5 enum types in production  
**Data Migration**: Not needed - JSON ready for NEW features only  
**Documentation**: All in this one file  
**Status**: ✅ Production ready  

**Test**: `sudo make install && sudo /usr/viewtouch/bin/vtpos`

---

**Last Updated**: October 16, 2025  
**Version**: 25.02.0+

