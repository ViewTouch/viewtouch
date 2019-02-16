# Sample toolchain file for building with clang-tidy warnings
#
# Typical usage:
#    *) cmake -H. -B_build -DCMAKE_TOOLCHAIN_FILE="${PWD}/toolchains/clang.cmake"

# set compiler
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# set clang-tidy variable for warnings
set(CMAKE_CXX_CLANG_TIDY clang-tidy)
