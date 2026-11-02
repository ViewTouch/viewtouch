#!/usr/bin/env python3
"""Line coverage of ViewTouch's own code, from a build made with
-DVTM_COVERAGE=ON after the tests ran in it.

    tools/coverage.py <build dir> [--functions core app net storage]

Prints each folder's lines run, the files least covered, and (with
--functions) every function the tests never run in those folders, biggest
first. gcov's JSON (GCC 9 or later); nothing else needed.
"""
import argparse
import collections
import json
import os
import subprocess
import sys

OURS = ("app", "core", "layout", "net", "print", "storage", "ui/cpp", "main.cpp")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--functions", nargs="*", default=None, help="folders to list never-run functions for")
    ap.add_argument("--worst", type=int, default=25)
    args = ap.parse_args()
    source = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))

    lines = collections.defaultdict(dict)        # file -> line -> count (the most any unit saw)
    functions = {}                               # (file, start) -> [name, count, lines]
    gcdas = []
    for root, _, files in os.walk(args.build):
        gcdas += [os.path.join(root, f) for f in files if f.endswith(".gcda")]
    if not gcdas:
        sys.exit("No .gcda files: build with -DVTM_COVERAGE=ON and run the tests first.")
    for gcda in gcdas:
        out = subprocess.run(["gcov", "--json-format", "--stdout", "-o", os.path.dirname(gcda), gcda],
                             capture_output=True, text=True, cwd=os.path.dirname(gcda))
        for doc in out.stdout.splitlines():
            doc = doc.strip()
            if not doc.startswith("{"):
                continue
            for f in json.loads(doc).get("files", []):
                path = os.path.realpath(os.path.join(os.path.dirname(gcda), f["file"])) \
                    if not os.path.isabs(f["file"]) else os.path.realpath(f["file"])
                if not path.startswith(source + os.sep):
                    continue
                rel = os.path.relpath(path, source)
                if not rel.startswith(OURS) or "/tests/" in "/" + rel:
                    continue
                for l in f.get("lines", []):
                    n = l["line_number"]
                    lines[rel][n] = max(lines[rel].get(n, 0), l["count"])
                for fn in f.get("functions", []):
                    key = (rel, fn["start_line"])
                    seen = functions.get(key)
                    count = fn["execution_count"]
                    size = fn["end_line"] - fn["start_line"] + 1
                    name = fn.get("demangled_name", fn["name"])
                    functions[key] = [name, max(count, seen[1]) if seen else count, size]

    def folder(rel):
        return "ui/cpp" if rel.startswith("ui/cpp") else rel.split("/")[0]

    total = collections.Counter()
    run = collections.Counter()
    per_file = []
    for rel, ls in lines.items():
        n, r = len(ls), sum(1 for c in ls.values() if c > 0)
        total[folder(rel)] += n
        run[folder(rel)] += r
        per_file.append((r / n if n else 1, n - r, rel, r, n))
    print("Lines run by the tests:")
    for f in sorted(total):
        print(f"  {f:10} {100 * run[f] / total[f]:5.1f}%  ({run[f]} of {total[f]})")
    print(f"  {'all':10} {100 * sum(run.values()) / sum(total.values()):5.1f}%  "
          f"({sum(run.values())} of {sum(total.values())})")
    print(f"\nThe {args.worst} files with the most lines never run:")
    for share, missing, rel, r, n in sorted(per_file, key=lambda x: -x[1])[:args.worst]:
        print(f"  {rel:45} {100 * share:5.1f}%  {missing} lines never run")
    if args.functions is not None:
        wanted = tuple(args.functions) or OURS
        never = [(size, rel, start, name) for (rel, start), (name, count, size) in functions.items()
                 if count == 0 and rel.startswith(wanted)]
        print(f"\nFunctions never run ({len(never)}), biggest first:")
        for size, rel, start, name in sorted(never, reverse=True):
            print(f"  {size:4} lines  {rel}:{start}  {name[:110]}")


if __name__ == "__main__":
    main()
