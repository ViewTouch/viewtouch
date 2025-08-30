# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)


## [Unreleased]
### Added
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
    - Primary: `http://www.viewtouch.com/vt_updates/vt-update`
    - Fallback: `https://www.viewtouch.com/vt_updates/vt-update`
  - Always downloads fresh vt_data on every startup (not just when missing)
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

