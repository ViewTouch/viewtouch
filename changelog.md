# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)


## [Unreleased]
### Added
- **Scalable Font Support**: Implemented Xft (X FreeType) font rendering system for resolution-independent text display
  - Added XftFont loading with automatic fallback to default fonts when requested fonts are unavailable  
  - Integrated XftTextExtentsUtf8 for accurate scalable font text measurement in UI layout calculations
  - Added XftDrawStringUtf8 rendering with preserved 3D embossed text effects (shadow, highlight, main text)
  - Enhanced error reporting with specific font loading failure messages and fallback notifications
- **Modern C++ Type Safety**: Implemented proper type casting practices throughout font rendering subsystem
  - Added reinterpret_cast for safe conversion between char* and unsigned char* pointer types
  - Added const_cast for legacy X11/Motif library compatibility while maintaining const correctness

### Changed
- cmake: `gen_compiler_tag`: handle Clang compiler to contain compiler version #163
- **Font System Migration**: Migrated from legacy bitmapped fonts to scalable Xft outline fonts for improved rendering quality and resolution independence
  - Core text rendering in `term/layer.cc` now uses XftTextExtentsUtf8 and XftDrawStringUtf8 for scalable font measurement and rendering
  - Terminal initialization in `term/term_view.cc` loads scalable fonts via XftFontOpenName with fallback support
  - Loader interface in `loader/loader_main.cc` updated to use Xft font rendering for startup messages
  - UI layout calculations in `main/manager.cc` use Xft text measurement for accurate scalable font sizing
- Button Properties Dialog: Updated font choices to provide better progression for scalable fonts, moved away from legacy bitmapped font limitations to more appropriate scalable font sizes
- Dialog fonts: Applied temporary fix changing oversized dialog text from Times 34 to Times 24 Bold in MessageDialog, DialogZone, CreditCardEntryDialog, OpenTabDialog, SimpleDialog, and CreditCardDialog classes
- Global Button Font: Changed default from Times 24 to Times 24 Bold in ZoneDB constructor
- **Cast Modernization**: Updated C-style casts to modern C++ style casts throughout font rendering code
  - Changed `(unsigned const char*)` casts to `reinterpret_cast<const unsigned char*>()` for Xft function compatibility
  - Replaced legacy `(String)` casts with safer `const_cast<char*>()` for X11/Motif string arguments
- **Error Message Enhancement**: Improved font loading diagnostics with detailed fallback notifications and specific error reporting for missing scalable fonts

### Removed
- **Documentation Files**: Removed `SCALABLE_FONTS_MIGRATION.md` per PR review feedback - migration documentation better suited for PR descriptions rather than repository files
- remove GTK+3 dependency, only used in loader, where we revert to use X11 directly #127

### Fixed
- **Font System Build Errors**: Resolved compilation failures preventing scalable font system from building
  - Fixed `invalid 'static_cast' from type 'const char*' to type 'const unsigned char*'` errors in text rendering functions
  - Corrected type casting issues in `term/layer.cc`, `loader/loader_main.cc`, and `main/manager.cc` for Xft font compatibility
  - Fixed parameter type syntax error in `main/license_hash.cc` (`unsigned const char*` → `const unsigned char*`)
- **Compiler Warnings**: Eliminated 50+ warnings that were cluttering build output
  - Fixed ISO C++ string constant warnings: "forbids converting a string constant to 'String'" in X11/Motif code
  - Resolved format truncation warnings by increasing buffer sizes in `term/term_view.cc` and other files
  - Applied `const_cast<char*>()` fixes for X11/Motif compatibility in widget creation and dialog titles
- **Build System**: Ensured complete compilation of all executables (vt_main, vt_term, vt_print, vt_cdu) and test suite
- **Code Documentation**: Added comprehensive inline comments explaining all font system changes, cast fixes, and compatibility requirements for future maintenance
- fix configure step by searching for `PkgConfig` before using `pkg_check_module` #128
- update embedded `catch.hpp` to `v2.13.10` to fix compilation on Ubuntu 20.04 and newer #131
- fix `TimeInfo.Set(date_string)` function fixing `RunReport` `to`/`from` fields and C++20 compatibility #145
- update embedded `date` to `v3.0.4` and fix CMake 4+ compatibility #155
- update embedded `curlpp` to latest `master` for newer `libcurl` compatibility #157
- update download URLs from `http` to `https`, otherwise redirect html is downloaded
  - `viewouch/bin/vt_data`     from https://www.viewtouch.com/vt_data #151
  - `viewouch/dat/tables.dat`  from https://www.viewtouch.com/tables.dat #159
  - `viewouch/dat/menu.dat`    from https://www.viewtouch.com/menu.dat #159
  - `viewouch/dat/zone_db.dat` from https://www.viewtouch.com/zone_db.dat #159
- fix CMake warning about deprecated `exec_program()` and replace with `execute_process()` #154
- fix CMake < 3.10 deprecation warning, require CMake 3.25.1 at least #156


## [v21.05.1] - 2021-05-18
### Added
- download bootstrap files if missing #119
  - `viewouch/bin/vt_data`     from http://www.viewtouch.com/vt_data
  - `viewouch/dat/tables.dat`  from http://www.viewtouch.com/tables.dat
  - `viewouch/dat/menu.dat`    from http://www.viewtouch.com/menu.dat
  - `viewouch/dat/zone_db.dat` from http://www.viewtouch.com/zone_db.dat
  - download functionality require a package providing `libcurl-dev`, for example `libcurl4-gntuls-dev` on Debian/Ubuntu
- create `viewtouch/dat/conf` directory if missing #119
- create `viewtouch/dat/screensaver` directory if missing #119
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

