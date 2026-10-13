#!/bin/sh
# Install ViewTouch on a new Linux machine, then set up what it is.
#
# On a fresh Raspberry Pi OS (or Debian 13), one line does everything:
#   curl -fsSL https://raw.githubusercontent.com/No0ne558/viewtouchFork/Modernization/modern/packaging/install-viewtouch.sh | sudo sh
# It asks one question: a computer only for ViewTouch (no desktop, locked
# down, set up by touch on its screen after it restarts), or ViewTouch next to
# the desktop. To answer it ahead of time:
#   ... | sudo sh -s -- --station              only for ViewTouch: install, lock down, restart
#   ... | sudo sh -s -- --station --keep-ssh   ...and leave SSH on
#
# More:
#   sudo sh install-viewtouch.sh PACKAGE        install this package (.deb or .rpm) instead
#   sudo sh install-viewtouch.sh --build        build it from the source here (a few minutes)
#   sudo sh install-viewtouch.sh [PACKAGE] -- kitchen auto Grill
#                                               next to the desktop, set up without asking
#                                               (anything after -- goes to vtmodern-setup)
#   --no-reboot                                 with --station: don't restart at the end
#
# Works on Fedora (and RHEL-likes), Debian, Ubuntu and Raspberry Pi OS. On
# Debian 13 and Raspberry Pi OS it downloads a ready-made package; elsewhere
# it builds one from the source.
set -eu

REPO=${VTM_REPO:-https://github.com/No0ne558/viewtouchFork.git}
BRANCH=${VTM_BRANCH:-Modernization}
SRC=${VTM_SRC:-/opt/viewtouch-src}
# Ready-made packages: vtmodern-debian13-arm64.deb, ...
PACKAGES=${VTM_PACKAGES:-https://github.com/No0ne558/viewtouchFork/releases/download/modern-packages}

say() { printf '\n== %s\n' "$*"; }
die() { echo "$*" >&2; exit 1; }

for a in "$@"; do
    case "$a" in -h|--help) sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;; esac
done
[ "$(id -u)" -eq 0 ] || die "Run this with sudo."
[ -r /etc/os-release ] || die "Can't tell which Linux this is (no /etc/os-release)."
. /etc/os-release
case " ${ID:-} ${ID_LIKE:-} " in
    *" fedora "*|*" rhel "*) family=fedora ;;
    *" debian "*|*" ubuntu "*) family=debian ;;
    *) die "This installer knows Fedora, Debian, Ubuntu and Raspberry Pi OS; this is ${PRETTY_NAME:-unknown}." ;;
esac

pkg_install() {
    if [ "$family" = fedora ]; then
        dnf -y install "$@"
    else
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y "$@"
    fi
}

# --- the arguments ----------------------------------------------------------------------
package=
station=
keep_ssh=no
reboot=yes
build=no
while [ $# -gt 0 ]; do
    case "$1" in
        --) shift; station=no; break ;;
        --station) station=yes; shift ;;
        --keep-ssh) keep_ssh=yes; shift ;;
        --no-reboot) reboot=no; shift ;;
        --build) build=yes; shift ;;
        -h|--help) sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *.rpm|*.deb) package=$1; shift ;;
        *) die "Unknown argument: $1 (see --help)" ;;
    esac
done

# --- only for ViewTouch, or next to the desktop? ---------------------------------------------
if [ -z "$station" ]; then
    station=no
    if [ "$family" = debian ] && [ -r /dev/tty ]; then
        cat <<'EOF'

How will this computer be used?

  1) Only for ViewTouch: a register, kitchen screen, kiosk or time clock.
     No desktop and locked down; it restarts and you set it up by touch
     on its screen. (Recommended)
  2) ViewTouch next to the desktop, set up here with a text menu.

EOF
        printf 'Choose 1 or 2 [1]: '
        # No one to answer (not a real terminal): next to the desktop, changing nothing more.
        if ! { read -r answer < /dev/tty; } 2>/dev/null; then
            echo
            answer=2
        fi
        case "${answer:-1}" in
            1) station=yes ;;
            2) station=no ;;
            *) die "Choose 1 or 2." ;;
        esac
    fi
