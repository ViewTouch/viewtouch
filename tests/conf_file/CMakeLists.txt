project(vt_ut_conf_file)


# test if all needed targets exist
if(NOT TARGET conf_file)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'conf_file' not found")
endif()
if(NOT TARGET gtest)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest' not found")
endif()
if(NOT TARGET gtest_main)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest_main' not found")
endif()

# create test
add_executable(test_conf_file test_conf_file.cc)
add_test(test_conf_file test_conf_file)
target_link_libraries(test_conf_file PRIVATE
    conf_file
    gtest gtest_main)
