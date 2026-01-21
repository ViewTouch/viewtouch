cmake_minimum_required(VERSION 3.10)

project(ViewTouch)
message(STATUS "CMake version ${CMAKE_VERSION}")
message(STATUS "")
message(STATUS "ViewTouch Build Configuration")
message(STATUS "==============================")
message(STATUS "If you encounter missing dependency errors, run: ./check_dependencies.sh")
message(STATUS "")

# Default to an optimized build unless the user explicitly asks otherwise.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Choose the type of build." FORCE)
    message(STATUS "Defaulting CMAKE_BUILD_TYPE to ${CMAKE_BUILD_TYPE}")
endif()

# Allow enabling LTO/IPO to shrink binaries and speed up load/link time.
option(VT_ENABLE_LTO "Enable interprocedural optimization for optimized builds" ON)
if(VT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _vt_ipo_supported OUTPUT _vt_ipo_msg)
    if(_vt_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        message(STATUS "IPO/LTO enabled for Release/RelWithDebInfo")
    else()
        message(STATUS "IPO/LTO not supported by this toolchain: ${_vt_ipo_msg}")
    endif()
endif()

# Trim unused code/data where possible to shrink binaries.
option(VT_GC_SECTIONS "Enable section-level GC for optimized builds" ON)
if(VT_GC_SECTIONS)
    set(_vt_section_flags "-ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${_vt_section_flags}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${_vt_section_flags}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--gc-sections")
    message(STATUS "Section GC enabled for Release/RelWithDebInfo")
endif()

# Additional binary size optimizations
option(VT_STRIP_BINARIES "Strip debug symbols from release binaries to reduce size" ON)
if(VT_STRIP_BINARIES)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--strip-all")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -Wl,--strip-debug")
    message(STATUS "Binary stripping enabled for Release/RelWithDebInfo")
endif()

# Enable additional optimizations (replace -O3 with -O2 for better prefetch-loop-arrays support)
set(_vt_optimization_flags "-fmerge-all-constants -fomit-frame-pointer")
# Replace -O3 with -O2 in Release flags
string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
# Append additional optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${_vt_optimization_flags}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${_vt_optimization_flags}")
message(STATUS "Optimization flags enabled for Release builds (using -O2)")



if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # don't override user variables
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "prefix" FORCE)
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
string(COMPARE EQUAL "${CMAKE_CXX_STANDARD}" "" no_cmake_cxx_standard_set)
if(no_cmake_cxx_standard_set)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    message(STATUS "Using default C++ standard ${CMAKE_CXX_STANDARD}")
else()
    message(STATUS "Using user specified C++ standard ${CMAKE_CXX_STANDARD}")
endif()

# define version number with build time, compiler tag, architecture
include(${CMAKE_CURRENT_SOURCE_DIR}/config/version.cmake)

set(VIEWTOUCH_PATH ${CMAKE_INSTALL_PREFIX}/viewtouch)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# add macro define 'DEBUG' to debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# Option to enable sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer) in debug builds
# This catches runtime bugs like use-after-free, out-of-bounds access, double free,
# integer overflow, invalid shifts, bad casts, etc.
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer in debug builds" ON)
if(ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -fno-omit-frame-pointer")
    # Also set linker flags to ensure sanitizers are linked
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
    message(STATUS "Sanitizers enabled for Debug builds (AddressSanitizer + UndefinedBehaviorSanitizer)")
    message(STATUS "  Disable with: cmake -DENABLE_SANITIZERS=OFF")
endif()

# enable various compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")  # Don't treat warnings as errors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-null-dereference")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-dangling-reference")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat=2")

# Performance optimizations
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # Enable fast math optimizations (safe for most applications)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
    # Enable vectorization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-vectorize")
    # Enable loop optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops")
    # Prefetch data
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprefetch-loop-arrays")
    message(STATUS "Performance optimizations enabled")
endif()

# disable specific compiler warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused") # also disables other unused warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-double-promotion")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shadow")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-nonliteral")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-overflow")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-const-variable")
	# Additional clang-specific flags for better compatibility
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-overflow")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-private-field")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-constant-conversion")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-shorten-64-to-32")
	# For C++20 compatibility
	if(CMAKE_CXX_STANDARD EQUAL 20)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++20-compat")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++2a-compat")
	endif()
