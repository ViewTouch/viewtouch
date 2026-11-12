#!/usr/bin/env python3
# A made-up legacy ViewTouch store's people and sales, in legacy's own file
# formats (the ones app/legacy reads), for the tests: settings.dat, staff,
# customers, three closed business days, the time clock and an open day
# with nothing open. Its menu and pages are the El Mirador fixture's
# (tests/fixtures/legacy/elmirador); everything here is invented.
#
#   python3 -I tools/legacy/make_sample.py tests/fixtures/legacy/sample
import gzip, os, sys

OUT = sys.argv[1]
D = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"


def i(v):
    """A legacy int: base 64, a negative one as 64 bits."""
    v &= (1 << 64) - 1
    if v == 0:
        return "A"
    s = ""
    while v:
        s = D[v & 63] + s
        v >>= 6
    return s


def s(text):
    return text.replace(" ", "_") if text else "~"


def t(year=0, day=0, hour=0, minute=0):
    """A legacy TimeInfo: seconds into the year (by the store's clock), the year."""
    if not year:
        return "A A"
    return f"{i(((day - 1) * 24 + hour) * 3600 + minute * 60)} {i(year)}"


def write(path, version, words, gz=False):
    text = f"vtpos 0 {version}\n" + " ".join(words) + "\n"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = text.encode()
    open(path, "wb").write(gzip.compress(data, mtime=0) if gz else data)


# --- settings.dat (version 107) ---------------------------------------------------
w = [s("Casa Sample"), s("100 Main Street"), s("Springfield, ST 00000"), "A", "A"]
w += [i(825), i(825), i(0), i(700), "A", "A", "A", "A", "A", "A"]          # taxes x 10000, royalty, VAT
w += ["A"] * 13 + ["2.0", "A"] + ["A"] * 5                                 # screens...rounding, double, change for...
w += ["A", "A A", "A", "A A", "A"]                                         # sales/labor periods, discount_alcohol
w += [s("en"), "A", "A", "A", "A", "A"]                                    # language, overtime, wage start, auth, always open
w += ["A"] * 5                                                             # item target, formats, measures
w += [i(2), i(6), "A", "A", i(12), "A", "A"]                               # jobs: server, manager
w += [s("Welcome!"), "~"]                                                  # customer display
w += [s("100 Main Street"), s("Phone 555-0100"), "~", "~"]                 # receipt header
w += [s("Thank you!"), s("Come again"), "~", "~"]                          # receipt footer
w += ["A", "A"] + [i(-1)] * 12 + ["A", "A"] * 12 + ["A"] * (64 * 3)        # flags, shifts, meals, families
w += [i(1)] + [s("Front"), "A", "A", s("front"), s("192.168.1.50"), "A", i(9100), i(1), "A", "A", "A", "A",
               i(1), "A", "~", "A", "~", "~", "A", "A", "A", "A", "A", "A"]                       # a terminal
w += [i(2), s("Kitchen"), s("192.168.1.60:9100"), i(9100), "A", i(1), "A", "A",
            s("Receipts"), s("192.168.1.61"), i(9100), "A", i(6), "A", "A"]                       # printers
w += [i(2), i(1), i(1), s("Ten Percent"), i(1), i(1000), "A", i(1)]                               # discounts
w += [i(1), i(1), i(1), s("Happy Hour"), i(1), i(1500), "A", i(1), i(255), i(-1),
      t(2000, 1, 15), t(2000, 1, 18), i(62), i(4095), i(1), "A A", "A A", "~"]                    # coupons
w += [i(1), i(1), i(1), s("Visa"), "A", i(1)]                                                     # credit cards
w += [i(1), i(1), i(1), s("Manager Comp"), "A", "A", i(1)]                                        # comps
w += [i(1), i(1), i(1), s("Staff Meal"), i(1), i(5000), i(1)]                                     # meals
w += ["A", "A", "A", "A"]                                                                         # taxes, money
# The rest, in Settings::Load's order (version 107).
w += ["A"] * 16 + ["~"] * 4 + ["A"] * 6          # order entry windows; VisaNet
w += ["~"] * 3 + ["A"]                            # update address, user, password; auto update vt_data
w += ["A", "A"]                                   # account numbers
w += ["A", i(20000), i(10000)]                    # allow_user_select; drawer day and night floats ($200, $100)
w += ["A", "A", "A", "A", "A", "A"]               # country, store code, drawer account; min day; drawer starts
w += ["~", "~", "~", "A", "A"]                    # email server, reply to; QuickBooks path, auto, format
w += ["A", s("$"), "A", "A"]                      # fast takeouts, money symbol, require balance, report period
w += ["A"] * 4 + ["A"] + ["A", "A"] + ["A"] + ["~"] + ["~"] * 4 + ["A", "A"] + ["~"] * 8   # card settings
w += ["A"] * 5 + ["~"] * 4                        # report header, timeout, card types, split view, separator; expire msgs
w += ["A"] * 3 + ["A"] + ["A", "A", "A", "A"]     # receipts, bar mode; midnight; multi coupons, login, iconify, emboss
w += ["A"] * 5 + ["A"] + ["A"] * 3 + ["A", "A"]   # antialias..blur; F3/F4; restart; button images
w += ["A"] + ["A"] * 3 + ["A", "A", "A", "A"]     # all modifiers; header length, numbers; drawer print, kv, tab, coupons
w += ["0.0"] + ["A", "A"] + ["A"] * 6 + ["A", "A"]   # ad fund; cc custinfo, kv user; kitchen video; timers, language
write(os.path.join(OUT, "settings.dat"), 107, w)

