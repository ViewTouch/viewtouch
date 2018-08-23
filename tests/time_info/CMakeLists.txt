project(vt_ut_boost_serialization)


# test if all needed targets exist
if(NOT TARGET vtcore)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'vtcore' not found")
endif()
if(NOT TARGET gtest)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest' not found")
endif()
if(NOT TARGET gtest_main)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest_main' not found")
endif()

# create test
add_executable(test_time_info test_time_info.cc)
add_test(test_time_info test_time_info)
target_link_libraries(test_time_info PRIVATE
    vtcore
    gtest gtest_main)