fi
[ "$station" = yes ] && [ "$family" != debian ] &&
    die "A computer only for ViewTouch needs Raspberry Pi OS or Debian 13."

# --- get a package ---------------------------------------------------------------------------
# Debian 13 / Raspberry Pi OS: the ready-made one, if there is one for this computer.
download() {   # url file
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 -o "$2" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$2" "$1"
    else
        return 1
    fi
}
if [ -z "$package" ] && [ "$build" = no ] && [ "$family" = debian ] && [ "${VERSION_ID:-}" = 13 ]; then
    arch=$(dpkg --print-architecture)
    url=$PACKAGES/vtmodern-debian13-$arch.deb
    say "Downloading ViewTouch ($arch)"
    tmpdir=$(mktemp -d)
    if download "$url" "$tmpdir/vtmodern.deb" && dpkg-deb --info "$tmpdir/vtmodern.deb" >/dev/null 2>&1; then
        package=$tmpdir/vtmodern.deb
        echo "Got $(dpkg-deb --field "$package" Package Version | tr '\n' ' ')"
    else
        echo "No ready-made package for this computer; building one instead."
    fi
fi
if [ -z "$package" ]; then
    here=$(cd "$(dirname "$0")" 2>/dev/null && pwd || true)
    if [ -n "$here" ] && [ -x "$here/build-packages.sh" ]; then
        modern=$(dirname "$here")   # run from a copy of the source
    else
        say "Getting the source"
        command -v git >/dev/null 2>&1 || pkg_install git
        if [ -d "$SRC/.git" ]; then
            git -C "$SRC" fetch --depth 1 origin "$BRANCH" && git -C "$SRC" checkout -q FETCH_HEAD
        else
            git clone --depth 1 --branch "$BRANCH" "$REPO" "$SRC"
        fi
        modern=$SRC/modern
    fi
    say "Installing the build tools"
    sh "$modern/packaging/build-packages.sh" deps
    say "Building ViewTouch for this machine (a few minutes)"
    sh "$modern/packaging/build-packages.sh"
    if [ "$family" = fedora ]; then
        package=$(ls -t "$modern"/dist/vtmodern-*.rpm 2>/dev/null | head -1)
    else
        package=$(ls -t "$modern"/dist/vtmodern_*.deb 2>/dev/null | head -1)
    fi
    [ -n "$package" ] || die "The build made no package; see the messages above."
fi
[ -f "$package" ] || die "No such package: $package"

# --- install it, and the screen's kiosk --------------------------------------------------------
say "Installing ViewTouch"
case "$package" in
    /*) ;;
    *) package=$(pwd)/$package ;;
esac
pkg_install "$package"
# cage runs ViewTouch full screen on a touch screen (not needed for a server
# without a screen, but small).
command -v cage >/dev/null 2>&1 || pkg_install cage || echo "Couldn't install cage: screens need it (install it later)."

# --- only for ViewTouch: lock it down, restart into the touch setup ------------------------------
if [ "$station" = yes ]; then
    if [ "$keep_ssh" = yes ]; then vtmodern-lockdown --keep-ssh; else vtmodern-lockdown; fi
    echo
    echo "ViewTouch is installed. After the restart the screen asks what this computer is"
    echo "(main register, register, kitchen screen, kiosk, time clock); answer by touch."
    if [ "$reboot" = yes ]; then
        echo "Restarting in 15 seconds (Ctrl+C to restart later with: sudo reboot)..."
        sleep 15
        systemctl reboot
    else
        echo "Restart when ready: sudo reboot"
    fi
    exit 0
fi

# --- next to the desktop: what is this machine? -------------------------------------------------
say "Setting up this machine"
if [ $# -gt 0 ]; then
    vtmodern-setup "$@"
elif [ -r /dev/tty ]; then
    vtmodern-setup < /dev/tty   # the menu, even when this script came through a pipe
else
    echo "Installed. Now run:  sudo vtmodern-setup   (it asks what this machine is)"
fi
