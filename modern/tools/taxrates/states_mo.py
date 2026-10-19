"""Missouri: the Department of Revenue's quarterly Sales and Use Tax Rate
Tables (every county and city, combined rates; the codes are Census FIPS
codes). https://dor.mo.gov/business/sales/rates/"""

import re

import geo

STATE_RATE = 4.225
ROW = re.compile(r"^\S.*?\s(\d{5})-(\d{3})-000\s+([\d.]+)%")


def build(folder, census, counties, places):
    city, county = {}, {}
    with open(f"{folder}/oct2026.txt", encoding="latin-1") as f:
        for line in f:
            m = ROW.match(line)
            if not m:
                continue
            place, cty, rate = m.group(1), m.group(2), float(m.group(3))
            cname = counties.get(("29", cty))
            if not cname:
                continue
            ckey = geo.norm(cname.replace(" County", ""))
            if place == "00000":
                county[ckey] = rate
            elif not place.startswith("9") and ("29", place) in places:
                city.setdefault(geo.norm(places[("29", place)]), {})[ckey] = rate
    return geo.by_city_and_county(geo.load(census, "29"), city, county, "Missouri", STATE_RATE)
