# ViewTouch Modern

A ground-up rewrite of ViewTouch in C++23 and Qt6/QML. It lives on the `Modernization` branch. The legacy code in the repo root stays as a reference until the new app reaches feature parity.

The full design and milestones are in [docs/PLAN.md](docs/PLAN.md).

## What carries over from ViewTouch

- **Pages are made of freely placed buttons (zones).** The owner can move, resize, restyle and rewire every button in an in-app **edit mode**.
- **Template pages.** A page can inherit the buttons of a template page, for example an order bar shared by every menu page.
- **Style inheritance.** Button styles resolve in this order: zone → page → theme. Each button has normal, selected and disabled looks.
- **Modifier page sequences.** For example, "Steak" → Temperature → Sides. Meal-period index pages and index tabs work as before.

## What changes

| Legacy | Modern |
|---|---|
| X11/Motif, the server draws every pixel | Qt Quick (GPU); the UI runs locally on each terminal |
| Fixed pixel page sizes | Logical canvas scaled to any screen |
| Positional `.dat` files | Versioned JSON pages and SQLite (WAL, transactional) |
| Negative magic page IDs | Page `role`s (`login`, `tables`, `settle`, …) |
| About 75 zone types | One `button` with an action list, plus about 20 widgets |
| Motif dialogs, no undo | Inspector panel, snap and guides, full undo/redo |
| Starter pages downloaded at runtime | Seed pages shipped in `seed/` |
| All I/O on one event loop | Printing and storage on worker threads |

## Status

| Milestone | State |
|---|---|
| M0 Scaffold | done |
| M1 Layout engine: pages, templates, styles, navigation, starter pages | done |
| M2 Page editor | done |
| M3 Core POS flow (login → order → pay) | done |
| M4 Printing, drawers, reports, end of day, admin screens, split check | done |
| M5 Several terminals on one server, kitchen display, takeout/delivery details | done |
| M6 Tips, party gratuity, per-terminal drawers and printers, pay outs / paid ins, tip cash-out | done |
| M7 Packages (Fedora, Debian/Raspberry Pi OS, Ubuntu), service and kiosk at boot, backups, meal periods | done |
| M8 Card payments | last |

## Installing

Works on Fedora 44, Debian 13 (and Raspberry Pi OS based on it) and Ubuntu 26.04, on x86_64 and aarch64 (64-bit Raspberry Pi). It needs Qt 6.8 or newer, so older releases (Debian 12, Ubuntu 24.04) are not supported.

**A new machine, from zero:** one command installs everything.

```sh
curl -fsSL https://raw.githubusercontent.com/No0ne558/viewtouchFork/Modernization/modern/packaging/install-viewtouch.sh | sudo sh
```

It asks one question:

1. **Only for ViewTouch** (registers, kitchen screens, kiosks, time clocks; recommended). It installs ViewTouch, locks the computer down (`vtmodern-lockdown`, below) and restarts. The screen then asks, by touch, what this computer is, its name, the network, the time zone and a settings PIN, and starts ViewTouch. To answer ahead of time: `... | sudo sh -s -- --station` (add `--keep-ssh` to leave SSH on).
2. **Next to the desktop**: it installs ViewTouch and runs `vtmodern-setup` (below), a text menu.

