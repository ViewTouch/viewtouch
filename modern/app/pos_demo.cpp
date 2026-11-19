// Demo data: two months of service (and the same two months last year, for
// "vs Last Year", and a few days two and five years back, for Find a
// Check), played through the real POS with a pretend clock - staff clock in,
// tick the opening checklist, take orders with their choices (No onion,
// Extra bacon), send, the kitchen bumps, guests pay by card (a reader's:
// brand and last four, with tips), cash, gift card or house account;
// phone orders with names and a promised time, regulars' "same as last
// time", deliveries out with the driver; a manager's voids, staff meals and
// refunds; vendors deliver twice a week; servers check out, the closing
// checklist, End of Day. Then customers, gift cards, a schedule with time
// off and a swap, a posted message, and right now: tables seated, a bar
// tab, tickets in the kitchen, a delivery out, an order for later, held and
// dirty tables, tonight's waitlist and bookings.
//
// A food truck's (its kind: settings.serviceMode) plays orders at the window
// instead: called by number and name, prep counts made each morning and
// selling out, lunch rushes, orders made, called and picked up; and right
// now orders cooking and waiting at the window. A shop's: sales scanned by
// barcode (a size and color each), produce weighed, returns and exchanges
// back as cash, to the card or as store credit, stock counted and restocked;
// and right now a sale being rung up and one put aside. Each plays the menu
// the store has (a sushi bar's, a bookstore's), not a fixed one.

#include "app/pos_demo.hh"

#include "app/pos_json.hh"
#include "core/report.hh"

#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kMinute = 60'000;
// Clock in; someone with more than one job works their main one.
void clockInMainJob(PosService &pos)
{
    pos.clockIn();
    const QVariantList jobs = pos.clockInJobs().value(u"jobs"_s).toList();
    if (!jobs.isEmpty())
        pos.clockInAs(jobs.first().toMap().value(u"role"_s).toString());
}

// The restaurant's staff: who takes tables, and everyone's PIN and id (a
// food truck's and a shop's: Demo::crewFor).
const std::vector<std::string> kServers = {"1111", "2222", "4444", "1234", "7531"};
// Lou drives (PIN 7777), added by the demo.
const std::vector<std::pair<std::string, std::string>> kStaff = {
    {"1111", "sam"}, {"2222", "casey"}, {"4444", "jo"}, {"1234", "manager"}, {"3333", "riley"}, {"7777", "lou"},
    {"7531", "priya"}, {"1357", "theo"}, {"2468", "ava"}};
const char *const kGuests[][2] = {{"Avery", "555-210-4411"}, {"Blake", "555-341-9902"}, {"Carmen", "555-480-1123"},
                                  {"Devon", "555-602-7781"}, {"Elena", "555-733-5520"}, {"Finn", "555-819-3307"},
                                  {"Grace", "555-904-6612"}, {"Hector", "555-115-2290"}};
const char *const kReasons[] = {"Wrong item", "Food quality", "Charged twice", "Guest complaint"};
const char *const kBrands[] = {"visa", "visa", "mastercard", "amex", "discover"};

struct Demo {
    PosService &pos;
    std::int64_t clock = 0;
    QRandomGenerator rng{20260930};
    std::vector<std::string> sellable;     // menu items guests order
    std::map<std::string, double> stock;   // starting stock, restocked each morning
    std::vector<std::string> cards;        // gift card numbers sold
    int checks = 0;
    // Who works: who takes orders, and everyone (PIN, id); only those on the staff.
    std::vector<std::string> servers = kServers;
    std::vector<std::pair<std::string, std::string>> staff = kStaff;
    // The menu's drinks and desserts (by their categories): one with a meal now and then.
    std::vector<std::string> drinks, desserts;
    int returns = 0;   // a shop's

    int pick(int n) { return n > 0 ? int(rng.bounded(n)) : 0; }
    bool chance(int percent) { return pick(100) < percent; }

    std::int64_t at(QDate day, int hour, int minute = 0) const
    {
        return QDateTime(day, QTime(hour, minute)).toMSecsSinceEpoch();
    }

    void as(const char *pin)
    {
        pos.logout();
        pos.loginWithPin(QString::fromLatin1(pin));
    }
    void as(const std::string &pin) { as(pin.c_str()); }
    const std::string &anyServer(int among = 0) { return servers[pick(among > 0 ? std::min(among, int(servers.size())) : int(servers.size()))]; }

    const MenuItem *item(const std::string &id) const
    {
        for (const MenuItem &m : pos.shared()->menu)
            if (m.id == id)
                return &m;
        return nullptr;
    }
    // The sample café's item, or (another menu: a sushi bar's) one of its own.
    std::string onMenu(const char *id)
    {
        const MenuItem *m = item(id);
        return m && m->available && !m->isModifier ? std::string(id) : sellable[pick(int(sellable.size()))];
    }
    std::string aDrink() { return drinks.empty() ? onMenu("soda") : drinks[pick(int(drinks.size()))]; }

    // The menu's drinks and desserts, and everything else guests order.
    void readMenu()
    {
        const PosShared *s = pos.shared();
        for (const MenuItem &m : s->menu) {
            if (m.isModifier || !m.available)
                continue;
            const MenuCategory *c = nullptr;
            const std::vector<MenuCategory> cats = s->categories();
            for (const MenuCategory &x : cats)
                if (x.id == m.family) c = &x;
            const QString name = c ? QString::fromStdString(c->name).toLower() : QString();
            const bool drink = m.printer == "bar" || (c && c->printer == "bar") || name.contains(u"drink"_s)
                               || name.contains(u"beer"_s) || name.contains(u"wine"_s) || name.contains(u"bevande"_s);
            const bool dessert = name.contains(u"dessert"_s) || name.contains(u"dolci"_s) || name.contains(u"sweet"_s);
            if (drink)
                drinks.push_back(m.id);
            else if (dessert)
                desserts.push_back(m.id);
            sellable.push_back(m.id);
        }
    }

    void amount(std::int64_t cents)
    {
        pos.entryKey(u"clear"_s);
        pos.entryKey(QString::number(std::max<std::int64_t>(cents, 0)));
    }

