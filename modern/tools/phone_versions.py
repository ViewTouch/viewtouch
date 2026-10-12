#!/usr/bin/env python3
"""Phone versions of the seed pages: their zones shared with the page.

  python3 tools/phone_versions.py

A phone version (seed/pages/*-phone.json, "variantOf") keeps, for each zone of
its page it shows, only where it sits and what it keeps for phones:
{id, shared, rect, z, rotation, own: {"label": ...}}; the program fills in
the rest from the page (layout/zone.hh, Zone::shared), so a button changed on
the page changes on phones too. A zone written out whole is turned into that,
its differences from the page's kept as its own (labels, style keys). The
page's zones it leaves out are listed as hiddenZones (left off on purpose).
Runs again harmlessly; tools/floorplans.py runs it after writing its pages.
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
PLACE = ("id", "rect", "z", "rotation")
NEVER = ("comment", "decor")   # phones never show them: not "left off"


def leaves(o, prefix=""):
    """{"style.normal.fontSize": 34, ...} for a nested object."""
    out = {}
    for k, v in o.items():
        if isinstance(v, dict) and v:
            out.update(leaves(v, prefix + k + "."))
        else:
            out[prefix + k] = v
    return out


def shared(mine, theirs):
    z = {"id": mine["id"], "shared": True, "rect": mine["rect"]}
    for k in ("z", "rotation"):
        if mine.get(k):
            z[k] = mine[k]
    if mine.get("shared"):
        if mine.get("own"):
            z["own"] = mine["own"]
        return z
    own = {}
    if mine.get("label", "") != theirs.get("label", ""):
        own["label"] = mine.get("label", "")
    a, b = leaves(mine.get("style", {}), "style."), leaves(theirs.get("style", {}), "style.")
    for path, v in a.items():
        if b.get(path) != v:
            own[path] = v
    if own:
        z["own"] = own
    return z


def share(pages):
    changed = []
    for path in sorted(pages.glob("*.json")):
        page = json.loads(path.read_text())
        of = page.get("variantOf")
        if not of or not (pages / f"{of}.json").exists():
            continue
        base = {z["id"]: z for z in json.loads((pages / f"{of}.json").read_text())["zones"]}
        zones = [shared(z, base[z["id"]]) if z["id"] in base else z for z in page["zones"]]
        placed = {z["id"] for z in zones}
        hidden = [i for i, z in base.items() if i not in placed and z.get("kind") not in NEVER]
        new = {k: v for k, v in page.items() if k not in ("zones", "hiddenZones")}
        if hidden:
            new["hiddenZones"] = hidden
        new["zones"] = zones
        if new != page:
            path.write_text(json.dumps(new, indent=2, ensure_ascii=False) + "\n")
            changed.append(path.name)
    return changed


if __name__ == "__main__":
    done = share(ROOT / "seed" / "pages")
    print("shared:", ", ".join(done) if done else "nothing to change")
