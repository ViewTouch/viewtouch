#!/bin/sh
# The Qt that ViewTouch builds with and carries in its packages, from Qt's
# own binaries (download.qt.io), for this computer's CPU:
#
#   tools/get-qt.sh [VERSION] [PREFIX]      default: tools/qt-version, /opt/qt/VERSION
#
# Fetches only what ViewTouch uses (Qt Base, Declarative, SVG, Wayland,
# image formats, Lottie, Quick Timeline and ICU), checks each archive against the SHA-1 Qt publishes
# beside it, and unpacks it into PREFIX (PREFIX/lib/cmake/Qt6 is then what
# CMAKE_PREFIX_PATH points at). Needs curl and 7z (Debian/Ubuntu: 7zip;
# Fedora: 7zip). Already there: nothing to do.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
version=${1:-$(cat "$here/qt-version")}
prefix=${2:-/opt/qt/$version}
case "$(uname -m)" in
    x86_64) host=linux_x64; arch=linux_gcc_64; dir=gcc_64 ;;
    aarch64|arm64) host=linux_arm64; arch=linux_gcc_arm64; dir=gcc_arm64 ;;
    *) echo "No Qt binaries for $(uname -m): build Qt from source." >&2; exit 1 ;;
esac
[ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ] && { echo "Qt $version already in $prefix"; exit 0; }
sevenz=${SEVENZIP:-$(command -v 7z || command -v 7za || command -v 7zz || true)}
[ -n "$sevenz" ] || { echo "Install 7z (the 7zip package) first." >&2; exit 1; }
v=$(echo "$version" | tr -d .)
repo=https://download.qt.io/online/qtsdkrepository/$host/desktop/qt6_$v/qt6_$v
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
curl -fsSL -o "$work/Updates.xml" "$repo/Updates.xml" || { echo "No Qt $version for $host at download.qt.io." >&2; exit 1; }

# package name, its version, and the archives of it ViewTouch needs.
python3 - "$work/Updates.xml" "$v" "$arch" > "$work/archives" <<'PY'
import re, sys
xml, v, arch = open(sys.argv[1]).read(), sys.argv[2], sys.argv[3]
wanted = {f"qt.qt6.{v}.{arch}": ("qtbase", "qtdeclarative", "qtsvg", "qtwayland", "icu"),
          f"qt.qt6.{v}.addons.qtimageformats.{arch}": ("qtimageformats",),
          f"qt.qt6.{v}.addons.qtlottie.{arch}": ("qtlottie",),
          # Lottie plays its animations with Qt Quick Timeline (6.12 and later).
          f"qt.qt6.{v}.addons.qtquicktimeline.{arch}": ("qtquicktimeline",)}
for block in re.findall(r"<PackageUpdate>(.*?)</PackageUpdate>", xml, re.S):
    name = re.search(r"<Name>(.*?)</Name>", block).group(1)
    if name not in wanted:
        continue
    version = re.search(r"<Version>(.*?)</Version>", block).group(1)
    archives = re.search(r"<DownloadableArchives>(.*?)</DownloadableArchives>", block, re.S).group(1)
    for a in (x.strip() for x in archives.split(",")):
        if a.split("-")[0] in wanted[name]:
            print(name, version, a)
PY
[ -s "$work/archives" ] || { echo "Qt $version's index lists none of the parts ViewTouch needs." >&2; exit 1; }
mkdir -p "$work/unpacked"
while read -r name pkgversion archive; do
    url=$repo/$name/$pkgversion$archive
    echo "Qt $version: $archive"
    curl -fsSL -o "$work/a.7z" "$url"
    want=$(curl -fsSL "$url.sha1" | cut -c1-40)
    got=$(sha1sum "$work/a.7z" | cut -c1-40)
    [ "$want" = "$got" ] || { echo "$archive doesn't match Qt's checksum: not used." >&2; exit 1; }
    # Its libraries' symbolic links (libQt6Core.so -> libQt6Core.so.6) stay
    # inside Qt's folders; newer 7-Zips ask to be told so (25 and later:
    # -snld20, 23 and 24: -snld, older ones don't ask).
    "$sevenz" x -y -bd -snld20 -o"$work/unpacked" "$work/a.7z" >/dev/null 2>&1 ||
        "$sevenz" x -y -bd -snld -o"$work/unpacked" "$work/a.7z" >/dev/null 2>&1 ||
        "$sevenz" x -y -bd -o"$work/unpacked" "$work/a.7z" >/dev/null
done < "$work/archives"
# Qt's archives unpack to lib/, bin/... (some releases' to VERSION/gcc_64/...).
top=$work/unpacked
[ -d "$top/$version/$dir" ] && top=$top/$version/$dir
[ -f "$top/lib/cmake/Qt6/Qt6Config.cmake" ] || { echo "The archives didn't hold Qt's CMake files." >&2; exit 1; }
# ICU's archive unpacks beside lib/; Qt's libraries look for it in lib/.
for f in "$top"/libicu*; do [ -e "$f" ] && mv "$f" "$top/lib/"; done
mkdir -p "$(dirname "$prefix")"
rm -rf "$prefix"
mv "$top" "$prefix"
# Qt's binaries say where they were built; this is where they are now.
printf '[Paths]\nPrefix=..\n' > "$prefix/bin/qt.conf"
echo "Qt $version in $prefix"
