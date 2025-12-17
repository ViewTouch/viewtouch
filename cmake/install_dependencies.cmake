# install_dependencies.cmake
# This script checks for required dependencies and provides installation instructions
# Usage: cmake -P cmake/install_dependencies.cmake

cmake_minimum_required(VERSION 3.10)

message(STATUS "ViewTouch Dependency Checker")
message(STATUS "==============================")
message("")

# Detect Linux distribution
set(DISTRO_NAME "Unknown")
set(DISTRO_PKG_MGR "")
set(DISTRO_INSTALL_CMD "")

if(EXISTS "/etc/os-release")
    file(READ "/etc/os-release" os_release)
    if(os_release MATCHES "ID=([^\n]+)")
        set(DISTRO_NAME "${CMAKE_MATCH_1}")
    endif()
    if(os_release MATCHES "ID_LIKE=([^\n]+)")
        set(DISTRO_LIKE "${CMAKE_MATCH_1}")
    endif()
endif()

# Determine package manager and install command
if(DISTRO_NAME STREQUAL "ubuntu" OR DISTRO_NAME STREQUAL "debian" OR 
   DISTRO_LIKE MATCHES "debian")
    set(DISTRO_PKG_MGR "apt")
    set(DISTRO_INSTALL_CMD "sudo apt-get update && sudo apt-get install -y")
elseif(DISTRO_NAME STREQUAL "fedora" OR DISTRO_NAME STREQUAL "rhel" OR 
       DISTRO_NAME STREQUAL "centos" OR DISTRO_LIKE MATCHES "fedora")
    set(DISTRO_PKG_MGR "dnf")
    set(DISTRO_INSTALL_CMD "sudo dnf install -y")
elseif(DISTRO_NAME STREQUAL "arch" OR DISTRO_LIKE MATCHES "arch")
    set(DISTRO_PKG_MGR "pacman")
    set(DISTRO_INSTALL_CMD "sudo pacman -S --noconfirm")
elseif(DISTRO_NAME STREQUAL "opensuse" OR DISTRO_NAME STREQUAL "sles")
    set(DISTRO_PKG_MGR "zypper")
    set(DISTRO_INSTALL_CMD "sudo zypper install -y")
else()
    message(WARNING "Unknown Linux distribution. Please install dependencies manually.")
endif()

message(STATUS "Detected distribution: ${DISTRO_NAME}")
message(STATUS "Package manager: ${DISTRO_PKG_MGR}")
message("")

# Define required dependencies with their package names for different distributions
set(REQUIRED_DEPS "")
set(MISSING_DEPS "")

# Core build tools
list(APPEND REQUIRED_DEPS "cmake")
list(APPEND REQUIRED_DEPS "build-essential")  # gcc, g++, make (Debian/Ubuntu)
list(APPEND REQUIRED_DEPS "git")

# X11 libraries
list(APPEND REQUIRED_DEPS "libx11-dev")      # X11
list(APPEND REQUIRED_DEPS "libxft-dev")      # Xft
list(APPEND REQUIRED_DEPS "libxmu-dev")      # Xmu
list(APPEND REQUIRED_DEPS "libxpm-dev")      # Xpm
list(APPEND REQUIRED_DEPS "libxrender-dev") # Xrender
list(APPEND REQUIRED_DEPS "libxt-dev")       # Xt

# Motif
list(APPEND REQUIRED_DEPS "libmotif-dev")   # Motif (may be openmotif-dev on some systems)

# Font libraries
list(APPEND REQUIRED_DEPS "libfreetype6-dev")    # Freetype
list(APPEND REQUIRED_DEPS "libfontconfig1-dev")  # Fontconfig

# Compression
list(APPEND REQUIRED_DEPS "zlib1g-dev")      # ZLIB

# Image libraries (optional but recommended)
list(APPEND REQUIRED_DEPS "libpng-dev")      # PNG
list(APPEND REQUIRED_DEPS "libjpeg-dev")     # JPEG
list(APPEND REQUIRED_DEPS "libgif-dev")      # GIF

