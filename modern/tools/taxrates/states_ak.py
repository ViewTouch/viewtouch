"""Alaska: no state sales tax; the Alaska Remote Seller Sales Tax Commission's
rate sheet by ZIP code (its member cities and boroughs: inside and outside
city limits), and their alcohol taxes. https://arsstc.org/business-sellers/tax-rates/"""

import csv
import glob
from collections import defaultdict


def build(folder):
    general = defaultdict(dict)   # zip -> {"I"/"O": (rate, city, borough)}
    for r in csv.DictReader(open(glob.glob(f"{folder}/zips-Generals*.csv")[0], encoding="utf-8")):
        z = r["zip"].strip().zfill(5)
        where = r["inoutcity"].strip()
        rate = float(r["totalsalestax"] or 0)
        prev = general[z].get(where)
        if not prev or rate > prev[0]:
            general[z][where] = (rate, r["city"].strip().title(), r["county"].strip().title())
    alcohol = {}
    for r in csv.DictReader(open(glob.glob(f"{folder}/zips-Alcohol*.csv")[0], encoding="utf-8")):
        if r["inoutcity"].strip() == "I":
            alcohol[r["zip"].strip().zfill(5)] = float(r["alcoholtaxrate"] or 0)
    out = {}
    for z, w in general.items():
        inside, outside = w.get("I"), w.get("O")
        main = inside or outside or next(iter(w.values()))
        rate, city, borough = main
        entry = {"r": rate, "p": [[f"{city if inside else borough} local taxes", rate]] if rate else [], "n": borough}
        if inside:
            entry["c"] = city
        if inside and outside and outside[0] != rate:
            entry["where"] = f"{city}, inside city limits"
            entry["alt"] = [[f"{outside[2]}, outside a city", outside[0]]]
        if z in alcohol:
            entry["alc"] = alcohol[z]   # a local tax on alcohol too
        out[z] = entry
    return out
