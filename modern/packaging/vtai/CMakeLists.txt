# The vtmodern-ai package: VT Ai's engine (llama.cpp's server, a pinned
# release), its service and account. Separate from vtmodern (which works the
# same without it); built beside it by packaging/build-packages.sh:
#   cmake -S packaging/vtai -B build-vtai -DCMAKE_BUILD_TYPE=Release -DVTAI_VERSION=0.22.1700
#   cmake --build build-vtai && (cd build-vtai && cpack -G DEB)
# The model (a GGUF file, about 2 GB) isn't in it: vtmodern-device ai-model
# downloads the one the release page names, signed.
cmake_minimum_required(VERSION 3.28)   # (FetchContent EXCLUDE_FROM_ALL)
set(VTAI_VERSION "0.0.0" CACHE STRING "The ViewTouch version it goes with (MAJOR.MINOR.BUILD)")
project(vtmodern-ai VERSION ${VTAI_VERSION} LANGUAGES C CXX)

include(FetchContent)
include(GNUInstallDirs)

# llama.cpp, MIT. A release about two weeks old when chosen; its source
# checked against the hash (a changed download isn't built).
set(VTAI_LLAMA_TAG b12200)
FetchContent_Declare(llama
    URL https://github.com/ggml-org/llama.cpp/archive/refs/tags/${VTAI_LLAMA_TAG}.tar.gz
    URL_HASH SHA256=3ae917adc5894129427e67bfc6263b1b1b7f64dfb93c6fa4e425744544d7bc09
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    EXCLUDE_FROM_ALL)   # (its headers, libraries and programs aren't installed: only the server, below)

# One program, no shared libraries; only what the server needs.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)   # (off when it isn't the top project: the server needs it)
set(LLAMA_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER ON CACHE BOOL "" FORCE)
# No browser page (ViewTouch is its only user), and nothing downloaded while building it.
set(LLAMA_BUILD_UI OFF CACHE BOOL "" FORCE)
set(LLAMA_USE_PREBUILT_UI OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)   # built on CI: for the computers it runs on, not CI's
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    # The Raspberry Pi 5 / CM5 (Cortex-A76) and newer: dot product and fp16, llama.cpp's fast paths.
    set(GGML_CPU_ARM_ARCH "armv8.2-a+dotprod+fp16" CACHE STRING "" FORCE)
else()
    # A point-of-sale PC from the last ten years (Haswell or later).
    set(GGML_AVX ON CACHE BOOL "" FORCE)
    set(GGML_AVX2 ON CACHE BOOL "" FORCE)
    set(GGML_FMA ON CACHE BOOL "" FORCE)
    set(GGML_F16C ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(llama)

add_custom_target(vtai-server ALL DEPENDS llama-server)
install(PROGRAMS $<TARGET_FILE:llama-server> DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/vtmodern-ai)
install(FILES vtmodern-ai.service DESTINATION lib/systemd/system)
install(FILES vtmodern-ai.sysusers DESTINATION lib/sysusers.d RENAME vtmodern-ai.conf)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/vtmodern-ai)
install(FILES ${llama_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/vtmodern-ai RENAME llama.cpp-LICENSE)

set(CPACK_PACKAGE_NAME vtmodern-ai)
set(CPACK_PACKAGE_VENDOR "ViewTouch")
set(CPACK_PACKAGE_CONTACT "ViewTouch <https://github.com/ViewTouch/viewtouch>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VT Ai: ViewTouch's small AI, on the store's own computer")
set(CPACK_PACKAGE_DESCRIPTION
    "Turns a menu typed or pasted, a photo of one or a PDF into ViewTouch menu items, sizes and choices; "
    "answers a manager's questions about sales from ViewTouch's own reports. Runs on this computer only, "
    "below the point of sale; nothing leaves the store. The model is downloaded separately "
    "(vtmodern-device ai-model).")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ViewTouch/viewtouch")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)
set(VTAI_OS_TAG "")
if(EXISTS /etc/os-release)
    file(STRINGS /etc/os-release VTAI_OS_ID REGEX "^ID=")
    file(STRINGS /etc/os-release VTAI_OS_VERSION REGEX "^VERSION_ID=")
    string(REGEX REPLACE "^ID=\"?([^\"]*)\"?$" "\\1" VTAI_OS_ID "${VTAI_OS_ID}")
    string(REGEX REPLACE "^VERSION_ID=\"?([^\"]*)\"?$" "\\1" VTAI_OS_VERSION "${VTAI_OS_VERSION}")
    set(VTAI_OS_TAG "~${VTAI_OS_ID}${VTAI_OS_VERSION}")
endif()
set(CPACK_DEBIAN_PACKAGE_RELEASE "1${VTAI_OS_TAG}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Text in photos (Tesseract, English and Spanish) and PDFs (Poppler).
set(CPACK_DEBIAN_PACKAGE_DEPENDS "vtmodern, systemd, tesseract-ocr, tesseract-ocr-eng, tesseract-ocr-spa, poppler-utils")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_CURRENT_SOURCE_DIR}/postinst ${CMAKE_CURRENT_SOURCE_DIR}/prerm)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_REQUIRES "vtmodern, systemd, tesseract, tesseract-langpack-eng, tesseract-langpack-spa, poppler-utils")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/postinst)
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/prerm)
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/lib/systemd /usr/lib/systemd/system /usr/lib/sysusers.d /usr/share/doc /usr/lib /usr/libexec)
include(CPack)
