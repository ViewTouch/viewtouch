"""Census county and place names (public domain): FIPS codes to names."""

import os
import re


# Formal Census names to the ones people use.
PLAIN = {
    "Nashville-Davidson metropolitan government": "Nashville", "Urban Honolulu": "Honolulu",
    "Louisville/Jefferson County metro government": "Louisville", "Athens-Clarke County unified government": "Athens",
    "Augusta-Richmond County consolidated government": "Augusta", "Macon-Bibb County": "Macon",
    "Lexington-Fayette urban county": "Lexington", "Indianapolis city": "Indianapolis",
}


def plain(name):
    name = re.sub(r" \(balance\)$", "", name)
    return PLAIN.get(name, name)


def load(folder):
    counties, places = {}, {}
    with open(os.path.join(folder, "national_county2020.txt"), encoding="latin-1") as f:
        next(f)
        for line in f:
            p = line.rstrip("\n").split("|")
            counties[(p[1], p[2])] = p[4]
    suffix = re.compile(r" (city|town|village|borough|CDP|municipality|city and borough|metro government"
                        r"|unified government|consolidated government|urban county|plantation|comunidad"
                        r"|zona urbana)( \(balance\))?$")
    with open(os.path.join(folder, "national_place2020.txt"), encoding="latin-1") as f:
        next(f)
        for line in f:
            p = line.rstrip("\n").split("|")
            places[(p[1], p[2])] = plain(suffix.sub("", p[4]))
    return counties, places
