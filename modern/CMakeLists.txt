cmake_minimum_required(VERSION 3.21)

# The version: MAJOR.MINOR here, changed by hand for a release; the third
# number is the build number (commits so far), added by every build:
# 0.9.1283, 0.9.1284... (cmake/BuildInfo.cmake).
project(ViewTouchModern VERSION 0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VTM_BUILD_TESTS "Build ViewTouch Modern unit tests" ON)

# The repository (its LICENSE); everything else the build uses is in modern/.
set(VTM_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ---------------------------------------------------------------------------
# core: pure C++ domain library (no Qt). Always buildable.
# ---------------------------------------------------------------------------
add_library(vtm_core STATIC
    core/money.hh core/tax.hh core/menu.hh core/employee.hh core/check.hh core/settings.hh
    core/day.hh core/report.hh core/customer.hh core/waitlist.hh core/inventory.hh
    core/rush.hh
    core/core.cpp core/report.cpp core/rush.cpp
)
target_include_directories(vtm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Qt6 parts. Skipped (with a warning) when Qt6 dev files are missing so the
# core library and its tests can still be built.
#   Fedora: sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel
# ---------------------------------------------------------------------------
find_package(Qt6 6.8 QUIET COMPONENTS Core Gui Quick QuickControls2 Sql Network)

if(Qt6_FOUND)
    qt_standard_project_setup(REQUIRES 6.8)
    qt_policy(SET QTP0004 NEW)

    # layout + navigation: QtCore only, unit-testable without a GUI
    add_library(vtm_layout STATIC
        layout/style.hh layout/style.cpp
        layout/zone.hh layout/zone.cpp
        layout/page.hh layout/page.cpp
        layout/layout.hh layout/layout.cpp
        layout/json_path.hh layout/json_path.cpp
        layout/schema.hh layout/schema.cpp
        layout/reflow.hh layout/reflow.cpp
        layout/menu_screens.hh layout/menu_screens.cpp
        app/navigator.hh app/navigator.cpp
    )
    target_include_directories(vtm_layout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(vtm_layout PUBLIC vtm_core Qt6::Core)
    set_target_properties(vtm_layout PROPERTIES AUTOMOC OFF)

    # page editor: draft layout + undo stack (QUndoStack lives in QtGui)
    add_library(vtm_editor STATIC app/layout_editor.hh app/layout_editor.cpp app/looks.hh app/looks.cpp)
    target_link_libraries(vtm_editor PUBLIC vtm_layout Qt6::Gui)
    set_target_properties(vtm_editor PROPERTIES AUTOMOC OFF)

    # POS session service (QtCore): login, checks, payments over the core domain
    add_library(vtm_pos STATIC
        app/pos_json.hh app/pos_json.cpp
        app/pos_session.hh app/pos_session.cpp
        app/i18n.hh app/i18n.cpp
        app/pos_service.hh app/pos_service.cpp
        app/pos_day.cpp app/pos_admin.cpp app/pos_admin_shared.hh app/pos_admin_fields.cpp app/pos_admin_records.cpp app/pos_admin_save.cpp app/pos_changes.cpp app/pos_checks.cpp app/pos_menu.cpp app/pos_customers.cpp app/pos_waitlist.cpp app/pos_inventory.cpp app/pos_schedule.cpp app/pos_demo.cpp app/pos_loyalty.cpp app/pos_kiosk.cpp app/pos_search.cpp app/pos_images.cpp app/pos_setup.cpp app/pos_punches.cpp app/pos_dashboard.cpp app/pos_kitchen.cpp app/pos_rush.cpp app/pos_commands.cpp app/pos_store_check.cpp app/tax_areas.hh app/tax_areas.cpp app/pos_tax_area.cpp app/pos_requests.cpp app/pos_phone_orders.cpp app/pos_cards.cpp app/pos_holds.cpp app/pos_forecast.cpp app/pos_refunds.cpp app/pos_receipts.cpp app/pos_menu_builder.cpp app/menu_import.hh app/menu_import.cpp app/xlsx.hh app/xlsx.cpp app/menu_templates.cpp
    )
    target_link_libraries(vtm_pos PUBLIC vtm_core Qt6::Core)
    # Phrase files for the screens' languages (see app/i18n.hh)
    qt_add_resources(vtm_pos "vtm_i18n" PREFIX "/i18n" BASE i18n FILES i18n/en.json i18n/es.json i18n/fr.json i18n/pt.json i18n/zh.json)
    # PosService is exposed to QML via QML_FOREIGN (ui/cpp/posforeign.hh),
    # which needs its type information.
    qt_extract_metatypes(vtm_pos)

    # Printing: ticket formatting (plain C++) and the background spooler
    add_library(vtm_print STATIC
        print/document.hh print/document.cpp
        print/labels.hh print/labels.cpp
        print/tickets.hh print/tickets.cpp
        print/spooler.hh print/spooler.cpp
        print/ticket_printer.hh print/ticket_printer.cpp
        print/raster.hh print/raster.cpp
        print/printer_monitor.hh print/printer_monitor.cpp
    )
    target_link_libraries(vtm_print PUBLIC vtm_pos Qt6::Network Qt6::Gui)

    # SQLite persistence: layout, POS data, and the background writer
    add_library(vtm_storage STATIC
        storage/layout_store.hh storage/layout_store.cpp
        storage/pos_store.hh storage/pos_store.cpp
        storage/async_writer.hh storage/async_writer.cpp
        storage/backup.hh storage/backup.cpp
        storage/sealed.hh storage/sealed.cpp
    )
    target_link_libraries(vtm_storage PUBLIC vtm_layout vtm_pos Qt6::Sql)

    # Several terminals on one store: server, remote session, page sync
    # This build's number, commit and version (net/build_info.cpp), fresh at every build.
    set(VTM_GENERATED ${CMAKE_BINARY_DIR}/generated)
    add_custom_target(vtm_build_info ALL
        BYPRODUCTS ${VTM_GENERATED}/vtm_build_number.hh ${VTM_GENERATED}/vtm_history.inc
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${VTM_GENERATED}/vtm_build_number.hh
                -DHISTORY=${VTM_GENERATED}/vtm_history.inc -DKEEP=60
                -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInfo.cmake
        COMMENT "Build number and history")
    add_library(vtm_net STATIC
        net/build_info.hh net/build_info.cpp
        net/protocol.hh net/protocol.cpp
        net/pairing.hh net/pairing.cpp
        net/discovery.hh net/discovery.cpp
        net/layout_hub.hh net/layout_hub.cpp
        net/pos_server.hh net/pos_server.cpp
        net/stripe_api.hh net/stripe_api.cpp
        net/remote_session.hh net/remote_session.cpp net/stress.hh net/stress.cpp
        net/standby.hh net/standby.cpp
    )
    target_link_libraries(vtm_net PUBLIC vtm_pos vtm_layout vtm_storage Qt6::Network)
    target_include_directories(vtm_net PRIVATE ${VTM_GENERATED})
    add_dependencies(vtm_net vtm_build_info)

    # UI: the ViewTouch QML module as a static library, so the app and the
    # UI tests load the very same QML, C++ types, and resources.
    qt_add_library(vtm_ui STATIC)
    set_source_files_properties(ui/qml/EditorStyle.qml ui/qml/TouchGuard.qml ui/qml/StoreColors.qml ui/qml/DialogWords.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    qt_add_qml_module(vtm_ui
        URI ViewTouch
        VERSION 1.0
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ViewTouch
        QML_FILES
            ui/qml/Main.qml
            ui/qml/JoinWindow.qml
            ui/qml/StationWindow.qml
            ui/qml/StationIcon.qml
            ui/qml/PicturePicker.qml
            ui/qml/HandoffSheet.qml
            ui/qml/StressPanel.qml
            ui/qml/PageView.qml
            ui/qml/ZoneItem.qml
            ui/qml/ZoneShape.qml
            ui/qml/EditorStyle.qml
            ui/qml/StoreColors.qml
            ui/qml/DialogWords.qml
            ui/qml/ColorPicker.qml
            ui/qml/QuickItemCard.qml
            ui/qml/MenuButtons.qml
            ui/qml/TouchGuard.qml
            ui/qml/EditLayer.qml
            ui/qml/EditorToolbar.qml
            ui/qml/PagePanel.qml
            ui/qml/Inspector.qml
            ui/qml/FieldEditor.qml
            ui/qml/FloorPlanView.qml
            ui/qml/PhoneScreens.qml
            ui/qml/ActionListEditor.qml
            ui/qml/WidgetKey.qml
            ui/qml/PosButton.qml
            ui/qml/WidgetTable.qml
            ui/qml/WidgetTableGrid.qml
            ui/qml/WidgetStaffPicker.qml
            ui/qml/WidgetCheckHistory.qml
            ui/qml/WidgetModifierPicker.qml
            ui/qml/WidgetSoldOutList.qml
            ui/qml/TouchScrollBar.qml
            ui/qml/TouchScrollColumn.qml
            ui/qml/SelfOrder.qml
            ui/qml/JobChooser.qml
            ui/qml/CashTipsPrompt.qml
            ui/qml/DrawerCount.qml
            ui/qml/ReasonSheet.qml
            ui/qml/TouchKeyboard.qml
            ui/qml/ApprovalPad.qml
            ui/qml/MessageBanner.qml
            ui/qml/WidgetMessageComposer.qml
            ui/qml/WidgetCustomerLookup.qml
            ui/qml/WidgetGiftCard.qml
            ui/qml/WidgetWaitlist.qml
            ui/qml/WidgetHostStand.qml
            ui/qml/WidgetDeliveryBoard.qml
            ui/qml/WidgetDecor.qml
            ui/qml/WidgetFloorPlan.qml
            ui/qml/WidgetSchedule.qml
            ui/qml/WidgetFactoryReset.qml
            ui/qml/WidgetNetwork.qml
            ui/qml/WidgetReceiveDelivery.qml
            ui/qml/WidgetStockCount.qml
            ui/qml/WidgetWasteLog.qml
            ui/qml/WidgetStoreCheck.qml
            ui/qml/WidgetPrepLabels.qml
            ui/qml/WidgetCheckSearch.qml
            ui/qml/WidgetOrderLater.qml
            ui/qml/WidgetMenuGrid.qml
            ui/qml/WidgetMenuCategories.qml
            ui/qml/WidgetMenuBuilder.qml
            ui/qml/MenuBuilderImport.qml
            ui/qml/MenuBuilderCheck.qml
            ui/qml/MenuBuilderStarter.qml
            ui/qml/MenuBuilderSeveral.qml
            ui/qml/MenuBuilderPrices.qml
            ui/qml/MenuBuilderLike.qml
            ui/qml/MenuBuilderPrint.qml
            ui/qml/MenuBuilderTheme.qml
            ui/qml/MenuBuilderSizes.qml
            ui/qml/MenuBuilderPicture.qml
            ui/qml/MenuTouchButton.qml
            ui/qml/WidgetTimeClock.qml
            ui/qml/WidgetDashboard.qml
            ui/qml/RushRadarView.qml
            ui/qml/WidgetChecklist.qml
            ui/qml/PagePreview.qml
            ui/qml/SetupGuide.qml
            ui/qml/LayoutGallery.qml
            ui/qml/CustomerDisplay.qml
            ui/qml/CustomerDisplayWindow.qml
            ui/qml/WidgetLoginPad.qml
            ui/qml/WidgetNumPad.qml
            ui/qml/WidgetGuestCount.qml
            ui/qml/WidgetOrderList.qml
            ui/qml/WidgetPaymentPanel.qml
            ui/qml/WidgetCheckList.qml
            ui/qml/WidgetKeyboard.qml
            ui/qml/WidgetClock.qml
            ui/qml/WidgetLogoutPanel.qml
            ui/qml/WidgetStatusBar.qml
            ui/qml/WidgetAdminPanel.qml
            ui/qml/WidgetReportView.qml
            ui/qml/WidgetDrawerPanel.qml
            ui/qml/WidgetEndOfDay.qml
            ui/qml/WidgetSplitCheck.qml
            ui/qml/WidgetKitchenDisplay.qml
            ui/qml/WidgetCustomerInfo.qml
        SOURCES
            ui/cpp/zonemodel.hh ui/cpp/zonemodel.cpp
            ui/cpp/layoutcontroller.hh ui/cpp/layoutcontroller.cpp
            ui/cpp/language.hh ui/cpp/language.cpp
            ui/cpp/editorcontroller.hh ui/cpp/editorcontroller.cpp
            ui/cpp/joincontroller.hh ui/cpp/joincontroller.cpp
            ui/cpp/stationcontroller.hh ui/cpp/stationcontroller.cpp
            ui/cpp/reportexport.hh ui/cpp/reportexport.cpp
            ui/cpp/menuprint.hh ui/cpp/menuprint.cpp
            ui/cpp/cardreader.hh ui/cpp/cardreader.cpp
            ui/cpp/posforeign.hh
    )
    target_include_directories(vtm_ui PUBLIC ui/cpp)
    target_link_libraries(vtm_ui PUBLIC vtm_layout vtm_editor vtm_storage vtm_pos vtm_print vtm_net Qt6::Gui Qt6::Quick Qt6::QuickControls2)

    qt_add_executable(vtmodern main.cpp)
    target_link_libraries(vtmodern PRIVATE vtm_ui vtm_uiplugin)

    if(ANDROID)
        # The tablet app: a terminal of a store (see main.cpp, Q_OS_ANDROID).
        # Each build must number higher than the last for Android to update:
        # the version (0.9 -> 900) times 100000 plus the build number (CI run).
        # The build number (commits so far), unless given.
        execute_process(COMMAND git -c safe.directory=* rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        OUTPUT_VARIABLE VTM_COMMITS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(NOT VTM_COMMITS MATCHES "^[0-9]+$")
            set(VTM_COMMITS 0)
        endif()
        set(VTM_ANDROID_BUILD ${VTM_COMMITS} CACHE STRING "Build number, part of the Android version code")
        math(EXPR VTM_VERSION_CODE
             "(${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100) * 100000 + ${VTM_ANDROID_BUILD}")
        set_target_properties(vtmodern PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
            QT_ANDROID_PACKAGE_NAME org.viewtouch.pos
            QT_ANDROID_APP_NAME "ViewTouch"
            QT_ANDROID_VERSION_NAME ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${VTM_ANDROID_BUILD}
            QT_ANDROID_VERSION_CODE ${VTM_VERSION_CODE}
            QT_ANDROID_MIN_SDK_VERSION 28
            QT_ANDROID_TARGET_SDK_VERSION 35)
        # Pages use DejaVu Sans, which Android doesn't have.
        qt_add_resources(vtmodern "fonts" PREFIX "/fonts" BASE ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSans-Bold.ttf)
        # Qt for Android has no OpenSSL of its own; the encrypted terminal
        # connection needs it. KDAB's builds, as the Qt docs recommend.
        include(FetchContent)
        FetchContent_Declare(android_openssl
            URL https://github.com/KDAB/android_openssl/archive/b71f1470962019bd89534a2919f5925f93bc5779.zip
            DOWNLOAD_EXTRACT_TIMESTAMP ON)
        FetchContent_MakeAvailable(android_openssl)
        include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
        add_android_openssl_libraries(vtmodern)
    endif()

    # Starter layout shipped with the app (qrc:/seed)
    file(GLOB_RECURSE VTM_SEED_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
         CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/seed/*.json)
    qt_add_resources(vtm_ui "seed" PREFIX "/" FILES ${VTM_SEED_FILES})

    # The classic ViewTouch textures under short names (qrc:/textures/<name>.xpm)
    set(VTM_TEXTURES
        sand:sand-8 litsand:litsand-6 darksand:darksand-6
        graymarble:graymarble-12 greenmarble:greenmarble-12
        wood:wood-10 darkwood:darkwood-10 litewood:litewood-8 woodfloor:woodfloor-12
        parchment:parchment-6 blueparchment:blueparchment leather:leather-8
        steel:steel-8 metal:metal-8 canvas:canvas-8 velvet:velvet-8 pearl:pearl-8
        stone:stone-8 carbonfiber:carbonfiber-128-6
    )
    set(VTM_TEXTURE_FILES)
    foreach(entry IN LISTS VTM_TEXTURES)
        string(REPLACE ":" ";" parts ${entry})
        list(GET parts 0 alias)
        list(GET parts 1 file)
        set(path ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/${file}.xpm)
        set_source_files_properties(${path} PROPERTIES QT_RESOURCE_ALIAS ${alias}.xpm)
        list(APPEND VTM_TEXTURE_FILES ${path})
    endforeach()
    qt_add_resources(vtm_ui "textures" PREFIX "/textures" FILES ${VTM_TEXTURE_FILES})

    set(VTM_BURGER ${CMAKE_CURRENT_SOURCE_DIR}/assets/images/burger.png)
    set_source_files_properties(${VTM_BURGER} PROPERTIES QT_RESOURCE_ALIAS burger.png)
    qt_add_resources(vtm_ui "images" PREFIX "/images" FILES ${VTM_BURGER})

    if(NOT ANDROID)
        include(packaging/Packaging.cmake)
    endif()
else()
    message(WARNING
        "Qt6 (Core Gui Quick QuickControls2 Sql >= 6.8) development files not found: "
        "building core library and tests only. "
        "Install qt6-qtbase-devel and qt6-qtdeclarative-devel to build vtmodern.")
endif()

# ---------------------------------------------------------------------------
# Tests (Catch2 v3, vendored in external/catch2)
# ---------------------------------------------------------------------------
if(VTM_BUILD_TESTS)
    enable_testing()
    if(NOT TARGET Catch2::Catch2WithMain)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/catch2
                         ${CMAKE_CURRENT_BINARY_DIR}/catch2 EXCLUDE_FROM_ALL)
    endif()
    add_subdirectory(tests)
endif()
