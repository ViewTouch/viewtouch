#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "print/document.hh"
#include "print/tickets.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Inventory: recipes use stock up, sold out by itself, low stock, food cost.

namespace {

double onHand(PosService &pos, const char *id)
{
    const core::Ingredient *g = pos.shared()->ingredient(id);
    REQUIRE(g);
    return g->onHand;
}

int indexOf(PosService &pos, const char *id)
{
    for (int i = 0; i < int(pos.shared()->ingredients.size()); ++i)
        if (pos.shared()->ingredients[i].id == id)
            return i;
    return -1;
}

void setOnHand(PosService &pos, const char *id, double amount)
{
    const int i = indexOf(pos, id);
    QVariantMap r = pos.adminRecords(u"inventory"_s)[i].toMap();
    r[u"onHand"_s] = amount;
    REQUIRE(pos.adminSave(u"inventory"_s, i, r));
}

QStringList cellsOf(const QVariantMap &report, const QString &first)
{
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (!cells.isEmpty() && cells.first() == first)
            return cells;
    }
    return {};
}

std::vector<std::string> rowIn(const core::Report &r, const std::string &section, const std::string &first)
{
    std::string current;
    for (const core::ReportRow &x : r.rows) {
        if (x.kind == core::ReportRow::Kind::Section)
            current = x.cells.empty() ? std::string() : x.cells.front();
        else if (current == section && !x.cells.empty() && x.cells.front() == first)
            return x.cells;
    }
    return {};
}

} // namespace

TEST_CASE("Combos: a side and a drink from the menu, their stock, sold out, the Items report", "[inventory][combo]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const double buns = onHand(pos, "bun");
    const double potatoes = onHand(pos, "potatoes");

    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"burger-combo"_s);
    QVariantMap choosing = pos.choosingInfo();
    REQUIRE(choosing[u"active"_s].toBool());
    CHECK(choosing[u"groups"_s].toList().size() == 3);   // Temperature, Side, Drink

    // A sold-out drink can't be picked.
    REQUIRE(pos.setAvailable(u"lemonade"_s, false));
    const QVariantList drinks = pos.choosingInfo()[u"groups"_s].toList()[2].toMap()[u"options"_s].toList();
    CHECK(drinks[2].toMap()[u"soldOut"_s].toBool());
    CHECK_FALSE(pos.chooseOption(u"combo-drink"_s, 2));
    REQUIRE(pos.setAvailable(u"lemonade"_s, true));

    REQUIRE(pos.chooseOption(u"temperature"_s, 2));      // Medium
    REQUIRE(pos.chooseOption(u"side"_s, 0));             // Fries
    REQUIRE(pos.chooseOption(u"combo-drink"_s, 2));      // Lemonade + 0.30
    REQUIRE(pos.finishChoosing());
    const core::OrderLine &line = pos.shared()->open.at(pos.checkInfo()[u"id"_s].toLongLong()).lines.front();
    CHECK(line.total() == Money::fromCents(1695 + 30));
    CHECK(line.modifiers[1].itemId == "fries");
    CHECK(line.modifiers[2].itemId == "lemonade");
    CHECK_FALSE(line.modifiers[1].kitchenHide);          // the fries are made in the kitchen
    CHECK(line.modifiers[2].kitchenHide);                 // the lemonade isn't

    REQUIRE(pos.sendOrder());
    QStringList made;
    for (const QVariant &l : pos.kitchenTickets().last().toMap()[u"lines"_s].toList())
        made << l.toMap()[u"name"_s].toString() << l.toMap()[u"modifiers"_s].toStringList();
    CHECK(made.join(u'|').contains(u"Fries"_s));
    CHECK_FALSE(made.join(u'|').contains(u"Lemonade"_s));
    {
        const core::Check &c = pos.shared()->open.begin()->second;
        print::TicketContext ctx{pos.shared()->settings, [](std::int64_t) { return std::string("1/1"); },
                                 [](std::int64_t) { return std::string("12:00"); }, 0};
        const std::string ticket = print::renderText(print::kitchenTicket(c, c.lines, "Kitchen", false, ctx), 42);
        CHECK(ticket.find("Fries") != std::string::npos);
        CHECK(ticket.find("Lemonade") == std::string::npos);
    }
    CHECK(onHand(pos, "bun") == buns - 1);               // the burger's own recipe
    CHECK(onHand(pos, "potatoes") == potatoes - 6);      // and the fries'

    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());
    const core::Report r = pos.buildReport(u"items"_s);
    CHECK(rowIn(r, "Chosen with other items", "Fries") == std::vector<std::string>{"Fries", "1", "$0.00"});
    CHECK(rowIn(r, "Chosen with other items", "Lemonade") == std::vector<std::string>{"Lemonade", "1", "$0.30"});
    CHECK(rowIn(r, "Chosen with other items", "Medium").empty());   // not a menu-item group
}

