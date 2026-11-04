#!/bin/sh
# The Qt modules ViewTouch uses, built with ThreadSanitizer, for its tests to
# run under it (ThreadSanitizer only sees Qt's locks, threads and queued
# signals in a Qt built with it; with the distribution's Qt it reports races
# that aren't there and stops on a restarted QThread):
#
#   tools/build-qt-tsan.sh VERSION PREFIX      e.g. 6.8.2 /opt/qt-tsan
#
# Then build ViewTouch against it (its tools from the plain Qt, QT_HOST):
#   cmake -S modern -B build-tsan -DCMAKE_PREFIX_PATH=PREFIX -DQT_HOST_PATH=QT_HOST -DVTM_SANITIZE=thread ...
# About 30 to 60 minutes; Modern sanitizers keeps the result in its cache.
#
# Only the libraries are built with it: Qt's own tools (moc, rcc,
# qmltyperegistrar, qmlcachegen, the shader compiler...) come from a plain Qt of the same version
# (QT_HOST, tools/get-qt.sh's by default), as in a cross build. Built with
# ThreadSanitizer they run during the build too, slowly, and in CI's
# container one hung for five hours on Qt Quick (6.12).
set -eu
version=${1:?Qt version, like 6.8.2}
prefix=${2:?where to install it}
here=$(cd "$(dirname "$0")" && pwd)
host=${QT_HOST:-/opt/qt/$version}
sh "$here/get-qt.sh" "$version" "$host"
series=${version%.*}
work=${WORK:-$(mktemp -d)}
jobs=${JOBS:-$(nproc)}
mkdir -p "$work"
cd "$work"
for m in qtbase qtdeclarative qtsvg; do
    [ -d "$m-everywhere-src-$version" ] && continue
    # The newest versions are in official_releases, older ones move to archive.
    [ -f "$m-everywhere-src-$version.tar.xz" ] ||
        curl -fsSL -O "https://download.qt.io/official_releases/qt/$series/$version/submodules/$m-everywhere-src-$version.tar.xz" ||
        curl -fsSL -O "https://download.qt.io/archive/qt/$series/$version/submodules/$m-everywhere-src-$version.tar.xz"
    tar xf "$m-everywhere-src-$version.tar.xz"
done
flags="-fsanitize=thread -fno-omit-frame-pointer -O1 -g1"
(
    cd qtbase-everywhere-src-"$version"
    mkdir -p build && cd build
    ../configure -prefix "$prefix" -qt-host-path "$host" -release -force-debug-info -nomake examples -nomake tests -nomake benchmarks \
        -no-opengl -no-feature-vulkan -no-dbus -no-icu -no-glib -no-feature-sql-mysql -no-feature-sql-psql \
        -no-feature-sql-odbc -sql-sqlite -qt-sqlite -openssl-linked -- \
        -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
        -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=thread
    cmake --build . --parallel "$jobs"
    cmake --install .
)
for m in qtsvg qtdeclarative; do
    (
        cd "$m-everywhere-src-$version"
        mkdir -p build && cd build
        "$prefix/bin/qt-configure-module" .. -- -DQT_HOST_PATH="$host" -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
            -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=thread \
            -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF
        cmake --build . --parallel "$jobs"
        cmake --install .
    )
done
echo "Qt $version with ThreadSanitizer in $prefix"
