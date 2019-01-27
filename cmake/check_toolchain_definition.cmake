# Copyright (c) 2017 NeroBurner
# All rights reserved.

include(CMakeParseArguments) # cmake_parse_arguments

function(check_toolchain_definition)
  set(function_name "check_toolchain_definition")
  set(function_synopsis "${function_name}(INPUT input_string NAME definition [DEFINED out_is_defined] [VALUE out_value])")

  # parse arguments
  set(one_value_args INPUT NAME DEFINED VALUE)
  cmake_parse_arguments(x "" "${one_value_args}" "" ${ARGV})

  # No free arguments allowed
  list(LENGTH x_UNPARSED_ARGUMENTS x_len)
  if(NOT x_len EQUAL 0)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " expected no free arguments '${x_UNPARSED_ARGUMENTS}'."
      " Synopsis: ${function_synopsis}"
    )
  endif()
  # option INPUT is mandatory
  string(COMPARE EQUAL "${x_INPUT}" "" is_empty)
  if(is_empty)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " option 'INPUT' with one argument is mandatory."
      " Synopsis: ${function_synopsis}"
    )
  endif()
  # option NAME is mandatory
  string(COMPARE EQUAL "${x_NAME}" "" is_empty)
  if(is_empty)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " option 'NAME' with one argument is mandatory."
      " Synopsis: ${function_synopsis}"
    )
  endif()
  string(FIND "${x_NAME}" " " x_NAME_whitespace_position)
  if(NOT x_NAME_whitespace_position EQUAL -1)
    message(FATAL_ERROR
      "'${function_name}' incorrect usage,"
      " definition '${x_NAME}' with whitespaces not allowed."
    )
  endif()
  set(definition "${x_NAME}")

  # clean ouptut variables
  set(is_defined NO)
  set(defined_value)

  foreach(line ${x_INPUT})
    # find the specified define
    string(REGEX MATCH "^#define ${definition} .*$" match "${line}")
    if(match)
      # check for double definition
      if(is_defined)
        message(FATAL_ERROR
          "'${function_name}' incorrect input string,"
          " double definition of define '${definition}'."
        )
      endif()
      # definition found
      set(is_defined YES)
      # extract the definition
      string(REGEX REPLACE "^#define ${definition} " "" defined_value "${match}")
    endif()
  endforeach()

  # set output_var to found definition
  string(COMPARE NOTEQUAL "${x_DEFINED}" "" is_set)
  if(is_set)
    set(${x_DEFINED} ${is_defined} PARENT_SCOPE)
  endif()
  string(COMPARE NOTEQUAL "${x_VALUE}" "" is_set)
  if(is_set)
    set(${x_VALUE} ${defined_value} PARENT_SCOPE)
  endif()
endfunction()
