#!/bin/sh
# Builds ViewTouch and runs the whole test suite on Qt 6.8, the Qt of
# Debian 13 and Raspberry Pi OS (the computers in stores), in a clean
# debian:trixie container: what CI does, before a release. Needs Podman or
# Docker (on macOS, Docker Desktop or Podman's machine).
#
#   tools/test-qt68.sh            from anywhere in the repository (about 15 minutes)
set -eu
if [ "${1:-}" = --inside ]; then
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        g++ cmake ninja-build git qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools libgl-dev \
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
        qml6-module-qtquick-layouts qml6-module-qtquick-shapes qml6-module-qtquick-dialogs \
        qml6-module-qtquick-window qml6-module-qtqml-workerscript qml6-module-qttest \
        libqt6sql6-sqlite qt6-qpa-plugins fonts-dejavu-core fonts-noto-cjk fonts-noto-color-emoji \
        catch2 libssl-dev >/dev/null
    echo "Building on Qt $(dpkg-query -W -f '${Version}' qt6-base-dev)..."
    cmake -S /src/modern -B /b -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build /b -j"$(nproc)" 2>&1 | grep -E " error|FAILED" && exit 1
    cd /b
    QT_QPA_PLATFORM=offscreen timeout 1500 ./tests/vtm_tests 2>&1 | tee /tmp/tests.log |
        grep -E "FAILED|Cyclic|not ready|test cases|ReferenceError|TypeError" -A3 | head -60
    grep -q "^All tests passed" /tmp/tests.log
    exit
fi
repo=$(cd "$(dirname "$0")/../.." && pwd)
engine=$(command -v podman || command -v docker || true)
[ -n "$engine" ] || { echo "Install Podman or Docker first." >&2; exit 1; }
# :z lets SELinux (Fedora) share the folder with the container; others ignore it.
exec "$engine" run --rm -v "$repo:/src:ro,z" debian:trixie sh /src/modern/tools/test-qt68.sh --inside