    // An item, its required choices, and a few optional ones.
    void order(const std::string &id)
    {
        if (!pos.addItem(QString::fromStdString(id)))
            return;
        if (pos.checkInfo().contains(u"idAsk"_s) && !pos.checkInfo().value(u"idAsk"_s).isNull())
            pos.idChecked(true);   // a drink: the server looked at the ID
        for (int guard = 0; guard < 8; ++guard) {
            const QVariantMap info = pos.choosingInfo();
            if (!info.value(u"active"_s).toBool())
                break;
            bool chose = false;
            for (const QVariant &gv : info.value(u"groups"_s).toList()) {
                const QVariantMap g = gv.toMap();
                const int options = int(g.value(u"options"_s).toList().size());
                // Toppings: "No Onion", "Extra Bacon", now and then.
                if (g.value(u"id"_s) == u"toppings"_s && g.value(u"chosen"_s).toInt() == 0 && chance(35)) {
                    pos.chooseOptionAs(g.value(u"id"_s).toString(), pick(options), chance(60) ? u"no"_s : u"extra"_s);
                    chose = true;
                    continue;
                }
                if (!g.value(u"done"_s).toBool() || (g.value(u"chosen"_s).toInt() == 0 && chance(30))) {
                    pos.chooseOption(g.value(u"id"_s).toString(), pick(options));
                    chose = true;
                }
            }
            if (pos.finishChoosing() || !chose)
                break;
        }
        // Burgers: how they're cooked and a side (modifier items).
        if (id.find("burger") != std::string::npos || id == "mushroom-swiss") {
            static const char *temps[] = {"medium-rare", "medium", "medium-well", "well-done"};
            static const char *sides[] = {"fries", "fries", "side-salad", "onion-rings", "sweet-potato-fries"};
            if (id != "kids-burger")
                pos.addItem(QString::fromLatin1(temps[pick(4)]));
            if (id != "kids-burger")
                pos.addItem(QString::fromLatin1(sides[pick(5)]));
        }
    }

