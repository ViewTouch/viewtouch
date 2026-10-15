#!/usr/bin/env python3
"""Builds seed/tax/areas.json: the sales tax rate in every US ZIP code, from
each state's own published rates (see each states_*.py, sst.py and data/),
with each state's restaurant rules (data/states.json).

    tools/taxrates/build.py [--cache DIR] [--on YYYYMMDD]

The source files are downloaded by hand into the cache (~/.cache/vtm-taxsrc
by default; see README.md here for where each comes from), then this run.
"""

import argparse
import datetime
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import census  # noqa: E402
import sst  # noqa: E402
import states_ak  # noqa: E402
import states_al  # noqa: E402
import states_az  # noqa: E402
import states_ca  # noqa: E402
import states_co  # noqa: E402
import states_il  # noqa: E402
import states_mo  # noqa: E402
import states_nm  # noqa: E402
import states_tx  # noqa: E402
import states_va  # noqa: E402
import tables  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))

# Where each state's rates come from (the app shows it with the rate).
SOURCES = {
    "CA": "California CDTFA, rates effective October 1, 2026",
    "TX": "Texas Comptroller, local rate file for the quarter",
    "MO": "Missouri DOR, rate tables October to December 2026",
    "VA": "Virginia Tax, sales tax rates by locality, January 2026 to now",
    "NM": "New Mexico TRD, gross receipts tax rates July 2026 to June 2027",
    "AL": "Alabama DOR, local tax rates file (current)",
    "AZ": "Arizona DOR, TPT rates for restaurants and bars, October 2026",
    "CO": "Colorado DOR, jurisdiction codes and rates July to December 2026",
    "AK": "Alaska Remote Seller Sales Tax Commission rate sheet, October 1, 2026",
    "IL": "Illinois DOR, machine-readable rate files (current)",
}
SST_SOURCE = "Streamlined Sales Tax rate and boundary files for the quarter"
# States whose local rate couldn't be gathered for every place: the app
# says to look it up (where), with the state's rate meanwhile.
LOOK_UP = {
    "LA": "https://lulstb.com/ (Louisiana Uniform Local Sales Tax Board rate lookup)",
    "ID": "https://tax.idaho.gov/ (resort city taxes: Ketchum, Sun Valley, McCall and others)",
    "MT": "https://mtrevenue.gov/ (resort area taxes: Whitefish, Big Sky, West Yellowstone and others)",
}
# Local rates gathered by rules from the state's file that may miss a case:
# the app asks the manager to confirm with the state.
CONFIRM = {"AL": "https://myalabamataxes.alabama.gov/ (Alabama DOR rate lookup)"}


def compact(entry, state_rate):
    """Smaller: the parts only when there are more than the state's and one local line."""
    out = {"r": entry["r"]}
    if len(entry.get("p", [])) > 2:
        out["p"] = entry["p"]
    for k in ("c", "n", "where", "alt", "alc"):
        if k in entry:
            out[k] = entry[k]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=os.path.expanduser("~/.cache/vtm-taxsrc"))
    ap.add_argument("--on", default=datetime.date.today().strftime("%Y%m%d"))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "..", "seed", "tax", "areas.json"))
    a = ap.parse_args()
    c = a.cache
    counties, places = census.load(f"{c}/census")
    rules = json.load(open(f"{HERE}/data/states.json"))
    rules.pop("_about", None)

    zips, sources = {}, {}
    # Real ZIP areas (Census ZCTAs): a state's file may list whole ranges.
    with open(f"{c}/census/tab20_zcta520_county20_natl.txt", encoding="utf-8-sig") as f:
        real = {line.split("|")[1] for line in f if line.split("|")[1:2] and line.split("|")[1].isdigit()}

    def add(state, entries, source):
        n = 0
        for z, e in entries.items():
            if z in zips or z not in real:
                continue   # a ZIP across a state line: the first state's
            if e["r"] == rules[state]["rate"] and not e.get("alt") and not e.get("alc"):
                continue   # the state's rate and nothing else: the state's entry says it
            zips[z] = dict(compact(e, rules[state]["rate"]), s=state)
            n += 1
        sources[state] = source
        print(f"{state}: {n} ZIP codes", file=sys.stderr)

    for st in sorted(sst.STATES):
        add(st, sst.build(st, f"{c}/sst/x", a.on, counties, places), SST_SOURCE)
    cen = f"{c}/census"
    add("CA", states_ca.build(f"{c}/states/CA", cen), SOURCES["CA"])
    add("TX", states_tx.build(f"{c}/states/TX", cen), SOURCES["TX"])
    add("MO", states_mo.build(f"{c}/states/MO", cen, counties, places), SOURCES["MO"])
    add("VA", states_va.build(f"{c}/states/VA", cen, counties), SOURCES["VA"])
    add("NM", states_nm.build(f"{c}/states/NM", cen), SOURCES["NM"])
    add("AL", states_al.build(f"{c}/states/AL", cen, a.on), SOURCES["AL"])
    add("AZ", states_az.build(f"{c}/states/AZ", cen), SOURCES["AZ"])
    add("CO", states_co.build(f"{c}/states/CO", cen), SOURCES["CO"])
    add("AK", states_ak.build(f"{c}/states/AK"), SOURCES["AK"])
    add("IL", states_il.build(f"{c}/states/IL", cen, a.on), SOURCES["IL"])
    for st in ("NY", "FL", "SC", "PA", "HI", "MS"):
        t = json.load(open(f"{HERE}/data/{st}.json"))
        add(st, tables.build(f"{HERE}/data/{st}.json", cen), t["source"])

    states = {}
    for st, r in sorted(rules.items()):
        s = {"rate": r["rate"]}
        for k in ("meals", "alcohol", "notes"):
            if k in r:
                s[k] = r[k]
        s["source"] = sources.get(st, r.get("source", "State rate"))
        if st in LOOK_UP:
            s["lookUp"] = LOOK_UP[st]
        if st in CONFIRM:
            s["confirm"] = CONFIRM[st]
        states[st] = s

    # Each ZIP code's state by its first three digits (for ZIP codes the
    # table doesn't list: a state with one rate everywhere), from the Census.
    fips_state = {v: k for k, v in tables.FIPS.items()}
    from collections import Counter, defaultdict
    by_prefix = defaultdict(Counter)
    with open(f"{c}/census/tab20_zcta520_county20_natl.txt", encoding="utf-8-sig") as f:
        for line in f:
            p = line.split("|")
            if len(p) > 9 and p[1].isdigit() and p[9][:2] in fips_state:
                by_prefix[p[1][:3]][fips_state[p[9][:2]]] += int(p[16] or 0) if len(p) > 16 and p[16].isdigit() else 1
    prefixes = {k: v.most_common(1)[0][0] for k, v in sorted(by_prefix.items())}

    out = {
        "about": "Sales tax rates by US ZIP code, from each state's published rates. A suggestion for the "
                 "store to confirm: rates can differ within a ZIP code, and local restaurant taxes are noted, "
                 "not always included. Check with the store's accountant.",
        "asOf": f"{a.on[:4]}-{a.on[4:6]}-{a.on[6:]}",
        "states": states,
        "prefixes": prefixes,
        "zips": dict(sorted(zips.items())),
    }
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(out, f, separators=(",", ":"), ensure_ascii=False)
    print(f"{len(zips)} ZIP codes, {os.path.getsize(a.out) // 1024} KB -> {a.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
