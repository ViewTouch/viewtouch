# conf_file tests

# Check if conf_file target exists
if(NOT TARGET conf_file)
    message(FATAL_ERROR "conf_file target not found")
endif()

# Create test executable
add_executable(test_conf_file test_conf_file.cc)
target_link_libraries(test_conf_file PRIVATE conf_file Catch2::Catch2WithMain)

# Register test
add_test(NAME conf_file_test COMMAND test_conf_file) 