    // One table or takeout, from sitting down to paying.
    void serve(QDate, const char *serverPin, std::int64_t start, bool breakfast)   // the day: its start says it
    {
        clock = start;
        as(serverPin);
        const bool takeout = chance(25);
        const bool delivery = takeout && chance(30);
        const int guests = takeout ? 1 : 1 + pick(5);
        bool regular = false;
        if (takeout) {
            pos.startCheck(delivery ? CheckType::Delivery : CheckType::Takeout);
            const auto &customers = pos.shared()->customers;
            if (!customers.empty() && chance(60)) {
                pos.useCustomer(QString::fromStdString(customers[pick(int(customers.size()))].id));
                regular = true;
            } else {   // a phone order: the name, the number (and where it goes)
                const auto &g = kGuests[pick(8)];
                pos.setCustomer({{u"name"_s, QString::fromLatin1(g[0])}, {u"phone"_s, QString::fromLatin1(g[1])},
                                 {u"address"_s, delivery ? u"%1 Oak Ave"_s.arg(10 + pick(990)) : QString()}});
            }
            if (delivery && pos.checkInfo().value(u"customer"_s).toMap().value(u"address"_s).toString().isEmpty()) {
                QVariantMap who = pos.checkInfo().value(u"customer"_s).toMap();
                who[u"address"_s] = u"%1 Elm St"_s.arg(10 + pick(990));
                pos.setCustomer(who);
            }
        } else {
            static const char *tables[] = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "Bar 1", "Bar 2", "Bar 3"};
            pos.selectTable(QString::fromLatin1(tables[pick(10)]));
            amount(guests);
            if (!pos.startCheck(CheckType::DineIn))
                return;
        }
        if (chance(3))
            pos.toggleFlag(u"rush"_s);
        // A regular: often what they had last time.
        const bool same = regular && !pos.checkInfo().value(u"lastOrder"_s).toString().isEmpty() && chance(40)
                          && pos.sameAsLastTime();
        // Breakfast items in the morning, the rest later.
        const int items = same ? 0 : std::max(1, guests + pick(3) - 1);
        for (int i = 0; i < items; ++i) {
            std::string id;
            for (int tries = 0; tries < 6; ++tries) {
                id = sellable[pick(int(sellable.size()))];
                const MenuItem *m = nullptr;
                for (const MenuItem &x : pos.shared()->menu)
                    if (x.id == id) m = &x;
                if (m && (m->family == "breakfast") == breakfast)
                    break;
            }
            order(id);
            if (chance(50))   // something to drink
                order(item("soda") ? std::string(chance(60) ? "soda" : chance(50) ? "coffee" : "draft-beer") : aDrink());
        }
        if (!desserts.empty() && chance(18))   // and to finish
            order(desserts[pick(int(desserts.size()))]);
        clock += 2 * kMinute;
        pos.sendOrder();
        const std::int64_t checkId = pos.checkInfo().value(u"id"_s).toLongLong();
        // Now and then something sent comes off: a manager voids it.
        if (!takeout && chance(3)) {
            as("1234");
            pos.openCheck(checkId);
            const QVariantList lines = pos.lines();
            if (!lines.isEmpty()) {
                pos.selectLine(lines.first().toMap().value(u"id"_s).toLongLong());
                pos.voidItem();
            }
            as(serverPin);
            pos.openCheck(checkId);
        }
        // The kitchen makes it.
        clock += (6 + pick(14)) * kMinute;
        for (const QVariant &tv : pos.kitchenTickets()) {
            const QVariantMap t = tv.toMap();
            if (t.value(u"checkId"_s).toLongLong() == checkId)
                pos.bumpTicket(checkId, t.value(u"sentAt"_s).toLongLong());
        }
        clock += (1 + pick(3)) * kMinute;   // the expediter runs it out
        for (const QVariant &tv : pos.expoTickets()) {
            const QVariantMap t = tv.toMap();
            if (t.value(u"checkId"_s).toLongLong() == checkId)
                pos.expoBump(checkId, t.value(u"sentAt"_s).toLongLong());
        }
        // A delivery: out with Lou, back, and Lou takes the payment.
        if (delivery) {
            pos.releaseCheck();
            as("1234");
            clock += 4 * kMinute;
            pos.sendOut({qint64(checkId)}, u"lou"_s);
            clock += (12 + pick(20)) * kMinute;
            pos.deliveryBack(checkId);
            as("7777");
            pos.openCheck(checkId);
        }
        clock += (takeout ? 3 : 25 + pick(35)) * kMinute;
        if (chance(3)) {
            pos.tender(u"discount"_s);
        } else if (!takeout && chance(1)) {   // a staff meal, rung by the manager
            as("1234");
            pos.openCheck(checkId);
            pos.tender(u"staff-meal"_s);
        }
        pay(takeout);
        if (!pos.closeCheck()) {   // whatever is left, in cash
            pos.tender(u"cash"_s);
            pos.closeCheck();
        }
        ++checks;
        // Once in a while, money back afterwards (a manager's).
        if (chance(2))
            refund(checkId);
    }

    void refund(std::int64_t checkId)
    {
        as("1234");
        for (const Check &c : pos.shared()->closedToday) {
            if (c.id != checkId)
                continue;
            for (const Payment &p : c.payments) {
                if (p.kind != TenderKind::Card && p.kind != TenderKind::Cash)
                    continue;
                const Money paid = p.amount;   // the bill (tips aren't refunded)
                const std::int64_t cents = chance(60) ? std::min<std::int64_t>(paid.cents(), 300 + pick(900)) : 0;
                pos.refundPayment(checkId, p.id, cents, QString::fromLatin1(kReasons[pick(4)]));
                return;
            }
        }
    }

    void pay(bool takeout)
    {
        const std::int64_t due = pos.totals().value(u"balanceCents"_s).toLongLong();
        const int how = pick(100);
        const QVariantMap check = pos.checkInfo();
        if (how < 6 && !cards.empty()) {   // a gift card
            pos.lookupGiftCard(QString::fromStdString(cards[pick(int(cards.size()))]));
            pos.payWithGiftCard();
        } else if (how < 10) {             // the house account, when they have one
            std::vector<const CustomerRecord *> open;
            for (const CustomerRecord &r : pos.shared()->customers)
                if (r.houseAccount && r.accountBalance + Money::fromCents(due) <= r.accountLimit) open.push_back(&r);
            const CustomerRecord *c = open.empty() ? nullptr : open[pick(int(open.size()))];
            if (c) {
                pos.useCustomer(QString::fromStdString(c->id));
                pos.tender(u"house"_s);
            }
        }
        if (pos.totals().value(u"balanceCents"_s).toLongLong() <= 0)
            return;
        if (how < 70) {                    // card, usually with a tip
            const int tipPercent = takeout ? (chance(50) ? 0 : 10) : 15 + pick(9);
            const std::int64_t left = pos.totals().value(u"balanceCents"_s).toLongLong();
            if (chance(70)) {
                // On the card reader: the card's brand and last four, the tip with it.
                const std::int64_t tip = left * tipPercent / 100;
                pos.recordCardPayment({{u"reference"_s, u"sim_%1"_s.arg(rng.generate64(), 0, 16)},
                                       {u"brand"_s, QString::fromLatin1(kBrands[pick(5)])},
                                       {u"last4"_s, u"%1"_s.arg(1000 + pick(9000))},
                                       {u"amountCents"_s, qint64(left + tip)}, {u"tipCents"_s, qint64(tip)},
                                       {u"checkId"_s, pos.checkInfo().value(u"id"_s)}, {u"tenderId"_s, u"credit"_s},
                                       {u"processor"_s, u"simulated"_s}});
                return;
            }
            pos.tender(u"credit"_s);
            if (tipPercent > 0) {
                amount(due * tipPercent / 100);
                pos.addTip(0);
            }
        } else {                           // cash, rounded up
            const std::int64_t given = (due + 499) / 500 * 500;
            pos.tender(u"cash"_s, given);
        }
        Q_UNUSED(check)
    }

    // Everyone out: tips, banks counted, clock out, End of Day.
    void closeDay()
    {
        for (const auto &[pin, id] : staff) {
            clock += 2 * kMinute;   // one at a time
            as(pin);
            pos.cashOutTips();
            const Employee *me = pos.shared()->employee(id);
            for (const DrawerSession &d : pos.shared()->drawers) {
                if (me && d.open() && d.employeeId == me->id) {
                    const Money expected = expectedCash(d, pos.shared()->closedToday, pos.shared()->settings.tax);
                    amount(expected.cents() + (chance(85) ? 0 : (pick(2) ? 1 : -1) * (100 + pick(400))));
                    pos.countDrawer();
                }
            }
            pos.clockOut();
        }
        as("1234");
        for (const DrawerSession &d : pos.shared()->drawers) {   // anything left open
            if (d.open()) {
                amount(expectedCash(d, pos.shared()->closedToday, pos.shared()->settings.tax).cents());
                pos.countDrawerById(d.id);
            }
        }
        clock += 5 * kMinute;
        pos.endOfDay();
    }

    // Stores with a drawer on the terminal: the manager opens it with $200.
    void openDrawer()
    {
        as("1234");
        if (pos.shared()->settings.cashMode != CashMode::ServerBank && !pos.drawerInfo().value(u"open"_s).toBool()) {
            amount(20000);
            pos.openDrawerSession();
        }
    }

    void restock()
    {
        as("1234");
        const QVariantList records = pos.adminRecords(u"inventory"_s);
        for (int i = 0; i < int(records.size()); ++i) {
            QVariantMap r = records[i].toMap();
            const std::string id = r.value(u"id"_s).toString().toStdString();
            if (stock.contains(id) && r.value(u"onHand"_s).toDouble() < stock[id]) {
                r[u"onHand"_s] = stock[id];
                pos.adminSave(u"inventory"_s, i, r);
            }
        }
    }

    // Mondays, before opening: the shelves counted, a little short here and there.
    void countStock(QDate day)
    {
        if (day.dayOfWeek() != 1)
            return;
        as("1234");
        QVariantList lines;
        for (const Ingredient &g : pos.shared()->ingredients) {
            const double off = chance(40) ? std::round(g.onHand * pick(5) / 100.0 * 10) / 10 : 0;
            lines.append(QVariantMap{{u"ingredient"_s, QString::fromStdString(g.id)},
                                     {u"counted"_s, QString::number(std::max(0.0, std::round((g.onHand - off) * 10) / 10))}});
        }
        if (!lines.isEmpty())
            pos.saveStockCount({{u"lines"_s, lines}});
    }

    // Now and then something is dropped, or the greens go bad.
    void throwAway(QDate day)
    {
        if (!chance(45))
            return;
        clock = at(day, 14, 10 + pick(40));
        as(chance(50) ? "1111" : "1234");
        std::vector<std::string> made;
        for (const MenuItem &m : pos.shared()->menu)
            if (!m.recipe.empty() && !m.isModifier)
                made.push_back(m.id);
        const QStringList reasons = {u"Dropped or spilled"_s, u"Made wrong"_s, u"Sent back by a guest"_s};
        if (!made.empty() && chance(70))
            pos.logWaste({{u"item"_s, QString::fromStdString(made[pick(int(made.size()))])}, {u"qty"_s, 1},
                          {u"reason"_s, reasons[pick(3)]}});
        else
            pos.logWaste({{u"ingredient"_s, u"lettuce"_s}, {u"qty"_s, 4 + pick(8)}, {u"reason"_s, u"Expired or spoiled"_s}});
    }

    void playDay(QDate day, double busy)
    {
        clock = at(day, 9, 0);
        countStock(day);
        clock = at(day, 9, 30);
        receive(day);
        restock();
        // Staff in; Sam takes a break mid-afternoon.
        for (const auto &[pin, id] : staff) {
            as(pin);
            clockInMainJob(pos);
        }
        openDrawer();
        checklist(u"opening"_s, day.dayOfWeek() == 7 ? 4 : 5);
        // Mondays the house accounts pay what they owe (by card).
        if (day.dayOfWeek() == 1) {
            as("1234");
            for (const CustomerRecord &c : std::vector<CustomerRecord>(pos.shared()->customers)) {
                if (c.houseAccount && c.accountBalance.cents() > 0) {
                    pos.selectCustomer(QString::fromStdString(c.id));
                    pos.payOnAccount(u"card"_s);
                }
            }
        }
        const bool weekend = day.dayOfWeek() >= 6;
        const int tables = int(std::lround((weekend ? 34 : 24) * busy)) + pick(8);
        for (int i = 0; i < tables; ++i) {
            // Breakfast and lunch rushes, a dinner rush.
            const int slot = pick(100);
            const int hour = slot < 18 ? 10 + pick(2) : slot < 55 ? 12 + pick(2) : slot < 65 ? 14 + pick(3) : 17 + pick(4);
            serve(day, anyServer().c_str(), at(day, hour, pick(60)), hour < 11);
            if (i == tables / 2) {
                as("1111");
                clock = at(day, 15);
                pos.toggleBreak();
                clock = at(day, 15, 30);
                pos.toggleBreak();
            }
        }
        throwAway(day);
        clock = at(day, 22, 15);
        checklist(u"closing"_s, chance(85) ? 5 : 3);   // now and then something is left
        clock = at(day, 22, 30);
        closeDay();
    }

    // The opening or closing checklist, ticked by whoever is there.
    void checklist(const QString &list, int done)
    {
        for (int i = 0; i < done; ++i) {
            as(anyServer(4));
            clock += kMinute;
            pos.tickChecklist(list, i);
        }
    }

    // Vendors deliver Tuesdays and Fridays.
    void receive(QDate day)
    {
        if (day.dayOfWeek() != 2 && day.dayOfWeek() != 5)
            return;
        as("1234");
        for (const Vendor &v : std::vector<Vendor>(pos.shared()->settings.vendors)) {
            QVariantList lines;
            for (const Ingredient &g : pos.shared()->ingredients)
                if (g.vendor == v.id && chance(70))
                    lines.append(QVariantMap{{u"ingredient"_s, QString::fromStdString(g.id)},
                                             {u"qty"_s, std::max(1.0, std::round(stock[g.id] * (0.3 + pick(40) / 100.0)))}});
            if (!lines.isEmpty())
                pos.receiveDelivery({{u"vendor"_s, QString::fromStdString(v.id)},
                                     {u"invoice"_s, u"INV-%1"_s.arg(10000 + pick(89999))}, {u"lines"_s, lines}});
        }
    }

    // --- a food truck ----------------------------------------------------------------

    // The items made ahead and counted down (birria, al pastor...): the
    // menu's first two that are its own, or any two.
    std::vector<std::string> prepped;
    void choosePrepped()
    {
        for (const char *want : {"Birria", "Al Pastor", "Brisket", "Pulled Pork"})
            for (const MenuItem &m : pos.shared()->menu)
                if (prepped.size() < 2 && !m.isModifier && QString::fromStdString(m.name).contains(QLatin1String(want))
                    && std::ranges::find(prepped, m.id) == prepped.end())
                    prepped.push_back(m.id);
        for (std::size_t i = 0; prepped.size() < 2 && i < sellable.size(); ++i)
            if (std::ranges::find(drinks, sellable[i]) == drinks.end())
                prepped.push_back(sellable[i]);
    }

    // One order at the window: rung up, a name (and now and then a pager),
    // paid, made, called and picked up (`waiting`: still cooking, or ready).
    std::int64_t serveTruck(const std::string &pin, std::int64_t start, int waiting = 0)
    {
        clock = start;
        as(pin);
        const int items = 1 + pick(3);
        for (int i = 0; i < items; ++i) {
            std::string id = sellable[pick(int(sellable.size()))];
            if (!prepped.empty() && chance(30))
                id = prepped[pick(int(prepped.size()))];   // the ones they come for
            if (std::ranges::find(drinks, id) == drinks.end())
                order(id);
        }
        if (chance(55))
            order(aDrink());
        if (!pos.hasCheck())
            return 0;
        const auto &g = kGuests[pick(8)];
        pos.setCustomer({{u"name"_s, QString::fromLatin1(g[0])}});
        if (chance(25))
            pos.setPager(1 + pick(40));
        const std::int64_t id = pos.checkInfo().value(u"id"_s).toLongLong();
        clock += kMinute;
        pos.sendOrder();
        pay(true);
        if (!pos.closeCheck()) {
            pos.tender(u"cash"_s);
            pos.closeCheck();
        }
        ++checks;
        if (waiting == 1)
            return id;   // still on the grill
        clock += (4 + pick(9)) * kMinute;
        for (const QVariant &tv : pos.kitchenTickets())
            if (tv.toMap().value(u"checkId"_s).toLongLong() == id)
                pos.bumpTicket(id, tv.toMap().value(u"sentAt"_s).toLongLong());
        pos.orderReady(id);
        if (waiting == 2)
            return id;   // called, not picked up yet
        clock += (1 + pick(4)) * kMinute;
        pos.orderPickedUp(id);
        return id;
    }

    void playTruckDay(QDate day, double busy)
    {
        clock = at(day, 9, 30);
        countStock(day);
        restock();
        for (const auto &[pin, id] : staff) {
            as(pin);
            clockInMainJob(pos);
        }
        openDrawer();
        checklist(u"opening"_s, 5);
        // Made this morning: birria, al pastor; sold out some days.
        as("1234");
        for (const std::string &id : prepped)
            pos.setPrepCount(QString::fromStdString(id), 35 + pick(30));
        const bool weekend = day.dayOfWeek() >= 5;
        const int orders = int(std::lround((weekend ? 70 : 45) * busy)) + pick(10);
        for (int i = 0; i < orders; ++i) {
            // A lunch rush and an evening one.
            const int slot = pick(100);
            const int hour = slot < 55 ? 11 + pick(3) : slot < 70 ? 14 + pick(3) : 17 + pick(4);
            if (hour == 12 && i % 9 == 0) {   // the line's long: Rush (no receipts, Pay sends)
                as("1234");
                pos.setRush(true);
            } else if (hour >= 14 && pos.shared()->settings.rush) {
                as("1234");
                pos.setRush(false);
            }
            serveTruck(anyServer(), at(day, hour, pick(60)));
        }
        throwAway(day);
        clock = at(day, 21, 15);
        checklist(u"closing"_s, chance(85) ? 5 : 3);
        clock = at(day, 21, 30);
        closeDay();
    }

    // --- a shop --------------------------------------------------------------------

    // One sale: scanned (a size and color, or the item's own barcode),
    // weighed, a loyal customer's now and then, paid.
    std::int64_t sell(const std::string &pin, std::int64_t start, bool pay = true)
    {
        clock = start;
        as(pin);
        const int items = 1 + pick(3) + (chance(25) ? 2 : 0);
        for (int i = 0; i < items; ++i) {
            const MenuItem *m = item(sellable[pick(int(sellable.size()))]);
            if (!m)
                continue;
            if (!m->variants.empty()) {
                const MenuItem::Variant &v = m->variants[pick(int(m->variants.size()))];
                pos.addItemByCode(QString::fromStdString(v.barcode));   // the tag scanned
            } else if (m->byWeight) {
                amount(40 + pick(260));   // 0.40 to 3.00 on the scale
                pos.addItem(QString::fromStdString(m->id));
            } else if (!m->barcodes.empty()) {
                pos.addItemByCode(QString::fromStdString(m->barcodes.front()));
            } else {
                pos.addItem(QString::fromStdString(m->id));
            }
        }
        if (!pos.hasCheck())
            return 0;
        const auto &customers = pos.shared()->customers;
        if (!customers.empty() && chance(25))
            pos.useCustomer(QString::fromStdString(customers[pick(int(customers.size()))].id));   // rewards
        const std::int64_t id = pos.checkInfo().value(u"id"_s).toLongLong();
        if (!pay)
            return id;
        payShop();
        if (!pos.closeCheck()) {
            pos.tender(u"cash"_s);
            pos.closeCheck();
        }
        ++checks;
        return id;
    }

    // Card on the reader most of the time, cash, now and then a gift card;
    // no tips in a shop.
    void payShop()
    {
        const std::int64_t due = pos.totals().value(u"balanceCents"_s).toLongLong();
        const int how = pick(100);
        if (how < 5 && !cards.empty()) {
            pos.lookupGiftCard(QString::fromStdString(cards[pick(int(cards.size()))]));
            pos.payWithGiftCard();
            if (pos.totals().value(u"balanceCents"_s).toLongLong() <= 0)
                return;
        }
        if (how < 72) {
            const std::int64_t left = pos.totals().value(u"balanceCents"_s).toLongLong();
            pos.recordCardPayment({{u"reference"_s, u"sim_%1"_s.arg(rng.generate64(), 0, 16)},
                                   {u"brand"_s, QString::fromLatin1(kBrands[pick(5)])},
                                   {u"last4"_s, u"%1"_s.arg(1000 + pick(9000))},
                                   {u"amountCents"_s, qint64(left)}, {u"tipCents"_s, qint64(0)},
                                   {u"checkId"_s, pos.checkInfo().value(u"id"_s)}, {u"tenderId"_s, u"credit"_s},
                                   {u"processor"_s, u"simulated"_s}});
            return;
        }
        pos.tender(u"cash"_s, (due + 499) / 500 * 500);
    }

    // Something brought back (a manager's): from the receipt, why, and back
    // as cash, to the card or as store credit; now and then swapped for another.
    void bringBack(const std::vector<std::int64_t> &sales)
    {
        if (sales.empty())
            return;
        as("1234");
        const std::int64_t sale = sales[pick(int(sales.size()))];
        if (!pos.startReturn(sale))
            return;
        const QVariantList lines = pos.returnSale().value(u"lines"_s).toList();
        if (lines.isEmpty()) {
            pos.cancelReturn();
            return;
        }
        static const char *why[] = {"Wrong size", "Changed mind", "Gift", "Damaged", "Didn't fit"};
        const int reason = pick(5);
        QVariantMap line{{u"line"_s, lines[pick(int(lines.size()))].toMap().value(u"id"_s)}, {u"qty"_s, 1}};
        if (reason == 3)
            line.insert(u"restock"_s, false);   // damaged: not back on the shelf
        if (!pos.returnLines({line}, QString::fromLatin1(why[reason]))) {
            pos.cancelReturn();
            return;
        }
        ++returns;
        if (reason == 0 || chance(20)) {
            // An exchange: another one instead, the difference paid (or given back).
            const MenuItem *m = item(sellable[pick(int(sellable.size()))]);
            if (m && !m->variants.empty())
                pos.addItemByCode(QString::fromStdString(m->variants[pick(int(m->variants.size()))].barcode));
            else if (m && !m->byWeight)
                pos.addItem(QString::fromStdString(m->id));
            if (pos.totals().value(u"balanceCents"_s).toLongLong() > 0) {
                payShop();
                pos.closeCheck();
                return;
            }
        }
        for (const QString &how : {chance(40) ? u"card"_s : u"cash"_s, u"credit"_s, u"cash"_s})
            if (pos.refundReturn(how))
                return;
        pos.cancelReturn();
    }

    void playShopDay(QDate day, double busy)
    {
        clock = at(day, 9, 0);
        countStock(day);
        if (day.dayOfWeek() == 1 || day.dayOfWeek() == 4)   // deliveries Mondays and Thursdays
            restock();
        for (const auto &[pin, id] : staff) {
            as(pin);
            clockInMainJob(pos);
        }
        openDrawer();
        checklist(u"opening"_s, 5);
        const bool weekend = day.dayOfWeek() >= 6;
        const int sales = int(std::lround((weekend ? 48 : 28) * busy)) + pick(8);
        std::vector<std::int64_t> today;
        for (int i = 0; i < sales; ++i) {
            const int slot = pick(100);
            const int hour = slot < 20 ? 10 + pick(2) : slot < 65 ? 12 + pick(4) : 16 + pick(3);
            if (const std::int64_t id = sell(anyServer(), at(day, hour, pick(60))))
                today.push_back(id);
            if (chance(4))
                bringBack(today);
        }
        clock = at(day, 19, 15);
        checklist(u"closing"_s, chance(85) ? 5 : 3);
        clock = at(day, 19, 30);
        closeDay();
    }
};

} // namespace

