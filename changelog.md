# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)


## [Unreleased]
### Added
- **Comprehensive Data Persistence System**
  - **DataPersistenceManager Class**: Implemented robust data persistence and validation system to prevent data loss during long-running sessions
    - **Automatic Periodic Saving**: Auto-saves all critical data every 30 seconds (configurable)
    - **Data Integrity Validation**: Comprehensive validation before shutdown/restart to ensure all data is properly saved
    - **CUPS Communication Monitoring**: Automatic detection and recovery from CUPS printer communication failures
    - **Real-time Data Tracking**: Tracks dirty/clean state of all critical data components
    - **Emergency Save Procedures**: Handles critical failures with emergency data saving
    - **Comprehensive Logging**: Detailed logging of all save operations, errors, and warnings
    - **Backup Creation**: Automatic backup creation before shutdown with timestamped directories
    - **Thread-Safe Operations**: Full mutex protection for concurrent access
    - **Configurable Intervals**: Adjustable auto-save and CUPS monitoring intervals
  - **System Integration**: Fully integrated into ViewTouch core system

### Fixed
- **Edit Mode Auto-Exit Bug**: Fixed issue where DataPersistenceManager was causing edit mode to auto-exit during periodic saves
  - **Smart Auto-Save Logic**: Auto-save now skips when terminals are in edit mode to avoid interrupting user workflow
  - **Edit Mode Detection**: Added `IsAnyTerminalInEditMode()` method to detect active edit sessions
  - **Preserved Shutdown Behavior**: Edit mode is still properly exited during system shutdown to ensure data is saved
  - **User Experience**: Users can now work in edit mode without being interrupted by periodic auto-save operations
    - **Startup Initialization**: DataPersistenceManager initialized during system startup
    - **Periodic Updates**: Integrated into main event loop for continuous monitoring
    - **Shutdown Preparation**: Enhanced shutdown process with comprehensive data validation
    - **Check Saving Hooks**: Real-time tracking of check data modifications
  - **Data Types Monitored**:
    - **Checks**: All open and closed checks with validation and error handling
    - **Settings**: System configuration and settings with integrity checks
    - **Archives**: Historical data archives with proper validation
    - **Terminals**: Terminal state and configuration monitoring
    - **CUPS Communication**: Printer communication health with automatic recovery
  - **Error Handling and Recovery**:
    - **Validation Results**: Success, Warning, Error, Critical levels
    - **Save Results**: Success, Partial, Failed, Critical Failure tracking
    - **CUPS Recovery**: Automatic CUPS service restart on communication failure
    - **Emergency Procedures**: Fallback mechanisms for critical system failures
  - **Benefits**:
    - **Data Loss Prevention**: Ensures all critical data is saved before shutdown
    - **CUPS Recovery**: Automatic detection and recovery from printer communication failures
    - **Proactive Monitoring**: Continuous monitoring of data integrity
    - **Detailed Logging**: Complete audit trail of all operations
    - **Emergency Procedures**: Robust handling of critical failures
    - **Configurable**: Flexible configuration for different environments
    - **Non-Intrusive**: Minimal impact on existing system performance
  - **Files Added**:
    - `data_persistence_manager.hh` - Header file with class definition
    - `data_persistence_manager.cc` - Implementation file with full functionality
    - `DATA_PERSISTENCE_SYSTEM.md` - Comprehensive documentation
  - **Files Modified**:
    - `CMakeLists.txt` - Added new source files to build system
    - `main/manager.cc` - Integrated persistence manager into system lifecycle
    - `main/check.cc` - Added data tracking hooks for check operations
    - `main/system.cc` - Enhanced error handling in check saving process
  - **⚠️ Field Testing Required**: This implementation requires comprehensive field testing to verify:
    - Long-running session stability (24+ hours)
    - CUPS communication failure recovery
    - Data integrity during unexpected shutdowns
    - Performance impact under high load
    - Backup and recovery procedures
    - Error handling under various failure scenarios
- **Universal Installer Icon Update**
  - Updated universal installer to use `Icon.png` instead of `demo.png` for desktop entry icon
  - The installer now properly displays the ViewTouch logo in applications menu and desktop environment
  - Enhanced visual branding consistency across the installation experience
