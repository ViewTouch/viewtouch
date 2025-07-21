# Sample toolchain file for building with gcc compiler
#
# Typical usage:
#    *) cmake -H. -B_build -DCMAKE_TOOLCHAIN_FILE="${PWD}/toolchains/gcc.cmake"

# set compiler
set(CMAKE_C_COMPILER gcc-8)
set(CMAKE_CXX_COMPILER g++-8)
