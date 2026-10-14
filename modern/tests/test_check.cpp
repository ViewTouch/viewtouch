#include <catch2/catch_test_macros.hpp>

#include "core/check.hh"
#include "core/employee.hh"

using namespace vt::core;
using vt::Money;

namespace {

Money usd(std::int64_t cents) { return Money::fromCents(cents); }

MenuItem item(const char *id, std::int64_t cents, TaxClass tax = TaxClass::Food, bool modifier = false)
{
    MenuItem m;
    m.id = id;
    m.name = id;
    m.price = usd(cents);
    m.taxClass = tax;
    m.isModifier = modifier;
    return m;
}

TaxRates rates()
{
    TaxRates r;
    r.foodPpm = 82500;      // 8.25%
    r.alcoholPpm = 100000;  // 10%
    return r;
}

// Burger 12.50 + Medium Rare + Onion Rings 1.00 = 13.50 food; Beer 6.00 alcohol.
Check burgerAndBeer()
{
    Check c;
    const auto &burger = c.addItem(item("Burger", 1250));
    REQUIRE(c.addModifier(burger.id, item("Medium Rare", 0, TaxClass::Food, true)));
    REQUIRE(c.addModifier(burger.id, item("Onion Rings", 100, TaxClass::Food, true)));
    c.addItem(item("Beer", 600, TaxClass::Alcohol));
    return c;
}

} // namespace

TEST_CASE("Money rounding matches legacy FltToPrice (half away from zero)", "[money][tax]")
{
    CHECK(usd(10000).ppm(98750).cents() == 988);    // 9.875% of $100.00 = 9.875 -> 9.88
    CHECK(usd(10000).ppm(99750).cents() == 998);    // QST 9.975% of $100.00
    CHECK(usd(1100).ppm(82500).cents() == 91);      // 90.75c -> 91c
    CHECK(usd(1).ppm(500000).cents() == 1);         // half a cent rounds up
    CHECK(usd(-1).ppm(500000).cents() == -1);       // and away from zero when negative
    CHECK(usd(1350).ppm(82500).cents() == 111);     // 111.375c -> 111c
}

TEST_CASE("Totals: items, modifiers, per-class tax", "[check]")
{
    const Check c = burgerAndBeer();
    REQUIRE(c.lines.size() == 2);
    CHECK(c.lines[0].total() == usd(1350));

    const Totals t = c.totals(rates());
    CHECK(t.items == usd(1950));
    CHECK(t.taxByClass.at(TaxClass::Food) == usd(111));      // 13.50 * 8.25% = 1.11375
    CHECK(t.taxByClass.at(TaxClass::Alcohol) == usd(60));
    CHECK(t.tax == usd(171));
    CHECK(t.total == usd(2121));
    CHECK(t.balance == usd(2121));
}

TEST_CASE("Tax is figured on each class total, not per line", "[check][tax]")
{
    // Three 1.10 items at 8.25%: per line 9.075c -> 9c each = 27c;
    // on the 3.30 total it is 27.225c -> 27c. Pick a case where they differ:
    // three 0.30 items: per line 2.475c -> 2c each = 6c; on 0.90: 7.425c -> 7c.
    Check c;
    for (int i = 0; i < 3; ++i)
        c.addItem(item("Mint", 30));
    CHECK(c.totals(rates()).tax == usd(7));
}

TEST_CASE("Qualifiers change modifier and item prices", "[check]")
{
    Check c;
    const auto &burger = c.addItem(item("Burger", 1000));
    c.addModifier(burger.id, item("Cheese", 100, TaxClass::Food, true), Qualifier::No);
    c.addModifier(burger.id, item("Bacon", 200, TaxClass::Food, true), Qualifier::Double);
    c.addModifier(burger.id, item("Onions", 50, TaxClass::Food, true), Qualifier::Extra);
    CHECK(c.lines[0].total() == usd(1000 + 0 + 400 + 50));
    CHECK(c.lines[0].modifiers[0].displayName() == "No Cheese");

    c.addItem(item("Fries", 300), Qualifier::Lite);
    CHECK(c.lines[1].total() == usd(0));
    CHECK(c.lines[1].displayName() == "Lite Fries");
}

TEST_CASE("Takeout food can be tax free; alcohol still taxed", "[check][tax]")
{
    Check c = burgerAndBeer();
    c.type = CheckType::Takeout;
    TaxRates r = rates();
    r.taxTakeoutFood = false;
    const Totals t = c.totals(r);
    CHECK_FALSE(t.taxByClass.contains(TaxClass::Food));
    CHECK(t.tax == usd(60));

    c.type = CheckType::DineIn;
    CHECK(c.totals(r).tax == usd(171));
}

TEST_CASE("Discounts reduce each tax class proportionally", "[check][tax]")
{
    Check c = burgerAndBeer();
    Tender tenPercent{"d10", "10% off", TenderKind::Discount, 1000};
    c.addPayment(tenPercent, {});

    const Totals t = c.totals(rates());
    CHECK(t.discounts == usd(195));
    CHECK(t.subtotal == usd(1755));
    // food net 13.50 - 1.35 = 12.15 -> 1.002375 -> 1.00; alcohol 6.00 - 0.60 = 5.40 -> 0.54
    CHECK(t.taxByClass.at(TaxClass::Food) == usd(100));
    CHECK(t.taxByClass.at(TaxClass::Alcohol) == usd(54));
    CHECK(t.total == usd(1755 + 154));
}