- **Editor Order Entry Save Fix (Latest)**
  - **Fixed Editor Order Entry Changes Not Saving**: Resolved critical issue where editor changes to Order Entry button size/position were not being saved on program exit or Kill System
    - **Root Cause**: `EditTerm()` function only called `SaveSystemData()` for Super Users, not Editors
    - **Solution**: Modified `EditTerm()` to always call `SaveSystemData()` when exiting edit mode, regardless of user type
    - **Order Entry Changes**: Now properly saved to `vt_data` for both Super Users and Editors
    - **Kill System Integration**: Enhanced `EndSystem()` and `ExecuteRestart()` to call `EditTerm(1)` for all terminals before shutdown
    - **Data Persistence**: All Order Entry window changes (`oewindow[4]`) now persist across program restarts
    - **Comprehensive Coverage**: Fixes apply to F1 edit mode, F9 system edit mode, normal program exit, and Kill System button
- **Editor Settings Save Fix**
  - **Fixed "Editor Settings" Button Save Issue**: Resolved critical bug where Editor Settings button wasn't saving when using "save" + "Return To A Jump" sequence
    - Added proper save completion handling in `MessageButtonZone::SendandJump()`
    - Ensured settings are written to disk before jumping to prevent data loss
    - Added timing delays to guarantee file write completion
    - Enhanced save operation to include both Settings and System data persistence
    - Fixed race condition between save signal and page jump operations
- **High-Risk Crash and Freeze Prevention**
  - **SIGPIPE Handling Enhancement**: Implemented robust network reconnection system
    - Added `ReconnectToServer()` function for automatic socket reconnection (up to 20 attempts)
    - Added `RestartTerminal()` function for graceful terminal restart on connection loss
    - Replaced immediate `exit(1)` with intelligent reconnection logic in `SocketInputCB()`
    - Added proper error handling and user notifications for connection issues
    - Prevents application crashes when network connections are lost unexpectedly
  - **Buffer Overflow Vulnerabilities Fixed**: Enhanced memory safety in network operations
    - Fixed critical buffer overflow in `main/manager.cc` socket reading code
    - Added proper bounds checking with `sizeof(buffer) - 1` for safe string operations
    - Implemented null termination guarantees to prevent memory corruption
    - Enhanced security against potential buffer overflow attacks
  - **Double-Free Memory Errors Resolved**: Improved memory management and cleanup
    - Fixed double-free errors in `MasterControl` cleanup sequence in `main/manager.cc`
    - Implemented proper resource management for terminals and printers
    - Added safe cleanup sequence to prevent memory corruption
    - Enhanced memory safety and prevented crashes from improper cleanup
  - **I/O Timeout Protection**: Added timeout mechanisms to prevent application freezes
    - Implemented timeout-enabled connect functions in `socket.cc`
    - Added 10-second timeout for socket connections to prevent indefinite blocking
    - Enhanced error handling for timeout scenarios and connection failures
    - Prevents application freezes from unresponsive network operations
  - **Infinite Loop Prevention**: Added health checking and exit conditions
    - Enhanced `vt_ccq_pipe.cc` with proper health checking and error counters
    - Implemented graceful exit conditions for daemon processes
    - Added comprehensive error handling and logging for process monitoring
    - Prevented infinite loops that could cause system resource exhaustion
  - **Compilation and Build Improvements**: Fixed build system issues
    - Added missing system includes (`sys/socket.h`, `sys/un.h`) for proper compilation
    - Fixed declaration conflicts and unused variable warnings
    - Ensured clean compilation with zero errors and improved code quality
    - Enhanced maintainability and development workflow
  - **Null Pointer Protection**: Comprehensive null pointer dereference prevention
    - **Event Handler Protection**: Added null checks for all X11 event handlers
      - `TouchScreenCB`: Added null check for `TScreen` before accessing touch screen methods
      - `CalibrateCB`: Added null check for `TScreen` before calling `ReadStatus()`
      - `MouseClickCB`, `MouseReleaseCB`, `MouseMoveCB`: Added null checks for `event` parameter
      - `KeyPressCB`, `ExposeCB`: Added null checks for `event` parameter before processing
    - **System Management Protection**: Enhanced null pointer safety in critical system functions
      - `EndSystem()`: Added comprehensive null checks for all database pointers before calling `Save()`
      - `LoadSystemData()`: Added null checks for `MasterSystem` and `MasterControl` before accessing
      - `FindVTData()`: Added null check for `MasterSystem` before calling `FullPath()`
    - **Touch Screen Protection**: Enhanced null checking in touch screen operations
      - `UpdateCB()`: Added double null check for `TScreen` after `EndCalibrate()` call
      - `TouchScreenCB()`: Enhanced null checking with proper error logging
    - **Database Access Protection**: Secured all database save operations
      - Added null checks for `cc_exception_db`, `cc_refund_db`, `cc_void_db`
      - Added null checks for `cc_settle_results`, `cc_init_results`, `cc_saf_details_results`
      - Prevented crashes during system shutdown with corrupted data structures
