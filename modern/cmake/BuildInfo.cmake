# Writes OUT: this build's number (the commits so far, so each newer build
# numbers higher), its commit, its version and whether it has changes not
# committed yet; and HISTORY: the recent builds (their number, version, date
# and what changed), for Store Settings -> About.
#
# The version is MAJOR.MINOR (project() in CMakeLists.txt, changed by hand
# for a release) and the build number: 0.9.1283, then 0.9.1284... by itself.
#
# Files are rewritten only when they change, so nothing recompiles
# needlessly. Run at every build (vtm_build_info).
# (safe.directory: in a build container the checkout belongs to someone else.)
set(git git -c safe.directory=*)
execute_process(COMMAND ${git} rev-list --count HEAD WORKING_DIRECTORY "${SRC}"
                OUTPUT_VARIABLE number OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND ${git} rev-parse --short HEAD WORKING_DIRECTORY "${SRC}"
                OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
# Changes to the program's files not committed yet (new files don't count).
execute_process(COMMAND ${git} status --porcelain --untracked-files=no -- . WORKING_DIRECTORY "${SRC}"
                OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT number MATCHES "^[0-9]+$")
    set(number 0)   # not from git (a source tarball): build 0
endif()
if(changes STREQUAL "")
    set(modified 0)
else()
    set(modified 1)
endif()
set(version "${MAJOR}.${MINOR}.${number}")

function(write_if_changed path content)
    if(EXISTS "${path}")
        file(READ "${path}" old)
    endif()
    if(NOT "${old}" STREQUAL "${content}")
        file(WRITE "${path}" "${content}")
    endif()
endfunction()

write_if_changed("${OUT}" "#pragma once
#define VTM_BUILD_NUMBER ${number}
#define VTM_BUILD_COMMIT \"${commit}\"
#define VTM_BUILD_VERSION \"${version}\"
#define VTM_BUILD_MODIFIED ${modified}
")

# The recent builds that changed the program, newest first: one a line,
# number, version, commit, date and what changed, with \x1f between them.
# A build's version: MAJOR.MINOR in its CMakeLists.txt, and its number.
string(ASCII 31 sep)
set(history "")
execute_process(COMMAND ${git} log --format=%h -n ${KEEP} -- . WORKING_DIRECTORY "${SRC}"
                OUTPUT_VARIABLE hashes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(REPLACE "\n" ";" hashes "${hashes}")
# Newest first by build number, not by date: after a merge, a side branch's
# commits are dated among the others but numbered by their own history. One
# entry a build number.
set(numbered "")
foreach(h IN LISTS hashes)
    execute_process(COMMAND ${git} rev-list --count ${h} WORKING_DIRECTORY "${SRC}"
                    OUTPUT_VARIABLE n OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    list(APPEND numbered "${n}:${h}")
endforeach()
list(SORT numbered COMPARE NATURAL ORDER DESCENDING)
set(previous "")
foreach(entry IN LISTS numbered)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 n)
    list(GET parts 1 h)
    if(n STREQUAL previous)
        continue()
    endif()
    set(previous "${n}")
    execute_process(COMMAND ${git} log -1 --format=%cI%x1f%s ${h} WORKING_DIRECTORY "${SRC}"
                    OUTPUT_VARIABLE about OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${git} show ${h}:./CMakeLists.txt WORKING_DIRECTORY "${SRC}"
                    OUTPUT_VARIABLE lists ERROR_QUIET)
    string(REGEX MATCH "project\\([A-Za-z]+ VERSION ([0-9]+)\\.([0-9]+)" found "${lists}")
    if(found)
        set(v "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${n}")
    else()
        set(v "${n}")
    endif()
    string(APPEND history "${n}${sep}${v}${sep}${h}${sep}${about}\n")
endforeach()
write_if_changed("${HISTORY}" "R\"vtmhistory(${history})vtmhistory\"\n")
