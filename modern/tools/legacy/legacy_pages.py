#!/usr/bin/env python3
# The legacy ViewTouch's pages as Modern pages, for the Legacy style: every
# system page in vt_data, the starter menu pages (zone_db.dat) and the dining
# floor (tables.dat), each zone where legacy has it, looking as legacy drew it
# (frames, textures, colors, fonts, shapes), doing what Modern does for it.
#
#   python3 -I tools/legacy/legacy_pages.py [LEGACY-DATA-DIR]
#
# LEGACY-DATA-DIR (default tools/legacy/data) holds the four files the legacy
# program downloads from www.viewtouch.com (vt_data, zone_db.dat, tables.dat,
# menu.dat). Writes seed/legacy/*.json and seed/legacy/NOT-IN-MODERN.md: the
# legacy buttons with nothing behind them in Modern yet, page by page.
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vtdata  # noqa: E402

MODERN = os.path.dirname(os.path.dirname(HERE))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "data")
OUT = os.path.join(MODERN, "seed", "legacy")

# --- legacy's look (term/term_view.hh, src/core/image_data.cc) -------------------------
COLORS = ["#000000", "#ffffff", "#eb0000", "#008000", "#0000e6", "#ffff00", "#844c26", "#ff5400", "#6400c8",
          "#0084a8", "#606060", "#c03088", "#ff3800", "#008060", "#502d19", "#a50000", "#005a00", "#000091",
          "#005c82", "#a0206e", "#006248"]
COLOR_DEFAULT, COLOR_PAGE_DEFAULT, COLOR_CLEAR = 255, 254, 253
TEXTURES = ["sand", "litsand", "darksand", "litewood", "wood", "darkwood", "grayparchment", "graymarble",
            "greenmarble", "parchment", "pearl", "canvas", "tanparchment", "smoke", "leather", "blueparchment",
            "gradient", "gradientbrown", "black", "greysand", "whitemesh", "carbonfiber", "whitetexture",
            "darkorangetexture", "yellowtexture", "greentexture", "orangetexture", "bluetexture", "pooltable",
            "test", "diamondleather", "bread", "lava", "darkmarble"]
IMAGE_CLEAR, IMAGE_DEFAULT = 253, 255
# FONT_* -> (family, pixels on the 1920x1080 page, bold): legacy's Xft fonts at 96 dpi.
SERIF, PLAIN = "DejaVu Serif", "Liberation Serif"
FONTS = {1: (SERIF, 37, False), 2: (SERIF, 37, True), 4: (SERIF, 16, False), 5: (SERIF, 19, False),
         6: (SERIF, 24, False), 7: (SERIF, 16, True), 8: (SERIF, 19, True), 9: (SERIF, 24, True),
         10: (SERIF, 13, False), 11: (SERIF, 13, True), 12: (SERIF, 15, False), 13: (SERIF, 15, True),
         14: (PLAIN, 15, False), 15: (PLAIN, 15, True), 16: (PLAIN, 16, False), 17: (PLAIN, 16, True)}
SHAPES = {1: "rect", 2: "diamond", 3: "circle", 4: "hexagon", 5: "octagon", 6: "diamond"}
BEHAVIORS = {0: "none", 1: "toggle", 2: "blink", 3: "toggle", 4: "double", 5: "passthrough"}
SIZES = {1: (640, 480), 2: (768, 1024), 3: (800, 480), 4: (800, 600), 5: (1024, 600), 6: (1024, 768),
         7: (1280, 800), 8: (1280, 1024), 9: (1366, 768), 10: (1440, 900), 11: (1600, 900), 12: (1600, 1200),
         13: (1680, 1050), 14: (1920, 1080), 15: (1920, 1200), 16: (2560, 1440), 17: (2560, 1600)}


def frame_of(code):
    """ZF_* -> Modern's frame (and its width); None: inherited."""
    if code in (0, 1):
        return None
    if code == 2:
        return ("hidden", 0)
    if code == 3:
        return ("none", 0)
    if 10 <= code <= 13:
        return ("raised", 3)
    if 20 <= code <= 23:
        return ("inset", 3)
    if 30 <= code <= 33:
        return ("raised", 6)
    if 40 <= code <= 51:
        return ("border", 6)
    return ("raised", 3)


