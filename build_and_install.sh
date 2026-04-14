#!/usr/bin/env bash
set -euo pipefail

# build_and_install.sh - installs system deps, builds and installs ViewTouch
# Usage: ./build_and_install.sh [--yes] [--no-deps] [--prefix PREFIX] [--build-type TYPE] [--jobs N]

PROG="$(basename "$0")"
TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$TOP_DIR/build"
PREFIX="/usr/local"
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc || echo 1)"
AUTO_YES=0
INSTALL_DEPS=1

print_help() {
  cat <<EOF
Usage: $PROG [options]

Options:
  --yes            Auto-accept installing packages (non-interactive)
  --no-deps        Skip installing system dependencies
  --prefix PATH    Install prefix (default: /usr/local)
  --build-type T   CMake build type (default: ${BUILD_TYPE})
  --jobs N         Parallel build jobs (default: number of CPU cores)
  -h, --help       Show this help

This script will attempt to install required system packages for your distribution
and then run CMake configure, build, and install.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes) AUTO_YES=1; shift ;;
    --no-deps) INSTALL_DEPS=0; shift ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    --build-type) BUILD_TYPE="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    -h|--help) print_help; exit 0 ;;
    *) echo "Unknown option: $1"; print_help; exit 2 ;;
  esac
done

echo "Top dir: $TOP_DIR"

# Detect distro via /etc/os-release and available package managers
ID=""
ID_LIKE=""
if [[ -f /etc/os-release ]]; then
  . /etc/os-release
  ID=${ID:-}
  ID_LIKE=${ID_LIKE:-}
fi

echo "Detected distro: ${ID:-unknown} (ID_LIKE=${ID_LIKE:-})"

# Package sets (broad coverage across common distros)
DEB_PACKAGES=(cmake build-essential git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev libmotif-dev libfreetype6-dev libfontconfig1-dev zlib1g-dev libpng-dev libjpeg-dev libgif-dev pkg-config libxext-dev libxcb1-dev)
RPM_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype-devel fontconfig-devel zlib-devel libpng-devel libjpeg-turbo-devel giflib-devel pkgconfig libXext-devel libxcb-devel)
ARCH_PACKAGES=(cmake base-devel git libx11 libxft libxmu libxpm libxrender libxt openmotif freetype2 fontconfig zlib libpng libjpeg-turbo giflib pkgconf libxext libxcb)
ZYPPER_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype2-devel fontconfig-devel zlib-devel libpng16-devel libjpeg-devel libgif-devel pkg-config libXext-devel libxcb-devel)
APK_PACKAGES=(cmake build-base git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev openmotif-dev freetype-dev fontconfig-dev zlib-dev libpng-dev libjpeg-turbo-dev giflib-dev pkgconfig libxext-dev libxcb-dev)

PKG_MANAGER=""
PKGS=()

if command -v apt-get >/dev/null 2>&1; then
  PKG_MANAGER="apt"
  PKGS=("${DEB_PACKAGES[@]}")
elif command -v dnf >/dev/null 2>&1; then
  PKG_MANAGER="dnf"
  PKGS=("${RPM_PACKAGES[@]}")
elif command -v yum >/dev/null 2>&1; then
  PKG_MANAGER="yum"
  PKGS=("${RPM_PACKAGES[@]}")
elif command -v pacman >/dev/null 2>&1; then
  PKG_MANAGER="pacman"
  PKGS=("${ARCH_PACKAGES[@]}")
elif command -v zypper >/dev/null 2>&1; then
  PKG_MANAGER="zypper"
  PKGS=("${ZYPPER_PACKAGES[@]}")
elif command -v apk >/dev/null 2>&1; then
  PKG_MANAGER="apk"
  PKGS=("${APK_PACKAGES[@]}")
else
  PKG_MANAGER="unknown"
fi

if [[ $INSTALL_DEPS -eq 1 ]]; then
  if [[ "$PKG_MANAGER" == "unknown" ]]; then
    echo "Could not detect supported package manager. Run ./check_dependencies.sh for guidance." >&2
    INSTALL_DEPS=0
  else
    echo "Packages to install:"
    printf '  %s\n' "${PKGS[@]}"
    if [[ $AUTO_YES -ne 1 ]]; then
      read -r -p "Install these packages? [y/N]: " yn
      case "$yn" in
        [Yy]*) ;;
        *) echo "Skipping package installation."; INSTALL_DEPS=0 ;;
      esac
    fi
  fi
fi

if [[ $INSTALL_DEPS -eq 1 ]]; then
  echo "Installing packages using: $PKG_MANAGER"
  set -x
  case "$PKG_MANAGER" in
    apt)
      sudo apt-get update
      sudo apt-get install -y "${PKGS[@]}"
      ;;
    dnf)
      sudo dnf install -y "${PKGS[@]}"
      ;;
    yum)
      sudo yum install -y "${PKGS[@]}"
      ;;
    pacman)
      sudo pacman -Syu --noconfirm "${PKGS[@]}"
      ;;
    zypper)
      sudo zypper refresh
      sudo zypper install -y "${PKGS[@]}"
      ;;
    apk)
      sudo apk update
      sudo apk add --no-cache "${PKGS[@]}"
      ;;
    *)
      echo "Unsupported package manager: $PKG_MANAGER" >&2
      ;;
  esac
  set +x
fi

# Configure, build, install
echo "Configuring project (build dir: ${BUILD_DIR})"
cmake -S "$TOP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building (jobs=${JOBS})"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Installing to prefix: ${PREFIX} (may require sudo)"
sudo cmake --install "$BUILD_DIR" --prefix "$PREFIX"

echo "Build and install complete."

cat <<EOF
Notes:
 - If dependencies could not be auto-installed, run: ./check_dependencies.sh
 - To change install prefix, re-run with --prefix /your/path
 - For development builds with sanitizers, run: cmake -S . -B build -DENABLE_SANITIZERS=ON
EOF
