#!/usr/bin/env python3
"""The store computer's updater (packaging/vtmodern-device) from start to
finish, without root or a real computer: a copy of it whose system paths
point into a temporary folder, with pretend apt-get, dpkg, curl, systemctl
and ViewTouch. Real openssl signs and checks.

    python3 packaging/tests/test_updates.py
"""
import json
import os
import pathlib
import shutil
import stat
import subprocess
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
DEVICE = os.path.join(HERE, "..", "vtmodern-device")

FAKES = {
    # curl -fsSL ... -o OUT URL: the file from the pretend release page.
    "curl": r'''#!/bin/sh
out=; url=
while [ $# -gt 0 ]; do case "$1" in -o) out=$2; shift ;; https://*) url=$1 ;; esac; shift; done
cp "$ROOT/page/${url##*/}" "$out" 2>/dev/null
''',
    "dpkg-query": '#!/bin/sh\ncat "$ROOT/state/installed"\n',
    # A pretend package is a text file: "Version: 0.15.1571-1~debian13".
    "dpkg-deb": '#!/bin/sh\nsed -n "s/^Version: //p" "$2"\n',
    "dpkg": '#!/bin/sh\necho amd64\n',
    "apt-get": r'''#!/bin/sh
echo "apt-get $*" >> "$ROOT/state/log"
for a; do case "$a" in *.deb) sed -n "s/^Version: //p" "$a" > "$ROOT/state/installed" ;; esac; done
[ -f "$ROOT/state/apt-fails" ] && exit 1
exit 0
''',
    "systemctl": r'''#!/bin/sh
echo "systemctl $*" >> "$ROOT/state/log"
[ "$1" = is-active ] || exit 0
for u in $(cat "$ROOT/state/active" 2>/dev/null); do [ "$u" = "$3" ] && exit 0; done
exit 3
''',
    "systemd-run": '#!/bin/sh\necho "systemd-run $*" >> "$ROOT/state/log"\n',
    "runuser": '#!/bin/sh\nshift 3\nexec "$@"\n',   # runuser -u viewtouch -- CMD...
    "vtmodern": r'''#!/bin/sh
while [ $# -gt 0 ]; do case "$1" in --snapshot) cp "$ROOT/var/lib/viewtouch/viewtouch.db" "$2" ;; esac; shift; done
''',
    "id": '#!/bin/sh\nexit 0\n',
    "chown": '#!/bin/sh\nexit 0\n',
    # install without -o/-g (not root here).
    "install": r'''#!/bin/sh
for a; do shift; case "$skip$a" in -o|-g) skip=1 ;; 1*) skip= ;; *) set -- "$@" "$a" ;; esac; done
exec /usr/bin/install "$@"
''',
}


