#!/bin/sh
# Set up this fork's own release channel, so its store computers update from
# its own releases and trust only packages it signs:
#
#   packaging/release-channel.sh OWNER/REPO [RELEASE-TAG]
#
# - makes a signing key on this computer if there isn't one
#   (~/.config/viewtouch/signing/update-signing-key.pem, or VTM_SIGNING_KEY;
#   never put it in the repository, and keep a copy somewhere safe and offline)
# - writes its public half to packaging/update-signing-key.pub.pem
# - points release-channel.conf and install-viewtouch.sh at
#   https://github.com/OWNER/REPO/releases/download/RELEASE-TAG
#   (modern-packages to start)
# Then commit those three files, build, and publish with packaging/publish.sh.
set -eu

[ $# -ge 1 ] || { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }
repo=$1
tag=${2:-modern-packages}
printf '%s' "$repo" | grep -Eq '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$' || { echo "OWNER/REPO, like someone/viewtouch" >&2; exit 1; }
printf '%s' "$tag" | grep -Eq '^[A-Za-z0-9_.-]+$' || { echo "A release tag has letters, digits, . _ -" >&2; exit 1; }
here=$(cd "$(dirname "$0")" && pwd)
key=${VTM_SIGNING_KEY:-$HOME/.config/viewtouch/signing/update-signing-key.pem}

if [ ! -f "$key" ]; then
    mkdir -p "$(dirname "$key")"
    chmod 700 "$(dirname "$key")"
    (umask 077 && openssl genpkey -algorithm ed25519 -out "$key")
    echo "New signing key: $key (keep a copy somewhere safe and offline)"
fi
openssl pkey -in "$key" -pubout -out "$here/update-signing-key.pub.pem"

url=https://github.com/$repo/releases/download/$tag
sed -i "s|^url=.*|url=$url|" "$here/release-channel.conf"

# The installer: its download page, its source, and the key it checks with.
inst=$here/install-viewtouch.sh
sed -i -e "s|^REPO=\${VTM_REPO:-[^}]*}|REPO=\${VTM_REPO:-https://github.com/$repo.git}|" \
       -e "s|^PACKAGES=\${VTM_PACKAGES:-[^}]*}|PACKAGES=\${VTM_PACKAGES:-$url}|" \
       -e "s|raw.githubusercontent.com/[^/]*/[^/]*/|raw.githubusercontent.com/$repo/|" "$inst"
python3 - "$inst" "$here/update-signing-key.pub.pem" <<'PY'
import re, sys
inst, pub = sys.argv[1], open(sys.argv[2]).read().strip()
s = open(inst).read()
s, n = re.subn(r"-----BEGIN PUBLIC KEY-----\n.*?\n-----END PUBLIC KEY-----", lambda m: pub, s, flags=re.S)
assert n == 1, "install-viewtouch.sh: no key block"
open(inst, "w").write(s)
PY
echo "Release channel: $url"
echo "Commit packaging/release-channel.conf, packaging/update-signing-key.pub.pem and packaging/install-viewtouch.sh."
