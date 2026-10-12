#!/usr/bin/env python3
"""Generate modern/seed (theme, starter pages, POS data). One-off helper; the JSON is the source of truth."""
import json, os, re, sys

OUT = sys.argv[1]
SCHEMA = 1
os.makedirs(os.path.join(OUT, "pages"), exist_ok=True)
os.makedirs(os.path.join(OUT, "pos"), exist_ok=True)
os.makedirs(os.path.join(OUT, "layouts"), exist_ok=True)

def write(path, obj, versioned=True):
    if versioned and isinstance(obj, dict):
        obj = {"schemaVersion": SCHEMA, **obj}
    with open(os.path.join(OUT, path), "w") as f:
        json.dump(obj, f, indent=2)
        f.write("\n")

def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")

def rect(x, y, w, h):
    return {"x": x, "y": y, "w": w, "h": h}

def jump(page=None, role=None, mode="push"):
    a = {"type": "jump", "mode": mode}
    if page: a["page"] = page
    if role: a["role"] = role
    return a

def command(name, **args):
    a = {"type": "command", "name": name}
    if args: a["args"] = args
    return a

def fill(color, **more):
    return {"normal": {"fill": color, **more}}

def zone(id, x, y, w, h, label="", kind="button", actions=(), **kw):
    z = {"id": id, "kind": kind, "rect": rect(x, y, w, h), "shape": kw.pop("shape", "rect"),
         "behavior": kw.pop("behavior", "blink" if kind in ("button", "image") else "none")}
    if label: z["label"] = label
    if actions: z["actions"] = list(actions)
    z.update(kw)
    return z

def label(id, x, y, w, h, text, **kw):
    return zone(id, x, y, w, h, text, kind="label", **kw)

PAGES = {}

def page(id, name, kind, zones, **kw):
    p = {"id": id, "name": name, "kind": kind, "canvas": {"w": 1920, "h": 1080}, "grid": 8}
    p.update(kw)
    p["zones"] = zones
    PAGES[id] = p
    write(f"pages/{id}.json", p)

# ---------------------------------------------------------------- POS data
MENU = []
def menu(name, price, family, tax="food", modifier=False, printer="kitchen"):
    item = {"id": slug(name), "name": name, "price": price, "family": family, "taxClass": tax,
            "printer": printer}
    if modifier:
        item["modifier"] = True
    MENU.append(item)
    return item["id"]

BURGERS = [("Classic Burger", 11.50), ("Cheeseburger", 12.25), ("Bacon Burger", 13.50),
           ("Mushroom Swiss", 13.25), ("Veggie Burger", 12.00), ("Kids Burger", 7.50),
           ("Burger of the Day", 14.00)]
SALADS = [("House Salad", 8.50), ("Caesar", 9.75), ("Cobb", 12.50), ("Greek", 10.25)]
DRINKS = [("Coffee", 2.75, "food"), ("Tea", 2.50, "food"), ("Soda", 2.95, "food"), ("Juice", 3.50, "food"),
          ("Water", 0.00, "food"), ("Lemonade", 3.25, "food"), ("Draft Beer", 6.00, "alcohol"),
          ("House Wine", 8.00, "alcohol")]
BREAKFAST = [("Two Eggs", 8.95), ("Pancakes", 9.50), ("French Toast", 9.75), ("Omelette", 11.25)]
TEMPS = ["Rare", "Medium Rare", "Medium", "Medium Well", "Well Done"]
SIDES = [("Fries", 0.00), ("Side Salad", 0.00), ("Onion Rings", 1.00), ("Sweet Potato Fries", 1.50),
         ("No Side", 0.00)]

for n, p in BURGERS: menu(n, p, "burgers")
for n, p in SALADS: menu(n, p, "salads")
for n, p, t in DRINKS: menu(n, p, "drinks", tax=t, printer="bar")
for n, p in BREAKFAST: menu(n, p, "breakfast")
# Numbers to ring items in by (101 the first burger, 201 the first salad...).
for base, names in ((100, BURGERS), (200, SALADS), (300, DRINKS), (400, BREAKFAST)):
    for i, entry in enumerate(names):
        next(m for m in MENU if m["name"] == entry[0])["number"] = str(base + 1 + i)

# Choices asked for when an item is ordered (the Choose page). Burgers keep
# their Temperature -> Side pages: both ways work.
def item(id):
    return next(m for m in MENU if m["id"] == id)
for sid in ("house-salad", "greek"):
    item(sid)["modifierGroups"] = ["dressing", "salad-protein"]
for sid in ("caesar", "cobb"):
    item(sid)["modifierGroups"] = ["salad-protein"]
item("two-eggs")["modifierGroups"] = ["eggs", "toast", "breakfast-add-ons"]
item("omelette")["modifierGroups"] = ["omelette-fillings", "toast", "breakfast-add-ons"]
for bid in ("pancakes", "french-toast"):
    item(bid)["modifierGroups"] = ["syrup", "breakfast-add-ons"]
item("kids-burger")["modifierGroups"] = ["kids-side", "kids-drink"]
# Burgers ask how they're cooked and for a side (the veggie patty just the side),
# on staff screens and the self-order kiosk alike.
# Toppings: what comes on it, to leave off (No Onion), go easy on (Lite) or
# double (Extra), and add-ons at a price.
for bid in ("classic-burger", "cheeseburger", "bacon-burger", "mushroom-swiss", "burger-of-the-day"):
    item(bid)["modifierGroups"] = ["temperature", "side", "toppings"]
item("veggie-burger")["modifierGroups"] = ["side", "toppings"]
for did in ("soda", "lemonade", "juice"):
    item(did)["modifierGroups"] = ["drink-size"]
item("coffee")["modifierGroups"] = ["coffee-extras"]
item("tea")["modifierGroups"] = ["hot-or-iced"]
item("draft-beer")["modifierGroups"] = ["draft"]
item("house-wine")["modifierGroups"] = ["wine", "wine-pour"]
# Staff pay full price for drinks from the bar.
for aid in ("draft-beer", "house-wine"):
    item(aid)["noStaffDiscount"] = True
# Dinner portions and the evening wine price.
for bid, price in (("classic-burger", 12.50), ("bacon-burger", 14.50), ("house-wine", 9.00)):
    item(bid)["periodPrices"] = {"dinner": price}
for n in TEMPS: menu(n, 0.00, "temperature", modifier=True)
for n, p in SIDES: menu(n, p, "sides", modifier=True)
# Tickets to an event: 40 seats, sold out by itself (Manager -> Menu sets the date).
menu("Wine Dinner Ticket", 65.00, "events")
item("wine-dinner-ticket").update({"ticketCapacity": 40, "kitchenHide": True,
                                   "description": "Five courses, five wines. Friday at 7."})
# Substitutes: Sub, then a salad, in place of the fries.
for sid, extra in (("house-salad", 3.00), ("caesar", 3.50)):
    item(sid)["substitute"] = True
    item(sid)["substitutePrice"] = extra
# Sold by weight: the price is per pound; ordering it asks for the weight.
menu("Smoked Brisket", 22.00, "plates")
item("smoked-brisket")["byWeight"] = True
item("smoked-brisket")["weightUnit"] = "lb"
# A combo: the burger, a side and a drink for one price (the choices are
# the menu's own items, so they use up their stock).
menu("Burger Combo", 16.95, "combos")
item("burger-combo")["modifierGroups"] = ["temperature", "side", "combo-drink"]
item("burger-combo")["description"] = "Classic burger, a side and a drink"

# Inventory: what's on the shelf (Manager -> Inventory), and what each item
# uses up. Items sell out by themselves when an ingredient runs short.
INGREDIENTS = [
    # id, name, unit, on hand, low at, cost per unit
    ("bun", "Burger Buns", "each", 48, 12, 0.35),
    ("patty", "Beef Patties", "each", 40, 10, 1.60),
    ("veggie-patty", "Veggie Patties", "each", 12, 4, 1.40),
    ("cheese", "Cheese Slices", "slice", 80, 20, 0.18),
    ("swiss", "Swiss Slices", "slice", 30, 8, 0.22),
    ("bacon", "Bacon", "strip", 60, 15, 0.30),
    ("mushrooms", "Mushrooms", "oz", 64, 16, 0.25),
    ("lettuce", "Lettuce", "oz", 160, 40, 0.12),
    ("romaine", "Romaine", "oz", 120, 30, 0.14),
    ("chicken", "Chicken Breast", "each", 24, 6, 2.10),
    ("eggs", "Eggs", "each", 120, 30, 0.25),
    ("bread", "Bread", "slice", 60, 16, 0.12),
    ("batter", "Pancake Batter", "oz", 240, 60, 0.06),
    ("coffee-beans", "Coffee", "oz", 80, 20, 0.40),
    ("potatoes", "Fries (potatoes)", "oz", 400, 80, 0.05),
    ("onions", "Onion Rings", "each", 120, 30, 0.08),
]
# Who delivers what (Manager -> Vendors; Inventory -> Receive a Delivery).
VENDORS = [
    {"id": "valley-foods", "name": "Valley Foods", "phone": "555-0140", "account": "VT-2210",
     "note": "Meat, cheese, eggs, dry goods. Delivers Tue and Fri; order by noon the day before."},
    {"id": "green-farms", "name": "Green Farms Produce", "phone": "555-0172", "account": "GF-881",
     "note": "Produce, daily except Sunday."},
    {"id": "city-bakery", "name": "City Bakery", "phone": "555-0119", "account": "",
     "note": "Buns and bread every morning before 8."},
]
VENDOR_OF = {"bun": "city-bakery", "bread": "city-bakery",
             "lettuce": "green-farms", "romaine": "green-farms", "mushrooms": "green-farms", "potatoes": "green-farms",
             "onions": "green-farms"}
write("pos/ingredients.json", [{"id": i, "name": n, "unit": u, "onHand": h, "lowAt": lo, "cost": c,
                                "vendor": VENDOR_OF.get(i, "valley-foods")}
                               for i, n, u, h, lo, c in INGREDIENTS], versioned=False)
RECIPES = {
    "classic-burger": [("bun", 1), ("patty", 1), ("lettuce", 0.5)],
    "cheeseburger": [("bun", 1), ("patty", 1), ("cheese", 2), ("lettuce", 0.5)],
    "bacon-burger": [("bun", 1), ("patty", 1), ("cheese", 1), ("bacon", 2)],
    "mushroom-swiss": [("bun", 1), ("patty", 1), ("swiss", 2), ("mushrooms", 2)],
    "veggie-burger": [("bun", 1), ("veggie-patty", 1), ("lettuce", 0.5)],
    "kids-burger": [("bun", 1), ("patty", 1)],
    "burger-of-the-day": [("bun", 1), ("patty", 1), ("bacon", 1), ("cheese", 1)],
    "house-salad": [("lettuce", 4)],
    "caesar": [("romaine", 5)],
    "cobb": [("lettuce", 4), ("chicken", 1), ("bacon", 2), ("eggs", 1)],
    "greek": [("lettuce", 4)],
    "two-eggs": [("eggs", 2), ("bread", 2)],
    "omelette": [("eggs", 3), ("bread", 2)],
    "pancakes": [("batter", 8)],
    "french-toast": [("bread", 3), ("eggs", 1)],
    "coffee": [("coffee-beans", 0.5)],
    "fries": [("potatoes", 6)],
    "sweet-potato-fries": [("potatoes", 6)],
    "onion-rings": [("onions", 6)],
}
# How some things look in the kitchen (Manager -> Menu -> Kitchen name...).
for iid, short in (("rare", "R"), ("medium-rare", "MR"), ("medium", "M"), ("medium-well", "MW"),
                   ("well-done", "WD"), ("sweet-potato-fries", "SWT FRY"), ("bacon-burger", "BCN BGR"),
                   ("mushroom-swiss", "MUSH SWS"), ("burger-of-the-day", "BOTD")):
    item(iid)["kitchenName"] = short