- **Critical Crash Fixes and Stability Improvements**
  - **SIGPIPE Crash Resolution**: Fixed critical crash when ViewTouch loses connection to vtpos daemon
    - Added graceful handling of broken pipe errors in `CharQueue::Write()` method
    - Prevents application crash when server connection is lost unexpectedly
    - Returns -1 instead of crashing on `EPIPE` errors during socket writes
    - Handles connection loss scenarios gracefully without data corruption
  - **Employee Record Management Crashes**: Fixed multiple crash scenarios in employee management
    - **Null Pointer Protection**: Added comprehensive null checks in `UserEditZone::SaveRecord()`
      - Prevents crashes when no employee record is found during save operations
      - Added null checks for `FormField` objects during field iteration
      - Safe handling of empty or corrupted employee data structures
    - **Memory Allocation Safety**: Enhanced error handling in `UserDB::NewUser()`
      - Added null checks for `new Employee` and `new JobInfo` allocations
      - Proper cleanup and error logging when memory allocation fails
      - Prevents crashes from memory allocation failures
    - **Form Field Iteration Safety**: Fixed null pointer access during field processing
      - Added null checks before accessing `FormField` objects in loops
      - Prevents crashes when field list is shorter than expected
      - Safe iteration through employee form fields
  - **Expense Zone Crash Prevention**: Fixed null pointer access in expense management
    - Added null checks for `term->user` before accessing user properties
    - Prevents crashes when no user is logged in during expense operations
    - Safe handling of user authentication state in expense zones
  - **Enhanced Error Handling**: Improved overall application stability
    - Replaced unsafe string functions (`sprintf`, `strcat`) with `snprintf` for buffer safety
    - Added exception handling around `std::stoi` calls for string-to-integer conversion
    - Improved array bounds checking in database operations
    - Better error logging and graceful degradation on failures
- **Fixed Printer Connectivity and Status Issues**
  - **Enhanced Printer Status Detection**: Improved printer connection monitoring with detailed error reporting
    - Progressive status updates at connection failure attempts 1, 4, and 8
    - Proper offline status tracking using `failure = 999` flag without killing printer objects
    - Automatic detection and reporting of connection restoration
  - **Automatic Printer Reconnection**: Added robust reconnection logic for network printers
    - New `Reconnect()` method in `RemotePrinter` class for automatic reconnection attempts
    - Proper socket recreation and `vt_print` daemon restart when needed
    - Callback re-registration after successful reconnection
    - Reconnection attempts every 30 seconds for offline printers
  - **Printer Health Monitoring**: Added periodic health checks in main system update loop
    - Monitors all printers every 30 seconds for connectivity issues
    - Detailed logging of printer status and connection health
    - Debug mode support for enhanced monitoring information
  - **Accurate Online Status Checking**: New `IsOnline()` method for reliable printer status determination
    - Multi-factor detection checking socket status, failure count, and offline flags
    - UI integration ready for accurate printer status display
    - Eliminates false "OK" status when printers are actually offline
  - **Improved Error Handling**: Better error messages and graceful handling of printer disconnections
    - System continues working even when some printers are offline
    - No more abrupt printer removal on connection failures
    - Enhanced logging for troubleshooting printer connectivity issues