TEST_CASE("Combos: a group of menu items only takes names on the menu", "[inventory][combo]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap g{{u"name"_s, u"Combo Side"_s}, {u"min"_s, 1}, {u"max"_s, 1}, {u"menuItems"_s, true},
                  {u"options"_s, u"Fries\nOnion Rings + 1.00\nTater Tots"_s}};
    CHECK_FALSE(pos.adminSave(u"modifierGroups"_s, -1, g));        // no Tater Tots on the menu
    g[u"options"_s] = u"fries\nOnion Rings + 1.00"_s;
    REQUIRE(pos.adminSave(u"modifierGroups"_s, -1, g));
    const core::ModifierGroup &saved = pos.shared()->settings.modifierGroups.back();
    CHECK(saved.menuItems);
    CHECK(saved.options[0].itemId == "fries");
    CHECK(saved.options[1].itemId == "onion-rings");
    CHECK(saved.options[1].price == Money::fromCents(100));
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).modifierGroups.back() == saved);
}

TEST_CASE("Inventory: sending uses stock, a void gives it back, modifiers count", "[inventory]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.shared()->ingredients.size() >= 10);
    const double buns = onHand(pos, "bun");
    const double bacon = onHand(pos, "bacon");
    const double potatoes = onHand(pos, "potatoes");

    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"bacon-burger"_s);                  // bun, patty, cheese, 2 bacon
    pos.addItem(u"fries"_s);                         // a modifier: 6 oz potatoes
    pos.addItem(u"cheeseburger"_s);
    CHECK(onHand(pos, "bun") == buns);               // nothing until it goes to the kitchen
    REQUIRE(pos.sendOrder());
    CHECK(onHand(pos, "bun") == buns - 2);
    CHECK(onHand(pos, "bacon") == bacon - 2);
    CHECK(onHand(pos, "potatoes") == potatoes - 6);

    REQUIRE(pos.voidItem());                         // the cheeseburger, never made
    CHECK(onHand(pos, "bun") == buns - 1);

    // Two of an item: twice the recipe; "No" on a modifier uses none.
    pos.addItem(u"two-eggs"_s);
    pos.lines();
    const qint64 eggsBefore = qint64(onHand(pos, "eggs"));
    REQUIRE(pos.sendOrder());
    CHECK(qint64(onHand(pos, "eggs")) == eggsBefore - 2);
}

TEST_CASE("Inventory: sold out by itself when short, back when restocked; low-stock warning", "[inventory]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QStringList notices;
    QObject::connect(&pos, &PosService::notice, [&](const QString &n) { notices << n; });

    setOnHand(pos, "veggie-patty", 1);               // low (at 4) - and one left
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"veggie-burger"_s);
    REQUIRE(pos.sendOrder());
    CHECK(onHand(pos, "veggie-patty") == 0);
    CHECK(pos.soldOut().contains(u"veggie-burger"_s));
    CHECK(notices.join(u'|').contains(u"now sold out: Veggie Burger"_s));
    CHECK_FALSE(pos.addItem(u"veggie-burger"_s));     // can't be ordered
    CHECK(pos.lowStock().size() >= 1);
    CHECK(pos.adminRecords(u"inventory"_s)[indexOf(pos, "veggie-patty")].toMap()[u"_detail"_s]
              .toString().contains(u"OUT"_s));

    // Crossing the low mark warns once.
    notices.clear();
    setOnHand(pos, "bun", 13);                       // low at 12
    pos.addItem(u"kids-burger"_s);
    REQUIRE(pos.sendOrder());
    CHECK(notices.join(u'|').contains(u"Running low: Burger Buns (12 each left)"_s));

    // Restocked: back on the menu.
    setOnHand(pos, "veggie-patty", 12);
    CHECK_FALSE(pos.soldOut().contains(u"veggie-burger"_s));
    // One a person 86'd stays sold out after a restock.
    REQUIRE(pos.setAvailable(u"cobb"_s, false));
    setOnHand(pos, "chicken", 30);
    CHECK(pos.soldOut().contains(u"cobb"_s));
}

TEST_CASE("Recipes in Manager -> Menu, ingredients in Manager -> Inventory", "[inventory][admin]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // A new ingredient, then a recipe that uses it (by name or id).
    QVariantMap g = pos.adminNewRecord(u"inventory"_s);
    g[u"name"_s] = u"Avocado"_s;
    g[u"unit"_s] = u"each"_s;
    g[u"onHand"_s] = 10.0;
    g[u"lowAt"_s] = 3.0;
    g[u"cost"_s] = 1.25;
    REQUIRE(pos.adminSave(u"inventory"_s, -1, g));
    REQUIRE(pos.shared()->ingredient("avocado"));

    int cobb = -1;
    for (int i = 0; i < int(pos.shared()->menu.size()); ++i)
        if (pos.shared()->menu[i].id == "cobb") cobb = i;
    QVariantMap r = pos.adminRecords(u"menu"_s)[cobb].toMap();
    CHECK(r[u"recipe"_s].toString().startsWith(u"lettuce 4\nchicken 1"_s));
    r[u"recipe"_s] = r[u"recipe"_s].toString() + u"\nAvocado 0.5"_s;
    REQUIRE(pos.adminSave(u"menu"_s, cobb, r));
    CHECK(pos.shared()->menu[cobb].recipe.back() == core::RecipeLine{"avocado", 0.5});
    r[u"recipe"_s] = u"unicorn 1"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, cobb, r));
    r[u"recipe"_s] = u"bacon -2"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, cobb, r));

    // Saved with the item and the ingredient.
    const core::MenuItem &m = pos.shared()->menu[cobb];
    CHECK(app::menuItemFromJson(app::toJson(m)) == m);
    const core::Ingredient &a = *pos.shared()->ingredient("avocado");
    CHECK(app::ingredientFromJson(app::toJson(a)) == a);
}

