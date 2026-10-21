#!/bin/sh
# Publish ViewTouch to the release page that store computers check for
# updates (Home screen -> Updates -> Check for a New ViewTouch):
#
#   packaging/publish.sh [--apk-arm64 FILE] [--apk-armv7 FILE] [--dry-run]
#
# Takes the newest Debian 13 package for each architecture in modern/dist
# (built and signed by build-packages.sh), the Ubuntu 26.04 and Fedora 44
# ones there (for new installs), and the tablet apps if given;
# signs the apps and latest.json (what's newest, and its files) with the key
# on this computer; uploads them all, each with its .sig and .sha256.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
modern=$(dirname "$here")
dist=$modern/dist
release=${VTM_RELEASE:-modern-packages}
key=${VTM_SIGNING_KEY:-$HOME/.config/viewtouch/signing/update-signing-key.pem}
pub=$here/update-signing-key.pub.pem
apk64=
apk32=
dry=no
while [ $# -gt 0 ]; do
    case "$1" in
        --apk-arm64) apk64=$2; shift ;;
        --apk-armv7) apk32=$2; shift ;;
        --dry-run) dry=yes ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done
[ -f "$key" ] || { echo "No signing key at $key" >&2; exit 1; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
sign() {   # file: its .sig (checked) and .sha256
    openssl pkeyutl -sign -inkey "$key" -rawin -in "$1" -out "$1.sig"
    openssl pkeyutl -verify -pubin -inkey "$pub" -rawin -in "$1" -sigfile "$1.sig" >/dev/null
    (cd "$(dirname "$1")" && sha256sum "$(basename "$1")" > "$(basename "$1").sha256")
}

version=
packages=
for arch in arm64 amd64; do
    deb=$(ls -t "$dist"/vtmodern_*~debian13_"$arch"*.deb 2>/dev/null | head -1 || true)
    [ -n "$deb" ] || continue
    v=$(dpkg-deb --field "$deb" Version 2>/dev/null || basename "$deb" | sed 's/^vtmodern_\([^_]*\)_.*/\1/')
    v=${v%%-*}
    # Store computers update when this build (0.9.<build>) is past theirs:
    # build 0 is a package made without the git history, never newer.
    case "$(echo "$v" | cut -s -d. -f3)" in
        ''|0|*[!0-9]*) false ;;
        *) true ;;
    esac || {
        echo "$(basename "$deb") is version $v, without a build number (built without the git history?): not publishing." >&2
        exit 1
    }
    if [ -n "$version" ] && [ "$v" != "$version" ]; then
        echo "The packages are different versions ($version, $v): build them together." >&2
        exit 1
    fi
    version=$v
    name=vtmodern-debian13-$arch.deb
    cp "$deb" "$out/$name"
    sign "$out/$name"
    packages="$packages${packages:+,
}    \"debian13-$arch\": \"$name\""
    echo "Package ($arch): $(basename "$deb") as $name"
done
[ -n "$version" ] || { echo "No Debian 13 package in $dist" >&2; exit 1; }

# Ubuntu and Fedora too, for installing (install-viewtouch.sh): store
# computers update from the Debian 13 ones only.
for spec in "ubuntu26.04-arm64:vtmodern_*~ubuntu26.04_arm64*.deb" "ubuntu26.04-amd64:vtmodern_*~ubuntu26.04_amd64*.deb" \
            "fedora44-aarch64:vtmodern-*.fc44.aarch64*.rpm" "fedora44-x86_64:vtmodern-*.fc44.x86_64*.rpm"; do
    what=${spec%%:*}; pattern=${spec#*:}
    # shellcheck disable=SC2086
    file=$(ls -t "$dist"/$pattern 2>/dev/null | head -1 || true)
    [ -n "$file" ] || continue
    v=$(basename "$file" | sed -n 's/^vtmodern[_-]\([0-9.]*\)-.*/\1/p')
    if [ "$v" != "$version" ]; then
        echo "$(basename "$file") is version $v, not $version: build them together." >&2
        exit 1
    fi
    name=vtmodern-$what.${file##*.}
    cp "$file" "$out/$name"
    sign "$out/$name"
    packages="$packages,
    \"$what\": \"$name\""
    echo "Package ($what): $(basename "$file") as $name"
done

apps=
for pair in "arm64-v8a:$apk64:ViewTouch-android-arm64.apk" "armeabi-v7a:$apk32:ViewTouch-android-armv7.apk"; do
    abi=${pair%%:*}; rest=${pair#*:}; file=${rest%%:*}; name=${rest#*:}
    [ -n "$file" ] || continue
    # Its own build (ViewTouch-b1351-arm64-v8a.apk): what tablets compare with.
    appbuild=$(basename "$file" | sed -n 's/^ViewTouch-b\([0-9]*\)-.*/\1/p')
    [ -n "$appbuild" ] || { echo "$file: name it ViewTouch-b<build>-$abi.apk (as built)" >&2; exit 1; }
    cp "$file" "$out/$name"
    sign "$out/$name"
    apps="$apps${apps:+,
}    \"android-$abi\": \"$name\",
    \"android-$abi-build\": \"$appbuild\""
    echo "Tablet app ($abi): $(basename "$file") as $name"
done

cat > "$out/latest.json" <<JSON
{
  "version": "$version",
  "published": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "packages": {
$packages
  },
  "apps": {
$apps
  }
}
JSON
sign "$out/latest.json"
echo "latest.json: ViewTouch $version"
ls -l "$out"; cat "$out/latest.json"
if [ "$dry" = yes ]; then
    echo "(dry run: nothing uploaded)"
    exit 0
fi
gh release upload "$release" "$out"/* --clobber
echo "Published ViewTouch $version to $release."
