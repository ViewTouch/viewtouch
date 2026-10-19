"""Texas: the Comptroller's quarterly rate file (every city and unincorporated
community, with its city, county, special district and transit rates).
https://comptroller.texas.gov/taxes/file-pay/edi/sales-tax-rates.php"""

from collections import Counter, defaultdict

import geo

STATE_RATE = 6.25


def build(folder, census):
    combos = defaultdict(lambda: defaultdict(Counter))   # place -> county -> Counter(combined rate)
    unincorporated = defaultdict(Counter)  # county -> Counter(combined rate), outside any city
    with open(f"{folder}/taxrates.txt", encoding="latin-1") as f:
        for line in f:
            p = [x.strip() for x in line.rstrip("\n").split("\t")]
            if len(p) < 12 or p[0] in ("",) or not p[3]:
                continue
            try:
                local = sum(float(p[i] or 0) for i in (2, 5, 8, 11))
            except ValueError:
                continue
            # San Antonio's Advanced Transportation District (0.375%, the city's
            # limits) is in the file without a place: added to the city here.
            if geo.norm(p[0]) == "san antonio" and p[1] != "n/a":
                local += 0.00375
            rate = round(STATE_RATE + local * 100, 4)
            combos[geo.norm(p[0])][geo.norm(p[3])][rate] += 1
            if p[1] == "n/a":
                unincorporated[geo.norm(p[3])][rate] += 1
    city = {k: {cty: c.most_common(1)[0][0] for cty, c in by.items()} for k, by in combos.items()}
    county = {k: c.most_common(1)[0][0] for k, c in unincorporated.items()}
    return geo.by_city_and_county(geo.load(census, "48"), city, county, "Texas", STATE_RATE)