for iid in ("classic-burger", "cheeseburger", "bacon-burger", "mushroom-swiss", "veggie-burger", "burger-of-the-day"):
    item(iid)["kitchenColor"] = "orange"
item("kids-burger")["kitchenColor"] = "blue"
item("kids-burger")["kitchenName"] = "KIDS BGR"
for iid in ("water", "no-side"):
    item(iid)["kitchenHide"] = True
# Kitchen stations, each with its own screen (the Station button picks one).
for iid in ("classic-burger", "cheeseburger", "bacon-burger", "mushroom-swiss", "veggie-burger", "kids-burger",
            "burger-of-the-day", "burger-combo", "two-eggs", "pancakes", "french-toast", "omelette"):
    item(iid)["station"] = "grill"
for iid in ("fries", "onion-rings", "sweet-potato-fries"):
    item(iid)["station"] = "fryer"
for iid in ("house-salad", "caesar", "cobb", "greek", "side-salad"):
    item(iid)["station"] = "cold"
RECIPES["burger-combo"] = RECIPES["classic-burger"]   # its side and drink count on their own
item("burger-combo")["kitchenName"] = "COMBO BGR"
item("burger-combo")["kitchenColor"] = "orange"
for iid, recipe in RECIPES.items():
    item(iid)["recipe"] = [{"ingredient": g, "qty": q} for g, q in recipe]
write("pos/menu.json", MENU, versioned=False)

write("pos/employees.json", [
    # Pay an hour before tips; Jo also works the floor as a server some nights.
    {"id": "manager", "name": "Morgan (Manager)", "role": "manager", "pin": "1234", "sample": True, "payRate": 24.00},
    {"id": "sam", "name": "Sam", "role": "server", "pin": "1111", "sample": True, "payRate": 7.25},
    {"id": "riley", "name": "Riley", "role": "busser", "pin": "3333", "sample": True, "payRate": 11.00},
    {"id": "jo", "name": "Jo", "role": "bartender", "pin": "4444", "sample": True, "payRate": 9.00,
     "otherJobs": [{"role": "server", "rate": 7.25}]},
    {"id": "casey", "name": "Casey", "role": "cashier", "pin": "2222", "sample": True, "payRate": 15.50},
    {"id": "rosa", "name": "Rosa", "role": "server", "pin": "5555", "language": "es", "sample": True, "payRate": 7.25},   # screens in Spanish
], versioned=False)

# Groups whose options are menu items (combos): each option is that item.
def link_items(groups):
    by_name = {m["name"].lower(): m["id"] for m in MENU}
    for grp in groups:
        if grp.get("menuItems"):
            for o in grp["options"]:
                o["item"] = by_name[o["name"].lower()]
    return groups

write("pos/settings.json", {
    "vendors": VENDORS,
    "storeName": "ViewTouch Café",
    "currencySymbol": "$",
    # Ticked off on the Checklists page; End of Day shows what's left of closing.
    "openingChecklist": ["Count the starting cash", "Turn on the grill and fryers", "Check the walk-in temperature",
                         "Stock the bar and the line", "Unlock the front door"],
    "closingChecklist": ["Clean the grill and fryers", "Wipe down the tables", "Take out the trash",
                         "Count the drawers", "Lock the back door"],
    "autoLogoutMinutes": 3,
    "tableLongMinutes": 90,
    # Points for what regulars spend, and what the points buy.
    "loyalty": {"enabled": True, "pointsPerDollar": 1,
                "rewards": [{"points": 50, "value": 5.00}, {"points": 120, "value": 15.00},
                            {"points": 250, "value": 35.00}]},
    # Applied by themselves while they run (Manager -> Promotions).
    "promotions": [
        {"id": "happy-hour", "name": "Happy Hour", "active": True, "items": ["draft-beer", "house-wine"],
         "families": [], "percent": 50, "buy": 0, "get": 0, "start": 15 * 60, "end": 18 * 60, "days": 0b0111110},
        {"id": "burger-tuesday", "name": "Burger Tuesday", "active": True, "families": ["burgers"], "items": [],
         "percent": 50, "buy": 1, "get": 1, "start": 0, "end": 0, "days": 0b0000100},
    ],
    # What the customer display shows between guests.
    "display": {"logo": "", "accent": "#2f6fd6",
                "slides": ["Happy Hour, weekdays 3 - 6 PM: half-price draft beer and wine",
                           "Burger Tuesday: the second burger is half off",
                           "Join our rewards: 1 point for every dollar, $5 off at 50 points",
                           "Gift cards make great gifts - ask your server"]},
    "tax": {"food": 8.25, "alcohol": 10.0, "merchandise": 8.25, "room": 0, "taxTakeoutFood": True},
    "tenders": [
        {"id": "cash", "name": "Cash", "kind": "cash"},
        {"id": "credit", "name": "Credit Card", "kind": "card"},
        {"id": "gift", "name": "Gift Card", "kind": "giftcard"},
        {"id": "house", "name": "House Account", "kind": "house"},
        {"id": "discount", "name": "10% Discount", "kind": "discount", "percent": 10},
        {"id": "comp", "name": "Comp", "kind": "discount", "percent": 100},
        {"id": "staff-meal", "name": "Staff Meal", "kind": "discount", "percent": 50, "staffMeal": True},
    ],
    # "file" printers write text under <app data>/printouts so tickets can be
    # seen without hardware. Switch them to network/CUPS in Manager -> Printers.
    "printers": [
        {"id": "receipt", "name": "Receipt", "type": "file", "path": "receipt.txt", "width": 42,
         "cutter": True, "drawerKick": True},
        {"id": "kitchen", "name": "Kitchen", "type": "file", "path": "kitchen.txt", "width": 42,
         "cutter": True, "drawerKick": False},
        {"id": "bar", "name": "Bar", "type": "file", "path": "bar.txt", "width": 42,
         "cutter": True, "drawerKick": False},
    ],
    "receiptHeader": "123 Main Street\nOpen daily 7am - 10pm",
    "gratuity": {"percent": 18, "minGuests": 6},
    "terminals": [],
    # Servers carry their own bank; terminals need no drawer of their own.
    "cashMode": "serverBank",
    # A small chart of accounts for Reports -> Accounting (and its CSV).
    "accounts": {"sales": "4000 Food sales", "sales:drinks": "4100 Beverage sales", "sales:events": "4300 Event sales",
                 "tax": "2200 Sales tax payable", "gratuity": "2310 Gratuity payable", "tips": "2300 Tips payable",
                 "discounts": "4900 Discounts and comps", "staffMeals": "6150 Staff meals",
                 "rounding": "4950 Cash rounding", "giftCardsSold": "2400 Gift card liability",
                 "tender:cash": "1000 Cash on hand", "tender:credit": "1100 Card clearing",
                 "tender:gift": "2400 Gift card liability", "tender:house": "1200 House accounts receivable"},
    "stations": [{"id": "grill", "name": "Grill"}, {"id": "fryer", "name": "Fryer"},
                 {"id": "cold", "name": "Cold Line"}],
    # The menu's categories, in order: their buttons on the menu screens, their
    # meal periods, and what new items in them start with (Menu Builder).
    "menuCategories": [
        {"id": "breakfast", "name": "Breakfast Plates", "color": "#a86a12", "periods": ["breakfast"],
         "printer": "kitchen"},
        {"id": "burgers", "name": "Burgers", "color": "#a86a12", "periods": ["lunch", "dinner"],
         "printer": "kitchen", "station": "grill"},
        {"id": "salads", "name": "Salads", "color": "#1f8a4c", "periods": ["lunch", "dinner"],
         "printer": "kitchen", "station": "cold"},
        {"id": "plates", "name": "Plates", "color": "#6b46c1", "periods": ["lunch", "dinner"], "printer": "kitchen"},
        {"id": "combos", "name": "Combos", "color": "#1f8a4c", "periods": ["lunch", "dinner"], "printer": "kitchen"},
        {"id": "drinks", "name": "Drinks", "color": "#1f6f73", "periods": [], "printer": "bar"},
        {"id": "events", "name": "Events", "color": "#6b46c1", "periods": [], "printer": "kitchen"},
    ],
    "modifierGroups": link_items([
        {"id": "dressing", "name": "Dressing", "min": 1, "max": 1, "askHow": True,   # on the side
         "options": [{"name": n, "price": 0, "kitchenName": k} for n, k in
                     (("Ranch", "RNCH"), ("Blue Cheese", "BLU CHZ"), ("Balsamic", "BALS"), ("Caesar", "CAES"),
                      ("Oil & Vinegar", "O&V"))]},
        {"id": "salad-protein", "name": "Add a Protein", "min": 0, "max": 1, "askHow": True,
         "options": [{"name": "Grilled Chicken", "price": 4.00}, {"name": "Shrimp", "price": 5.00},
                     {"name": "Salmon", "price": 6.00}]},
        {"id": "eggs", "name": "Eggs", "min": 1, "max": 1,
         "options": [{"name": n, "price": 0} for n in ("Scrambled", "Over Easy", "Over Medium", "Sunny Side Up", "Poached")]},
        {"id": "toast", "name": "Toast", "min": 1, "max": 1,
         "options": [{"name": n, "price": 0} for n in ("White", "Wheat", "Sourdough", "Rye", "English Muffin")]},
        {"id": "syrup", "name": "Syrup", "min": 1, "max": 1, "askHow": True,
         "options": [{"name": "Maple", "price": 0}, {"name": "Blueberry", "price": 0},
                     {"name": "Strawberry", "price": 0}, {"name": "Real Maple", "price": 1.50}]},
        {"id": "breakfast-add-ons", "name": "Add-ons", "min": 0, "max": 0, "askHow": True,
         "options": [{"name": "Bacon", "price": 3.00}, {"name": "Sausage", "price": 3.00},
                     {"name": "Fruit Cup", "price": 2.50}, {"name": "Hash Browns", "price": 2.75},
                     {"name": "Whipped Cream", "price": 0.75}]},
        {"id": "temperature", "name": "Temperature", "min": 1, "max": 1,
         "options": [{"name": n, "price": 0, "kitchenName": k} for n, k in
                     zip(TEMPS, ("RARE", "MR", "MED", "MW", "WELL"))]},
        {"id": "side", "name": "Side", "min": 1, "max": 1, "menuItems": True,   # "No Side": nothing for the kitchen
         "options": [dict({"name": n, "price": p}, **({"kitchenHide": True} if n == "No Side" else {}))
                     for n, p in SIDES]},
        # Lettuce, tomato, onion, pickles and mayo come on it: No, Lite, Extra, on the Side.
        {"id": "toppings", "name": "Toppings", "min": 0, "max": 0, "askHow": True,
         "options": [dict({"name": n, "price": p, "kitchenName": k}, **({"included": True} if p == 0 else {}))
                     for n, p, k in
                     (("Lettuce", 0, "LETT"), ("Tomato", 0, "TOM"), ("Onion", 0, "ONION"), ("Pickles", 0, "PICK"),
                      ("Mayo", 0, "MAYO"), ("Cheese", 1.00, "CHZ"), ("Bacon", 2.00, "BCN"), ("Avocado", 1.50, "AVO"),
                      ("Jalapeños", 0.75, "JAL"))]},
        {"id": "combo-drink", "name": "Drink", "min": 1, "max": 1, "menuItems": True,
         "options": [{"name": "Soda", "price": 0}, {"name": "Tea", "price": 0}, {"name": "Lemonade", "price": 0.30},
                     {"name": "Juice", "price": 0.55}, {"name": "Draft Beer", "price": 3.00}]},
        {"id": "kids-side", "name": "Kids Side", "min": 1, "max": 1,
         "options": [{"name": n, "price": 0} for n in ("Fries", "Apple Slices", "Fruit Cup", "Carrot Sticks")]},
        {"id": "kids-drink", "name": "Kids Drink", "min": 0, "max": 1,
         "options": [{"name": "Milk", "price": 0}, {"name": "Apple Juice", "price": 0},
                     {"name": "Chocolate Milk", "price": 0.50}]},
        {"id": "drink-size", "name": "Size", "min": 1, "max": 1,
         "options": [{"name": "Small", "price": 0}, {"name": "Medium", "price": 0.50},
                     {"name": "Large", "price": 1.00}]},
        {"id": "coffee-extras", "name": "Coffee", "min": 0, "max": 0, "askHow": True,
         "options": [{"name": "Cream", "price": 0}, {"name": "Oat Milk", "price": 0.75},
                     {"name": "Sugar", "price": 0}, {"name": "Sweetener", "price": 0},
                     {"name": "Extra Shot", "price": 1.25}, {"name": "Decaf", "price": 0}]},
        {"id": "hot-or-iced", "name": "Hot or Iced", "min": 1, "max": 1,
         "options": [{"name": "Hot", "price": 0}, {"name": "Iced", "price": 0}]},
        {"id": "draft", "name": "Draft", "min": 1, "max": 1,
         "options": [{"name": "IPA", "price": 0}, {"name": "Lager", "price": 0}, {"name": "Stout", "price": 0},
                     {"name": "Seasonal", "price": 1.00}]},
        {"id": "wine", "name": "Wine", "min": 1, "max": 1,
         "options": [{"name": n, "price": 0} for n in ("Red", "White", "Rosé")]},
        {"id": "wine-pour", "name": "Pour", "min": 1, "max": 1,
         "options": [{"name": "Glass", "price": 0}, {"name": "Bottle", "price": 22.00}]},
        {"id": "omelette-fillings", "name": "Fillings", "min": 0, "max": 3, "askHow": True,
         "options": [{"name": "Cheese", "price": 0}, {"name": "Ham", "price": 1.00}, {"name": "Mushrooms", "price": 0},
                     {"name": "Peppers", "price": 0}, {"name": "Onions", "price": 0}, {"name": "Spinach", "price": 0},
                     {"name": "Bacon", "price": 1.50}]},
    ]),
    "mealPeriods": [{"id": "breakfast", "name": "Breakfast", "start": "04:00"},
                    {"id": "lunch", "name": "Lunch", "start": "11:00"},
                    {"id": "dinner", "name": "Dinner", "start": "16:00"}],
    "receiptFooter": "Thank you for visiting!\nPowered by ViewTouch",
})