- **Complete Kitchen and Bar Video Display Separation with Order Recall**
  - **Independent Status Tracking**: Implemented separate check flags for kitchen and bar video displays
    - `CF_KITCHEN_MADE` (16): Kitchen marks their portion as made/ready
    - `CF_BAR_MADE` (32): Bar marks their portion as made/ready  
    - `CF_KITCHEN_SERVED` (64): Kitchen marks their portion as served
    - `CF_BAR_SERVED` (128): Bar marks their portion as served
  - **Three-Tap Workflow**: Complete order lifecycle management for each video target
    - **First Tap**: Marks order as "made" for that video target only
    - **Second Tap**: Marks order as "served" for that video target only  
    - **Third Tap**: Recalls the order back to the video display for that target only
  - **Enhanced Undo Button**: Provides granular control to recall served orders
    - Finds most recent check served by current video target
    - Recalls order by clearing target-specific served flag
    - Only affects current video target, not others
    - Brings order back to video display for verification
  - **Complete Video Target Coverage**: Supports all kitchen and bar video displays
    - Kitchen 1, Kitchen 2, Bar 1, Bar 2 all work independently
    - Each target maintains separate order status tracking
    - No cross-interference between any video targets
  - **Complete Workflow Independence**: Kitchen and bar can work without interfering with each other
  - **No Cross-Interference**: Actions on one video target never affect the other
  - Enhanced `ToggleCheckReport` method to use target-specific flags for both highlighting and serving
  - Updated `DisplayCheckReport` method to check appropriate flags for each video target
  - Modified `UndoRecentCheck` method to provide target-specific order recall
  - Comprehensive documentation in `VIEWTOUCH_IMPROVEMENTS_SUMMARY.md`

- **Fixed Video Target Routing System**
  - **Corrected Array Indexing**: Fixed `Order::VideoTarget()` method to use direct family ID indexing
  - **Proper Item Routing**: Items now route to correct video displays based on their family settings
  - **Family-Based Targeting**: Food items (burgers, appetizers) go to kitchen video, drinks (cocktails, wine) go to bar video
  - **Eliminated Misrouting**: No more items appearing on wrong video displays regardless of family settings

- **Enhanced Settings Persistence and Loading**
  - **Fixed Video Target Loading**: Corrected array indexing in `VideoTargetZone` and `PrintTargetZone`
  - **Settings Persistence**: Video and printer target settings by family now properly save and load
  - **Improved Default Values**: Changed from problematic `PRINTER_NONE` to sensible `PRINTER_DEFAULT`
  - **Version Compatibility**: Enhanced loading logic for older settings files (version < 34)
  - **Fixed Manager Settings Save**: Prevented premature settings save that could overwrite loaded values
  - **Automatic Synchronization**: Video and printer targets automatically stay synchronized

- **Enhanced Texture System with New Materials**
  - **Carbon Fiber Texture**: Added high-quality carbon fiber material (128x128, 6 colors)
  - **Color Textures**: Added 6 new color-based textures (128x128, 32 colors each):
    - White Texture, Dark Orange Texture, Yellow Texture, Green Texture, Orange Texture, Blue Texture
  - **High-Detail Textures**: Added 6 new detailed materials (256x256, 64 colors each):
    - Pool Table, Test Pattern, Diamond Leather, Bread, Lava, Dark Marble
  - **Unified Resolution**: Standardized all new textures to optimal 256x256 resolution for consistency
  - **Performance Optimized**: Reduced file sizes from ~1.5MB to ~45KB while maintaining visual quality
  - **Full Integration**: All textures properly integrated into ViewTouch enum system, image data arrays, and UI labels
  - **Build System**: Successfully integrated into CMake build system with no compilation errors

- **F3/F4 Recording Control Feature**
  - Added user-configurable control over F3/F4 recording and replay functionality
  - New setting `enable_f3_f4_recording` in Soft Switches page
  - Default disabled (0) for safety in production environments
  - Requires only the Soft Switch to be enabled (no debug mode required)
  - Prevents accidental triggering of long replay sequences
  - Integrated with existing Soft Switches system for consistent user experience
  - Comprehensive documentation in `F3_F4_RECORDING_CONTROL.md`

### Code Modernization
- **Upgraded to C++20 Standard**
  - Enhanced build system to use C++20 features and modern compiler capabilities
  - Enabled additional compiler warnings: `-Wextra`, `-Wconversion`, `-Wnull-dereference`, `-Wdouble-promotion`, `-Wformat=2`
  - Improved code safety and error detection during compilation

