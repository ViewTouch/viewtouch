project(vt_ut_image_data)


# test if all needed targets exist
if(NOT TARGET image_data)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'image_data' not found")
endif()
if(NOT TARGET gtest)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest' not found")
endif()
if(NOT TARGET gtest_main)
    message(FATAL_ERROR "${PROJECT_NAME}: target 'gtest_main' not found")
endif()

# create test
add_executable(test_image_data test_image_data.cc)
add_test(test_image_data test_image_data)
target_link_libraries(test_image_data PRIVATE
    image_data
    gtest gtest_main)