# pkg-config
list(APPEND REQUIRED_DEPS "pkg-config")

# Package name mappings for different distributions
set(DEB_PACKAGES
    "cmake"
    "build-essential"
    "git"
    "libx11-dev"
    "libxft-dev"
    "libxmu-dev"
    "libxpm-dev"
    "libxrender-dev"
    "libxt-dev"
    "libmotif-dev"
    "libfreetype6-dev"
    "libfontconfig1-dev"
    "zlib1g-dev"
    "libpng-dev"
    "libjpeg-dev"
    "libgif-dev"
    "pkg-config"
)

set(RPM_PACKAGES
    "cmake"
    "gcc"
    "gcc-c++"
    "make"
    "git"
    "libX11-devel"
    "libXft-devel"
    "libXmu-devel"
    "libXpm-devel"
    "libXrender-devel"
    "libXt-devel"
    "openmotif-devel"
    "freetype-devel"
    "fontconfig-devel"
    "zlib-devel"
    "libpng-devel"
    "libjpeg-devel"
    "giflib-devel"
    "pkgconfig"
)

set(ARCH_PACKAGES
    "cmake"
    "base-devel"
    "git"
    "libx11"
    "libxft"
    "libxmu"
    "libxpm"
    "libxrender"
    "libxt"
    "openmotif"
    "freetype2"
    "fontconfig"
    "zlib"
    "libpng"
    "libjpeg-turbo"
    "giflib"
    "pkgconf"
)

set(ZYPPER_PACKAGES
    "cmake"
    "gcc"
    "gcc-c++"
    "make"
    "git"
    "libX11-devel"
    "libXft-devel"
    "libXmu-devel"
    "libXpm-devel"
    "libXrender-devel"
    "libXt-devel"
    "openmotif-devel"
    "freetype2-devel"
    "fontconfig-devel"
    "zlib-devel"
    "libpng16-devel"
    "libjpeg-devel"
    "libgif-devel"
    "pkg-config"
)

# Function to check if a package is installed
function(check_package package_name result_var)
    set(check_result 1)
    
    if(DISTRO_PKG_MGR STREQUAL "apt")
        execute_process(
            COMMAND dpkg -l ${package_name}
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE check_result
        )
    elseif(DISTRO_PKG_MGR STREQUAL "dnf")
        execute_process(
            COMMAND rpm -q ${package_name}
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE check_result
        )
    elseif(DISTRO_PKG_MGR STREQUAL "pacman")
        execute_process(
            COMMAND pacman -Qi ${package_name}
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE check_result
        )
    elseif(DISTRO_PKG_MGR STREQUAL "zypper")
        execute_process(
            COMMAND rpm -q ${package_name}
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE check_result
        )
    else()
        set(check_result 1)
    endif()
    
    set(${result_var} ${check_result} PARENT_SCOPE)
endfunction()

# Check dependencies and build install command
set(INSTALL_COMMAND "")
set(MISSING_COUNT 0)

if(DISTRO_PKG_MGR STREQUAL "apt")
    set(PACKAGES ${DEB_PACKAGES})
elseif(DISTRO_PKG_MGR STREQUAL "dnf")
    set(PACKAGES ${RPM_PACKAGES})
elseif(DISTRO_PKG_MGR STREQUAL "pacman")
    set(PACKAGES ${ARCH_PACKAGES})
elseif(DISTRO_PKG_MGR STREQUAL "zypper")
    set(PACKAGES ${ZYPPER_PACKAGES})
else()
    set(PACKAGES ${DEB_PACKAGES})  # Default to Debian packages
endif()

message(STATUS "Checking dependencies...")
message("")

