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
INTERACTIVE=0

# Detect interactive terminal (default to interactive when run in a TTY and not --yes)
if [[ -t 1 ]]; then
  INTERACTIVE=1
fi

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
    --no-ui) INTERACTIVE=0; shift ;;
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

# UI detection: prefer `dialog`, then `whiptail`; fall back to simple text menus
has_cmd() { command -v "$1" >/dev/null 2>&1; }
if has_cmd dialog; then
  UI_CMD=dialog
elif has_cmd whiptail; then
  UI_CMD=whiptail
else
  UI_CMD=
fi

ask_yes_no() {
  local msg="$1"
  if [[ -n "$UI_CMD" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$UI_CMD" == "dialog" ]]; then
      dialog --clear --yesno "$msg" 8 60
      return $?
    else
      # whiptail
      whiptail --yesno "$msg" 8 60
      return $?
    fi
  else
    # Fallback to simple prompt
    read -r -p "$msg [y/N]: " resp
    case "$resp" in
      [Yy]*) return 0 ;;
      *) return 1 ;;
    esac
  fi
}

show_info() {
  local msg="$1"
  if [[ -n "$UI_CMD" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$UI_CMD" == "dialog" ]]; then
      dialog --msgbox "$msg" 8 60
    else
      whiptail --msgbox "$msg" 8 60
    fi
  else
    echo "$msg"
  fi
}

# Step functions (so the TUI can invoke them individually)
install_dependencies() {
  if [[ "$PKG_MANAGER" == "unknown" ]]; then
    echo "Could not detect supported package manager. Run ./check_dependencies.sh for guidance." >&2
    return 1
  fi

  echo "Packages to install:"
  printf '  %s\n' "${PKGS[@]}"

  if [[ $AUTO_YES -ne 1 ]]; then
    if ! ask_yes_no "Install these packages?"; then
      echo "Skipping package installation.";
      return 0
    fi
  fi

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
}

do_configure() {
  echo "Configuring project (build dir: ${BUILD_DIR})"
  cmake -S "$TOP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
}

do_build() {
  echo "Building (jobs=${JOBS})"
  cmake --build "$BUILD_DIR" --parallel "$JOBS"
}

do_install() {
  echo "Installing to prefix: ${PREFIX} (may require sudo)"
  sudo cmake --install "$BUILD_DIR" --prefix "$PREFIX"
}

run_all() {
  if [[ $INSTALL_DEPS -eq 1 ]]; then
    install_dependencies
  fi
  do_configure
  do_build
  do_install
}

run_tui() {
  local tmpf
  tmpf=$(mktemp)
  while true; do
    if [[ -n "$UI_CMD" ]]; then
      if [[ "$UI_CMD" == "dialog" ]]; then
        dialog --clear --title "ViewTouch Build" --menu "Select action" 15 60 8 \
          1 "Install dependencies" \
          2 "Configure (cmake)" \
          3 "Build" \
          4 "Install" \
          5 "Settings" \
          6 "Run all" \
          7 "Quit" 2>"$tmpf" || true
        choice=$(cat "$tmpf")
      else
        whiptail --title "ViewTouch Build" --menu "Select action" 15 60 8 \
          1 "Install dependencies" \
          2 "Configure (cmake)" \
          3 "Build" \
          4 "Install" \
          5 "Settings" \
          6 "Run all" \
          7 "Quit" 2>"$tmpf" || true
        choice=$(cat "$tmpf")
      fi
    else
      echo "1) Install dependencies"
      echo "2) Configure (cmake)"
      echo "3) Build"
      echo "4) Install"
      echo "5) Settings"
      echo "6) Run all"
      echo "7) Quit"
      read -r -p "Choose: " choice
    fi

    case "$choice" in
      1) install_dependencies ;;
      2) do_configure ;;
      3) do_build ;;
      4) do_install ;;
      5)
        # Settings submenu
        if [[ -n "$UI_CMD" ]]; then
          if [[ "$UI_CMD" == "dialog" ]]; then
            dialog --inputbox "Install prefix" 8 60 "$PREFIX" 2>"$tmpf" && PREFIX=$(cat "$tmpf") || true
            dialog --inputbox "CMake build type" 8 60 "$BUILD_TYPE" 2>"$tmpf" && BUILD_TYPE=$(cat "$tmpf") || true
            dialog --inputbox "Parallel jobs" 8 60 "$JOBS" 2>"$tmpf" && JOBS=$(cat "$tmpf") || true
            dialog --yesno "Enable dependency installation by default?" 8 60 && INSTALL_DEPS=1 || INSTALL_DEPS=0
          else
            whiptail --inputbox "Install prefix" 8 60 "$PREFIX" 2>"$tmpf" && PREFIX=$(cat "$tmpf") || true
            whiptail --inputbox "CMake build type" 8 60 "$BUILD_TYPE" 2>"$tmpf" && BUILD_TYPE=$(cat "$tmpf") || true
            whiptail --inputbox "Parallel jobs" 8 60 "$JOBS" 2>"$tmpf" && JOBS=$(cat "$tmpf") || true
            whiptail --yesno "Enable dependency installation by default?" 8 60 && INSTALL_DEPS=1 || INSTALL_DEPS=0
          fi
        else
          read -r -p "Install prefix [$PREFIX]: " input || true; [[ -n "$input" ]] && PREFIX=$input
          read -r -p "CMake build type [$BUILD_TYPE]: " input || true; [[ -n "$input" ]] && BUILD_TYPE=$input
          read -r -p "Parallel jobs [$JOBS]: " input || true; [[ -n "$input" ]] && JOBS=$input
          read -r -p "Install dependencies by default? (y/N): " input || true; [[ $input =~ ^[Yy]$ ]] && INSTALL_DEPS=1 || INSTALL_DEPS=0
        fi
        ;;
      6) run_all ;;
      7) break ;;
      *) show_info "Unknown choice: $choice" ;;
    esac
  done
  rm -f "$tmpf"
}


if [[ $INTERACTIVE -eq 1 && $AUTO_YES -eq 0 ]]; then
  run_tui
else
  # Non-interactive default flow
  if [[ $INSTALL_DEPS -eq 1 ]]; then
    install_dependencies
  fi
  do_configure
  do_build
  do_install
  echo "Build and install complete."
fi

cat <<EOF
Notes:
 - If dependencies could not be auto-installed, run: ./check_dependencies.sh
 - To change install prefix, re-run with --prefix /your/path
 - For development builds with sanitizers, run: cmake -S . -B build -DENABLE_SANITIZERS=ON
EOF