TEST_CASE("A full comp leaves nothing to pay", "[check]")
{
    Check c = burgerAndBeer();
    c.addPayment({"comp", "Comp", TenderKind::Discount, 10000}, {});
    c.addPayment({"comp2", "Comp again", TenderKind::Discount, 10000}, {});   // capped
    const Totals t = c.totals(rates());
    CHECK(t.discounts == t.items);
    CHECK(t.tax == usd(0));
    CHECK(t.total == usd(0));
}

TEST_CASE("Cash overpayment gives change; removing a payment restores the balance", "[check]")
{
    Check c = burgerAndBeer();
    const Tender cash{"cash", "Cash", TenderKind::Cash, 0};
    const auto pid = c.addPayment(cash, usd(2500)).id;
    Totals t = c.totals(rates());
    CHECK(t.paid == usd(2500));
    CHECK(t.balance == usd(-379));
    CHECK(t.change == usd(379));

    REQUIRE(c.removePayment(pid));
    CHECK(c.totals(rates()).balance == usd(2121));
    CHECK_FALSE(c.removePayment(pid));
}

TEST_CASE("Unsent lines are removed; sent lines must be voided", "[check]")
{
    Check c = burgerAndBeer();
    const auto burgerId = c.lines[0].id;
    const auto beerId = c.lines[1].id;
    CHECK(c.unsentCount() == 2);
    CHECK(c.sendAll(1000) == 2);
    CHECK(c.unsentCount() == 0);
    CHECK(c.lines[0].sentAt == 1000);

    CHECK_FALSE(c.removeLine(burgerId));
    CHECK_FALSE(c.addModifier(burgerId, item("Cheese", 100, TaxClass::Food, true)));
    REQUIRE(c.voidLine(burgerId));
    CHECK_FALSE(c.voidLine(burgerId));
    CHECK(c.totals(rates()).items == usd(600));

    const auto &fries = c.addItem(item("Fries", 300));
    CHECK_FALSE(c.voidLine(fries.id));           // not sent yet
    CHECK(c.removeLine(fries.id));
    CHECK(c.line(beerId));
    CHECK(c.lastItemLine() == nullptr);          // everything left is sent
}

TEST_CASE("Comments are untaxed free lines", "[check]")
{
    Check c;
    c.addItem(item("Soup", 500));
    const auto &note = c.addComment("Allergy: nuts");
    CHECK(note.isComment());
    CHECK(c.totals(rates()).items == usd(500));
    CHECK(c.lastItemLine()->name == "Soup");
}

TEST_CASE("Roles grant permissions", "[employee]")
{
    Employee server;
    server.role = "server";
    CHECK(server.can(perm::Order));
    CHECK(server.can(perm::Settle));
    CHECK_FALSE(server.can(perm::Void));
    CHECK_FALSE(server.can(perm::EditLayout));

    Employee manager;
    manager.role = "manager";
    CHECK(manager.can(perm::Void));
    CHECK(manager.can(perm::EditLayout));

    Employee nobody;
    nobody.role = "unknown";
    CHECK_FALSE(nobody.can(perm::Order));
}

TEST_CASE("Each send gets its own time, even within one millisecond", "[check]")
{
    Check c;
    c.addItem(item("Soup", 500));
    c.sendAll(1000);
    c.addItem(item("Bread", 200));
    c.sendAll(1000);
    CHECK(c.lines[0].sentAt == 1000);
    CHECK(c.lines[1].sentAt == 1001);
}

TEST_CASE("Cash rounding: to the nearest 5 cents when paid in cash, cards exact", "[check][rounding]")
{
    TaxRates r;   // no tax: the totals are the prices
    r.cashRoundingCents = 5;
    Tender cash;
    cash.id = "cash";
    cash.name = "Cash";
    cash.kind = TenderKind::Cash;
    Tender card;
    card.id = "card";
    card.name = "Card";
    card.kind = TenderKind::Card;

    // $17.62 in cash: $17.60 settles it, 2 cents rounded away.
    Check down;
    down.addItem(item("Plate", 1762, TaxClass::None));
    CHECK(down.totals(r).rounding.cents() == 0);              // nothing paid yet
    down.addPayment(cash, usd(1760));
    Totals t = down.totals(r);
    CHECK(t.rounding.cents() == -2);
    CHECK(t.balance.cents() == 0);
    CHECK(t.cashNet().cents() == 1760);

    // $17.63 rounds up to $17.65; a $20 bill gets $2.35 back.
    Check up;
    up.addItem(item("Plate", 1763, TaxClass::None));
    up.addPayment(cash, usd(2000));
    t = up.totals(r);
    CHECK(t.rounding.cents() == 2);
    CHECK(t.change.cents() == 235);

    // Card for part, cash for the rest: only what cash pays is rounded.
    Check split;
    split.addItem(item("Plate", 1762, TaxClass::None));
    split.addPayment(card, usd(1000));
    split.addPayment(cash, usd(760));
    t = split.totals(r);
    CHECK(t.rounding.cents() == -2);
    CHECK(t.balance.cents() == 0);

    // All on a card: exact.
    Check exact;
    exact.addItem(item("Plate", 1762, TaxClass::None));
    exact.addPayment(card, usd(1762));
    CHECK(exact.totals(r).rounding.cents() == 0);
    CHECK(exact.totals(r).balance.cents() == 0);

    // No rounding set: to the cent.
    r.cashRoundingCents = 0;
    CHECK(down.totals(r).rounding.cents() == 0);
    CHECK(down.totals(r).balance.cents() == 2);
}
