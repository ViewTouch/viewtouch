"""Colorado: the Department of Revenue's jurisdiction codes and rates
(July-December 2026): every city and town's total rate and each county's
unincorporated rate, with the city's own rate on food served (restaurants)
where it differs. https://tax.colorado.gov/how-to-look-up-sales-use-tax-rates"""

import csv

import geo

STATE_RATE = 2.9


def build(folder, census):
    rows = list(csv.reader(open(f"{folder}/rates-By Name.csv", encoding="utf-8")))[1:]
    served = {}   # city -> its rate on food served, where different
    for r in csv.DictReader(open(f"{folder}/rates-Alternate City Rates.csv", encoding="utf-8")):
        if "immediate consumption" in r["Tax Type"]:
            served[geo.norm(r["Location Code"])] = float(r["Tax Rate"])
    city, county = {}, {}
    for r in rows:
        if len(r) < 8 or not r[7].strip():
            continue
        name, cty, total = r[0].strip(), geo.norm(r[2]), float(r[7])
        parts = {r[i].strip(): float(r[i + 1] or 0) for i in range(8, len(r) - 2, 3) if r[i].strip()}
        if name == "UNINCORPORATED":
            county.setdefault(cty, total)
            continue
        if "(" in name or "UNINCORPORATED" in name:
            continue   # a district inside a city or county: by address
        key = geo.norm(name)
        if key in served and "City" in parts:
            total = round(total - parts["City"] + served[key], 4)
        city.setdefault(key, {})[cty] = total
    return geo.by_city_and_county(geo.load(census, "08"), city, county, "Colorado", STATE_RATE)
