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
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    message(STATUS "Using default C++ standard ${CMAKE_CXX_STANDARD}")
else()
    message(STATUS "Using user specified C++ standard ${CMAKE_CXX_STANDARD}")
endif()

set(VIEWTOUCH_PATH ${CMAKE_INSTALL_PREFIX}/viewtouch)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(SYSTEM ${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

#set(CMAKE_VERBOSE_MAKEFILE ON)

# add macro define 'DEBUG' to debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# enable various compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
# disable specific compiler warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused") # also disables other unused warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-truncation")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-overflow")

# set tz (timezone) library flags
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "Use the operating system's timezone database" FORCE)
set(ENABLE_DATE_TESTING OFF CACHE BOOL "Enable unit tests" FORCE)
set(DISABLE_STRING_VIEW ON CACHE BOOL "Disable string view" FORCE)
add_subdirectory(external/date)

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
set(x11_required_libraries Xft Xmu Xpm Xrender Xt)
foreach(x11_requirement ${x11_required_libraries})
    if(NOT DEFINED X11_${x11_requirement}_FOUND)
        list(APPEND x11_missing_requirements "${x11_requirement}")
    endif()
endforeach()
if(DEFINED x11_missing_requirements)
    foreach(x11_lib_missing ${x11_missing_requirements})
        message(STATUS "X11: ${x11_lib_missing} library missing")
    endforeach()
    message(FATAL_ERROR "Missing X library dependencies")
endif()
list(APPEND VT_XLIBS ${x11_required_libraries})

find_package(Motif REQUIRED)
list(APPEND VT_XLIBS ${MOTIF_LIBRARIES})

find_package(Freetype REQUIRED)
list(APPEND VT_XLIBS Freetype::Freetype)

pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
list(APPEND VT_XLIBS ${FONTCONFIG_LIBRARIES})

foreach(vt_xlib ${VT_XLIBS})
    message(STATUS "Linking ${vt_xlib}")
endforeach()

add_library(vtcore
    main/admission.cc  main/admission.hh
    external/sha1.cc   external/sha1.hh
    fntrace.cc         fntrace.hh
    time_info.cc       time_info.hh
    utility.cc         utility.hh
    remote_link.cc     remote_link.hh
    debug.cc           debug.hh
    generic_char.cc    generic_char.hh
    logger.cc          logger.hh
    socket.cc          socket.hh
    main/labels.cc     main/labels.hh
    )

target_include_directories(vtcore PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}) # include generated files like build_number.h
target_link_libraries(vtcore tz)

add_executable(vtpos 
		loader/loader_main.cc )
target_link_libraries(vtpos vtcore ${GTK3_LIBRARIES})


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
target_link_libraries(vt_main zone vtcore ${VT_XLIBS})
target_link_libraries(vt_main tz)

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

target_link_libraries(vt_term
    vtcore conf_file image_data
    ${VT_XLIBS} ${TERM_CREDIT_LIBS})

add_executable(vt_print print/print_main.cc)
target_link_libraries(vt_print vtcore)
add_executable(vt_cdu cdu/cdu_main.cc main/cdu_att.cc)
target_link_libraries(vt_cdu vtcore)
#add_executable(vt_authorize authorize_main.cc utility.cc)
#add_executable(vt_ccq_pipe vt_ccq_pipe.cc socket.cc utility.cc conf_file.cc)

# create build_number.h file
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/last-release-rev LAST_RELEASE_REV)
execute_process(COMMAND "date" "+%Y%m%d%H%M" OUTPUT_VARIABLE TODAY OUTPUT_STRIP_TRAILING_WHITESPACE)
set(BUILD_NUMBER "\"${LAST_RELEASE_REV}-${TODAY}\"")
MESSAGE(STATUS "BUILD_NUMBER=${BUILD_NUMBER}")
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build_number.h.in ${CMAKE_CURRENT_BINARY_DIR}/build_number.h @ONLY)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
    enable_testing()
    add_subdirectory(tests/conf_file)
    add_subdirectory(tests/image_data)
    add_subdirectory(tests/time_info)
endif()

install(TARGETS vtpos vt_cdu vt_print vt_term vt_main
	RUNTIME DESTINATION viewtouch/bin
        LIBRARY DESTINATION viewtouch/lib
	ARCHIVE DESTINATION viewtouch/lib/static)

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
install(DIRECTORY dat DESTINATION viewtouch)
install(DIRECTORY graphics DESTINATION viewtouch)
install(DIRECTORY css DESTINATION viewtouch)
install(PROGRAMS scripts/vtcommands.pl DESTINATION viewtouch/bin/vtcommands)

#install(CODE "MESSAGE(\"Sample install message.\")")

