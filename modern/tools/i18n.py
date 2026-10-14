#!/usr/bin/env python3
"""Phrases to translate, and which ones a language file is missing.

  python3 tools/i18n.py list            every phrase (JSON list)
  python3 tools/i18n.py missing es      phrases i18n/es.json doesn't have yet
  python3 tools/i18n.py stale es        entries no longer used anywhere
  python3 tools/i18n.py check es        missing phrases, or translations that lose a %1
  python3 tools/i18n.py add es < new.json   add or change translations ({"English": "Español", ...})

Phrases come from qsTr("...") in QML, tr("...") / translate("main", "...") in
C++, and the button and label text of the starter pages (seed/pages).
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

STR = r'"(?:[^"\\\n]|\\.)*"'
# A literal, or literals joined by + (QML) or side by side (C++).
JOINED = rf'{STR}(?:\s*\+?\s*{STR})*'
CALLS = [
    (re.compile(rf'\bqsTr\(\s*({JOINED})'), "qml"),
    (re.compile(rf'(?<![\w.])tr\(\s*({JOINED})'), "cpp"),
    (re.compile(rf'\btranslate\(\s*"[^"]*"\s*,\s*({JOINED})'), "cpp"),
    (re.compile(rf'\bQT_TR_NOOP\(\s*({JOINED})'), "cpp"),
    # Printed tickets (print/tickets.cpp): ctx.tr("...")
    (re.compile(rf'\bctx\.tr\(\s*({JOINED})'), "cpp"),
]


def unescape(lit):
    return json.loads(lit) if lit.startswith('"') else lit


def joined(text):
    return "".join(unescape(m) for m in re.findall(STR, text))


def source_phrases():
    out = []
    for path in sorted(list((ROOT / "ui/qml").rglob("*.qml"))):
        out += [joined(m.group(1)) for rx, kind in CALLS if kind == "qml" for m in rx.finditer(path.read_text())]
    for d in ["app", "net", "ui/cpp", "storage", "print", "core"]:
        for path in sorted((ROOT / d).rglob("*.cpp")) + sorted((ROOT / d).rglob("*.hh")):
            text = path.read_text()
            out += [joined(m.group(1)) for rx, kind in CALLS if kind == "cpp" for m in rx.finditer(text)]
    # Choices in the Manager forms: options({{"value", "Text"}, ...})
    for path in sorted((ROOT / "app").glob("*.cpp")) + sorted((ROOT / "app").glob("*.hh")):
        for block in re.finditer(r'\boptions\(\{(.*?)\}\)', path.read_text(), re.S):
            out += [unescape(m.group(1)) for m in re.finditer(rf'\{{\s*{STR}\s*,\s*({STR})\s*\}}', block.group(1))]
    # The page editor's fields (layout/schema.cpp): labels, sections, hints,
    # choices: the literals that read as words (not paths, ids or colors).
    # Literals side by side (u"..."_s u"..."_s) are one phrase.
    for run in re.finditer(rf'(?:u?{STR}(?:_s)?\s*)+', (ROOT / "layout/schema.cpp").read_text()):
        s = joined(run.group(0))
        if re.search(r"[A-Za-z]", s) and (s[:1].isupper() or s[:1] in "(“" or " " in s) and not s.startswith("#"):
            out.append(s)
    text = (ROOT / "main.cpp").read_text()
    out += [joined(m.group(1)) for rx, kind in CALLS if kind == "cpp" for m in rx.finditer(text)]
    return out


def page_phrases():
    out = []
    for path in sorted((ROOT / "seed/pages").glob("*.json")):
        page = json.loads(path.read_text())
        if page.get("name"):
            out.append(page["name"])   # the page editor's list
        for z in page.get("zones", []):
            if z.get("label"):
                out.append(z["label"])
            for key in ("placeholder", "title"):
                if isinstance(z.get("props", {}).get(key), str):
                    out.append(z["props"][key])
    return out


def report_phrases():
    """Reports are written in English (core/report.cpp, and the reports
    PosService builds); the screen shows each title, column and cell that is
    a phrase on its own in the screen's language."""
    texts = [(ROOT / "core/report.cpp").read_text()]
    for name in ["app/pos_day.cpp", "app/pos_inventory.cpp", "app/pos_dashboard.cpp", "app/pos_forecast.cpp"]:
        src = (ROOT / name).read_text()
        texts += [m.group(0) for m in re.finditer(r'\nReport PosService::\w+\(.*?\n\}\n', src, re.S)]
    out = []
    for text in texts:
        for run in re.finditer(rf'{STR}(?:\s*{STR})*', text):
            p = joined(run.group(0))
            if (re.search(r"[A-Za-z]{2}", p) and p == p.strip() and (p[:1].isupper() or p[:1] in "(*")
                    and "%" not in p and "{" not in p):
                out.append(p)
    return out


def phrases():
    seen, out = set(), []
    for p in source_phrases() + page_phrases() + report_phrases():
        if p.strip() and p not in seen and re.search(r"[A-Za-z]", p):
            seen.add(p)
            out.append(p)
    return out


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "missing"
    if cmd == "list":
        print(json.dumps(phrases(), ensure_ascii=False, indent=1))
        return 0
    lang = sys.argv[2] if len(sys.argv) > 2 else "es"
    have = json.loads((ROOT / f"i18n/{lang}.json").read_text()) if (ROOT / f"i18n/{lang}.json").exists() else {}
    all_ = phrases()
    if cmd == "missing":
        missing = [p for p in all_ if p not in have]
        print(json.dumps(missing, ensure_ascii=False, indent=1))
        print(f"{len(missing)} of {len(all_)} missing", file=sys.stderr)
        return 1 if missing else 0
    if cmd == "add":
        new = json.loads(sys.stdin.read())
        have.update(new)
        (ROOT / f"i18n/{lang}.json").write_text(json.dumps(have, ensure_ascii=False, indent=1, sort_keys=True) + "\n")
        print(f"{lang}: {len(have)} phrases ({len(new)} added or changed)", file=sys.stderr)
        return 0
    if cmd == "check":
        bad = [p for p in all_ if p not in have]
        for key, value in have.items():
            forms = value.items() if isinstance(value, dict) else [("", value)]
            want = sorted(re.findall(r"%(?:\d+|n)", key))
            for form, f in forms:
                got = sorted(re.findall(r"%(?:\d+|n)", f))
                # The singular may say "its item" instead of "its 1 item(s)".
                if form == "one" and "%n" in want and "%n" not in got:
                    got = sorted(got + ["%n"])
                if got != want:
                    bad.append(f"placeholders differ: {key!r} -> {f!r}")
        for b in bad:
            print(b if b.startswith("placeholders") else f"missing: {b!r}")
        print(f"{lang}: {len(all_)} phrases, {len(bad)} problems", file=sys.stderr)
        return 1 if bad else 0
    if cmd == "stale":
        used = set(all_)
        print(json.dumps([k for k in have if k not in used], ensure_ascii=False, indent=1))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
