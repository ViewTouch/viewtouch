#!/usr/bin/env python3
"""VT Ai's benchmark: the menus in tests/vtai-bench through a VT Ai server
(llama.cpp's llama-server), scored against what each should become.

  python3 tools/vtai_bench.py [--url http://127.0.0.1:7720] [--only 03]

For each menu: items found (of those expected), prices right (to the cent,
"none" when the menu gives none), sizes right, choice groups right (each
expected group's options found in one of the answer's groups), items made
up (not on the menu), and the seconds it took. Uses seed/vtai/menu.json,
the instructions and answer shape the app sends.
"""
import argparse
import difflib
import json
import pathlib
import re
import sys
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent


def norm(name):
    s = re.sub(r"[^a-z0-9áéíóúñü ]+", " ", (name or "").lower())
    words = [w[:-1] if len(w) > 3 and w.endswith("s") else w for w in s.split()]
    return " ".join(words)


def same(a, b):
    a, b = norm(a), norm(b)
    if not a or not b:
        return False
    if a == b or set(a.split()) <= set(b.split()) or set(b.split()) <= set(a.split()):
        return True
    return difflib.SequenceMatcher(None, a, b).ratio() >= 0.82


def price_same(want, got):
    if want is None:
        return got is None or got == 0
    return isinstance(got, (int, float)) and abs(want - got) < 0.005


def parse_lines(text):
    """The line format (seed/vtai/menu-lines.json) as {items, groups}."""
    def money(t):
        t = t.strip().lstrip("$")
        try:
            return round(float(t), 2)
        except ValueError:
            return None
    items, groups, category = [], [], ""
    for raw in text.splitlines():
        line = raw.strip()
        if len(line) < 3 or line[1] != " ":
            continue
        kind, rest = line[0], line[2:]
        cells = [c.strip() for c in rest.split("|")]
        if kind == "C":
            category = rest.strip()
        elif kind == "I" and cells[0]:
            sizes = []
            for part in (cells[2] if len(cells) > 2 else "").split(";"):
                if "=" in part:
                    n, _, v = part.rpartition("=")
                    if n.strip() and money(v) is not None:
                        sizes.append({"name": n.strip(), "price": money(v)})
            names = [g.strip() for g in (cells[3] if len(cells) > 3 else "").split(";") if g.strip()]
            items.append({"name": cells[0], "price": money(cells[1]) if len(cells) > 1 else None,
                          "category": category, "sizes": sizes, "groups": names})
        elif kind == "G" and cells[0]:
            options = []
            for part in (cells[1] if len(cells) > 1 else "").split(";"):
                if "=" in part:
                    n, _, v = part.rpartition("=")
                    if n.strip():
                        options.append({"name": n.strip(), "price": money(v) or 0})
            groups.append({"name": cells[0], "options": options})
    return {"items": items, "groups": groups}


def ask_lines(url, menu_text, spec, timeout=600):
    ex = spec["example"]
    body = {
        "messages": [{"role": "system", "content": spec["system"]},
                     {"role": "user", "content": ex["menu"]}, {"role": "assistant", "content": ex["lines"]},
                     {"role": "user", "content": menu_text}],
        "temperature": 0,
        "max_tokens": 2000,
        "grammar": spec["grammar"],
        "stop": ["\nEND"],
    }
    req = urllib.request.Request(url.rstrip("/") + "/v1/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    t = time.time()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        reply = json.load(r)
    took = time.time() - t
    return parse_lines(reply["choices"][0]["message"]["content"]), took, reply.get("usage", {})


def ask(url, menu_text, spec, timeout=600):
    body = {
        "messages": [{"role": "system", "content": spec["system"]}, {"role": "user", "content": menu_text}],
        "temperature": 0,
        "max_tokens": 3000,
        "response_format": {"type": "json_object", "schema": spec["schema"]},
    }
    req = urllib.request.Request(url.rstrip("/") + "/v1/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    t = time.time()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        reply = json.load(r)
    took = time.time() - t
    text = reply["choices"][0]["message"]["content"]
    usage = reply.get("usage", {})
    return json.loads(text), took, usage


def score(want, got):
    items = got.get("items", [])
    found = prices = sizes = sizes_wanted = 0
    used = set()
    for w in want["items"]:
        match = next((i for i, g in enumerate(items) if i not in used and same(w["name"], g.get("name"))), None)
        if match is None:
            continue
        used.add(match)
        found += 1
        g = items[match]
        if price_same(w.get("price"), g.get("price")) or (
                w.get("sizes") and price_same(min(s["price"] for s in w["sizes"]), g.get("price"))):
            prices += 1
        if w.get("sizes"):
            sizes_wanted += 1
            gs = g.get("sizes") or []
            if len(gs) == len(w["sizes"]) and sorted(round(s["price"], 2) for s in w["sizes"]) == sorted(
                    round(s.get("price") or 0, 2) for s in gs):
                sizes += 1
    made_up = len(items) - len(used)
    groups = 0
    for wg in want.get("groups", []):
        for gg in got.get("groups", []):
            opts = gg.get("options", [])
            hits = sum(1 for wo in wg["options"]
                       if any(same(wo["name"], o.get("name")) and price_same(wo["price"], o.get("price")) for o in opts))
            if hits >= max(1, len(wg["options"]) - 1):   # all, or all but one
                groups += 1
                break
    return {"items": len(want["items"]), "found": found, "prices": prices, "sizes": sizes,
            "sizesWanted": sizes_wanted, "groups": groups, "groupsWanted": len(want.get("groups", [])),
            "madeUp": made_up}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:7720")
    ap.add_argument("--only", default="")
    ap.add_argument("--show", action="store_true", help="print each answer")
    ap.add_argument("--format", default="json", choices=["json", "lines"], help="the answer's shape")
    a = ap.parse_args()
    spec = json.loads((ROOT / ("seed/vtai/menu-lines.json" if a.format == "lines" else "seed/vtai/menu.json")).read_text())
    asker = ask_lines if a.format == "lines" else ask
    total = {"items": 0, "found": 0, "prices": 0, "sizes": 0, "sizesWanted": 0, "groups": 0, "groupsWanted": 0,
             "madeUp": 0, "seconds": 0.0, "tokens": 0}
    for path in sorted((ROOT / "tests/vtai-bench").glob("*.txt")):
        if a.only and not path.name.startswith(a.only):
            continue
        want = json.loads(path.with_suffix(".json").read_text())
        try:
            got, took, usage = asker(a.url, path.read_text(), spec)
        except Exception as e:   # a broken answer counts as nothing found
            print(f"{path.stem:20} FAILED: {e}")
            total["items"] += len(want["items"])
            continue
        s = score(want, got)
        for k, v in s.items():
            total[k] += v
        total["seconds"] += took
        total["tokens"] += usage.get("completion_tokens", 0)
        print(f"{path.stem:20} items {s['found']}/{s['items']}  prices {s['prices']}  sizes {s['sizes']}/{s['sizesWanted']}"
              f"  groups {s['groups']}/{s['groupsWanted']}  made up {s['madeUp']}  {took:5.1f}s")
        if a.show:
            print(json.dumps(got, indent=1, ensure_ascii=False))
    t = total
    print(f"{'ALL':20} items {t['found']}/{t['items']}  prices {t['prices']}/{t['items']}  sizes {t['sizes']}/{t['sizesWanted']}"
          f"  groups {t['groups']}/{t['groupsWanted']}  made up {t['madeUp']}  {t['seconds']:.0f}s"
          f"  ({t['tokens'] / max(t['seconds'], 0.01):.1f} tokens/s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