# --- employee.dat (version 8) ------------------------------------------------------
staff = [  # id, key (PIN), first, last, jobs [(job, pay rate, cents)]
    (1, 42, "Pat", "Manager", [(12, 1, 2000)]),
    (2, 1234, "Sam", "Server", [(6, 1, 800)]),
    (3, 5678, "Lee", "Cook", [(3, 1, 1600), (5, 1, 1200)]),
]
w = [i(len(staff))]
for eid, key, first, last, jobs in staff:
    w += [s(first.lower()), s(last), s(first), "~", "~", "~", "~", s("000-00-0000"), "~"]
    w += [i(eid), i(key), i(eid), "A", "~", i(1), i(len(jobs))]
    for job, rate, amount in jobs:
        w += [i(job), i(rate), i(amount), i(-1), "A"]
write(os.path.join(OUT, "employee.dat"), 8, w, gz=True)

# --- customers -----------------------------------------------------------------------
for cid, first, last, phone, comment in [(1, "Dana", "Regular", "555-0101", "no onions"),
                                          (2, "", "", "555-0102", "")]:
    w = [i(cid), s(last), s(first), "~", s(phone), s("1 Elm St"), s("Springfield"), s("ST"), s("00000"),
         s("4111111111111111"), s("12/30"), "~", s(comment), "~", "~", s("Oak Ave")]
    write(os.path.join(OUT, "customers", f"customer_{cid}"), 14, w)


# --- three closed days ------------------------------------------------------------------
def order(name, cost, family=11, typ=0, count=1, qualifier=0, status=39, seat=0):
    return [s(name), i(typ), i(cost), i(family), "A", i(count), i(qualifier), i(status), i(2), i(seat), "A", "A", "A", i(-1)]


def check(serial, day, hour, label, orders, payments, typ=1, guests=2, flags=0):
    w = [i(serial), t(2025, day, hour), i(2), i(2), i(flags), i(typ), i(-1), i(8), "A A", "A A", i(serial),
         "A A", "A A", "A A", "~", i(guests), "A", s(label), "A", i(1)]
    w += [i(2), i(2), t(2025, day, hour, 40), i(1), i(sum(len(o) // 14 for o in orders))]
    for o in orders:
        w += o
    w += [i(len(payments))]
    for tender, tid, amount, pflags in payments:
        w += [i(tender), i(tid), i(amount), i(pflags), i(1), i(2)]
    w += ["~", "A", "A", "A"]
    return w


for n, day in enumerate([10, 11, 12], 1):
    checks = [
        # 2 tacos plates and a soda, cash with change: 2 x 1195 + 250 = 2640, 8.25% tax 218 -> 2858
        check(100 * n + 1, day, 12, "5", [order("TACOS", 1195, count=2), order("Pepsi", 250, family=1)],
              [(0, 0, 3000, 128), (18, 0, 142, 0)]),
        # a takeout by card, with a 10% discount: 1495 - 150 = 1345, tax 111 -> 1456
        check(100 * n + 2, day, 13, "", [order("BURRITO", 1495), order("NO! RICE", 0, family=23, typ=1)],
              [(8, 1, 1000, 129), (2, 1, 1456, 128)], typ=2),
        # practice: never a sale
        check(100 * n + 3, day, 14, "9", [order("TACOS", 1195)], [(0, 0, 1294, 128)], flags=4),
    ]
    w = [i(n), t(2025, day, 9) if n > 1 else "A A", t(2025, day, 22)]
    w += [i(5), i(1)]                                          # drawers
    w += [i(1), s("front"), "A", t(2025, day, 9), t(2025, day, 22), t(2025, day, 22, 5), i(1), i(1), i(1), i(1),
          i(1), "A", "A", i(22858 - 20000 + 20000), i(1), i(19), i(500), i(1), i(1), t(2025, day, 15)]
    w += [i(25), i(len(checks))]
    for c in checks:
        w += c
    w += [i(1), "A"]                                           # tips
    w += [i(3), i(1), t(2025, day, 12, 30), i(1), i(2), "A", i(100 * n + 1), s("Pepsi"), i(250), i(1), "A", "A"]
    w += [i(8), "A", "A"]                                      # expenses
    w += [i(106), "A", "A", "A", "A", "A"]                     # the day's discounts... (none kept: settings')
    w += ["0.0825", "0.0825", "0.0", "0.07", "0.0", "0.0", "0.0", "0.0", "0.0"] + ["A"] * 6
    w += ["0.0"]                                               # VAT
    w += ["B A"] * 3 + ["B A"] * 3                             # card records: none
    w += ["0.0"]                                               # ad fund
    write(os.path.join(OUT, "archive", f"archive_{n:06d}"), 14, w, gz=True)

# --- the time clock -----------------------------------------------------------------------
entries = []
for day in [10, 11, 12]:
    entries.append([i(2), i(6), i(1), i(800), i(300), t(2025, day, 10), t(2025, day, 21), "A", "A"])
    entries.append([i(3), i(3), i(1), i(1600), "A", t(2025, day, 9, 30), t(2025, day, 20), "A", "A"])
w = [i(1), "A A", i(len(entries))]
for e in entries:
    w += e
write(os.path.join(OUT, "labor", "labor_000000001"), 4, w)

# --- the open day: just its drawer ----------------------------------------------------------
write(os.path.join(OUT, "current", "drawer_2"), 5,
      [i(2), s("front"), "A", t(2025, 12, 22), "A A", "A A", i(1), "A", i(1), "A", "A", "A"])
print("written", OUT)