# --- what each page is in Modern -------------------------------------------------------
# Legacy pages standing in for a Modern page: the Legacy style shows them in its place.
STANDS_IN = {-1: "login", -20: "settle", -5: "guest-count", -10: "manager", -72: "time-clock", -7: "logout",
             -24: "check-list", -901: "kitchen", -921: "bar-display", -99: "order-template"}
# (Legacy's dining room, page 50, is its sample store's: in Legacy a store keeps its own
# tables, and that page is kept as a picture of legacy's.)
KINDS = {0: "custom", 1: "tables", 2: "index", 3: "items", 4: "modifier", 5: "modifier", 6: "modifier",
         7: "custom", 8: "library", 9: "items", 10: "tables", 12: "custom", 13: "kitchen", 14: "kitchen",
         15: "kitchen", 16: "kitchen", 17: "modifier", 18: "template"}
# Legacy's menu area on its order screen (-99: the check at the left, keys along the bottom).
ORDER_CONTENT = {"x": 460, "y": 8, "w": 1452, "h": 944}


def page_id(lid):
    return STANDS_IN.get(lid) or (f"legacy-n{-lid}" if lid < 0 else f"legacy-{lid}")


def own_id(lid):
    """The legacy page's own id (a version's id when it stands in for a Modern page)."""
    return f"legacy-n{-lid}" if lid < 0 else f"legacy-{lid}"


# --- what each zone does in Modern ------------------------------------------------------
JUMPS = {1: "push", 2: "replace", 3: "back", 4: "home", 5: "back", 6: "index", 7: "push"}
MESSAGES = {"logout": "logout", "clockin": "clockIn", "clockout": "clockOut", "open drawer": "noSale",
            "takeout": "startTakeout", "starttakeout": "startTakeout", "quicktogo": "startTakeout",
            "quickdinein": "startQuick", "faststart": "startQuick", "start": "startQuick", "taxexempt": "taxExempt",
            "final": "sendOrder", "finalize": "closeCheck", "print": "printReceipt", "payout": "payout",
            "backspace": "lineLess", "cancel": "releaseCheck"}
# Keys: legacy sends a button's name when it has no message ("1", "Clear"), to
# the page's PIN or amount entry (button_zone.cc SendandJump).
KEYS = {str(d): str(d) for d in range(10)} | {"00": "00", "clear": "clear", "enter": "enter", "backspace": "back"}
QUALIFIERS = {1: "no", 2: "side", 4: "sub", 8: "lite", 32: "extra"}
# The words legacy draws on a qualifier key (main/business/sales.hh, bit by bit).
QUALIFIER_NAMES = ["No", "Side", "Sub", "Lite", "Only", "Extra", "Double", "Dry", "Plain", "Toasted", "Untoasted",
                   "Crispy", "Hard", "Soft", "Grilled", "Left", "Right", "Whole", "Cut/2", "Cut/3", "Cut/4", "Easy",
                   "Add", "Senior Share"]
TENDERS = {0: "cash", 2: "credit", 11: "credit", 12: "credit", 4: "gift", 6: "house", 8: "discount", 5: "comp",
           10: "staff-meal"}
WIDGETS = {"ZONE_LOGIN": "loginPad", "ZONE_ORDER_ENTRY": "orderList", "ZONE_PAYMENT_ENTRY": "paymentPanel",
           "ZONE_CHECK_LIST": "checkList", "ZONE_GUEST_COUNT": "guestCount", "ZONE_END_DAY": "endOfDay",
           "ZONE_SCHEDULE": "schedule", "ZONE_CUSTOMER_INFO": "customerInfo", "ZONE_DRAWER_MANAGE": "drawerPanel",
           "ZONE_LOGOUT": "logoutPanel", "ZONE_LABOR": "timeClock", "ZONE_INVENTORY": "stockCount",
           "ZONE_SEARCH": "checkSearch", "ZONE_SPLIT_CHECK": "splitCheck", "ZONE_KITCHEN_VIDEO": "kitchenDisplay"}
