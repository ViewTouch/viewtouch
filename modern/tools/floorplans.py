#!/usr/bin/env python3
"""The starter floor plans, from one description.

  python3 tools/floorplans.py

Writes seed/pages/tables.json (Main: every table, with walls),
seed/pages/floor-dining.json, floor-bar.json, floor-patio.json, their phone
versions (tables-phone.json, floor-*-phone.json: the plans in a floorPlan
panel above the phone's buttons), and the Main page's ready-made
arrangements (seed/layouts/tables.json). Each plan has
tabs to the others and the usual buttons. A table on two plans is the same
table (its name), so T1 on Main and on Dining Area is one table.

The room is drawn in "floor units": a box FW x FH that each page scales
(evenly) into the space it gives the floor.
"""
import copy
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
FW, FH = 1440, 872
WALL = 16


def table(label, x, y, w, h, shape, seats, rotation=0, chairs=True):
    z = {"id": "table-" + label.lower().replace(" ", "-"), "kind": "table",
         "rect": {"x": x, "y": y, "w": w, "h": h}, "shape": shape, "behavior": "none",
         "label": label, "props": {"seats": seats}}
    if chairs:
        z["props"]["chairs"] = True
    if rotation:
        z["rotation"] = rotation
    return z


_ids = {}


def decor(kind, x, y, w, h, label="", rotation=0, shape="rect", color=None):
    n = _ids.get(kind, 0) + 1
    _ids[kind] = n
    z = {"id": f"{kind}-{n}", "kind": "decor", "rect": {"x": x, "y": y, "w": w, "h": h},
         "shape": shape, "behavior": "none", "props": {"decor": kind}}
    if label:
        z["label"] = label
    if rotation:
        z["rotation"] = rotation
    if color:
        z["props"]["color"] = color
    return z


# --- the whole place, in floor units ------------------------------------------
# Dining room x 0..860, bar x 860..1440 (inside the building, y 0..560);
# the patio below it, outside, behind a fence.
def main_floor():
    _ids.clear()
    z = []
    # The rooms' floors (drawn first: under everything).
    z += [decor("area", 16, 16, 836, 528, "Dining Area"),
          decor("area", 868, 16, 556, 528, "Bar", color="#5b3a1e"),
          decor("area", 0, 584, FW, 288, "Patio", color="#2f5d3a")]
    # Outside walls, with the front door (top) and two doors to the patio.
    z += [decor("wall", 0, 0, 620, WALL), decor("wall", 780, 0, 660, WALL),
          decor("wall", 0, 0, WALL, 560), decor("wall", FW - WALL, 0, WALL, 560),
          decor("wall", 0, 544, 600, WALL), decor("wall", 720, 544, 280, WALL), decor("wall", 1120, 544, 320, WALL)]
    # Windows on the street side.
    z += [decor("window", 100, 0, 180, WALL), decor("window", 360, 0, 180, WALL),
          decor("window", 960, 0, 160, WALL), decor("window", 1180, 0, 160, WALL)]
    # Between the dining room and the bar: a wall with a wide opening.
    z += [decor("wall", 852, WALL, WALL, 180), decor("wall", 852, 360, WALL, 184)]
    # Doors: in from the street; out to the patio (dining room, bar).
    z += [decor("entrance", 640, 24, 120, 96, "Entrance"),
          decor("door", 600, 424, 120, 120), decor("door", 1000, 424, 120, 120)]
    # Dining room.
    # (Below the room's name, in its top left corner.)
    z += [table("T1", 40, 76, 130, 130, "circle", 2),
          table("T2", 200, 76, 130, 130, "circle", 2),
          table("T3", 370, 76, 210, 130, "rect", 4),
          table("T4", 40, 230, 210, 136, "rect", 4),
          table("T6", 290, 226, 144, 144, "octagon", 4),
          table("T7", 470, 226, 144, 144, "octagon", 4, rotation=22),
          table("T5", 40, 388, 300, 144, "rect", 6),
          decor("booth", 680, 220, 150, 60, rotation=90),
          decor("plant", 470, 430, 80, 80)]
    # The bar: the counter along the far wall, stools in front, two high-tops.
    z += [decor("counter", 1300, 60, 96, 420),
          table("Bar 1", 1180, 84, 108, 108, "circle", 1, chairs=False),
          table("Bar 2", 1180, 216, 108, 108, "circle", 1, chairs=False),
          table("Bar 3", 1180, 348, 108, 108, "circle", 1, chairs=False),
          table("H1", 910, 80, 130, 130, "circle", 2),
          table("H2", 910, 250, 130, 130, "circle", 2),
          decor("pillar", 1100, 270, 48, 48, shape="circle")]
    # The patio: square tables turned to diamonds, plants, a rail around it.
    z += [decor("fence", 0, FH - WALL, FW, WALL), decor("fence", 0, 584, WALL, FH - 584),
          decor("fence", FW - WALL, 584, WALL, FH - 584)]
    for i, x in enumerate([130, 470, 810, 1150]):
        z.append(table(f"P{i + 1}", x, 650, 150, 150, "rect", 4, rotation=45))
    z += [decor("plant", 340, 690, 70, 70), decor("plant", 680, 690, 70, 70), decor("plant", 1020, 690, 70, 70)]
    return z


