#!/usr/bin/env python3
# Which optional fields each legacy zone type keeps in a page file, from the
# legacy program's own source (the legacy branch's zone/*.hh and
# zone/pos_zone.cc): PosZone::Read reads Amount(), Expression()... in turn,
# and a type has a field when its class (or one it comes from) gives that
# accessor. Writes zone_fields.json beside this file:
#
#   python3 -I tools/legacy/zone_fields.py LEGACY-SOURCE-DIR
import json, os, re, sys

FIELDS = [  # PosZone::Read's order after the zone's look (version 29), and each one's kind
    ("Amount", "int"), ("Expression", "str"), ("FileName", "str"), ("ImagePath", "always-str"),
    ("JumpType", "int"), ("JumpID", "int"), ("Message", "str"), ("ItemName", "str"), ("Script", "str"),
    ("QualifierType", "int"), ("ReportType", "int"), ("Spacing", "flt"), ("TenderType", "int"),
    ("TenderAmount", "int"), ("ReportPrint", "int"), ("Columns", "int"), ("SwitchType", "int"),
    ("CustomerType", "int"), ("CheckDisplayNum", "int"), ("VideoTarget", "int"),
    ("DrawerZoneType", "int"), ("Confirm", "int"), ("ConfirmMsg", "str"),
]
src = sys.argv[1]
zone_dir = os.path.join(src, "zone")
headers = "".join(open(os.path.join(zone_dir, f), errors="replace").read() + "\n"
                  for f in sorted(os.listdir(zone_dir)) if f.endswith(".hh"))
# class X : public Y[, public Z] { ... };  -> its bases and the accessors it gives
classes = {}
for m in re.finditer(r"\bclass\s+(\w+)\s*(?::\s*([^{;]+))?\{", headers):
    name, bases = m.group(1), m.group(2) or ""
    depth, i = 1, m.end()
    while depth and i < len(headers):
        depth += {"{": 1, "}": -1}.get(headers[i], 0)
        i += 1
    body = headers[m.end():i]
    gives = {f for f, _ in FIELDS if re.search(r"\*\s*" + f + r"\s*\(\s*\)\s*(?:override)?\s*\{\s*return\s*&", body)}
    classes[name] = {"bases": re.findall(r"(?:public|protected|private)?\s*(\w+)", bases), "gives": gives}

def gives(cls, seen=()):
    c = classes.get(cls)
    if not c or cls in seen:
        return set()
    out = set(c["gives"])
    for b in c["bases"]:
        if b not in ("public", "protected", "private"):
            out |= gives(b, seen + (cls,))
    return out

enum = {}
for m in re.finditer(r"\b(ZONE_\w+)\s*=\s*(\d+)", open(os.path.join(zone_dir, "pos_zone.hh"), errors="replace").read()):
    enum[m.group(1)] = int(m.group(2))
cc = open(os.path.join(zone_dir, "pos_zone.cc"), errors="replace").read()
switch = cc[cc.index("Zone *NewPosZone"):]
switch = switch[:switch.index("\n}\n")]
types = {}
pending = []
for m in re.finditer(r"case\s+(ZONE_\w+)\s*:|new\s+(\w+)\s*[;(]", switch):
    if m.group(1):
        pending.append(m.group(1))
    else:
        for z in pending:
            types[enum[z]] = {"zone": z, "class": m.group(2),
                              "fields": [f for f, k in FIELDS if k == "always-str" or f in gives(m.group(2))]}
        pending = []
missing = sorted({t["class"] for t in types.values() if t["class"] not in classes})
out = {"fields": FIELDS, "types": {str(k): v for k, v in sorted(types.items())}, "unknownClasses": missing}
json.dump(out, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "zone_fields.json"), "w"), indent=1)
print(f"{len(types)} zone types; classes not found: {missing}")