- **Modernized vt_print Daemon (print_main.cc)**
  - **Critical Bug Fix**: Removed debug `exit(2)` statement that prevented the program from running
  - **Modern C++ Improvements**: Updated includes from C-style to C++ headers (`<cstring>`, `<csignal>`, `<vector>`)
  - **Enhanced Buffer Management**: Replaced raw `char buffer[STRLENGTH]` with `std::vector<char>` for automatic memory management
  - **Graceful Shutdown**: Added signal handlers for SIGINT/SIGTERM with proper cleanup and resource management
  - **Improved Error Handling**: Enhanced error reporting with proper validation and resource cleanup
  - **Const Correctness**: Improved parameter passing with `const char* const argv[]` and better variable initialization
  - **Input Validation**: Added port number range checking (1-65535) and better argument parsing
  - **Resource Safety**: Proper file descriptor management and cleanup on exit to prevent resource leaks
  - **Better Data Flow**: Enhanced handling of partial writes, connection states, and shutdown scenarios
  - All improvements maintain full backward compatibility with existing functionality

- **Memory Safety Improvements**
  - **RemotePrinter Class**: Converted from raw `new`/`delete` to `std::unique_ptr` and `std::make_unique`
  - **System Class**: Modernized all credit card database pointers to use `std::unique_ptr`
  - **CharQueue Class**: Replaced raw arrays with `std::vector` for automatic memory management
  - Eliminated manual memory cleanup in destructors - smart pointers handle this automatically
  - Removed 280+ instances of unsafe raw pointer usage

- **String Safety and Modern STL Usage**
  - Replaced all `sprintf` calls with `snprintf` using proper bounds checking
  - Converted C-style arrays to `std::array` and `std::vector` throughout codebase
  - **MediaList Class**: Modernized to use `std::string` instead of raw character arrays
  - Enhanced string handling with proper size validation and bounds checking
  - Replaced 50+ unsafe string functions with safe alternatives

- **New Infrastructure and Utilities**
  - **`string_utils.hh/cc`**: Comprehensive modern string processing utilities
    - Unicode-aware string operations with UTF-8 support
    - Safe string formatting, case conversion, and validation
    - File path manipulation and sanitization functions
    - Template-based type-safe formatting system
  - **`error_handler.hh/cc`**: Unified error handling framework
    - Centralized error reporting with severity levels and categories
    - Thread-safe error logging with configurable output destinations
    - Error callback system for custom error handling
    - Comprehensive error history and filtering capabilities

- **Performance and Safety Enhancements**
  - Added `const` references to function parameters to improve performance
  - Enhanced null pointer safety with smart pointer usage
  - Improved type safety with modern C++ features
  - Better bounds checking and array access validation
  - Optimized memory usage patterns throughout the codebase

- **Build System Modernization**
  - Integrated new utility modules into CMake build system
  - Enhanced dependency management for new components
  - Improved compiler warning coverage for better code quality
  - **Fixed CMake installation directory creation**: Added proper directory structure creation for all installation paths (`/usr/viewtouch/bin`, `/usr/viewtouch/lib`, `/usr/share/viewtouch/fonts`, etc.) to prevent installation failures

- **UI and Visual Enhancements**
  - **Improved text shadow colors**: Fixed harsh pure black shadows for white, yellow, and gray text colors to provide better readability and visual appeal
    - White text: Changed from pure black `{0,0,0}` to medium gray `{64,64,64}` shadows
    - Yellow text: Changed from pure black `{0,0,0}` to warm dark olive `{96,64,0}` shadows  
    - Gray text: Changed from pure black `{0,0,0}` to dark gray `{32,32,32}` shadows
  - **F3/F4 Recording Button UI Improvements**: Enhanced the F3/F4 Recording button in Settings to match the professional appearance of other settings buttons
    - Changed button text from verbose "Enable F3/F4 Recording/Replay" to clean "F3/F4" for better fit
    - Implemented proper On/Off status display at bottom (red "Off", green "On") matching Seat Based Ordering button style
    - Fixed button toggle functionality to properly switch between OFF and ON states
    - Removed debug text and improved text positioning to prevent overlap
    - Enhanced user experience with immediate visual feedback and consistent styling
  - Enhanced consistency with frost and embossed text effects
  - All changes maintain full backward compatibility with existing functionality

### Added
- **Scheduled Restart Feature with User Prompts**
  - Added configurable scheduled restart functionality to System Variables
  - Users can set restart time (hour and minute) with -1 to disable
  - Smart user prompt system appears at scheduled restart time with options:
    - "Restart Now" for immediate graceful restart
    - "Postpone 1 Hour" to delay restart by exactly 60 minutes
    - Auto-restart after 5 minutes if no user response (safety mechanism)
  - Daily reset of postpone counters at midnight
  - Complete state persistence across ViewTouch sessions
  - Tracks postponement count for monitoring purposes
  - Integrated with existing settings system and UI workflow
