#include <catch2/catch_test_macros.hpp>

#include "app/pos_demo.hh"
#include "app/pos_service.hh"
#include "app/pos_starter.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;
using namespace vt;

// The starter sets: a restaurant, a food truck and a shop, each with its
// menus (a sushi bar's, a pizzeria's, a hardware store's, a bookstore's...):
// every one loads, is ready to sell, and food and a shop's goods never mix.

namespace {

bool validEan13(const std::string &code)
{
    if (code.size() != 13 || !std::ranges::all_of(code, [](char c) { return c >= '0' && c <= '9'; }))
        return false;
    int sum = 0;
    for (int i = 0; i < 12; ++i)
        sum += (code[i] - '0') * (i % 2 ? 3 : 1);
    return (10 - sum % 10) % 10 == code[12] - '0';
}

QStringList seriousProblems(app::PosService &pos)
{
    QStringList out;
    for (const QVariant &p : pos.menuProblems())
        if (p.toMap()[u"serious"_s].toBool())
            out << p.toMap()[u"text"_s].toString();
    return out;
}

} // namespace

TEST_CASE("Starter sets: every menu loads on its kind of store, ready to sell", "[starter]")
{
    QSet<std::string> barcodes;   // every shop's together: unique
    int food = 0, shops = 0;
    for (const app::StarterPack &p : app::starterPacks()) {
        INFO(p.id.toStdString());
        const bool shop = p.kind == u"retail";
        (shop ? shops : food) += 1;
        CHECK_FALSE(p.name.isEmpty());
        CHECK_FALSE(p.description.isEmpty());
        CHECK(p.items >= 15);
        QString error;
        const auto data = app::starterData({shop ? u"retail"_s : u"restaurant"_s, p.id}, &error);
        INFO(error.toStdString());
        REQUIRE(data);
        CHECK(int(std::ranges::count_if(data->menu, [](const core::MenuItem &m) { return !m.isModifier; })) == p.items);
        CHECK(data->settings.serviceMode == (shop ? "retail" : ""));

        app::PosShared shared(*data, nullptr);
        app::PosService pos(&shared, u"Test"_s);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        INFO(seriousProblems(pos).join(u"\n"_s).toStdString());
        CHECK(seriousProblems(pos).isEmpty());
        for (const core::MenuItem &m : data->menu) {
            if (m.isModifier)
                continue;
            INFO(m.name);
            if (shop) {
                // Each scans: the item, or each of its sizes and colors, its own barcode.
                if (m.variants.empty()) {
                    REQUIRE(m.barcodes.size() == 1);
                    CHECK(validEan13(m.barcodes[0]));
                    CHECK_FALSE(barcodes.contains(m.barcodes[0]));
                    barcodes.insert(m.barcodes[0]);
                } else {
                    CHECK(m.barcodes.empty());
                    for (const core::MenuItem::Variant &v : m.variants) {
                        CHECK(validEan13(v.barcode));
                        CHECK_FALSE(barcodes.contains(v.barcode));
                        barcodes.insert(v.barcode);
                        CHECK_FALSE(v.sku.empty());
                    }
                }
                CHECK(m.taxClass != core::TaxClass::Room);
            } else {
                // Food: no barcodes, sizes and colors or by-weight goods.
                CHECK(m.barcodes.empty());
                CHECK(m.variants.empty());
                CHECK_FALSE(m.byWeight);
            }
        }
        if (shop) {
            // Counted stock for what's counted: each size and color its own.
            for (const core::MenuItem &m : data->menu)
                for (const core::MenuItem::Variant &v : m.variants)
                    if (!m.recipe.empty())
                        CHECK(shared.ingredient(app::variantStockId(m.id, v.key)) != nullptr);
        }
    }
    CHECK(food >= 19);
    CHECK(shops >= 13);
}

