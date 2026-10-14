#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "app/pos_json.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The menu while ordering: modifier groups, meal-period prices, 86.

namespace {

QVariantMap groupOf(const PosService &pos, const QString &id)
{
    for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList()) {
        if (g.toMap()[u"id"_s] == id)
            return g.toMap();
    }
    return {};
}

std::int64_t todayAt(int hour)
{
    return QDateTime(QDate::currentDate(), QTime(hour, 0)).toMSecsSinceEpoch();
}

} // namespace

TEST_CASE("Modifier groups: choose one, up to N, required before Done", "[menu][modifiers]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.addItem(u"house-salad"_s));                // $8.50, dressing + protein
    QVariantMap choosing = pos.choosingInfo();
    REQUIRE(choosing[u"active"_s].toBool());
    CHECK(choosing[u"item"_s] == u"House Salad"_s);
    CHECK(groupOf(pos, u"dressing"_s)[u"rule"_s] == u"Choose 1"_s);
    CHECK_FALSE(groupOf(pos, u"dressing"_s)[u"done"_s].toBool());
    CHECK(groupOf(pos, u"salad-protein"_s)[u"done"_s].toBool());   // optional

    CHECK_FALSE(pos.finishChoosing());                    // dressing first
    REQUIRE(pos.chooseOption(u"dressing"_s, 0));           // Ranch
    REQUIRE(pos.chooseOption(u"dressing"_s, 1));           // Blue Cheese replaces it
    QVariantList mods = pos.lines().first().toMap()[u"modifiers"_s].toList();
    REQUIRE(mods.size() == 1);
    CHECK(mods.first().toMap()[u"name"_s] == u"Blue Cheese"_s);
    REQUIRE(pos.chooseOption(u"salad-protein"_s, 2));      // Salmon + 6.00
    CHECK(pos.lines().first().toMap()[u"price"_s] == u"$14.50"_s);
    REQUIRE(pos.chooseOption(u"salad-protein"_s, 2));      // touched again: off
    CHECK(pos.lines().first().toMap()[u"price"_s] == u"$8.50"_s);
    REQUIRE(pos.finishChoosing());
    CHECK_FALSE(pos.choosingInfo()[u"active"_s].toBool());

    // Up to three fillings.
    REQUIRE(pos.addItem(u"omelette"_s));
    for (int i : {0, 1, 2})
        REQUIRE(pos.chooseOption(u"omelette-fillings"_s, i));
    CHECK_FALSE(pos.chooseOption(u"omelette-fillings"_s, 3));
    REQUIRE(pos.chooseOption(u"omelette-fillings"_s, 1));  // Ham off...
    REQUIRE(pos.chooseOption(u"omelette-fillings"_s, 3));  // ...Peppers on
    CHECK(groupOf(pos, u"omelette-fillings"_s)[u"chosen"_s] == 3);
    CHECK_FALSE(pos.finishChoosing());                    // toast is required too
    REQUIRE(pos.chooseOption(u"toast"_s, 2));              // Sourdough
    REQUIRE(pos.finishChoosing());

    // Cancel Item takes it off.
    REQUIRE(pos.addItem(u"two-eggs"_s));
    CHECK(pos.lines().size() == 3);
    REQUIRE(pos.cancelChoosing());
    CHECK(pos.lines().size() == 2);
    CHECK_FALSE(pos.choosingInfo()[u"active"_s].toBool());

    // Items without groups don't ask.
    REQUIRE(pos.addItem(u"water"_s));
    CHECK_FALSE(pos.choosingInfo()[u"active"_s].toBool());

    // The choices are saved with the check.
    const core::Check &c = pos.shared()->open.begin()->second;
    CHECK(c.lines[0].modifiers[0].group == "dressing");
    CHECK(app::checkFromJson(app::toJson(c))->lines[0].modifiers[0].group == "dressing");
}

