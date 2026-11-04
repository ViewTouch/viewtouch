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

# The package's Qt is complete: installed into a scratch folder, nothing in
# it loads a Qt library from anywhere else (the system's Qt once stood in for
# a missing one), and the program starts.
check_bundle() {
    stage=$(mktemp -d)
    DESTDIR=$stage cmake --install "$1" >/dev/null
    home=$stage/usr/lib/vtmodern
    [ -x "$home/bin/vtmodern" ] || { echo "No bundled program in the package." >&2; exit 1; }
    outside=$(find "$home" -type f \( -name '*.so*' -o -path '*/bin/vtmodern' \) -exec ldd {} \; 2>/dev/null |
              grep -E 'libQt6|libicu' | grep -v "=> $home/" | sort -u || true)
    if [ -n "$outside" ]; then
        echo "The package's Qt isn't complete: these would come from outside it:" >&2
        echo "$outside" >&2
        exit 1
    fi
    # Every Qt QML module the screens import is in it (Qt/labs/lottieqt...).
    for module in $(sed -n 's/.*RELATIVEPATH;\([^;]*\);TYPE;module.*/\1/p' "$1/.qt/qml_imports/vtmodern_build.cmake" | sort -u); do
        case "$module" in ViewTouch*|QML) continue ;; esac   # ours, and the built-in QML
        [ -d "$home/qml/$module" ] || { echo "The package lacks the QML module $module." >&2; exit 1; }
    done
    QT_QPA_PLATFORM=offscreen "$stage/usr/bin/vtmodern" --version || { echo "The bundled program doesn't start." >&2; exit 1; }
    rm -rf "$stage"
}

build_here() {
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" fedora "*|*" rhel "*) generator=RPM ;;
        *" debian "*|*" ubuntu "*) generator=DEB ;;
        *) echo "Unknown distribution ${ID:-}; building both" >&2; generator="DEB;RPM" ;;
    esac
    build=${BUILD_DIR:-$modern/build-package-${ID:-linux}-$(uname -m)}
    # Reproducible: the same commit built again (same system, same folder)
    # gives the same package, byte for byte. Qt's resource compiler and
    # CPack stamp files with this time instead of the build's.
    SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-$(git -C "$repo" log -1 --format=%ct 2>/dev/null || date +%s)}
    export SOURCE_DATE_EPOCH
    # The Qt it carries (tools/qt-version), fetched once: VTM_QT names another.
    qt=${VTM_QT:-/opt/qt/$(cat "$modern/tools/qt-version")}
    sh "$modern/tools/get-qt.sh" "$(cat "$modern/tools/qt-version")" "$qt"
    cmake -S "$modern" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
          -DCMAKE_PREFIX_PATH="$qt" -DVTM_BUILD_TESTS=OFF
    # Which QML modules the screens import is scanned once and not again when
    # a screen imports a new one: scanned afresh, or the package lacks it.
    rm -f "${build:?}/.qt/qml_imports/vtmodern_build.cmake"
    cmake --build "$build"
    check_bundle "$build"
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

# The build tools, what tools/get-qt.sh needs (curl, 7z, python3), OpenGL's
# headers (Qt's CMake files ask for them), and the system libraries the Qt
# ViewTouch carries uses (dpkg-shlibdeps finds each one's package from them).
# (libSM, libICE: the aarch64 Qt 6.12 xcb plugin links them; x86_64's doesn't.)
deps_fedora="gcc-c++ cmake ninja-build rpm-build git curl ca-certificates python3 7zip mesa-libGL-devel mesa-libEGL-devel \
    libxkbcommon-devel libxkbcommon-x11 fontconfig freetype dbus-libs glib2 libwayland-client libwayland-cursor \
    libwayland-egl xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm libdrm mesa-libgbm \
    krb5-libs brotli libzstd libSM libICE"
deps_debian="g++ cmake ninja-build dpkg-dev file git curl ca-certificates python3 7zip libgl-dev libegl-dev \
    libxkbcommon-dev libxkbcommon-x11-0 libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0t64 libwayland-client0 \
    libwayland-cursor0 libwayland-egl1 libx11-xcb1 libxcb-cursor0 libxcb-glx0 libxcb-icccm4 libxcb-image0 \
    libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xkb1 \
    libdrm2 libgbm1 libgssapi-krb5-2 libbrotli1 libzstd1 libopengl0 libsm6 libice6"

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
