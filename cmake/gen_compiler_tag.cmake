include("${CMAKE_CURRENT_LIST_DIR}/gen_target_architecture.cmake")

# Generate a build tag containing some information about architecture, system and compiler.
function(gen_compiler_tag BUILD_TAG_OUT)
	#message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")
	#message(STATUS "System name:      ${CMAKE_SYSTEM_NAME}")
	#message(STATUS "System compiler:  ${CMAKE_CXX_COMPILER_ID}")

	# identify the system architecture
	gen_target_architecture(OUTPUT SYS_ARCHITECTURE_TAG)

	# identify the system name
	string(TOLOWER ${CMAKE_SYSTEM_NAME} SYS_NAME_TAG)

	# identify compiler and possibly version
	string(COMPARE EQUAL "${CMAKE_CXX_COMPILER_ID}" "GNU" is_GNU)
	if(is_GNU)
		# using GCC
		execute_process(
			COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} -dumpversion
			OUTPUT_VARIABLE GPLUSPLUS_COMPILER_VERSION_NUMBER
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)

		if(MINGW) # MinGW
			set(SYS_COMPILER_TAG "mgw${GPLUSPLUS_COMPILER_VERSION_NUMBER}")
		else() # any other GCC flavor
			set(SYS_COMPILER_TAG "gcc${GPLUSPLUS_COMPILER_VERSION_NUMBER}")
		endif()
	# quotation marks around the variable are important, because MSVC is also a CMake variable and
	# CMake reads the value of that variable and compares it to the string for whatever reason...
	elseif(MSVC)
		# using Visual C++
		set(SYS_COMPILER_TAG "vc${MSVC_VERSION}")
	else()
		# using any other compiler
		set(SYS_COMPILER_TAG ${CMAKE_CXX_COMPILER_ID})
	endif()

	# write the build tag to output variable
	set(${BUILD_TAG_OUT} "${SYS_ARCHITECTURE_TAG}-${SYS_NAME_TAG}-${SYS_COMPILER_TAG}" PARENT_SCOPE)
endfunction()