TEST_CASE("Ordering an item with choices opens the Choose page", "[menu][modifiers][ui]")
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    PosService pos(test::seedPosData(true), nullptr);
    LayoutController c(*l);
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(c.jumpTo(u"items-salads"_s));
    c.activate(u"item-1"_s);                                // House Salad
    CHECK(c.pageId() == u"modifiers"_s);
    REQUIRE(pos.chooseOption(u"dressing"_s, 0));
    c.finishChoosing();
    CHECK(c.pageId() == u"items-salads"_s);
}

TEST_CASE("Prices change with the meal period", "[menu][prices]")
{
    app::PosData data = test::seedPosData();
    for (core::MenuItem &m : data.menu) {
        if (m.id == "classic-burger")
            m.periodPrices["dinner"] = Money::fromCents(1250);
    }
    PosService pos(data, nullptr);
    std::int64_t clock = todayAt(12);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.addItem(u"classic-burger"_s));
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$11.50"_s);   // lunch
    clock = todayAt(19);
    REQUIRE(pos.addItem(u"classic-burger"_s));
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$12.50"_s);   // dinner
    CHECK(pos.lines().first().toMap()[u"price"_s] == u"$11.50"_s);  // already ordered: unchanged
}

TEST_CASE("Sold out (86): can't be ordered, marked on its buttons", "[menu][86]")
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    PosService pos(test::seedPosData(true), nullptr);
    LayoutController c(*l);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1111"_s));                 // servers may 86
    REQUIRE(pos.setAvailable(u"cobb"_s, false));
    CHECK(pos.soldOut().contains(u"cobb"_s));
    CHECK(pos.soldOut().contains(u"cobb"_s));
    CHECK_FALSE(pos.addItem(u"cobb"_s));
    CHECK_FALSE(pos.menuItems().isEmpty());

    REQUIRE(c.jumpTo(u"items-salads"_s));
    ZoneModel *m = c.zones();
    bool marked = false, othersFine = true;
    for (int r = 0; r < m->rowCount(); ++r) {
        const QModelIndex i = m->index(r);
        if (m->data(i, ZoneModel::LabelRole) == u"Cobb"_s)
            marked = m->data(i, ZoneModel::SoldOutRole).toBool() && !m->data(i, ZoneModel::ZoneEnabledRole).toBool();
        else if (m->data(i, ZoneModel::SoldOutRole).toBool())
            othersFine = false;
    }
    CHECK(marked);
    CHECK(othersFine);

    REQUIRE(pos.setAvailable(u"cobb"_s, true));
    CHECK(pos.addItem(u"cobb"_s));
    CHECK_FALSE(pos.setAvailable(u"nothing"_s, false));
}

TEST_CASE("Manager screens: modifier groups and an item's groups and prices", "[menu][admin]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap g = pos.adminNewRecord(u"modifierGroups"_s);
    g[u"name"_s] = u"Size"_s;
    g[u"options"_s] = u"Small\nLarge + 1.50\n"_s;
    REQUIRE(pos.adminSave(u"modifierGroups"_s, -1, g));
    const core::ModifierGroup *size = pos.shared()->settings.modifierGroup("size");
    REQUIRE(size);
    REQUIRE(size->options.size() == 2);
    CHECK(size->options[1].name == "Large");
    CHECK(size->options[1].price.cents() == 150);
    g[u"options"_s] = u"Large + lots"_s;
    CHECK_FALSE(pos.adminSave(u"modifierGroups"_s, -1, g));
    g[u"options"_s] = u""_s;
    CHECK_FALSE(pos.adminSave(u"modifierGroups"_s, -1, g));

    // The coffee: a size, and an evening price.
    const QVariantList menu = pos.adminRecords(u"menu"_s);
    int coffee = -1;
    for (int i = 0; i < menu.size(); ++i) {
        if (menu[i].toMap()[u"id"_s] == u"coffee"_s)
            coffee = i;
    }
    REQUIRE(coffee >= 0);
    QVariantMap item = menu[coffee].toMap();
    item[u"modifierGroups"_s] = u"nope"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, coffee, item));
    item[u"modifierGroups"_s] = u"size"_s;
    item[u"periodPrices"_s] = u"dinner 3"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, coffee, item));
    item[u"periodPrices"_s] = u"dinner = 3.25"_s;
    REQUIRE(pos.adminSave(u"menu"_s, coffee, item));
    const core::MenuItem &saved = pos.shared()->menu[coffee];
    CHECK(saved.modifierGroups == std::vector<std::string>{"size"});
    CHECK(saved.periodPrices.at("dinner").cents() == 325);
    const QVariantMap back = pos.adminRecords(u"menu"_s)[coffee].toMap();
    CHECK(back[u"modifierGroups"_s] == u"size"_s);
    CHECK(back[u"periodPrices"_s] == u"dinner = 3.25"_s);

    // Saved with the settings and the menu.
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).modifierGroups
          == pos.shared()->settings.modifierGroups);
    CHECK(app::menuItemFromJson(app::toJson(saved)) == saved);
}