class Store:
    """A pretend store computer and the release page it updates from."""

    def __init__(self, test):
        self.root = tempfile.mkdtemp()
        test.addCleanup(shutil.rmtree, self.root)
        r = self.root
        for d in ("fake", "state", "page", "etc/viewtouch", "usr/share/vtmodern", "var/lib/viewtouch",
                  "usr/bin", "usr/libexec/vtmodern"):
            os.makedirs(os.path.join(r, d))
        for name, body in FAKES.items():
            self.write("fake/" + name, body, exe=True)
        self.write("usr/bin/vtmodern", FAKES["vtmodern"], exe=True)
        self.write("etc/os-release", "ID=debian\nVERSION_ID=13\n")
        self.write("etc/machine-id", "0123456789abcdef0123456789abcdef\n")
        self.write("usr/share/vtmodern/release-channel.conf", "url=https://example.com/releases/download/modern-packages\n")
        self.key = os.path.join(r, "signing.pem")
        subprocess.run(["openssl", "genpkey", "-algorithm", "ed25519", "-out", self.key], check=True,
                       capture_output=True)
        subprocess.run(["openssl", "pkey", "-in", self.key, "-pubout", "-out",
                        os.path.join(r, "usr/share/vtmodern/update-signing-key.pub.pem")], check=True)
        # The updater, its paths in here, run without root.
        s = pathlib.Path(DEVICE).read_text()
        s = s.replace("PATH=/usr/sbin:/usr/bin:/sbin:/bin", f"PATH={r}/fake:/usr/sbin:/usr/bin:/sbin:/bin")
        s = s.replace('[ "$(id -u)" -eq 0 ] || die "Run this with sudo."', "")
        for path in ("/var/lib/vtmodern-updates", "/var/lib/viewtouch", "/etc/viewtouch", "/usr/share/vtmodern",
                     "/run/vtmodern-online", "/etc/machine-id", "/etc/os-release", "/usr/bin/vtmodern",
                     "/usr/libexec/vtmodern"):
            s = s.replace(path, r + path)
        self.write("device", s, exe=True)
        self.installed("0.15.1571-1~debian13")
        self.write("var/lib/viewtouch/viewtouch.db", "the store before\n")

    def write(self, rel, text, exe=False):
        path = os.path.join(self.root, rel)
        with open(path, "w") as f:
            f.write(text)
        if exe:
            os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)
        return path

    def read(self, rel):
        path = os.path.join(self.root, rel)
        return pathlib.Path(path).read_text() if os.path.exists(path) else None

    def installed(self, version):
        self.write("state/installed", version + "\n")

    def active(self, *units):
        self.write("state/active", " ".join(units) + "\n")

    def sign(self, rel):
        path = os.path.join(self.root, rel)
        subprocess.run(["openssl", "pkeyutl", "-sign", "-inkey", self.key, "-rawin", "-in", path, "-out",
                        path + ".sig"], check=True)

    def publish(self, version, hours_ago=0.0, rollout=None):
        """latest.json for VERSION (and its package), published HOURS_AGO."""
        published = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(time.time() - hours_ago * 3600))
        doc = {"version": version, "published": published,
               "packages": {"debian13-amd64": "vtmodern-debian13-amd64.deb"}, "apps": {}}
        if rollout is not None:
            doc["rollout"] = rollout
        self.write("page/latest.json", json.dumps(doc, indent=2))
        self.sign("page/latest.json")
        self.write("page/vtmodern-debian13-amd64.deb", f"Version: {version}-1~debian13\n")
        self.sign("page/vtmodern-debian13-amd64.deb")

    def run(self, *args):
        env = dict(os.environ, ROOT=self.root)
        p = subprocess.run(["sh", os.path.join(self.root, "device"), *args], capture_output=True, text=True, env=env)
        return p.returncode, (p.stdout + p.stderr).strip()

    def last_line(self, *args):
        code, out = self.run(*args)
        return code, out.splitlines()[-1] if out else ""