QString fillDemoData(PosService &pos, std::int64_t realNow)
{
    PosShared *s = pos.shared();
    if (!s->closedToday.empty() || !s->pastDays.empty() || !s->open.empty())
        return QObject::tr("This store already has sales. Run --factory-reset first (it takes a backup).");
    Demo d{pos};
    d.clock = realNow;
    s->setClock([&d] { return d.clock; });
    d.readMenu();
    if (d.sellable.empty())
        return QObject::tr("This store has nothing on its menu to sell yet.");
    for (const Ingredient &g : s->ingredients)
        d.stock[g.id] = g.onHand;
    // A food truck's crew or a shop's (their starter sets'); whoever of them is on the staff.
    const bool truck = s->settings.serviceMode == "foodTruck";
    const bool shop = s->settings.serviceMode == "retail";
    if (truck) {
        d.servers = {"2222", "5555", "6666"};
        d.staff = {{"1234", "manager"}, {"2222", "casey"}, {"5555", "ana"}, {"6666", "luis"}};
    } else if (shop) {
        d.servers = {"2222", "3333", "4444"};
        d.staff = {{"1234", "manager"}, {"2222", "casey"}, {"3333", "jordan"}, {"4444", "avery"}};
    }
    std::erase_if(d.staff, [&](const auto &p) { return p.second != "lou" && !s->employee(p.second); });
    std::erase_if(d.servers, [&](const std::string &pin) {
        return std::ranges::none_of(d.staff, [&](const auto &p) { return p.first == pin; });
    });
    if (d.servers.empty())
        d.servers = {"1234"};
    if (truck)
        d.choosePrepped();
    const auto play = [&](QDate day, double busy) {
        if (truck)
            d.playTruckDay(day, busy);
        else if (shop)
            d.playShopDay(day, busy);
        else
            d.playDay(day, busy);
    };

    const QDate today = QDateTime::fromMSecsSinceEpoch(realNow).date();
    const QTime nowTime = QDateTime::fromMSecsSinceEpoch(realNow).time();
    // Made between midnight and 3: a late night, the day still going since
    // last evening (so yesterday isn't a day of its own).
    const bool lateNight = nowTime.hour() < 3;
    const QDate lastYear = today.addYears(-1).addDays(-60);
    const QDate thisYear = today.addDays(lateNight ? -61 : -60);

    // The store "opened" the morning of the first day (five years back).
    const QDate firstDay = today.addYears(-5);
    d.clock = d.at(firstDay, 9);
    s->day.openedAt = d.clock;
    if (s->sink)
        s->sink->saveDay(s->day, {});

    // Regulars, two with house accounts; tip-outs to the busser and the bar.
    d.as("1234");
    static const char *people[][3] = {
        {"Dana Lee", "555-010-1234", "no onions"}, {"Alex Kim", "555-777-0000", ""},
        {"Robin Park", "555-222-3333", "gate code 1942"}, {"Jordan Diaz", "555-414-2020", ""},
        {"Taylor Brooks", "555-303-9090", "peanut allergy"}, {"Morgan Reyes", "555-818-4545", ""},
        {"Chris Novak", "555-626-1111", ""}, {"Sky Patel", "555-505-7272", "extra napkins"},
        {"Jamie Cole", "555-121-3434", ""}, {"Pat Quinn", "555-999-0101", ""},
        {"Acme Office", "555-600-1000", "lunch orders, deliver to suite 4"},
        {"City Hall", "555-600-2000", "charge to the clerk's account"},
        {"Lena Ortiz", "555-232-8811", ""}, {"Marcus Bell", "555-347-1290", "birthday in June"},
        {"Nina Rossi", "555-458-6633", "gluten free"}, {"Omar Haddad", "555-569-2244", ""},
        {"Priscilla Yu", "555-671-0098", "VIP"}, {"Quinn Murphy", "555-782-4455", ""},
        {"Rafael Santos", "555-893-1176", "prefers the patio"}, {"Sofia Andersen", "555-904-3321", ""},
        {"Tomás Rivera", "555-115-7788", "vegetarian"}, {"Uma Shah", "555-226-9900", ""},
        {"Victor Chen", "555-337-5566", "allergic to shellfish"}, {"Wendy Park", "555-448-2233", ""},
        {"Xavier Moore", "555-559-6677", ""}, {"Yasmin Ali", "555-660-1144", "dairy free"},
        {"Zoe Fischer", "555-771-8899", ""}, {"Ben Okafor", "555-882-3355", ""},
        {"Hannah Kowalski", "555-993-7722", "large orders on Fridays"}, {"Luca Romano", "555-104-6688", ""},
    };
    for (const auto &p : people) {
        pos.saveCustomer({{u"name"_s, QString::fromLatin1(p[0])}, {u"phone"_s, QString::fromLatin1(p[1])},
                          {u"note"_s, QString::fromLatin1(p[2])},
                          {u"address"_s, d.chance(50) ? u"%1 Main St"_s.arg(100 + d.pick(800)) : QString()}});
        if (QString::fromLatin1(p[0]).contains(u"Office"_s) || QString::fromLatin1(p[0]).contains(u"Hall"_s))
            pos.saveCustomer({{u"id"_s, QString::fromStdString(s->customers.back().id)},
                              {u"name"_s, QString::fromLatin1(p[0])}, {u"phone"_s, QString::fromLatin1(p[1])},
                              {u"note"_s, QString::fromLatin1(p[2])}, {u"houseAccount"_s, true},
                              {u"accountLimit"_s, 50000}});
    }
    pos.releaseCheck();
    // A delivery driver, and a delivery fee (a restaurant's).
    if (!truck && !shop) {
        QVariantMap lou = pos.adminNewRecord(u"employees"_s);
        lou[u"id"_s] = u"lou"_s;
        lou[u"name"_s] = u"Lou"_s;
        lou[u"role"_s] = u"driver"_s;
        lou[u"pin"_s] = u"7777"_s;
        lou[u"payRate"_s] = 12.0;
        pos.adminSave(u"employees"_s, -1, lou);
        // A demo PIN, like the samples': retired with them.
        for (Employee &e : s->employees)
            if (e.id == "lou") {
                e.sample = true;
                if (s->sink)
                    s->sink->saveEmployee(e);
            }
    }
    if (!truck && !shop) {
        s->settings.deliveryFee = Money::fromCents(350);
        s->settings.tipOuts = {{"busser", 1500, "tips"}, {"bartender", 200, "sales"}};
        if (s->sink)
            s->sink->saveSettings(s->settings);
    }

    // Gift cards sold on the first days.
    d.clock = d.at(firstDay, 9, 15);
    d.as("1234");
    pos.entryKey(u"clear"_s);
    pos.entryKey(u"20000"_s);
    pos.openDrawerSession();
    for (int i = 0; i < 8; ++i) {
        const QString number = u"6000%1"_s.arg(100000 + i * 7919);
        if (pos.sellGiftCard(number, (2 + d.pick(4)) * 2500)) {
            if (truck)   // a truck's order goes by a name
                pos.setCustomer({{u"name"_s, u"Gift cards"_s}});
            pos.tender(u"credit"_s);
            pos.closeCheck();
            d.cards.push_back(number.toStdString());
        }
    }

    // A few quiet days five and two years back (Find a Check: 5 Years).
    for (const QDate start : {firstDay, today.addYears(-2)})
        for (int i = 0; i < 3; ++i)
            play(start.addDays(i), 0.4);
    for (int i = 0; i < 60; ++i)
        play(lastYear.addDays(i), 0.8);
    for (int i = 0; i < 60; ++i)
        play(thisYear.addDays(i), 1.0);

    // Today so far: opened at 9:30, or (made early in the morning) two
    // hours ago, or (a late night) last evening, so there are guests so far.
    d.clock = lateNight ? realNow - 180 * kMinute
                        : std::min(d.at(today, 9, 30), std::max(d.at(today, 0, 0) + kMinute / 6, realNow - 120 * kMinute));
    d.receive(today);
    d.restock();
    for (const auto &[pin, id] : d.staff) {
        d.as(pin);
        clockInMainJob(pos);
    }
    d.openDrawer();
    d.checklist(u"opening"_s, 5);
    if (truck) {   // made this morning
        d.as("1234");
        for (const std::string &id : d.prepped)
            pos.setPrepCount(QString::fromStdString(id), 45);
    }
    const auto guest = [&](QDate day, std::int64_t at, bool morning) {
        if (truck)
            d.serveTruck(d.anyServer(), at);
        else if (shop)
            d.sell(d.anyServer(), at);
        else
            d.serve(day, d.anyServer(3).c_str(), at, morning);
    };
    // Guests who'd be gone by now (a visit takes up to an hour and a half).
    const std::int64_t lastStart = realNow - 95 * kMinute;
    for (int hour = 10; hour < std::min(22, nowTime.hour()); ++hour) {
        for (int k = 0; k < 2 + d.pick(3); ++k)
            if (const std::int64_t at = d.at(today, hour, d.pick(50)); at <= lastStart)
                guest(today, at, hour < 11);
    }
    // Made early in the morning (or late at night): still a few guests so far
    // (today's reports and the checks to reopen have something).
    if (nowTime.hour() < 11) {
        const std::int64_t from = d.clock;
        const std::int64_t step = std::clamp<std::int64_t>((lastStart - from) / 6, kMinute, 15 * kMinute);
        for (std::int64_t at = from + step; at <= lastStart; at += step)
            guest(QDateTime::fromMSecsSinceEpoch(at).date(), at, !lateNight);
    }

    // The schedule for this week and next.
    d.clock = realNow;
    d.as("1234");
    const int back = (today.dayOfWeek() % 7 - s->settings.weekStartsOn + 7) % 7;
    const QDate week = today.addDays(-back);
    static const char *crew[][3] = {{"sam", "16:00", "22:00"}, {"casey", "10:00", "16:00"}, {"jo", "17:00", "23:30"},
                                    {"riley", "11:00", "19:00"}, {"morgan", "09:00", "17:00"}, {"priya", "11:00", "17:00"},
                                    {"theo", "17:30", "23:30"}, {"ava", "17:00", "22:00"}};
    static const char *truckCrew[][3] = {{"casey", "10:30", "15:30"}, {"ana", "09:30", "17:30"}, {"luis", "14:00", "21:30"},
                                         {"manager", "09:00", "17:00"}};
    static const char *shopCrew[][3] = {{"casey", "09:30", "15:30"}, {"jordan", "13:00", "19:30"}, {"avery", "10:00", "18:00"},
                                        {"manager", "09:00", "17:00"}};
    std::vector<std::array<const char *, 3>> shifts;
    const auto add = [&](const auto &list) {
        for (const auto &c : list)
            if (s->employee(c[0]) || qstrcmp(c[0], "morgan") == 0)
                shifts.push_back({c[0], c[1], c[2]});
    };
    if (truck)
        add(truckCrew);
    else if (shop)
        add(shopCrew);
    else
        add(crew);
    for (int dd = 0; dd < 14; ++dd) {
        const QString day = week.addDays(dd).toString(u"yyyy-MM-dd"_s);
        for (const auto &c : shifts) {
            if (d.chance(78))
                pos.addShift({{u"employeeId"_s, QString::fromLatin1(c[0])}, {u"start"_s, day + u' ' + QString::fromLatin1(c[1])},
                              {u"end"_s, day + u' ' + QString::fromLatin1(c[2])}});
        }
    }

    if (!truck && !shop) {
        // Tonight's host stand: a short line and some bookings.
        static const char *waiting[][2] = {{"Nguyen", "4"}, {"Okafor", "2"}, {"Silva", "6"}};
        for (int i = 0; i < 3; ++i) {
            d.clock = realNow - (25 - i * 9) * kMinute;
            pos.addToWaitlist({{u"name"_s, QString::fromLatin1(waiting[i][0])}, {u"size"_s, QString::fromLatin1(waiting[i][1]).toInt()},
                               {u"phone"_s, u"555-%1"_s.arg(3000 + i * 111)}});
        }
        d.clock = realNow;
        static const char *booked[][3] = {{"Hughes", "2", "19:00"}, {"Martinez", "8", "19:30"}, {"Lee", "4", "20:00"}};
        for (int dd = 0; dd < 2; ++dd) {
            for (const auto &b : booked) {
                const QDate day = today.addDays(dd);
                const QDateTime when(day, QTime::fromString(QString::fromLatin1(b[2]), u"HH:mm"_s));
                if (when.toMSecsSinceEpoch() > realNow)
                    pos.addReservation({{u"name"_s, QString::fromLatin1(b[0])}, {u"size"_s, QString::fromLatin1(b[1]).toInt()},
                                        {u"at"_s, day.toString(u"yyyy-MM-dd"_s) + u' ' + QString::fromLatin1(b[2])},
                                        {u"note"_s, dd == 0 && b[0][0] == 'M' ? u"birthday"_s : QString()}});
            }
        }

        // --- right now ---
        d.clock = realNow - 20 * kMinute;
        // Tables bussed but two (still to clean).
        d.as("1234");
        for (const char *t : {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "Bar 1", "Bar 2", "Bar 3"})
            if (qstrcmp(t, "T3") != 0 && qstrcmp(t, "Bar 2") != 0)
                pos.setTableState(QString::fromLatin1(t), u"clean"_s);
        // Seated, ordered, in the kitchen.
        const auto seat = [&](const char *pin, const char *table, int guests, std::initializer_list<const char *> items) {
            d.as(pin);
            pos.selectTable(QString::fromLatin1(table));
            d.amount(guests);
            if (!pos.startCheck(CheckType::DineIn))
                return;
            for (const char *i : items)
                d.order(d.onMenu(i));
            pos.sendOrder();
            pos.releaseCheck();
        };
        seat("1111", "T2", 3, {"classic-burger", "cobb", "soda", "coffee"});
        d.clock += 6 * kMinute;
        seat("2222", "T7", 2, {"bacon-burger", "house-salad", "draft-beer"});
        // A tab at the bar.
        d.as("4444");
        if (pos.openTab(u"Riley's friends"_s)) {
            d.order(d.onMenu("draft-beer"));
            d.order(d.onMenu("draft-beer"));
            pos.sendOrder();
            pos.releaseCheck();
        }
        // A phone order cooking (promised for later), and a delivery out with Lou.
        d.clock += 4 * kMinute;
        d.as("2222");
        if (pos.startCheck(CheckType::Takeout)) {
            pos.setCustomer({{u"name"_s, u"Avery"_s}, {u"phone"_s, u"555-210-4411"_s}});
            d.order(d.onMenu("cheeseburger"));
            d.order(d.onMenu("soda"));
            pos.sendOrder();
            pos.releaseCheck();
        }
        d.as("1111");
        if (pos.startCheck(CheckType::Delivery)) {
            pos.setCustomer({{u"name"_s, u"Blake"_s}, {u"phone"_s, u"555-341-9902"_s}, {u"address"_s, u"42 Oak Ave"_s}});
            d.order(d.onMenu("mushroom-swiss"));
            d.order(d.onMenu("lemonade"));
            pos.sendOrder();
            const qint64 out = pos.checkInfo().value(u"id"_s).toLongLong();
            for (const QVariant &tv : pos.kitchenTickets())
                if (tv.toMap().value(u"checkId"_s).toLongLong() == out)
                    pos.bumpTicket(out, tv.toMap().value(u"sentAt"_s).toLongLong());
            pos.releaseCheck();
            d.as("1234");
            pos.sendOut({out}, u"lou"_s);
        }
        // Tonight at 7:30, for pickup.
        d.as("2222");
        if (pos.startCheck(CheckType::Takeout)) {
            pos.setCustomer({{u"name"_s, u"Carmen"_s}, {u"phone"_s, u"555-480-1123"_s}});
            pos.setDueAt(QDateTime(today, QTime(19, 30)).toMSecsSinceEpoch() > realNow
                             ? QDateTime(today, QTime(19, 30)).toMSecsSinceEpoch()
                             : QDateTime(today.addDays(1), QTime(12, 0)).toMSecsSinceEpoch());
            d.order(d.onMenu("house-salad"));
            d.order(d.onMenu("veggie-burger"));
            pos.sendOrder();
            pos.releaseCheck();
        }
        d.clock = realNow;
        // A walk-in just seated; the Martinez party's tables held.
        d.as("1234");
        pos.seatWalkIn(2, {u"T1"_s}, u"casey"_s);
        for (const QVariant &v : pos.waitlistInfo().value(u"booked"_s).toList())
            if (v.toMap().value(u"name"_s) == u"Martinez"_s) {
                pos.reserveTables(v.toMap().value(u"id"_s).toLongLong(), {u"T5"_s, u"T6"_s});
                break;
            }
        // A note for everyone, up until closing (made late at night: a few hours).
        pos.sendMessage(u"all"_s, u"86 %1 tonight"_s.arg(QString::fromStdString(d.item(d.onMenu("smoked-brisket"))->name)),
                        std::max(QDateTime(today, QTime(23, 0)).toMSecsSinceEpoch(), realNow + 4 * 3'600'000LL));
        // Time off and a swap: one approved, one waiting; a shift up for grabs, taken.
        const auto askOff = [&](const char *pin, int days, const char *why) {
            pos.timeClockStart(QString::fromLatin1(pin));
            pos.timeClockRequestOff(today.addDays(days).toString(u"yyyy-MM-dd"_s), QString::fromLatin1(why));
            pos.timeClockDone();
        };
        askOff("1111", 10, "Family wedding");
        askOff("2222", 5, "Doctor");
        for (const Shift &sh : std::vector<Shift>(s->shifts))
            if (sh.employeeId == "jo" && sh.start > realNow + 2 * 24 * 3'600'000) {
                pos.timeClockStart(u"4444"_s);
                pos.timeClockGiveAway(sh.id);
                pos.timeClockDone();
                for (const PosSettings::StaffRequest &r : std::vector<PosSettings::StaffRequest>(s->settings.staffRequests))
                    if (r.kind == "swap" && r.shiftId == sh.id) {
                        pos.timeClockStart(u"3333"_s);
                        pos.timeClockTake(r.id);
                        pos.timeClockDone();
                    }
                break;
            }
        d.as("1234");
        const QVariantList requests = pos.adminRecords(u"requests"_s);
        for (int i = 0; i < int(requests.size()); ++i)
            if (requests[i].toMap().value(u"_title"_s).toString().contains(u"Casey"_s)
                || requests[i].toMap().value(u"employee"_s).toString().contains(u"Casey"_s)) {
                QVariantMap r = requests[i].toMap();
                r[u"status"_s] = u"approved"_s;
                pos.adminSave(u"requests"_s, i, r);
                break;
            }
    } else if (truck) {
        // --- right now, at the window ---
        // Orders cooking, two called and waiting, one just handed out; the
        // birria running low.
        d.clock = realNow - 14 * kMinute;
        for (int i = 0; i < 3; ++i)
            d.serveTruck(d.anyServer(), d.clock + 2 * kMinute, 1);
        for (int i = 0; i < 2; ++i)
            d.serveTruck(d.anyServer(), d.clock + kMinute, 2);
        d.serveTruck(d.anyServer(), d.clock + kMinute);
        d.clock = realNow;
        d.as("1234");
        if (!d.prepped.empty())
            pos.setPrepCount(QString::fromStdString(d.prepped.front()), 8);
        pos.sendMessage(u"all"_s, u"Parked at 5th & Main tomorrow: 11 to 8"_s,
                        std::max(QDateTime(today, QTime(23, 0)).toMSecsSinceEpoch(), realNow + 4 * 3'600'000LL));
        pos.timeClockStart(u"5555"_s);
        pos.timeClockRequestOff(today.addDays(6).toString(u"yyyy-MM-dd"_s), u"Family visit"_s);
        pos.timeClockDone();
    } else {
        // --- right now, at the counter ---
        // A sale being rung up, another put aside while they look for a size;
        // a popular size down to its last one.
        d.clock = realNow - 6 * kMinute;
        d.sell(d.anyServer(), d.clock, false);
        pos.releaseCheck();
        d.clock = realNow - 2 * kMinute;
        d.sell(d.anyServer(), d.clock, false);
        pos.releaseCheck();
        d.clock = realNow;
        d.as("1234");
        for (const MenuItem &m : std::vector<MenuItem>(s->menu))
            if (!m.variants.empty() && !m.recipe.empty()) {
                pos.setVariantStock(QString::fromStdString(m.id), QString::fromStdString(m.variants[m.variants.size() / 2].key), 1);
                break;
            }
        pos.sendMessage(u"all"_s, u"New arrivals on Friday: make room on the front table"_s,
                        std::max(QDateTime(today, QTime(23, 0)).toMSecsSinceEpoch(), realNow + 4 * 3'600'000LL));
        pos.timeClockStart(u"3333"_s);
        pos.timeClockRequestOff(today.addDays(8).toString(u"yyyy-MM-dd"_s), u"Exams"_s);
        pos.timeClockDone();
    }

    pos.logout();
    s->setClock([] { return QDateTime::currentMSecsSinceEpoch(); });
    if (truck)
        return QObject::tr("Added %1 orders over 126 days (some years back), %2 customers, %3 gift cards, prep counts "
                           "and rushes, checklists, a schedule, and orders cooking and waiting at the window.")
            .arg(d.checks).arg(s->customers.size()).arg(d.cards.size());
    if (shop)
        return QObject::tr("Added %1 sales over 126 days (some years back), %2 returns and exchanges, %3 customers, %4 gift "
                           "cards, stock counts and deliveries, a schedule, and a sale being rung up.")
            .arg(d.checks).arg(d.returns).arg(s->customers.size()).arg(d.cards.size());
    return QObject::tr("Added %1 checks over 126 days (some years back), %2 customers, %3 gift cards, deliveries, "
                       "refunds, checklists, a schedule with requests, and tonight's floor, kitchen and waitlist.")
        .arg(d.checks).arg(s->customers.size()).arg(d.cards.size());
}

} // namespace vt::app
