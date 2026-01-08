# ViewTouch Testing Configuration

# Unit tests
add_executable(vt_tests
    main_test.cc
    unit/test_check.cc
    unit/test_settings.cc
    unit/test_utility.cc
    unit/test_memory_modernization.cc
    unit/test_safe_string_utils.cc
    unit/test_input_validation.cc
    unit/test_enhanced_logging.cc
    unit/test_coupon_calculations.cc
    unit/test_sales_tax_calculations.cc
    unit/test_time_operations.cc
    unit/test_error_handler.cc
    mocks/mock_terminal.cc
    mocks/mock_settings.cc
)

# Include directories for tests
target_include_directories(vt_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/business
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/data
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../zone
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/../external
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../_deps/magic_enum-src/include
    ${CMAKE_CURRENT_BINARY_DIR}/../_deps/spdlog-src/include
    ${CMAKE_CURRENT_BINARY_DIR}/../_deps/json-src/include
)

# Link libraries
target_link_libraries(vt_tests PRIVATE
    vtcore
    zone
    Catch2::Catch2WithMain
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    magic_enum::magic_enum
)

# Test discovery
include(Catch)
catch_discover_tests(vt_tests)

# Integration tests (future)
# add_subdirectory(integration)
