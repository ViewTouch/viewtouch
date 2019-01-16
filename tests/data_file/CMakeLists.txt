project(vt_ut_data_file)


# test if all needed targets exist
if(NOT TARGET vtcore)
	message(FATAL_ERROR "${PROJECT_NAME}: target 'vtcore' not found")
endif()

# create test
add_executable(test_data_file test_data_file.cc)
add_test(test_data_file test_data_file)
target_link_libraries(test_data_file PRIVATE
	vtcore tz)

target_link_libraries(test_data_file PRIVATE ZLIB::ZLIB)
target_sources(test_data_file PRIVATE
	../../data_file.hh
	../../data_file.cc)
target_include_directories(test_data_file PRIVATE ../..)

target_compile_definitions(test_data_file PRIVATE VT_TESTING)
