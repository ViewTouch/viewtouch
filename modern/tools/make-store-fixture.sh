#!/bin/sh
# A saved store from this release, for the upgrade tests
# (tests/fixtures/stores/store-<version>.db): a business day of the stress
# test's pretend staff, closed with End of Day, by this build's program.
#
#   tools/make-store-fixture.sh BUILD-DIR
#
# Run it at each release (the build of the release commit) and commit the
# file: every later ViewTouch must open it with nothing lost
# (tests/test_old_store.cpp).
set -eu
build=${1:?the build directory}
here=$(cd "$(dirname "$0")/.." && pwd)
program=$build/vtmodern
[ -x "$program" ] || { echo "No $program" >&2; exit 1; }
version=$(sed -n 's/^project(.* VERSION \([0-9]*\.[0-9]*\).*/\1/p' "$here/CMakeLists.txt" | head -1)
[ -n "$version" ] || { echo "No version in CMakeLists.txt" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
QT_QPA_PLATFORM=offscreen "$program" --stress --stress-hours 6 --stress-speed 120 --stress-exit \
    --data-dir "$work/store" --size 1280x800
out=$here/tests/fixtures/stores/store-$version.db
sqlite3 "$work/store/viewtouch.db" 'PRAGMA wal_checkpoint(TRUNCATE); VACUUM;' 2>/dev/null ||
    python3 -c 'import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); c.execute("PRAGMA wal_checkpoint(TRUNCATE)"); c.execute("VACUUM"); c.close()' "$work/store/viewtouch.db"
cp "$work/store/viewtouch.db" "$out"
echo "Saved $out ($(du -h "$out" | cut -f1))"