# ---------------------------------------------------------------- theme
WIDGETS = ["orderList", "loginPad", "guestCount", "numPad", "paymentPanel",
           "logoutPanel", "clock", "checkList", "keyboard", "statusBar",
           "adminPanel", "reportView", "drawerPanel", "endOfDay", "splitCheck", "customerInfo",
           "customerLookup", "giftCard", "waitlist", "schedule", "factoryReset", "messageComposer", "network", "receiveDelivery", "stockCount", "checkSearch", "orderLater", "menuGrid", "menuCategories", "menuBuilder", "timeClock", "dashboard", "checklist", "hostStand", "deliveryBoard"]
widget_style = {"normal": {"fill": "#232933", "frame": "flat", "shadow": 0, "radius": 12,
                           "textColor": "#e6e9ef", "fontSize": 28, "bold": False}}
write("theme.json", {
    "name": "ViewTouch Dark",
    "background": {"fill": "#171a1f"},
    "style": {
        "normal": {"fill": "#2d3440", "textColor": "#f2f4f7", "font": "DejaVu Sans",
                   "fontSize": 30, "bold": True, "frame": "raised", "frameWidth": 3,
                   "radius": 14, "shadow": 5, "textStyle": "none"},
        "selected": {"fill": "#2f6fd6", "frame": "inset", "shadow": 2},
        "disabled": {"opacity": 0.35},
    },
    "kinds": {
        "label": {"normal": {"fill": "transparent", "frame": "none", "shadow": 0,
                             "fontSize": 44, "textColor": "#e6e9ef"},
                  "selected": {"fill": "transparent", "frame": "none"}},
        "comment": {"normal": {"fill": "#fff3b0", "textColor": "#3d3200", "frame": "flat",
                               "shadow": 0, "fontSize": 20, "bold": False}},
        "image": {"normal": {"fill": "#f5efe0", "textColor": "#2b2b2b"}},
        **{w: widget_style for w in WIDGETS},
    },
})

GREEN, RED, BLUE, AMBER, TEAL, PURPLE = "#1f8a4c", "#b83232", "#2b62b0", "#a86a12", "#1f6f73", "#6b46c1"

# ---------------------------------------------------------------- order template
flow = [
    ("flow-tables", "Tables", [command("releaseCheck"), jump(role="tables", mode="replace")], {}),
    ("flow-no", "No", [{"type": "qualifier", "qualifier": "no"}], {}),
    ("flow-extra", "Extra", [{"type": "qualifier", "qualifier": "extra"}], {}),
    ("flow-lite", "Lite", [{"type": "qualifier", "qualifier": "lite"}], {}),
    ("flow-side", "Side", [{"type": "qualifier", "qualifier": "side"}], {}),
    ("flow-sub", "Sub", [{"type": "qualifier", "qualifier": "sub"}], {}),
    ("flow-void", "Void", [command("voidItem")], {"behavior": "double", "style": fill(RED)}),
    ("flow-send", "Send", [command("sendOrder")], {"style": fill(GREEN), "hotkey": "s"}),
    ("flow-pay", "Pay", [jump(role="settle")], {"style": fill(BLUE), "hotkey": "p"}),
]
tmpl = [zone("order-list", 16, 16, 560, 948, kind="orderList")]
step = (1904 - 16 + 8) // len(flow)
for i, (zid, text, acts, kw) in enumerate(flow):
    tmpl.append(zone(zid, 16 + i * step, 980, step - 8, 84, text, actions=acts, **kw))
for i, (zid, text, target) in enumerate([("tab-breakfast", "Breakfast", "index-breakfast"),
                                          ("tab-lunch", "Lunch", "index-lunch"),
                                          ("tab-dinner", "Dinner", "index-dinner")]):
    tmpl.append(zone(zid, 592 + i * 218, 16, 210, 72, text, actions=[jump(page=target, mode="replace")],
                     style={"normal": {"fontSize": 26}}))
# Find an item by typing part of its name.
tmpl.append(zone("tab-find", 1246, 16, 120, 72, "Find", actions=[command("clearText"), jump(page="find-item")],
                 style={"normal": {"fontSize": 26}}))
tmpl.append(zone("tab-categories", 1374, 16, 170, 72, "‹ Menu", actions=[jump(mode="index")],
                 style={"normal": {"fontSize": 26}}))
tmpl.append(zone("tab-note", 1560, 16, 160, 72, "Note", actions=[jump(page="note")],
                 style={"normal": {"fontSize": 26}}))
# Transfer, move, merge, reopen, and the check's history.
tmpl.append(zone("tab-check", 1736, 16, 168, 72, "Check…", actions=[jump(page="check-options")],
                 style={"normal": {"fontSize": 26}}))
page("order-template", "Order Template", "template", tmpl)

# ---------------------------------------------------------------- index pages
# The meal's categories fill themselves (Menu Builder): a new category or item
# shows up with no page editing. Everything and Popular beneath.
def index_page(id, name, period):
    zs = [label("title", 592, 104, 1312, 72, name),
          zone("categories", 592, 192, 1312, 600, kind="menuCategories", props={"period": period, "columns": 3}),
          zone("cat-menu-all", 592, 812, 640, 152, "Everything", actions=[jump(page="menu-all", mode="replace")],
               style=fill(PURPLE)),
          zone("cat-menu-popular", 1264, 812, 640, 152, "Popular", actions=[jump(page="menu-popular", mode="replace")],
               style=fill(RED))]
    page(id, name, "index", zs, templateId="order-template", mealPeriod=period)

index_page("index-breakfast", "Breakfast", "breakfast")
index_page("index-lunch", "Lunch", "lunch")
index_page("index-dinner", "Dinner", "dinner")

# Today's best sellers, most first: they fill in as the day goes.
page("menu-popular", "Popular", "items", [
    label("title", 592, 104, 1312, 72, "Popular today"),
    zone("popular", 592, 192, 1312, 772, kind="menuGrid", props={"popular": True, "columns": 4}),
], templateId="order-template")

# Find: type part of a name, touch the item.
page("find-item", "Find an Item", "items", [
    zone("results", 592, 104, 1312, 400, kind="menuGrid", props={"search": True, "columns": 4}),
    zone("keyboard", 592, 516, 1312, 448, kind="keyboard", props={"placeholder": "Part of a name…"}),
], templateId="order-template")

# The whole menu, laid out by itself: a category's items (touched on the meal's
# page), with a button for each category across the top. New items appear
# with no editing.
page("menu-all", "Menu", "items", [
    # No columns set: as many as fit the category's items, as big as they can be.
    zone("menu", 592, 104, 1312, 860, kind="menuGrid"),
], templateId="order-template", role="menu")

# ---------------------------------------------------------------- item pages
def add(name, seq=None):
    a = {"type": "addItem", "item": slug(name)}
    if seq: a["modifierSequence"] = seq
    return a

# Hand-built item pages: the menu screens fill themselves now; these stay as
# the tests' fixtures (pages of buttons placed by hand, for the page editor).
FIXTURES = os.path.join(OUT, "..", "tests", "fixtures", "pages")
os.makedirs(FIXTURES, exist_ok=True)

