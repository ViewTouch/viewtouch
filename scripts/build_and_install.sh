#!/usr/bin/env bash
set -euo pipefail

# build_and_install.sh
# Installs system dependencies, builds the project with CMake, and installs it.
# Usage:
#   ./scripts/build_and_install.sh [--yes] [--no-deps] [--prefix PREFIX] [--build-type TYPE] [--jobs N]
# Examples:
#   ./scripts/build_and_install.sh --yes
#   ./scripts/build_and_install.sh --prefix /usr --build-type Release

PROG="$(basename "$0")"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$TOP_DIR/build"
PREFIX="/usr/local"
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc)"
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

# Parse args
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

cd "$TOP_DIR"

# Detect distro
ID=""
ID_LIKE=""
if [[ -f /etc/os-release ]]; then
  . /etc/os-release
  ID=${ID:-}
  ID_LIKE=${ID_LIKE:-}
fi

echo "Detected distro: ${ID:-unknown} (ID_LIKE=${ID_LIKE:-})"

# Package lists (mirror of cmake/install_dependencies.cmake)
DEB_PACKAGES=(cmake build-essential git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev libmotif-dev libfreetype6-dev libfontconfig1-dev zlib1g-dev libpng-dev libjpeg-dev libgif-dev pkg-config)
RPM_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype-devel fontconfig-devel zlib-devel libpng-devel libjpeg-turbo-devel giflib-devel pkgconfig)
ARCH_PACKAGES=(cmake base-devel git libx11 libxft libxmu libxpm libxrender libxt openmotif freetype2 fontconfig zlib libpng libjpeg-turbo giflib pkgconf)
ZYPPER_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype2-devel fontconfig-devel zlib-devel libpng16-devel libjpeg-devel libgif-devel pkg-config)

PKG_CMD=""
PKGS=()
if [[ "$ID" == "ubuntu" || "$ID" == "debian" || "$ID_LIKE" =~ debian ]]; then
  PKG_CMD="sudo apt-get update && sudo apt-get install -y"
  PKGS=("${DEB_PACKAGES[@]}")
elif [[ "$ID" == "fedora" || "$ID" == "rhel" || "$ID" == "centos" || "$ID_LIKE" =~ fedora ]]; then
  PKG_CMD="sudo dnf install -y"
  PKGS=("${RPM_PACKAGES[@]}")
elif [[ "$ID" == "arch" || "$ID_LIKE" =~ arch ]]; then
  PKG_CMD="sudo pacman -S --noconfirm"
  PKGS=("${ARCH_PACKAGES[@]}")
elif [[ "$ID" == "opensuse" || "$ID" == "sles" || "$ID_LIKE" =~ suse ]]; then
  PKG_CMD="sudo zypper install -y"
  PKGS=("${ZYPPER_PACKAGES[@]}")
else
  echo "Unknown distribution; will show dependency checker output instead of automatic install."
  INSTALL_DEPS=0
fi

if [[ $INSTALL_DEPS -eq 1 ]]; then
  echo "The following packages will be installed (may include development packages):"
  printf '  %s\n' "${PKGS[@]}"

  if [[ $AUTO_YES -ne 1 ]]; then
    read -r -p "Install these packages? [y/N]: " yn
    case "$yn" in
      [Yy]*) ;;
      *) echo "Skipping package installation."; INSTALL_DEPS=0 ;;
    esac
  fi
fi

if [[ $INSTALL_DEPS -eq 1 && -n "$PKG_CMD" ]]; then
  echo "Installing packages..."
  set -x
  ${PKG_CMD} ${PKGS[*]}
  set +x
else
  echo "Skipping automatic package install. You can run ./check_dependencies.sh to get distro-specific suggestions."
fi

# Configure, build, install
echo "Configuring project (build dir: ${BUILD_DIR})"
cmake -S "$TOP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "Building (jobs=${JOBS})"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "Installing to prefix: ${PREFIX} (may require sudo)"
sudo cmake --install "$BUILD_DIR" --prefix "$PREFIX"

echo "Build and install complete."

# Post-install notes
cat <<EOF
Notes:
 - If dependencies could not be auto-installed, run: ./check_dependencies.sh
 - To change install prefix, re-run with --prefix /your/path
 - For development builds with sanitizers, run: cmake -S . -B build -DENABLE_SANITIZERS=ON
EOF