TEST_CASE("Food cost report: sales against recipe cost, and the stock", "[inventory][reports]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"classic-burger"_s);                // $11.50; bun .35 + patty 1.60 + lettuce .06 = $2.01
    pos.addItem(u"water"_s);                         // no recipe
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    const QVariantMap report = pos.report(u"foodcost"_s);
    const QStringList burger = cellsOf(report, u"Classic Burger"_s);
    REQUIRE(burger.size() == 5);
    CHECK(burger[1] == u"1"_s);
    CHECK(burger[3] == u"$2.01"_s);
    CHECK(burger[4] == u"17.5%"_s);
    CHECK(cellsOf(report, u"Water"_s)[3] == u"-"_s);
    CHECK(cellsOf(report, u"Burger Buns"_s).size() == 5);
}

TEST_CASE("Menu engineering: each item by how it sells and what it earns", "[inventory][reports]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 3; ++i)
        pos.addItem(u"classic-burger"_s);          // earns $11.50 - $2.01 = $9.49 each
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    const QVariantMap report = pos.report(u"engineering"_s);
    const QStringList burger = cellsOf(report, u"Classic Burger"_s);
    REQUIRE(burger.size() == 6);
    CHECK(burger[1] == u"3"_s);
    CHECK(burger[2] == u"100.0%"_s);
    CHECK(burger[3] == u"$9.49"_s);
    CHECK(burger[4] == u"$28.47"_s);
    CHECK(burger[5] == u"Star"_s);
    // Another burger, not sold: slow.
    int slow = 0;
    for (const core::MenuItem &m : pos.shared()->menu) {
        if (m.family != pos.findItem(u"classic-burger"_s)->family || m.id == "classic-burger" || m.isModifier)
            continue;
        const QStringList row = cellsOf(report, QString::fromStdString(m.name + (m.recipe.empty() ? " *" : "")));
        REQUIRE(row.size() == 6);
        CHECK(row[1] == u"0"_s);
        CHECK((row[5] == u"Weak"_s || row[5] == u"Hidden gem"_s));
        ++slow;
    }
    CHECK(slow > 0);
    // Water's category sold nothing: not in it.
    CHECK(cellsOf(report, u"Water"_s).isEmpty());
}

TEST_CASE("Inventory is saved and comes back; starter stock on a new store", "[inventory][store]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees, nullptr, seed.ingredients));
    }
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1111"_s));
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        pos.addItem(u"kids-burger"_s);
        pos.finishChoosing();
        REQUIRE(pos.sendOrder());
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->ingredients.size() == seed.ingredients.size());
    CHECK(data->ingredients[0].id == "bun");
    CHECK(data->ingredients[0].onHand == seed.ingredients[0].onHand - 1);
}

TEST_CASE("Vendors, and which vendor each ingredient comes from", "[inventory][vendors]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.adminRecords(u"vendors"_s).size() == 3);               // the demo's
    QVariantMap v = pos.adminNewRecord(u"vendors"_s);
    v[u"name"_s] = u"Ice House"_s;
    v[u"phone"_s] = u"555-0101"_s;
    REQUIRE(pos.adminSave(u"vendors"_s, -1, v));
    REQUIRE(pos.adminRecords(u"vendors"_s).size() == 4);
    CHECK(pos.shared()->settings.vendors.back().id == "ice-house");
    v[u"name"_s] = u""_s;
    CHECK_FALSE(pos.adminSave(u"vendors"_s, -1, v));                 // needs a name

    // An ingredient's vendor is chosen from them.
    bool listed = false;
    for (const QVariant &f : pos.adminFields(u"inventory"_s))
        if (f.toMap()[u"path"_s] == u"vendor"_s)
            for (const QVariant &o : f.toMap()[u"options"_s].toList())
                listed = listed || o.toMap()[u"text"_s] == u"Ice House"_s;
    CHECK(listed);
    const int bun = indexOf(pos, "bun");
    QVariantMap r = pos.adminRecords(u"inventory"_s)[bun].toMap();
    CHECK(r[u"vendor"_s] == u"city-bakery"_s);
    r[u"vendor"_s] = u"valley-foods"_s;
    REQUIRE(pos.adminSave(u"inventory"_s, bun, r));
    CHECK(pos.shared()->ingredient("bun")->vendor == "valley-foods");

    REQUIRE(pos.adminDelete(u"vendors"_s, 3));
    CHECK(pos.adminRecords(u"vendors"_s).size() == 3);
}

