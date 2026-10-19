"""Arizona: the Department of Revenue's TPT rate table (all business codes),
restaurants and bars (business code 011): counties (with the state's 5.6%)
and cities (their own part). https://azdor.gov/node/831"""

import csv
from datetime import datetime

import geo

STATE_RATE = 5.6


def build(folder, census, on=datetime(2026, 10, 1)):
    county, city_part = {}, {}
    with open(f"{folder}/rates.csv", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            if r["BusinessCode"] != "011":
                continue
            start = datetime.strptime(r["RateStartDate"].split()[0], "%m/%d/%Y")
            end = datetime.strptime(r["RateEndDate"].split()[0], "%m/%d/%Y")
            if not (start <= on <= end):
                continue
            name, rate = r["RegionName"].strip(), float(r["TaxRate"])
            if "(" in name or "DISTRICT" in name or "AUTHORITY" in name:
                continue   # tribal lands, districts: by address
            if rate >= STATE_RATE:
                county[geo.norm(name)] = rate          # the state's and the county's
            else:
                city_part[geo.norm(name)] = rate       # the city's own
    city = {c: {k: round(v + part, 4) for k, v in county.items()} for c, part in city_part.items()}
    return geo.by_city_and_county(geo.load(census, "04"), city, county, "Arizona", STATE_RATE)
