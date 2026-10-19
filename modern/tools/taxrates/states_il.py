"""Illinois: the Department of Revenue's machine-readable rate file
(ordmache-current.txt: every municipality, by county, and each county's
unincorporated area; combined general merchandise rates).
https://tax.illinois.gov/research/taxrates/sales-tax-rate-machine-readable-files.html"""

import geo

STATE_RATE = 6.25


def build(folder, census, on="20261001"):
    city, county = {}, {}
    with open(f"{folder}/ordmache.txt", encoding="latin-1") as f:
        for line in f:
            if len(line) < 80:
                continue
            name, cty = line[10:35].strip(), line[35:60].strip()
            # Current rate first, the one before it after: the one in effect on `on`.
            rec = line[60:]
            start, digits = rec[1:9], rec[9:15]
            if start > on and len(rec) > 72:
                start, digits = rec[72:80], rec[88:94] if False else rec[80:86]
            if not digits.isdigit() or int(digits) == 0:
                continue   # a home-rule city that collects its own (rate 0 here)
            rate = int(digits) / 10000
            if name.endswith(" COUNTY") and geo.norm(name[:-7]) == geo.norm(cty):
                county[geo.norm(cty)] = rate
            else:
                city.setdefault(geo.norm(name), {})[geo.norm(cty)] = rate
    # Cities whose rate varies by address (business districts): the address
    # file's base row (R00) for the city, outside its districts.
    with open(f"{folder}/ordmacha.txt", encoding="latin-1") as f:
        for line in f:
            if line[10:13] != "R00" or len(line) < 112:
                continue
            name, cty = line[13:38].strip(), line[38:63].strip()
            start, digits = line[98:106], line[106:112]
            if start > on or not digits.isdigit() or int(digits) == 0:
                continue
            city.setdefault(geo.norm(name), {}).setdefault(geo.norm(cty), int(digits) / 10000)
    return geo.by_city_and_county(geo.load(census, "17"), city, county, "Illinois", STATE_RATE)