TEST_CASE("Receiving a delivery: stock up, costs updated, kept for Purchases", "[inventory][vendors]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));                            // servers can't
    CHECK_FALSE(pos.receiveDelivery({{u"vendor"_s, u"city-bakery"_s},
                                     {u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"qty"_s, 24}}}}}));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // Out of buns: every burger is sold out until they come.
    setOnHand(pos, "bun", 0);
    CHECK_FALSE(pos.shared()->menu[0].available);

    CHECK_FALSE(pos.receiveDelivery({{u"vendor"_s, u"nobody"_s}, {u"lines"_s, QVariantList{}}}));
    CHECK_FALSE(pos.receiveDelivery({{u"vendor"_s, u"city-bakery"_s}, {u"lines"_s, QVariantList{}}}));   // nothing came
    CHECK_FALSE(pos.receiveDelivery({{u"vendor"_s, u"city-bakery"_s},
                                     {u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"qty"_s, -2}}}}}));
    const double bread = onHand(pos, "bread");
    REQUIRE(pos.receiveDelivery({{u"vendor"_s, u"city-bakery"_s}, {u"invoice"_s, u"A-1001"_s},
                                 {u"lines"_s, QVariantList{
                                      QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"qty"_s, 48}, {u"cost"_s, 0.40}},
                                      QVariantMap{{u"ingredient"_s, u"bread"_s}, {u"qty"_s, 20}}}}}));   // its usual cost
    CHECK(onHand(pos, "bun") == 48);
    CHECK(onHand(pos, "bread") == bread + 20);
    CHECK(pos.shared()->ingredient("bun")->cost.cents() == 40);       // the new cost
    CHECK(pos.shared()->ingredient("bread")->cost.cents() == 12);
    CHECK(pos.shared()->menu[0].available);                          // burgers are back
    REQUIRE(pos.shared()->deliveries.size() == 1);
    const core::Delivery &d = pos.shared()->deliveries.front();
    CHECK(d.vendorName == "City Bakery");
    CHECK(d.invoice == "A-1001");
    CHECK(d.by == "Morgan (Manager)");
    CHECK(d.total().cents() == 48 * 40 + 20 * 12);                   // $21.60
    CHECK(app::deliveryFromJson(app::toJson(d)) == d);

    // What the Receive screen shows.
    const QVariantMap info = pos.receiving();
    CHECK(info[u"vendors"_s].toList().size() == 3);
    CHECK(info[u"recent"_s].toList().size() == 1);
    CHECK(info[u"recent"_s].toList()[0].toMap()[u"total"_s] == u"$21.60"_s);

    // Reports -> Purchases.
    const QVariantMap report = pos.report(u"purchases"_s);
    CHECK(cellsOf(report, u"City Bakery"_s).value(3) == u"$21.60"_s);
    CHECK(cellsOf(report, u"Total received"_s).value(3) == u"$21.60"_s);
    CHECK(cellsOf(report, u"Burger Buns"_s).value(1) == u"48 each"_s);
}

TEST_CASE("Deliveries are saved and come back", "[inventory][vendors][store]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees, nullptr, seed.ingredients));
    }
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        REQUIRE(pos.receiveDelivery({{u"vendor"_s, u"green-farms"_s},
                                     {u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"lettuce"_s}, {u"qty"_s, 40}}}}}));
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->deliveries.size() == 1);
    CHECK(data->deliveries[0].vendorName == "Green Farms Produce");
    CHECK(data->lastDeliveryId == 1);
    CHECK(data->settings.vendors.size() == 3);
}