TEST_CASE("Demo menu: every group an item names exists and makes sense", "[menu][modifiers][seed]")
{
    const app::PosData data = test::seedPosData(true);
    CHECK(data.settings.modifierGroups.size() >= 15);
    int itemsWithGroups = 0;
    for (const core::MenuItem &m : data.menu) {
        if (!m.modifierGroups.empty())
            ++itemsWithGroups;
        for (const std::string &g : m.modifierGroups) {
            INFO(m.id << " -> " << g);
            CHECK(data.settings.modifierGroup(g) != nullptr);
        }
    }
    CHECK(itemsWithGroups >= 16);
    for (const core::ModifierGroup &g : data.settings.modifierGroups) {
        INFO(g.id);
        CHECK_FALSE(g.options.empty());
        CHECK(g.min <= int(g.options.size()));
        CHECK((g.max == 0 || g.min <= g.max));
    }
}

TEST_CASE("Demo drinks and breakfast: sizes, bottles, any number of add-ons", "[menu][modifiers][seed]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));

    REQUIRE(pos.addItem(u"soda"_s));                        // $2.95
    REQUIRE(pos.chooseOption(u"drink-size"_s, 2));          // Large + 1.00
    REQUIRE(pos.finishChoosing());
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$3.95"_s);

    REQUIRE(pos.addItem(u"house-wine"_s));                  // $8.00
    CHECK_FALSE(pos.finishChoosing());                      // which wine, and how much
    REQUIRE(pos.chooseOption(u"wine"_s, 0));
    REQUIRE(pos.chooseOption(u"wine-pour"_s, 1));           // Bottle + 22.00
    REQUIRE(pos.finishChoosing());
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$30.00"_s);

    REQUIRE(pos.addItem(u"pancakes"_s));                    // $9.50
    REQUIRE(pos.chooseOption(u"syrup"_s, 0));
    for (int i = 0; i < 5; ++i)                             // add-ons: any number
        REQUIRE(pos.chooseOption(u"breakfast-add-ons"_s, i));
    REQUIRE(pos.finishChoosing());
    CHECK(pos.lines().last().toMap()[u"modifiers"_s].toList().size() == 6);
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$21.50"_s);   // 9.50 + 3 + 3 + 2.50 + 2.75 + 0.75

    REQUIRE(pos.addItem(u"kids-burger"_s));                 // $7.50
    REQUIRE(pos.chooseOption(u"kids-side"_s, 1));           // Apple Slices
    REQUIRE(pos.chooseOption(u"kids-drink"_s, 2));          // Chocolate Milk + 0.50
    REQUIRE(pos.finishChoosing());
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$8.00"_s);
}

