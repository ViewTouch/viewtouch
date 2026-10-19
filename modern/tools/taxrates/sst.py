"""Streamlined Sales Tax states: their rate and boundary files (public, the
same format for every member state, refreshed each quarter) to the rate in
each 5-digit ZIP code.

Boundary records ("4": a ZIP+4 range, "Z": a whole 5-digit ZIP) name the
jurisdictions that apply there: the state, county, place and up to 20
special districts. The rate file gives each one's rate. A ZIP gets the rate
that covers most of its ZIP+4 codes; the others are kept as "also".
"""

import csv
import glob
import os
from collections import Counter, defaultdict

# Streamlined member states (two letters: FIPS code).
STATES = {
    "AR": "05", "GA": "13", "IA": "19", "IN": "18", "KS": "20", "KY": "21", "MI": "26", "MN": "27",
    "NC": "37", "ND": "38", "NE": "31", "NJ": "34", "NV": "32", "OH": "39", "OK": "40", "RI": "44",
    "SD": "46", "TN": "47", "UT": "49", "VT": "50", "WA": "53", "WI": "55", "WV": "54", "WY": "56",
}


def newest(folder, state, kind):
    """The state's newest file of a kind ("R" rates, "B" boundaries)."""
    files = [f for f in glob.glob(os.path.join(folder, "*"))
             if os.path.basename(f).upper().startswith(state + kind) and f.lower().endswith(".csv")]
    return max(files, key=lambda f: os.path.basename(f).upper()) if files else None


def active(begin, end, on):
    return (begin or "00000000") <= on <= (end or "99999999")


def rates(path, on):
    """(type, code) -> general intrastate rate, and code -> rate (any type)."""
    by_type, by_code = {}, {}
    with open(path, newline="", encoding="latin-1") as f:
        for row in csv.reader(f):
            if len(row) < 9 or not active(row[7].strip(), row[8].strip(), on):
                continue
            # Some states write the type in one digit ("0" for "00").
            kind, code = row[1].strip().zfill(2), row[2].strip()
            try:
                rate = float(row[3])
            except ValueError:
                continue
            by_type[(kind, code)] = rate
            by_code.setdefault(code, rate)
    return by_type, by_code