TEST_CASE("Forecast: a coming day from the same weekday in past weeks; prep and what to order", "[inventory][forecast]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QDate today = QDateTime::fromMSecsSinceEpoch(pos.shared()->now()).date();
    const QDate target = today.addDays(1);
    const core::MenuItem burger = *pos.findItem(u"classic-burger"_s);
    REQUIRE_FALSE(burger.recipe.empty());
    const std::string bun = burger.recipe.front().ingredientId;
    const double perBurger = burger.recipe.front().quantity;
    core::Ingredient *g = pos.shared()->ingredient(bun);
    REQUIRE(g);
    g->onHand = 1;
    g->lowAt = 2;
    g->vendor = pos.shared()->settings.vendors.empty() ? std::string() : pos.shared()->settings.vendors.front().id;

    // Last week: two checks, 4 burgers; the week before: one, 2 burgers;
    // an older one had none (left out). Recent weeks count more (4 : 3).
    std::vector<core::Check> past;
    qint64 id = 9000;
    const auto check = [&](QDate day, int burgers, int hour) {
        core::Check c;
        c.id = ++id;
        c.status = core::CheckStatus::Closed;
        c.guests = 2;
        c.openedAt = QDateTime(day, QTime(hour, 15)).toMSecsSinceEpoch();
        c.closedAt = c.openedAt + 3600 * 1000;
        for (int i = 0; i < burgers; ++i)
            c.addItem(burger);
        past.push_back(c);
    };
    check(target.addDays(-7), 2, 18);
    check(target.addDays(-7), 2, 19);
    check(target.addDays(-14), 2, 18);
    check(target.addDays(-8), 9, 18);    // another weekday: not counted
    pos.shared()->history = [&](std::int64_t, std::int64_t) { return past; };
    // Sam works 6 to 10 PM that day.
    std::string sam;
    for (const core::Employee &e : pos.shared()->employees)
        if (e.name == "Sam")
            sam = e.id;
    pos.shared()->shifts.push_back({777, sam, QDateTime(target, QTime(18, 0)).toMSecsSinceEpoch(),
                                    QDateTime(target, QTime(22, 0)).toMSecsSinceEpoch(), {}});

    REQUIRE(pos.requestForecast(1));
    for (int i = 0; i < 200 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    const QVariantMap report = pos.rangeReport()[u"report"_s].toMap();
    CHECK(report[u"title"_s] == u"Forecast"_s);
    CHECK(report[u"rows"_s].toList().value(0).toMap()[u"cells"_s].toStringList().value(0).contains(u"2 past weeks"_s));
    CHECK(cellsOf(report, u"Checks"_s).value(1) == u"2"_s);               // 2 x 4/7 + 1 x 3/7 = 1.6
    CHECK(cellsOf(report, u"Guests"_s).value(1) == u"3"_s);               // 4 x 4/7 + 2 x 3/7 = 3.1
    CHECK(cellsOf(report, u"Classic Burger"_s).value(1) == u"3"_s);       // 4 x 4/7 + 2 x 3/7 = 3.1
    // Servers: 6 PM needs one (1.6 checks, 4 a server an hour), and Sam is on.
    QStringList six;
    QString in;
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QVariantMap row = v.toMap();
        if (row[u"kind"_s] == u"section"_s)
            in = row[u"cells"_s].toStringList().value(0);
        else if (in == u"Servers"_s && row[u"cells"_s].toStringList().value(0) == u"6:00 PM"_s)
            six = row[u"cells"_s].toStringList();
    }
    REQUIRE(six.size() == 3);
    CHECK(six[1] == u"1 server needed"_s);
    CHECK(six[2] == u"1 scheduled"_s);
    // The bun: 3.1 burgers' worth (rounded up to a tenth), 1 on hand; to
    // order: that plus the low level (2), less what's on hand.
    const double need = std::ceil(22.0 / 7.0 * perBurger * 10) / 10;
    const QStringList prep = cellsOf(report, QString::fromStdString(g->name));
    REQUIRE(prep.size() == 3);
    CHECK(prep[1].startsWith(QString::number(need, 'f', 1).replace(u".0"_s, QString())));
    CHECK(prep[2] == u"have 1"_s);
    bool ordered = false;
    QString section;
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QVariantMap row = v.toMap();
        const QStringList cells = row[u"cells"_s].toStringList();
        if (row[u"kind"_s] == u"section"_s)
            section = cells.value(0);
        else if (section.startsWith(u"To order"_s) && cells.value(0) == QString::fromStdString(g->name))
            ordered = cells.value(1).startsWith(QString::number(std::ceil(need + 2 - 1)));
    }
    CHECK(ordered);

    // Nothing to go on: says so.
    pos.shared()->history = [](std::int64_t, std::int64_t) { return std::vector<core::Check>{}; };
    REQUIRE(pos.requestForecast(3));
    for (int i = 0; i < 200 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    const QVariantList rows = pos.rangeReport()[u"report"_s].toMap()[u"rows"_s].toList();
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].toMap()[u"cells"_s].toStringList().value(0).startsWith(u"Not enough sales yet"_s));
}

