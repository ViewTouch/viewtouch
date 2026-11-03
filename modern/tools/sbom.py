#!/usr/bin/env python3
"""What a release is made of (a software bill of materials, SPDX 2.3 JSON):

    tools/sbom.py OUT.spdx.json PACKAGE.deb...

ViewTouch itself (its version, the commit, each package's checksum) and
what each Debian/Ubuntu package needs from the system (Qt, OpenSSL,
SQLite... as its Depends say), so a store or a security notice can tell
whether a release is affected by a library's problem. ViewTouch bundles
no third-party code in what it installs (Catch2 is for its tests only).
"""
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import uuid


def field(deb, name):
    return subprocess.run(["dpkg-deb", "--field", deb, name], capture_output=True, text=True).stdout.strip()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out, debs = sys.argv[1], sys.argv[2:]
    commit = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    packages, relations, seen = [], [], {}
    for deb in debs:
        name = os.path.basename(deb)
        version = field(deb, "Version")
        spdx = "SPDXRef-" + re.sub(r"[^A-Za-z0-9.-]", "-", name)
        packages.append({
            "SPDXID": spdx, "name": "vtmodern", "versionInfo": version, "packageFileName": name,
            "downloadLocation": "https://github.com/ViewTouch/viewtouch/releases",
            "sourceInfo": f"git commit {commit}", "licenseConcluded": "GPL-3.0-or-later",
            "licenseDeclared": "GPL-3.0-or-later", "copyrightText": "NOASSERTION", "filesAnalyzed": False,
            "checksums": [{"algorithm": "SHA256", "checksumValue": hashlib.sha256(open(deb, "rb").read()).hexdigest()}],
            "supplier": "Organization: ViewTouch",
        })
        relations.append({"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES", "relatedSpdxElement": spdx})
        for dep in filter(None, (d.strip() for d in field(deb, "Depends").split(","))):
            first = dep.split("|")[0].strip()
            dname = first.split(" ")[0].split(":")[0]
            constraint = first[len(dname):].strip(" ()") or "NOASSERTION"
            key = (dname, constraint)
            if key not in seen:
                seen[key] = "SPDXRef-dep-" + re.sub(r"[^A-Za-z0-9.-]", "-", f"{dname}-{len(seen)}")
                packages.append({"SPDXID": seen[key], "name": dname, "versionInfo": constraint,
                                 "downloadLocation": "NOASSERTION", "licenseConcluded": "NOASSERTION",
                                 "licenseDeclared": "NOASSERTION", "copyrightText": "NOASSERTION",
                                 "filesAnalyzed": False, "supplier": "NOASSERTION"})
            relations.append({"spdxElementId": spdx, "relationshipType": "DEPENDS_ON", "relatedSpdxElement": seen[key]})
    doc = {
        "spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"ViewTouch Modern {packages[0]['versionInfo'] if packages else ''}",
        "documentNamespace": f"https://github.com/ViewTouch/viewtouch/sbom/{commit}/{uuid.uuid4()}",
        "creationInfo": {"created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "creators": ["Tool: tools/sbom.py"]},
        "packages": packages, "relationships": relations,
    }
    with open(out, "w") as f:
        json.dump(doc, f, indent=1)
    print(f"{out}: {len(debs)} packages, {len(seen)} things they need from the system")


if __name__ == "__main__":
    main()