class Rollout(unittest.TestCase):
    def test_no_rollout_in_latest_json_is_taken_at_once(self):
        s = Store(self)
        s.publish("0.16.1600")
        self.assertEqual(s.last_line("online-check"), (0, "READY 0.16.1600 0.15.1571"))

    def test_just_published_waits_a_day_except_on_the_beta_channel(self):
        s = Store(self)
        s.publish("0.16.1600", rollout={"betaHours": 24, "spreadHours": 72, "paused": False})
        code, line = s.last_line("online-check")
        self.assertEqual(code, 0)
        self.assertTrue(line.startswith("SOON 0.16.1600 "), line)
        code, out = s.run("online-update")
        self.assertIn("SOON", out)
        self.assertEqual(s.read("state/installed").strip(), "0.15.1571-1~debian13")   # nothing installed
        s.write("etc/viewtouch/updates.conf", "channel=beta\n")
        self.assertEqual(s.last_line("online-check"), (0, "READY 0.16.1600 0.15.1571"))

    def test_after_the_beta_day_and_the_spread_every_computer_takes_it(self):
        s = Store(self)
        s.publish("0.16.1600", hours_ago=24 + 72 + 1, rollout={"betaHours": 24, "spreadHours": 72, "paused": False})
        self.assertEqual(s.last_line("online-check"), (0, "READY 0.16.1600 0.15.1571"))

    def test_computers_spread_over_the_days(self):
        # Many computers: each its own place, between the beta day and its end.
        rollout = {"betaHours": 24, "spreadHours": 72, "paused": False}
        s = Store(self)
        s.publish("0.16.1600", rollout=rollout)
        published = json.loads(s.read("page/latest.json"))["published"]
        start = time.mktime(time.strptime(published, "%Y-%m-%dT%H:%M:%SZ")) - time.timezone
        days = []
        for n in range(40):
            s.write("etc/machine-id", f"{n:032x}\n")
            code, line = s.last_line("online-check")
            due = time.mktime(time.strptime(line.split()[2], "%Y-%m-%dT%H:%M"))
            days.append((due - start) / 3600)
        self.assertTrue(all(24 - 1 <= h <= 24 + 72 + 1 for h in days), days)
        self.assertGreater(max(days) - min(days), 36)   # not all on one night

    def test_paused_nobody_takes_it(self):
        s = Store(self)
        s.publish("0.16.1600", hours_ago=500, rollout={"betaHours": 0, "spreadHours": 0, "paused": True})
        self.assertEqual(s.last_line("online-check"), (0, "PAUSED 0.16.1600 0.15.1571"))
        s.write("etc/viewtouch/updates.conf", "channel=beta\n")
        self.assertEqual(s.last_line("online-check"), (0, "PAUSED 0.16.1600 0.15.1571"))

    def test_a_latest_json_changed_on_the_way_is_refused(self):
        s = Store(self)
        s.publish("0.16.1600", rollout={"betaHours": 24, "spreadHours": 72, "paused": False})
        doc = json.loads(s.read("page/latest.json"))
        doc["rollout"]["betaHours"] = 0
        s.write("page/latest.json", json.dumps(doc))
        code, out = s.run("online-check")
        self.assertNotEqual(code, 0)
        self.assertIn("isn't signed", out)

    def test_publish_rollout_pause_now_signs_it_again(self):
        s = Store(self)
        s.publish("0.16.1600", rollout={"betaHours": 24, "spreadHours": 72, "paused": False})
        env = dict(os.environ, VTM_SIGNING_KEY=s.key, VTM_LATEST_JSON=os.path.join(s.root, "page/latest.json"),
                   VTM_SIGNING_PUB=os.path.join(s.root, "usr/share/vtmodern/update-signing-key.pub.pem"))
        for what, want in (("pause", "PAUSED"), ("now", "READY")):
            p = subprocess.run(["sh", os.path.join(HERE, "..", "publish.sh"), "--rollout", what, "--dry-run"],
                               capture_output=True, text=True, env=env)
            self.assertEqual(p.returncode, 0, p.stderr)
            body = p.stdout[p.stdout.index("{"):p.stdout.rindex("}") + 1]
            s.write("page/latest.json", body + "\n")
            s.sign("page/latest.json")
            self.assertEqual(s.last_line("online-check")[1].split()[0], want)