TEST_CASE("Stock counts: what's on the shelf against what the sales and deliveries say", "[inventory][stockCount]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        const auto seed = test::seedPosData();
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    storage::AsyncWriter writer(path);
    storage::SqlPosSink sink(writer);
    app::PosData data = test::seedPosData();
    PosService pos(data, &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const double buns = onHand(pos, "bun");          // 48, never counted
    CHECK(pos.shared()->ingredient("bun")->start == buns);

    // In: a delivery of 24. Out: a burger combo (one bun). By hand: +2.
    REQUIRE(pos.receiveDelivery({{u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"qty"_s, 24}}}}}));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"burger-combo"_s);
    REQUIRE(pos.chooseOption(u"temperature"_s, 2));
    REQUIRE(pos.chooseOption(u"side"_s, 0));
    REQUIRE(pos.chooseOption(u"combo-drink"_s, 0));
    REQUIRE(pos.finishChoosing());
    REQUIRE(pos.sendOrder());
    setOnHand(pos, "bun", onHand(pos, "bun") + 2);
    const core::Ingredient &bun = *pos.shared()->ingredient("bun");
    CHECK(bun.received == 24);
    CHECK(bun.sold == 1);
    CHECK(bun.adjusted == 2);
    CHECK(bun.onHand == buns + 24 - 1 + 2);   // 73 should be there

    // While counting, what should be there isn't shown (unless the store says so).
    QVariantMap counting = pos.stockCounting();
    REQUIRE(counting[u"ingredients"_s].toList().size() == int(pos.shared()->ingredients.size()));
    CHECK_FALSE(counting[u"ingredients"_s].toList()[0].toMap().contains(u"expected"_s));
    CHECK(counting[u"ingredients"_s].toList()[0].toMap()[u"lastCounted"_s] == u"never"_s);

    // Nothing typed, or not a number: not saved.
    CHECK_FALSE(pos.saveStockCount({{u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"counted"_s, u""_s}}}}}));
    CHECK_FALSE(pos.saveStockCount({{u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"counted"_s, u"-3"_s}}}}}));

    // 69 buns on the shelf: 4 missing. Patties not counted: left alone.
    const double patties = onHand(pos, "patty");
    REQUIRE(pos.saveStockCount({{u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"bun"_s}, {u"counted"_s, u"69"_s}},
                                                         QVariantMap{{u"ingredient"_s, u"patty"_s}, {u"counted"_s, QString()}}}}}));
    CHECK(onHand(pos, "bun") == 69);
    CHECK(onHand(pos, "patty") == patties);
    CHECK(bun.start == 69);
    CHECK(bun.received == 0);
    CHECK(bun.sold == 0);
    CHECK(bun.adjusted == 0);
    CHECK(bun.countedAt > 0);
    REQUIRE(pos.stockCounts().size() == 1);
    const core::StockCount &count = pos.stockCounts().back();
    REQUIRE(count.lines.size() == 1);
    CHECK(count.lines[0].expected == buns + 25);
    CHECK(count.lines[0].off() == -4);
    CHECK(count.value() == Money::fromCents(-140));   // 4 x $0.35
    CHECK(app::stockCountFromJson(app::toJson(count)) == count);
    CHECK(app::ingredientFromJson(app::toJson(bun)) == bun);

    const core::Report r = pos.buildReport(u"stock"_s);
    CHECK(rowIn(r, r.rows.front().cells.front(), "Burger Buns (each)")
          == std::vector<std::string>{"Burger Buns (each)", "73", "69", "-4", "-$1.40"});
    CHECK(pos.stockCounting()[u"recent"_s].toList().value(0).toMap()[u"short"_s].toBool());

    // The store can show what should be there while counting.
    pos.shared()->settings.stockCountShowsExpected = true;
    CHECK(pos.stockCounting()[u"ingredients"_s].toList()[0].toMap()[u"expected"_s] == u"69"_s);

    // Kept in the database.
    QTest::qWait(50);
    writer.flush();
    const auto loaded = store.load();
    REQUIRE(loaded);
    REQUIRE(loaded->stockCounts.size() == 1);
    CHECK(loaded->stockCounts.back() == count);
    CHECK(loaded->lastStockCountId == 1);
}

TEST_CASE("Waste: logged by item or ingredient, off the shelf, told apart in the Stock report; par and the order list", "[inventory][waste]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));   // anyone working can log waste
    const double buns = onHand(pos, "bun"), lettuce = onHand(pos, "lettuce");

    CHECK_FALSE(pos.logWaste({{u"item"_s, u"classic-burger"_s}, {u"qty"_s, u"1"_s}}));                    // why?
    CHECK_FALSE(pos.logWaste({{u"item"_s, u"classic-burger"_s}, {u"qty"_s, u"0"_s}, {u"reason"_s, u"Made wrong"_s}}));
    CHECK_FALSE(pos.logWaste({{u"item"_s, u"no-such-item"_s}, {u"qty"_s, u"1"_s}, {u"reason"_s, u"Made wrong"_s}}));
    // A dropped burger: its bun, patty and lettuce. Lettuce gone bad: 8 oz.
    REQUIRE(pos.logWaste({{u"item"_s, u"classic-burger"_s}, {u"qty"_s, u"1"_s}, {u"reason"_s, u"Dropped or spilled"_s}}));
    REQUIRE(pos.logWaste({{u"ingredient"_s, u"lettuce"_s}, {u"qty"_s, u"8"_s}, {u"reason"_s, u"Expired or spoiled"_s}}));
    CHECK(onHand(pos, "bun") == buns - 1);
    CHECK(onHand(pos, "lettuce") == lettuce - 8.5);
    CHECK(pos.shared()->ingredient("lettuce")->wasted == 8.5);
    REQUIRE(pos.wasteEntries().size() == 2);
    const core::Waste &burger = pos.wasteEntries().front();
    CHECK(burger.what == "Classic Burger");
    CHECK(burger.by == "Sam");
    CHECK(burger.lines.size() == 3);
    CHECK(burger.value() == Money::fromCents(35 + 160 + 6));   // bun, patty, half an oz of lettuce
    CHECK(app::wasteFromJson(app::toJson(burger)) == burger);
    const QVariantList today = pos.wasteLog()[u"today"_s].toList();
    REQUIRE(today.size() == 2);
    CHECK(today[0].toMap()[u"what"_s].toString().toStdString() == pos.shared()->ingredient("lettuce")->name);   // newest first

    // Counted: lettuce right where it should be after the waste; the waste is known, not missing.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveStockCount({{u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"lettuce"_s},
                                                                      {u"counted"_s, QString::number(lettuce - 8.5)}}}}}));
    const core::StockCount &count = pos.stockCounts().back();
    CHECK(count.lines[0].wasted == 8.5);
    CHECK(count.lines[0].off() == 0);
    CHECK(count.value().cents() == 0);
    CHECK(count.wasteValue() == Money::fromCents(102));   // 8.5 oz x $0.12
    CHECK(pos.shared()->ingredient("lettuce")->wasted == 0);

    // Par: buns kept at 100; 47 on hand -> order 53 from City Bakery.
    const int bun = indexOf(pos, "bun");
    QVariantMap r = pos.adminRecords(u"inventory"_s)[bun].toMap();
    r[u"par"_s] = 100;
    REQUIRE(pos.adminSave(u"inventory"_s, bun, r));
    CHECK(pos.shared()->ingredient("bun")->par == 100);
    CHECK(pos.shared()->ingredient("bun")->wasted == 1);   // kept through the edit
    CHECK(app::ingredientFromJson(app::toJson(*pos.shared()->ingredient("bun"))).par == 100);

    const core::Report rep = pos.buildReport(u"stock"_s);
    CHECK(!rowIn(rep, "Thrown away today", "All").empty());
    const auto order = rowIn(rep, "To order: City Bakery (on hand, par, order)", "Burger Buns (each)");
    CHECK(order == std::vector<std::string>{"Burger Buns (each)", "47", "100", "53", "$18.55"});
    bool known = false;
    for (const core::ReportRow &row : rep.rows)
        known = known || (!row.cells.empty() && row.cells.front() == "Thrown away (logged)" && row.cells.back() == "-$1.02");
    CHECK(known);
}

