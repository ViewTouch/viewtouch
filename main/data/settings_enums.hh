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
 * settings_enums.hh - Modern enum definitions for settings
 * Uses magic_enum for automatic string conversions
 */

#ifndef SETTINGS_ENUMS_HH
#define SETTINGS_ENUMS_HH

#include "src/utils/vt_enum_utils.hh"
#include <string_view>

// Example: Modernized drawer mode enum
enum class DrawerModeType {
    Trusted = 0,    // DRAWER_NORMAL
    Assigned = 1,   // DRAWER_ASSIGNED  
    ServerBank = 2  // DRAWER_SERVER
};

// Receipt printing options - values must match RECEIPT_* constants
enum class ReceiptPrintType {
    Never = 0,       // RECEIPT_NONE
    OnSend = 1,      // RECEIPT_SEND
    OnFinalize = 2,  // RECEIPT_FINALIZE
    OnBoth = 3       // RECEIPT_BOTH
};

// Drawer print options - values must match DRAWER_PRINT_* constants
enum class DrawerPrintType {
    Never = 0,     // DRAWER_PRINT_NEVER
    OnPull = 1,    // DRAWER_PRINT_PULL
    OnBalance = 2, // DRAWER_PRINT_BALANCE
    OnBoth = 3     // DRAWER_PRINT_BOTH
};

// Example: Price rounding options
enum class PriceRoundingType {
    None = 0,              // ROUNDING_NONE
    DropPennies = 1,       // ROUNDING_DROP_PENNIES
    RoundUpGratuity = 2    // ROUNDING_UP_GRATUITY
};

// Example: Time format options
enum class TimeFormatType {
    Hour12 = 0,  // TIME_12HOUR
    Hour24 = 1   // TIME_24HOUR
};

// Example: Date format options
enum class DateFormatType {
    MMDDYY = 0,  // DATE_MMDDYY
    DDMMYY = 1   // DATE_DDMMYY
};

// Example: Number format options
enum class NumberFormatType {
    Standard = 0,  // NUMBER_STANDARD (1,000,000.00)
    Euro = 1       // NUMBER_EURO (1.000.000,00)
};

// Example: Sales period options
enum class SalesPeriodType {
    None = 0,      // SP_NONE
    OneWeek = 1,   // SP_WEEK
    TwoWeeks = 2,  // SP_2WEEKS
    FourWeeks = 3, // SP_4WEEKS
    Month = 4,     // SP_MONTH
    HM1126 = 5     // SP_HM_11
};

// Example: Printer destinations
enum class PrinterDestType {
    None = 0,           // PRINTER_NONE
    Kitchen1 = 1,       // PRINTER_KITCHEN1
    Kitchen2 = 2,       // PRINTER_KITCHEN2
    Bar1 = 3,           // PRINTER_BAR1
    Bar2 = 4,           // PRINTER_BAR2
    Expediter = 5,      // PRINTER_EXPEDITER
    Kitchen1Notify = 6, // PRINTER_KITCHEN1_NOTIFY
    Kitchen2Notify = 7, // PRINTER_KITCHEN2_NOTIFY
    RemoteOrder = 8,    // PRINTER_REMOTEORDER
    Default = 9         // PRINTER_DEFAULT
};

namespace vt {

// Helper functions to get display names (translated versions)
// These can be used in UI dropdowns, reports, etc.

inline const char* GetDrawerModeDisplayName(DrawerModeType mode) {
    switch (mode) {
        case DrawerModeType::Trusted: return "Trusted";
        case DrawerModeType::Assigned: return "Assigned";
        case DrawerModeType::ServerBank: return "Server Bank";
        default: return "Unknown";
    }
}

inline const char* GetReceiptPrintDisplayName(ReceiptPrintType type) {
    switch (type) {
        case ReceiptPrintType::OnSend: return "On Send";
        case ReceiptPrintType::OnFinalize: return "On Finalize";
        case ReceiptPrintType::OnBoth: return "On Both";
        case ReceiptPrintType::Never: return "Never";
        default: return "Unknown";
    }
}

inline const char* GetDrawerPrintDisplayName(DrawerPrintType type) {
    switch (type) {
        case DrawerPrintType::OnPull: return "On Pull";
        case DrawerPrintType::OnBalance: return "On Balance";
        case DrawerPrintType::OnBoth: return "On Both";
        case DrawerPrintType::Never: return "Never";
        default: return "Unknown";
    }
}

inline const char* GetTimeFormatDisplayName(TimeFormatType format) {
    switch (format) {
        case TimeFormatType::Hour12: return "12 hour";
        case TimeFormatType::Hour24: return "24 hour";
        default: return "Unknown";
    }
}

inline const char* GetDateFormatDisplayName(DateFormatType format) {
    switch (format) {
        case DateFormatType::MMDDYY: return "MM/DD/YY";
        case DateFormatType::DDMMYY: return "DD/MM/YY";
        default: return "Unknown";
    }
}

inline const char* GetNumberFormatDisplayName(NumberFormatType format) {
    switch (format) {
        case NumberFormatType::Standard: return "1,000,000.00";
        case NumberFormatType::Euro: return "1.000.000,00";
        default: return "Unknown";
    }
}

// Example usage functions
inline void LogDrawerModeChange(DrawerModeType old_mode, DrawerModeType new_mode) {
    auto old_name = EnumToString(old_mode);
    auto new_name = EnumToString(new_mode);
    // Would log: "Drawer mode changed from Trusted to Assigned"
}

// Get all available options for UI dropdowns
inline auto GetAllDrawerModes() {
    return GetEnumPairs<DrawerModeType>();
}

inline auto GetAllReceiptPrintOptions() {
    return GetEnumPairs<ReceiptPrintType>();
}

inline auto GetAllDrawerPrintOptions() {
    return GetEnumPairs<DrawerPrintType>();
}

inline auto GetAllTimeFormats() {
    return GetEnumPairs<TimeFormatType>();
}

} // namespace vt

/*
 * MIGRATION NOTES:
 * ================
 * 
 * OLD WAY (manual arrays in settings.cc):
 * ---------------------------------------
 * const char* DrawerModeName[] = {
 *     GlobalTranslate("Trusted"), GlobalTranslate("Assigned"), 
 *     GlobalTranslate("Server Bank"), nullptr};
 * int DrawerModeValue[] = {
 *     DRAWER_NORMAL, DRAWER_ASSIGNED, DRAWER_SERVER, -1};
 *
 * NEW WAY (using magic_enum):
 * ---------------------------
 * enum class DrawerModeType { Trusted, Assigned, ServerBank };
 * 
 * // Get name: vt::EnumToString(DrawerModeType::Trusted) -> "Trusted"
 * // Parse: vt::StringToEnum<DrawerModeType>("Assigned") -> DrawerModeType::Assigned
 * // All values: vt::GetEnumValues<DrawerModeType>()
 * // For dropdowns: vt::GetAllDrawerModes()
 *
 * BENEFITS:
 * ---------
 * ✅ No manual array maintenance
 * ✅ Type-safe conversions
 * ✅ Compile-time validation
 * ✅ No nullptr terminators to forget
 * ✅ Automatic JSON serialization
 * ✅ Better logging with vt::Logger
 */

#endif // SETTINGS_ENUMS_HH

