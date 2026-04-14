#!/usr/bin/env bash
set -euo pipefail

# build.sh - interactive build/install helper for ViewTouch
# Supports graphical UI via zenity or kdialog when available, dialog/whiptail TUI, and a text fallback.
# Usage: ./build.sh [--yes] [--no-deps] [--no-ui] [--prefix PREFIX] [--build-type TYPE] [--jobs N]

PROG="$(basename "$0")"
TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$TOP_DIR/build"
PREFIX="/usr/local"
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc || echo 1)"
AUTO_YES=0
INSTALL_DEPS=1
INTERACTIVE=0

if [[ -t 1 ]]; then
  INTERACTIVE=1
fi

print_help() {
  cat <<EOF
Usage: $PROG [options]

Options:
  --yes            Auto-accept installing packages (non-interactive)
  --no-deps        Skip installing system dependencies
  --no-ui          Force non-interactive (text) mode
  --prefix PATH    Install prefix (default: /usr/local)
  --build-type T   CMake build type (default: ${BUILD_TYPE})
  --jobs N         Parallel build jobs (default: number of CPU cores)
  -h, --help       Show this help

This helper will try to install system packages, configure, build, and install ViewTouch.
It prefers a graphical UI (`zenity` or `kdialog`) when run in a graphical session, falls
back to `dialog`/`whiptail`, and finally to a plain text menu on the terminal.
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

# Detect distro via package manager presence
DEB_PACKAGES=(cmake build-essential git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev libmotif-dev libfreetype6-dev libfontconfig1-dev zlib1g-dev libpng-dev libjpeg-dev libgif-dev pkg-config libxext-dev libxcb1-dev zenity kdialog)
RPM_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype-devel fontconfig-devel zlib-devel libpng-devel libjpeg-turbo-devel giflib-devel pkgconfig libXext-devel libxcb-devel zenity kdialog)
ARCH_PACKAGES=(cmake base-devel git libx11 libxft libxmu libxpm libxrender libxt openmotif freetype2 fontconfig zlib libpng libjpeg-turbo giflib pkgconf libxext libxcb zenity kdialog)
ZYPPER_PACKAGES=(cmake gcc gcc-c++ make git libX11-devel libXft-devel libXmu-devel libXpm-devel libXrender-devel libXt-devel openmotif-devel freetype2-devel fontconfig-devel zlib-devel libpng16-devel libjpeg-devel libgif-devel pkg-config libXext-devel libxcb-devel zenity kdialog)
APK_PACKAGES=(cmake build-base git libx11-dev libxft-dev libxmu-dev libxpm-dev libxrender-dev libxt-dev openmotif-dev freetype-dev fontconfig-dev zlib-dev libpng-dev libjpeg-turbo-dev giflib-dev pkgconfig libxext-dev libxcb-dev zenity kdialog)

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

# Detect available UI helpers
has_cmd() { command -v "$1" >/dev/null 2>&1; }
GUI_TOOL=""
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" || "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
  if has_cmd zenity; then
    GUI_TOOL=zenity
  elif has_cmd kdialog; then
    GUI_TOOL=kdialog
  fi
fi

TUI_CMD=""
if has_cmd dialog; then
  TUI_CMD=dialog
elif has_cmd whiptail; then
  TUI_CMD=whiptail
fi

ask_yes_no() {
  local msg="$1"
  if [[ -n "$GUI_TOOL" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$GUI_TOOL" == "zenity" ]]; then
      zenity --question --text="$msg" --width=500
      return $?
    else
      kdialog --yesno "$msg"
      return $?
    fi
  elif [[ -n "$TUI_CMD" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$TUI_CMD" == "dialog" ]]; then
      dialog --clear --yesno "$msg" 8 60
      return $?
    else
      whiptail --yesno "$msg" 8 60
      return $?
    fi
  else
    read -r -p "$msg [y/N]: " resp
    case "$resp" in
      [Yy]*) return 0 ;;
      *) return 1 ;;
    esac
  fi
}

show_info() {
  local msg="$1"
  if [[ -n "$GUI_TOOL" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$GUI_TOOL" == "zenity" ]]; then
      zenity --info --text="$msg" --width=500
    else
      kdialog --msgbox "$msg"
    fi
  elif [[ -n "$TUI_CMD" && $INTERACTIVE -eq 1 ]]; then
    if [[ "$TUI_CMD" == "dialog" ]]; then
      dialog --msgbox "$msg" 8 60
    else
      whiptail --msgbox "$msg" 8 60
    fi
  else
    echo "$msg"
  fi
}

# Step functions
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
    if [[ -n "$GUI_TOOL" && $INTERACTIVE -eq 1 ]]; then
      if [[ "$GUI_TOOL" == "zenity" ]]; then
        choice=$(zenity --list --radiolist --title "ViewTouch Build" --column "Select" --column "Action" TRUE "install" "Install dependencies" FALSE "configure" "Configure (cmake)" FALSE "build" "Build" FALSE "installprog" "Install" FALSE "settings" "Settings" FALSE "runall" "Run all" FALSE "quit" "Quit" --height=360 --width=640) || true
      else
        # kdialog menu prints selection to stdout
        choice=$(kdialog --menu "Select action" install "Install dependencies" configure "Configure (cmake)" build "Build" installprog "Install" settings "Settings" runall "Run all" quit "Quit") || true
      fi
    elif [[ -n "$TUI_CMD" && $INTERACTIVE -eq 1 ]]; then
      if [[ "$TUI_CMD" == "dialog" ]]; then
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
      install|1) install_dependencies ;;
      configure|2) do_configure ;;
      build|3) do_build ;;
      installprog|4) do_install ;;
      settings|5)
        if [[ -n "$GUI_TOOL" && $INTERACTIVE -eq 1 ]]; then
          if [[ "$GUI_TOOL" == "zenity" ]]; then
            PREFIX=$(zenity --entry --title "Settings" --text "Install prefix" --entry-text "$PREFIX" ) || true
            BUILD_TYPE=$(zenity --entry --title "Settings" --text "CMake build type" --entry-text "$BUILD_TYPE" ) || true
            JOBS=$(zenity --entry --title "Settings" --text "Parallel jobs" --entry-text "$JOBS" ) || true
            if zenity --question --text "Enable dependency installation by default?" ; then INSTALL_DEPS=1; else INSTALL_DEPS=0; fi
          else
            PREFIX=$(kdialog --inputbox "Install prefix" "$PREFIX" ) || true
            BUILD_TYPE=$(kdialog --inputbox "CMake build type" "$BUILD_TYPE" ) || true
            JOBS=$(kdialog --inputbox "Parallel jobs" "$JOBS" ) || true
            if kdialog --yesno "Enable dependency installation by default?" ; then INSTALL_DEPS=1; else INSTALL_DEPS=0; fi
          fi
        else
          read -r -p "Install prefix [$PREFIX]: " input || true; [[ -n "$input" ]] && PREFIX=$input
          read -r -p "CMake build type [$BUILD_TYPE]: " input || true; [[ -n "$input" ]] && BUILD_TYPE=$input
          read -r -p "Parallel jobs [$JOBS]: " input || true; [[ -n "$input" ]] && JOBS=$input
          read -r -p "Install dependencies by default? (y/N): " input || true; [[ $input =~ ^[Yy]$ ]] && INSTALL_DEPS=1 || INSTALL_DEPS=0
        fi
        ;;
      runall|6) run_all ;;
      quit|7) break ;;
      "") ;; # no selection/cancel
      *) show_info "Unknown choice: $choice" ;;
    esac
  done
  rm -f "$tmpf"
}

# Entry point
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
