project(vt_ut_main)


# test if all needed targets exist
if(NOT TARGET vtcore)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'vtcore' not found")
endif()

# create test
add_executable(test_main
    test_main.cc
    test_labor.cc
    test_report.cc)
add_test(test_main test_main)
target_link_libraries(test_main PRIVATE
    vtcore tz)
target_link_libraries(test_main PRIVATE zone vtcore ${VT_XLIBS})
target_link_libraries(test_main PRIVATE tz)

target_link_libraries(test_main PRIVATE ZLIB::ZLIB)
# on gcc 8 need to link against std++fs for std::filesystem support
# https://stackoverflow.com/questions/54290254/problem-adding-stdfilesystem-to-cmake-project
target_link_libraries(test_main PRIVATE stdc++fs)

target_sources(test_main PRIVATE
    # for OutputDataFile and InputDataFile
    ../../data_file.hh
    ../../data_file.cc
    # dependencies of labor.hh
    ../../main/license_hash.cc    ../../main/license_hash.hh
    #../../main/manager.cc         ../../main/manager.hh
    # - copy of manager.cc with main() removed
    manager_stub.cc
    ../../main/printer.cc         ../../main/printer.hh
    ../../main/terminal.cc        ../../main/terminal.hh
    ../../main/settings.cc        ../../main/settings.hh
    #../../main/labels.cc          ../../main/labels.hh
    ../../main/locale.cc          ../../main/locale.hh
    ../../main/credit.cc          ../../main/credit.hh
    ../../main/sales.cc           ../../main/sales.hh
    ../../main/check.cc           ../../main/check.hh
    ../../main/account.cc         ../../main/account.hh
    ../../main/system.cc          ../../main/system.hh
    ../../main/archive.cc         ../../main/archive.hh
    ../../main/drawer.cc          ../../main/drawer.hh
    ../../main/inventory.cc       ../../main/inventory.hh
    ../../main/employee.cc        ../../main/employee.hh
    ../../main/labor.cc           ../../main/labor.hh
    ../../main/tips.cc            ../../main/tips.hh
    ../../main/exception.cc       ../../main/exception.hh
    ../../main/customer.cc        ../../main/customer.hh
    ../../main/report.cc          ../../main/report.hh
    ../../main/system_report.cc
    ../../main/system_salesmix.cc
    #../../main/chart.cc           ../../main/chart.hh
    ../../main/expense.cc         ../../main/expense.hh
    ../../main/cdu.cc             ../../main/cdu.hh
    ../../main/cdu_att.cc         ../../main/cdu_att.hh
    )
target_include_directories(test_main PRIVATE
    ../..
    ../../main)

target_compile_definitions(test_main PRIVATE VT_TESTING)
