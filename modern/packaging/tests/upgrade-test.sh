#!/bin/sh
# What a store computer goes through, with real packages, as root in a
# throwaway Debian (Modern packages runs it in a container):
#
#   packaging/tests/upgrade-test.sh OLD.deb NEW.deb
#
# The old ViewTouch installed, a store made on it and served; the new one
# installed over it: it serves the same store. Then back to the old one
# (what update-verify does), with the store as it was before the upgrade:
# it serves it. Then up to the new one again.
set -eu
old=$(realpath "$1")
new=$(realpath "$2")
export DEBIAN_FRONTEND=noninteractive QT_QPA_PLATFORM=offscreen
store=$(mktemp -d)
before=$(mktemp -d)

step() { echo; echo "== $*"; }
version() { dpkg-query -W -f='${Version}' vtmodern; }
install_pkg() { apt-get install -y -qq --allow-downgrades "$1" >/dev/null; echo "installed $(version)"; }
# The store loads and is served: the server running on it hands out a pairing code.
serves() {
    vtmodern --headless --serve --port 47123 --data-dir "$store" > "$store/server.log" 2>&1 &
    pid=$!
    ok=no
    for _ in $(seq 60); do
        if vtmodern --pairing-code --data-dir "$store" >/dev/null 2>&1; then ok=yes; break; fi
        kill -0 "$pid" 2>/dev/null || break
        sleep 1
    done
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    if [ "$ok" != yes ]; then
        cat "$store/server.log"
        echo "FAILED: ViewTouch $(version) didn't serve the store"
        exit 1
    fi
    echo "ViewTouch $(version) serves the store"
}

step "The old ViewTouch, a store on it"
install_pkg "$old"
vtmodern --demo-data --data-dir "$store"
serves
cp "$store"/viewtouch.db* "$before"/

step "Up to the new one"
install_pkg "$new"
for f in /usr/bin/vtmodern /usr/libexec/vtmodern/vtmodern-device /usr/lib/systemd/system/vtmodern.service; do
    [ -e "$f" ] || [ -e "/lib/systemd/system/${f##*/}" ] || { echo "FAILED: no $f"; exit 1; }
done
serves
vtmodern --snapshot "$before/snapshot.db" --data-dir "$store"
[ -s "$before/snapshot.db" ] || { echo "FAILED: no snapshot"; exit 1; }

step "Back to the old one, with the store from before"
install_pkg "$old"
rm -f "$store"/viewtouch.db*
cp "$before"/viewtouch.db* "$store"/
serves

step "Up to the new one again"
install_pkg "$new"
serves
echo
echo "Upgrade, downgrade and upgrade: all fine."