endif()

# set tz (timezone) library flags
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "Use the operating system's timezone database" FORCE)
set(ENABLE_DATE_TESTING OFF CACHE BOOL "Enable unit tests" FORCE)
set(DISABLE_STRING_VIEW ON CACHE BOOL "Disable string view" FORCE)

# Catch2 v3 integration
option(BUILD_TESTING "Build tests" OFF)
option(CATCH_DEVELOPMENT_BUILD "Build Catch2 tests and examples" OFF)
option(CATCH_BUILD_TESTING "Build Catch2 self-tests" OFF)
option(CATCH_BUILD_EXAMPLES "Build Catch2 examples" OFF)
option(CATCH_BUILD_EXTRA_TESTS "Build Catch2 extra tests" OFF)

# Ensure external libraries use the same C++ standard
if(CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD} CACHE STRING "C++ standard for external libraries" FORCE)
endif()

# Set compiler flags for external libraries
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything" CACHE STRING "Compiler flags for external libraries" FORCE)
endif()

add_subdirectory(external/date EXCLUDE_FROM_ALL)

# don't need the curlpp shared lib, using the statically linked one
set(CURLPP_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
add_subdirectory(external/curlpp EXCLUDE_FROM_ALL)
if(NOT TARGET curlpp_static)
    message(FATAL_ERROR "missing 'curlpp_static' target")
endif()

# Add Catch2 v3
add_subdirectory(external/catch2 EXCLUDE_FROM_ALL)
if(NOT TARGET Catch2::Catch2)
    message(FATAL_ERROR "missing 'Catch2::Catch2' target")
endif()

# Modern C++ dependencies for improved code quality and maintainability
include(FetchContent)

# spdlog - Fast C++ logging library
message(STATUS "Fetching spdlog...")
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
    GIT_SHALLOW TRUE
)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json - JSON for Modern C++
message(STATUS "Fetching nlohmann/json...")
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# magic_enum - Static reflection for enums
message(STATUS "Fetching magic_enum...")
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(magic_enum)

include_directories(main)
include_directories(main/business)
include_directories(main/hardware)
include_directories(main/ui)
include_directories(main/data)
include_directories(zone)
include_directories(src/core)
include_directories(src/utils)
include_directories(src/network)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(external)
include_directories(/usr/include/freetype2)

string(TOUPPER ${CMAKE_SYSTEM_NAME} SYSTEM_UPPER)
add_definitions(-DPLATFORM="${CMAKE_SYSTEM_NAME}")
add_definitions(-DVIEWTOUCH_PATH="${VIEWTOUCH_PATH}")
add_definitions(-DKILLALL_CMD="/usr/bin/killall")
add_definitions(-DLICENSE_SERVER="${LICENSE_SERVER}")
add_definitions(-D${SYSTEM_UPPER})
add_definitions(-DNODRM)

set(TERM_CREDIT credit CACHE STRING "Credit source mode.  Can be credit, credit_cheq, credit_mcve")

if (${TERM_CREDIT} MATCHES "credit_mcve")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCREDITMCVE")
	include_directories(/usr/local/monetra/include)
	set(TERM_CREDIT_LIBS monetra)
endif(${TERM_CREDIT} MATCHES "credit_mcve")



add_library(vt_version STATIC
    config/version_generated.hh.in
    version/vt_version_info.hh
    version/vt_version_info.cc
    )
target_include_directories(vt_version PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(vt_print_version
    version/main_print_version.cc)
target_link_libraries(vt_print_version vt_version)


# read and write ini-config files
add_library(conf_file STATIC
    src/core/conf_file.cc src/core/conf_file.hh)
# provide xdb image data, only vt_term needs to link, as it accesses 'ImageData'
add_library(image_data STATIC
    src/core/image_data.cc src/core/image_data.hh)

find_package(ZLIB REQUIRED)
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR "\n"
        "ZLIB library not found. Please install:\n"
        "  Debian/Ubuntu: sudo apt-get install zlib1g-dev\n"
        "  Fedora/RHEL:   sudo dnf install zlib-devel\n"
        "  Arch Linux:    sudo pacman -S zlib\n"
        "\n"
        "Run './check_dependencies.sh' for a complete dependency check.\n")