# The Patio's own plan: the four tables bigger, around the building's back.
def patio_floor():
    _ids.clear()
    z = [decor("area", 0, 120, FW, FH - 120, "Patio", color="#2f5d3a"),
         decor("wall", 0, 0, 600, WALL * 2), decor("wall", 720, 0, 280, WALL * 2), decor("wall", 1120, 0, 320, WALL * 2),
         decor("door", 600, 32, 120, 120, rotation=180), decor("door", 1000, 32, 120, 120, rotation=180),
         decor("fence", 0, FH - WALL, FW, WALL), decor("fence", 0, 120, WALL, FH - 120),
         decor("fence", FW - WALL, 120, WALL, FH - 120)]
    for i, (x, y) in enumerate([(220, 230), (900, 230), (220, 560), (900, 560)]):
        z.append(table(f"P{i + 1}", x, y, 230, 230, "rect", 4, rotation=45))
    z += [decor("plant", 640, 300, 120, 120), decor("plant", 640, 620, 120, 120),
          decor("plant", 40, 160, 90, 90), decor("plant", FW - 130, 160, 90, 90),
          decor("plant", 40, FH - 120, 90, 90), decor("plant", FW - 130, FH - 120, 90, 90)]
    return z


def inside(z, region):
    x, y, w, h = region
    r = z["rect"]
    return r["x"] >= x - 1 and r["y"] >= y - 1 and r["x"] + r["w"] <= x + w + 1 and r["y"] + r["h"] <= y + h + 1


def clip(z, region):
    """A wall or window partly in the region: the part inside it (None: none)."""
    x, y, w, h = region
    r = z["rect"]
    x1, y1 = max(r["x"], x), max(r["y"], y)
    x2, y2 = min(r["x"] + r["w"], x + w), min(r["y"] + r["h"], y + h)
    if x2 - x1 < 8 or y2 - y1 < 8:
        return None
    c = copy.deepcopy(z)
    c["rect"] = {"x": x1, "y": y1, "w": x2 - x1, "h": y2 - y1}
    return c


def part(zones, region, keep_areas):
    """The pieces of Main inside `region` (walls cut to it), in its own units."""
    out = []
    for z in zones:
        if z["kind"] == "decor" and z["props"]["decor"] == "area":
            if z.get("label") in keep_areas:
                c = clip(z, region)
                if c:
                    out.append(c)
            continue
        if inside(z, region):
            out.append(copy.deepcopy(z))
        elif z["kind"] == "decor" and z["props"]["decor"] in ("wall", "window", "fence"):
            c = clip(z, region)
            if c:
                out.append(c)
    x, y = region[0], region[1]
    for z in out:
        z["rect"]["x"] -= x
        z["rect"]["y"] -= y
    return out, region[2], region[3]


def fit(zones, w, h, box):
    """Zones drawn in a w x h space, scaled evenly into box (x, y, bw, bh), centered."""
    bx, by, bw, bh = box
    s = min(bw / w, bh / h)
    ox, oy = bx + (bw - w * s) / 2, by + (bh - h * s) / 2
    out = []
    for z in zones:
        c = copy.deepcopy(z)
        r = c["rect"]
        c["rect"] = {"x": round(ox + r["x"] * s), "y": round(oy + r["y"] * s),
                     "w": max(8, round(r["w"] * s)), "h": max(8, round(r["h"] * s))}
        out.append(c)
    return out


# --- the buttons around the floor ------------------------------------------------
PLANS = [("tables", "Main"), ("floor-dining", "Dining Area"), ("floor-bar", "Bar"), ("floor-patio", "Patio")]