foreach(pkg ${PACKAGES})
    # Extract base package name for checking (remove -dev, -devel suffixes)
    string(REGEX REPLACE "-dev$" "" base_pkg ${pkg})
    string(REGEX REPLACE "-devel$" "" base_pkg ${base_pkg})
    
    # For some packages, check multiple possible names
    if(pkg MATCHES "libmotif")
        set(check_pkgs "libmotif-dev" "openmotif-dev" "libmotif")
    elseif(pkg MATCHES "openmotif")
        set(check_pkgs "openmotif-devel" "libmotif-devel" "openmotif")
    elseif(pkg MATCHES "build-essential")
        set(check_pkgs "build-essential" "gcc" "g++")
    elseif(pkg MATCHES "base-devel")
        set(check_pkgs "base-devel" "gcc" "make")
    else()
        set(check_pkgs ${pkg} ${base_pkg})
    endif()
    
    set(found FALSE)
    foreach(check_pkg ${check_pkgs})
        check_package(${check_pkg} pkg_result)
        if(pkg_result EQUAL 0)
            set(found TRUE)
            break()
        endif()
    endforeach()
    
    if(found)
        message(STATUS "  ✓ ${pkg}")
    else()
        message(STATUS "  ✗ ${pkg} (MISSING)")
        list(APPEND MISSING_DEPS ${pkg})
        math(EXPR MISSING_COUNT "${MISSING_COUNT} + 1")
    endif()
endforeach()

message("")
message(STATUS "==============================")

if(MISSING_COUNT EQUAL 0)
    message(STATUS "All dependencies are installed!")
else()
    message(STATUS "Missing ${MISSING_COUNT} package(s)")
    message("")
    message(STATUS "To install missing dependencies, run:")
    message("")
    
    if(DISTRO_PKG_MGR STREQUAL "apt")
        message(STATUS "  ${DISTRO_INSTALL_CMD} \\")
        foreach(pkg ${MISSING_DEPS})
            message(STATUS "    ${pkg} \\")
        endforeach()
        message(STATUS "")
        message(STATUS "Or as a single command:")
        string(REPLACE ";" " " MISSING_STR "${MISSING_DEPS}")
        message(STATUS "  ${DISTRO_INSTALL_CMD} ${MISSING_STR}")
    elseif(DISTRO_PKG_MGR STREQUAL "dnf")
        message(STATUS "  ${DISTRO_INSTALL_CMD} \\")
        foreach(pkg ${MISSING_DEPS})
            message(STATUS "    ${pkg} \\")
        endforeach()
        message(STATUS "")
        message(STATUS "Or as a single command:")
        string(REPLACE ";" " " MISSING_STR "${MISSING_DEPS}")
        message(STATUS "  ${DISTRO_INSTALL_CMD} ${MISSING_STR}")
    elseif(DISTRO_PKG_MGR STREQUAL "pacman")
        message(STATUS "  ${DISTRO_INSTALL_CMD} \\")
        foreach(pkg ${MISSING_DEPS})
            message(STATUS "    ${pkg} \\")
        endforeach()
        message(STATUS "")
        message(STATUS "Or as a single command:")
        string(REPLACE ";" " " MISSING_STR "${MISSING_DEPS}")
        message(STATUS "  ${DISTRO_INSTALL_CMD} ${MISSING_STR}")
    elseif(DISTRO_PKG_MGR STREQUAL "zypper")
        message(STATUS "  ${DISTRO_INSTALL_CMD} \\")
        foreach(pkg ${MISSING_DEPS})
            message(STATUS "    ${pkg} \\")
        endforeach()
        message(STATUS "")
        message(STATUS "Or as a single command:")
        string(REPLACE ";" " " MISSING_STR "${MISSING_DEPS}")
        message(STATUS "  ${DISTRO_INSTALL_CMD} ${MISSING_STR}")
    else()
        message(STATUS "  Please install the following packages manually:")
        foreach(pkg ${MISSING_DEPS})
            message(STATUS "    - ${pkg}")
        endforeach()
    endif()
    
    message("")
    message(STATUS "Note: Package names may vary between distributions.")
    message(STATUS "If installation fails, check your distribution's package repository")
    message(STATUS "for the equivalent package names.")
endif()

message("")