endif()
target_link_libraries(conf_file PRIVATE ZLIB::ZLIB)

set(VT_XLIBS X11)
find_package(${VT_XLIBS} REQUIRED)
set(VT_XLIBS_INCLUDE_DIRS ${X11_INCLUDE_DIR})

find_package(Motif REQUIRED) # Library Xm must be linked before Xmu
if(NOT Motif_FOUND)
    message(FATAL_ERROR "\n"
        "Motif library not found. Please install:\n"
        "  Debian/Ubuntu: sudo apt-get install libmotif-dev\n"
        "  Fedora/RHEL:   sudo dnf install openmotif-devel\n"
        "  Arch Linux:    sudo pacman -S openmotif\n"
        "\n"
        "Run './check_dependencies.sh' for a complete dependency check.\n")
endif()
list(APPEND VT_XLIBS ${MOTIF_LIBRARIES})
list(APPEND VT_XLIBS_INCLUDE_DIRS ${MOTIF_INCLUDE_DIR})

set(x11_required_libraries Xft Xmu Xpm Xrender Xt)
foreach(x11_requirement ${x11_required_libraries})
    if(NOT DEFINED X11_${x11_requirement}_FOUND)
        list(APPEND x11_missing_requirements "${x11_requirement}")
    else()
        list(APPEND VT_XLIBS_INCLUDE_DIRS "${X11_${x11_requirement}_INCLUDE_PATH}")
    endif()
endforeach()
if(DEFINED x11_missing_requirements)
    foreach(x11_lib_missing ${x11_missing_requirements})
        message(STATUS "X11: ${x11_lib_missing} library missing")
    endforeach()
    message(FATAL_ERROR "\n"
        "Missing X library dependencies. Please install the required packages:\n"
        "  Debian/Ubuntu: sudo apt-get install libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev\n"
        "  Fedora/RHEL:   sudo dnf install libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel\n"
        "  Arch Linux:    sudo pacman -S libx11 libxft libxmu libxpm libxrender libxt\n"
        "\n"
        "Run './check_dependencies.sh' for a complete dependency check and installation instructions.\n")
endif()
list(APPEND VT_XLIBS ${x11_required_libraries})

find_package(Freetype REQUIRED)
if(NOT Freetype_FOUND AND NOT TARGET Freetype::Freetype)
    message(FATAL_ERROR "\n"
        "Freetype library not found. Please install:\n"
        "  Debian/Ubuntu: sudo apt-get install libfreetype6-dev\n"
        "  Fedora/RHEL:   sudo dnf install freetype-devel\n"
        "  Arch Linux:    sudo pacman -S freetype2\n"
        "\n"
        "Run './check_dependencies.sh' for a complete dependency check.\n")
endif()
if(TARGET Freetype::Freetype) # prefer target
	list(APPEND VT_XLIBS Freetype::Freetype)
else() # fall back to variables
	list(APPEND VT_XLIBS_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIRS}")
	list(APPEND VT_XLIBS ${FREETYPE_LIBRARIES})
endif()

find_package(PkgConfig REQUIRED) # https://cmake.org/cmake/help/latest/module/FindPkgConfig.html
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
if(NOT FONTCONFIG_FOUND)
    message(FATAL_ERROR "\n"
        "Fontconfig library not found. Please install:\n"
        "  Debian/Ubuntu: sudo apt-get install libfontconfig1-dev\n"
        "  Fedora/RHEL:   sudo dnf install fontconfig-devel\n"
        "  Arch Linux:    sudo pacman -S fontconfig\n"
        "\n"
        "Run './check_dependencies.sh' for a complete dependency check.\n")
endif()
list(APPEND VT_XLIBS ${FONTCONFIG_LIBRARIES})
list(APPEND VT_XLIBS_INCLUDE_DIRS ${FONTCONFIG_INCLUDE_DIRS})

# Image loading libraries for Image Button Zone
pkg_check_modules(PNG libpng)
pkg_check_modules(JPEG libjpeg)
pkg_check_modules(GIF libgif)