def fixture_page(id, name, kind, zones, **kw):
    p = {"id": id, "name": name, "kind": kind, "canvas": {"w": 1920, "h": 1080}, "grid": 8}
    p.update(kw)
    p["zones"] = zones
    with open(os.path.join(FIXTURES, f"{id}.json"), "w") as f:
        json.dump({"schemaVersion": SCHEMA, **p}, f, indent=2)
        f.write("\n")

def item_page(id, name, items, color, shape="rounded", cols=4, cell=(316, 180), extra=()):
    zs = [label("title", 592, 104, 1312, 72, name)]
    w, h = cell
    gap_x = (1312 - cols * w) // max(cols - 1, 1)
    for i, (text, seq) in enumerate(items):
        col, row = i % cols, i // cols
        zs.append(zone(f"item-{i + 1}", 592 + col * (w + gap_x), 192 + row * (h + 16), w, h, text,
                       actions=[add(text, seq)], shape=shape, style=fill(color)))
    zs.extend(extra)
    fixture_page(id, name, "items", zs, templateId="order-template")

item_page("items-burgers", "Burgers", [(n, None) for n, _ in BURGERS[:6]], AMBER,
          extra=[zone("combo", 924, 596, 300, 180, "Burger Combo", actions=[add("Burger Combo")],
                      shape="rounded", style=fill(GREEN)),
                 zone("brisket", 924, 792, 300, 136, "Smoked Brisket\n$22.00 / lb", actions=[add("Smoked Brisket")],
                      shape="rounded", style=fill(PURPLE)),
                 zone("burger-photo", 592, 596, 316, 260, "Burger of the Day", kind="image",
                      imagePath="qrc:/images/burger.png", actions=[add("Burger of the Day")]),
                 zone("note", 1240, 596, 664, 120,
                      "Burgers ask for Temperature and Side (Manager -> Modifier Groups).", kind="comment")])
item_page("items-salads", "Salads", [(n, None) for n, _ in SALADS], GREEN, shape="hexagon")
item_page("items-drinks", "Drinks", [(n, None) for n, _, _ in DRINKS], TEAL, shape="circle", cols=5,
          cell=(200, 200),
          extra=[zone("wine-dinner", 592, 640, 632, 150, "Wine Dinner Ticket\nFriday 7 PM · $65",
                      actions=[add("Wine Dinner Ticket")], shape="rounded", style=fill(PURPLE))])
item_page("items-breakfast", "Breakfast Plates", [(n, None) for n, _ in BREAKFAST], AMBER, shape="octagon",
          cell=(316, 220))

# Free-text note for the kitchen
page("note", "Note", "custom", [
    label("title", 592, 104, 1312, 72, "Note for the kitchen"),
    zone("keyboard", 592, 192, 1312, 560, kind="keyboard"),
    zone("cancel", 592, 800, 420, 120, "Cancel", actions=[jump(mode="back")]),
    zone("add-note", 1484, 800, 420, 120, "Add Note", actions=[command("addComment"), jump(mode="back")],
         style=fill(GREEN)),
], templateId="order-template")

# ---------------------------------------------------------------- system pages
page("login", "Login", "login", [
    label("title", 460, 40, 1000, 110, "{store.name}", style={"normal": {"fontSize": 72}}),
    # The store's logo (Store Settings -> Store logo); nothing shows until one is set.
    zone("logo", 60, 260, 560, 420, "", kind="image", imagePath="logo:", behavior="none",
         style={"normal": {"fill": "transparent", "frame": "none", "shadow": 0}}),
    zone("clock", 660, 160, 600, 80, kind="clock"),
    zone("login-pad", 660, 260, 600, 600, kind="loginPad"),
    zone("clock-in", 300, 880, 344, 120, "Clock In", actions=[command("clockIn")]),
    zone("start", 660, 880, 600, 120, "Log In", actions=[command("login")], style=fill(GREEN)),
    zone("clock-out", 1276, 880, 344, 120, "Clock Out", actions=[command("clockOut")]),
    zone("hint", 1300, 260, 560, 200,
         "Demo PINs: 1234 manager, 1111 server, 2222 cashier, 5555 server in Spanish. Remove this note in the editor.",
         kind="comment"),
    # Clock in / out and see your schedule, without logging in.
    zone("time-clock", 1300, 480, 560, 110, "Time Clock & My Schedule", actions=[jump(page="time-clock")],
         style=fill(TEAL)),
], role="login")

# The Time Clock: clock in / out, breaks and the schedule, by PIN alone. A
# terminal set to "Time Clock" (Manager -> Terminals) shows only this.
page("time-clock", "Time Clock", "custom", [
    label("title", 16, 16, 1300, 100, "{store.name} · Time Clock", style={"normal": {"fontSize": 52}}),
    zone("clock", 1332, 26, 572, 80, kind="clock"),
    zone("time-clock", 16, 132, 1888, 812, kind="timeClock"),
    zone("register", 16, 960, 520, 104, "Log In to the Register…", actions=[command("timeClockDone"), jump(role="login")]),
], role="timeClock", permission="public")

TABLES = [
    {"label": "T1", "x": 80, "y": 80, "w": 200, "h": 200, "shape": "circle", "seats": 2},
    {"label": "T2", "x": 360, "y": 80, "w": 200, "h": 200, "shape": "circle", "seats": 2},
    {"label": "T3", "x": 640, "y": 80, "w": 320, "h": 200, "shape": "rect", "seats": 4},
    {"label": "T4", "x": 1040, "y": 80, "w": 320, "h": 200, "shape": "rect", "seats": 4},
    {"label": "T5", "x": 80, "y": 400, "w": 480, "h": 220, "shape": "rect", "seats": 6},
    {"label": "T6", "x": 640, "y": 400, "w": 240, "h": 240, "shape": "octagon", "seats": 4},
    {"label": "T7", "x": 960, "y": 400, "w": 240, "h": 240, "shape": "octagon", "seats": 4},
    {"label": "Bar 1", "x": 80, "y": 760, "w": 240, "h": 160, "shape": "rounded", "seats": 1},
    {"label": "Bar 2", "x": 360, "y": 760, "w": 240, "h": 160, "shape": "rounded", "seats": 1},
    {"label": "Bar 3", "x": 640, "y": 760, "w": 240, "h": 160, "shape": "rounded", "seats": 1},
]
# Each table is its own zone: move, resize, reshape or restyle it in the
# editor, or add more with + Panel -> table.
def table_zone(t):
    tid = "table-" + t["label"].lower().replace(" ", "-")
    return zone(tid, 16 + t["x"], 16 + t["y"], t["w"], t["h"], t["label"], kind="table",
                shape=t["shape"], props={"seats": t["seats"]})

page("tables", "Tables", "tables", [
    *[table_zone(t) for t in TABLES],
    zone("quick", 1472, 16, 432, 150, "Quick Order", actions=[command("startQuick"), jump(mode="index")],
         style=fill(GREEN)),
    zone("takeout", 1472, 182, 208, 150, "Takeout", actions=[command("startTakeout"), jump(mode="index")]),
    zone("delivery", 1696, 182, 208, 150, "Delivery", actions=[command("startDelivery"), jump(mode="index")]),
    zone("checks", 1472, 348, 208, 150, "Open Checks", actions=[jump(role="checkList")]),
    zone("deliveries", 1696, 348, 208, 150, "Deliveries", actions=[jump(page="deliveries")], style=fill(BLUE)),
    zone("host", 1056, 914, 400, 150, "Host Stand", actions=[jump(page="seating")], style=fill(TEAL)),
    zone("tabs", 1056, 748, 400, 150, "Bar Tabs", actions=[jump(page="tabs")], style=fill(AMBER)),
    zone("status", 1472, 514, 432, 140, kind="logoutPanel"),
    # The next person: their PIN, while this one stays clocked in (their check opens again when they're back).
    zone("switch", 1472, 664, 432, 74, "Switch User", actions=[command("logout")], style=fill(BLUE)),
    zone("manager", 1472, 748, 432, 150, "Manager", actions=[jump(role="manager")]),
    zone("logout", 1472, 914, 432, 150, "Log Out", actions=[jump(role="logout")], style=fill(RED)),
], role="tables", background={"texture": "woodfloor", "fill": "#3b2a1a"})

# Bar tabs: open under the guest's name, kept open all night.
page("tabs", "Bar Tabs", "custom", [
    label("title", 16, 16, 1888, 80, "Bar tabs"),
    zone("list", 16, 112, 1888, 816, kind="checkList", props={"mode": "tabs"}),
    zone("back", 16, 944, 432, 120, "‹ Tables", actions=[jump(role="tables")]),
    zone("new", 1472, 944, 432, 120, "New Tab…", actions=[command("clearText"), jump(page="new-tab")],
         style=fill(GREEN)),
], permission="order")
page("new-tab", "New Tab", "custom", [
    label("title", 16, 16, 1888, 80, "Name on the tab"),
    zone("keyboard", 16, 112, 1888, 760, kind="keyboard", props={"placeholder": "Mike, red jacket…"}),
    zone("cancel", 16, 944, 432, 120, "Cancel", actions=[command("clearText"), jump(mode="back")]),
    zone("open", 1472, 944, 432, 120, "Open Tab", actions=[command("openTab"), jump(mode="index")], style=fill(GREEN)),
], permission="order")

page("customer", "Customer", "custom", [
    label("title", 16, 16, 1888, 80, "Who is the order for?"),
    zone("customer", 16, 112, 1888, 800, kind="customerInfo"),
    zone("cancel", 16, 944, 432, 120, "Cancel", actions=[command("releaseCheck"), jump(mode="back")]),
    zone("later", 1024, 944, 432, 120, "Ready Later…", actions=[jump(page="order-later")], style=fill(TEAL)),
    zone("menu", 1472, 944, 432, 120, "Continue to Menu ›", actions=[jump(mode="index")], style=fill(GREEN)),
])