TEST_CASE("Send, Fire and Close wait for required choices", "[menu][modifiers]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.selectTable(u"T7"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    REQUIRE(pos.addItem(u"tea"_s));                         // hot or iced?
    pos.addItem(u"water"_s);                                // left the Choose page without choosing
    const qint64 tea = pos.lines().first().toMap()[u"id"_s].toLongLong();
    CHECK(pos.lines().first().toMap()[u"needsChoice"_s].toBool());
    CHECK(pos.lines().first().toMap()[u"choices"_s].toBool());
    CHECK_FALSE(pos.lines().last().toMap()[u"choices"_s].toBool());

    CHECK_FALSE(pos.sendOrder());
    REQUIRE(pos.tender(u"cash"_s));
    CHECK_FALSE(pos.closeCheck());

    // Back to its choices from the check.
    REQUIRE(pos.chooseLine(tea));
    CHECK(pos.choosingInfo()[u"item"_s] == u"Tea"_s);
    REQUIRE(pos.chooseOption(u"hot-or-iced"_s, 1));
    REQUIRE(pos.finishChoosing());
    CHECK_FALSE(pos.lines().first().toMap()[u"needsChoice"_s].toBool());
    REQUIRE(pos.sendOrder());
    CHECK_FALSE(pos.chooseLine(tea));                       // sent: no more changes

    // A held course is checked when it is fired.
    REQUIRE(pos.setCourse(2));
    REQUIRE(pos.addItem(u"lemonade"_s));
    REQUIRE(pos.sendOrder() == false);                      // course 2 is on hold anyway
    CHECK_FALSE(pos.fireCourse());                          // lemonade needs a size
    REQUIRE(pos.chooseLine(pos.lines().last().toMap()[u"id"_s].toLongLong()));
    REQUIRE(pos.chooseOption(u"drink-size"_s, 0));
    REQUIRE(pos.finishChoosing());
    REQUIRE(pos.fireCourse());
    REQUIRE(pos.tender(u"cash"_s));                         // the lemonade
    REQUIRE(pos.closeCheck());
}

