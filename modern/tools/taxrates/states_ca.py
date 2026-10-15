"""California: CDTFA's rates by city and county (combined: state, county and
district taxes). https://www.cdtfa.ca.gov/taxes-and-fees/sales-use-tax-rates.htm"""

import csv

import geo

STATE_RATE = 7.25


def build(folder, census):
    city, county = {}, {}
    with open(f"{folder}/rates.csv", encoding="utf-8", errors="replace") as f:
        for row in csv.reader(f):
            if len(row) < 4 or not row[1].replace(".", "").isdigit():
                continue
            rate = round(float(row[1]) * 100, 4)
            if row[3].strip().lower() == "county":
                county[geo.norm(row[2])] = rate
            elif row[3].strip().lower() == "city":
                city[geo.norm(row[0])] = rate
    return geo.by_city_and_county(geo.load(census, "06"), city, county, "California", STATE_RATE)