# Orders for later: the day and time it should be ready.
page("order-later", "Order for Later", "custom", [
    label("title", 16, 16, 1888, 80, "When should it be ready?"),
    zone("when", 16, 112, 1888, 816, kind="orderLater"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
    zone("done", 1472, 944, 432, 120, "Done", actions=[jump(mode="back")], style=fill(GREEN)),
], permission="order")

# How much something sold by weight weighs (Smoked Brisket).
page("weigh", "Weigh", "modifier", [
    label("title", 592, 104, 1312, 72, "How much does it weigh?"),
    zone("weight", 592, 192, 640, 700, kind="numPad", props={"mode": "weight"}),
    zone("add", 1264, 192, 640, 200, "Add", actions=[command("addWeighed")], style=fill(GREEN)),
    zone("cancel", 1264, 408, 640, 140, "Cancel", actions=[command("cancelWeighing")], style=fill(RED)),
    label("hint", 1264, 564, 640, 200, "Type the weight from the scale: 125 is 1.25 lb.",
          style={"normal": {"fontSize": 30, "bold": False}}),
], templateId="order-template", role="weigh")

# Choices for the item just ordered (its modifier groups). Inside the order
# screen, like item pages, so phones frame it too.
page("modifiers", "Choose", "modifier", [
    zone("choices", 592, 104, 1312, 860, kind="modifierPicker"),
], templateId="order-template", role="modifiers")

# Sold out (86): from Check Options or the Manager page.
page("sold-out", "Sold Out (86)", "custom", [
    label("title", 16, 16, 1888, 80, "Touch an item to mark it sold out, or back on"),
    zone("list", 16, 112, 1888, 816, kind="soldOutList"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="order")

# --- the host stand ---
page("host", "Host Stand", "custom", [
    label("title", 16, 16, 1888, 80, "Waitlist & reservations"),
    zone("waitlist", 16, 112, 1888, 816, kind="waitlist"),
    zone("back", 16, 944, 432, 120, "‹ Tables", actions=[jump(role="tables")]),
    zone("seating", 744, 944, 432, 120, "Seating…", actions=[jump(page="seating")], style=fill(TEAL)),
    zone("customers", 1472, 944, 432, 120, "Customers…", actions=[jump(page="customers")]),
], permission="order")

# Deliveries: open ones by where they are; send them out with a driver.
page("deliveries", "Deliveries", "custom", [
    label("title", 16, 16, 1888, 80, "Deliveries"),
    zone("board", 16, 112, 1888, 816, kind="deliveryBoard"),
    zone("back", 16, 944, 432, 120, "‹ Tables", actions=[jump(role="tables")]),
    zone("new", 1472, 944, 432, 120, "New Delivery", actions=[command("startDelivery"), jump(mode="index")],
         style=fill(GREEN)),
], permission="order")

# The host stand: every table by state (available, seated, dirty, held),
# the waitlist and reservations; seat or hold parties at one or more tables.
page("seating", "Seating", "custom", [
    label("title", 16, 16, 1888, 80, "Seating"),
    zone("host-stand", 16, 112, 1888, 816, kind="hostStand"),
    zone("back", 16, 944, 432, 120, "‹ Tables", actions=[jump(role="tables")]),
    zone("waitlist", 1472, 944, 432, 120, "Waitlist & Reservations…", actions=[jump(page="host")], style=fill(TEAL)),
], permission="order")

# --- customers and gift cards ---
page("customers", "Customers", "custom", [
    label("title", 16, 16, 1888, 80, "Customers"),
    zone("customers", 16, 112, 1888, 816, kind="customerLookup"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
    zone("gift-card", 1472, 944, 432, 120, "Gift Cards…", actions=[jump(page="gift-card")]),
], permission="order")
page("gift-card", "Gift Card", "custom", [
    label("title", 16, 16, 1888, 80, "Gift cards"),
    zone("card", 16, 112, 1888, 816, kind="giftCard"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
    zone("pay", 1472, 944, 432, 120, "Pay ›", actions=[jump(role="settle")], style=fill(BLUE)),
], permission="order")

# --- managing a check (from the order screen's Check… tab) ---
page("check-options", "Check Options", "custom", [
    label("title", 16, 16, 440, 80, "This check"),
    zone("later", 472, 16, 444, 80, "Ready Later…", actions=[jump(page="order-later")], style=fill(TEAL)),
    zone("rush", 932, 16, 478, 80, "Rush", actions=[command("rush")], style=fill(RED)),
    zone("vip", 1426, 16, 478, 80, "VIP", actions=[command("vip")], style=fill(AMBER)),
    zone("history", 16, 112, 900, 952, kind="checkHistory"),
    zone("customer", 932, 112, 972, 104, "Customer…", actions=[jump(page="customers")]),
    zone("gift-card", 932, 230, 972, 104, "Sell / Check a Gift Card…", actions=[jump(page="gift-card")]),
    zone("transfer", 932, 348, 972, 104, "Transfer to Another Server…", actions=[jump(page="transfer")]),
    zone("move", 932, 466, 972, 104, "Move to Another Table…", actions=[jump(page="move-table")]),
    zone("merge", 932, 584, 972, 104, "Merge Another Check Into This One…", actions=[jump(page="merge")]),
    zone("reopen", 932, 702, 972, 104, "Reopen a Closed Check… (manager)", actions=[jump(page="closed-checks")]),
    zone("sold-out", 932, 820, 972, 104, "Sold Out (86)…", actions=[jump(page="sold-out")]),
    zone("back", 932, 944, 600, 120, "‹ Back to the Order", actions=[jump(mode="back")]),
    zone("allergies", 1548, 944, 356, 120, "Allergy…", actions=[command("allergies")], style=fill(RED)),
    # Someone else needs the screen: this check opens again at this person's next login.
    zone("switch", 1548, 944, 356, 120, "Switch User", actions=[command("logout")], style=fill(BLUE)),
], permission="order")
page("transfer", "Transfer Check", "custom", [
    label("title", 16, 16, 1888, 80, "Give this check to…"),
    zone("staff", 16, 112, 1888, 816, kind="staffPicker"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="order")
page("move-table", "Move Check", "custom", [
    label("title", 16, 16, 1888, 80, "Move this check to table…"),
    zone("tables", 16, 112, 1888, 816, kind="tableGrid", props={"action": "move", "columns": 6}),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="order")
page("merge", "Merge Checks", "custom", [
    label("title", 16, 16, 1888, 80, "Merge which check into this one?"),
    zone("list", 16, 112, 1888, 816, kind="checkList", props={"mode": "merge"}),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="order")
page("closed-checks", "Closed Checks", "custom", [
    label("title", 16, 16, 1888, 80, "Checks closed today: touch one to reopen it"),
    zone("list", 16, 112, 1888, 816, kind="checkList", props={"mode": "closed"}),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
    zone("find", 1472, 944, 432, 120, "Find an Older Check…", actions=[jump(page="find-check")]),
], permission="manager")

page("check-list", "Open Checks", "custom", [
    label("title", 16, 16, 1440, 80, "Open checks"),
    zone("list", 16, 112, 1440, 952, kind="checkList"),
    zone("back", 1472, 914, 432, 150, "‹ Back", actions=[jump(mode="back")]),
], role="checkList")

page("guest-count", "Guest Count", "guestCount", [
    label("title", 560, 32, 800, 80, "How many guests?"),
    zone("guests", 660, 128, 600, 160, kind="guestCount"),
    zone("pad", 660, 304, 600, 560, kind="numPad"),
    zone("cancel", 660, 888, 290, 120, "Cancel", actions=[command("releaseCheck"), jump(mode="back")]),
    zone("start", 970, 888, 290, 120, "Start Order", actions=[command("startCheck"), jump(mode="index")],
         style=fill(GREEN)),
], role="guestCount")

# Gift Card opens its page (the card number first); the rest pay right away.
tenders = [("Cash", "cash", GREEN), ("Credit Card", "credit", BLUE), ("Gift Card…", "gift", TEAL),
           ("House Account", "house", TEAL), ("10% Off", "discount", AMBER), ("Comp", "comp", PURPLE),
           ("Staff Meal", "staff-meal", PURPLE)]


def tender_action(tid):
    return [jump(page="gift-card")] if tid == "gift" else [{"type": "tender", "tender": tid}]
settle = [zone("payment", 16, 16, 900, 1048, kind="paymentPanel"),
          zone("pad", 932, 16, 520, 620, kind="numPad", props={"mode": "amount"})]
# The payment types share the column above Split / Ask Guest for Tip.
step = (624 - 16) // len(tenders)
for i, (text, tid, color) in enumerate(tenders):
    settle.append(zone(f"tender-{tid}", 1468, 16 + i * step, 436, step - 10, text,
                       actions=tender_action(tid), style=fill(color)))
settle += [
    zone("receipt", 932, 652, 520, 120, "Print Receipt", actions=[command("printReceipt")]),
    zone("close", 932, 788, 520, 120, "Close Check", actions=[command("closeCheck")], style=fill(GREEN)),
    zone("remove-payment", 932, 924, 520, 120, "Undo Payment", actions=[command("removePayment")]),
    zone("split", 1468, 640, 212, 100, "Split", actions=[jump(page="split")]),
    zone("drawer", 1692, 640, 212, 100, "Drawer…", actions=[jump(page="drawer")]),
    # On the customer display: the guest picks a tip, which goes on their card.
    zone("ask-tip", 1468, 752, 436, 90, "Ask Guest for Tip", actions=[command("askForTip")], style=fill(TEAL)),
    # A discount of what's typed on the keypad (a manager's, or their PIN).
    zone("off-amount", 1468, 852, 212, 82, "$ Off", actions=[command("amountOff")], style=fill(AMBER)),
    zone("off-percent", 1692, 852, 212, 82, "% Off", actions=[command("percentOff")], style=fill(AMBER)),
    zone("done", 1468, 944, 436, 120, "‹ Back to Order", actions=[jump(mode="back")]),
]
page("settle", "Settle", "settle", settle, role="settle", permission="check.settle")

page("split", "Split Check", "custom", [
    zone("split-check", 16, 16, 1888, 932, kind="splitCheck"),
    zone("back", 16, 964, 432, 100, "‹ Back", actions=[jump(mode="back")]),
], permission="order")

page("logout", "Log Out", "logout", [
    label("title", 560, 40, 800, 90, "End of shift"),
    zone("panel", 560, 150, 800, 400, kind="logoutPanel"),
    zone("clock-out", 560, 580, 390, 140, "Clock Out", actions=[command("clockOut")]),
    zone("tips", 560, 900, 390, 120, "Cash Out My Tips", actions=[command("cashOutTips")], style=fill(GREEN)),
    # Server banks: check out (count your cash); with drawers: this terminal's drawer.
    zone("bank", 970, 900, 390, 120, "My Bank…", actions=[jump(page="drawer")]),
    zone("break", 970, 580, 390, 140, "Start / End Break", actions=[command("toggleBreak")]),
    zone("logout", 560, 740, 390, 140, "Log Out", actions=[command("logout")], style=fill(RED)),
    zone("cancel", 970, 740, 390, 140, "Cancel", actions=[jump(mode="back")]),
    # A manager switches this screen to practice (training) and back.
    zone("practice", 1380, 900, 390, 120, "Practice Mode", actions=[command("toggleTraining")], style=fill(AMBER)),
    zone("message", 150, 900, 390, 120, "Send a Message…", actions=[jump(page="message")], style=fill(BLUE)),
    # Opening / closing tasks, for whoever does them.
    zone("checklists", 150, 740, 390, 140, "Checklists…", actions=[jump(page="checklists")], style=fill(TEAL)),
], role="logout")

admin = [("Menu", "menu"), ("Employees", "employees"), ("Settings", "settings"), ("Taxes", "taxes"),
         ("Tenders", "tenders"), ("Printers", "printers"), ("Reports", "reports"), ("Banks & Drawers", "drawers"),
         ("End of Day", "endOfDay"), ("Terminals", "terminals"), ("Meal Periods", "mealPeriods"),
         ("Modifier Groups", "modifierGroups"), ("Inventory", "inventory"), ("Schedule", "schedule"),
         ("Promotions", "promotions")]
mgr = [label("title", 160, 24, 1600, 100, "Manager")]
# The Manager screens, then the rest.
# Today at a glance first.
slots = [zone("dashboard", 0, 0, 0, 0, "Dashboard", actions=[jump(page="dashboard")], style=fill(BLUE)),
         # The menu on one screen: categories, items, choices, what's on them.
         zone("menu-builder", 0, 0, 0, 0, "Menu Builder", actions=[jump(page="menu-builder")], style=fill(GREEN))]
slots += [zone(f"admin-{panel}", 0, 0, 0, 0, text, actions=[command("openAdmin", panel=panel)]) for text, panel in admin]
slots.append(zone("checklists", 0, 0, 0, 0, "Checklists", actions=[jump(page="checklists")], style=fill(TEAL)))
slots += [
    zone("kitchen-display", 0, 0, 0, 0, "Kitchen Display", actions=[jump(page="kitchen")]),
    zone("bar-display", 0, 0, 0, 0, "Bar Display", actions=[jump(page="bar-display")]),
    zone("expo-display", 0, 0, 0, 0, "Expo Display", actions=[jump(page="expo")]),
    zone("edit-pages", 0, 0, 0, 0, "Edit Pages", actions=[command("editMode")], style=fill(BLUE)),
    zone("setup-guide", 0, 0, 0, 0, "Setup Guide…", actions=[command("setupGuide")], style=fill(TEAL)),
    zone("customers", 0, 0, 0, 0, "Customers…", actions=[jump(page="customers")]),
    zone("gift-cards", 0, 0, 0, 0, "Gift Cards…", actions=[jump(page="gift-card")]),
    zone("sold-out", 0, 0, 0, 0, "Sold Out (86)…", actions=[jump(page="sold-out")]),
    # Touch twice. On a kiosk screen it stays closed until the next boot.
    zone("close-app", 0, 0, 0, 0, "Close ViewTouch", actions=[command("closeApp")], behavior="double",
         style=fill(RED)),
    zone("factory-reset", 0, 0, 0, 0, "Factory Reset…", actions=[jump(page="factory-reset")], style=fill(RED)),
]
# Five across, six rows (the bottom row has its own four).
for i, z in enumerate(slots):
    col, row = i % 5, i // 5
    z["rect"] = rect(160 + col * 324, 124 + row * 134, 300, 120)
    mgr.append(z)
mgr.append(zone("back", 160, 940, 384, 120, "‹ Back", actions=[jump(mode="back")]))
mgr.append(zone("network", 568, 940, 384, 120, "Network…", actions=[jump(page="network")]))
# This screen becomes a kiosk for guests; a manager's PIN ends it.
mgr.append(zone("self-order", 976, 940, 384, 120, "Self-Order Kiosk", actions=[command("selfOrder")], style=fill(BLUE)))
mgr.append(zone("find-check", 1384, 940, 384, 120, "Find a Check…", actions=[jump(page="find-check")]))
page("manager", "Manager", "manager", mgr, role="manager", permission="manager")

# The Menu Builder: categories, a category's items, an item's card.
page("menu-builder", "Menu Builder", "manager", [
    label("title", 16, 16, 1888, 80, "Menu Builder"),
    zone("builder", 16, 112, 1888, 816, kind="menuBuilder"),
    zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
], permission="manager")

# Manager screens (reached through openAdmin from the Manager page)
for pid, name, panel in [("admin-menu", "Menu Items", "menu"), ("admin-employees", "Employees", "employees"),
                         ("admin-tenders", "Payment Types", "tenders"), ("admin-printers", "Printers", "printers"),
                         ("admin-taxes", "Taxes", "taxes"), ("admin-store", "Store Settings", "store"),
                         ("admin-terminals", "Terminals", "terminals"),
                         ("admin-meal-periods", "Meal Periods", "mealPeriods"),
                         ("admin-modifier-groups", "Modifier Groups", "modifierGroups"),
                         ("admin-inventory", "Inventory", "inventory"),
                         ("admin-vendors", "Vendors", "vendors"),
                         ("admin-card-readers", "Card Readers", "cardReaders"),
                         ("admin-promotions", "Promotions", "promotions"),
                         ("admin-punches", "Time Punches", "punches"),
                         ("admin-requests", "Time Off & Swaps", "requests")]:
    # Inventory: vendors, and receiving what they deliver.
    extra = [zone("count", 544, 944, 432, 120, "Count Stock…", actions=[jump(page="stock-count")]),
             zone("vendors", 1008, 944, 432, 120, "Vendors…", actions=[command("openAdmin", panel="vendors")]),
             zone("receive", 1472, 944, 432, 120, "Receive a Delivery…", actions=[jump(page="receive-delivery")],
                  style=fill(GREEN))] if panel == "inventory" else []
    # Terminals: the Stripe readers beside the registers.
    if panel == "terminals":
        extra = [zone("card-readers", 1472, 944, 432, 120, "Card Readers…",
                      actions=[command("openAdmin", panel="cardReaders")])]
    page(pid, name, "manager", [
        label("title", 16, 16, 1888, 80, name),
        zone("editor", 16, 112, 1888, 816, kind="adminPanel", props={"panel": panel}),
        zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
        *extra,
    ], permission="manager")

# Any check, today's or earlier: by number, amount, name, phone, table, item...
page("find-check", "Find a Check", "custom", [
    label("title", 16, 16, 1888, 80, "Find a check"),
    zone("search", 16, 112, 1888, 816, kind="checkSearch"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="check.settle")

page("receive-delivery", "Receive a Delivery", "manager", [
    label("title", 16, 16, 1888, 80, "Receive a delivery"),
    zone("receive", 16, 112, 1888, 816, kind="receiveDelivery"),
    zone("back", 16, 944, 432, 120, "‹ Inventory", actions=[jump(mode="back")]),
], permission="manager")

page("stock-count", "Count Stock", "manager", [
    label("title", 16, 16, 1888, 80, "Count stock"),
    zone("count", 16, 112, 1888, 816, kind="stockCount"),
    zone("back", 16, 944, 432, 120, "‹ Inventory", actions=[jump(mode="back")]),
], permission="manager")

page("factory-reset", "Factory Reset", "manager", [
    label("title", 16, 16, 1888, 80, "Factory reset"),
    zone("reset", 16, 112, 1888, 816, kind="factoryReset"),
    zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
], permission="manager")

# The store's computers and printers: the standby, the screens, printer trouble.
page("network", "Network", "manager", [
    label("title", 16, 16, 1888, 80, "Network"),
    zone("network", 16, 112, 1888, 816, kind="network"),
    zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
], permission="manager")

# Opening and closing tasks, ticked off by whoever does them.
page("checklists", "Checklists", "custom", [
    label("title", 16, 16, 1888, 80, "Checklists"),
    zone("opening", 16, 112, 932, 816, kind="checklist", props={"list": "opening"}),
    zone("closing", 972, 112, 932, 816, kind="checklist", props={"list": "closing"}),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="order")

# Today so far, at a glance.
page("dashboard", "Dashboard", "manager", [
    label("title", 16, 16, 1400, 80, "Today"),
    zone("clock", 1432, 16, 472, 80, kind="clock"),
    zone("dashboard", 16, 112, 1888, 816, kind="dashboard"),
    zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
    zone("reports", 1472, 944, 432, 120, "Reports…", actions=[command("openAdmin", panel="reports")]),
], permission="manager")

page("admin-schedule", "Schedule", "manager", [
    label("title", 16, 16, 1888, 80, "Schedule"),
    zone("schedule", 16, 112, 1888, 816, kind="schedule"),
    zone("back", 16, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
    # Fix a missed or wrong punch (the change and why go on the Labor report).
    zone("punches", 1472, 944, 432, 120, "Time Punches…", actions=[command("openAdmin", panel="punches")],
         style=fill(TEAL)),
    # Time off and shift swaps asked for on the Time Clock.
    zone("requests", 1024, 944, 432, 120, "Requests…", actions=[command("openAdmin", panel="requests")],
         style=fill(AMBER)),
], permission="manager")

page("reports", "Reports", "manager", [
    zone("report", 16, 16, 1440, 1048, kind="reportView"),
    zone("back", 1472, 944, 432, 120, "‹ Manager", actions=[jump(mode="back")]),
], permission="manager")

page("drawer", "Drawer", "manager", [
    zone("drawer", 16, 16, 900, 1048, kind="drawerPanel"),
    zone("pad", 932, 16, 520, 620, kind="numPad", props={"mode": "amount"}),
    zone("reason", 932, 652, 520, 120, "Reason…", actions=[jump(page="drawer-reason")]),
    zone("back", 1472, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="check.settle")

# Why cash left or entered the drawer; Pay Out / Paid In use the typed text.
page("drawer-reason", "Drawer Reason", "manager", [
    label("title", 16, 16, 1888, 80, "Reason for the pay out or paid in"),
    zone("keyboard", 16, 112, 1888, 760, kind="keyboard", props={"placeholder": "Vendor, ice, change…"}),
    zone("cancel", 16, 944, 432, 120, "Cancel", actions=[command("clearText"), jump(mode="back")]),
    zone("done", 1472, 944, 432, 120, "Done", actions=[jump(mode="back")], style=fill(GREEN)),
], permission="manager")

# Kitchen and bar displays. "public": they run without anyone logged in;
# start a kitchen screen with --connect <server> --page kitchen.
KDS = {"normal": {"fill": "#101317", "frame": "flat", "shadow": 0, "radius": 0}}
page("kitchen", "Kitchen Display", "kitchen", [
    zone("tickets", 0, 0, 1920, 1080, kind="kitchenDisplay", props={"station": "kitchen"}, style=KDS),
], role="kitchen", permission="public", background={"fill": "#101317"})
page("bar-display", "Bar Display", "kitchen", [
    zone("tickets", 0, 0, 1920, 1080, kind="kitchenDisplay", props={"station": "bar"}, style=KDS),
], role="bar", permission="public", background={"fill": "#101317"})

# Messages between screens.
page("message", "Send a Message", "custom", [
    label("title", 16, 16, 1888, 80, "Send a message"),
    zone("composer", 16, 112, 1888, 816, kind="messageComposer"),
    zone("back", 16, 944, 432, 120, "‹ Back", actions=[jump(mode="back")]),
], permission="public")

# The expediter: every station's tickets; green when all of it is made.
page("expo", "Expo Display", "kitchen", [
    zone("tickets", 0, 0, 1920, 1080, kind="kitchenDisplay", props={"mode": "expo"}, style=KDS),
], role="expo", permission="public", background={"fill": "#101317"})

page("end-of-day", "End of Day", "manager", [
    zone("eod", 460, 40, 1000, 880, kind="endOfDay"),
    zone("drawer", 460, 944, 480, 120, "Drawer…", actions=[jump(page="drawer")]),
    zone("back", 980, 944, 480, 120, "‹ Manager", actions=[jump(mode="back")]),
], permission="manager")

page("library", "Button Library", "library", [
    label("title", 40, 24, 1840, 80, "Button Library — copy these onto any page"),
    zone("lib-send", 40, 140, 300, 120, "Send", actions=[command("sendOrder")], style=fill(GREEN)),
    zone("lib-void", 360, 140, 300, 120, "Void", actions=[command("voidItem")], behavior="double",
         style=fill(RED)),
    zone("lib-pay", 680, 140, 300, 120, "Pay", actions=[jump(role="settle")], style=fill(BLUE)),
    zone("lib-close", 1320, 140, 300, 120, "Close ViewTouch", actions=[command("closeApp")], behavior="double",
         style=fill(RED)),
    zone("lib-toggle", 1000, 140, 300, 120, "Toggle", behavior="toggle"),
    zone("lib-diamond", 40, 300, 240, 240, "Diamond", shape="diamond"),
    zone("lib-hexagon", 320, 300, 300, 240, "Hexagon", shape="hexagon"),
    zone("lib-octagon", 660, 300, 240, 240, "Octagon", shape="octagon"),
    zone("lib-circle", 940, 300, 240, 240, "Circle", shape="circle"),
    zone("lib-sand", 40, 580, 300, 140, "Sand", style={"normal": {"texture": "sand", "fill": "#ae9877",
         "textColor": "#1b1b1b"}, "selected": {"texture": "litsand", "fill": "#d9c49a"}}),
    zone("lib-marble", 360, 580, 300, 140, "Marble", style={"normal": {"texture": "greenmarble",
         "fill": "#3a5a40"}}),
    zone("lib-wood", 680, 580, 300, 140, "Wood", shape="rounded", style={"normal": {"texture": "darkwood",
         "fill": "#5a3a22"}}),
    zone("lib-parchment", 1000, 580, 300, 140, "Parchment", style={"normal": {"texture": "parchment",
         "fill": "#d8c8a0", "textColor": "#2b2b2b", "textStyle": "embossed"}}),
    zone("lib-disabled", 1320, 580, 300, 140, "Disabled", enabled=False),
    zone("lib-status", 40, 780, 1840, 80, kind="statusBar"),
], background={"texture": "graymarble", "fill": "#555a60"})

# ---------------------------------------------------------------- phone pages
# Phone versions of the pages a server uses. Phones show them in place of the
# page they are a version of (variantOf). Pages without one are shown inside
# the phone order template, their buttons laid out in its content area
# (index, item and modifier pages, the note page).
PW, PH = 1080, 2280

def phone_page(base, name, kind, zones, **kw):
    page(f"{base}-phone", f"{name} (phone)", kind, zones, variantOf=base, formFactor="phone",
         canvas={"w": PW, "h": PH}, **kw)

def grid_buttons(buttons, y, cols, h, x=24, width=PW - 48, gap=16):
    """(id, text, actions, kw) in rows of `cols` from `y`; returns zones and the y below them."""
    out = []
    w = (width - gap * (cols - 1)) // cols
    for i, (zid, text, acts, kw) in enumerate(buttons):
        col, row = i % cols, i // cols
        out.append(zone(zid, x + col * (w + gap), y + row * (h + gap), w, h, text, actions=acts, **kw))
    rows = (len(buttons) + cols - 1) // cols
    return out, y + rows * (h + gap)

phone_page("login", "Login", "login", [
    label("title", 40, 60, 1000, 150, "ViewTouch", style={"normal": {"fontSize": 96}}),
    zone("clock", 140, 230, 800, 100, kind="clock"),
    zone("login-pad", 60, 360, 960, 1400, kind="loginPad"),
    zone("clock-in", 40, 1800, 320, 200, "Clock In", actions=[command("clockIn")]),
    zone("start", 380, 1800, 320, 200, "Log In", actions=[command("login")], style=fill(GREEN)),
    zone("clock-out", 720, 1800, 320, 200, "Clock Out", actions=[command("clockOut")]),
])

tables_buttons, _ = grid_buttons([
    ("quick", "Quick Order", [command("startQuick"), jump(mode="index")], {"style": fill(GREEN)}),
    ("checks", "Open Checks", [jump(role="checkList")], {}),
    ("takeout", "Takeout", [command("startTakeout"), jump(mode="index")], {}),
    ("delivery", "Delivery", [command("startDelivery"), jump(mode="index")], {}),
    ("deliveries", "Deliveries", [jump(page="deliveries")], {"style": fill(BLUE)}),
    ("host", "Host Stand", [jump(page="seating")], {"style": fill(TEAL)}),
    ("manager", "Manager", [jump(role="manager")], {}),
    ("logout", "Log Out", [jump(role="logout")], {"style": fill(RED)}),
], 1640, 3, 190)
phone_page("tables", "Tables", "tables", [
    label("title", 24, 24, 1032, 100, "Tables"),
    zone("tables", 24, 140, 1032, 1480, kind="tableGrid"),
    *tables_buttons,
], background={"fill": "#2a2118"})

phone_page("guest-count", "Guest Count", "guestCount", [
    label("title", 24, 40, 1032, 120, "How many guests?"),
    zone("guests", 140, 180, 800, 220, kind="guestCount"),
    zone("pad", 140, 420, 800, 1300, kind="numPad"),
    zone("cancel", 40, 1780, 480, 220, "Cancel", actions=[command("releaseCheck"), jump(mode="back")]),
    zone("start", 560, 1780, 480, 220, "Start Order", actions=[command("startCheck"), jump(mode="index")],
         style=fill(GREEN)),
])

# The order screen: tabs, the check, the page's buttons, qualifiers, actions.
TAB = {"normal": {"fontSize": 34}}
qualifiers, y = grid_buttons([(f"flow-{q}", q.capitalize(), [{"type": "qualifier", "qualifier": q}], {})
                              for q in ("no", "extra", "lite", "side", "sub")], 1926, 5, 150, x=16, width=PW - 32)
actions, _ = grid_buttons([
    ("flow-void", "Void", [command("voidItem")], {"behavior": "double", "style": fill(RED)}),
    ("flow-send", "Send", [command("sendOrder")], {"style": fill(GREEN)}),
    ("flow-pay", "Pay", [jump(role="settle")], {"style": fill(BLUE)}),
], y, 3, 172, x=16, width=PW - 32)
top_tabs, _ = grid_buttons([
    ("tab-categories", "‹ Menu", [jump(mode="index")], {"style": TAB}),
    ("tab-note", "Note", [jump(page="note")], {"style": TAB}),
    ("tab-check", "Check…", [jump(page="check-options")], {"style": TAB}),
    ("tab-find", "Find", [command("clearText"), jump(page="find-item")], {"style": TAB}),
    ("flow-tables", "Tables", [command("releaseCheck"), jump(role="tables", mode="replace")], {"style": TAB}),
], 16, 5, 110, x=16, width=PW - 32)
phone_page("order-template", "Order Template", "template", [
    *top_tabs,
    zone("order-list", 16, 142, 1048, 720, kind="orderList"),
    *qualifiers,
    *actions,
], contentArea={"x": 16, "y": 878, "w": 1048, "h": 1032})

tender_zones = []
# Six payment types beside the keypad, each still a finger tall on a phone.
th = (840 - 16 * (len(tenders) - 1)) // len(tenders)
for i, (text, tid, color) in enumerate(tenders):
    tender_zones.append(zone(f"tender-{tid}", 552, 792 + i * (th + 16), 512, th, text,
                             actions=tender_action(tid), style=fill(color)))
phone_page("settle", "Settle", "settle", [
    zone("payment", 16, 16, 1048, 760, kind="paymentPanel"),
    zone("pad", 16, 792, 520, 840, kind="numPad", props={"mode": "amount"}),
    *tender_zones,
    zone("close", 16, 1648, 520, 180, "Close Check", actions=[command("closeCheck")], style=fill(GREEN)),
    zone("receipt", 552, 1648, 512, 180, "Print Receipt", actions=[command("printReceipt")]),
    zone("remove-payment", 16, 1844, 336, 180, "Undo Payment", actions=[command("removePayment")]),
    zone("split", 368, 1844, 336, 180, "Split Check", actions=[jump(page="split")]),
    zone("drawer", 720, 1844, 344, 180, "Drawer…", actions=[jump(page="drawer")]),
    zone("done", 16, 2040, 1048, 220, "‹ Back to Order", actions=[jump(mode="back")]),
])

logout_buttons, _ = grid_buttons([
    ("clock-out", "Clock Out", [command("clockOut")], {}),
    ("break", "Start / End Break", [command("toggleBreak")], {}),
    ("logout", "Log Out", [command("logout")], {"style": fill(RED)}),
    ("cancel", "Cancel", [jump(mode="back")], {}),
    ("tips", "Cash Out My Tips", [command("cashOutTips")], {"style": fill(GREEN)}),
    ("bank", "My Bank…", [jump(page="drawer")], {}),
    ("practice", "Practice Mode", [command("toggleTraining")], {"style": fill(AMBER)}),
    ("message", "Send a Message…", [jump(page="message")], {"style": fill(BLUE)}),
], 720, 2, 200)
phone_page("logout", "Log Out", "logout", [
    label("title", 24, 40, 1032, 110, "End of shift"),
    zone("panel", 24, 170, 1032, 520, kind="logoutPanel"),
    *logout_buttons,
])

phone_page("customer", "Customer", "custom", [
    label("title", 24, 24, 1032, 100, "Who is the order for?"),
    zone("customer", 24, 140, 1032, 1860, kind="customerInfo"),
    zone("cancel", 24, 2030, 508, 220, "Cancel", actions=[command("releaseCheck"), jump(mode="back")]),
    zone("menu", 548, 2030, 508, 220, "Menu ›", actions=[jump(mode="index")], style=fill(GREEN)),
])

phone_page("check-list", "Open Checks", "custom", [
    label("title", 24, 24, 1032, 100, "Open checks"),
    zone("list", 24, 140, 1032, 1860, kind="checkList"),
    zone("back", 24, 2030, 1032, 220, "‹ Back", actions=[jump(mode="back")]),
])

print("seed written to", OUT)

# ---------------------------------------------------------------- ready-made layouts
# Five arrangements of the main screens (edit mode -> Layouts…): the same
# buttons (same ids and actions), moved and resized, or restyled. Applying one
# replaces the page's zones; Undo takes it back.
import copy

def arrangement(page_id, vid, name, description, rects=None, styles=None, props=None, add=(), background=None):
    zones = copy.deepcopy(PAGES[page_id]["zones"])
    for z in zones:
        if rects and z["id"] in rects:
            z["rect"] = rect(*rects[z["id"]])
        if styles and z["id"] in styles:
            z["style"] = styles[z["id"]]
        if props and z["id"] in props:
            z["props"] = props[z["id"]]
    zones += [copy.deepcopy(z) for z in add]
    # Each with its background, so going back to Classic brings the page's own back.
    return {"id": vid, "name": name, "description": description, "zones": zones,
            "background": background if background is not None else PAGES[page_id].get("background", {})}

def layouts(page_id, variants):
    p = PAGES[page_id]
    # Kitchen panels side by side keep their own station (the screen's
    # Station button would change them all).
    for v in variants:
        panels = [z for z in v["zones"] if z["kind"] == "kitchenDisplay"]
        if len(panels) > 1:
            for z in panels:
                if "station" in z.get("props", {}):
                    z["props"]["lockStation"] = True
    write(f"layouts/{page_id}.json", {"page": page_id, "role": p.get("role", ""), "variants": variants})

# Login
layouts("login", [
    arrangement("login", "classic", "Classic", "The keypad in the middle, the logo to its left."),
    arrangement("login", "logo-left", "Logo first", "A big logo on the left half, the keypad on the right.", {
        "title": (80, 40, 800, 110), "logo": (80, 170, 800, 680),
        "clock": (1080, 40, 680, 80), "login-pad": (1080, 140, 680, 640),
        "start": (1080, 800, 680, 120), "clock-in": (1080, 936, 330, 110), "clock-out": (1430, 936, 330, 110),
        "hint": (80, 880, 800, 70), "time-clock": (80, 960, 800, 90)}),
    arrangement("login", "keypad-left", "Keypad left", "The keypad on the left, the logo and clock on the right.", {
        "login-pad": (160, 140, 680, 640), "start": (160, 800, 680, 120),
        "clock-in": (160, 936, 330, 110), "clock-out": (510, 936, 330, 110),
        "title": (1000, 40, 840, 110), "logo": (1000, 170, 840, 580), "clock": (1000, 770, 840, 90),
        "hint": (1000, 880, 840, 70), "time-clock": (1000, 960, 840, 90)}),
    arrangement("login", "big-keypad", "Big keypad", "Big keys and buttons, for gloves or a screen farther away.", {
        "logo": (40, 20, 340, 150), "title": (400, 20, 1120, 100), "clock": (660, 120, 600, 60),
        "login-pad": (460, 190, 1000, 690), "clock-in": (160, 900, 480, 150), "start": (680, 900, 560, 150),
        "clock-out": (1280, 900, 480, 150), "hint": (1500, 190, 400, 220), "time-clock": (1500, 430, 400, 120)}),
    arrangement("login", "compact", "Compact", "A small keypad in the center, lots of space around it.", {
        "logo": (810, 10, 300, 110), "title": (560, 120, 800, 90), "clock": (660, 220, 600, 70),
        "login-pad": (760, 310, 400, 430), "start": (760, 760, 400, 110),
        "clock-in": (760, 890, 195, 90), "clock-out": (965, 890, 195, 90), "hint": (1400, 320, 480, 200),
        "time-clock": (1400, 540, 480, 100)}),
])

# Tables
def tables_moved(dx=0, dy=0, scale=1.0, x0=96, y0=96):
    out = {}
    for t in TABLES:
        tid = "table-" + t["label"].lower().replace(" ", "-")
        x, y = 16 + t["x"], 16 + t["y"]
        out[tid] = (int(x0 + (x - 96) * scale + dx), int(y0 + (y - 96) * scale + dy),
                    int(t["w"] * scale), int(t["h"] * scale))
    return out

layouts("tables", [
    arrangement("tables", "classic", "Classic", "The floor plan, the buttons down the right side."),
    arrangement("tables", "buttons-left", "Buttons left", "The buttons down the left side, the floor to their right.", {
        **tables_moved(dx=440),
        "quick": (16, 16, 432, 150), "takeout": (16, 182, 208, 150), "delivery": (240, 182, 208, 150),
        "checks": (16, 348, 432, 150), "status": (16, 514, 432, 140), "switch": (16, 664, 432, 74),
        "manager": (16, 748, 432, 150),
        "logout": (16, 914, 432, 150), "tabs": (1456, 748, 448, 150), "host": (1456, 914, 448, 150)}),
    arrangement("tables", "buttons-bottom", "Buttons below", "The floor on top, big buttons across the bottom.", {
        **tables_moved(scale=0.75, x0=40, y0=24),
        "status": (1200, 16, 704, 220), "switch": (1200, 246, 704, 76), "checks": (1200, 332, 704, 170),
        "manager": (1200, 518, 704, 170),
        **{zid: (16 + i * 315, 716, 299, 348) for i, zid in
           enumerate(["quick", "takeout", "delivery", "tabs", "host", "logout"])}}),
    arrangement("tables", "counter", "Counter first", "Quick Order, Takeout and Delivery big; a smaller floor.", {
        **tables_moved(scale=0.72, x0=24, y0=24),
        "quick": (1100, 16, 804, 300), "takeout": (1100, 332, 396, 220), "delivery": (1508, 332, 396, 220),
        "checks": (1100, 568, 396, 160), "tabs": (1508, 568, 396, 160), "status": (1100, 744, 520, 150),
        "switch": (1632, 744, 272, 150),
        "host": (640, 910, 444, 154), "manager": (1100, 910, 396, 154), "logout": (1508, 910, 396, 154)}),
    arrangement("tables", "plain", "Plain floor", "The classic arrangement on a plain dark floor (no wood).",
                background={"fill": "#1d2128"}),
])

# The order screen (the frame around every menu page). The menu area stays
# where every menu page puts its buttons: x 592-1904, y 104-964.
flow_ids = [f[0] for f in flow]
tab_ids = ["tab-breakfast", "tab-lunch", "tab-dinner", "tab-find", "tab-categories", "tab-note", "tab-check"]
layouts("order-template", [
    arrangement("order-template", "classic", "Classic", "The check on the left, the buttons along the bottom."),
    arrangement("order-template", "buttons-under-check", "Buttons under the check",
                "A shorter check with its buttons in a grid below it.", {
        "order-list": (16, 16, 560, 620),
        **{zid: (16 + (i % 3) * 188, 652 + (i // 3) * 140, 180, 130) for i, zid in enumerate(flow_ids)}}),
    arrangement("order-template", "big-send-pay", "Big Send and Pay", "Send and Pay much bigger; the rest smaller.", {
        **{zid: (16 + i * 168, 980, 160, 84) for i, zid in enumerate(flow_ids[:6])},
        "flow-void": (1024, 980, 200, 84), "flow-send": (1232, 980, 330, 84), "flow-pay": (1570, 980, 334, 84)}),
    arrangement("order-template", "tabs-below", "Menus below", "The buttons on top, the menu tabs along the bottom.", {
        **{zid: (592 + i * 146, 16, 138, 72) for i, zid in enumerate(flow_ids)},
        **{zid: (16 + i * 270, 980, 262, 84) for i, zid in enumerate(tab_ids)}}),
    arrangement("order-template", "colorful", "Colorful", "The classic arrangement, every button in its own color.",
                styles={**{zid: fill(c) for zid, c in zip(flow_ids, [BLUE, AMBER, AMBER, AMBER, AMBER, AMBER,
                                                                     RED, GREEN, BLUE])},
                        **{zid: {"normal": {"fill": TEAL, "fontSize": 26}} for zid in tab_ids}}),
])

# Pay (Settle)
tender_ids = [f"tender-{tid}" for _, tid, _ in tenders]
layouts("settle", [
    arrangement("settle", "classic", "Classic", "The check on the left, the keypad in the middle, payment types right."),
    arrangement("settle", "keypad-left", "Keypad left", "The keypad and its buttons on the left, the check in the middle.", {
        "pad": (16, 16, 520, 620), "receipt": (16, 652, 520, 120), "close": (16, 788, 520, 120),
        "remove-payment": (16, 924, 520, 120), "payment": (552, 16, 900, 1048)}),
    arrangement("settle", "payment-types-top", "Payment types on top", "The payment types across the top.", {
        **{tid: (16 + i * 270, 16, 262, 130) for i, tid in enumerate(tender_ids)},
        "payment": (16, 162, 900, 902), "pad": (932, 162, 520, 560), "receipt": (932, 738, 520, 100),
        "close": (932, 850, 520, 100), "remove-payment": (932, 962, 520, 100),
        "split": (1468, 162, 212, 120), "drawer": (1692, 162, 212, 120), "ask-tip": (1468, 298, 436, 120),
        "off-amount": (1468, 434, 212, 110), "off-percent": (1692, 434, 212, 110),
        "done": (1468, 944, 436, 120)}),
    arrangement("settle", "cash-and-card", "Cash and card first", "Cash and Credit Card big, the rest smaller.", {
        "tender-cash": (1468, 16, 436, 180), "tender-credit": (1468, 206, 436, 180),
        "tender-gift": (1468, 396, 212, 84), "tender-house": (1692, 396, 212, 84),
        "tender-discount": (1468, 490, 212, 84), "tender-comp": (1692, 490, 212, 84),
        "tender-staff-meal": (1468, 584, 212, 84), "off-amount": (1692, 584, 212, 84),
        "off-percent": (1468, 678, 212, 84), "split": (1692, 678, 212, 84),
        "drawer": (1468, 772, 212, 84), "ask-tip": (1692, 772, 212, 84)}),
    arrangement("settle", "wide-buttons", "Wide buttons", "A narrower check, wider keypad and payment buttons.", {
        "payment": (16, 16, 700, 1048), "pad": (732, 16, 600, 620), "receipt": (732, 652, 600, 120),
        "close": (732, 788, 600, 120), "remove-payment": (732, 924, 600, 120),
        **{tid: (1348, 16 + i * step, 556, step - 10) for i, tid in enumerate(tender_ids)},
        "split": (1348, 640, 272, 100), "drawer": (1632, 640, 272, 100), "ask-tip": (1348, 752, 556, 90),
        "off-amount": (1348, 852, 272, 82), "off-percent": (1632, 852, 272, 82),
        "done": (1348, 944, 556, 120)}),
])

# The kitchen screen
ticket = PAGES["kitchen"]["zones"][0]
def kitchen_zone(zid, x, w, props):
    z = copy.deepcopy(ticket)
    z["id"], z["rect"], z["props"] = zid, rect(x, 0, w, 1080), props
    return z
layouts("kitchen", [
    arrangement("kitchen", "classic", "Whole kitchen", "Every kitchen order on one screen."),
    arrangement("kitchen", "grill-fryer", "Grill | Fryer", "Two stations side by side.",
                rects={"tickets": (0, 0, 956, 1080)}, props={"tickets": {"station": "grill", "lockStation": True}},
                add=[kitchen_zone("tickets-2", 964, 956, {"station": "fryer", "lockStation": True})]),
    arrangement("kitchen", "three-stations", "Grill | Fryer | Cold", "Three stations side by side.",
                rects={"tickets": (0, 0, 636, 1080)}, props={"tickets": {"station": "grill"}},
                add=[kitchen_zone("tickets-2", 642, 636, {"station": "fryer"}),
                     kitchen_zone("tickets-3", 1284, 636, {"station": "cold"})]),
    arrangement("kitchen", "kitchen-bar", "Kitchen | Bar", "The kitchen's orders and the bar's, side by side.",
                rects={"tickets": (0, 0, 1276, 1080)},
                add=[kitchen_zone("tickets-2", 1284, 636, {"station": "bar"})]),
    arrangement("kitchen", "kitchen-expo", "Kitchen | Expo", "The kitchen's orders, and the expediter beside them.",
                rects={"tickets": (0, 0, 1276, 1080)},
                add=[kitchen_zone("tickets-2", 1284, 636, {"mode": "expo"})]),
])