# What each legacy zone kind was for, said when touched (not in Modern yet).
NAMES = {"ZONE_REPORT": "this report", "ZONE_SWITCH": "this setting switch", "ZONE_TOGGLE": "this toggle",
         "ZONE_SETTINGS": "these settings", "ZONE_TAX_SETTINGS": "these tax settings",
         "ZONE_KILL_SYSTEM": "quitting ViewTouch from here", "ZONE_CLEAR_SYSTEM": "clearing the system here",
         "ZONE_DEVELOPER": "the developer's settings", "ZONE_HARDWARE": "these hardware settings"}

missing = {}   # page name -> [(label, what)]


def note_missing(page, label, what):
    missing.setdefault(page, []).append((label.replace("\n", " ") or "(no label)", what))


def label_of(name):
    # A toggle's words for each state: "!Zero Exclusion OFF!Zero Exclusion ON" (the first shows).
    if name.startswith("!") and name.count("!") >= 2:
        name = name.split("!")[1]
    s = name.replace("{termname}", "{terminal}").replace("\\", "\n")
    return s.strip()


def actions_of(z, page_name, label, page_ids):
    kind = z["kind"]
    acts = []
    msg = (z.get("Message") or "").strip()
    if kind == "ZONE_QUALIFIER":
        q = QUALIFIERS.get(z.get("QualifierType", 0))
        if q:
            return [{"type": "qualifier", "qualifier": q}]
        note_missing(page_name, label, f"the qualifier {label!r}")
        return [{"type": "notInModern", "what": label or "this qualifier"}]
    if kind == "ZONE_TENDER":
        t = TENDERS.get(z.get("TenderType", -1))
        if t:
            return [{"type": "tender", "tender": t}]
        if z.get("TenderType") == 3:
            return [{"type": "command", "name": "coupon"}]
        if z.get("TenderType") == 21:
            return [{"type": "command", "name": "gratuity", "args": {"percent": (z.get("TenderAmount") or 1500) / 100}}]
        note_missing(page_name, label, f"the payment {label!r}")
        return [{"type": "notInModern", "what": label or "this payment"}]
    if kind == "ZONE_ITEM":
        if z.get("ItemName"):
            return [{"type": "addItem", "item": z["ItemName"]}]
    if kind == "ZONE_ORDER_FLOW":
        return [{"type": "jump", "mode": "index"}]
    if kind == "ZONE_ORDER_ADD":
        return [{"type": "command", "name": "repeatLine"}]
    if kind == "ZONE_ORDER_DELETE":
        return [{"type": "command", "name": "voidItem"}]
    if kind == "ZONE_ORDER_COMMENT":
        return [{"type": "jump", "mode": "push", "page": "note"}]
    if kind == "ZONE_ORDER_PAGE":
        return [{"type": "jump", "mode": "index"}]
    key = KEYS.get((msg or z.get("name") or "").strip().lower())
    if kind in ("ZONE_STANDARD", "ZONE_CONDITIONAL") and key:
        acts.append({"type": "key", "key": key})
    elif kind in ("ZONE_STANDARD", "ZONE_CONDITIONAL", "ZONE_COMMAND") and msg:
        if msg.startswith("RUNCMD:"):
            note_missing(page_name, label, "running a program on the computer")
            return [{"type": "notInModern", "what": "running a program on the computer"}]
        cmd = MESSAGES.get(msg.lower())
        if cmd:
            acts.append({"type": "command", "name": cmd})
        elif z.get("JumpType") and msg.lower() in ("save", "restore", "cancel", "clear"):
            pass   # a form's Save/Return: Modern keeps what's typed as it goes; the jump is what's left
        else:
            note_missing(page_name, label, f"the command {msg!r}")
            return [{"type": "notInModern", "what": label or msg}]
    jt = z.get("JumpType")
    if jt:
        mode = JUMPS.get(jt)
        if mode in ("push", "replace"):
            target = page_ids.get(z.get("JumpID"))
            if target:
                acts.append({"type": "jump", "mode": mode, "page": target})
        elif mode:
            acts.append({"type": "jump", "mode": mode})
    if not acts and kind in NAMES:
        note_missing(page_name, label, NAMES[kind])
        return [{"type": "notInModern", "what": NAMES[kind]}]
    if not acts and kind not in ("ZONE_SIMPLE", "ZONE_STANDARD", "ZONE_CONDITIONAL", "ZONE_COMMENT", "ZONE_ITEM"):
        what = kind.replace("ZONE_", "").replace("_", " ").lower()
        note_missing(page_name, label, what)
        return [{"type": "notInModern", "what": what}]
    return acts