if(PNG_FOUND)
    message(STATUS "PNG library found: ${PNG_LIBRARIES}")
    list(APPEND VT_XLIBS ${PNG_LIBRARIES})
    list(APPEND VT_XLIBS_INCLUDE_DIRS ${PNG_INCLUDE_DIRS})
    add_definitions(-DHAVE_PNG)
else()
    message(WARNING "PNG library not found - PNG support disabled")
endif()

if(JPEG_FOUND)
    message(STATUS "JPEG library found: ${JPEG_LIBRARIES}")
    list(APPEND VT_XLIBS ${JPEG_LIBRARIES})
    list(APPEND VT_XLIBS_INCLUDE_DIRS ${JPEG_INCLUDE_DIRS})
    add_definitions(-DHAVE_JPEG)
else()
    message(WARNING "JPEG library not found - JPEG support disabled")
endif()

if(GIF_FOUND)
    message(STATUS "GIF library found: ${GIF_LIBRARIES}")
    list(APPEND VT_XLIBS ${GIF_LIBRARIES})
    list(APPEND VT_XLIBS_INCLUDE_DIRS ${GIF_INCLUDE_DIRS})
    add_definitions(-DHAVE_GIF)
else()
    message(WARNING "GIF library not found - GIF support disabled")
endif()

string(REPLACE ";" " " _vt_xlibs_spaced "${VT_XLIBS}")
message(STATUS "Linking X11 libraries: ${_vt_xlibs_spaced}")

list(REMOVE_DUPLICATES VT_XLIBS_INCLUDE_DIRS)

add_library(vtcore
    main/data/admission.cc  main/data/admission.hh
    external/core/sha1.cc   external/core/sha1.hh
    src/utils/fntrace.cc         src/utils/fntrace.hh
    src/core/time_info.cc       src/core/time_info.hh
    src/utils/utility.cc         src/utils/utility.hh
    src/core/data_persistence_manager.cc src/core/data_persistence_manager.hh
    src/utils/string_utils.cc    src/utils/string_utils.hh
    src/core/error_handler.cc   src/core/error_handler.hh
    src/core/crash_report.cc    src/core/crash_report.hh
    src/network/remote_link.cc     src/network/remote_link.hh
    src/core/debug.cc           src/core/debug.hh
    src/core/generic_char.cc    src/core/generic_char.hh
    src/core/logger.cc          src/core/logger.hh
    src/utils/vt_logger.cc         src/utils/vt_logger.hh
    src/utils/vt_json_config.cc    src/utils/vt_json_config.hh
    src/utils/vt_enum_utils.cc     src/utils/vt_enum_utils.hh
    src/utils/memory_utils.cc      src/utils/memory_utils.hh
    src/utils/safe_string_utils.cc src/utils/safe_string_utils.hh
    src/utils/input_validation.cc  src/utils/input_validation.hh
    src/network/socket.cc          src/network/socket.hh
    main/ui/labels.cc     main/ui/labels.hh
    )

target_include_directories(vtcore PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}  # include generated files like build_number.h
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/magic_enum-src/include)
target_link_libraries(vtcore PUBLIC vt_version tz spdlog::spdlog nlohmann_json::nlohmann_json magic_enum::magic_enum)

add_executable(vtpos 
		loader/loader_main.cc )
target_link_libraries(vtpos vtcore vt_version ${VT_XLIBS})


