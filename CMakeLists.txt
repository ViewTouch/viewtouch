cmake_minimum_required(VERSION 3.5.1)

project(ViewTouch)
message(STATUS "CMake version ${CMAKE_VERSION}")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # don't override user variables
    set(CMAKE_INSTALL_PREFIX "/usr")
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
set(LICENSE_SERVER "default_license_server" CACHE STRING "The license server to be compiled into viewtouch")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(SYSTEM ${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

#set(CMAKE_CXX_FLAGS "-Wall")
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG")
set(CMAKE_CXX_FLAGS "-Wall -Wno-unused -Wno-unused-result")
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


add_library(conf_file conf_file.cc conf_file.hh)
find_package(ZLIB REQUIRED)
target_link_libraries(conf_file PRIVATE ZLIB::ZLIB)
set(VT_XLIBS X11 Xm Xmu Xt Xpm Xft freetype fontconfig Xrender)

add_library(vtcore main/admission.cc  external/sha1.cc utility.cc remote_link.cc  debug.cc debug.hh generic_char.cc 
		generic_char.hh logger.cc logger.hh socket.cc socket.hh
		main/labels.cc
	)


add_executable(vtpos 
		loader/loader_main.cc )
target_link_libraries(vtpos vtcore ${GTK3_LIBRARIES})


add_library(zone zone/zone.cc zone/zone_object.cc zone/pos_zone.cc 
                 zone/layout_zone.cc zone/form_zone.cc 
                 zone/dialog_zone.cc zone/button_zone.cc 
                 zone/order_zone.cc zone/payment_zone.cc 
                 zone/login_zone.cc zone/user_edit_zone.cc 
                 zone/check_list_zone.cc zone/settings_zone.cc 
                 zone/report_zone.cc zone/table_zone.cc 
                 zone/split_check_zone.cc zone/drawer_zone.cc 
                 zone/printer_zone.cc zone/payout_zone.cc 
                 zone/inventory_zone.cc zone/labor_zone.cc 
                 zone/phrase_zone.cc zone/merchant_zone.cc 
                 zone/account_zone.cc zone/hardware_zone.cc 
                 zone/search_zone.cc zone/chart_zone.cc 
                 zone/video_zone.cc zone/expense_zone.cc 
                 zone/cdu_zone.cc zone/creditcard_list_zone.cc
                 )

target_link_libraries(zone conf_file)

add_executable(vt_main data_file.cc 
                 main/manager.cc external/blowfish.c main/license_hash.cc 
	         main/license.cc main/printer.cc 
                 main/terminal.cc main/settings.cc main/labels.cc 
                 main/locale.cc main/credit.cc main/sales.cc 
                 main/check.cc main/account.cc main/system.cc 
                 main/archive.cc main/drawer.cc main/inventory.cc 
                 main/employee.cc main/labor.cc main/tips.cc 
                 main/exception.cc main/customer.cc main/report.cc 
                 main/system_report.cc main/system_salesmix.cc 
                 main/chart.cc main/expense.cc
                 main/cdu.cc main/cdu_att.cc )
target_link_libraries(vt_main zone vtcore ${VT_XLIBS})

add_executable(vt_term term/term_main.cc term/term_view.cc term/touch_screen.cc 
	             term/layer.cc term/term_dialog.cc 
			     image_data.cc
		term/term_${TERM_CREDIT}.cc)

target_link_libraries(vt_term vtcore conf_file ${VT_XLIBS} ${TERM_CREDIT_LIBS})

add_executable(vt_print print/print_main.cc)
target_link_libraries(vt_print vtcore)
add_executable(vt_cdu cdu/cdu_main.cc main/cdu_att.cc)
target_link_libraries(vt_cdu vtcore)
#add_executable(vt_authorize authorize_main.cc utility.cc)
add_executable(vt_temphash main/license_hash.cc main/temphash.cc external/sha1.cc)
#add_executable(vt_ccq_pipe vt_ccq_pipe.cc socket.cc utility.cc conf_file.cc)

ADD_CUSTOM_TARGET(GenerateBuildNumber
                  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/BuildNumber.cmake
 			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(zone GenerateBuildNumber)
add_dependencies(vtpos GenerateBuildNumber)


install(TARGETS vtpos vt_temphash vt_cdu vt_print vt_term vt_main
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

