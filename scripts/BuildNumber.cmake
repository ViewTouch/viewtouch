EXECUTE_PROCESS(COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/getbuildnum OUTPUT_VARIABLE BUILD_NUMBER)
MESSAGE(STATUS "BUILD_NUMBER=${BUILD_NUMBER}")
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/build_number.h.in ${CMAKE_CURRENT_BINARY_DIR}/build_number.h @ONLY)
