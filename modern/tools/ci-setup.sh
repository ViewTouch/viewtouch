#!/bin/sh
# Everything a clean Debian, Ubuntu or Fedora needs to build ViewTouch and
# run its tests (as root): the build tools and libraries
# (packaging/build-packages.sh deps), the fonts the tests measure text with,
# and the Qt it's built with (tools/get-qt.sh into /opt/qt/VERSION, or
# $VTM_QT). CI runs it first; so does tools/test-debian13.sh.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
sh "$here/../packaging/build-packages.sh" deps
. /etc/os-release
case " ${ID:-} ${ID_LIKE:-} " in
    *" fedora "*|*" rhel "*) dnf -y install dejavu-sans-fonts google-noto-sans-cjk-fonts google-noto-emoji-fonts ;;
    *) DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
           fonts-dejavu-core fonts-noto-cjk fonts-noto-color-emoji libssl-dev ;;
esac
version=$(cat "$here/qt-version")
sh "$here/get-qt.sh" "$version" "${VTM_QT:-/opt/qt/$version}"
