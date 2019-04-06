# load helper scripts
include(cmake/load_git_variables.cmake)
include(cmake/gen_version_string.cmake)
include(cmake/gen_compiler_tag.cmake)

# set project identifier for version variable creation
set(PROJECT_IDENTIFIER ${TOP_PROJECT_UPPER})

# The version number.
set (ViewTouch_VERSION_MAJOR 19)
set (ViewTouch_VERSION_MINOR 04)
set (ViewTouch_VERSION_PATCH  1)

# generate short version string <MAJOR>.<MINOR>.<PATCH>
gen_version_string(
    MAJOR ${ViewTouch_VERSION_MAJOR}
    MINOR ${ViewTouch_VERSION_MINOR}
    PATCH ${ViewTouch_VERSION_PATCH}
    VERSION_OUT ViewTouch_VERSION_STRING)

# load git variables
load_git_variables(
    BRANCH        GIT_BRANCH
    IS_TAG        GIT_IS_TAG
    LAST_TAG      GIT_LAST_TAG
    TAG           GIT_TAG
    COMMIT_COUNT  GIT_COUNT
    COMMIT_COUNT_TOTAL GIT_COUNT_TOTAL
    COMMIT_HASH   GIT_COMMIT
    REPO_IS_CLEAN GIT_CLEAN
    TIMESTAMP     ViewTouch_TIMESTAMP)


# build postfix that differentiates special releases from regular builds
# determine postfix for version string
if (GIT_CLEAN)
    # all checked in files are clean
    if (GIT_IS_TAG)
        # we are on a tag, create version postfix from tag
        string(COMPARE EQUAL "${GIT_TAG}" "" is_empty)
        if (is_empty) # GIT_TAG is required
            message(FATAL_ERROR "We are on a tag, but no tag-name is given")
        endif()
        set(_project_version "${ViewTouch_VERSION_STRING}")
        string(COMPARE EQUAL "${_project_version}" "" is_empty)
        if (is_empty) # short version string is required
            message(FATAL_ERROR "PROJECT version string not set")
        endif()
        # remove leading 'v' or 'V' in git tag
        string(REGEX REPLACE "^[vV]" "" _tag_tmp "${GIT_TAG}")
        # remove current version from git tag
        string(REGEX REPLACE "^${_project_version}" "" _tag_tmp "${_tag_tmp}")
        # remove leading - sign from postfix
        string(REGEX REPLACE "^[-]" "" _tag_tmp "${_tag_tmp}")
        # final version postfix
        set(_version_postfix "${_tag_tmp}")

        string(COMPARE EQUAL "${_version_postfix}" "" _is_empty)
        if(_is_empty)
            # release
            # for release tags the revision is empty
            set(_version_revision "")
        else()
            # release candidate
            # for release candidate tags the revision is only the git commit hash
            set(_version_revision "${GIT_COMMIT}")
        endif()
    else()
        # we are on a branch, assume development version
        set(_version_postfix "dev")
        set(_version_revision "${GIT_COMMIT} (${GIT_BRANCH})")
    endif()
else()
    # the repository has modified files, assume development version
    set(_version_postfix "dev")
    set(_version_revision "${GIT_COMMIT} (${GIT_BRANCH}, dirty)")
endif()
# copy local variable to output variable
set(ViewTouch_VERSION_POSTFIX "${_version_postfix}")
set(ViewTouch_REVISION "${_version_revision}")

# create a build tag identifying system settings
gen_compiler_tag(ViewTouch_COMPILER_TAG)

# define version strings
gen_version_string(
    VERSION_FULL_OUT ViewTouch_VERSION_EXTENDED_STRING
    MAJOR ${ViewTouch_VERSION_MAJOR}
    MINOR ${ViewTouch_VERSION_MINOR}
    PATCH ${ViewTouch_VERSION_PATCH}
    APPEND_MINUS ${GIT_COUNT})
gen_version_string(
    VERSION_FULL_OUT ViewTouch_VERSION_FULL_STRING
    MAJOR ${ViewTouch_VERSION_MAJOR}
    MINOR ${ViewTouch_VERSION_MINOR}
    PATCH ${ViewTouch_VERSION_PATCH}
    APPEND_MINUS ${GIT_COUNT}
    APPEND_PLUS  ${ViewTouch_VERSION_POSTFIX} ${GIT_COMMIT})

message(STATUS "Configure file version_generated.hpp.in")
configure_file(
    "version_generated.hh.in"
    "version_generated.hh"
    @ONLY
    )

message(STATUS "Building ${PROJECT_NAME} ${ViewTouch_VERSION_FULL_STRING}
    - branch:                   ${GIT_BRANCH}
    - compiler tag:             ${ViewTouch_COMPILER_TAG}
    - working on a git tag:     ${GIT_IS_TAG}
    - latest git tag:           ${GIT_LAST_TAG}
    - commits since latest tag: ${GIT_COUNT}
    - total commits:            ${GIT_COUNT_TOTAL}
    - current commit hash:      ${GIT_COMMIT}
    - repository is clean:      ${GIT_CLEAN}
    - git timestamp:            ${ViewTouch_TIMESTAMP}
    ")
