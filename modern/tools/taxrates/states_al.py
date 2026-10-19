"""Alabama: the Department of Revenue's local tax rates file (every city and
county, current). A city's rate: the state's, its county's and its own.
https://revenue.alabama.gov/ (taxrates_current.csv)"""

import csv
from collections import defaultdict

import geo

STATE_RATE = 4


def build(folder, census, on="20261001"):
    sales = defaultdict(float)   # locality code -> its general sales tax rate
    names, counties_of = {}, defaultdict(set)
    with open(f"{folder}/taxrates_current.csv", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            code = r["Locality Code"]
            names[code] = r["Locality Name"]
            if r["County Code"]:
                counties_of[code].add(r["County Code"])
            if r["TaxType"] != "ST" or r["Rate Type"] != "GENER":
                continue
            if r["Active Date"] > on or (r["Inactive Date"] and r["Inactive Date"] <= on):
                continue
            # The same levy is listed once for each county a city reaches into.
            key = (code, r["Active Date"], r["Rate"])
            if key in seen:
                continue
            seen.add(key)
            sales[code] += float(r["Rate"] or 0)
    # Counties: "X COUNTY" (70xx) outside cities; levies in named cities or
    # county-wide (77xx) and ones that exclude a city ("EXC") per their names.
    county = {}
    for code, name in names.items():
        if code.startswith("70"):
            base = geo.norm(name.split(" COUNTY")[0].split(" CO ")[0])
            # Outside cities: its own levy and any county-wide one (77xx with its number).
            wide = sum(sales.get(x, 0) for x in names if x.startswith("77") and x[2:] == code[2:]
                       and "COUNTY-WIDE" in names[x])
            county[base] = round(STATE_RATE + sales.get(code, 0) + wide, 4)
    # A city, for each county it's in: the state's rate, its own, the county
    # levies on it there: the county's own (70xx) unless that one leaves the
    # city out ("EXC") or the county has a levy inside city limits instead
    # ("CL", 77xx: Mobile, Auburn, Tuscaloosa), and county-wide ones (77xx).
    city = {}
    for code, name in names.items():
        if not code.startswith("9") or code not in sales:
            continue
        by = city.setdefault(geo.norm(name), {})
        # A county levy listed for the city (77xx) names its county too (7045 for 7745).
        listed = counties_of[code] | {"70" + x[2:] for x in counties_of[code] if x.startswith("77") and "70" + x[2:] in names}
        if not any(x.startswith("70") for x in listed):   # no county named: the city's own and the state's
            by[""] = round(STATE_RATE + sales[code] + sum(sales.get(x, 0) for x in listed if x.startswith("77")), 4)
        for c70 in sorted(x for x in listed if x.startswith("70")):
            cname = names.get(c70, "")
            extra = [x for x in listed if x.startswith("77") and x[2:] == c70[2:]]
            inside = any(" CL" in f" {names.get(x, '')}" for x in extra)
            own = 0 if inside or " EXC " in f" {cname} " else sales.get(c70, 0)
            home = geo.norm(cname.replace(" CO EXC", " COUNTY EXC").split(" COUNTY")[0].split(" CO ")[0])
            by[home] = round(STATE_RATE + sales[code] + own + sum(sales.get(x, 0) for x in extra), 4)
    return geo.by_city_and_county(geo.load(census, "01"), city, county, "Alabama", STATE_RATE)


seen = set()
