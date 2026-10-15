set(VTM_TEST_SOURCES test_money.cpp test_check.cpp)
if(TARGET vtm_layout)
    list(APPEND VTM_TEST_SOURCES
        main.cpp   # QCoreApplication for Qt plugins
        test_layout.cpp test_navigator.cpp test_seed.cpp test_controller.cpp
        test_editor.cpp test_store.cpp test_edit_mode.cpp test_editor_ui.cpp
        test_pos_service.cpp test_pos_store.cpp test_pos_flow.cpp
        test_print.cpp test_pos_m4.cpp test_pages_ui.cpp test_net.cpp test_pos_m6.cpp test_pos_m7.cpp test_server_bank.cpp test_pairing.cpp test_phone.cpp test_checks.cpp test_menu.cpp test_reports.cpp test_staff.cpp test_safety.cpp test_customers.cpp test_waitlist.cpp test_kitchen.cpp test_inventory.cpp test_loyalty.cpp test_standby.cpp test_i18n.cpp test_kiosk.cpp test_search.cpp test_phone_orders.cpp test_cards.cpp test_refunds.cpp test_robust.cpp test_receipts.cpp test_menu_builder.cpp test_menu_screens.cpp test_stress.cpp test_labor_law.cpp test_books.cpp test_tax_areas.cpp
    test_images.cpp
    )
endif()

add_executable(vtm_tests ${VTM_TEST_SOURCES})
target_link_libraries(vtm_tests PRIVATE vtm_core)
if(NOT TARGET vtm_layout)
    target_link_libraries(vtm_tests PRIVATE Catch2::Catch2WithMain)
else()
    target_link_libraries(vtm_tests PRIVATE Catch2::Catch2)
    find_package(Qt6 REQUIRED COMPONENTS Qml Test)
    target_link_libraries(vtm_tests PRIVATE vtm_ui vtm_uiplugin Qt6::Test)
    target_compile_definitions(vtm_tests PRIVATE VTM_SEED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../seed"
                                                 VTM_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/pages")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../external/catch2/extras/Catch.cmake)
catch_discover_tests(vtm_tests)

# Every phrase on the screens has its Spanish, with the same %1 placeholders.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME i18n_es COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/../tools/i18n.py check es)
endif()
