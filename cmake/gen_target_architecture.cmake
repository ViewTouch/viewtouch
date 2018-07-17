include("${CMAKE_CURRENT_LIST_DIR}/gen_toolchain_definitions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/check_toolchain_definition.cmake")

include(CMakeParseArguments) # cmake_parse_arguments

# output can be one of the following
# - x86
# - x86_64
# - arm
# - arm64

# list of `uname -m` values
# https://stackoverflow.com/questions/45125516/possible-values-for-uname-m

# c++ architecture macros
# https://sourceforge.net/p/predef/wiki/Architectures/
function(gen_target_architecture)
  set(function_name "gen_target_architecture")
  set(function_synopsis "${function_name}(OUTPUT output)")

  # parse arguments
  set(parse_prefix x)
  set(optional_value_args "")
  set(one_value_args OUTPUT)
  set(multi_value_args "")
  cmake_parse_arguments(${parse_prefix}
    "${optional_args}"
    "${one_value_args}"
    "${multi_value_args}"
    ${ARGV})

  # No free arguments allowed
  list(LENGTH x_UNPARSED_ARGUMENTS x_len)
  if(NOT x_len EQUAL 0)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " expected no free arguments '${x_UNPARSED_ARGUMENTS}'."
      " Synopsis: ${function_synopsis}"
    )
  endif()
  # option OUTPUT is mandatory
  string(COMPARE EQUAL "${x_OUTPUT}" "" is_empty)
  if(is_empty)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " option 'OUTPUT' with one argument is mandatory."
      " Synopsis: ${function_synopsis}"
    )
  endif()
  string(FIND "${x_OUTPUT}" " " x_OUTPUT_whitespace_position)
  if(NOT x_OUTPUT_whitespace_position EQUAL -1)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " definition '${x_OUTPUT}' with whitespaces not allowed."
    )
  endif()

  # list defines for each architecture
  set(def_x86
    __i386__
    __i386
    _WIN32
  )
  set(def_x86_64
    __amd64__
    _WIN64
  )
  set(def_arm
    __arm__
  )
  set(def_arm64
    __aarch64__
  )

  # call compiler to get the set defines
  gen_toolchain_definitions(OUTPUT tc_defs
    DEFINITIONS
      ${def_x86_64}
      ${def_x86}
      ${def_arm}
      ${def_arm64}
  )

  # check defines for each architecture
  foreach(arch x86 x86_64 arm arm64)
    foreach(def in ${def_${arch}})
      check_toolchain_definition(INPUT ${tc_defs} NAME "${def}" DEFINED "${arch}_defined")
      if(${arch}_defined)
        set(architecture "${arch}")
      endif()
    endforeach()
  endforeach()

  set(${x_OUTPUT} ${architecture} PARENT_SCOPE)
endfunction()

