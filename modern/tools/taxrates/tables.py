"""States whose rates come from a hand-checked table (data/<ST>.json): the
state's rate, combined rates by county (outside cities with their own) and by
city, from the state's own publication, named in the file."""

import json
import os

import geo

FIPS = {
    "AL": "01", "AK": "02", "AZ": "04", "AR": "05", "CA": "06", "CO": "08", "CT": "09", "DE": "10", "DC": "11",
    "FL": "12", "GA": "13", "HI": "15", "ID": "16", "IL": "17", "IN": "18", "IA": "19", "KS": "20", "KY": "21",
    "LA": "22", "ME": "23", "MD": "24", "MA": "25", "MI": "26", "MN": "27", "MS": "28", "MO": "29", "MT": "30",
    "NE": "31", "NV": "32", "NH": "33", "NJ": "34", "NM": "35", "NY": "36", "NC": "37", "ND": "38", "OH": "39",
    "OK": "40", "OR": "41", "PA": "42", "RI": "44", "SC": "45", "SD": "46", "TN": "47", "TX": "48", "UT": "49",
    "VT": "50", "VA": "51", "WA": "53", "WV": "54", "WI": "55", "WY": "56",
}


def build(path, census):
    t = json.load(open(path))
    county = {geo.norm(k): v for k, v in t.get("counties", {}).items()}
    city = {geo.norm(k): v for k, v in t.get("cities", {}).items()}
    if not county:   # one rate for the whole state, but for some counties and cities
        default = t.get("everywhere", t["stateRate"])
        zips = geo.load(census, FIPS[t["state"]])
        county = {geo.norm(c[0].replace(" County", "").replace(" Parish", "").replace(" Borough", "")): default
                  for g in zips.values() for c in g["counties"]}
        county.update({geo.norm(k): v for k, v in t.get("countyRates", {}).items()})
        return geo.by_city_and_county(zips, city, county, t["name"], t["stateRate"])
    return geo.by_city_and_county(geo.load(census, FIPS[t["state"]]), city, county, t["name"], t["stateRate"])