add_library(zone
    zone/zone.cc                 zone/zone.hh
    zone/zone_object.cc          zone/zone_object.hh
    zone/pos_zone.cc             zone/pos_zone.hh
    zone/layout_zone.cc          zone/layout_zone.hh
    zone/form_zone.cc            zone/form_zone.hh
    zone/dialog_zone.cc          zone/dialog_zone.hh
    zone/button_zone.cc          zone/button_zone.hh
    zone/order_zone.cc           zone/order_zone.hh
    zone/payment_zone.cc         zone/payment_zone.hh
    zone/login_zone.cc           zone/login_zone.hh
    zone/user_edit_zone.cc       zone/user_edit_zone.hh
    zone/check_list_zone.cc      zone/check_list_zone.hh
    zone/settings_zone.cc        zone/settings_zone.hh
    zone/report_zone.cc          zone/report_zone.hh
    zone/report_zone_quickbooks.cc
    zone/table_zone.cc           zone/table_zone.hh
    zone/split_check_zone.cc     zone/split_check_zone.hh
    zone/drawer_zone.cc          zone/drawer_zone.hh
    zone/printer_zone.cc         zone/printer_zone.hh
    zone/payout_zone.cc          zone/payout_zone.hh
    zone/inventory_zone.cc       zone/inventory_zone.hh
    zone/labor_zone.cc           zone/labor_zone.hh
    zone/phrase_zone.cc          zone/phrase_zone.hh
    zone/merchant_zone.cc        zone/merchant_zone.hh
    zone/account_zone.cc         zone/account_zone.hh
    zone/hardware_zone.cc        zone/hardware_zone.hh
    zone/search_zone.cc          zone/search_zone.hh
    zone/chart_zone.cc           zone/chart_zone.hh
    zone/video_zone.cc           zone/video_zone.hh
    zone/expense_zone.cc         zone/expense_zone.hh
    zone/cdu_zone.cc             zone/cdu_zone.hh
    zone/creditcard_list_zone.cc zone/creditcard_list_zone.hh
    )

target_include_directories(zone PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/main/data
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/magic_enum-src/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/spdlog-src/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/json-src/include>)
target_link_libraries(zone PUBLIC conf_file vtcore spdlog::spdlog nlohmann_json::nlohmann_json magic_enum::magic_enum)

add_executable(vt_main
    src/core/data_file.cc
    src/core/data_file.hh
    main/data/license_hash.cc    main/data/license_hash.hh
    main/data/manager.cc         main/data/manager.hh
    main/hardware/printer.cc         main/hardware/printer.hh
    main/hardware/terminal.cc        main/hardware/terminal.hh
    main/data/settings.cc        main/data/settings.hh
    main/ui/labels.cc          main/ui/labels.hh
    main/data/locale.cc          main/data/locale.hh
    main/data/credit.cc          main/data/credit.hh
    main/business/sales.cc           main/business/sales.hh
    main/business/check.cc           main/business/check.hh
    main/business/account.cc         main/business/account.hh
    main/data/system.cc          main/data/system.hh
    main/data/archive.cc         main/data/archive.hh
    main/hardware/drawer.cc          main/hardware/drawer.hh
    main/business/inventory.cc       main/business/inventory.hh
    main/business/employee.cc        main/business/employee.hh
    main/business/labor.cc           main/business/labor.hh
    main/business/tips.cc            main/business/tips.hh
    main/data/exception.cc       main/data/exception.hh
    main/business/customer.cc        main/business/customer.hh
    main/ui/report.cc          main/ui/report.hh
    main/ui/system_report.cc
    main/ui/system_salesmix.cc
    main/ui/chart.cc           main/ui/chart.hh
    main/data/expense.cc         main/data/expense.hh
    main/hardware/cdu.cc             main/hardware/cdu.hh
    main/hardware/cdu_att.cc         main/hardware/cdu_att.hh
    )
target_include_directories(vt_main PRIVATE ${VT_XLIBS_INCLUDE_DIRS})
target_link_libraries(vt_main zone vtcore ${VT_XLIBS})
target_link_libraries(vt_main tz curlpp_static)
# Link filesystem library if needed (GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(vt_main stdc++fs)
endif()

add_executable(vt_term
    term/term_main.cc
    term/term_view.cc
    term/term_view.hh
    term/touch_screen.cc
    term/touch_screen.hh
    term/layer.cc
    term/layer.hh
    term/term_dialog.cc
    term/term_dialog.hh
    term/term_${TERM_CREDIT}.cc)

target_include_directories(vt_term PRIVATE ${VT_XLIBS_INCLUDE_DIRS})
target_link_libraries(vt_term
    vtcore conf_file image_data
    ${VT_XLIBS} ${TERM_CREDIT_LIBS})
# Link filesystem library if needed (GCC < 9)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(vt_term stdc++fs)
endif()