TEST_CASE("Starter sets: a restaurant, a food truck and a shop, each its own", "[starter]")
{
    // A restaurant: the sample café, its tables, kitchen and bar.
    const auto restaurant = app::starterData({});
    REQUIRE(restaurant);
    CHECK(restaurant->settings.serviceMode.empty());
    CHECK(restaurant->settings.printer("kitchen"));
    CHECK(std::ranges::any_of(restaurant->menu, [](const core::MenuItem &m) { return m.id == "classic-burger"; }));

    // A food truck: its taqueria, order numbers, its own crew.
    const auto truck = app::starterData({u"foodTruck"_s, {}});
    REQUIRE(truck);
    CHECK(truck->settings.serviceMode == "foodTruck");
    CHECK(truck->settings.orderNumbers);
    CHECK(std::ranges::any_of(truck->menu, [](const core::MenuItem &m) { return m.name == "Al Pastor Taco"; }));
    CHECK(std::ranges::none_of(truck->menu, [](const core::MenuItem &m) { return m.id == "classic-burger"; }));
    CHECK(std::ranges::any_of(truck->employees, [](const core::Employee &e) { return e.id == "ana"; }));

    // A shop: its clothes in sizes and colors, returns, no kitchen.
    const auto shop = app::starterData({u"retail"_s, {}});
    REQUIRE(shop);
    CHECK(shop->settings.serviceMode == "retail");
    CHECK(shop->settings.returnDays == 30);
    CHECK_FALSE(shop->settings.printer("kitchen"));
    CHECK(shop->settings.stations.empty());
    CHECK(std::ranges::any_of(shop->menu, [](const core::MenuItem &m) { return m.name == "Classic Tee" && !m.variants.empty(); }));
    CHECK(std::ranges::none_of(shop->menu, [](const core::MenuItem &m) { return !m.modifierGroups.empty(); }));

    // A sushi bar on a restaurant: its own stations come with it.
    const auto sushi = app::starterData({u"restaurant"_s, u"sushi"_s});
    REQUIRE(sushi);
    CHECK(sushi->settings.printer("kitchen"));
    CHECK(std::ranges::any_of(sushi->settings.stations, [](const core::Station &s) { return s.id == "sushi"; }));
    CHECK(std::ranges::none_of(sushi->menu, [](const core::MenuItem &m) { return m.id == "classic-burger"; }));

    // A grocery: produce by the pound.
    const auto grocery = app::starterData({u"retail"_s, u"grocery"_s});
    REQUIRE(grocery);
    CHECK(std::ranges::any_of(grocery->menu, [](const core::MenuItem &m) { return m.name == "Bananas" && m.byWeight; }));

    // Food and a shop's goods never on one store; nonsense refused.
    QString error;
    CHECK_FALSE(app::parseStarterSet(u"restaurant:hardware"_s, &error));
    CHECK(error.contains(u"a shop"_s));
    CHECK_FALSE(app::parseStarterSet(u"retail:sushi"_s));
    CHECK_FALSE(app::parseStarterSet(u"spaceship"_s));
    CHECK_FALSE(app::parseStarterSet(u"retail:nothing-like-it"_s));
    const auto parsed = app::parseStarterSet(u"truck:hotdogs"_s);
    REQUIRE(parsed);
    CHECK(parsed->kind == u"foodTruck"_s);
}

TEST_CASE("Starter sets: the kinds of store and their menus, for Factory reset and the setup guide", "[starter]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    const QVariantList kinds = pos.starterSets();
    REQUIRE(kinds.size() == 3);
    for (const QVariant &k : kinds) {
        const QVariantMap kind = k.toMap();
        INFO(kind[u"kind"_s].toString().toStdString());
        const QVariantList packs = kind[u"packs"_s].toList();
        REQUIRE(packs.size() >= 13);
        // Each one's own first: a restaurant's café, a truck's taqueria, a shop's clothes.
        const QString first = packs[0].toMap()[u"set"_s].toString();
        if (kind[u"kind"_s] == u"restaurant"_s)
            CHECK(first == u"restaurant"_s);
        else if (kind[u"kind"_s] == u"foodTruck"_s)
            CHECK(first == u"foodTruck:taqueria"_s);
        else
            CHECK(first == u"retail:apparel"_s);
        for (const QVariant &p : packs)
            CHECK(app::parseStarterSet(p.toMap()[u"set"_s].toString()));
    }
    // A shop lists a shop's for Menu Builder's templates; a restaurant food.
    app::PosShared shop(*app::starterData({u"retail"_s, {}}), nullptr);
    app::PosService retail(&shop, u"Shop"_s);
    REQUIRE(retail.loginWithPin(u"1234"_s));
    const QVariantList templates = retail.menuTemplates();
    REQUIRE(templates.size() >= 13);
    for (const QVariant &t : templates)
        CHECK(t.toMap()[u"kind"_s] == u"retail"_s);
    CHECK_FALSE(retail.applyMenuTemplate(u"sushi"_s));
    REQUIRE(retail.applyMenuTemplate(u"books"_s));   // another shop's goods, alongside
    CHECK(std::ranges::any_of(shop.menu, [](const core::MenuItem &m) {
        return m.name == "The Lighthouse Keeper" && m.barcodes.size() == 1 && m.barcodes[0].starts_with("978");
    }));
}

