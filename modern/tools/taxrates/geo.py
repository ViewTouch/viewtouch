"""Which county and which city each ZIP code (Census ZCTA) lies in, by land
area, from the Census 2020 relationship files (public domain)."""

import csv
import os
import re
import unicodedata
from collections import defaultdict

import census

PLACE_SUFFIX = re.compile(r" (city|town|village|borough|CDP|municipality|city and borough|metro government"
                          r"|unified government|consolidated government|urban county|plantation)( \(balance\))?$")


def norm(name):
    """A name to match on: "St. Paul" and "Saint Paul", "Española" and
    "Espanola", case and punctuation aside."""
    n = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode()
    n = n.lower().replace("saint ", "st ").replace("st. ", "st ").replace("ste. ", "ste ")
    n = re.sub(r"[^a-z0-9 ]", "", n)
    return re.sub(r"\s+", " ", n).strip()


def load(folder, state_fips):
    """ZIP -> {"land": its land area, "counties": [(name, share)], "places": [(name, share)]},
    biggest share first, for one state."""
    zips = defaultdict(lambda: {"land": 0, "counties": [], "places": []})

    def read(name, key, strip):
        with open(os.path.join(folder, name), encoding="utf-8-sig") as f:
            for row in csv.DictReader(f, delimiter="|"):
                z = row["GEOID_ZCTA5_20"]
                geoid = row[f"GEOID_{key}_20"]
                if not z or not geoid.startswith(state_fips):
                    continue
                land = int(row["AREALAND_ZCTA5_20"] or 0)
                part = int(row["AREALAND_PART"] or 0)
                name = row[f"NAMELSAD_{key}_20"]
                if strip:
                    name = census.plain(PLACE_SUFFIX.sub("", census.plain(name)))
                zips[z]["land"] = land
                zips[z]["counties" if key == "COUNTY" else "places"].append(
                    (name, geoid, part / land if land else 0.0))

    read("tab20_zcta520_county20_natl.txt", "COUNTY", False)
    read("tab20_zcta520_place20_natl.txt", "PLACE", True)
    for z in zips.values():
        z["counties"].sort(key=lambda x: -x[2])
        z["places"].sort(key=lambda x: -x[2])
    return zips


def by_city_and_county(zips, city_rate, county_rate, state_name, state_rate, inside=0.1):
    """ZIP entries from a state's rates by city (combined, inside its limits) and
    by county (combined, outside any city). A ZIP whose biggest city with
    its own rate covers a tenth of it or more gets the city's (restaurants
    are mostly in town; a city ZIP often has a big rural edge); otherwise
    its main county's. The other
    rates found in the ZIP are kept as "also". A city's rate may be a dict
    by county (a city in two counties), the ZIP's county chosen."""
    def city_in(name, county_key):
        r = city_rate.get(norm(name))
        if isinstance(r, dict):
            return r.get(county_key, next(iter(r.values()), None))
        return r

    out = {}
    for z, g in zips.items():
        if not g["counties"]:
            continue
        county = g["counties"][0][0]
        county_key = norm(county.replace(" County", "").replace(" Parish", "").replace(" Borough", ""))
        rate, city = county_rate.get(county_key), None
        # The biggest place here with a rate of its own (a census-designated
        # place outside any city has none).
        place = next((p for p in g["places"] if city_in(p[0], county_key) is not None), None)
        if place and place[2] >= inside:
            rate, city = city_in(place[0], county_key), place[0]
        if rate is None:
            continue
        # The other rates in the ZIP, each with where it applies: the manager
        # chooses when the store's is one of them.
        alt = {}
        for name, _, share in g["places"]:
            if share >= 0.01 and city_in(name, county_key) is not None:
                alt.setdefault(round(city_in(name, county_key), 4), f"{name}, inside city limits")
        for name, _, share in g["counties"]:
            k = norm(name.replace(" County", "").replace(" Parish", "").replace(" Borough", ""))
            if share >= 0.01 and k in county_rate:
                alt.setdefault(round(county_rate[k], 4), f"{name}, outside a city")
        alt.pop(round(rate, 4), None)
        parts = [[state_name, state_rate]]
        if round(rate - state_rate, 4) > 0:
            parts.append([f"{city or county} local taxes", round(rate - state_rate, 4)])
        entry = {"r": round(rate, 4), "p": parts, "n": county}
        main = g["places"][0] if g["places"] else None   # for showing where (not the rate)
        if city or (main and main[2] >= 0.5):
            entry["c"] = city or main[0]
        if alt:
            entry["where"] = f"{city}, inside city limits" if city else f"{county}, outside a city"
            entry["alt"] = [[label, r] for r, label in sorted(alt.items())]
        out[z] = entry
    return out
