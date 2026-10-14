#!/usr/bin/env python3
"""CHANGELOG.md: every build of the program, grouped by version, newest first.

  python3 tools/changelog.py

A build is a commit that changed modern/; its number is the commits so far
(as the program counts it, cmake/BuildInfo.cmake) and its version MAJOR.MINOR
from its CMakeLists.txt with that number: 0.9.1283. The program itself shows
its own recent builds in Store Settings -> About; this file keeps them all.
"""
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
GIT = ["git", "-c", "safe.directory=*", "-C", str(ROOT)]
PROJECT = re.compile(r"project\([A-Za-z]+ VERSION (\d+)\.(\d+)")


def git(*args):
    return subprocess.run(GIT + list(args), check=True, capture_output=True, text=True).stdout


def main():
    builds = []
    log = git("log", "--format=%h%x1f%cs%x1f%s", "--", ".")
    for line in log.splitlines():
        commit, day, subject = line.split("\x1f", 2)
        number = int(git("rev-list", "--count", commit).strip())
        try:
            found = PROJECT.search(git("show", f"{commit}:./CMakeLists.txt"))
        except subprocess.CalledProcessError:
            found = None
        minor = f"{found.group(1)}.{found.group(2)}" if found else "0.0"
        subject = re.sub(r"^[a-z]+(\([^)]*\))?:\s*", "", subject)   # "feat(modern): "
        builds.append((minor, number, commit, day, subject))

    out = ["# Changelog", "",
           "Every build of ViewTouch Modern, newest first. The version is MAJOR.MINOR and the build",
           "number (the commits so far): 0.9.1283. Written by `tools/changelog.py`.", ""]
    current = None
    for minor, number, commit, day, subject in builds:
        if minor != current:
            current = minor
            out += ["", f"## {minor}", ""]
        out.append(f"- **{minor}.{number}** ({day}, `{commit}`): {subject}")
    (ROOT / "CHANGELOG.md").write_text("\n".join(out).replace("\n\n\n", "\n\n") + "\n")
    print(f"CHANGELOG.md: {len(builds)} builds")


if __name__ == "__main__":
    main()
