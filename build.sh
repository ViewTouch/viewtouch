#!/usr/bin/env bash
set -euo pipefail

# Minimal terminal-only build helper for ViewTouch
# - Installs missing packages for common Linux distros (apt/dnf/yum/pacman/zypper/apk)
# - Configures, builds and installs via CMake
# Usage: ./build.sh [--yes|-y] [--no-deps] [--prefix PATH] [--build-type TYPE] [--jobs N]

PROG="$(basename "$0")"
TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$TOP_DIR/build"
PREFIX="/usr/local"
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc 2>/dev/null || echo 1)"
AUTO_YES=0
INSTALL_DEPS=1

print_help() {
  cat <<EOF
Usage: $PROG [options]

Options:
  --yes, -y       Auto-accept installing missing packages
  --no-deps       Skip installing system dependencies
  --prefix PATH   Install prefix (default: ${PREFIX})
  --build-type T  CMake build type (default: ${BUILD_TYPE})
  --jobs N        Parallel build jobs (default: number of CPU cores)
  -h, --help      Show this help

Examples:
  # Build without installing OS packages
  ./build.sh --no-deps

  # Install deps (prompt) then build and install to /usr
  sudo ./build.sh

  # Auto-install deps, build and install to /usr/local
  ./build.sh --yes
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes|-y) AUTO_YES=1; shift ;;
    --no-deps) INSTALL_DEPS=0; shift ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    --build-type) BUILD_TYPE="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    -h|--help) print_help; exit 0 ;;
    *) echo "Unknown option: $1"; print_help; exit 2 ;;
  esac
done

# Package lists taken from the repo's cmake/install_dependencies.cmake
DEB_PACKAGES=(cmake build-essential git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev libmotif-dev libfreetype6-dev libfontconfig1-dev zlib1g-dev libpng-dev libjpeg-dev libgif-dev pkg-config libxext-dev libxcb1-dev)
RPM_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype-devel fontconfig-devel zlib-devel libpng-devel libjpeg-turbo-devel giflib-devel pkgconfig libXext-devel libxcb-devel)
ARCH_PACKAGES=(cmake base-devel git libx11 libxft libxmu libxpm libxrender libxt openmotif freetype2 fontconfig zlib libpng libjpeg-turbo giflib pkgconf libxext libxcb)
ZYPPER_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype2-devel fontconfig-devel zlib-devel libpng16-devel libjpeg-devel libgif-devel pkg-config libXext-devel libxcb-devel)
APK_PACKAGES=(cmake build-base git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev openmotif-dev freetype-dev fontconfig-dev zlib-dev libpng-dev libjpeg-turbo-dev giflib-dev pkgconfig libxext-dev libxcb-dev)

# detect package manager and select package list
PKG_MANAGER="unknown"
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
elif command -v brew >/dev/null 2>&1; then
  PKG_MANAGER="brew"
  PKGS=(cmake git pkg-config)
fi

check_installed() {
  local pkg="$1"
  case "$PKG_MANAGER" in
    apt)
      dpkg -s "$pkg" >/dev/null 2>&1 && return 0 || return 1
      ;;
    dnf|yum|zypper)
      rpm -q "$pkg" >/dev/null 2>&1 && return 0 || return 1
      ;;
    pacman)
      pacman -Qi "$pkg" >/dev/null 2>&1 && return 0 || return 1
      ;;
    apk)
      apk info -e "$pkg" >/dev/null 2>&1 && return 0 || return 1
      ;;
    brew)
      brew list "$pkg" >/dev/null 2>&1 && return 0 || return 1
      ;;
    *)
      return 1
      ;;
  esac
}

install_missing() {
  local missing=("${@}")
  if [[ ${#missing[@]} -eq 0 ]]; then
    echo "All required packages appear installed."
    return 0
  fi

  echo "Missing packages: ${missing[*]}"
  if [[ $AUTO_YES -ne 1 ]]; then
    read -r -p "Install missing packages? [y/N]: " yn
    [[ $yn =~ ^[Yy]$ ]] || { echo "Skipping package installation."; return 0; }
  fi

  case "$PKG_MANAGER" in
    apt)
      sudo apt-get update || true
      if ! sudo apt-get install -y "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo apt-get install -y "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    dnf)
      if ! sudo dnf install -y "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo dnf install -y "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    yum)
      if ! sudo yum install -y "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo yum install -y "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    pacman)
      sudo pacman -Syu --noconfirm || true
      if ! sudo pacman -S --noconfirm "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo pacman -S --noconfirm "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    zypper)
      sudo zypper refresh || true
      if ! sudo zypper install -y "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo zypper install -y "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    apk)
      if ! sudo apk add --no-cache "${missing[@]}"; then
        for p in "${missing[@]}"; do sudo apk add --no-cache "$p" || echo "Warning: failed to install $p"; done
      fi
      ;;
    brew)
      for p in "${missing[@]}"; do brew install "$p" || echo "Warning: failed to install $p"; done
      ;;
    *)
      echo "Unsupported package manager: $PKG_MANAGER" >&2
      return 1
      ;;
  esac
}

if [[ $INSTALL_DEPS -eq 1 ]]; then
  if [[ "$PKG_MANAGER" == "unknown" ]]; then
    echo "Could not detect a supported package manager. Run './check_dependencies.sh' for guidance." >&2
  else
    echo "Detected package manager: $PKG_MANAGER"
    # collect missing packages
    missing_list=()
    for pkg in "${PKGS[@]}"; do
      if ! check_installed "$pkg"; then
        missing_list+=("$pkg")
      fi
    done
    install_missing "${missing_list[@]}"
  fi
fi

# Configure, build, install
echo "Configuring (build dir: ${BUILD_DIR})"
cmake -S "$TOP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="$PREFIX"

echo "Building (jobs=${JOBS})"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Installing to ${PREFIX} (may require sudo)"
sudo cmake --install "$BUILD_DIR" --prefix "$PREFIX"

echo "Done. If packages failed to install, run './check_dependencies.sh' for manual instructions."