def build(state, folder, on, counties, places):
    """ZIP -> {"r": percent, "p": [[name, percent]...], "c": city, "n": county,
    "also": [other percents in the ZIP]} for one Streamlined state."""
    fips = STATES[state]
    rpath, bpath = newest(folder, state, "R"), newest(folder, state, "B")
    if not rpath or not bpath:
        raise SystemExit(f"{state}: no rate or boundary file in {folder}")
    by_type, by_code = rates(rpath, on)

    def parts_of(row):
        """The jurisdictions on a boundary row: [(name, percent)], and its place/county codes."""
        parts = []
        indicator, county, place = row[23].strip(), row[24].strip(), row[25].strip()
        if indicator and indicator != "00":
            r = by_type.get(("45", indicator), by_code.get(indicator))
            if r is not None:
                parts.append(("state", indicator, r))
        if county:
            r = by_type.get(("00", county))
            if r:
                parts.append(("county", county, r))
        if place:
            r = next((by_type[k] for k in by_type if k[1] == place and k[0] not in ("45", "00")), None)
            if r:
                parts.append(("place", place, r))
        for i in range(29, min(len(row), 89), 3):
            code = row[i + 1].strip() if i + 1 < len(row) else ""
            kind = row[i + 2].strip().zfill(2) if i + 2 < len(row) else ""
            if not code:
                continue
            r = by_type.get((kind, code), by_code.get(code))
            if r:
                parts.append(("district", code, r))
        return tuple(parts), county, place

    # Each ZIP: how many addresses (address records, where the state gives
    # them: the most exact) or else ZIP+4 codes have each set of
    # jurisdictions; whole-ZIP records only where neither says.
    addresses = defaultdict(Counter)
    weights = defaultdict(Counter)
    whole = {}
    with open(bpath, newline="", encoding="latin-1") as f:
        for row in csv.reader(f):
            if len(row) < 26 or row[22].strip() != fips:
                continue
            kind = row[0].strip().upper().lstrip("\ufeff")
            if kind not in ("A", "4", "Z") or not active(row[1].strip(), row[2].strip(), on):
                continue
            if kind == "A":
                z = row[15].strip()[:5]
                if z.isdigit():
                    addresses[z][parts_of(row)] += 1
                continue
            lo, hi = row[17].strip(), row[19].strip()
            if not (lo.isdigit() and hi.isdigit()):
                continue
            key = parts_of(row)
            if kind == "Z":
                for z in range(int(lo), int(hi) + 1):
                    whole.setdefault(f"{z:05d}", key)
                continue
            elo = int(row[18]) if row[18].strip().isdigit() else 0
            ehi = int(row[20]) if row[20].strip().isdigit() else 9999
            for z in range(int(lo), int(hi) + 1):
                n = (ehi if z == int(hi) else 9999) - (elo if z == int(lo) else 0) + 1
                weights[f"{z:05d}"][key] += max(1, n)

    out = {}
    for z in set(addresses) | set(weights) | set(whole):
        counted = addresses.get(z) or weights.get(z)
        key = counted.most_common(1)[0][0] if counted else whole[z]
        parts, county, place = key
        named = []
        for what, code, r in parts:
            if what == "state":
                name = STATE_NAMES[state]
            elif what == "county":
                name = counties.get((fips, code), f"County {code}")
            elif what == "place":
                name = places.get((fips, code), f"City {code}")
            else:
                name = f"Local district {code}"
            named.append([name, round(r * 100, 4)])
        total = round(sum(p[1] for p in named), 4)

        def where(k):
            _, cty, plc = k
            if plc and (fips, plc) in places:
                return f"{places[(fips, plc)]}, inside city limits"
            if cty and (fips, cty) in counties:
                return f"{counties[(fips, cty)]}, outside a city"
            return "Elsewhere in this ZIP code"

        alt = {}
        for k, _ in (counted.most_common() if counted else []):
            r = round(sum(x[2] for x in k[0]) * 100, 4)
            if r != total:
                alt.setdefault(r, where(k))
        entry = {"r": total, "p": named}
        if place and (fips, place) in places:
            entry["c"] = places[(fips, place)]
        if county and (fips, county) in counties:
            entry["n"] = counties[(fips, county)]
        if alt:
            entry["where"] = where(key)
            entry["alt"] = [[label, r] for r, label in sorted(alt.items())]
        out[z] = entry
    return out


STATE_NAMES = {
    "AL": "Alabama", "AK": "Alaska", "AZ": "Arizona", "AR": "Arkansas", "CA": "California", "CO": "Colorado",
    "CT": "Connecticut", "DE": "Delaware", "DC": "District of Columbia", "FL": "Florida", "GA": "Georgia",
    "HI": "Hawaii", "ID": "Idaho", "IL": "Illinois", "IN": "Indiana", "IA": "Iowa", "KS": "Kansas",
    "KY": "Kentucky", "LA": "Louisiana", "ME": "Maine", "MD": "Maryland", "MA": "Massachusetts",
    "MI": "Michigan", "MN": "Minnesota", "MS": "Mississippi", "MO": "Missouri", "MT": "Montana",
    "NE": "Nebraska", "NV": "Nevada", "NH": "New Hampshire", "NJ": "New Jersey", "NM": "New Mexico",
    "NY": "New York", "NC": "North Carolina", "ND": "North Dakota", "OH": "Ohio", "OK": "Oklahoma",
    "OR": "Oregon", "PA": "Pennsylvania", "RI": "Rhode Island", "SC": "South Carolina", "SD": "South Dakota",
    "TN": "Tennessee", "TX": "Texas", "UT": "Utah", "VT": "Vermont", "VA": "Virginia", "WA": "Washington",
    "WV": "West Virginia", "WI": "Wisconsin", "WY": "Wyoming",
}
