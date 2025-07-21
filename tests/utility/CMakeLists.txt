# utility tests

# Create test executable
add_executable(test_utility test_utility.cc)
target_link_libraries(test_utility PRIVATE Catch2::Catch2WithMain)

# Register test
add_test(NAME utility_test COMMAND test_utility) 