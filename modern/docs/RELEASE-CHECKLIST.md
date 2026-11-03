# Before a release: the checks only real hardware can do

The tests run thousands of checks on every push, but they never touch a
receipt printer, a cash drawer, a card reader, a touch screen, a Raspberry
Pi or a TV. These do. Run them on the release build (the packages the
Modern release workflow made, installed with `install-viewtouch.sh`), and
put the results in the release notes. About an hour.

**Qt first.** ViewTouch carries its own Qt, so Qt's security fixes reach
stores only through a ViewTouch release. Before each release, look at Qt's
releases and security advisories (qt.io/blog, download.qt.io/official_releases/qt):
a newer patch release (6.12.1...) or a newer version means changing
`tools/qt-version` (and `QT_VERSION` in the Android and sanitizer
workflows), building and running the tests, then this checklist. A
security advisory for a part of Qt ViewTouch uses (networking, TLS, images,
SQLite, QML) is a reason for a release of its own.

Mark each: **✓** worked, **✗** didn't (open an issue, and don't approve the
publish until it's fixed or understood), **–** not available this time.

## The store's computers

| | Check | How |
|---|---|---|
| ☐ | Fresh install on a Raspberry Pi 5 (Raspberry Pi OS, 64-bit) | `install-viewtouch.sh`, touch setup as a main register: it comes up on the home screen, then ViewTouch |
| ☐ | Upgrade on a Pi running the last release | Home screen → Updates → Check (on the beta channel: `channel=beta` in `/etc/viewtouch/updates.conf`), install: the store's checks, staff and menu all there afterwards |
| ☐ | The update's health check | 15 minutes after the upgrade, `sudo journalctl -u 'vtmodern-update-verify*'` says `OK` |
| ☐ | A rollback, on purpose | On a spare computer: `sudo systemctl stop vtmodern-kiosk vtmodern-station` right after an update and wait 15 minutes: it goes back to the version before, with its data, and the top bar says so |
| ☐ | A PC (Debian 13 or Ubuntu 26.04, amd64) | Same install; a second register joins the main one with a pairing code |
| ☐ | Power cut in the middle of a sale | Pull the plug on the main register with a check open and a payment half done: after it starts, the check is there, nothing paid twice, Store Check says it closed by itself |
| ☐ | A week's worth of End of Day | Seven End of Days on the main register (demo data, `--demo-data`): the backup each night, Store Check clean (no books, drill or integrity problem) |

## Touch screens

| | Check | How |
|---|---|---|
| ☐ | Touch lands where it should | Every corner button on the order page, the keypad, the on-screen keyboard |
| ☐ | A screen mounted sideways | Home screen → Display → turn it: touch still lands right |
| ☐ | Brightness | Home screen → Display: the backlight follows (official Pi screen) |
| ☐ | Fat fingers | Ring 20 items fast on the order page: no double adds, no missed touches |

## Printers and the cash drawer

| | Check | How |
|---|---|---|
| ☐ | Network receipt printer (Epson TM-m30 or similar) | Manager → Printers → the printer → **Test Print**: logo, text, the ruler, cut |
| ☐ | Kitchen printer | Send an order: the ticket at the right station, modifiers and allergies on it |
| ☐ | Out of paper | Open the printer's cover mid-service: Store Check and the screen say so within a minute; the ticket prints once it's closed, once |
| ☐ | Printer unplugged | Send an order with the kitchen printer off: the order isn't lost, the server is told, it prints when the printer is back |
| ☐ | Cash drawer | **Test Print + Open Drawer**; a cash sale opens it; No Sale opens it and is in the report |
| ☐ | USB/CUPS printer, if a store uses one | Test page through CUPS |

## Card readers (Stripe)

| | Check | How |
|---|---|---|
| ☐ | The reader (a Stripe Reader S700 running ViewTouch, Stripe test mode) | Manager → Terminals → *Card reader* → **Stripe smart reader (this device)**: it connects |
| ☐ | A card payment with a tip | Test card: approved, tip on the check, receipt says so |
| ☐ | A declined card | Stripe's decline test card: the check stays open, the server is told why |
| ☐ | A refund | Refund the payment above: in Stripe's dashboard, and in the day's refunds |
| ☐ | Internet down mid-payment | Unplug the router while a card is on the reader: it says so; once back, the payment is either on the check or clearly not taken (never both, never twice) |
| ☐ | Bar tab hold | Open a tab with a card held, close it with a tip |

## Tablets, the kitchen screen and the board

| | Check | How |
|---|---|---|
| ☐ | Android tablet (arm64), fresh | Install the APK from the release, pair, take an order at a table |
| ☐ | Older tablet (armeabi-v7a) | Same |
| ☐ | Tablet update | With an older app paired: the strip offers the new one, Install works |
| ☐ | Out of Wi-Fi range | Walk a tablet out of range with an order on it: it says Reconnecting, and the order goes through once back, once |
| ☐ | Kitchen display | Orders arrive, bump, recall; the timers go amber and red |
| ☐ | Now Serving on a TV | Order numbers move from Preparing to Ready; the chime plays; readable from across the room |
| ☐ | Customer display (second monitor) | The check and total follow the register; the tip screen works by touch |

## The standby server

| | Check | How |
|---|---|---|
| ☐ | Failover | Unplug the main register's network mid-service: within 15 seconds the standby takes over, tablets follow, nothing lost |
| ☐ | The main one comes back | Plug it back in: it becomes the standby, not a second server |

---

Who ran it: ______________ Date: __________ Release: __________