def button(id, label, rect, actions, fill=None):
    z = {"id": id, "kind": "button", "rect": dict(zip("xywh", rect)), "shape": "rect", "behavior": "blink",
         "label": label, "actions": actions}
    if fill:
        z["style"] = {"normal": {"fill": fill}}
    return z


def tabs(box):
    """Main | Dining Area | Bar | Patio across the top of the floor; this plan's lit."""
    x, y, w, h = box
    gap = 8
    tw = (w - gap * (len(PLANS) - 1)) / len(PLANS)
    return [button("plan-" + pid.replace("floor-", "") if pid != "tables" else "plan-main", name,
                   (round(x + i * (tw + gap)), y, round(tw), h),
                   [{"type": "jump", "mode": "replace", "page": pid}], "#2a313d")
            for i, (pid, name) in enumerate(PLANS)]


def chrome(r):
    """The usual buttons, at the rects in r (id -> (x, y, w, h))."""
    go = lambda cmd: [{"type": "command", "name": cmd}, {"type": "jump", "mode": "index"}]
    out = [
        button("quick", "Quick Order", r["quick"], go("startQuick"), "#1f8a4c"),
        button("takeout", "Takeout", r["takeout"], go("startTakeout")),
        button("delivery", "Delivery", r["delivery"], go("startDelivery")),
        button("checks", "Open Checks", r["checks"], [{"type": "jump", "mode": "push", "role": "checkList"}]),
        button("deliveries", "Deliveries", r["deliveries"], [{"type": "jump", "mode": "push", "page": "deliveries"}], "#2b62b0"),
        button("host", "Host Stand", r["host"], [{"type": "jump", "mode": "push", "page": "seating"}], "#1f6f73"),
        button("tabs", "Bar Tabs", r["tabs"], [{"type": "jump", "mode": "push", "page": "tabs"}], "#a86a12"),
        {"id": "status", "kind": "logoutPanel", "rect": dict(zip("xywh", r["status"])), "shape": "rect", "behavior": "none"},
        button("switch", "Switch User", r["switch"], [{"type": "command", "name": "logout"}], "#2b62b0"),
        button("manager", "Manager", r["manager"], [{"type": "jump", "mode": "push", "role": "manager"}]),
        button("logout", "Log Out", r["logout"], [{"type": "jump", "mode": "push", "role": "logout"}], "#b83232"),
    ]
    return out


# Each arrangement: where the buttons go, the tabs, and the floor's box.
CLASSIC = {
    "quick": (1472, 16, 432, 150), "takeout": (1472, 182, 208, 150), "delivery": (1696, 182, 208, 150),
    "checks": (1472, 348, 208, 150), "deliveries": (1696, 348, 208, 150), "status": (1472, 514, 432, 140),
    "switch": (1472, 664, 432, 74), "manager": (1472, 748, 432, 150), "logout": (1472, 914, 432, 150),
    "tabs": (740, 976, 354, 88), "host": (1102, 976, 354, 88),
    "planTabs": (16, 16, 1440, 80), "floor": (16, 104, 1440, 864),
}
ARRANGEMENTS = [
    ("classic", "Classic", "The floor plan, the buttons down the right side.", CLASSIC, "wood"),
    ("buttons-left", "Buttons left", "The buttons down the left side, the floor to their right.", {
        "quick": (16, 16, 432, 150), "takeout": (16, 182, 208, 150), "delivery": (240, 182, 208, 150),
        "checks": (16, 348, 208, 150), "deliveries": (240, 348, 208, 150), "status": (16, 514, 432, 140),
        "switch": (16, 664, 432, 74), "manager": (16, 748, 432, 150), "logout": (16, 914, 432, 150),
        "tabs": (1188, 976, 354, 88), "host": (1550, 976, 354, 88),
        "planTabs": (464, 16, 1440, 80), "floor": (464, 104, 1440, 864)}, "wood"),
    ("buttons-bottom", "Buttons below", "The floor on top, big buttons across the bottom.", {
        "quick": (16, 716, 299, 348), "takeout": (331, 716, 299, 348), "delivery": (646, 716, 299, 348),
        "tabs": (961, 716, 299, 348), "host": (1276, 716, 299, 348), "logout": (1591, 716, 299, 348),
        "status": (1200, 16, 704, 220), "switch": (1200, 246, 704, 76), "checks": (1200, 332, 348, 170),
        "deliveries": (1556, 332, 348, 170), "manager": (1200, 518, 704, 170),
        "planTabs": (16, 16, 1168, 72), "floor": (16, 96, 1168, 604)}, "wood"),
    ("counter", "Counter first", "Quick Order, Takeout and Delivery big; a smaller floor.", {
        "quick": (1100, 16, 804, 300), "takeout": (1100, 332, 260, 220), "delivery": (1372, 332, 260, 220),
        "deliveries": (1644, 332, 260, 220), "checks": (1100, 568, 396, 160), "tabs": (1508, 568, 396, 160),
        "status": (1100, 744, 520, 150), "switch": (1632, 744, 272, 150), "manager": (1100, 910, 396, 154),
        "logout": (1508, 910, 396, 154), "host": (640, 910, 444, 154),
        "planTabs": (16, 16, 1068, 72), "floor": (16, 96, 1068, 800)}, "wood"),
    ("plain", "Plain floor", "The classic arrangement on a plain dark floor (no wood).", CLASSIC, "plain"),
]
BACKGROUNDS = {"wood": {"texture": "woodfloor", "fill": "#3b2a1a"}, "plain": {"fill": "#1d2128"}}


