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

    REQUIRE(pos.requestForecast(1));
    for (int i = 0; i < 200 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    const QVariantMap report = pos.rangeReport()[u"report"_s].toMap();
    CHECK(report[u"title"_s] == u"Forecast"_s);
    CHECK(report[u"rows"_s].toList().value(0).toMap()[u"cells"_s].toStringList().value(0).contains(u"2 past weeks"_s));
    CHECK(cellsOf(report, u"Checks"_s).value(1) == u"2"_s);               // 2 x 4/7 + 1 x 3/7 = 1.6
    CHECK(cellsOf(report, u"Guests"_s).value(1) == u"3"_s);               // 4 x 4/7 + 2 x 3/7 = 3.1
    CHECK(cellsOf(report, u"Classic Burger"_s).value(1) == u"3"_s);       // 4 x 4/7 + 2 x 3/7 = 3.1
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