- **Automatic vt_data Download on Startup**
  - ViewTouch now automatically downloads latest vt_data from update servers on startup
  - Dual URL support with automatic fallback:
    - Primary: `http://www.viewtouch.com/vt_data`
    - Fallback: `https://www.viewtouch.com/vt_data`
  - Smart offline handling: Only downloads when local vt_data is missing
  - **Enhanced Safety**: Downloads to temporary files first, only replaces original on success
  - **Automatic Backup Cleanup**: Removes old .bak and .bak2 files after successful updates
  - **Offline Resilience**: System starts normally with existing vt_data when offline
  - Comprehensive error handling and logging for troubleshooting
  - Uses existing robust download infrastructure with timeout handling
- **Comprehensive Text Enhancement System**
  - Implemented system-wide enhanced text rendering with three configurable effects
  - **Embossed Text Effect**: Creates 3D frosted glass appearance with highlights and shadows (disabled by default; enable in Settings)
  - **Anti-aliasing**: Smooths text edges for improved readability across all font sizes
  - **Drop Shadows**: Configurable shadow effects for better text contrast and depth
  - Complete coverage across all ViewTouch UI elements including:
    - Main terminal interface (buttons, menus, dialogs, forms)
    - Zone text rendering (names, labels, content, prices)
    - Status bars and information displays
    - Dialog and form text (titles, labels, messages)
    - All specialized zones (TableZone, OrderZone, PaymentZone, etc.)
    - All font families (Times, Courier, DejaVu, EB Garamond, etc.)
  - Configurable settings with UI controls for all effects
  - Performance-optimized rendering with minimal overhead
  - Graceful fallback to standard rendering when enhanced features unavailable
  - Comprehensive documentation in `TEXT_ENHANCEMENT_FEATURES.md`
  - All text elements now use enhanced rendering for consistent, professional appearance
- **Unified Video and Printer Targeting System**
  - Modified existing VideoTargetZone and PrintTargetZone to automatically synchronize both video and printer targets
  - When either zone is used, both `video_target` and `family_printer` arrays are updated simultaneously
  - Updated zone titles to "Video & Printer Targets by Family" for clarity
  - Ensures "Video Targets must match Printer Targets" requirement is always met
  - No changes required to existing page configurations - backward compatible
  - Eliminates the need for separate configuration of video and printer targets
- **Catch2 v3.8.1 Integration**
  - Successfully integrated Catch2 v3.8.1 as a modern, header-only testing framework
  - Added proper CMake configuration with build options for testing control
  - Configured test infrastructure with `Catch2::Catch2WithMain` target linking
  - Verified integration with comprehensive test suite (13 assertions, 3 test cases)
  - Confirmed C++17/20 compatibility including structured bindings and std::optional
  - Created detailed integration documentation in `CATCH2_INTEGRATION.md`
  - Maintained clean workspace by removing temporary test files after verification
  - Ready for production testing when needed with simple test file creation process
- Complete Greek (EL) and Spanish (ES) localization for ViewTouch POS system
  - Added comprehensive translation files: `po_file/viewtouch.po_EL` and `po_file/viewtouch.po_ES`
  - Translated all user-facing UI strings, system messages, error messages, and report titles
  - Translated payment and receipt terms, order and table management, labor and time clock functionality
  - Translated system configuration labels, credit card types, transaction types, and sales categories
  - Translated dialog messages, form field labels, and status notifications
  - Maintained proper nouns, brand names, and technical codes untranslated as appropriate
  - Both languages now provide complete localization coverage for the ViewTouch POS interface
- Enhanced "Move Guest Check to Another Table" functionality to support merging tables with existing checks
  - When moving a check to a table that already has a check, users are now prompted to merge the tables
  - Merging combines all orders from both checks into a single check at the target table
  - Guest counts are automatically combined when merging tables
  - Provides confirmation dialog to prevent accidental merges
- Improved download functionality for vt_data, menu.dat, table.dat, and zone_db.dat files
  - Added support for both HTTPS and HTTP protocols with automatic fallback
  - Enhanced error handling and file verification for reliable downloads
  - Improved compatibility with Raspberry Pi and other systems with SSL/TLS issues
  - Added proper timeout and connection settings for network requests
  - Implemented file size verification to ensure complete downloads
