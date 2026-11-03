#!/usr/bin/env python3
"""Mutation testing: do the tests notice a mistake?

    tools/mutate.py [--files core/money.hh core/core.cpp ...] [--per-file 40]
                    [--tests "~[ui]"] [--seed 1] [--work DIR]

Copies the source to a work folder, builds the tests there once, then makes
one small change at a time in the code that handles money (a + for a -, a <
for a <=, == for !=, && for ||, true for false), rebuilds and runs the tests.
A change no test fails on ("survived") is a line the tests don't really
check: each is printed, with what was changed. Changes that don't compile
don't count. Slow: about a minute a change; --per-file picks that many at
random (the same ones for the same --seed).
"""
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_FILES = ["core/money.hh", "core/core.cpp", "core/tax.hh", "core/report.cpp", "app/pos_day.cpp",
                 "core/inventory.hh"]
# (pattern, replacement, name): applied to one place in one line at a time.
OPERATORS = [
    (r" \+ ", " - ", "+ to -"),
    (r" - ", " + ", "- to +"),
    (r" < ", " <= ", "< to <="),
    (r" <= ", " < ", "<= to <"),
    (r" > ", " >= ", "> to >="),
    (r" >= ", " > ", ">= to >"),
    (r" == ", " != ", "== to !="),
    (r" != ", " == ", "!= to =="),
    (r" && ", " || ", "&& to ||"),
    (r" \|\| ", " && ", "|| to &&"),
    (r"\breturn true;", "return false;", "true to false"),
    (r"\breturn false;", "return true;", "false to true"),
    (r"\+= ", "-= ", "+= to -="),
    (r"-= ", "+= ", "-= to +="),
]


def code_part(line):
    """The line without a // comment, or None for lines not to touch."""
    s = line.strip()
    if not s or s.startswith(("//", "#", "*", "/*")) or "static_assert" in s or "qCDebug" in s or "qDebug" in s:
        return None
    return line.split("//")[0]


def mutants(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    out = []
    for i, line in enumerate(lines):
        code = code_part(line)
        if code is None or '"' in code:   # leave text alone
            continue
        for pattern, repl, name in OPERATORS:
            for m in re.finditer(pattern, code):
                out.append((i, m.start(), m.end(), repl, name))
    return lines, out


def run(cmd, cwd, timeout):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return p.returncode
    except subprocess.TimeoutExpired:
        return "timeout"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--files", nargs="*", default=DEFAULT_FILES)
    ap.add_argument("--per-file", type=int, default=40)
    ap.add_argument("--tests", default="~[ui]~[program]~[soak]~[stress]~[net]~[timing]")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--work", default=None)
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args()

    work = args.work or os.path.join(os.environ.get("TMPDIR", "/tmp"), "vtm-mutate")
    src = os.path.join(work, "src")
    build = os.path.join(work, "build")
    if not os.path.isdir(src):
        print("Copying the source...", flush=True)
        repo = os.path.dirname(HERE)
        os.makedirs(work, exist_ok=True)
        def skip(folder, names):   # the history and build folders, not build_info.hh
            return [n for n in names if n == ".git" or (os.path.isdir(os.path.join(folder, n))
                                                         and (n.startswith("build") or n == "dist"))]
        shutil.copytree(repo, os.path.join(work, "repo"), symlinks=True, ignore=skip)
        os.symlink(os.path.join(work, "repo", "modern"), src)
    # The files to change, fresh from the source: a run stopped halfway may
    # have left a change in the copy.
    for rel in args.files:
        if os.path.exists(os.path.join(HERE, rel)):
            shutil.copy2(os.path.join(HERE, rel), os.path.join(src, rel))
    if not os.path.isdir(build):
        subprocess.run(["cmake", "-S", src, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("Building once...", flush=True)
    subprocess.run(["cmake", "--build", build, "--target", "vtm_tests", "-j", str(args.jobs)], check=True,
                   stdout=subprocess.DEVNULL)
    tests = os.path.join(build, "tests", "vtm_tests")
    env_cmd = ["env", "QT_QPA_PLATFORM=offscreen", tests, args.tests, "--order", "decl", "-a", "-r", "compact"]
    start = time.time()
    if run(env_cmd, build, 3600) != 0:
        sys.exit("The tests fail before any change: fix that first.")
    baseline = time.time() - start
    print(f"The tests take {baseline:.0f} s unchanged.", flush=True)

    rng = random.Random(args.seed)
    totals = {"killed": 0, "survived": 0, "no build": 0}
    survivors = []
    for rel in args.files:
        path = os.path.join(src, rel)
        if not os.path.exists(path):
            print(f"{rel}: no such file", flush=True)
            continue
        lines, all_mutants = mutants(path)
        chosen = rng.sample(all_mutants, min(args.per_file, len(all_mutants)))
        print(f"\n{rel}: {len(chosen)} of {len(all_mutants)} possible changes", flush=True)
        original = "\n".join(lines)
        for n, (i, a, b, repl, name) in enumerate(chosen, 1):
            changed = list(lines)
            code = changed[i]
            changed[i] = code[:a] + repl + code[b:]
            with open(path, "w", encoding="utf-8") as f:
                f.write("\n".join(changed))
            try:
                built = run(["cmake", "--build", build, "--target", "vtm_tests", "-j", str(args.jobs)], build, 1800)
                if built != 0:
                    result = "no build"
                else:
                    r = run(env_cmd, build, max(120, int(baseline * 3)))
                    result = "survived" if r == 0 else "killed"
            finally:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(original)
            totals[result] += 1
            mark = {"killed": ".", "survived": "S", "no build": "x"}[result]
            print(mark, end="", flush=True)
            if result == "survived":
                survivors.append(f"{rel}:{i + 1}: {name}: {lines[i].strip()}")
        print(flush=True)
    # Put the build back as the source is.
    run(["cmake", "--build", build, "--target", "vtm_tests", "-j", str(args.jobs)], build, 1800)
    counted = totals["killed"] + totals["survived"]
    print(f"\nKilled {totals['killed']} of {counted} changes "
          f"({100 * totals['killed'] / max(1, counted):.0f}%); {totals['no build']} didn't build.")
    if survivors:
        print("Survived (no test noticed):")
        for s in survivors:
            print("  " + s)


if __name__ == "__main__":
    main()
