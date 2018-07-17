# read git variables into given arguments
include(CMakeParseArguments) # cmake_parse_arguments

# Generate VERSION and VERSION_FULL strings
# All input-variables except MAJOR are optional
function(gen_version_string)
    set(function_name "gen_version_string_v2")
    set(function_synopsis "${function_name}(
        [VERSION_OUT      output variable for short version]
        [VERSION_FULL_OUT output variable for long version]
        MAJOR major_version
        [MINOR minor_version]
        [PATCH patch_version]
        [APPEND_MINUS list_of_arguments_each_appended_with_minus]
        [APPEND_PLUS  list_of_arguments_each_appended_with_plus]")

    # parse arguments
    set(one_value_args
        VERSION_OUT
        VERSION_FULL_OUT
        MAJOR
        MINOR
        PATCH)
    set(multi_value_args
        APPEND_MINUS
        APPEND_PLUS)
    cmake_parse_arguments(x "" "${one_value_args}" "${multi_value_args}" ${ARGV})

    # No free arguments allowed
    list(LENGTH x_UNPARSED_ARGUMENTS x_len)
    if(NOT x_len EQUAL "0")
        message(FATAL_ERROR
            "'${function_name}' incorrect usage,"
            " expected no free arguments '${x_UNPARSED_ARGUMENTS}'."
            " Synopsis: ${function_synopsis}"
            )
    endif()

    # set x_${arg_name}_is_defined for each variable
    foreach(arg_name ${one_value_args} ${multi_value_args})
        string(COMPARE NOTEQUAL "${x_${arg_name}}" "" x_${arg_name}_is_defined)
    endforeach()

    # MAJOR version is mandatory
    if (NOT x_MAJOR_is_defined)
        message(FATAL_ERROR
            "'${function_name}' incorrect usage,"
            " argument 'MAJOR' is mandatory."
            " Synopsis: ${function_synopsis}")
    endif()

    # <MAJOR>.<MINOR>.<PATCH>
    set(VERSION ${x_MAJOR})
    if(x_MINOR_is_defined)
        set(VERSION "${VERSION}.${x_MINOR}")
    endif()
    if(x_PATCH_is_defined)
        set(VERSION "${VERSION}.${x_PATCH}")
    endif()

    # create VERSION_FULL based on VERSION
    set(VERSION_FULL ${VERSION})

    # <VERSION_FULL>[-<arg1>[-<arg2> ...]]
    foreach(arg ${x_APPEND_MINUS})
        set(VERSION_FULL "${VERSION_FULL}-${arg}")
    endforeach()

    # <VERSION_FULL>[+<arg1>[+<arg2> ...]]
    foreach(arg ${x_APPEND_PLUS})
        set(VERSION_FULL "${VERSION_FULL}+${arg}")
    endforeach()

    # write to output variables
    if(x_VERSION_OUT_is_defined)
        set(${x_VERSION_OUT} ${VERSION} PARENT_SCOPE)
    endif()
    if(x_VERSION_FULL_OUT_is_defined)
        set(${x_VERSION_FULL_OUT} ${VERSION_FULL} PARENT_SCOPE)
    endif()
endfunction()
