"""Virginia: Virginia Tax's sales tax rates by locality (counties and
independent cities, by Census FIPS code), the sheet in effect now.
https://www.tax.virginia.gov/retail-sales-and-use-tax"""

import csv
import glob

import geo

STATE_RATE = 4.3


def build(folder, census, counties):
    sheet = next(f for f in glob.glob(f"{folder}/rates-*.csv") if "Present" in f)
    county = {}
    with open(sheet, encoding="utf-8") as f:
        for row in csv.reader(f):
            if len(row) > 3 and row[2].strip().isdigit() and row[3].strip().endswith("%"):
                code = row[2].strip()
                name = counties.get(("51", code[2:]))
                if name:
                    county[geo.norm(name.replace(" County", ""))] = float(row[3].strip().rstrip("%"))
    return geo.by_city_and_county(geo.load(census, "51"), {}, county, "Virginia", STATE_RATE)
