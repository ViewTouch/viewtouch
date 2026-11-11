# Reading the legacy ViewTouch's page files (vt_data, zone_db.dat,
# tables.dat) by the legacy program's own rules: src/core/data_file.cc
# (numbers in base 64, A-Z a-z 0-9 + /; "~" an empty string, "_" a space),
# zone/pos_zone.cc PosPage::Read and PosZone::Read, and each zone type's
# fields (zone_fields.json, from the legacy headers by zone_fields.py).
import gzip, json, os

_DIGITS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
_DECODE = {c: i for i, c in enumerate(_DIGITS)}
_TABLE = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "zone_fields.json")))
FIELD_KIND = dict((f, k) for f, k in _TABLE["fields"])
FIELD_ORDER = [f for f, _ in _TABLE["fields"]]
TYPES = {int(k): v for k, v in _TABLE["types"].items()}


class Tokens:
    def __init__(self, path):
        raw = open(path, "rb").read()
        if raw[:2] == b"\x1f\x8b":
            raw = gzip.decompress(raw)
        self.t = raw.decode("latin-1").split()
        self.i = 0

    def done(self):
        return self.i >= len(self.t)

    def word(self):
        if self.i >= len(self.t):
            raise EOFError("the file ended early")
        w = self.t[self.i]
        self.i += 1
        return w

    def int(self):
        v = 0
        for c in self.word():
            v = (v << 6) + _DECODE[c]
        if v >= 1 << 63:              # written as a 64-bit number: negative
            v -= 1 << 64
        if (1 << 31) <= v < (1 << 32):   # a negative 32-bit int
            v -= 1 << 32
        return v

    def str(self):
        w = self.word()
        return "" if w == "~" else w.replace("_", " ")

    def flt(self):
        return float(self.word())


def read_zone(f, version):
    z = {"type": f.int(), "name": f.str(), "group": f.int(), "x": f.int(), "y": f.int(), "w": f.int(), "h": f.int(),
         "behave": f.int(), "font": f.int(), "frame": [], "texture": [], "color": [], "image": []}
    for _ in range(3):
        z["frame"].append(f.int())
        z["texture"].append(f.int())
        z["color"].append(f.int())
        z["image"].append(f.int())
    z["shadow"] = f.int()
    z["shape"] = f.int()
    kind = TYPES.get(z["type"])
    if kind is None:
        raise ValueError(f"zone type {z['type']} ({z['name']!r}) isn't one the legacy program knows")
    has = set(kind["fields"])
    for name in FIELD_ORDER:
        if name == "ImagePath" and version < 29:
            continue
        if name not in has:
            continue
        k = FIELD_KIND[name]
        z[name] = f.int() if k == "int" else f.flt() if k == "flt" else f.str()
    z["key"] = f.int()
    z["kind"] = kind["zone"]
    return z


def read_page(f, version):
    p = {"name": f.str(), "id": f.int(), "image": f.int(), "titleColor": f.int(), "parent": f.int(), "type": f.int(),
         "index": f.int(), "size": f.int(), "font": f.int()}
    p["defaults"] = [{"frame": f.int(), "texture": f.int(), "color": f.int()} for _ in range(3)]
    p["spacing"] = f.int()
    p["shadow"] = f.int()
    p["zones"] = [read_zone(f, version) for _ in range(f.int())]
    return p


def read_vt_data(path):
    """vt_data: 'vtpos 0 1', the zone version, the system pages, then the default accounts."""
    f = Tokens(path)
    assert f.word() == "vtpos", "not a ViewTouch data file"
    f.word()
    file_version = int(f.word())
    version = f.int()
    pages = [read_page(f, version) for _ in range(f.int())]
    accounts = []
    f.int()   # the accounts' version
    for _ in range(f.int()):
        accounts.append({"no": f.int(), "name": f.str()})
    return {"fileVersion": file_version, "version": version, "pages": pages, "accounts": accounts, "rest": f.t[f.i:]}


def read_zone_db(path):
    """zone_db.dat / tables.dat: 'vtpos 0 VERSION', how many pages, the pages (ZoneDB::Load)."""
    f = Tokens(path)
    assert f.word() == "vtpos", "not a ViewTouch data file"
    f.word()
    version = int(f.word())
    pages = [read_page(f, version) for _ in range(f.int())]
    # The store's defaults for every page (version 28 on): what a page's
    # "default" frame, texture, color, font... are.
    defaults = None
    if version >= 28:
        defaults = {"font": f.int(), "shadow": f.int(), "spacing": f.int()}
        defaults["looks"] = [{"frame": f.int(), "texture": f.int(), "color": f.int()} for _ in range(3)]
        defaults.update(image=f.int(), titleColor=f.int(), size=f.int())
    return {"version": version, "pages": pages, "defaults": defaults, "rest": f.t[f.i:]}