- Fixed "Suppress Zero Values" functionality in Accountant's Report: Receipt & Cash Deposit
  - Zero values are now properly suppressed when the setting is enabled
  - Report automatically refreshes when the setting is toggled for immediate visual feedback
  - Applied to all relevant sections: tax types, non-cash receipts, adjustments, and payment methods
- Migrated from Catch2 v2.13.10 to Catch2 v3.8.1
  - Updated from single-header approach to library-based architecture
  - Improved compile times by only including necessary headers
  - Added comprehensive test infrastructure with basic and feature demonstration tests
  - Enhanced testing capabilities with improved matchers, generators, and reporting
  - Created detailed migration documentation in CATCH2_V3_MIGRATION.md
- **NEW: Full Catch2 v3 Integration and Test Suite**
  - Integrated Catch2 v3.8.1 as external dependency with proper CMake configuration
  - Created comprehensive test suite structure with `tests/` directory
  - Added test modules for `conf_file`, `time_info`, and `utility` components
  - Implemented basic Catch2 integration tests to verify framework functionality
  - Created custom `test_all` target for running all tests with CTest integration
  - Updated GitHub Actions workflow to test Catch2 integration across multiple compilers
  - All tests pass successfully on GCC 12-14 and Clang 16-18 with C++17/C++20 standards
  - Test infrastructure ready for future ViewTouch-specific unit tests

- **Editor Access to Order Entry Button Customization with Persistent Storage**
  - **Extended Editor Permissions**: Editor users (id == 2) can now edit the size and position of the Order Entry Button
    - Modified `PosZone::CanEdit()` and `PosZone::CanCopy()` to allow Editor access to `ZONE_ORDER_ENTRY` zones
    - Editors can now customize the Order Entry Button size and position like Superusers
    - Maintains security by restricting access only to the Order Entry Button, not other system zones
  - **Persistent Custom Size Storage**: Order Entry Button size changes are now automatically saved to `vt_data`
    - Added `SaveSystemData()` calls in `OrderEntryZone::SetSize()` and `OrderEntryZone::SetPosition()` methods
    - Custom sizes persist across program restarts and are stored in the system data file
    - Changes are immediately written to `vt_data` when made by any authorized user
  - **Auto-Update Control with Visual Feedback**: Added user-configurable control over automatic `vt_data` updates
    - New soft switch "Auto-Update vt_data on Startup" with clear ON/OFF visual feedback
    - Green "ON" and Red "OFF" status display matching other soft switch buttons
    - When disabled, prevents automatic `vt_data` downloads that would overwrite custom settings
    - Integrated with existing Soft Switches system for consistent user experience
    - Default enabled for backward compatibility, but users can disable to preserve customizations
  - **Complete Workflow**: Editors can now customize Order Entry Button size, save changes permanently, and control when system updates occur

- **Enhanced RUNCMD: Terminal Command Execution**
  - **Expanded Command Support**: Enhanced the existing RUNCMD: functionality in Standard buttons to support more shell command characters
    - Now supports colons (`:`), dollar signs (`$`), pipes (`|`), ampersands (`&`), redirection (`>`, `<`), and other common shell operators
    - Allows complex shell commands like `RUNCMD:sudo /usr/viewtouch/bin/pull.sh` to execute properly
    - Maintains security by still validating commands while being more permissive for legitimate shell operations
  - **Improved Command Validation**: Updated `ValidateCommand` function to allow essential shell command characters
    - Supports command substitution, pipes, redirection, and other shell features
    - Prevents execution of commands starting with dots (`.`) for security
    - Maintains protection against potentially dangerous command patterns
  - **Seamless Integration**: Works with existing Standard button Message section without requiring new button types
    - Commands execute using `system()` and output is logged to `/usr/viewtouch/dat/text/command.log`
    - No changes required to existing button configurations - backward compatible
    - Users can now run complex terminal commands directly from ViewTouch interface

### Changed
- **Improved Embossed Text Rendering for Better Readability**
  - Enhanced embossed text frosting effects to use proportional brightness adjustments
  - Replaced color-distorting red tinting with balanced luminance-based highlighting
  - Shadow effects now use 60% intensity while highlights use 40% brightness boost
  - Maintains original color hue and saturation for improved readability
  - Applied consistently across all embossed text rendering (main interface, loader, font checker)