def look(z, state, page, store):
    """The zone's look in a state (0 normal, 1 selected, 2 its other), legacy's defaults resolved."""
    pd = page["defaults"][state]
    sd = store["looks"][state]
    frame = frame_of(z["frame"][state]) or frame_of(pd["frame"]) or frame_of(sd["frame"]) or ("raised", 3)
    tex = z["texture"][state]
    if tex == IMAGE_DEFAULT:
        tex = pd["texture"] if pd["texture"] != IMAGE_DEFAULT else sd["texture"]
    color = z["color"][state]
    if color in (COLOR_DEFAULT, COLOR_PAGE_DEFAULT):
        color = pd["color"] if pd["color"] not in (COLOR_DEFAULT, COLOR_PAGE_DEFAULT) else sd["color"]
    font = z["font"] or page["font"] or store["font"]
    st = {}
    if frame[0] == "hidden":
        st.update(frame="none", fill="transparent", textColor="transparent", texture="")
    else:
        st["frame"] = frame[0]
        if frame[1]:
            st["frameWidth"] = frame[1]
        if tex == IMAGE_CLEAR or tex >= len(TEXTURES):
            st["fill"] = "transparent"
            st["texture"] = ""
        else:
            st["texture"] = TEXTURES[tex]
        st["textColor"] = "transparent" if color == COLOR_CLEAR or color >= len(COLORS) else COLORS[color]
    fam, px, bold = FONTS.get(font, FONTS[9])
    st.update(font=fam, fontSize=px, bold=bold)
    shadow = z["shadow"] if z["shadow"] < 256 else (page["shadow"] if page["shadow"] < 256 else store["shadow"])
    st["shadow"] = shadow
    st["radius"] = 0
    return st


def convert_zone(z, n, page, store, page_ids, canvas):
    label = label_of(z["name"])
    if not label and z["kind"] == "ZONE_QUALIFIER":
        q = z.get("QualifierType", 0)
        label = " ".join(QUALIFIER_NAMES[i] for i in range(len(QUALIFIER_NAMES)) if q & (1 << i))
    kind = z["kind"]
    # Its id says its page (a page's zones and its template's are one set): "n99-8".
    tag = f"n{-page['id']}" if page["id"] < 0 else str(page["id"])
    # On the page: legacy drew what was past its edge cut off there.
    x, y = max(0, z["x"]), max(0, z["y"])
    w, h = min(z["w"] - (x - z["x"]), canvas[0] - x), min(z["h"] - (y - z["y"]), canvas[1] - y)
    out = {"id": f"{tag}-{n}", "rect": {"x": x, "y": y, "w": max(1, w), "h": max(1, h)},
           "shape": SHAPES.get(z["shape"], "rect"), "behavior": BEHAVIORS.get(z["behave"], "blink")}
    if kind == "ZONE_COMMENT":
        out.update(kind="comment", label=label, behavior="none")
    elif kind in WIDGETS:
        out.update(kind=WIDGETS[kind], behavior="none")
        if kind == "ZONE_LOGIN":
            out["props"] = {"keys": False}   # legacy's PIN keys are buttons of their own on the page
    elif kind == "ZONE_TABLE":   # legacy's sample tables: drawn, not the store's
        out.update(kind="comment", label=label, behavior="none")
    else:
        out.update(kind="button", label=label)
        acts = actions_of(z, page["name"], label, page_ids)
        if acts:
            out["actions"] = acts
        if not acts and kind in ("ZONE_SIMPLE", "ZONE_STANDARD", "ZONE_CONDITIONAL"):
            out["kind"] = "label"   # legacy text on the page, touching it does nothing
            out["behavior"] = "none"
    out["style"] = {"normal": look(z, 0, page, store), "selected": look(z, 1, page, store)}
    # (A legacy zone's picture is a file on that store's computer: not here.)
    out["legacy"] = {"kind": kind, "id": z.get("JumpID")} if z.get("JumpID") else {"kind": kind}
    return out


