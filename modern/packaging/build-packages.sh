#!/bin/sh
# Build ViewTouch packages into modern/dist/.
#
#   packaging/build-packages.sh              for this machine (.rpm on Fedora, .deb on Debian/Ubuntu)
#   packaging/build-packages.sh fedora       in a Fedora container (podman or docker)
#   packaging/build-packages.sh debian       in a Debian 13 container (also for Raspberry Pi OS)
#   packaging/build-packages.sh ubuntu       in an Ubuntu 26.04 container
#   packaging/build-packages.sh deps         install the build tools here (as root)
#
# Containers build for this machine's CPU (x86_64 or aarch64). For the other
# one, run it there, or use the "Modern packages" GitHub workflow.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
modern=$(dirname "$here")
repo=$(dirname "$modern")
dist=$modern/dist

build_here() {
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" fedora "*|*" rhel "*) generator=RPM ;;
        *" debian "*|*" ubuntu "*) generator=DEB ;;
        *) echo "Unknown distribution ${ID:-}; building both" >&2; generator="DEB;RPM" ;;
    esac
    build=${BUILD_DIR:-$modern/build-package-${ID:-linux}-$(uname -m)}
    cmake -S "$modern" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
          -DVTM_BUILD_TESTS=OFF
    cmake --build "$build"
    mkdir -p "$dist"
    (cd "$build" && cpack -G "$generator" -B "$dist/.cpack-$$")
    # Named with the build number (viewtouch-...-b1250.deb): the store's
    # computer tells screens that are behind from it.
    number=$(sed -n 's/^#define VTM_BUILD_NUMBER //p' "$build/generated/vtm_build_number.hh")
    for f in "$dist/.cpack-$$"/*.deb "$dist/.cpack-$$"/*.rpm; do
        [ -e "$f" ] || continue
        mv "$f" "$dist/$(basename "${f%.*}")-b${number:-0}.${f##*.}"
    done
    rm -rf "$dist/.cpack-$$"
    ls -l "$dist"
}

deps_fedora="gcc-c++ cmake ninja-build rpm-build git qt6-qtbase-devel qt6-qtdeclarative-devel"
deps_debian="g++ cmake ninja-build dpkg-dev file git qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools libgl-dev"

install_deps() {
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" fedora "*|*" rhel "*) dnf -y install $deps_fedora ;;
        *" debian "*|*" ubuntu "*)
            apt-get update
            DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $deps_debian ;;
        *) echo "Install a C++23 compiler, CMake, Ninja and Qt 6.8+ (base, declarative) yourself." >&2; exit 1 ;;
    esac
}

in_container() {
    image=$1
    install=$2
    engine=$(command -v podman || command -v docker || true)
    if [ -z "$engine" ]; then
        echo "Install podman (or docker) to build in a container." >&2
        exit 1
    fi
    # The whole repository is mounted: the build uses textures from the legacy tree.
    "$engine" run --rm -v "$repo:/src:Z" -e BUILD_DIR=/tmp/build "$image" \
        sh -c "$install && /src/modern/packaging/build-packages.sh"
}

# Each package signed (an Ed25519 signature, PACKAGE.sig) with the key kept
# on this computer only: "Update from USB" and the installer check it.
sign_all() {
    key=${VTM_SIGNING_KEY:-$HOME/.config/viewtouch/signing/update-signing-key.pem}
    if [ ! -f "$key" ] || ! command -v openssl >/dev/null 2>&1; then
        echo "Not signed (no key at $key): \"Update from USB\" won't take these." >&2
        return 0
    fi
    for f in "$dist"/*.deb "$dist"/*.rpm; do
        [ -e "$f" ] || continue
        [ -e "$f.sig" ] && [ "$f.sig" -nt "$f" ] && continue
        openssl pkeyutl -sign -inkey "$key" -rawin -in "$f" -out "$f.sig"
        openssl pkeyutl -verify -pubin -inkey "$here/update-signing-key.pub.pem" -rawin -in "$f" -sigfile "$f.sig" >/dev/null
        echo "Signed $(basename "$f")"
    done
}

case "${1:-}" in
"") build_here ;;
deps) install_deps ;;
fedora) in_container fedora:44 "/src/modern/packaging/build-packages.sh deps" ;;
debian) in_container debian:trixie "/src/modern/packaging/build-packages.sh deps" ;;
ubuntu) in_container ubuntu:26.04 "/src/modern/packaging/build-packages.sh deps" ;;
-h|--help) sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//' ;;
*) echo "Unknown target: $1" >&2; exit 1 ;;
esac
# On this computer (not inside a build container): sign what was built.
case "${1:-}" in
deps|-h|--help) ;;
*) [ -f /run/.containerenv ] || [ -f /.dockerenv ] || sign_all ;;
esac