add_executable(vt_print print/print_main.cc)
target_link_libraries(vt_print vtcore)
add_executable(vt_cdu cdu/cdu_main.cc main/hardware/cdu_att.cc)
target_link_libraries(vt_cdu vtcore)



# Test executable to verify Catch2 integration
if(BUILD_TESTING)
    # Enable testing with CMake
    enable_testing()

    # Add tests subdirectory
    add_subdirectory(tests)

    message(STATUS "Building ViewTouch with unit tests enabled")
else()
    message(STATUS "Building ViewTouch without tests - use -DBUILD_TESTING=ON to enable")
endif()

#add_executable(vt_authorize authorize_main.cc utility.cc)
#add_executable(vt_ccq_pipe vt_ccq_pipe.cc socket.cc utility.cc conf_file.cc)


# Ensure all necessary directories exist before installing files
# Create parent directories first
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/share)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/share/viewtouch)")
# Create subdirectories
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/bin)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/lib)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/lib/static)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/dat)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/dat/languages)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/dat/crashreports)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/bin/vtcommands)")
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/share/viewtouch/fonts)")

install(TARGETS vtpos vt_cdu vt_print vt_term vt_main
	RUNTIME DESTINATION viewtouch/bin
        LIBRARY DESTINATION viewtouch/lib
	ARCHIVE DESTINATION viewtouch/lib/static)

# Install test executable if built
if(BUILD_TESTING)
    # Uncomment when test executable is created:
    # install(TARGETS vt_tests
    #     RUNTIME DESTINATION viewtouch/bin
    # )
endif()

install(PROGRAMS 	scripts/system/vtrestart 
			scripts/system/vtrun
			scripts/system/keeprunning 
			scripts/system/keeprunningcron 
			scripts/maintenance/vt_ping 
			scripts/system/lpd-restart
			scripts/tools/vt_openterm
			scripts/system/runonce
	DESTINATION viewtouch/bin
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dat" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dat")
    install(DIRECTORY dat DESTINATION viewtouch)
else()
    message(STATUS "'dat' folder is missing in source directory, excluding it from install target. You can get the bootstrap files from http://www.viewtouch.com/vt_data.html")
endif()

# Install language files (PO files)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/po_file" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/po_file")
    install(DIRECTORY assets/data/po_file/ DESTINATION viewtouch/dat/languages FILES_MATCHING PATTERN "*.po_*")
    message(STATUS "Installing language files from assets/data/po_file/ to viewtouch/dat/languages/")
else()
    message(STATUS "'assets/data/po_file' folder is missing in source directory, excluding language files from install target.")
endif()
install(PROGRAMS scripts/tools/vtcommands.pl DESTINATION viewtouch/bin/vtcommands)



#install(CODE "MESSAGE(\"Sample install message.\")")

# Install bundled fonts
install(DIRECTORY fonts/ DESTINATION share/viewtouch/fonts FILES_MATCHING PATTERN "*.ttf" PATTERN "*.otf")

# Install default images for Image Button zones
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/default_images" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/default_images")
    install(DIRECTORY assets/default_images/ DESTINATION viewtouch/imgs FILES_MATCHING
        PATTERN "*.xpm"
        PATTERN "*.png"
        PATTERN "*.jpg"
        PATTERN "*.jpeg"
        PATTERN "*.gif")
    message(STATUS "Installing default images from assets/default_images/ to viewtouch/imgs/")
else()
    message(STATUS "'assets/default_images' folder is missing in source directory")
endif()

# Install texture files for runtime loading
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/xpm" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/xpm")
    install(DIRECTORY assets/images/xpm/ DESTINATION viewtouch/assets/images/xpm FILES_MATCHING
        PATTERN "*.xpm" PATTERN "*.png")
    message(STATUS "Installing texture files from assets/images/xpm/ to viewtouch/assets/images/xpm/")
else()
    message(STATUS "'assets/images/xpm' folder is missing in source directory")
endif()

# Set permissions for images directory to allow all users to add images
install(CODE "file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/viewtouch/imgs)")
install(CODE "execute_process(COMMAND chmod 1777 \${CMAKE_INSTALL_PREFIX}/viewtouch/imgs)")