On Debian 13 and Raspberry Pi OS (64-bit) it downloads the ready-made package from the [modern-packages](https://github.com/No0ne558/viewtouchFork/releases/tag/modern-packages) release (a minute). Elsewhere, or with `--build`, it fetches the source and builds a package for this machine (a few minutes). With a package already at hand: `sudo sh install-viewtouch.sh vtmodern-*.rpm` (or `.deb`). To skip the questions next to a desktop, put the setup after `--`: `sudo sh install-viewtouch.sh -- kitchen auto Grill`.

### A computer only for ViewTouch

`sudo vtmodern-lockdown` (run by the installer's first choice) turns a fresh Raspberry Pi OS or Debian 13 into one:

- **The home screen** (`vtmodern --station`, `vtmodern-station.service`) instead of the desktop. It starts ViewTouch and starts it again if it stops by itself (after 5 seconds; after 3 stops in 3 minutes it stays on the home screen and says so). When ViewTouch is closed (Manager → Close ViewTouch) the home screen shows its icons: **Start ViewTouch**; on the main register **Setup Guide, Menu, Staff, Printers, Add a Screen, Reports, Back Up Now** (each opens ViewTouch there after a manager logs in, `--after-login`); and for the computer **Network** (Wi-Fi with an on-screen keyboard), **Date & Time**, **Updates** (everything now, or a new ViewTouch from a USB stick), **Remote Help** (SSH on or off), **Restart**, **Shut Down**, **Reset…** (start over and choose again what the computer is, or erase the store) and **About**. The settings icons ask for the computer's PIN.
- **Its own account:** ViewTouch runs as `viewtouch` (or `vtpos`, when `viewtouch` is somebody's login), which can't log in and may run only `/usr/libexec/vtmodern/vtmodern-device` as root. That helper does the home screen's Wi-Fi, time zone, updates, restart and setup, and checks every value.
- **Locked down:** no desktop login; a firewall lets in only ViewTouch (7719), finding it (mDNS) and SSH; SSH is off until Remote Help turns it on; Ctrl+Alt+Del does nothing.
- **Kept running:** security updates install each night, the hardware watchdog restarts a frozen computer, logs are kept under 200 MB, and the boot is quiet.
- **Your administrator account** still works: Ctrl+Alt+F2 on the screen for a text login, or SSH while Remote Help is on. `sudo vtmodern-lockdown --undo` brings the desktop back.

### Next to the desktop

Then it runs **`sudo vtmodern-setup`**, which asks what this machine is (any time later too, to change it):

| Choice | Command | This machine |
|---|---|---|
| Main register | `vtmodern-setup store` | Has a screen and keeps the store's data; other screens join it |
| Back-office server | `vtmodern-setup server` | Keeps the data with no screen (back office, closet box) |
| Another register | `vtmodern-setup terminal [host] [name]` | A register that joins the store |
| Kitchen screen | `vtmodern-setup kitchen [host] [name]` | Opens on the orders to make; its Station button picks what it shows |
| Self-order kiosk | `vtmodern-setup selforder [host] [name]` | Guests order on their own and pay at the counter |
| Time clock | `vtmodern-setup timeclock [host] [name]` | Staff clock in and out and see their schedules |
| Standby server | `vtmodern-setup standby CODE [host]` | A live copy of the data that takes over if the main machine stops (pairs once with the code) |
| Nothing | `vtmodern-setup off` | ViewTouch doesn't start at boot |

`host` is the main machine's address, or `auto` (the default) to find it on the network; `name` is the screen's name (the default: the computer's). Screens that join show the Join screen the first time: pick the store and type a pairing code from Manager → Terminals → **Pair a Device** (on a server with no screen: `sudo vtmodern-setup pair`). `--guest-display` adds a guest-facing second monitor (registers); `--no-desktop` turns the desktop login off. `vtmodern-setup status` shows what is set up and the newest backup.

- **Screens start full screen at boot** (`vtmodern-kiosk.service`) in the `cage` kiosk on the first console, with no desktop and no mouse pointer. To leave it, a manager uses **Manager → Close ViewTouch** (touch it twice). The kiosk then stays closed until the next boot and the screen switches to the second console: your desktop, or a text login on a dedicated register. A technician can also switch consoles with Ctrl+Alt+F2 (add Fn on keyboards whose F-keys need it). Turn off any desktop login (`sudo systemctl disable gdm`) on a dedicated POS screen. On a desktop, *ViewTouch* is also in the applications menu.
- **The server** (`vtmodern.service`) runs sandboxed as the `viewtouch` account and restarts if it stops. Open port 7719 (TCP and UDP) in the firewall for the terminals.
- **Settings** live in `/etc/viewtouch/kiosk.conf` and `/etc/viewtouch/server.conf`: any `vtmodern --help` option, written `option = value`. Restart the service after a change.
- **Data** lives in `/var/lib/viewtouch` and is kept when the package is removed. Only one ViewTouch can use a database at a time; a second one is refused and told to `--connect` instead.
- **Logs:** `journalctl -u vtmodern` or `journalctl -u vtmodern-kiosk`.

**Building the packages:** `modern/packaging/build-packages.sh` builds for this machine; add `fedora`, `debian` or `ubuntu` to build in a podman container. The *Modern packages* GitHub workflow (Actions tab, or a `modern-v*` tag) builds all six.

## Android tablets

The Android app is a terminal: it finds the store's server on the Wi-Fi, pairs with it, and then works like any other terminal (same pages, live updates). It needs Android 9 or newer on a 64-bit or 32-bit ARM tablet.

1. **Get the app:** download it on the tablet or phone from the [modern-packages](https://github.com/No0ne558/viewtouchFork/releases/tag/modern-packages) release: `ViewTouch-android-arm64.apk` (most phones and tablets) or `ViewTouch-android-armv7.apk` (older or budget ones). To build it yourself: the *Modern Android* workflow (Actions tab → Modern Android → Run workflow).
2. **Install:** copy the APK to the tablet and open it. Android asks to allow installing apps from that source (Settings → Apps → Special access → Install unknown apps).
3. **Pair:** open ViewTouch. The Join screen lists the stores on the Wi-Fi. Type a code from Manager → Terminals → **Pair a Device**, name the tablet, and **Join**.

**Phones.** On a phone (a screen whose shorter side is under 600dp, like a folded Fold) the app turns portrait and shows *phone pages*: big buttons in one column. Unfolded or on a tablet it goes back to the standard pages in landscape. Details are under *Phones and small screens*.

The app is full screen and keeps the screen on. Back goes back a page. If the server is out of reach, it starts with the pages it saved last and shows *Reconnecting…*. Its pairing key is never included in Android backups.

**Cards when the internet is down** (`offlineCards`, `offlineCardMax`, `offlineCardTotal`; off by default): only on a Stripe reader running ViewTouch (Apps on Devices, Stripe's offline mode, which `setStripeOffline` turns on in the account's default Terminal configuration). `StripeBridge.charge` requires the internet past the limits (`OfflineBehavior.REQUIRE_ONLINE`); a card taken offline is a payment with `offline = "waiting"` and reference `offline:<id>` (the PaymentIntent's `viewtouch_payment` metadata); `OfflineListener.onPaymentIntentForwarded` → `cardForwarded`: Stripe's `pi_` id, or `declined` and a manager's alert (`cardAlerts`). Not refundable until forwarded. The simulated reader does the same (`setSimulatedOffline`, `simulateForwarded`). The Kotlin side is built by the Android workflow, not tried on a reader here.

**Updating tablets from the store's computer:** every build has a number (the commits so far; the workflow names the app `ViewTouch-b<build>-arm64-v8a.apk`). Copy the new app into the store computer's `updates` folder (in its data folder, `/var/lib/viewtouch/updates` for the packages; Manager → Network shows where). A tablet that's behind shows a strip; a manager there touches **Update Now**: the app comes over the store's own connection (checked by its SHA-256), and Android asks to install it (the first time, to allow ViewTouch to install apps). Linux screens get their package the same way (`-b<build>.deb` / `.rpm`) and install it with the package manager. Apps on a Stripe reader are updated through Stripe (Apps on Devices), not this way.

**Signing:** to install updates over the previous version, every build must be signed with the same key. Make one once and store it in the repository's secrets:
```sh
keytool -genkeypair -keystore viewtouch.keystore -alias viewtouch -keyalg RSA -keysize 3072 -validity 10000
base64 -w0 viewtouch.keystore      # → secret ANDROID_KEYSTORE_BASE64
```
Also add ANDROID_KEYSTORE_ALIAS (`viewtouch`) and ANDROID_KEYSTORE_PASSWORD. Keep the keystore somewhere safe: an app signed with a lost key can't be updated, only reinstalled. Without these secrets, each build gets a throwaway key.

**Building elsewhere:** the workflow uses Qt 6.11.2 for Android (installed with `aqtinstall`), Android NDK r27c, JDK 17 and KDAB's OpenSSL for Android. Android builds need an x86_64 Linux, Windows or macOS machine: the NDK is not made for ARM64 Linux.

## Phones and small screens

Every page is designed on a 1920 × 1080 landscape canvas and scaled to the screen. On a phone that makes buttons too small, so phones get **phone pages** instead:

- **Phone versions.** A page can have a phone version: a separate page on a portrait canvas (1080 × 2280) marked *Phone version of* that page (page inspector → *Phones*). Phones show it instead; buttons and roles keep pointing at the original page. The starter pages include phone versions of Login, the four floor plans (`tables-phone`, `floor-*-phone`: a `floorPlan` panel above the phone's buttons), Guest Count, the order template, Settle, Log Out, Customer and Open Checks.
- **Laid out for you.** A page without a phone version, such as the menu, item and modifier pages, is shown inside the phone version of its template, the phone order screen. That screen has the check on top, qualifiers and Void / Send / Pay at the bottom. The page's own buttons go in the middle in reading order, two or three across, with titles and panels across the full width. So new menu pages work on phones without any extra work. Every other page (Manager screens, reports, the kitchen screen, the Time Clock…) is laid out again on a portrait phone screen the same way, its main panel taking the room that's left; panels that would together run past the bottom give up height together. A test checks that every page fits a portrait screen and none is shown sideways.
- **Panels held upright.** A panel taller than wide stacks its parts: Manager lists show the list, then a record's form on its own with **‹ List**; Customers, Find a Check and the Waitlist work the same way (**‹ Search**, **‹ Results**, **‹ List**); Gift Cards, Messages, Split Check, the Time Clock and the dashboard stack their halves; the schedule shows the week as rows; Reports and Order Later use fewer, bigger buttons a row.
- **Which screens are phones.** On Android, a screen whose shorter side is under 600dp (Android's own phone/tablet line), so a Fold switches as it opens and closes. Manager → Terminals → *Screen layout* can force phone or standard pages for a terminal, and `--screen phone|standard|auto` does the same from the command line. Try phone pages on a desktop with `vtmodern --screen phone --size 540x1140`.
- **Editing.** The editor always shows pages as designed. Open a phone version from the page list to edit it, or make one with *Copy*, then set *Phone version of*, *For screens: Phones* and a portrait canvas.

## Factory reset and demo data

- `vtmodern --factory-reset` puts the store back to a fresh install: it backs the database up first (`backups/viewtouch-<time>-before-reset.db`), then deletes it, so the next start begins with the starter pages, menu, staff and settings. Backups and saved report exports stay. Stop ViewTouch first (`sudo systemctl stop vtmodern vtmodern-kiosk` when installed; add `--data-dir /var/lib/viewtouch`).
- Or from the screen: **Manager → Factory Reset…**, type RESET. ViewTouch backs up, closes, deletes the database and starts again with the starter set (the installed services are restarted by systemd; on a desktop it starts itself again). Terminals reconnect on their own.
- `vtmodern --demo-data` fills a store that has no sales yet with demo history, played through the real POS (about 10 seconds): two months of service, the same two months last year, and a few days two and five years back (Find a Check, 5 Years). Staff clock in and take breaks; the opening and closing checklists get ticked; vendors deliver Tuesdays and Fridays; orders with their choices (toppings with No / Extra); the kitchen and expo bump tickets; cards on a (simulated) reader with brand, last four and tips, cash, gift cards, house accounts; phone orders with the guest's name and a promised time, regulars' Same as Last Time; deliveries out with Lou the driver (PIN 7777) and a delivery fee; a manager's voids, staff meals, discounts and refunds (card and cash); banks checked out; End of Day. Then 12 regulars (plus phone-order guests), 8 gift cards, two weeks of shifts with time off and a swap, a posted message, and right now: tables seated with tickets in the kitchen, a bar tab, a phone order cooking, a delivery out, an order for later, a walk-in, the Martinez party's tables held, a table to bus, tonight's waitlist and bookings.
- Together: `vtmodern --factory-reset --demo-data`.

## Backups

- **Automatic:** the machine that keeps the data backs up when it starts, every 24 hours and after every End of Day, keeping the newest 30. Backups go to `backups/` next to the database (`/var/lib/viewtouch/backups` when installed). A USB stick or network share is safer: set `backup-dir` in the .conf file. `backup-every` and `backup-keep` change the timing and count.
- **A second copy:** Manager → Store Settings → *Also copy backups to* (for example `/media/usb/viewtouch`). Every backup is also copied there and checked, keeping as many. If the drive is missing, the backup still runs and the Manager is told.
- **Status:** Manager → End of Day shows when the last backup ran, whether the second copy worked, and a **Back Up Now** button.
- **By hand:** `vtmodern --backup` (add `--data-dir /var/lib/viewtouch` for an installed store). It is safe while ViewTouch runs.
- **Restore:**
  ```sh
  sudo systemctl stop vtmodern vtmodern-kiosk
  sudo vtmodern --data-dir /var/lib/viewtouch --restore /var/lib/viewtouch/backups/viewtouch-20260929-230000.db
  sudo systemctl start vtmodern      # or vtmodern-kiosk
  ```
  The backup is checked first. The database it replaces is kept beside it as `viewtouch.db.before-restore-<time>`.
- **Encrypted backups:** Manager → Store Settings → *Encrypt backups* and a *Backup password* (8 characters or more). Backups and the second copy are then `viewtouch-<time>.vtbak` files that only that password opens (AES-256-GCM, with the key made from the password by PBKDF2). Each one is opened again and checked as it is made. Restoring one on the same computer needs nothing more. On another computer `--restore` asks for the password (or reads `VTM_BACKUP_PASSWORD`). **Write the password down somewhere safe:** without it, an encrypted backup can't be opened by anyone. Changing it leaves older backups on the old password.

**Encrypting the data itself.** The live database is an ordinary SQLite file, readable by anyone who has the disk. For a store server, turn on disk encryption when installing the operating system (Fedora and Ubuntu installers: *Encrypt my data*). Everything is then encrypted at rest with no change to ViewTouch, and the server asks for the disk password when it boots. PINs are never stored, only salted hashes.

**Power cuts and damage.** Each sale is synced to disk as soon as it is saved, on a background thread, so a power cut loses nothing that was already on the screen. At start ViewTouch checks the database. If it is damaged, ViewTouch won't run on it: it names the newest good backup and the command to restore it. The tests cover a power cut in the middle of a check, and a busy service of 1,000 checks on four terminals (every step under a millisecond, nothing lost).

Each backup is a complete SQLite copy. Checks, pages, menu, staff, settings and past days are all in it.

## Finding any check

Manager → **Find a Check…** (or *Find an Older Check…* on the Reopen page) finds any check ever closed: every one is kept in the database for good (`checks` table; nothing prunes it). Ranges: Today, Week, Month, Year, 2, 5 or 50 Years, All. Search a check number (`#123`), an amount (`17.62`, with or without the tip), or part of a name, phone number (digits), table, server, item or gift card number; **Show All** lists every check in the range. The database does the looking (`findClosedChecks`: newest first, a page of 100 records at a time, `closed_at` indexed; words are matched in the saved record with `LIKE`, then checked properly), in the background, so 50 years stay quick; **Older ›** / **‹ Newer** page. A check shows its items, payments, refunds and its **History** (its events: the audit trail). **Reprint Receipt** prints a copy. Staff who can take payments can search.

**Refunds** (`refundPayment`, app/pos_refunds.cpp): on a closed check, today's or found from any day, **Refund…** on a payment: the amount (up to the bill it paid: tips aren't refunded, cash less its change; or part) and a reason; a manager's (or their PIN). A Stripe card is refunded through Stripe (`POST /v1/refunds`), cash comes out of the drawer or bank as a *Refunds* pay-out, a typed card is recorded. Each `Refund` (who, why, how, Stripe's `re_…`) is kept on the check (`refunds`, and an event) and in the `refunds` table by the day it happened (schema 12), so today's Sales report has a *Refunds* section and *Collected after refunds*, and the Deposit report takes *Card refunds* off the card batch, whatever day the check was from. Undo Payment on a Stripe card (an open check) also needs a manager.

## Customers, gift cards and house accounts

- **Customers** (Check… → Customer…, or Manager → Customers…): find someone by any part of their phone number or by name, see their visits, what they've spent and their last visit, edit their details and notes, and put them on the check. Takeout and delivery customers go on file by themselves when their check closes, and the customer form suggests regulars as you type.
- **Gift cards** (Settle → Gift Card…, Check… → Sell / Check a Gift Card…): type or swipe the number to see the balance and history. *Sell* or *Reload* puts the card on the check; it works once that check is paid (no number: one is made up). *Pay the Check from This Card* takes up to what it holds. Undoing that payment, or reopening the check that sold a card, puts the money back where it was. No tax on gift card sales, and nothing goes to the kitchen.
- **House accounts:** a manager turns one on for a customer (with an optional limit). Put the customer on the check, then **House Account** on Settle charges it. Payments on the account (cash goes in the drawer, or card) are on the Customers page.
- **Report:** Reports → Gift Cards: cards sold and spent, what is still on cards (owed by the store), and house account charges, payments and balances.

## Waitlist and reservations

**Host Stand** on the Tables page opens the seating screen; its *Waitlist & Reservations…* opens the host stand's list and forms.

- **Seating screen** (`hostStand` panel, page `seating`; hosts start on it unless Settings → *Hosts start on* says otherwise): the floor plan as drawn (`LayoutController::floorPlans()` / `floorPlan(id)`: each plan's tables and pieces, the box they fill), a tab for each plan (the floor's own first, then its tabs' order; the last picked is kept), two fingers to zoom up to 3× and drag; every table where it stands, turned as it is, colored by what it is: **available** (green), **seated** (amber: guests, minutes, server), **dirty** (red: needs bussing, and for how long) or **held** for a party (purple: name and time), with a count of each. Touch a party in *Waiting* or *Reservations*, or **Walk-in** with − / + for how many, then touch one table, or several to push together for a big party; it shows the seats against the party size. **Seat** opens the check on the first table for the chosen server (the others show *with* it); **Hold for Them** keeps the tables for that party (others can't be seated there; touching the party later picks them again); **Mark Available** / **Mark Dirty** for bussing. When a table's last check closes, it and the tables pushed together with it turn dirty by themselves. Holds let go when the party is seated elsewhere, leaves or is a no-show. Table buttons on the floor plan show *Needs bussing*, *Held: name* and *With T5* too.

- **Waitlist:** add a party (name, phone, size, a note). The quote is the parties ahead plus one, times *minutes per party ahead* (Store Settings, 10 by default), which the host can change by 5 minutes. The line shows how long each party has waited against what they were told, in red once they are past it.
- **Table ready:** *Notify* texts the guest when a texting service is set up (Store Settings → *Texting service URL*: a JSON POST of `{"to", "message"}` to your SMS provider or a relay). Otherwise it reminds the host to tell them.
- **Seating:** *Seat Them…* lists the free tables, the smallest that fits first. Choose the server; the table's check opens for them with the party size and the guest's name.
- **Reservations:** book for today or the next six days at a time. *They're Here* puts them at the front of the line. Reservations more than 15 minutes late are marked; *No-Show* or *Cancel Booking* takes them off.
- The host stand shows how many parties were seated today, the average wait, and no-shows.

## Reports over a range

Reports have a row of periods: *Day* (one business day, as before), *This Week*, *Last Week*, *This Month*, *Last Month*, *This Year*, or *Dates…* (any range up to a year). *vs Last Year* adds the same days a year before and the change, row by row. Ranges are read from the saved checks away from the screen, so a busy terminal never waits. Sales, Items, Categories, By Hour, Servers, Kitchen, Audit and Food Cost work over a range; the rest are one day at a time. CSV and PDF save what is shown.

## Schedule and tip pooling

- **Manager → Schedule:** a week at a time (‹ ›), each day's shifts; touch a day, pick the person and the times, *Add Shift* (an end before the start runs past midnight). Touch a shift to remove it, or *Clock Them In*. Hours for the week are listed underneath, in red over the weekly overtime line.
- Staff see their **next shift** on the logout screen.
- **Clock in on schedule** (Store Settings): staff can only clock in from *…minutes early* before a scheduled shift until it ends. Managers always can, and can clock anyone in from the schedule.
- **Roles:** server, bartender, cashier, host (the waitlist), busser (clocks in, shares tips), manager, admin. The demo staff gained Riley (busser, PIN 3333) and Jo (bartender, PIN 4444).
- **Pay and labor cost:** Manager → Employees → *Pay rate* (an hour, before tips) and *Other jobs*, one per line with its pay (`bartender 9.00`). Someone with more than one job is asked *Which job today?* when they clock in. Each shift keeps the job and its pay, so a raise later doesn't change past labor. Reports → Labor adds **Labor cost today**: each person's pay, overtime at time and a half, the total, and **labor % of net sales**. Tip pools go by the job worked today.
- **Tip-outs** (Store Settings → Tips): a row each, who gets it, the percent, and of tips or of sales (kept as `busser 15 tips`, `bartender 2 sales`). That share of each person's tips (or sales) goes into a pool for everyone of that role who worked today, split by hours. With no one of that role on today, servers keep it. *Cash Out My Tips* and Reports → Tips use the result.
- **Tip pool** (Store Settings → Tips): one table, a row for each job in the pool and its share an hour (100 a full share, 50 half; kept as `busser 50`, `PosSettings::tipPool`). A job without a row is out of the pool; someone's own setting (Employees) can put them in or keep them out.

## Inventory and food cost

- **Manager → Inventory:** each ingredient's unit, what's on hand, the low-stock mark and its cost per unit. Count stock or add a delivery by editing *On hand*.
- **Recipes** (Manager → Menu → *Recipe*): one ingredient per line with the amount, e.g. `bun 1` or `lettuce 0.5`. Modifiers that are menu items (sides) have recipes too; *No* on a modifier uses none, *Extra* twice as much, *Lite* half.
- Stock goes down when an order is sent to the kitchen (or fired, or closed), and comes back when a sent item is voided.
- An item whose ingredients run short is **sold out by itself**, and comes back when restocked. An item a person 86'd stays sold out until they bring it back. Crossing the low mark shows *Running low: …*.
- **Reports → Food Cost:** each item sold today with its sales, recipe cost and cost %, then every ingredient's stock (low and out marked).
- **Vendors and deliveries:** Manager → Inventory → **Vendors…** lists who the store buys from (phone, account number, delivery days), and each ingredient can name its usual vendor. **Receive a Delivery…**: touch the vendor, type how much came of each item (and its cost if it changed) and the invoice number, then **Receive Delivery**. Stock goes up, costs are updated, sold-out items come back, and the delivery is kept (60 days loaded). Reports → **Purchases** shows today's deliveries by vendor. The demo has three vendors.
- New stores start with demo stock and recipes for the burgers, salads, breakfast, coffee and sides. An existing store starts with an empty inventory (or `--reset-menu` for the demo set).

## Kitchen display

- Tickets turn yellow after *Kitchen: ticket turns yellow after* minutes and red when late (Store Settings; 8 and 15 to start).
- **Rush** and **VIP** (order screen → Check…): a rush ticket goes to the front with a red frame, VIP gets a gold one, and both are printed on kitchen tickets.
- **All Day** on the kitchen screen counts everything still to make there ("3 Cobb, 2 Caesar"), for batching.
- **How things look in the kitchen** (Manager → Menu): *Kitchen name* (what the kitchen screen and kitchen tickets show instead, e.g. "BCN BGR"), *Kitchen highlight* (a color on the kitchen screen), *Don't show in the kitchen* (water, "No side"). Modifier group options take a kitchen name after `|` (`Ranch | RNCH`), or `| -` to leave it off. Checks and receipts keep the real names.
- **Rush:** on the order screen, Check… → **Rush** (again to turn it off).
- **Expo Display** (Manager → Expo Display, or a screen started on the `expo` page): every ticket across the stations, each line ticked when its station bumps it, "Waiting on: bar", and blue READY when all of it is made. Touch a ready ticket when it goes out (a ticket not ready yet needs a second touch); Recall brings the last one back.
- **Reports → Kitchen:** tickets per station from sent to bumped, with the average, the longest, how many were late, and the five slowest.

## Manager approval and practice mode

- **Manager approval:** when someone may not do something on their own (void a sent item, a discount, a manager action), a PIN pad appears on the same screen. A manager types their PIN and it goes through once; the check's history says who approved it. *Cancel* puts it away. No logging out and back in.
- **Practice mode** (training): mark a person *In training* (Manager → Employees), or a manager touches **Practice Mode** on the Log Out page to switch their own screen. A yellow *PRACTICE* banner shows. Practice checks are labelled "(practice)" and leave real tables free; they never reach the kitchen screens or printers, stock, drawers, banks, reports, points, gift cards or house accounts. Closing one says "not a sale"; any left open are cleared at End of Day.

## Kitchen screens by station

Store Settings → **Kitchen stations** lists the stations (the demo: Grill, Fryer, Cold Line), and Manager → Menu → *Made at* says where each item is made. On a kitchen screen, **Station…** picks which station it shows; the screen remembers it. Touch it again to go to the next station, and back around to the page's own (everything for the kitchen printer).
- A combo's part made elsewhere shows on its own at that station: the fryer sees "Fries, with COMBO BGR", and bumps it on its own.
- The expediter waits for every station: "Waiting on: Grill, Fryer".
- Printed tickets don't change.
- One screen for everything: leave *Kitchen stations* empty. The Station button goes away, and items' *Made at* is ignored, so a burger's fries stay under it and one touch bumps the whole order.

## Orders for later

On a takeout or delivery check (or Check… on any order), **Ready Later…** picks the day, hour and minutes it should be ready: up to 60 days ahead. Send saves it without bothering the kitchen. The store's computer sends it to the kitchen by itself 20 minutes before it's due (Store Settings → *Orders for later go to the kitchen*). The ticket and the kitchen screen say **READY AT 6:30 PM**.
- It can't be closed until the kitchen has it.
- An order for another day is paid on that day, and it doesn't hold up tonight's end of day.

## Exceptions and deposit reports

- **Exceptions** (Reports → Exceptions, over any dates): by employee, voids of sent items, discounts and comps, payments and discounts taken back, reopened checks, moved / transferred / merged checks, and (today) drawers opened with no sale; then the ten biggest. Each check event now carries its amount.
- **Customers** (over any dates): each customer's visits, spending, average check, last visit and the item they order most, best customers first. Saved customers by their record; a name on a takeout or delivery counts too.
- **Royalty** (any dates): net sales (no tax, no gift cards sold) times Store Settings → *Royalty* and *Advertising fund* percents.
- **Accounting** (any dates): a balanced journal. Debits: each payment type (cash kept, card payments with tips, gift cards, house accounts), discounts, staff meals. Credits: sales by family, tax by class, gratuity, tips payable, gift cards sold, cash rounding. Store Settings → *Chart of accounts* maps the keys (`sales`, `sales:<family>`, `tax`, `tax:<class>`, `gratuity`, `tips`, `discounts`, `staffMeals`, `rounding`, `giftCardsSold`, `tender:<id>`) to account numbers; accounts with the same number add up. The CSV button is the export for QuickBooks or an accountant. The demo has a small chart.
- **Deposit** (today): each drawer or server bank, counted (or expected if not yet), less its starting cash, is the cash to deposit; the card batch (payments + tips); and the book balance, sold with tax against collected (cash, cards, gift cards, house accounts), with the drawers' over/short and what's still open.

## Event tickets

Manager → Menu → *Event tickets: seats* (and the *Event date and time*, like 2026-10-09 19:00) makes an item admission to an event. Each one sold, on an open or closed check, takes a seat; when they're gone it's sold out, and a void gives the seat back. Each sale says how many are left, and the self-order kiosk shows it. The date goes on the check and receipt ("Wine Dinner Ticket (Fri Oct 9, 7:00 PM)"), and nobody can sell it after. End of Day keeps the count. The demo has a *Wine Dinner Ticket* ($65, 40 seats) on the Drinks page.

## Substitutes

Manager → Menu → *Can be a substitute* and its *Substitute price*. Touch **Sub** (in the order bar with No, Extra, Lite, Side), then the item: it goes on the item before it as "SUB House Salad" at the substitute price (+$3.00) instead of being its own $8.50 salad. Its stock and kitchen station count as usual. The demo's House Salad (+$3.00) and Caesar (+$3.50) can be substitutes.

## Sold by weight

Manager → Menu → *Sold by weight* (and the *Weight unit*: lb, oz or kg) makes the price per pound. Touching the item opens the **Weigh** page: type the weight from the scale (125 = 1.25 lb) and it shows what it comes to; *Add* puts "Smoked Brisket 1.37 lb" on the check at $22.00 × 1.37 = $30.14 (rounded to the cent). Typing the weight before touching the item adds it at once. Recipes are per unit, so stock goes down by the weight. Not shown on the self-order kiosk (no scale there). The demo has *Smoked Brisket* at $22.00/lb on the Burgers page.

## Combos

A combo is a menu item whose choices are other menu items. In Manager → Modifier Groups, turn on **Options are menu items** and list items by name ("Soda", "Fries + 0.00", "Draft Beer + 3.00"). Then give the combo item those groups. Its side and drink use up their own stock, can't be chosen while sold out, and are counted on the Items report under *Chosen with other items*. Only parts made where the combo is made go on its ticket: a drink whose menu item goes to the bar (or nowhere) is left off the kitchen ticket and screen. The demo's **Burger Combo** ($16.95) asks for the temperature, a side and a drink.

## Bar tabs

Tables → **Bar Tabs** lists the open tabs. *New Tab…* asks for the name on it ("Mike, red jacket"), then goes to the menu. A tab stays open even with nothing on it, until it is paid; touch it on the list to add another round.

## Screen saver

Store Settings → *Dim the screen after (minutes)* (10 to start; 0 = never). A screen nobody touches dims to the store's name and the time. The first touch only wakes it: it never presses the button underneath. Kitchen, bar and expo screens stay on, and the self-order kiosk shows its own pictures instead.

## Idle log-out, table timers and messages

- **Idle log-out** (Store Settings, 3 minutes to start, 0 = never): a screen nobody has touched logs out and goes back to the login page. Kitchen, bar and expo screens need no login.
- **Table timers:** an open table shows how long it has been seated ("Sam · $42.10 · 38m"), with a red frame after *Mark tables seated longer than* (90 minutes to start). **Reports → Turns:** dine-in checks from seated to paid, by party size and by table, with the average check and per guest (also over a range of days).
- **Messages:** Log Out → **Send a Message…**, or **Message…** on the kitchen screens: to everyone, the kitchen screens, the floor, or one person on the clock, picked from ready-made ones ("Need a runner", "86: …") or typed. It shows across the top of the screens it's for until someone there touches OK; the last hour's are listed on the message page.


**Messages that stay up.** In the message composer, *Show it* picks *Once* (the usual: the last hour, OK'd per screen), *Until tonight*, *Until tomorrow night* or *For a week*. A posted message is kept (it survives a restart and reaches every screen), shows to each person who logs in until it expires, and is listed with *Take Down* for whoever posted it or a manager.
## Customer display

A second monitor facing the guest:

- **Between guests:** the logo (or store name) in the store's color, then its messages and pictures in turn (Store Settings → *Customer display: between guests*, one per line; `image:/path/photo.jpg` for a picture), plus any promotion running now.
- **While ordering:** the table or check number, each item with its choices (add-on prices shown as +$1.00), voids struck through, discounts and promotions, the total. With loyalty on, *Earn rewards*: the guest types their phone number on the screen's keypad and is found or signed up. It shows their first name (never the number), their points and what this visit earns.
- **Tips:** on Settle, **Ask Guest for Tip** offers the tip choices, a custom amount on the keypad, or no tip; the choice goes on their card payment.
- **After paying:** "Thank you", their change and the points they earned, and *Print receipt*, *Text me* (when texting is set up) or *No receipt*.

Start with `--customer-display auto` (or `customer-display = auto` in the kiosk's .conf): on a desktop it opens a full-screen window on the other monitor; on a kiosk, whose one window spans both monitors, the POS stays on the left monitor and the guest's side is on the right. With one monitor, `--customer-display split` puts it in the right third, to try it out.

## Loyalty and promotions

- **Loyalty** (Store Settings): customers on a check earn points on what they spend after discounts (1 per dollar to start); reopening a check takes them back. *Rewards*, one per line like `50 = 5.00`, take money off: Customers → the customer on the check → the reward button. Undoing the reward gives the points back. Guests can sign up or be found by typing their phone number on the customer display.
- **Promotions** (Manager → Promotions): percent off chosen categories or items, or *buy N, get M* (the cheapest at the percent off), on chosen days and times. They apply by themselves while they run, show on the check and receipt by name, and come off again when the time is up or the items go. The demo set has *Happy Hour* (weekdays 3-6 PM, half-price draft beer and wine) and *Burger Tuesday* (the second burger half off).

## The menu: categories, the Menu Builder, screens that fill themselves

Categories are their own records (`PosSettings::menuCategories`: id, name, color, meal periods, and the kitchen printer, station and tax new items in them start with); an item's `family` is its category's id, and families nobody set up yet show as categories of their own (`PosShared::categories()`). Every screen gets `menuCategories` (with item counts and whether each is on now), `menuItems` (with what the Menu Builder's card needs) and `choiceGroups` (rules in words, who uses each).

The order screens fill themselves: the meal pages (`index-breakfast/lunch/dinner`) hold a **Menu categories** panel (`menuCategories`, `props.period`), whose buttons open the page with role `menu` (`menu-all`) on that category (`LayoutController::openCategory`, `menuCategory`); its **Menu (self-filling)** grid follows the category picked and colors items by it. The old hand-built item pages live on only as the tests' fixtures (`tests/fixtures/pages`, loaded by `tests/layout_fixture.hh`).

Stores set up before: `layout::hasHandBuiltMenu` (a meal page whose buttons go to an item page with no self-filling menu) makes the Menu Builder offer **Switch to Self-Filling** (`LayoutController::switchToSelfFillingMenu`): `layout::withSelfFillingMenu` replaces those buttons with a Menu categories panel over the same area (the rest of the page kept), makes the Everything page the menu page or adds one, and opens the result in the page editor as one undoable step. The menu grid with no columns set fits the category's items without scrolling (2–7 columns, as tall as the rows allow); a long word shrinks its button's text. Its first tab, **★ Favorites**, is the items marked `favorite` (Menu Builder) and today's best sellers, 12 in all.

**Manager → Menu Builder** (`menu-builder` page, `menuBuilder` panel, `app/pos_menu_builder.cpp`): categories (`saveCategory`, `moveCategory`, `deleteCategory` only when empty), a category's items as tiles, and an item's card (`saveMenuItemCard`, `deleteMenuItemCard`): name, price, category, photo, sold out, on the kiosk, the choice groups it asks for, and **What's on it**: typed ingredients become the item's own group (`on-<item id>`, every option included, Light/Extra/Side allowed), so No / Lite / Extra / Side work for it everywhere. A new item starts with its category's printer, station and tax, and goes at the end of its category; moved to another category, it takes the new one's kitchen if it had the old one's. **Choice Groups** (the builder's other mode): `saveChoiceGroup` (`{id, name, min, max, askHow, options: [{name, price, included, kitchenName}]}`; the screen turns *One / Up to… / Any number* and *Required* into min and max; what it doesn't show, like a combo's menu item, is kept) and `deleteChoiceGroup` (items stop asking for it). Hold and drag an item tile or a category to move it (`moveMenuItemTo`: its place among its category's items, which trade the places they have in the menu; `moveCategoryTo`). **Import…**: `LayoutController::readMenuFile` reads a spreadsheet saved as CSV on the device in hand (comma, tab or semicolon; quotes; a header by name, English or Spanish, else name, price, category; UTF-8 or Latin-1; an `.xlsx` is told to be saved as CSV) with `app::readMenuCsv`; the preview shows what's added and what's left out; `importMenuRows` adds them (categories made as needed; items already on the menu skipped, or given the file's price). **Starter Menu…** (`menuTemplates`, `applyMenuTemplate`, `app/menu_templates.cpp`): Taqueria, Café, Burgers & Bar, Pizza; categories, choice groups and items with what's on them, added alongside (nothing there changes; applied twice, nothing doubles). **Export** (`menuExport`, `LayoutController::exportMenu`: a `.vtmenu.json` of categories, choice groups and items, without sales counts, recipes, photos or today's 86) and **Import…** of such a file (`importMenuFile`: by name, what isn't there is added, item, group and category ids remapped, an item's What's on it copied; nothing there changes). 86 during service (`setAvailable`) sets `MenuItem::soldOutToday`; End of Day puts those back (`soldOutBackNextDay`, on by default); the builder's Sold out switch is lasting. The page editor's fields are translated as `EditorController` hands them out (labels, sections, hints, choices; composed labels by `labelTemplate`/`labelArg`; `groupKey` keeps the English section name); `tools/i18n.py` collects them from `layout/schema.cpp` (adjacent literals joined) and page names from the seed pages. Heartbeat: the server pings every terminal every 3 s (`PosServer::setHeartbeat`); a terminal that answers ("pong") and then is silent 15 s is dropped, freeing its check; a terminal whose server pings and then is silent 10 s (`RemoteSession::setWatchdog`) shows Reconnecting, fails what was waiting, and reconnects (Wi-Fi gone without the socket closing). Older peers that don't ping or pong aren't held to it. **Ready to go?** (`menuProblems`: no price or category, a choice group that's gone, a printer that isn't set up, two items with one name, a required group with too few options, a meal period that isn't set up; empty categories as notes) — the builder's ✓ / ⚠ button lists them and opens each. **Also a button on** (`LayoutController::handBuiltPages`, `addItemButton`): the item's button on a hand-built page, the size and look of its most common item button, in the next free place on their grid (spaced as they are; clear of the template's zones), saved like the page editor saves. **Terminals → Start orders on** (`TerminalConfig::startCategory`, `terminalStartCategory`): an order opened from outside the order screens lands on the menu page on that category. Fast entry: **Add Several…** (`addMenuItemsFromText`: one per line or comma-separated, the price last, `4,25` is a price; `Drinks:` puts what follows in that category, made if new; nothing is added while one lacks a price; names already on the menu are skipped) and **Duplicate** (`duplicateMenuItem`: "Fish Tacos 2" right after it, with its own copy of What's on it).

## Whose checks

**Open Checks** shows *My Checks*: yours, a delivery you're driving, and (for anyone who takes payments) the kiosk's orders waiting at the counter. **All Checks** shows everyone's; someone else's is dimmed with a lock (`openChecks()`: `forMe`, `mayOpen`). Opening a check that isn't yours needs the permission **Open other people's checks** (`check.others`: managers and cashiers by role; Manager → Employees → per person), or a manager's PIN on the spot, noted on that check's history. The same rule applies to touching another server's table.

## Self-order kiosk

A screen where guests order on their own, then pay at the counter.

- **Turning it on:** Manager → *Self-Order Kiosk* turns this screen into one on the spot. For a screen that should always be one, set Manager → Terminals → *Screen layout* to *Self-order kiosk*, or start it with `vtmodern --self-order`.
- **For the guest:** pictures and *Touch to Order* while nobody is ordering → *For Here* / *To Go* → the menu by category, with photos and descriptions → their choices (temperature, sides…) → a name to call the order by → *Your order number is 42*.
- **Tall kiosks:** on a portrait screen (e.g. a 21.5" floor stand such as the Chipsee KIOSK-CM4-215) the dishes are big photo cards, and the categories, the order bar and every button sit in the lower part, within reach. **Easy Reach** brings the whole menu down to the lower 60% of the screen for guests in a wheelchair, and children. Sold-out items show as sold out. If a guest walks away mid-order, *Are you still there?* appears, and the order is cleared (default after 90 s, set in Store Settings).
- **At the counter:** the order waits in Open Checks as *Kiosk 42* with the guest's name. The cashier opens it, takes the payment, and closing it sends it to the kitchen. With Store Settings → Self-order kiosk → *Send orders to the kitchen at once*, it goes to the kitchen as soon as the guest finishes.
- **What guests see:** every menu item except modifiers (they appear as choices), anything marked *Not on the self-order kiosk*, and alcohol, which never shows there. Each item can have a *Description* and a *Photo* (Manager → Menu Items): an image file on the store's computer; kiosks connected over the network get it from there. The attract screen shows the dishes' photos and the customer display's slides (Store Settings).
- **The store's look:** Store Settings → *Self-order kiosk* sets the background, card, Add/Place Order and text colors (`#rrggbb`), the font, the size of buttons and text (60–200%), and the welcome button's words. Switches choose whether it asks *For Here / To Go* (off: everything is for here), asks for a name (off: orders are called by number), and shows *Easy Reach*. The accent color, logo and pictures are the customer display's. Text on buttons turns dark or white by itself to stay readable.
- **Their way:** an option marked `*` in Manager → Modifier Groups comes on the item (`ModifierOption::included`); groups with *Guests can ask for Light, Extra or On the side* (`ModifierGroup::askHow`) let a choice be had that way. The kiosk's choices sheet lists *What's on it* with No · Light · Regular · Extra · On the side, and Regular · Light · Extra · On the side for add-ons chosen (`setChoice`: `off`, `""`, `no`, `lite`, `extra`, `side`). What comes on it is no choice of its own: it never counts toward a group's required or most choices. Staff touching it get the No / Lite / Extra / Side menu.
- **Their slip:** `kioskFinish` prints an order slip on the kiosk terminal's receipt printer (`PosPrinter::printOrderSlip`, `print::orderSlip`): the order number big, the name, For here / To go, the items (free choices as on receipts) and the total, "Please pay at the counter" and whether it's being made. The thank-you screen says to take it (`lastOrder.slip`). Store Settings → Self-order kiosk → *Print an order slip* (`kioskSlip`, on).
- **Staff:** nothing on the kiosk leads to staff screens. A manager holds the top-left corner for 3 seconds and types their PIN to end kiosk mode.
- Card payments at the kiosk will come with card readers (M8).

## Languages

The screens come in English and Spanish (Español).

- **Each person's own.** Manager → Employees → *Language*. The screen switches when that person logs in and back when they log out. On a store with several terminals each screen speaks its own user's language, messages included. The demo store has Rosa (PIN 5555), a server whose screens are in Spanish.
- **The store's.** Manager → Store Settings → *Language*: for screens nobody is logged in to, people without their own, and everything guests see: the customer display, receipts and kitchen tickets.
- **Your own words.** Menu item names stay as you typed them, since the check, the kitchen and receipts use them. To translate buttons you added (or change any phrase), put a file `translations/es.json` in the data folder: `{"Burger of the Day": "Hamburguesa del día"}`. It wins over the built-in phrases.
- **Developers.** Phrases live in `i18n/es.json` (English → Spanish). `python3 tools/i18n.py missing es` lists what a new screen still needs, and the `i18n_es` test fails until every phrase has its Spanish with the same `%1` placeholders.

## On-screen keyboard

Text fields (customer names, notes, Manager forms…) get an on-screen keyboard, since most touch screens have no keyboard. It is **on by default** everywhere except Android (phones and tablets use their own). Turn it off for a screen that has a keyboard in Manager → Terminals → *On-screen keyboard*, or with `--touch-keyboard yes|no` (which wins over the setting). It pops up over the screen (and over any dialog); the page keeps its size. When it would cover the field being typed in, the page (or the setup guide) slides up just enough to keep the field in view, and back down when the keyboard goes away. Messages show at the top while it's up. On a phone held upright the bottom row is `?123 , space . ⏎ ⌨▾` (`@` and `-` are on the symbols page), keys are as wide as their job (the space bar widest), and labels shrink to fit their key. On Android the phone's own keyboard is used; the app's manifest says `adjustNothing`, so Android doesn't shrink the window: the keyboard pops over it and the same slide-up keeps the field in view (from `Qt.inputMethod.keyboardRectangle`). PIN entry (login, manager approval, Time Clock, job choice) is always ViewTouch's own keypad, never the device's keyboard. Number fields (phone, amounts, counts) get a number pad; the first letter of an empty field is a capital.

## Meal periods

Manager → Meal Periods sets when breakfast, lunch and dinner start (the starter set is 04:00, 11:00 and 16:00). You can add others, such as Late Night. Each period runs until the next one starts, and past midnight until the first one. The Menu button opens the index page whose *Meal period* (page inspector) matches the time. With no page for the current period, it opens an *All day* index page, else the first one.

## Several terminals

One machine keeps the data; the others connect to it:

```sh
# The server. It is also a terminal unless --headless.
./modern/build/vtmodern --serve                 # port 7719; --port to change
./modern/build/vtmodern --serve --headless      # a back-office box with no screen

# Every other terminal (no database of its own): finds the store and pairs on screen
./modern/build/vtmodern --connect auto --terminal "Bar"
./modern/build/vtmodern --connect 192.168.1.10 --terminal "Line" --page kitchen   # a given server
./modern/build/vtmodern --connect 192.168.1.10 --pair K7QM4-XHP2W --terminal "Bar"   # pair from a script
```

**Pairing terminals.** Only paired devices can connect.
1. On any store screen: Manager → Terminals → **Pair a Device** shows a code like `K7QM4-XHP2W`. It works once, for 10 minutes. With a server that has no screen, run `sudo vtmodern-setup pair` (or `vtmodern --pairing-code`) on it instead.
2. The new terminal shows **Join a ViewTouch store**. It lists the stores it finds on the network (or type the server's address), then asks for the code and this terminal's name. It has its own keyboard for touch-only screens.
3. It gets its own key, saved in `terminal.json` in its data folder, and connects with it from then on. It shows up in Manager → Terminals as a *paired device*, where its printer and drawer are set. Removing it there disconnects it at once and keeps it out. It then shows the Join screen again.

**Finding the server.** Terminals ask on the network (UDP port 7719) and servers answer with the store's name. A paired terminal whose server changed address (DHCP) finds it again by its id and saves the new address.

- **Shared data.** Checks, tables, the menu, staff, the drawer and reports are the same on every terminal. Check numbers run in one sequence, and End of Day sees every terminal.
- **Check locks.** A check that is open on one terminal can't be opened on another (the table shows "on Bar"). Closing it, putting it away, logging out or losing the connection releases it.
- **Page edits reach every terminal.** Pages edited on any terminal are saved on the server and pushed to the others. Saving needs a manager on that terminal.
- **Lost connection.** A terminal that loses the server shows *Reconnecting…*, keeps trying, and returns to the login page when the server is back.
- **Responsiveness.** Terminals never wait on the network. Button actions continue when the server answers, and touches are ignored until then.
- **Printing.** All printing happens at the server's printers. Each terminal has its own drawer and prints receipts on the printer set for it in Manager → Terminals (default: the "receipt" printer).
- **Security.** Connections are encrypted (TLS 1.2, ECDHE-PSK with ChaCha20-Poly1305). Each paired device proves itself with its own 256-bit key, and the server with the same key, so no certificates are needed and a stranger's device can't connect or listen in. Pairing codes are stretched (PBKDF2, 600,000 rounds), so they can't be guessed from a recorded pairing. Open port 7719 for TCP and UDP in the server's firewall.

## Keep running if the server fails

A second store computer can be the **standby**: it keeps a live copy of everything and takes over if the main server stops.

```sh
# On the second computer, once (code from Manager -> Terminals -> Pair a Device):
./modern/build/vtmodern --standby auto --pair K7QM4-XHP2W     # or --standby 192.168.1.10
# From then on (as a service):
./modern/build/vtmodern --standby auto
```

- **Live copy.** The standby gets the whole database when it connects, then every change as the main server saves it (checks, payments, punches, settings, pages). It connects with the store's own server key, which only a standby pairing receives.
- **Taking over.** If the standby hears nothing from the main server for 20 seconds, it becomes the main server: same store, same paired screens, same check numbers. The screens show *The standby computer takes over in a few seconds*, find it on the network by the store's id and reconnect by themselves. A manager can also touch **Take Over Now** on a screen that lost the server and type their PIN.
- **The old main comes back as the standby.** A server that starts and finds the store already served on the network becomes the standby. A main server that was only cut off (a broken cable) sees the newer main on the network and steps down the same way. Either way it first keeps its own data as `backups/…-before-standby.db`, so nothing it took while cut off is lost for good.
- **Manager → Network** shows whether the standby is in sync, which screens are connected and who is on them, and whether each printer printed or failed last.

Run the main server and the standby as headless services on two computers, both on port 7719.

## Kitchen display and takeout / delivery

- **Kitchen and bar displays.** Manager → Kitchen Display / Bar Display, or start a terminal with `--page kitchen` or `--page bar-display`. These screens need no login.
  - Each Send is one ticket, oldest first, turning amber after 5 minutes and red after 10.
  - Touch a ticket when it's ready. Recall brings the last one back.
  - Each station bumps only its own lines, so the bar clearing drinks leaves the kitchen's food on its screen.
  - Paid-first counter orders stay on screen until they are bumped.
- **Takeout / Delivery** (floor plan) open the menu at once. The check shows **+ Name**, **+ Phone**, **+ Address** (delivery) and **Ready Later…** keys: each opens a field over the check (Enter or Save keeps it), so no separate page is needed. Three or more characters of a phone or name list up to three matching customers; touching one fills in their name, phone and address. Missing name (and address on a delivery) show in amber. The full form (page `customer`, name, phone, address and a note, saving as you type) is still there for layouts that link to it. They print on the receipt and kitchen tickets and show on the order, the kitchen card and the check list. A takeout or delivery that is put away with nothing ordered is discarded.
- **Same as Last Time.** A check with a saved customer shows their last order (kept on the customer record when a check closes: the lines, without voids, gift cards or the delivery fee) as one key; it adds the items again, unsent, at today's prices, leaving sold-out ones off.
- **Ready time.** Takeout and delivery checks show *Ready in about N min* / *Arrives in about N min*: Store Settings → *Takeout is usually ready in* / *Deliveries usually arrive in* (or today's average make time, when slower), plus *Add to the quote for each order cooking* for each other order with items not yet made, rounded up to 5 minutes. The first Send keeps it as the check's promised time (`promisedAt`).
- **A name before Send.** Store Settings → *Phone orders need a name before Send* (and an address for deliveries); Employees and Terminals have their own *yes / no / store setting*; the terminal's wins, then the person's.
- **Deliveries** (page `deliveries`, `deliveryBoard` panel; Tables → *Deliveries*): open deliveries by state (not sent, cooking, ready, out, back), oldest promise first. Pick orders and a driver, **Send Out** (the check stays with whoever took the order, its sale and tip, unless transferred; the driver collects, into their own bank), **Delivered**, **Open Check**. Drivers have the *driver* role (or job): they take orders and payments and keep their own bank unless their cash handling says otherwise. Store Settings → *Delivery fee* adds a `fee:delivery` line (no kitchen, no tax, no discounts; already "sent", so removing it is a void) once a delivery has an item, and takes it off again if the order is emptied. Reports → **Drivers**.
- **Swipe** an order line left to take it off (a void once sent) or right for one more (Again, once sent).

## Prices by order type, staff meals

- **Takeout and delivery prices:** Manager → Menu → *Takeout price* and *Delivery price* (0 = the regular price; delivery falls back on the takeout price). An item rung on a takeout or delivery check takes that price; the kiosk's *To Go* orders too.
- **Staff meals:** a discount payment type can be a *Staff meal* (Manager → Payment Types). The demo has **Staff Meal**, 50% off: ring the meal and touch Staff Meal on the payment screen. It records who ate (the person ringing it), and the Sales report shows *of which staff meals* under discounts.
- **Extra costs extra:** Store Settings → *Extra: percent added* (50 = half again) and/or *Extra: amount added* (e.g. $0.75, even on a free choice). Touching **Extra** before an item or a modifier charges it; the line keeps that price.
- **Items that aren't discounted:** *No discounts* (discounts and comps leave it out) and *No staff discount* (staff pay full price; the demo's beer and wine).

## Cash rounding

Where pennies are gone (Canada, and elsewhere): Manager → Taxes → *Cash rounding* → nearest 5 or 10 cents. A check paid in cash is rounded (what is owed when the cash comes: $17.62 → $17.60, $17.63 → $17.65); touching **Cash** with no amount typed takes the rounded balance. The check, the receipt and the Sales report show a *Cash rounding* line. Card payments are exact; a check paid partly by card rounds only what cash pays.

## Card readers (Stripe)

Cards can be taken on a **Stripe Reader S700** running ViewTouch itself (Stripe *Apps on Devices*). Manager → Terminals → *Card reader*: **Stripe smart reader (this device)**, **Simulated** (approves after `setSimulatedDelay` ms; totals whose cents end in 05 are declined, like Stripe's test cards) or none.

- **The flow.** A `tender` action of a card payment type on a screen with a reader calls `cardCharge(tenderId)` on the store (amount: what's typed or the balance, plus the tip chosen on the customer display; the check, currency and a description), hands it to the `CardReader` (ui/cpp/cardreader.cpp), and on approval calls `recordCardPayment` with Stripe's PaymentIntent id, the card's brand and last four. A payment id is recorded once (a resend after a dropped connection is ignored). The payment shows the card on the check and the receipt (with the `pi_…` id).
- **On the S700** (`android/src/org/viewtouch/pos/StripeBridge.kt`, Stripe Terminal Android SDK 6, `android/build.gradle`): the app connects to the reader it runs on (`AppsOnDevicesDiscoveryConfiguration`), creates a PaymentIntent (automatic capture, `viewtouch_check` metadata), collects (no tipping on the reader: ViewTouch asked) and confirms; Stripe's own app shows the payment screens and hands back. `ViewTouchApplication` calls `TerminalApplicationDelegate.onCreate`. It reconnects by itself (the reader restarts nightly).
- **The secret key stays on the store's computer** (Store Settings → *Stripe secret key*, `sk_…` or a restricted `rk_…`; shown again only as its last four). The store's computer (`net/stripe_api.cpp`) gets the readers' connection tokens (`requestReaderToken` → `readerToken`) and makes refunds: **Undo Payment** on a Stripe card refunds it (`POST /v1/refunds`) and takes the payment off when Stripe confirms. *Card currency* (default `usd`).
- **Getting it on a reader** needs Stripe to enable Apps on Devices, a DevKit for testing (sandbox only), then Stripe's app review and deployment from the Stripe Dashboard.

**A reader beside the register** (Stripe's server-driven integration; any register, e.g. a Raspberry Pi touch screen): Manager → Terminals → **Card Readers…** pairs a Stripe S700 / WisePOS E by the code it shows (`POST /v1/terminal/readers`; the first time, the store's Stripe location is Stripe's first one, or made from the address given), and Terminals → *Card reader* offers *Reader beside it: …* (`counter:tmr_…`). Credit Card on such a screen calls `startCounterCharge` on the store's computer: it creates the PaymentIntent (`card_present`, automatic capture), `process_payment_intent` on the reader (`skip_tipping` unless Store Settings → *Card tips are asked on* is the reader; customer cancellation on), then reads the Reader every 1.5 s (no webhooks: nothing outside the store has to reach it). `succeeded` (or a `connection_error` that Stripe says succeeded) records the payment from the PaymentIntent (`expand[]=latest_charge`: brand, last four, `amount_details.tip`), even if the register disconnected; `failed` cancels the PaymentIntent and says why. Cancel calls `cancel_action` (a reader busy reading says to wait). The register's waiting screen follows `counterCharge`; in Stripe test mode (`sk_test_`) it adds **Tap Test Card** / **Tap Declined Card** (`test_helpers/.../present_payment_method`); pair `simulated-wpe` to try it all without a reader.

## Tips and cash handling

- **Tips** go on card payments. On the settle page, touch 15%, 18% or 20%, or type an amount and touch Amount. The receipt, the Tips report and the server's "tips owed" show them.
- **Party gratuity.** Dine-in checks with at least *min guests* get the store's gratuity (Manager → Settings; 18% for 6+ by default, 0 turns it off). Anyone can add it on the settle page; lowering an automatic one needs a manager. Gratuity is part of the check total; tips are on top.
- **Server banks** (the starter setting; Manager → Settings → *Cash handling*). Each person keeps the cash they take in their own bank, on whichever terminal they use, so terminals need no drawer.
  - A bank starts with the first cash sale at $0. To start with change on hand, type it on the Drawer page and touch **Start Bank**.
  - **Check out** at the end of the shift: Log Out → **My Bank…**, count your cash, type it, **Check Out**. It shows *Cash to turn in*, then over / short. By default, open checks have to be closed or handed over first. Turn that off in Manager → Settings (*Close all checks before checking out*), or set it per person in Manager → Employees (*Checking out with open checks*).
  - Pay outs, paid ins and tip cash-outs come out of the bank of whoever does them.
- **Expenses.** A pay out is an expense: the manager touches what it was for (Food & supplies, Produce, Ice, Cleaning, Repairs, Other; Manager → Settings → *Expense categories*), types the amount and an optional reason, and touches **Pay Out**. Reports → **Expenses** adds them up by category and lists each one.
  - Managers see every other open bank on the Drawer page and can **Count** one for a server who left. End of Day waits until every bank is counted.
- **Terminals without a drawer:** Manager → Settings → *Terminals have a cash drawer* is the default; Manager → Terminals → *Cash drawer* sets it per terminal (e.g. the host stand has none). There, people on drawer handling can't take cash (they're told to use a terminal with a drawer or their own bank), No Sale is off, and nothing kicks a drawer. People with their own bank work as usual.
- **Per person:** Manager → Employees → *Cash handling* overrides the store setting for one person: *Own bank* or *Terminal's cash drawer*. For example, servers carry banks while the counter cashier rings into the counter's drawer; the drawer opens only for the cashier's cash sales.
- **Drawers per terminal** (the other *Cash handling* choice). Each terminal starts, counts and balances its own drawer ("Bar drawer"). Cash payments need this terminal's drawer open, and End of Day waits until every drawer is counted.
- **Pay outs and paid ins** (Drawer page, manager): type the amount, optionally touch Reason…, then Pay Out or Paid In. They show in the drawer's expected cash and on the Drawers report.
- **Cash out tips.** Logout → Cash Out My Tips pays the card tips owed to you from this terminal's drawer.
- **Manager → Terminals** sets each terminal's receipt printer (and so its drawer kick).

## Running the POS

Log in with a demo PIN: **1234** (manager), **1111** (server) or **2222** (cashier). Then:

1. Touch a table on the floor plan and enter the number of guests. Quick Order and Takeout skip the table.
2. Order from the menu pages. Burgers walk through Temperature and then Side. Tap No / Lite / Side and the touched item's choices open; touch the topping ("No Onion"). Or hold a choice on the Choose page for No / Lite / Extra / On the Side. Extra on a whole item is a bigger portion; Sub, then a substitute item, swaps a side. Touch a line in the check to select it.
3. **Send** the order. **Void** removes an item that hasn't been sent; voiding a sent item needs a manager.
4. **Pay**, then type an amount (or use the balance due) and choose a payment: Cash gives change, card payments are capped at the balance, and 10% Off and Comp are discounts. Then **Close Check**. Cash needs an open drawer: **Drawer…** on the Settle page.
5. **Split Check** on the Settle page moves items to another check at the same table. Touching a table that has several checks lets you choose one.

**Menu choices, prices and 86:**
- **Modifier groups** (Manager → Modifier Groups) are the choices an item asks for: *Dressing* (choose 1), *Add a Protein* (optional, priced), *Fillings* (up to 3). Give items their groups in Manager → Menu. Ordering such an item opens the **Choose** page beside the check. Required groups must be chosen before **Done**, and **Cancel Item** takes it off; **Choose** on an item already on the check opens the same page with **Cancel Changes**, which puts its choices back (`choosingBefore_`) and never takes it off. Modifier pages (the burgers' Temperature → Side) still work, so use whichever suits each item. The demo menu shows both: burgers use modifier pages. These items use groups:
  - salads: dressing, protein
  - eggs and omelettes: how cooked, fillings, toast
  - pancakes and French toast: syrup
  - add-ons on breakfast items
  - the Kids Burger: side and drink
  - burgers: toppings (lettuce, tomato, onion, pickles, mayo; cheese, bacon, avocado, jalapeños at a price)
  - drinks: soda, lemonade and juice sizes, coffee extras, tea hot or iced, draft beer choice, wine color and glass or bottle
- **Qualifiers on choices.** With No / Lite / Extra / Side lit, the next choice touched is had that way (*No Onion*, *Extra Bacon* at the Extra price, *Lite Mayo*, *Side of Ranch*); arming No / Lite / Side on a menu page opens the touched (or newest) unsent item's Choose page. Holding a choice opens No / Lite / Extra / On the Side for it. The same choice the same way again takes it off; another way changes it. "No" choices don't count toward a group's minimum or maximum, and a choose-one group's choice stays. No / Lite / Side on a whole item waits for a choice instead (Extra on an item is a bigger portion; Sub swaps in a substitute).
- An item still missing a required choice can't be sent, fired or closed. Its **Choose** button on the check (amber) reopens its choices.
- **Prices by meal period** (Manager → Menu → *Prices by meal period*, e.g. `dinner = 14.50`): an item costs that during the period. Make a *Happy Hour* meal period for happy-hour prices. Items already on a check keep their price.
- **Sold out (86):** Check Options or Manager → **Sold Out (86)…**. Touch an item to 86 it or bring it back. Its menu buttons say **SOLD OUT** and can't be ordered, on every terminal. Anyone taking orders can do this.

**Managing checks** (the order screen's **Check…** tab):
- **Transfer** a check to another server, **move** it to another table, or **merge** another open check into it (items, payments and guests come along). Servers can do this to their own checks, managers to anyone's.
- **Reopen** a check closed today (managers). Its cash leaves the drawer or bank until it is closed again. A refund is a reopen: void the item or take off the payment, then close.
- Every check keeps a **history**: who transferred, moved, merged, reopened, voided or discounted it, and when. It's on the Check Options page.

**Seats and courses** (the row at the top of the check):
- **Seat − / +** sets the seat for the next items. Touch an item first to move it to another seat. Kitchen tickets and the kitchen screen show "S2".
- **Course 1 2 3** sets the course for the next items (or a touched one). **Send** sends course 1 and anything already fired. Later courses wait, marked **HOLD**, until **Fire Course 2** sends them with a "COURSE 2" ticket. Closing a check sends anything still held.
- A counter that doesn't need seats or courses hides the row with the order list's `props.controls: false`.

**Manager** (PIN 1234 → Manager):
- **Menu, Employees, Payments (Tenders), Printers, Taxes, Settings, Terminals, Meal Periods, Modifier Groups:** edit and Save.
- **Breaks and overtime:** Log Out → **Start / End Break** (clocking out ends a break too). The Labor report shows each punch's breaks and worked hours, then each person's hours today and this pay week, regular and overtime. Manager → Settings: *Breaks are paid* (off by default), *Overtime after hours in a day* (off; 8 in California), *Overtime after hours in a week* (40, the US federal rule) and *Pay week starts on*. When both rules apply, whichever gives more overtime counts.
- **Permissions per person** (Manager → Employees): the role (server, cashier, manager, admin) sets the defaults. Each permission can then be set to *As the role*, *Yes* or *No* for one person: take orders, take payments, give discounts and comps, void sent items, manager screens, edit pages. For example, a lead server who may void, or a manager who doesn't edit pages. Staff are deactivated rather than deleted. PINs must be unique, and you can't lock yourself out.
- **Reports:** Sales, Items, Categories (with each one's share), By Hour, Servers, Tips, Labor, Drawer and **Audit** (every void, discount, reopened, moved, transferred and merged check, with who and when), for today (live) or any closed day (◀ ▶). **Print** sends a report to the receipt printer. **CSV** and **PDF** save it to `exports/` in the data folder, or the folder given with `--export-dir` (e.g. a USB stick).
- **Drawers:** start with a bank, then count at the end of the shift to see over or short. No Sale opens the drawer.
- **End of Day:** once every check is settled and the drawer is counted, this saves the day's reports and starts a new day.

**Printing**
- **When tickets print:** Send prints a kitchen ticket per station (each menu item names its printer, such as kitchen or bar). Voiding a sent item prints a VOID ticket. Closing a cash sale opens the drawer.
- **Where they go:** the starter printers write text files to `printouts/` in the data folder (`~/.local/share/ViewTouch/ViewTouch/`, or `/var/lib/viewtouch` when installed). In Manager → Printers, switch them to **Network** (raw TCP 9100, ESC/POS) or **CUPS**.
- **Printer failures:** printing runs on its own thread. A printer that is off or unplugged is retried and then reported on screen; it never freezes the POS.

**Behind the scenes**
- Every change to a check is saved to SQLite at once, on a background thread, so an open check survives a crash or restart. Tax follows the legacy rule: each tax class is taxed on its total, rounding half away from zero.
- Starter menu, staff and tax settings come from `seed/pos/`. `--reset-menu` reloads them without touching checks.
- Pages can require a permission (the inspector's *Who may open it*). The Manager page needs `manager`. Editing pages needs `layout.edit`, which the manager has.

**Widgets that work** (add them from *+ Panel* in the editor)

| Widget | What it does |
|---|---|
| loginPad | PIN entry |
| table | One table: its text is the table's name, `props.seats` its seats. Shows free / yours / someone else's |
| guestCount | Number of guests |
| numPad | Number or amount entry (`props.mode: "amount"`) |
| orderList | The current check |
| paymentPanel | Payments and the balance |
| checkList | Open checks |
| keyboard | Kitchen notes |
| clock | Time and date |
| logoutPanel | Who is on shift |
| statusBar | Latest message |
| kitchenDisplay | Kitchen/bar tickets (`props.station`: kitchen, bar, or empty for all) |
| customerInfo | Takeout/delivery customer details |

Manager widgets:

| Widget | What it does |
|---|---|
| adminPanel | Manager editors (`props.panel`: menu, employees, tenders, printers, taxes, store, terminals, mealPeriods) |
| reportView | Reports |
| drawerPanel | Cash drawer |
| endOfDay | End of day |
| splitCheck | Split a check |

## Preview on other screens

Edit mode → **Preview…** shows the page as drawn on a 6.1" phone, a 10.2" tablet, a 15.6" terminal and a 21.5" upright kiosk (fitted the way each would show it), with the size its smallest button comes out on each in millimeters (about 9 mm suits a finger) and whether the page has a phone version phones use instead (`EditorController::previewInfo`, `PagePreview.qml`).

## Ready-made layouts, and page files

Edit mode → **Layouts…** shows five arrangements of the page being edited, with a preview of each: the login page, the tables, the order screen (from any menu page: the frame around it), Pay and the kitchen screen. They're the same buttons (same ids and actions) moved and resized, so nothing stops working; one touch applies one, Undo takes it back, Save keeps it. The kitchen's side-by-side layouts (Grill | Fryer, Grill | Fryer | Cold, Kitchen | Bar, Kitchen | Expo) keep each panel's station (`props.lockStation`). They're generated in `tools/gen_seed.py` into `seed/layouts/<page>.json`.

The same window shares designs between stores: **Use a Page File for This Page…** (someone's exported page replaces this one's zones and background, keeping its name and role), **Add a Page from a File…**, **Export This Page…**, and for the whole restaurant **Export Every Page…** and **Replace Every Page from a File…** (after a confirmation). Page files are `.vtpage.json`, whole layouts `.vtlayout.json`. They carry the store pictures their pages use and the store's fonts (`"images": {name: base64}`); importing adds the ones the store doesn't have, so another store gets the whole look in one file (its own logo stays its own).

## The setup guide

The first time a manager logs in to a new store, the **setup guide** opens (Manager → **Setup Guide…** brings it back): the store's name and receipt lines, the logo (and printing it on receipts), a look (two made from the logo), food and alcohol tax, first menu items (they show on the *Everything* page at once), and the team: add yourself as a manager with your own PIN, then **Turn Off the Sample Staff**, whose PINs are public. The sample manager running the guide is turned off at *Finish* and logged out. *Finish Later* closes it until a manager logs in again. The demo's staff are marked `sample` in `employees.json`; `settings.setupDone` records that it's finished.

## Ready-made looks

Edit mode → **Theme** tab → **Looks**: ViewTouch Dark, Daylight, High Contrast, Warm Café, Ocean, Forest and Berry, plus two made from the store logo's own colors (dark and light) once a logo is set. One touch recolors the theme (background, buttons, panels, text, the pressed/chosen color, the panels' own buttons); fonts, sizes and each page's own colors stay. It's one Undo step; Save keeps it. Text on any button turns dark or white by itself when its color wouldn't read on the button's fill (WCAG AA contrast).

## Receipts: where they print, after paying, email

A printer with **Prints receipts** on (`PrinterConfig::receipts`; on by default for the `receipt` printer and printers with a drawer) is offered wherever a screen asks where a receipt goes. Manager → Terminals → **Receipts and cash drawer on** → *Ask each time* (`receiptPrinter: "ask"`): **Print Receipt** and Find a Check's **Reprint Receipt** offer those printers instead of printing (a handheld prints where the server is). **After a check is paid** (`TerminalConfig::afterPaying`): empty (nothing), `print` (on the terminal's printer; asked where if it's *Ask each time*), or `ask` (Print / Email / No Receipt). The offer is `PosSession::receiptOffer` (`{checkId, label, total, printers, canEmail, email}`), answered with `printReceiptOn`, `emailReceipt`, `noReceipt` (`app/pos_receipts.cpp`); the sheet is in `Main.qml` (`receiptSheet`). **Email** works for cards from a Stripe reader (`processor: stripe`, a `pi_` reference): the store's computer reads the payment intent's `latest_charge` and sets the charge's `receipt_email`, and Stripe emails its receipt (live mode only). The customer's email on file is filled in; a typed one is saved to a customer that has none.

## Out of paper, cover open, not answering

`print::PrinterMonitor` asks each network ESC/POS printer with *Warn when it's out of paper* on (`PrinterConfig::watch`, on by default) for its real-time status every 20 seconds, on its own thread: `DLE EOT 2` (cover open, stopped at the end of the paper, an error) and `DLE EOT 4` (the paper sensor: near the end, out); `problemFromStatus` reads the answers. It never asks while a ticket is waiting for or on its way to that printer (`PrintSpooler::busy`), asks again at once when a ticket fails, and calls a printer *not answering* after two missed connections in a row. Problems go to `PosShared::printerProblems` and every screen's `printerAlerts`: a banner at the top of every staff screen (red; amber for paper running low), not on the kiosk, until it's fixed; Manager → Network shows them too. Printers that don't answer status requests show nothing (turn the setting off for them).

## Testing a printer

Manager → Printers → a printer → **Test Print** (and **Test Print + Open Drawer** when a drawer is wired to it) sends a test page with the current saved settings: the store logo (ESC/POS), normal, bold, big and right-aligned text, a receipt-style item and total, a ruler of digits that should exactly fill one line (so *Characters per line* is right), accented letters, then the cut. "Receipt took the test page" appears when the printer accepted it; otherwise the usual "did not print" message says why.

**Everything printed fits the paper** (`print/document.cpp`): text is cleaned before it's measured (letters composed, typed line breaks start a new row, tabs and control characters can't reach the printer as commands, “quotes” and … made plain, emoji and decorative symbols dropped), so one character is one column. A line whose right side leaves the left fewer than 10 columns (narrow paper, report rows with several numbers) prints the left on its own rows and the right under it; a long word after an indent (a Stripe reference) is split instead of leaving a blank row. Printers have **Letters** (`PrinterConfig::charset`): empty = PC858 (`ESC t 19`: accented letters, ñ ¿ ¡ £ €; Epson and most), `ascii` = plain letters (é → e, € → EUR, ½ → 1/2). Store Settings → **Free choices on receipts** (`receiptFreeChoices`, off): the guest's receipt shows only choices that change the price; kitchen tickets show them all.

## Hold a button to see what it does

Holding any button (0.7 s) shows a card with its name and what it does, in plain words, instead of doing it: "Adds Classic Burger ($11.50) to the check", "Sends the new items to the kitchen and bar", "Opens the Settle screen". Built from the button's actions (`LayoutController::explain`, `describeAction`), so custom buttons explain themselves too. A touch on the card, or any button, closes it.

## Popular today

Each menu (Breakfast, Lunch, Dinner) has a **Popular** card: a page whose buttons are today's best sellers, most sold first (closed and open checks, not practice), up to 24. It fills in as the day goes; no editing. It's a menuGrid panel with *Today's best sellers* on (`PosSession::popularItems`).

## Another Round

Once drinks have been sent on a check, **Another Round** appears beside its name: it orders the drinks of the latest Send again (same choices and quantities) as new lines, ready for Send. Drinks are what the bar prints, or items in a drinks family; sold-out ones are skipped and named. Command `anotherRound`; the order list's built-in "round" button.

## Arrange the menu by touch

On the self-filling menu (Everything), a manager touches **Arrange…**: touching an item picks it (instead of ordering it), **◀ Earlier** / **Later ▶** move it within its family, the swatches give its button a color (dark text on light colors), **No color** takes it off, **Done** goes back to ordering. Saved with the menu (`MenuItem::buttonColor`, `PosService::moveMenuItem`, `setMenuItemColor`).

## A look per terminal

Manager → Terminals → a terminal → **Look**: one of the looks (or the store's). That screen shows every page recolored with it; the saved pages and other screens keep the store's look (`TerminalConfig::look`, `PosSession::terminalLook`; the controller keeps a recolored copy of the layout for this screen).

## Course pacing

With a later course on hold, **⏱** next to **Fire Course 2** offers Now, or in 5, 10, 15 or 20 minutes; the button then reads "Course 2 at 7:45" and the course fires by itself then (the same 30-second check as orders for later; `Check::fireAt`, `PosService::fireCourseIn`). Firing it by hand earlier, or "Don't fire it at…", ends the wait.

## Kitchen times

Each bump on a kitchen screen teaches the item's usual time (sent to made; a running average in `PosSettings::prepSeconds`, ignoring tickets over 90 minutes and practice). Manager → Menu → **Kitchen time (minutes)** sets one instead. A ticket's target is its slowest item's: it shows "7:32 / 12m", turns amber at three quarters of it and red **LATE** two minutes past it. Tickets with no times known keep the store's warn / late minutes.

## Opening and closing checklists

Manager → Settings has an **Opening checklist** and a **Closing checklist** (one task per line). The **Checklists** page (Manager page, and the Log Out page for staff) shows both; touching a task ticks it with who and when (again to undo). Ticks belong to the business day. End of Day says how many closing tasks are left (not a blocker), and the **Checklists** report (saved with each day) lists every task, who did it and when, or NOT DONE.

## Time off and shift swaps

On the Time Clock (by PIN, not logged in): **Ask for Time Off…** picks a day in the next four weeks and an optional reason; **Give Away** on a shift puts it *up for grabs*, and everyone else's Time Clock lists it with **Take It**. A manager decides under Schedule → **Requests…** (an admin form: Approve / Don't approve); approving a swap moves the shift to whoever took it, and a time-off request on a scheduled day says so. Each person sees their requests' status; the dashboard shows how many wait. Kept in `PosSettings::staffRequests` (pos_requests.cpp).

## Overtime warnings

Using the store's overtime rules (Manager → Store: daily and weekly hours, pay week start), the Time Clock shows each person's hours this week and, within two hours of overtime, "1.5 h until overtime" (red once they're in it); clocking in that close says so on screen; and the dashboard's On the clock list marks them "OT in 1.5 h" or "overtime". `PosService::overtimeFor`.

## Ring items in by number

Each menu item can have a **Number** (Manager → Menu; the demo's burgers are 101-107, salads 201-204, drinks 301-308, breakfast 401-404). On the order screen with a keyboard, type the number and press Enter (Backspace, Escape); on the Find page, typing digits puts the item with that number first. The self-filling menu shows each item's number in its corner.

## Dashboard

Manager → **Dashboard** (first on the Manager page): today so far. Net sales against the same weekday last week by this time (read from the store, cached five minutes), checks, guests and the average check, labor cost as a share of sales (amber over 28%, red over 35%), open checks and what's still due on them, the kitchen's average sent-to-made time and what's waiting, who's on the clock, the five best sellers, and ingredients running low with what's sold out. Managers only (`PosSession::dashboard`, the `dashboard` panel); it updates as checks close and orders go out, and once a minute while it's on screen.

## Running low

When an ingredient is down to its *Low at* (Manager → Inventory), every dish that uses it shows **N left** on its button (and in the self-filling menu): how many can still be made from what's on hand. It counts down as orders are sent; at zero the dish sells out by itself, and comes back when stock is received (as before). `PosSession::stockLeft`, the zone's `itemId` role.

## Fixing time punches

Manager → Schedule → **Time Punches…** lists the last month's punches, newest first (the week in memory, older ones read from the store when the list is opened). A manager changes the times, breaks (one per line, `12:30-13:00`) or job, adds a punch someone missed, or removes one made by mistake (tick *Remove this punch*). Every change needs a reason; overlapping punches, times in the future and shifts over 24 hours are refused. Changes are kept (`PosSettings::punchChanges`, the last 500): the Labor report marks changed punches with * and lists each change, who made it and why. End of Day lists whoever is still clocked in, flags anyone on for over 12 hours ("forgot to clock out?"), and **Clock Out Now** closes their punch (also logged).

## Time Clock

A screen just for clocking in and out and checking the schedule. **Time Clock & My Schedule** on the login page opens it; a terminal set to **Time Clock** in Manager → Terminals (a tablet by the back door, the Android app too) rests on it instead of the login page. Someone types their PIN (they don't log in to the register) and sees whether they're clocked in, today's hours, and their shifts for the next two weeks with this week's total; **Clock In** (asking which job when they have more than one), **Start / End Break**, **Clock Out**. It goes back to the keypad after 20 seconds, or **Done**. **Log In to the Register…** is there for managers. The `timeClock` panel (`PosSession::timeClock`, `timeClockStart`, `timeClockAct`, `timeClockDone`), page role `timeClock`.

## Discounts need a manager

Servers, bartenders and cashiers no longer have the discount permission: 10% Off, Comp, Staff Meal and custom discounts ask for a manager's PIN (the approval pad), unless the person is allowed it in Employees. On the Pay screen **$ Off** and **% Off** discount what's typed on the keypad (cents, or a whole percent); commands `amountOff` / `percentOff` (`PosService::customDiscount`).

## Undo

Taking an unsent item off (Void, or − on the last one) or making it fewer shows **Removed Bacon Burger · Undo** at the bottom of the check for a few seconds; **Undo** puts it back where it was, choices and all (or the number it was). Only on the check it happened on, once, within 30 seconds. `PosSession::undoText`, `undoLast`; the bar is the order list's built-in "undo" button.

## How many: − 2 + and Again

The touched line on the check (a new item is the touched one) shows **− 2 +** and **Again**. **+** and **−** change how many (the line reads "3 × Bacon Burger", priced, printed and counted as 3); **−** on the last one takes it off. **Again** adds one more the same way, choices and all, as its own line, so it can be changed on its own. A line already sent can't change: it shows only **Again**, which adds a new line for the next Send (taking one off is still a Void). Weighed items, gift cards and notes have no quantity. Commands for buttons: `lineMore`, `lineLess`, `repeatLine`; the bar is the order list's built-in "quantity" button.

## Separate checks at one table

On a table's order screen the check panel has a row of the table's checks: **Check 1 · Check 2 · + Check** (just **1 2 3 4 5 6 +** from three on). It stays one line however many there are: what doesn't fit is behind **‹ ›**, the open check is always on the part shown, and **+** stays at the right end. **+** opens another check at the same table, right there; touching a number switches to that check without leaving the page. **⋯** beside **+** has **One Check per Seat** (items with no seat stay; else the first seat does), **Print Every Check** and **Put Them Back Together** (all merged into this one). A touched line at a table has **Move…**: to another of the table's checks or a new one. Commands `splitBySeat`, `printTableChecks`, `combineTableChecks`. Each check is its own (its own items, Send, Pay); the header says which one is open ("T5 · Check 3"), and touching the table on the floor plan asks which check. `PosSession::tableChecks`, `newTableCheck` (also a button command), `switchCheck`; the row is the order list's built-in "tableChecks" button, so the editor can hide or rename it.

## Each person's own screen, and a start screen per job

Manager → Employees has, for each person: **Text size** (Normal, Bigger, Biggest: button and check text), **Left-handed** (on the order and Pay screens the check's column moves to the right and what was beside it to the left, each row in its order; what's below the check stays), **Start screen** (the page they land on after login) and the **Language** they already had. Manager → Store sets a start screen for each job (servers, bartenders, cashiers, hosts, managers); the job is the one they clocked in as, else their role. With none, everyone starts on the floor plan. These come with the session (`PosSession::userPrefs`), so remote terminals follow them too.

## Switch User

**Switch User** (on the floor plan, and on the order screen's **Check…** page) logs this person out so the next one can type their PIN, without closing anything. The check they were on is remembered: at their next login, on any terminal, it opens again on its order screen ("Welcome back, Sam: Takeout 1 is open again"), unless someone else has it open. The same happens after an automatic logout when the screen sits idle.

## Find an item by typing

**Find** on the order screen opens a keyboard with the menu above it: type part of a name ("cob" finds Cobb, "bur" every burger) and touch the item. Names that start with it come first, then names with a word that does. After ordering, the text clears for the next one; items with choices or a weight ask for them as usual. It's a menuGrid panel with *Find by name* on, next to a keyboard panel, so any page can have one.

## The menu laid out by itself

A **menuGrid** panel (+ Panel → menuGrid) shows the menu as buttons by itself: one family (its *Family* setting) or every family with a button for each across the top. New items, price changes and sold-out items show up with no page editing; items with choices or a weight ask for them as their own buttons would. Settings: *Columns*, *Show photos*. The demo's menu pages have an **Everything** button that opens one.

## Pictures and the store logo

The store keeps its own pictures (a logo, photos for buttons, backgrounds) in its database, so backups, the standby server and every paired screen have them. Anywhere a picture can go (a button, an image zone, a page or theme background, a menu item's kiosk photo, the store logo) there's a **Picture** field: pick one of the store's pictures or *The store logo*, see a preview, or touch **Add Picture…** to bring one in from the computer in front of you (PNG, JPEG, WebP, GIF, BMP or SVG, up to 8 MB; "Our Logo.PNG" becomes `store:our-logo.png`).

- **Store Settings → Store logo** shows on the login page (its *logo* zone stays hidden until a logo is set; the title shows `{store.name}`), on the screen saver, the customer display and the kiosk.
- **Print the logo on receipts** puts it at the top of every receipt on ESC/POS printers: scaled to three quarters of the paper (and about 2.5 cm tall at most), transparent parts white, gray dithered to dots, sent as `GS v 0` raster bands. Plain-text printers skip it.
- **Image buttons:** *+ Image* in the editor, then its Picture and *When touched*. Any regular button can have a picture too.
- **Fonts:** every font list (a button's, a page's, the theme's) has **Add Font…**: a TTF or OTF file joins the store's pictures and fonts, and every screen installs it, so it shows up in all the font lists there.
- **Backgrounds:** Page (or Theme) → Background → *Picture*, and *Picture fits*: fill the page, the whole picture, stretched, tiled or centered.

## Editing pages

**Tables** are zones like buttons. On the Tables page press F1 (or Ctrl+E), then *+ Panel ▾ → table* to add one, or select a table and *Duplicate* (T7 becomes T8, Bar 3 becomes Bar 4). Drag and resize them, pick a shape (circle, octagon…) and colors, and set *Table name* and *Seats* in the inspector. A table's name is the table: what checks and kitchen tickets show, and on one page each table needs its own. The same name on another page is the same table, so the starter floor plans (`tools/floorplans.py` writes them) are **Main** (page `tables`: every table, the walls, doors and patio) and **Dining Area**, **Bar** and **Patio** (`floor-dining`, `floor-bar`, `floor-patio`: each room bigger), with tabs between them; T1 open shows on Main and on Dining Area alike, and `LayoutController::tables()` lists each name once (sorted, numbers as numbers). Pasting a table onto another page keeps its name; pasting it where that name already is makes a new table.

**Store Settings → About** (the last section, read only: FieldEditor type `info`): the store's version, build number and commit, its computer, data and backups folders, when it started (`PosService::aboutValues`, from the `network` hook in `main.cpp`), and this screen's own build (`LayoutController::screenBuild`) and size; the license and source. 

**Versions and builds.** The version is MAJOR.MINOR, set by hand in `project(ViewTouchModern VERSION 0.9)` for a release, and the build number, added by every build: 0.9.1283, 0.9.1284… The build number is the commits so far (`git rev-list --count HEAD`), so a newer build is always a higher number; tablets and screens update from it. `cmake/BuildInfo.cmake` runs at every build and writes `generated/vtm_build_number.hh` (number, commit, version, and *modified* when the program's files have changes not committed yet) and `generated/vtm_history.inc` (the last 60 builds that changed the program: number, version, date, what changed: `net::appHistory()`). Packages are named with the same version (`CPACK_PACKAGE_VERSION`), Android's versionName too. Each store remembers the builds it has run and when each first started there (`PosSettings::builds`, added at startup). About shows both. `CHANGELOG.md` lists every build, by version: `python3 tools/changelog.py` rewrites it (do it when releasing).

**The floorPlan panel** (`WidgetFloorPlan.qml`, the phone floors and Move Check; `FloorPlanView.qml` is shared with the seating screen) draws a plan from `LayoutController::floorPlan(id)`: a tab for each plan (`props.tabs`), the tables where they stand, colored by whose check is on them, touched to open (`props.action` *move*: to move the check there); `props.plan` the one to start on (a room's phone page: its room), else the last picked. Two fingers zoom up to 3×; in a tall space a wide plan is turned a quarter when that makes it at least 25% bigger, words upright, rooms' names drawn over the tables.

**Floor plan pieces** are `decor` zones (*+ Floor Plan ▾*): `props.decor` is wall, window, door, counter, booth, plant, pillar, fence, entrance or area (a room's floor, its name in the corner), `props.color` their own color; drawn by `WidgetDecor.qml`, never touchable, added just under the tables (an area under everything). On a phone they're left out and the tables laid out in rows. Tables can show a chair for each seat (`props.chairs`).

**Turning:** any zone can be turned about its middle, `"rotation"` in degrees clockwise (kept with the zone's other keys, so older layouts load unchanged; `layout::rotationOf`). In the editor: the round handle above a single selection (15° steps, Shift free), *Turn 90° / 15° / Straighten* (hold menu, Arrange), or *Turned (degrees)*; selection, picking and resize handles follow the turn (`EditLayer.turn`). A turned table's name stays upright. Floor plans saved before tables became zones (a single *tableMap* panel) turn into separate table zones when loaded, each in the same place.

Press **F1** or **Ctrl+E** (no Fn needed), or touch **Manager → Edit Pages**. Changes go into a draft. The running app keeps the saved pages until you press **Save**, which writes to the database in one transaction. **Done** asks whether to save or discard.

**Mouse**
- Click a zone to select it. Shift- or Ctrl-click adds it to the selection or removes it.
- Drag across empty canvas to select every zone the box touches.
- Drag a zone to move it. Drag one of its 8 handles to resize it.
- Zones snap to the page grid, or to pink guides at other zones' edges and centers and the canvas center.
- Zones from a template are dimmed and locked. Double-click one to open its template page.
- Right-click a zone, or hold it (a touchscreen), for copy, cut, duplicate, delete and layer order. Toolbar menus open under their buttons; on a narrow screen the tools scroll sideways and Save / Done stay in view. New zones go in the free space nearest the middle.

**Keys**

| Key | Action |
|---|---|
| Arrows | Move one grid step |
| Shift+Arrows | Move one unit |
| Ctrl+D | Duplicate |
| Del / Backspace | Delete |
| Ctrl+C / X / V | Copy, cut, paste (works across pages) |
| Ctrl+A | Select all |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z or Ctrl+Y | Redo |
| Ctrl+] / Ctrl+[ | Bring to front / send to back |
| PgUp / PgDn | Previous / next page |
| Ctrl+S | Save |
| Esc | Clear selection |

**Panels**
- **Left: page list.** Create, copy or delete pages. Delete lists the buttons that lead to the page first.
- **Right: inspector, for the zone, the page, or the theme.** Every field is built from `layout/schema.cpp`, so a new property needs one line there and no QML. Style fields show the inherited value until you override them, and ↺ resets them. "When touched" edits the action list.
- **Panels (widgets).** Selecting one shows its **Settings** (a kitchen screen's station or expo mode, a check list's mode, the report to open, table columns…), its **Built-in buttons** (hide each one, all of them, or rename them: `props.buttons.<id>.hide / .label`, `props.hideButtons`) and their **Built-in button look** (`keyFill`, `keyTextColor`, `keyLitFill`, `keyFont`, `keyRadius`, inherited from the page and the theme like any style; form buttons follow it through the palette). Every built-in button has a command for a regular button placed anywhere: `kitchenStation`, `kitchenAllDay`, `recallTicket`, `expoRecall`, `seatNext`, `seatPrev`, `courseNext`, `fireCourse`, `finishChoosing`, `cancelChoosing`, `guestsMore`, `guestsFewer`, `noSale`, `countDrawer`, `payout`, `paidIn`, `backupNow`. The list is `schema::builtInButtons()`.
- **Order of built-in buttons.** On the kitchen screen, the choices panel and the drawer panel, each built-in button has a *Position* (1 = first in its row; `props.buttons.<id>.order`).
- **Status colors.** Theme → *Status colors*: a table with a check, with my check, being worked on, seated a long time; kitchen tickets new, getting old and late; the expediter's ready orders; the sold-out badge (theme `status.tableMine`, `status.kitchenLate`…).
- **Show only when.** Any zone can have rules (`showWhen`): who is logged in (anyone, nobody, a manager), whether a check is open, the kind of check (dine in, takeout, delivery, bar tab, quick), the meal period, and the screen (phone or standard). The zone is on the page only while all of them hold; a hidden button's keyboard key does nothing. In edit mode every zone shows, dimmed when its rules hide it.
- **Live text.** A button's or label's text can include `{check.total}`, `{check.balance}`, `{check.subtotal}`, `{check.tax}`, `{check.label}`, `{check.number}`, `{check.guests}`, `{check.items}`, `{check.server}`, `{check.customer}`, `{check.due}`, `{user.name}`, `{user.role}`, `{store.name}`, `{terminal}`, `{entry}`, `{typed}`, `{time}`, `{date}` or `{mealPeriod}`; they're filled in and kept up to date. For example: "Pay {check.balance}".
- **Toolbar.** Add a button, text, image, note or panel; undo and redo; arrange (align, space evenly, match size); and File, which exports or imports one page (`.vtpage.json`) or every page (`.vtlayout.json`).

## Layout

```
core/     pure C++ domain: money, tax, menu, checks, employees — no Qt
layout/   page / zone / action model, JSON, field schema
app/      navigator, layout editor (undo), PosSession API, PosService + PosShared store, JSON mapping
net/      terminal server, remote session, page sync (newline-delimited JSON over TCP)
storage/  SQLite: layout + POS stores, background AsyncWriter
print/    ticket formatting (text + ESC/POS) and the background PrintSpooler
ui/cpp/   QML-facing controllers (LayoutController, EditorController)
ui/qml/   QML views and the editor (static QML module 'ViewTouch')
seed/     starter pages, theme, menu, staff, settings (generated by tools/gen_seed.py)
tests/    Catch2 unit tests
```

## Building

Prerequisites: a C++23 compiler, CMake, Ninja and Qt 6.8 or newer. `sudo modern/packaging/build-packages.sh deps` installs them on Fedora, Debian and Ubuntu, or by hand on Fedora:

```sh
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel
```

```sh
cmake -S modern -B modern/build -G Ninja
cmake --build modern/build
ctest --test-dir modern/build
./modern/build/vtmodern                   # data in ~/.local/share/ViewTouch/ViewTouch/
./modern/build/vtmodern --reset-layout    # back to the starter pages
./modern/build/vtmodern --data-dir /tmp/t # use another data folder (database, backups, printouts)
./modern/build/vtmodern --windowed        # in a window (it starts full screen; F11 or Ctrl+Shift+F switches)
./modern/build/vtmodern --kiosk           # full screen, no mouse pointer, F11 off
./modern/build/vtmodern --login 1234      # start logged in (testing)
```

Tests run headless (offscreen). They include UI tests that drag, resize and type in the real editor.

If Qt6 is not found, only the core library and tests are built. To build it from the legacy root project instead, pass `-DVT_BUILD_MODERN=ON`.
