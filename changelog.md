# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)


## [Unreleased]
### Added
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
- Font selection now updates all zones and all toolbar/dialog buttons for consistent UI appearance
- Font compatibility is enforced: only fonts with compatible metrics are available for selection, preventing UI breakage
- Font size and weight selection logic improved and consolidated for reliability

### Removed
- remove GTK+3 dependency, only used in loader, where we revert to use X11 directly #127

### Fixed
- fix configure step by searching for `PkgConfig` before using `pkg_check_module` #128
- update embedded `catch.hpp` to `v2.13.10` to fix compilation on Ubuntu 20.04 and newer #131
- Fixed: Changing font now updates all UI elements, including toolbar and dialog buttons, not just some zones
- Fixed: UI no longer breaks or crashes when switching to scalable fonts; only compatible fonts are shown
- Fixed: Font size and weight selection no longer causes crashes or mismatches
- Fixed: Catch2 v3 migration completed successfully with all tests passing


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

