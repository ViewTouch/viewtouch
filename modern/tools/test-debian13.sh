#!/bin/sh
# Builds ViewTouch and runs the whole test suite in a clean debian:trixie
# container (Debian 13 and Raspberry Pi OS: the computers in stores), with
# the Qt it carries: what CI does, before a release. Needs Podman or Docker.
#
#   tools/test-debian13.sh        from anywhere in the repository (about 15 minutes)
set -eu
if [ "${1:-}" = --inside ]; then
    apt-get update -qq
    sh /src/modern/tools/ci-setup.sh
    cp -r /src /tmp/src
    cd /tmp/src
    cmake -S modern -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/opt/qt/$(cat modern/tools/qt-version)"
    cmake --build /tmp/build
    QT_QPA_PLATFORM=offscreen /tmp/build/tests/vtm_tests
    exit
fi
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
engine=$(command -v podman || command -v docker || true)
[ -n "$engine" ] || { echo "Install Podman or Docker." >&2; exit 1; }
exec "$engine" run --rm -v "$repo:/src:ro,z" debian:trixie sh /src/modern/tools/test-debian13.sh --inside