TEST_CASE("Demo history fits the kind of store: a taco truck's window, a shop's counter, a sushi bar's menu", "[starter][demo]")
{
    // Now: what's posted for tonight (the 86) is still up when the test looks.
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    SECTION("a taco truck: orders by number at the window, prep counts, no tables")
    {
        app::PosShared shared(*app::starterData({u"foodTruck"_s, {}}), nullptr);
        app::PosService pos(&shared, u"Truck"_s);
        const QString said = app::fillDemoData(pos, now);
        INFO(said.toStdString());
        REQUIRE(said.startsWith(u"Added"_s));
        CHECK(said.contains(u"orders"_s));
        CHECK(shared.pastDays.size() >= 55);   // (the store keeps the last 60 in memory)
        REQUIRE_FALSE(shared.closedToday.empty());
        for (const core::Check &c : shared.closedToday) {
            CHECK(c.type != core::CheckType::DineIn);
            CHECK(c.orderNumber > 0);
        }
        CHECK_FALSE(shared.settings.prepCounts.empty());   // made this morning, one running low
        REQUIRE(pos.loginWithPin(u"1234"_s));
        CHECK(pos.truckOrders().size() >= 5);              // cooking, and waiting at the window
        CHECK(std::ranges::none_of(shared.closedToday, [](const core::Check &c) {
            return std::ranges::any_of(c.lines, [](const core::OrderLine &l) { return l.itemId == "classic-burger"; });
        }));
    }
    SECTION("a shop: scanned sizes and colors, returns and exchanges, nothing for a kitchen")
    {
        app::PosShared shared(*app::starterData({u"retail"_s, {}}), nullptr);
        app::PosService pos(&shared, u"Shop"_s);
        const QString said = app::fillDemoData(pos, now);
        INFO(said.toStdString());
        REQUIRE(said.startsWith(u"Added"_s));
        const QRegularExpressionMatch returns = QRegularExpression(u"(\\d+) returns"_s).match(said);
        REQUIRE(returns.hasMatch());
        CHECK(returns.captured(1).toInt() > 20);
        CHECK(shared.pastDays.size() >= 55);   // (the store keeps the last 60 in memory)
        bool sized = false;
        for (const core::Check &c : shared.closedToday)
            for (const core::OrderLine &l : c.lines)
                sized = sized || !l.variant.empty();
        CHECK(sized);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        CHECK(pos.kitchenTickets().isEmpty());
        CHECK(shared.open.size() >= 2);   // one being rung up, one put aside
    }
    SECTION("a sushi bar: its own menu on every check, its own 86")
    {
        app::PosShared shared(*app::starterData({u"restaurant"_s, u"sushi"_s}), nullptr);
        app::PosService pos(&shared, u"Sushi"_s);
        REQUIRE(app::fillDemoData(pos, now).startsWith(u"Added"_s));
        // Nothing of the café's (its burgers, sodas): the sushi bar's own.
        QSet<std::string> cafe;
        const auto cafeSet = app::starterData({});   // (kept: a loop over a temporary's member reads freed memory)
        REQUIRE(cafeSet);
        for (const core::MenuItem &m : cafeSet->menu)
            cafe.insert(m.id);
        int lines = 0;
        for (const auto &[id, c] : shared.open)
            for (const core::OrderLine &l : c.lines) {
                ++lines;
                INFO(l.itemId);
                CHECK_FALSE(cafe.contains(l.itemId));
            }
        CHECK(lines > 4);   // the floor: tables seated with sushi, not empty checks
        bool told = false;
        for (const QVariant &m : pos.messages())
            told = told || m.toMap()[u"text"_s].toString().startsWith(u"86 "_s);
        CHECK(told);
    }
}

TEST_CASE("Setup guide: a new store starts over as another kind's sample, a store that sold doesn't", "[starter][setup]")
{
    auto data = *app::starterData({});
    data.settings.setupDone = false;
    app::PosShared shared(std::move(data), nullptr);
    app::PosService pos(&shared, u"Register"_s);
    QString asked;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.startWithStarter(u"foodTruck:taqueria"_s));   // not on the store's computer
    CHECK(pos.setupInfo()[u"canStartOver"_s] == false);
    shared.requestFactoryReset = [&](const QString &set) { asked = set; return true; };
    CHECK(pos.setupInfo()[u"canStartOver"_s] == true);
    CHECK_FALSE(pos.startWithStarter(u"foodTruck:hardware"_s));   // a shop's goods on a truck
    REQUIRE(pos.startWithStarter(u"foodTruck:taqueria"_s));
    CHECK(asked == u"foodTruck:taqueria"_s);
    // Something sold: Factory reset (the maintainer's) only.
    REQUIRE(pos.addItem(u"soda"_s));
    CHECK(pos.setupInfo()[u"canStartOver"_s] == false);
    CHECK_FALSE(pos.startWithStarter(u"retail:books"_s));
    CHECK(asked == u"foodTruck:taqueria"_s);
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));   // a server: not theirs
    CHECK_FALSE(pos.startWithStarter(u"retail:books"_s));
}
