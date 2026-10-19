"""New Mexico: the Taxation and Revenue Department's gross receipts tax
locations and rates, July 1, 2026 to June 30, 2027 (its public map data).
Cities and "Remainder of County" (outside cities)."""

import json
import re

import geo

STATE_RATE = 4.875


def build(folder, census):
    rows = [f["attributes"] for f in json.load(open(f"{folder}/locations.json"))["features"]]
    city, county = {}, {}
    for r in rows:
        name, rate = r["name"].strip(), float(r["grt_rate"])
        if name.startswith("Remainder of County"):
            county[geo.norm(r["county"])] = rate
            continue
        if not r["locat_cdr"].strip() or "TIF" in name or "Pueblo" in name or "Nation" in name:
            continue   # tribal lands and tax increment districts: by address
        name = re.sub(r"\*|\s*\([^)]*\)", "", name).strip()   # "Santa Fe (city)", "Espanola (Santa Fe)"
        city.setdefault(geo.norm(name), {})[geo.norm(r["county"])] = rate
    return geo.by_city_and_county(geo.load(census, "35"), city, county, "New Mexico", STATE_RATE)