TEST_CASE("Prices by order type: takeout and delivery prices", "[menu][prices]")
{
    app::PosData data = test::seedPosData();
    for (core::MenuItem &m : data.menu) {
        if (m.id == "cobb") {                       // $12.50 here
            m.takeoutPrice = Money::fromCents(1350);
            m.deliveryPrice = Money::fromCents(1450);
        }
        if (m.id == "caesar")                       // $9.75 here
            m.takeoutPrice = Money::fromCents(1050);
    }
    app::PosService pos(data, nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    const auto priceOf = [&](core::CheckType type, const char *item) {
        REQUIRE(pos.startCheck(type));
        pos.addItem(QString::fromLatin1(item));
        pos.finishChoosing();
        const QString price = pos.lines().last().toMap()[u"price"_s].toString();
        pos.voidItem();
        pos.releaseCheck();
        return price;
    };
    CHECK(priceOf(core::CheckType::Quick, "cobb") == u"$12.50"_s);
    CHECK(priceOf(core::CheckType::Takeout, "cobb") == u"$13.50"_s);
    CHECK(priceOf(core::CheckType::Delivery, "cobb") == u"$14.50"_s);
    CHECK(priceOf(core::CheckType::Delivery, "caesar") == u"$10.50"_s);   // no delivery price: the takeout one
    CHECK(priceOf(core::CheckType::Takeout, "soda") == u"$2.95"_s);       // none set: the regular price

    // Manager -> Menu: the fields, saved with the item.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    int cobb = -1;
    const QVariantList items = pos.adminRecords(u"menu"_s);
    for (int i = 0; i < items.size(); ++i)
        if (items[i].toMap()[u"id"_s] == u"cobb"_s)
            cobb = i;
    REQUIRE(cobb >= 0);
    QVariantMap r = items[cobb].toMap();
    CHECK(r[u"takeoutPrice"_s].toDouble() == 13.5);
    r[u"deliveryPrice"_s] = 15.0;
    r[u"noDiscount"_s] = true;
    REQUIRE(pos.adminSave(u"menu"_s, cobb, r));
    const core::MenuItem *m = nullptr;
    for (const core::MenuItem &x : pos.shared()->menu)
        if (x.id == "cobb")
            m = &x;
    REQUIRE(m);
    CHECK(m->deliveryPrice.cents() == 1500);
    CHECK(m->noDiscount);
    CHECK(app::menuItemFromJson(app::toJson(*m)) == *m);
}

TEST_CASE("Staff meals, and items discounts leave out", "[menu][prices][discounts]")
{
    app::PosData data = test::seedPosData();
    data.settings.tax = {};                         // no tax: easy totals
    for (core::MenuItem &m : data.menu)
        if (m.id == "caesar")
            m.noDiscount = true;                    // never discounted
    app::PosService pos(data, nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));           // Sam's meal

    // A cobb ($12.50) and a beer ($6.00, no staff discount): half off the cobb.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"cobb"_s);
    pos.finishChoosing();
    pos.addItem(u"draft-beer"_s);
    pos.finishChoosing();
    // A server's staff meal waits for a manager's PIN; it's still Sam's meal.
    CHECK_FALSE(pos.tender(u"staff-meal"_s));
    pos.invoke(u"tender"_s, {u"staff-meal"_s});
    REQUIRE(pos.approvalInfo()[u"needed"_s].toBool());
    REQUIRE(pos.approve(u"1234"_s));
    QVariantMap t = pos.totals();
    CHECK(t[u"total"_s] == u"$12.25"_s);           // 18.50 - 6.25
    const core::Check *c = nullptr;
    for (const auto &[id, x] : pos.shared()->open)
        c = &x;
    REQUIRE(c);
    CHECK(c->payments.back().staffMeal);
    CHECK(c->payments.back().reference == "Sam");
    CHECK(c->totals(pos.shared()->settings.tax).staffMeals.cents() == 625);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // "No discounts": 10% off leaves the caesar out.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"cobb"_s);
    pos.finishChoosing();
    pos.addItem(u"caesar"_s);
    pos.finishChoosing();
    pos.invoke(u"tender"_s, {u"discount"_s});       // 10% of the cobb only (a manager's PIN)
    REQUIRE(pos.giveReason(u"Guest unhappy"_s));
    REQUIRE(pos.approve(u"1234"_s));
    t = pos.totals();
    CHECK(t[u"discounts"_s] == u"-$1.25"_s);

    // The Sales report counts staff meals.
    bool line = false;
    for (const core::ReportRow &row : pos.buildReport(u"sales"_s).rows)
        line = line || (row.cells.size() == 2 && row.cells[0] == "  of which staff meals (1)" && row.cells[1] == "-$6.25");
    CHECK(line);
}

TEST_CASE("Extra costs what the store says", "[menu][prices][extra]")
{
    app::PosData data = test::seedPosData();
    data.settings.extraPercent = 50;
    app::PosService pos(data, nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"classic-burger"_s);                 // $11.50
    pos.finishChoosing();
    pos.setQualifier(u"extra"_s);
    pos.addItem(u"onion-rings"_s);                    // a $1.00 modifier: Extra = $1.50
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$13.00"_s);
    pos.setQualifier(u"extra"_s);
    pos.addItem(u"cobb"_s);                           // Extra on an item: $12.50 + 50%
    pos.finishChoosing();
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$18.75"_s);
    pos.addItem(u"cobb"_s);                           // without Extra: as usual
    pos.finishChoosing();
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$12.50"_s);

    // A fixed charge: even a free choice costs it.
    pos.shared()->settings.extraPercent = 0;
    pos.shared()->settings.extraCharge = Money::fromCents(75);
    pos.addItem(u"classic-burger"_s);
    pos.finishChoosing();
    pos.setQualifier(u"extra"_s);
    pos.addItem(u"medium"_s);                         // $0 + $0.75
    CHECK(pos.lines().last().toMap()[u"price"_s] == u"$12.25"_s);
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).extraCharge.cents() == 75);
}

