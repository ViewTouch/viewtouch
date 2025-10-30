cmake_minimum_required(VERSION 3.5.1)

project(ViewTouch)
message(STATUS "CMake version ${CMAKE_VERSION}")



if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # don't override user variables
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "prefix" FORCE)
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
string(COMPARE EQUAL "${CMAKE_CXX_STANDARD}" "" no_cmake_cxx_standard_set)
if(no_cmake_cxx_standard_set)
    set(CMAKE_CXX_STANDARD 20)
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

# enable various compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnull-dereference")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat=2")

# disable specific compiler warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused") # also disables other unused warnings
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-overflow")
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
target_link_libraries(conf_file PRIVATE ZLIB::ZLIB)

set(VT_XLIBS X11)
find_package(${VT_XLIBS} REQUIRED)
set(VT_XLIBS_INCLUDE_DIRS ${X11_INCLUDE_DIR})

find_package(Motif REQUIRED) # Library Xm must be linked before Xmu
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
    message(FATAL_ERROR "Missing X library dependencies")
endif()
list(APPEND VT_XLIBS ${x11_required_libraries})

find_package(Freetype REQUIRED)
if(TARGET Freetype::Freetype) # prefer target
	list(APPEND VT_XLIBS Freetype::Freetype)
else() # fall back to variables
	list(APPEND VT_XLIBS_INCLUDE_DIRS "${FREETYPE_INCLUDE_DIRS}")
	list(APPEND VT_XLIBS ${FREETYPE_LIBRARIES})
endif()

find_package(PkgConfig REQUIRED) # https://cmake.org/cmake/help/latest/module/FindPkgConfig.html
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
list(APPEND VT_XLIBS ${FONTCONFIG_LIBRARIES})
list(APPEND VT_XLIBS_INCLUDE_DIRS ${FONTCONFIG_INCLUDE_DIRS})

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