#include "print/labels.hh"

namespace {

struct LabelPrinter : app::PosPrinter {
    std::vector<std::pair<std::string, std::vector<std::string>>> labels;
    std::string printer;
    void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
    void printReceipt(const core::PosSettings &, const core::Check &, const std::string &) override {}
    void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
    void openDrawer(const core::PosSettings &, const std::string &) override {}
    void printLabels(const core::PosSettings &, const std::vector<std::pair<std::string, std::vector<std::string>>> &l,
                     const std::string &p) override
    {
        labels = l;
        printer = p;
    }
};

} // namespace

TEST_CASE("Labels: ZPL and TSPL for label printers, a slip for a receipt printer", "[labels]")
{
    const std::vector<print::Label> labels{{"Pico de gallo", {"Made Mon 10/12 9:00 AM", "Use by Wed 10/14 9:00 AM", "Sam"}},
                                           {"Café ^~ \"mix\"", {"Made Mon 10/12 9:00 AM"}}};
    const std::string zpl = print::renderZpl(labels, 51, 25);
    CHECK(std::ranges::count(zpl, '\n') == 2);                       // one ^XA ... ^XZ a label
    CHECK(zpl.find("^PW408^LL200") != std::string::npos);            // 51 x 25 mm at 8 dots a mm
    CHECK(zpl.find("^FDPico de gallo^FS") != std::string::npos);
    CHECK(zpl.find("^FDUse by Wed 10/14 9:00 AM^FS") != std::string::npos);
    CHECK(zpl.find("^FDCafe    \"mix\"^FS") != std::string::npos);   // plain letters, no command characters
    const std::string tspl = print::renderTspl(labels, 51, 25);
    CHECK(tspl.starts_with("SIZE 51 mm,25 mm\r\n"));
    CHECK(tspl.find("\"Pico de gallo\"\r\n") != std::string::npos);
    CHECK(tspl.find("\"Cafe ^~  mix \"\r\n") != std::string::npos);  // no quotes inside the text
    std::size_t prints = 0;
    for (std::size_t at = tspl.find("PRINT 1,1"); at != std::string::npos; at = tspl.find("PRINT 1,1", at + 1))
        ++prints;
    CHECK(prints == 2);
    const print::Document slip = print::labelSlip(labels[0]);
    CHECK(print::renderText(slip, 32).find("Pico de gallo") != std::string::npos);
}