TEST_CASE("Sold by weight: the price per pound times the weight, on the check, ticket and stock", "[menu][weight]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));

    // Touching it asks for the weight first: nothing is added yet.
    REQUIRE(pos.addItem(u"smoked-brisket"_s));
    CHECK(pos.weighingInfo()[u"active"_s].toBool());
    CHECK(pos.weighingInfo()[u"unit"_s] == u"lb"_s);
    CHECK(pos.lines().isEmpty());
    CHECK_FALSE(pos.addWeighed());                       // no weight typed
    for (const char ch : {'1', '3', '7'})
        pos.entryKey(QString(QChar(ch)));                 // 1.37 lb
    REQUIRE(pos.addWeighed());
    CHECK_FALSE(pos.weighingInfo()[u"active"_s].toBool());
    REQUIRE(pos.lines().size() == 1);
    const core::OrderLine &l = pos.shared()->open.begin()->second.lines.front();
    CHECK(l.weight == 1370);
    CHECK(l.displayName() == "Smoked Brisket 1.37 lb");
    CHECK(l.total() == Money::fromCents(3014));          // 22.00 x 1.37 = 30.14
    CHECK(pos.entry().isEmpty());

    // Typed before touching it: added at once. Cancel leaves nothing behind.
    for (const char ch : {'5', '0'})
        pos.entryKey(QString(QChar(ch)));
    REQUIRE(pos.addItem(u"smoked-brisket"_s));
    CHECK(pos.lines().size() == 2);
    CHECK(pos.shared()->open.begin()->second.lines.back().total() == Money::fromCents(1100));   // 0.5 lb
    REQUIRE(pos.addItem(u"smoked-brisket"_s));
    REQUIRE(pos.cancelWeighing());
    CHECK(pos.lines().size() == 2);

    // Saved with the check; never on the kiosk.
    const auto back = app::checkFromJson(app::toJson(pos.shared()->open.begin()->second));
    CHECK(back->lines.front().weight == 1370);
    CHECK(back->lines.front().weightUnit == "lb");
    CHECK(app::menuItemFromJson(app::toJson(*pos.findItem(u"smoked-brisket"_s))).byWeight);
    bool onKiosk = false;
    for (const QVariant &v : pos.kioskMenu()[u"items"_s].toList())
        onKiosk = onKiosk || v.toMap()[u"id"_s] == u"smoked-brisket"_s;
    CHECK_FALSE(onKiosk);
}

TEST_CASE("Substitutes: Sub puts an item in place of part of the one before it, at its substitute price",
          "[menu][substitute]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"classic-burger"_s);
    REQUIRE(pos.chooseOption(u"temperature"_s, 2));
    REQUIRE(pos.chooseOption(u"side"_s, 4));           // No Side
    REQUIRE(pos.finishChoosing());

    pos.setQualifier(u"sub"_s);
    REQUIRE(pos.addItem(u"house-salad"_s));            // + $3.00, not $8.50
    const core::Check &c = pos.shared()->open.begin()->second;
    REQUIRE(c.lines.size() == 1);
    const core::Modifier &m = c.lines[0].modifiers.back();
    CHECK(m.displayName() == "SUB House Salad");
    CHECK(m.price() == Money::fromCents(300));
    CHECK(m.itemId == "house-salad");                  // its stock, its station
    CHECK(c.lines[0].total() == Money::fromCents(1150 + 300));
    CHECK(pos.lines()[0].toMap()[u"modifiers"_s].toList().size() == 3);

    // On its own it's a full salad; something not set up as a substitute says so.
    REQUIRE(pos.addItem(u"house-salad"_s));
    pos.cancelChoosing();
    pos.setQualifier(u"sub"_s);
    CHECK_FALSE(pos.addItem(u"cobb"_s));
    CHECK(app::menuItemFromJson(app::toJson(*pos.findItem(u"caesar"_s))).substitutePrice == Money::fromCents(350));
}