class Rollback(unittest.TestCase):
    def updated(self, kept_package=True):
        """0.15 running (with its package kept), 0.16 installed online."""
        s = Store(self)
        s.active("vtmodern-kiosk.service")
        if kept_package:
            os.makedirs(os.path.join(s.root, "var/lib/vtmodern-updates"))
            s.write("var/lib/vtmodern-updates/installed.deb", "Version: 0.15.1571-1~debian13\n")
        s.publish("0.16.1600")
        code, out = s.run("online-update")
        self.assertEqual(code, 0, out)
        self.assertEqual(out.splitlines()[-1], "INSTALLED 0.16.1600")
        self.assertEqual(s.read("state/installed").strip(), "0.16.1600-1~debian13")
        return s

    def test_an_update_keeps_what_to_go_back_to_and_checks_in_15_minutes(self):
        s = self.updated()
        roll = "var/lib/vtmodern-updates/"
        self.assertEqual(s.read(roll + "previous.deb"), "Version: 0.15.1571-1~debian13\n")
        self.assertEqual(s.read(roll + "installed.deb"), "Version: 0.16.1600-1~debian13\n")
        self.assertEqual(s.read(roll + "snapshot.db"), "the store before\n")
        pending = s.read(roll + "pending")
        self.assertIn("from=0.15.1571-1~debian13", pending)
        self.assertIn("to=0.16.1600-1~debian13", pending)
        self.assertIn("vtmodern-kiosk.service", pending)
        self.assertIn("--on-active=15min", s.read("state/log"))
        self.assertIn("update-verify", s.read("state/log"))

    def test_the_new_one_came_up_nothing_undone(self):
        s = self.updated()
        s.write("var/lib/viewtouch/healthy", "0.16.1600\n")
        self.assertEqual(s.last_line("update-verify"), (0, "OK 0.16.1600"))
        self.assertIsNone(s.read("var/lib/vtmodern-updates/pending"))
        self.assertIsNone(s.read("var/lib/vtmodern-updates/previous.deb"))
        self.assertEqual(s.read("state/installed").strip(), "0.16.1600-1~debian13")
        self.assertIsNone(s.read("var/lib/viewtouch/update-result"))

    def test_the_new_one_never_came_up_back_to_the_one_before_with_its_data(self):
        s = self.updated()
        s.write("var/lib/viewtouch/healthy", "0.15.1571\n")    # only the old one's mark
        s.write("var/lib/viewtouch/viewtouch.db", "the store the new one changed\n")
        s.write("var/lib/viewtouch/viewtouch.db-wal", "wal\n")
        self.assertEqual(s.last_line("update-verify"), (0, "BACK 0.15.1571"))
        self.assertEqual(s.read("state/installed").strip(), "0.15.1571-1~debian13")
        log = s.read("state/log")
        self.assertIn("apt-get install -y --allow-downgrades", log)
        self.assertLess(log.index("systemctl stop vtmodern-kiosk.service"), log.index("--allow-downgrades"))
        self.assertGreater(log.rindex("systemctl start vtmodern-kiosk.service"), log.index("--allow-downgrades"))
        # The data as it was; the new one's kept with the backups.
        self.assertEqual(s.read("var/lib/viewtouch/viewtouch.db"), "the store before\n")
        self.assertIsNone(s.read("var/lib/viewtouch/viewtouch.db-wal"))
        aside = os.listdir(os.path.join(s.root, "var/lib/viewtouch/backups"))
        self.assertEqual(len([f for f in aside if f.endswith("-after-failed-update.db")]), 1, aside)
        self.assertEqual(len([f for f in aside if f.endswith("-after-failed-update.db-wal")]), 1, aside)
        # The home screen says so; this build isn't taken again, a newer one is.
        self.assertTrue(s.read("var/lib/viewtouch/update-result").startswith("ROLLED-BACK 0.16.1600 0.15.1571 "))
        self.assertEqual(s.last_line("online-check"), (0, "HELD 0.16.1600 0.15.1571"))
        s.publish("0.16.1610")
        self.assertEqual(s.last_line("online-check"), (0, "READY 0.16.1610 0.15.1571"))
        # And can go back again from that one: 0.15's package is kept.
        self.assertEqual(s.read("var/lib/vtmodern-updates/installed.deb"), "Version: 0.15.1571-1~debian13\n")

    def test_no_copy_of_the_one_before_says_so(self):
        s = self.updated(kept_package=False)
        code, out = s.run("update-verify")
        self.assertNotEqual(code, 0)
        self.assertIn("no copy of 0.15.1571", out)
        self.assertTrue(s.read("var/lib/viewtouch/update-result").startswith("FAILED 0.16.1600 0.15.1571 "))
        self.assertEqual(s.read("state/installed").strip(), "0.16.1600-1~debian13")

    def test_nothing_running_nothing_to_check(self):
        s = Store(self)
        s.publish("0.16.1600")
        self.assertEqual(s.run("online-update")[0], 0)
        self.assertIsNone(s.read("var/lib/vtmodern-updates/pending"))
        self.assertNotIn("systemd-run", s.read("state/log") or "")
        self.assertEqual(s.last_line("update-verify"), (0, "No update to check."))


if __name__ == "__main__":
    unittest.main()