TEST_CASE("Prep labels: what, made and use by, on the label printer", "[labels]")
{
    app::PosData data = test::seedPosData();
    PosService pos(data, nullptr);
    LabelPrinter printer;
    pos.setPrinter(&printer);
    // No label printer yet.
    CHECK(pos.prepLabels()[u"printer"_s].toString().isEmpty());
    CHECK_FALSE(pos.printPrepLabels({{u"ingredient"_s, u"lettuce"_s}, {u"hours"_s, 24}}));

    // A ZPL printer by the prep table: the store's label printer by itself.
    core::PrinterConfig zebra;
    zebra.id = "prep";
    zebra.name = "Prep Labels";
    zebra.type = "network";
    zebra.host = "10.0.0.9";
    zebra.format = "zpl";
    pos.shared()->settings.printers.push_back(zebra);
    CHECK(pos.prepLabels()[u"printer"_s] == u"Prep Labels"_s);

    // Nobody logged in (a kitchen screen): signed by the screen.
    const std::int64_t made = QDateTime(QDate(2026, 10, 12), QTime(9, 0)).toMSecsSinceEpoch();
    pos.shared()->setClock([&] { return made; });
    REQUIRE(pos.printPrepLabels({{u"ingredient"_s, u"lettuce"_s}, {u"hours"_s, 48}, {u"count"_s, 3}, {u"note"_s, u"Batch 2"_s}}));
    REQUIRE(printer.labels.size() == 3);
    CHECK(printer.printer == "prep");
    CHECK(printer.labels[0].first == pos.shared()->ingredient("lettuce")->name);
    CHECK(QString::fromStdString(printer.labels[0].second[0]).startsWith(u"Made Mon 10/12 9:00"_s));
    CHECK(QString::fromStdString(printer.labels[0].second[1]).startsWith(u"Use by Wed 10/14 9:00"_s));
    CHECK(QString::fromStdString(printer.labels[0].second[2]).startsWith(u"Batch 2"_s));

    // Something typed, no use-by; too many at once.
    REQUIRE(pos.printPrepLabels({{u"name"_s, u"Pico de gallo"_s}, {u"hours"_s, 0}}));
    CHECK(printer.labels[0].first == "Pico de gallo");
    CHECK(printer.labels[0].second.size() == 2);
    CHECK_FALSE(pos.printPrepLabels({{u"name"_s, u"Salsa"_s}, {u"count"_s, 51}}));
    CHECK_FALSE(pos.printPrepLabels({{u"hours"_s, 24}}));   // what is it?

    // The store's choice, and a screen's own.
    pos.shared()->settings.labelPrinter = "receipt";
    CHECK(pos.labelPrinterId() == "receipt");
    core::TerminalConfig here;
    here.name = pos.terminalName().toStdString();
    here.labelPrinter = "prep";
    pos.shared()->settings.terminals.push_back(here);
    CHECK(pos.labelPrinterId() == "prep");

    // Shelf life kept on the ingredient (Inventory).
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int i = indexOf(pos, "lettuce");
    QVariantMap r = pos.adminRecords(u"inventory"_s)[i].toMap();
    r[u"shelfLifeHours"_s] = 72;
    REQUIRE(pos.adminSave(u"inventory"_s, i, r));
    CHECK(pos.prepLabels()[u"ingredients"_s].toList()[i].toMap()[u"hours"_s] == 72);
    CHECK(app::ingredientFromJson(app::toJson(*pos.shared()->ingredient("lettuce"))).shelfLifeHours == 72);

    // A printer that's a label printer: its size kept.
    QVariantList printers = pos.adminRecords(u"printers"_s);
    QVariantMap pr = printers.back().toMap();
    CHECK(pr[u"format"_s] == u"zpl"_s);
    pr[u"labelWidthMm"_s] = 57;
    pr[u"labelHeightMm"_s] = 32;
    REQUIRE(pos.adminSave(u"printers"_s, int(printers.size()) - 1, pr));
    CHECK(pos.shared()->settings.printers.back().labelWidthMm == 57);
    pr[u"format"_s] = u"pdf"_s;
    CHECK_FALSE(pos.adminSave(u"printers"_s, int(printers.size()) - 1, pr));
}

TEST_CASE("Price watch: a delivery at higher prices names the items over the food cost target, with a price", "[inventory][pricewatch]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.shared()->settings.targetFoodCostPercent = 30;
    const QVariantList before = pos.overFoodCost();
    QStringList said;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { said << n; });
    // Patties at $9.00 each: every burger goes over 30%.
    REQUIRE(pos.receiveDelivery({{u"vendor"_s, u"valley-foods"_s},
                                 {u"lines"_s, QVariantList{QVariantMap{{u"ingredient"_s, u"patty"_s}, {u"qty"_s, 10}, {u"cost"_s, 9.0}}}}}));
    const QVariantList over = pos.overFoodCost();
    QVariantMap burger;
    for (const QVariant &v : over)
        if (v.toMap()[u"name"_s] == u"Classic Burger"_s)
            burger = v.toMap();
    REQUIRE_FALSE(burger.isEmpty());
    CHECK(burger[u"percent"_s].toInt() > 30);
    // The suggested price brings it back to 30% (rounded up to a quarter).
    const core::MenuItem *m = pos.findItem(u"classic-burger"_s);
    const double cost = double(pos.recipeCost(*m).cents());
    const double suggested = burger[u"suggested"_s].toString().remove(u'$').toDouble() * 100;
    CHECK(cost / suggested <= 0.30 + 1e-9);
    CHECK(cost / (suggested - 25) > 0.30);
    bool named = false;
    for (const QString &n : said)
        named = named || (n.contains(u"over 30%"_s) && n.contains(u"Classic Burger"_s));
    CHECK(named);
    // And Reports -> Food Cost lists it.
    bool listed = false;
    QString in;
    for (const QVariant &v : pos.report(u"foodcost"_s)[u"rows"_s].toList()) {
        const QVariantMap row = v.toMap();
        if (row[u"kind"_s] == u"section"_s)
            in = row[u"cells"_s].toStringList().value(0);
        else if (in == u"Prices to look at"_s && row[u"cells"_s].toStringList().value(0) == u"Classic Burger"_s)
            listed = row[u"cells"_s].toStringList().value(2).contains(u"→"_s);
    }
    CHECK(listed);
    CHECK(before.size() < over.size());
}