TEST_CASE("Event tickets: only as many as there are seats, kept across days, not after the event",
          "[menu][tickets]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = QDateTime(QDate::currentDate(), QTime(12, 0)).toMSecsSinceEpoch();
    pos.setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    auto *dinner = const_cast<core::MenuItem *>(pos.findItem(u"wine-dinner-ticket"_s));
    REQUIRE(dinner);
    dinner->ticketCapacity = 3;
    CHECK(pos.ticketsLeft(*dinner) == 3);

    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"wine-dinner-ticket"_s));
    REQUIRE(pos.addItem(u"wine-dinner-ticket"_s));
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"wine-dinner-ticket"_s));     // the last seat (on an open check: held)
    CHECK(pos.ticketsLeft(*dinner) == 0);
    CHECK(pos.soldOut().contains(u"wine-dinner-ticket"_s));
    CHECK_FALSE(pos.addItem(u"wine-dinner-ticket"_s));
    REQUIRE(pos.voidItem());                           // changed their mind: the seat is free again
    CHECK(pos.ticketsLeft(*dinner) == 1);
    pos.releaseCheck();

    // A new day: the two sold stay sold.
    REQUIRE(pos.endOfDay());
    CHECK(dinner->ticketsSoldBefore == 2);
    CHECK(pos.ticketsLeft(*dinner) == 1);

    // Editing the item keeps them; the date is typed; it can't be sold after.
    const int at = int(std::ranges::find_if(pos.shared()->menu, [](const core::MenuItem &m) {
                           return m.id == "wine-dinner-ticket"; }) - pos.shared()->menu.begin());
    QVariantMap rec = pos.adminRecords(u"menu"_s)[at].toMap();
    rec[u"eventAt"_s] = u"next friday"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, at, rec));
    const QDateTime when(QDate::currentDate(), QTime(19, 0));
    rec[u"eventAt"_s] = when.toString(u"yyyy-MM-dd HH:mm"_s);
    REQUIRE(pos.adminSave(u"menu"_s, at, rec));
    dinner = const_cast<core::MenuItem *>(pos.findItem(u"wine-dinner-ticket"_s));
    CHECK(dinner->ticketsSoldBefore == 2);
    CHECK(dinner->eventAt == when.toMSecsSinceEpoch());
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"wine-dinner-ticket"_s));
    CHECK(pos.lines().last().toMap()[u"name"_s].toString().contains(u"7:00"_s));   // the ticket says when
    clock = when.addSecs(60).toMSecsSinceEpoch();
    CHECK_FALSE(pos.addItem(u"wine-dinner-ticket"_s));
}

