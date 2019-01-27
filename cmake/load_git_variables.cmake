# read git variables into given arguments
include(CMakeParseArguments) # cmake_parse_arguments

function(load_git_variables)
    set(function_name "load_git_variables")
    set(function_synopsis "${function_name}(
        BRANCH branch_name
        IS_TAG is_tag
        TAG name_of_tag_if_is_tag
        LAST_TAG latest_tag_name
        COMMIT_COUNT commits_since_last_tag
        COMMIT_COUNT_TOTAL number_of_all_commits
        COMMIT_HASH hash_of_the_current_commit
        REPO_IS_CLEAN repo_is_clean)")

    # parse arguments
    set(one_value_args
        BRANCH
        IS_TAG
        TAG
        LAST_TAG
        COMMIT_COUNT
        COMMIT_COUNT_TOTAL
        COMMIT_HASH
        REPO_IS_CLEAN
        TIMESTAMP)
    cmake_parse_arguments(x "" "${one_value_args}" "" ${ARGV})

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
    foreach(arg_name ${one_value_args})
        string(COMPARE NOTEQUAL "${x_${arg_name}}" "" x_${arg_name}_is_defined)
    endforeach()

    find_package(Git)
    if (GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} status
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_STATUS_ERROR
            OUTPUT_QUIET
            ERROR_QUIET
        )
    else()
        set(GIT_STATUS_ERROR YES)
    endif()
    if (GIT_STATUS_ERROR)
        message(STATUS "Git or git archive was not found")
        set(GIT_BRANCH "detached")
        set(GIT_IS_TAG "NO")
        set(GIT_TAG "unknown")
        set(GIT_LAST_TAG "unknown")
        set(GIT_COMMIT_COUNT "~")
        set(GIT_COMMIT_COUNT_TOTAL "~")
        set(GIT_COMMIT_HASH "dirty")
        set(GIT_REPO_IS_CLEAN "NO")
        # if the repo is dirty use the current date and time
        string(TIMESTAMP GIT_TIMESTAMP "%Y-%m-%d %H:%M:%S +0000" UTC)
    else()
        # VARIABLE: BRANCH
        # extract the name of the branch the last commit has been made on, so this also works
        # in detached head state (which is the default on Gitlab CI)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} name-rev --name-only HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        STRING(REGEX REPLACE "^remotes/origin/" "" GIT_BRANCH ${GIT_BRANCH})


        # VARIABLE: COMMIT_HASH
        # Get the latest abbreviated commit hash of the working branch
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )

        # VARIABLE: LATEST_TAG
        # Get the last tag (if any)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_LAST_TAG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            )

        # VARIABLE: COMMIT_COUNT
        # count the number of commits since last tag or since the beginning
        if ("${GIT_LAST_TAG}" STREQUAL "")
            set(git_last_tag_flag "")
        else()
            set(git_last_tag_flag "^${GIT_LAST_TAG}")
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD ${git_last_tag_flag}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_COUNT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        # count the total number of commits
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_COUNT_TOTAL
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )

        # VARIABLE: IS_TAG and TAG
        if (GIT_COMMIT_COUNT EQUAL "0")
            set(GIT_IS_TAG "YES")
            set(GIT_TAG "${GIT_LAST_TAG}")
        else()
            set(GIT_IS_TAG "NO")
            set(GIT_TAG "")
        endif()

        # VARIABLE: REPO_IS_CLEAN
        # check if repo is clean
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --no-ext-diff --quiet --exit-code
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE repo_is_clean_flag
            OUTPUT_QUIET
            ERROR_QUIET
            )
        if (repo_is_clean_flag EQUAL "0")
            set(GIT_REPO_IS_CLEAN "YES")
        else()
            set(GIT_REPO_IS_CLEAN "NO")
        endif()

        # VARIABLE: COMMIT_TIMESTAMP
        if (GIT_REPO_IS_CLEAN)
            # if the repo is clean use the latest commit time
            execute_process(
                COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=iso
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_TIMESTAMP
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )
        else()
            # if the repo is dirty use the current date and time
            string(TIMESTAMP GIT_TIMESTAMP "%Y-%m-%d %H:%M:%S +0000" UTC)
        endif()
    endif()

    # fill return values if requested
    foreach(arg_name ${one_value_args})
        if (x_${arg_name}_is_defined)
            set(${x_${arg_name}} ${GIT_${arg_name}} PARENT_SCOPE)
        endif()
    endforeach()
endfunction()
