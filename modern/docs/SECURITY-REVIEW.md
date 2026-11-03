# Security review, November 2026

What an attacker could reach in a store running ViewTouch Modern 0.15, what
stops them, and what's left. Done by reading the code (and the tests that
attack it: tests/test_fuzz.cpp, tests/test_net.cpp, tests/test_safety.cpp).
**A review by someone who didn't write the code is still worth having**,
before ViewTouch takes cards itself or runs outside a store's own network.

## What there is to attack

| Way in | Who can reach it | What protects it |
|---|---|---|
| The store's TCP port (terminals, tablets, the standby) | Anyone on the store's network | TLS 1.2 with a pre-shared key (ECDHE-PSK-CHACHA20-POLY1305): no key, no connection. Each device has its own random 256-bit key; removing it in Manager → Terminals revokes it |
| Pairing a new device | Anyone on the network, while a manager has a code open | A 10-character code (32 symbols each: about 50 bits) from the system's secure random generator, valid 10 minutes, stretched with PBKDF2-SHA256 (600,000 rounds) into the TLS key. Guessing over the network is hopeless in 10 minutes; with ECDHE in the handshake a listener can't test guesses offline either |
| Discovery (UDP) | The local network | Says only that a store is there and its name; a fake one can't complete the TLS handshake without the device's key |
| Commands from a paired device | A paired device | Each command checks the logged-in staff member's permissions on the store's computer, never on the device (the fuzzer sends random commands as nobody, a server and a manager: a server never changes settings, staff or the menu) |
| The local control socket (`--pairing-code`) | Programs on the store's computer | Only the ViewTouch account (`QLocalServer::UserAccessOption`) |
| The home screen's root helper (`vtmodern-device`) | The ViewTouch account, through one sudo rule | Every argument checked before use (names, zones, percents); the Wi-Fi password on stdin, never on the command line |
| Updates | Whoever controls the release page, or the network in between | Every package and `latest.json` signed (Ed25519), checked against the key built in; never an older build; file names from `latest.json` checked; the release page must be https. Rollout changes are signed the same way |
| Backups | Whoever gets a USB stick or the backup folder | Optional encryption (AES-256-GCM, key by PBKDF2 from the store's backup password); every backup checked when made, and opened as a store each night |
| Card payments | — | ViewTouch never sees a card number: the Stripe reader takes the card, and ViewTouch keeps Stripe's payment id, the brand and the last four digits |

## Findings

**1. Staff PINs can be recovered from a device's files.** (Accepted, with
care.) PINs are kept as salted SHA-256, and the offline kit that lets a
tablet keep working without the store's computer includes every active
employee's salt and hash, managers' too. A PIN is 4 to 8 digits, so anyone
who copies a paired device's files (or an unencrypted backup) can work out
every PIN in seconds; a slower hash wouldn't change that for 4 digits.
What limits it: the kit only goes to paired devices, over TLS; on Android
it's in the app's private storage; on Linux in `/var/lib/viewtouch` (the
ViewTouch account only). What's never in the kit: the maintainer's code,
the Stripe key, the backup key, the devices' keys, the text-message
webhook. *What a store should do:* encrypt backups, keep devices locked,
and remove a lost tablet in Manager → Terminals at once (its key stops
working; change the PINs of the managers who used it). *Worth doing later:*
leave managers out of the kit unless the store wants managers to approve
things offline.

**2. The Stripe secret key is on the store's computer, in its database.**
(Accepted.) It has to be: refunds and reader connection tokens need it. It
never leaves that computer (not in the offline kit, not in the Changes
report, shown only as its last four characters). Anyone with the store
computer's files has it, so: a *restricted* Stripe key with only what
ViewTouch uses, and backups encrypted.

**3. The store's network is trusted for discovery.** (Accepted.) A device
on the network can pretend to be the store when a terminal looks for it;
it gets nowhere (no key), but the terminal shows "Reconnecting…" until the
real one answers. A store with guests on its network should give them
their own Wi-Fi.

**4. Nothing else found** in: the TLS setup (no fallback to plain TCP),
pairing (one code, one device, ten minutes), the update path (signature
before anything is installed, `apt-get` given a local file whose version
was checked), the root helper's argument checks, or the backup
encryption.

## Payment card rules (PCI DSS)

Card data never passes through ViewTouch, so a store's PCI scope is
Stripe's reader and Stripe's account, not ViewTouch: Stripe documents what
a merchant using Terminal fills in (usually SAQ B-IP or C). ViewTouch keeps
Stripe's payment id, the card brand and the last four digits, which PCI
allows. If ViewTouch ever reads cards itself, this whole section changes.

## Done with this review

- Dependabot keeps the workflows' actions up to date (`.github/dependabot.yml`).
- CodeQL scans the C++ and the workflows on every push (Modern static analysis).
- Each release lists every file's checksum (`SHA256SUMS`) and is signed.
- To turn on in the repository's settings (Settings → Code security):
  **secret scanning** and **push protection**, so a key pushed by mistake
  is caught.