def convert_page(p, store, page_ids, versions):
    lid = p["id"]
    w, h = SIZES.get(p["size"], (1920, 1080))
    bg_tex = p["image"] if p["image"] != IMAGE_DEFAULT else store["image"]
    out = {"schemaVersion": 1, "id": own_id(lid), "name": p["name"].replace("{termname}", "Terminal"),
           "kind": "custom" if KINDS.get(p["type"]) == "tables" and p["id"] not in STANDS_IN
           else KINDS.get(p["type"], "custom"), "inStyle": "legacy", "canvas": {"w": w, "h": h}, "grid": 4,
           "background": {"texture": TEXTURES[bg_tex]} if bg_tex < len(TEXTURES) else {"fill": "#000000"},
           "legacyId": lid}
    if lid in versions:
        out["variantOf"] = STANDS_IN[lid]
    parent = p["parent"]
    if parent in page_ids and parent != lid:
        out["templateId"] = own_id(parent)
    if lid == -99:
        out["kind"] = "template"
        out["contentArea"] = ORDER_CONTENT
    zones = p["zones"]
    # The login page's day-closing panel: legacy showed it only while closing the day, over the PIN entry.
    if any(z["kind"] == "ZONE_LOGIN" for z in zones):
        zones = [z for z in zones if z["kind"] != "ZONE_END_DAY"]
    out["zones"] = [convert_zone(z, n, p, store, page_ids, (w, h)) for n, z in enumerate(zones, 1)]
    # Off the page (what's left of it under 16x16): legacy never showed it either.
    out["zones"] = [z for z in out["zones"] if z["rect"]["w"] >= 16 and z["rect"]["h"] >= 16]
    # Legacy draws a page's zones last to first (zone/zone.cc Page::Render): its first on top.
    out["zones"].reverse()
    return out


def main():
    vt = vtdata.read_vt_data(os.path.join(DATA, "vt_data"))
    menu = vtdata.read_zone_db(os.path.join(DATA, "zone_db.dat"))
    tables = vtdata.read_zone_db(os.path.join(DATA, "tables.dat"))
    store = menu["defaults"]
    # One page per legacy id: the 1920x1080 one where there are two (-1), the first otherwise.
    pages = {}
    for p in vt["pages"] + menu["pages"] + tables["pages"]:
        have = pages.get(p["id"])
        if have is None or (have["size"] != 14 and p["size"] == 14):
            pages[p["id"]] = p
    page_ids = {lid: page_id(lid) for lid in pages}
    versions = {lid for lid in pages if lid in STANDS_IN}
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"):
            os.remove(os.path.join(OUT, f))
    for lid, p in sorted(pages.items()):
        page = convert_page(p, store, page_ids, versions)
        with open(os.path.join(OUT, page["id"] + ".json"), "w") as f:
            f.write(json.dumps(page, indent=1, ensure_ascii=True) + "\n")
    lines = ["# Legacy buttons not in ViewTouch Modern yet", "",
             "Made by tools/legacy/legacy_pages.py from the legacy ViewTouch's own pages. In the Legacy style",
             "these buttons are where legacy had them; touching one says it isn't in Modern yet.", ""]
    total = 0
    for page in sorted(missing):
        lines.append(f"## {page}")
        for label, what in missing[page]:
            lines.append(f"- {label}: {what}")
            total += 1
        lines.append("")
    open(os.path.join(OUT, "NOT-IN-MODERN.md"), "w").write("\n".join(lines))
    print(f"{len(pages)} pages, {sum(len(p['zones']) for p in pages.values())} zones; "
          f"{total} buttons not in Modern yet (seed/legacy/NOT-IN-MODERN.md)")


if __name__ == "__main__":
    main()