def page_zones(floor, w, h, r):
    return fit(floor, w, h, r["floor"]) + tabs(r["planTabs"]) + chrome(r)


def page(pid, name, zones, extra=None):
    p = {"schemaVersion": 1, "id": pid, "name": name, "kind": "tables", "canvas": {"w": 1920, "h": 1080},
         "grid": 8}
    p.update(extra or {})
    p["background"] = BACKGROUNDS["wood"]
    p["zones"] = zones
    return p


def write(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


# --- phones -------------------------------------------------------------------
# The plans in one panel (tabs, the tables where they stand, two fingers to
# zoom) above the phone's buttons; each room's phone page opens on that room.
def phone_pages(pages):
    path = pages / "tables-phone.json"
    phone = json.loads(path.read_text())
    buttons = [z for z in phone["zones"] if z["kind"] == "button" and z["id"] != "bar-tabs"]
    buttons.append(button("bar-tabs", "Bar Tabs", (722, 2052, 333, 190),
                          [{"type": "jump", "mode": "push", "page": "tabs"}], "#a86a12"))

    def make(pid, name, of, plan):
        panel = {"id": "tables", "kind": "floorPlan", "rect": {"x": 24, "y": 24, "w": 1032, "h": 1600},
                 "shape": "rect", "behavior": "none", "props": {"tabs": True}}
        if plan:
            panel["props"]["plan"] = plan
        return {"schemaVersion": 1, "id": pid, "name": name, "kind": "tables", "canvas": {"w": 1080, "h": 2280},
                "grid": 8, "variantOf": of, "formFactor": "phone", "background": phone.get("background", {}),
                "zones": [panel] + copy.deepcopy(buttons)}

    write(path, make("tables-phone", "Tables (phone)", "tables", ""))
    for pid, name in PLANS[1:]:
        write(pages / f"{pid}-phone.json", make(f"{pid}-phone", f"{name} (phone)", pid, pid))


def main():
    pages = ROOT / "seed" / "pages"
    phone_pages(pages)
    floor = main_floor()
    write(pages / "tables.json", page("tables", "Main", page_zones(floor, FW, FH, CLASSIC), {"role": "tables"}))
    dining, dw, dh = part(floor, (0, 0, 868, 560), {"Dining Area"})
    write(pages / "floor-dining.json", page("floor-dining", "Dining Area", page_zones(dining, dw, dh, CLASSIC)))
    bar, bw, bh = part(floor, (852, 0, FW - 852, 560), {"Bar"})
    write(pages / "floor-bar.json", page("floor-bar", "Bar", page_zones(bar, bw, bh, CLASSIC)))
    write(pages / "floor-patio.json", page("floor-patio", "Patio", page_zones(patio_floor(), FW, FH, CLASSIC)))

    variants = []
    for vid, name, about, r, bg in ARRANGEMENTS:
        variants.append({"id": vid, "name": name, "description": about, "background": BACKGROUNDS[bg],
                         "zones": page_zones(floor, FW, FH, r)})
    write(ROOT / "seed" / "layouts" / "tables.json",
          {"schemaVersion": 1, "page": "tables", "role": "tables", "variants": variants})


if __name__ == "__main__":
    main()
