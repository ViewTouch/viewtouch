# Tests CMakeLists.txt - ViewTouch Test Suite

# Add test directories
add_subdirectory(conf_file)
add_subdirectory(time_info)
add_subdirectory(utility)

# Create a test target that runs all tests
add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_conf_file test_time_info test_utility
    COMMENT "Running all ViewTouch tests"
) 