TEST_CASE("Qualifiers on choices: No onion, Extra bacon, Lite, on the Side", "[menu][qualifiers]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    pos.shared()->settings.extraPercent = 50;
    pos.shared()->settings.extraCharge = Money();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"classic-burger"_s));
    const auto option = [&](const QString &group, const QString &name) {
        for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList())
            if (g.toMap()[u"id"_s] == group)
                for (const QVariant &o : g.toMap()[u"options"_s].toList())
                    if (o.toMap()[u"name"_s] == name)
                        return o.toMap();
        return QVariantMap();
    };
    const auto mods = [&] {
        QStringList out;
        for (const QVariant &m : pos.lines().last().toMap()[u"modifiers"_s].toList())
            out << m.toMap()[u"name"_s].toString() + u' ' + m.toMap()[u"price"_s].toString();
        return out;
    };
    REQUIRE(pos.chooseOption(u"temperature"_s, 2));   // Medium
    REQUIRE(pos.chooseOption(u"side"_s, 0));

    // No onion: free, and doesn't count as a choice.
    pos.setQualifier(u"no"_s);
    CHECK(pos.chooseOption(u"toppings"_s, option(u"toppings"_s, u"Onion"_s)[u"index"_s].toInt()));
    CHECK(pos.pendingQualifier().isEmpty());                       // used up
    CHECK(option(u"toppings"_s, u"Onion"_s)[u"qualifier"_s] == u"No"_s);
    CHECK(mods().contains(u"No Onion "_s));

    // Extra bacon: $2.00 + 50%.
    REQUIRE(pos.chooseOptionAs(u"toppings"_s, option(u"toppings"_s, u"Bacon"_s)[u"index"_s].toInt(), u"extra"_s));
    CHECK(mods().contains(u"Extra Bacon $3.00"_s));
    // Touched again as Lite: the same bacon, another way (and free).
    REQUIRE(pos.chooseOptionAs(u"toppings"_s, option(u"toppings"_s, u"Bacon"_s)[u"index"_s].toInt(), u"lite"_s));
    CHECK(option(u"toppings"_s, u"Bacon"_s)[u"qualifier"_s] == u"Lite"_s);
    CHECK(std::ranges::count_if(mods(), [](const QString &m) { return m.contains(u"Bacon"_s); }) == 1);
    // The same way again: off.
    REQUIRE(pos.chooseOptionAs(u"toppings"_s, option(u"toppings"_s, u"Bacon"_s)[u"index"_s].toInt(), u"lite"_s));
    CHECK_FALSE(option(u"toppings"_s, u"Bacon"_s)[u"chosen"_s].toBool());

    // "No" in a choose-one group doesn't take the place of the choice.
    pos.setQualifier(u"no"_s);
    REQUIRE(pos.chooseOption(u"side"_s, 1));
    CHECK(option(u"side"_s, pos.choosingInfo()[u"groups"_s].toList()[1].toMap()[u"options"_s].toList()[0]
                                .toMap()[u"name"_s].toString())[u"chosen"_s].toBool());
    REQUIRE(pos.finishChoosing());                                  // still has its side

    // No / Lite / Side on a whole item: waits for a choice instead.
    pos.setQualifier(u"no"_s);
    const int before = int(pos.lines().size());
    CHECK_FALSE(pos.addItem(u"cobb"_s));
    CHECK(pos.lines().size() == before);
    CHECK(pos.pendingQualifier() == u"no"_s);                       // still armed
    pos.setQualifier(u"no"_s);                                      // touched again: off
    CHECK(pos.pendingQualifier().isEmpty());

    // The kitchen sees it.
    CHECK(QString::fromStdString(pos.shared()->open.begin()->second.lines.front().modifiers[2].kitchenText()) == u"No ONION"_s);
}

TEST_CASE("86'd during service: back after End of Day; marked sold out in the menu: kept", "[menu][eightysix]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = QDateTime(QDate::currentDate(), QTime(12, 0)).toMSecsSinceEpoch();
    pos.setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto available = [&](const char *id) { return pos.findItem(QString::fromLatin1(id))->available; };
    REQUIRE(pos.setAvailable(u"cobb"_s, false));            // ran out tonight
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"caesar"_s}, {u"name"_s, QString::fromStdString(pos.findItem(u"caesar"_s)->name)},
                                  {u"available"_s, false}}));   // off the menu for the season
    CHECK(pos.findItem(u"cobb"_s)->soldOutToday);
    CHECK_FALSE(pos.findItem(u"caesar"_s)->soldOutToday);
    // Saved through its card with the switch untouched: still today's.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, QString::fromStdString(pos.findItem(u"cobb"_s)->name)},
                                  {u"available"_s, false}, {u"price"_s, u"14.00"_s}}));
    CHECK(pos.findItem(u"cobb"_s)->soldOutToday);
    // Kept when stored and read back.
    CHECK(app::menuItemFromJson(app::toJson(*pos.findItem(u"cobb"_s))).soldOutToday);

    REQUIRE(pos.endOfDay());
    CHECK(available("cobb"));
    CHECK_FALSE(pos.findItem(u"cobb"_s)->soldOutToday);
    CHECK_FALSE(available("caesar"));

    // The store's choice: until someone turns it back on.
    pos.shared()->settings.soldOutBackNextDay = false;
    REQUIRE(pos.setAvailable(u"cobb"_s, false));
    clock += 24 * 3600 * 1000LL;
    REQUIRE(pos.endOfDay());
    CHECK_FALSE(available("cobb"));
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).soldOutBackNextDay == false);
    CHECK(app::settingsFromJson(QJsonObject()).soldOutBackNextDay);   // on unless turned off
}
