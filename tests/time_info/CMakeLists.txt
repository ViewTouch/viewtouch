# time_info tests

# Create test executable
add_executable(test_time_info test_time_info.cc)
target_link_libraries(test_time_info PRIVATE Catch2::Catch2WithMain)

# Register test
add_test(NAME time_info_test COMMAND test_time_info) 