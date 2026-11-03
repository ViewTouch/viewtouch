#!/bin/sh
# Bugs found by reading the code instead of running it:
#
#   tools/static-analysis.sh BUILD-DIR [--update-baseline]
#
# - clang-tidy (checks in .clang-tidy) over ViewTouch's own C++, using the
#   build's compile_commands.json;
# - shellcheck (warnings and errors) over the packaging and tools scripts.
#
# A finding already in tools/static-analysis-baseline.txt doesn't fail (it
# was looked at and is fine, or waits for its fix); a new one does. Findings
# are compared without line numbers, so moving code doesn't count as new.
# CLANG_TIDY and SHELLCHECK name the programs (default: on the PATH);
# JOBS how many files at once.
set -eu
build=${1:?the build directory (with compile_commands.json)}
update=no
[ "${2:-}" = --update-baseline ] && update=yes
here=$(cd "$(dirname "$0")/.." && pwd)
tidy=${CLANG_TIDY:-clang-tidy}
check=${SHELLCHECK:-shellcheck}
jobs=${JOBS:-$(nproc)}
baseline=$here/tools/static-analysis-baseline.txt
"$tidy" --version >/dev/null 2>&1 || { echo "No clang-tidy ($tidy): set CLANG_TIDY." >&2; exit 1; }
"$check" --version >/dev/null 2>&1 || { echo "No shellcheck ($check): set SHELLCHECK." >&2; exit 1; }
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

# Our sources in the build (not tests, Qt's generated code or external/).
python3 - "$build/compile_commands.json" "$here" > "$out/files" <<'PY'
import json, os, sys
here = sys.argv[2]
seen = set()
for e in json.load(open(sys.argv[1])):
    f = os.path.realpath(os.path.join(e["directory"], e["file"]))
    rel = os.path.relpath(f, here)
    if rel.startswith(("app/", "core/", "layout/", "net/", "print/", "storage/", "ui/cpp/")) or rel == "main.cpp":
        if f not in seen:
            seen.add(f)
            print(f)
PY
echo "clang-tidy: $(wc -l < "$out/files") files, $jobs at once..."
# shellcheck disable=SC2016
xargs -P "$jobs" -I{} sh -c '"$0" -p "$1" --quiet --extra-arg=-Wno-unknown-warning-option "$2" > "$3/$(echo "$2" | tr / _).log" 2>/dev/null || true' \
    "$tidy" "$build" {} "$out" < "$out/files"
# "path:line:col: warning: message [check]" -> "path: [check] message", ours only, once each.
cat "$out"/*.log 2>/dev/null | sed -n 's#^'"$here"'/\([^:]*\):[0-9]*:[0-9]*: \(warning\|error\): \(.*\) \[\([^]]*\)\]$#\1: [\4] \3#p' |
    sort -u > "$out/found"

echo "shellcheck..."
for f in "$here"/packaging/* "$here"/packaging/tests/*.sh "$here"/tools/*.sh; do
    [ -f "$f" ] && head -1 "$f" | grep -q '^#!.*sh' && echo "$f"
done > "$out/scripts"
# shellcheck disable=SC2046
"$check" -S warning -f gcc $(cat "$out/scripts") 2>/dev/null |
    sed -n 's#^'"$here"'/\([^:]*\):[0-9]*:[0-9]*: \(warning\|error\): \(.*\) \[\(SC[0-9]*\)\]$#\1: [\4] \3#p' |
    sort -u >> "$out/found" || true
sort -u -o "$out/found" "$out/found"

if [ "$update" = yes ]; then
    {
        echo "# Known static analysis findings (tools/static-analysis.sh): each looked at."
        echo "# Fix one and take it out; never add one without a reason."
        cat "$out/found"
    } > "$baseline"
    echo "Baseline: $(grep -vc '^#' "$baseline" || true) findings."
    exit 0
fi
grep -v '^#' "$baseline" 2>/dev/null | sort -u > "$out/known" || true
comm -23 "$out/found" "$out/known" > "$out/new"
comm -13 "$out/found" "$out/known" > "$out/gone"
echo "$(wc -l < "$out/found") findings, $(wc -l < "$out/known") known."
if [ -s "$out/gone" ]; then
    echo "Fixed since (take them out of the baseline):"
    sed 's/^/  /' "$out/gone"
fi
if [ -s "$out/new" ]; then
    echo "NEW findings:"
    sed 's/^/  /' "$out/new"
    exit 1
fi
echo "No new findings."