- Font selection now updates all zones and all toolbar/dialog buttons for consistent UI appearance
- Font compatibility is enforced: only fonts with compatible metrics are available for selection, preventing UI breakage
- Font size and weight selection logic improved and consolidated for reliability

### Removed
- remove GTK+3 dependency, only used in loader, where we revert to use X11 directly #127

### Fixed
- Allow settlement after reset for users with Settle permission
  - Removed terminal-type restriction in `Terminal::CanSettleCheck()` that blocked settling on `ORDER_ONLY` terminals after reset
  - Settlement still requires a valid drawer and respects ownership/supervisor checks
- fix configure step by searching for `PkgConfig` before using `pkg_check_module` #128
- update embedded `catch.hpp` to `v2.13.10` to fix compilation on Ubuntu 20.04 and newer #131
- Fixed: Changing font now updates all UI elements, including toolbar and dialog buttons, not just some zones
- Fixed: UI no longer breaks or crashes when switching to scalable fonts; only compatible fonts are shown
- Fixed: Font size and weight selection no longer causes crashes or mismatches
- Fixed: Catch2 v3 migration completed successfully with all tests passing
- Fixed: "Receipts Balance & Cash Deposit" report no longer causes infinite loading cursor on clean installations
  - Added early exit in `BalanceReportWorkFn` when no checks exist to process
  - Prevents work function from being rescheduled indefinitely when database is empty
  - Report now completes immediately and shows appropriate empty state


## [v21.05.1] - 2021-05-18
### Added
- download bootstrap files if missing #119
  - `viewouch/bin/vt_data`     from http://www.viewtouch.com/vt_data
  - `viewouch/dat/tables.dat`  from http://www.viewtouch.com/tables.dat
  - `viewouch/dat/menu.dat`    from http://www.viewtouch.com/menu.dat
  - `viewouch/dat/zone_db.dat` from http://www.viewtouch.com/zone_db.dat
  - download functionality require a package providing `libcurl-dev`, for example `libcurl4-gntuls-dev` on Debian/Ubuntu
- create `viewtouch/dat/conf` directory if missing #119
- create `viewouch/dat/screensaver` directory if missing #119
- require at least gcc-8 and C++17 for `std::filesystem` support #119

### Changed
- update external copy of Catch2 to v2.13.4

### Fixed
- fix double line after header in some report tables #115
- fix Segfault in Page 13 of 16 in "Customize Job Title, Families, Phrases" #117


## [v21.01.1] - 2021-01-06
### Fixed
- fix "Users have to once again Clock In when ViewTouch exits and is restarted" #108
- fix "Timeclock" page by handling non-set start TimeInfo


## [v19.05.1] - 2019-05-26
### Changed
- update embedded date library

### Fixed
- fix "Clear Highlighted Entries" button in payment zone
- fix build on FreeBSD (error in `license_hash.cc`)
- don't install `date` library and header files
- handle missing 'dat' folder for install step


## [v19.04.1] - 2019-04-06
### Added
- add SecondsInYear function to TimeInfo

### Fixed
- SecondsElapsed is expected to return the absolute difference between TimeInfo objects
- use SecondsInYear function to fix TimeInfo file writing (fixes wrong date displayed after EndOfDay)


## [v19.03.2] - 2019-03-18
### Fixed
- add StringCompare len parameter, fixes "End of Day" behavior and many other bugs


## [19.03.1] - 2019-03-08
### Changed
- disable auto updater of `vt_main`
- print finalized check receipe also in training mode

### Fixed
- segfault because of `input_id` narrowing conversion
- unhandled exception when ConfFile doesn't exist
- don't try to render reports that failed to load
- loader logofile path can be something other than `/usr/viewtouch/graphics/logofile`
- loader CSS deprecation warnings
- regression in StringCompare, old behavior restored (fixes subcheck receipe printing)


## [v19.02.1] - 2019-02-16
### Added
- support for building with clang toolchain

### Fixed
- armhf and arm64 builds by using exact types for read and write operations
- segfault when printing a work order when no printer is configured
- crash on shutdown preventing modifications to interface to be saved


## [v19.01.1] - 2019-01-21
First release after versioning change

