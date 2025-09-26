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
include(version.cmake)

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

include_directories(main)
include_directories(zone)
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
    version_generated.hh.in
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
    conf_file.cc conf_file.hh)
# provide xdb image data, only vt_term needs to link, as it accesses 'ImageData'
add_library(image_data STATIC
    image_data.cc image_data.hh)

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
    main/admission.cc  main/admission.hh
    external/sha1.cc   external/sha1.hh
    fntrace.cc         fntrace.hh
    time_info.cc       time_info.hh
    utility.cc         utility.hh
    data_persistence_manager.cc data_persistence_manager.hh
    string_utils.cc    string_utils.hh
    error_handler.cc   error_handler.hh
    remote_link.cc     remote_link.hh
    debug.cc           debug.hh
    generic_char.cc    generic_char.hh
    logger.cc          logger.hh
    socket.cc          socket.hh
    main/labels.cc     main/labels.hh
    )

target_include_directories(vtcore PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}) # include generated files like build_number.h
target_link_libraries(vtcore vt_version tz)

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

target_link_libraries(zone conf_file vtcore)

add_executable(vt_main
    data_file.cc
    data_file.hh
    main/license_hash.cc    main/license_hash.hh
    main/manager.cc         main/manager.hh
    main/printer.cc         main/printer.hh
    main/terminal.cc        main/terminal.hh
    main/settings.cc        main/settings.hh
    main/labels.cc          main/labels.hh
    main/locale.cc          main/locale.hh
    main/credit.cc          main/credit.hh
    main/sales.cc           main/sales.hh
    main/check.cc           main/check.hh
    main/account.cc         main/account.hh
    main/system.cc          main/system.hh
    main/archive.cc         main/archive.hh
    main/drawer.cc          main/drawer.hh
    main/inventory.cc       main/inventory.hh
    main/employee.cc        main/employee.hh
    main/labor.cc           main/labor.hh
    main/tips.cc            main/tips.hh
    main/exception.cc       main/exception.hh
    main/customer.cc        main/customer.hh
    main/report.cc          main/report.hh
    main/system_report.cc
    main/system_salesmix.cc
    main/chart.cc           main/chart.hh
    main/expense.cc         main/expense.hh
    main/cdu.cc             main/cdu.hh
    main/cdu_att.cc         main/cdu_att.hh
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
add_executable(vt_cdu cdu/cdu_main.cc main/cdu_att.cc)
target_link_libraries(vt_cdu vtcore)

# Test executable to verify Catch2 integration
# Note: Test files are created temporarily for verification and then removed
# To add tests, create a tests/ directory and add test files there
if(BUILD_TESTING)
    # Example test executable (uncomment and modify as needed):
    # add_executable(vt_tests tests/main_test.cc)
    # target_link_libraries(vt_tests Catch2::Catch2WithMain vtcore)
    # target_include_directories(vt_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    message(STATUS "Catch2 v3 integration ready - create tests/ directory and test files to enable testing")
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

install(PROGRAMS 	scripts/vtrestart 
			scripts/vtrun
			scripts/keeprunning 
			scripts/keeprunningcron 
			scripts/vt_ping 
			scripts/lpd-restart
			scripts/vt_openterm
			scripts/runonce
	DESTINATION viewtouch/bin
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dat" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dat")
    install(DIRECTORY dat DESTINATION viewtouch)
else()
    message(STATUS "'dat' folder is missing in source directory, excluding it from install target. You can get the bootstrap files from http://www.viewtouch.com/vt_data.html")
endif()

# Install language files (PO files)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po_file" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/po_file")
    install(DIRECTORY po_file/ DESTINATION viewtouch/dat/languages FILES_MATCHING PATTERN "*.po_*")
    message(STATUS "Installing language files from po_file/ to viewtouch/dat/languages/")
else()
    message(STATUS "'po_file' folder is missing in source directory, excluding language files from install target.")
endif()
install(PROGRAMS scripts/vtcommands.pl DESTINATION viewtouch/bin/vtcommands)

#install(CODE "MESSAGE(\"Sample install message.\")")

# Install bundled fonts
install(DIRECTORY fonts/ DESTINATION share/viewtouch/fonts FILES_MATCHING PATTERN "*.ttf" PATTERN "*.otf")

