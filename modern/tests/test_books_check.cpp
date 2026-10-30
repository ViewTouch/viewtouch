#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <random>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The books a bookkeeper does the taxes from: every report agrees with the
// checks and with every other report, each tax is its rate on what it's on,
// rounded once, and the money in matches the money counted. First a day of
// every kind of sale worked out by hand; then hundreds of random days.

namespace {

// "$1,234.56", "-$2.50" -> cents.
std::int64_t cents(const std::string &text)
{
    std::int64_t v = 0;
    bool negative = false, any = false;
    for (const char c : text) {
        if (c == '-')
            negative = true;
        else if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            any = true;
        }
    }
    REQUIRE(any);
    return negative ? -v : v;
}

std::string value(const core::Report &r, const std::string &label)
{
    const std::vector<std::string> cells = r.find(label);
    INFO("no row \"" << label << "\" in " << r.title);
    REQUIRE_FALSE(cells.empty());
    return cells.back();
}

std::int64_t amount(const core::Report &r, const std::string &label) { return cents(value(r, label)); }

// A tax on what it's on: parts per million, rounded half away from zero,
// worked out here on its own (not with Money).
std::int64_t taxOn(std::int64_t base, std::int64_t ppm)
{
    const std::int64_t n = base * ppm;
    return n >= 0 ? (n + 500'000) / 1'000'000 : -((-n + 500'000) / 1'000'000);
}

// Every choice an item asks for: the `pick`th option (or the first); with
// `pick`, now and then an optional one too (an add-on that costs extra).
void choose(PosService &pos, const std::function<int(int)> &pick = {})
{
    for (int guard = 0; guard < 8 && pos.choosingInfo()[u"active"_s].toBool(); ++guard) {
        for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList()) {
            const QVariantMap group = g.toMap();
            const int n = int(group[u"options"_s].toList().size());
            if (n == 0 || (group[u"done"_s].toBool() && (!pick || pick(3) != 0)))
                continue;
            pos.chooseOption(group[u"id"_s].toString(), pick ? pick(n) : 0);
        }
        pos.finishChoosing();
    }
}

void add(PosService &pos, const char *item)
{
    REQUIRE(pos.addItem(QString::fromLatin1(item)));
    choose(pos);
}

// A store for the books: the starter menu and tenders, no promotions that
// depend on the time of day, no reasons asked, $100.00 in the drawer.
std::unique_ptr<PosService> openStore()
{
    auto pos = std::make_unique<PosService>(test::seedPosData(), nullptr);
    core::PosSettings &s = pos->shared()->settings;
    s.promotions.clear();
    s.askReasons = false;
    REQUIRE(s.tax.foodPpm == 82500);      // 8.25%
    REQUIRE(s.tax.alcoholPpm == 100000);  // 10%
    core::PosSettings::Promotion save5;   // $5 off the order
    save5.id = "save5";
    save5.name = "Save $5";
    save5.code = "SAVE5";
    save5.amount = Money::fromCents(500);
    core::PosSettings::Promotion beer;    // half off a draft beer
    beer.id = "beer50";
    beer.name = "Half-price beer";
    beer.code = "BEER50";
    beer.items = {"draft-beer"};
    beer.percentBp = 5000;
    s.promotions = {save5, beer};
    REQUIRE(pos->loginWithPin(u"1234"_s));
    pos->entryKey(u"10000"_s);
    REQUIRE(pos->openDrawerSession());
    return pos;
}

qint64 currentId(PosService &pos) { return pos.checkInfo()[u"id"_s].toLongLong(); }

const core::Check &closedCheck(PosService &pos, qint64 id)
{
    const auto &closed = pos.shared()->closedToday;
    const auto it = std::ranges::find_if(closed, [&](const core::Check &c) { return c.id == id; });
    REQUIRE(it != closed.end());
    return *it;
}

} // namespace

TEST_CASE("Books check: a day of every kind of sale, to the cent", "[books][bookscheck]")
{
    auto store = openStore();
    PosService &pos = *store;
    const auto takeout = [&] { REQUIRE(pos.startCheck(core::CheckType::Takeout)); return currentId(pos); };

    // 1. Classic Burger $11.50 (food) and a draft beer $6.00 (alcohol), $20.00 cash.
    //    Food tax 8.25% of 11.50 = 0.94875 -> 0.95; alcohol 10% of 6.00 = 0.60.
    //    Total 19.05, change 0.95.
    takeout();
    add(pos, "classic-burger");
    add(pos, "draft-beer");
    REQUIRE(pos.tender(u"cash"_s, 2000));
    REQUIRE(pos.closeCheck());

    // 2. Cobb 12.50 + Caesar 9.75 = 22.25, 10% off: 2.225 -> 2.23. Net 20.02;
    //    tax 1.65165 -> 1.65; total 21.67 by card.
    takeout();
    add(pos, "cobb");
    add(pos, "caesar");
    REQUIRE(pos.tender(u"discount"_s));
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());

    // 3. Cobb and a soda sent; the soda (2.95) voided. 12.50, tax 1.03125 ->
    //    1.03, total 13.53 cash.
    takeout();
    add(pos, "cobb");
    add(pos, "soda");
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.voidItem());   // the newest line: the soda
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // 4. Bacon Burger 13.50 + house wine 8.00 = 21.50, coupon $5 off the
    //    order, by what each is worth: food 5 x 13.50 / 21.50 = 3.1395 ->
    //    3.14, wine 1.86. Food 10.36 -> tax 0.8547 -> 0.85; wine 6.14 ->
    //    0.614 -> 0.61. Net 16.50, tax 1.46, total 17.96 by card.
    const qint64 four = takeout();
    add(pos, "bacon-burger");
    add(pos, "house-wine");
    REQUIRE(pos.applyCoupon(u"save5"_s));
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());
    {
        const core::Totals t = closedCheck(pos, four).totals(pos.shared()->settings.tax);
        CHECK(t.taxableByClass.at(core::TaxClass::Food) == Money::fromCents(1036));
        CHECK(t.taxableByClass.at(core::TaxClass::Alcohol) == Money::fromCents(614));
    }

    // 5. House salad 8.50 + draft beer 6.00, half off the beer (3.00): all of
    //    it off the alcohol. Food 8.50 -> 0.70125 -> 0.70; beer 3.00 -> 0.30.
    //    Net 11.50, tax 1.00, total 12.50 cash.
    const qint64 five = takeout();
    add(pos, "house-salad");
    add(pos, "draft-beer");
    REQUIRE(pos.applyCoupon(u"beer50"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    {
        const core::Totals t = closedCheck(pos, five).totals(pos.shared()->settings.tax);
        CHECK(t.taxableByClass.at(core::TaxClass::Food) == Money::fromCents(850));
        CHECK(t.taxableByClass.at(core::TaxClass::Alcohol) == Money::fromCents(300));
    }

    // 6. A Caesar (9.75) comped: nothing to pay, nothing taxed.
    takeout();
    add(pos, "caesar");
    REQUIRE(pos.tender(u"comp"_s));
    REQUIRE(pos.closeCheck());

    // 7. A Cobb (12.50) sold tax-free on a certificate, by card.
    takeout();
    add(pos, "cobb");
    REQUIRE(pos.setTaxExempt(u"CERT-1"_s));
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());

    // 8. A $25.00 gift card sold for cash: owed to the guest, not a sale.
    REQUIRE(pos.sellGiftCard({}, 2500));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // 9. A practice check: in no report.
    REQUIRE(pos.setTraining(true));
    takeout();
    add(pos, "cobb");
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.setTraining(false));

    // 10. A coffee 2.75, tax 0.226875 -> 0.23, 2.98 cash; $1.00 of it given back.
    const qint64 ten = takeout();
    add(pos, "coffee");
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    {
        const core::Check &c = closedCheck(pos, ten);
        REQUIRE(pos.refundPayment(c.id, c.payments.front().id, 100, u"Cold"_s));
    }

    // The day, worked out by hand (checks 1-8 and 10):
    //   items      17.50 + 22.25 + 12.50 + 21.50 + 14.50 + 9.75 + 12.50 + 2.75 = 113.25
    //   discounts  2.23 + 5.00 + 3.00 + 9.75 = 19.98; net sales 93.27
    //   taxable food  11.50 + 20.02 + 12.50 + 10.36 + 8.50 + 2.75 = 65.63
    //   taxable alcohol  6.00 + 6.14 + 3.00 = 15.14; non-taxable 12.50 (the certificate)
    //   food tax  0.95 + 1.65 + 1.03 + 0.85 + 0.70 + 0.23 = 5.41; alcohol 0.60 + 0.61 + 0.30 = 1.51
    //   total with tax 93.27 + 6.92 + 25.00 (gift card) = 125.19
    //   cash 20.00 + 13.53 + 12.50 + 25.00 + 2.98 = 74.01; card 21.67 + 17.96 + 12.50 = 52.13
    const core::Report sales = pos.buildReport(u"sales"_s);
    CHECK(value(sales, "Checks") == "9");
    CHECK(value(sales, "Item sales") == "$113.25");
    CHECK(value(sales, "Discounts & comps") == "-$19.98");
    CHECK(value(sales, "Net sales") == "$93.27");
    CHECK(value(sales, "Taxable food sales") == "$65.63");
    CHECK(value(sales, "Taxable alcohol sales") == "$15.14");
    CHECK(value(sales, "Non-taxable sales") == "$12.50");
    CHECK(value(sales, "  of which tax-exempt, with certificates (1)") == "$12.50");
    CHECK(value(sales, "Food tax") == "$5.41");
    CHECK(value(sales, "Alcohol tax") == "$1.51");
    CHECK(value(sales, "Sales tax") == "$6.92");
    CHECK(value(sales, "Gift cards sold (owed to guests, not sales)") == "$25.00");
    CHECK(value(sales, "Total with tax") == "$125.19");
    CHECK(value(sales, "Cash") == "$74.01");
    CHECK(value(sales, "Credit Card") == "$52.13");
    CHECK(value(sales, "Change given") == "-$0.95");
    CHECK(value(sales, "Collected") == "$125.19");
    CHECK(value(sales, "Refunds") == "-$1.00");
    CHECK(value(sales, "Collected after refunds") == "$124.19");
    CHECK(value(sales, "Voided items (1)") == "$2.95");
    CHECK(sales.find("Collected and the total differ by").empty());   // no check closed with a balance

    // The same sales by item and by category.
    CHECK(value(pos.buildReport(u"items"_s), "Net sales") == "$93.27");
    CHECK(pos.buildReport(u"categories"_s).find("Net sales").at(2) == "$93.27");

    // The drawer: $100.00 to start, 74.01 cash in, 0.95 change, 1.00 back.
    const Money expected = core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday,
                                              pos.shared()->settings.tax);
    CHECK(expected == Money::fromCents(10000 + 7401 - 95 - 100));
    const core::Report deposit = pos.buildReport(u"deposit"_s);
    CHECK(value(deposit, "Card batch") == "$52.13");
    CHECK(value(deposit, "Cash refunds") == "-$1.00");

    // The journal balances.
    const std::vector<std::string> journal = pos.buildReport(u"accounting"_s).find("Total");
    REQUIRE(journal.size() == 3);
    CHECK(journal[1] == journal[2]);

    // Each coupon counted once, on the check it took something off.
    for (const core::PosSettings::Promotion &p : pos.shared()->settings.promotions)
        CHECK(p.uses == 1);
}

TEST_CASE("Books check: a promotion's tax comes off where its items are", "[books][bookscheck][tax]")
{
    // Happy hour, half off draft beer, on a check with a burger: all of the
    // beer's discount comes off the alcohol tax, none off the food's.
    auto store = openStore();
    PosService &pos = *store;
    core::PosSettings::Promotion happy;
    happy.id = "happy";
    happy.name = "Happy hour";
    happy.items = {"draft-beer"};
    happy.percentBp = 5000;
    pos.shared()->settings.promotions.push_back(happy);   // all day, by itself
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    const qint64 id = currentId(pos);
    add(pos, "classic-burger");   // 11.50 food
    add(pos, "draft-beer");       // 6.00 alcohol, 3.00 off
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    const core::Totals t = closedCheck(pos, id).totals(pos.shared()->settings.tax);
    CHECK(t.discounts == Money::fromCents(300));
    CHECK(t.taxableByClass.at(core::TaxClass::Food) == Money::fromCents(1150));    // untouched
    CHECK(t.taxableByClass.at(core::TaxClass::Alcohol) == Money::fromCents(300));
    CHECK(t.taxByClass.at(core::TaxClass::Food) == Money::fromCents(95));           // 0.94875
    CHECK(t.taxByClass.at(core::TaxClass::Alcohol) == Money::fromCents(30));

    // An order-wide coupon leaves out what may not be discounted (the wine,
    // marked so); one naming the wine still covers it.
    for (core::MenuItem &m : pos.shared()->menu)
        if (m.id == "house-wine")
            m.noDiscount = true;
    pos.shared()->settings.promotions.pop_back();
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    add(pos, "cobb");         // 12.50
    add(pos, "house-wine");   // 8.00, no discounts
    core::PosSettings::Promotion tenOff;
    tenOff.id = "ten";
    tenOff.name = "10% off";
    tenOff.code = "TEN";
    tenOff.percentBp = 1000;
    pos.shared()->settings.promotions.push_back(tenOff);
    REQUIRE(pos.applyCoupon(u"TEN"_s));
    const qint64 two = currentId(pos);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    const core::Totals u = closedCheck(pos, two).totals(pos.shared()->settings.tax);
    CHECK(u.discounts == Money::fromCents(125));   // 10% of the Cobb only
    CHECK(u.taxableByClass.at(core::TaxClass::Alcohol) == Money::fromCents(800));
}

// Hundreds of random checks on twenty days: every kind of item, choices that
// cost extra, quantities, discounts, comps, staff meals, coupons, voids, tax
// exemptions, gift cards sold, split payments, tips, gratuity, refunds and
// practice checks. Whatever happens, the books must agree.
TEST_CASE("Books check: random days, every report agrees with the checks and each other", "[books][bookscheck][random]")
{
    const std::vector<const char *> menu{"classic-burger", "cheeseburger", "bacon-burger", "veggie-burger", "kids-burger",
                                         "house-salad", "caesar", "cobb", "greek", "coffee", "tea", "soda", "juice",
                                         "lemonade", "draft-beer", "house-wine", "pancakes", "omelette",
                                         "burger-combo", "tshirt"};
    // How often each kind of thing happened, over all the days: each must
    // have, many times, for the agreement to mean something.
    std::map<std::string, int> seen;
    for (unsigned seed = 1; seed <= 20; ++seed) {
        INFO("day " << seed);
        std::mt19937 random(seed);
        const auto pick = [&](int n) { return int(random() % unsigned(std::max(1, n))); };
        const auto chance = [&](int percent) { return pick(100) < percent; };

        auto store = openStore();
        PosService &pos = *store;
        core::PosSettings &s = pos.shared()->settings;
        // Some days: pennies gone (cash to the nearest nickel), takeout food
        // untaxed, gratuity taxed.
        s.tax.cashRoundingCents = seed % 4 == 0 ? 5 : 0;
        s.tax.taxTakeoutFood = seed % 3 != 0;
        s.tax.taxGratuity = seed % 2 == 0;
        REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"T-Shirt"_s}, {u"price"_s, u"19.99"_s}, {u"family"_s, u"drinks"_s},
                                      {u"taxClass"_s, u"merchandise"_s}}));
        for (core::MenuItem &m : pos.shared()->menu) {
            if (m.name == "T-Shirt")
                m.id = "tshirt";
            if (m.id == "house-wine")
                m.noDiscount = true;   // a wine that's never discounted
        }

        Money refunded;
        for (int n = 0; n < 25; ++n) {
            const bool practice = chance(5);
            if (practice)
                REQUIRE(pos.setTraining(true));
            if (chance(10)) {   // a gift card sold, alone or with food
                REQUIRE(pos.sellGiftCard({}, 500 + pick(10) * 500));
            } else {
                REQUIRE(pos.startCheck(chance(70) ? core::CheckType::Takeout : core::CheckType::Quick));
            }
            const qint64 id = currentId(pos);
            const int lines = 1 + pick(4);
            for (int i = 0; i < lines; ++i) {
                REQUIRE(pos.addItem(QString::fromLatin1(menu[std::size_t(pick(int(menu.size())))])));
                choose(pos, pick);
                if (chance(15)) {   // two or three of it
                    const QVariantList ls = pos.lines();
                    pos.setLineQuantity(ls.last().toMap()[u"id"_s].toLongLong(), 2 + pick(2));
                }
            }
            if (chance(20)) {   // something sent, then voided
                REQUIRE(pos.sendOrder());
                pos.voidItem();
            }
            if (chance(10))
                pos.setTaxExempt(u"CERT-"_s + QString::number(n));
            if (chance(10))
                pos.setGratuity(1800);
            if (chance(15))
                pos.tender(u"discount"_s);
            else if (chance(5))
                pos.tender(u"comp"_s);
            else if (chance(5))
                pos.tender(u"staff-meal"_s);
            if (chance(15))
                pos.applyCoupon(chance(50) ? u"SAVE5"_s : u"BEER50"_s);
            // Paid: in one go or split, cash or card, maybe a tip on the card.
            if (chance(25)) {
                pos.tender(u"cash"_s, 500);
            }
            if (chance(30)) {
                pos.tender(u"credit"_s);
                if (chance(50))
                    pos.addTip(1500 + pick(3) * 250);
            }
            pos.tender(u"cash"_s, chance(30) ? std::optional<std::int64_t>(10000) : std::nullopt);
            // $100 in cash may not be enough (a big gift card): the rest, exactly.
            if (pos.shared()->open.contains(id) && pos.shared()->open.at(id).totals(s.tax).balance.cents() > 0)
                REQUIRE(pos.tender(u"cash"_s));
            REQUIRE(pos.closeCheck());
            if (practice) {   // gone when closed: in no report, no drawer
                ++seen["practice checks"];
                CHECK(std::ranges::none_of(pos.shared()->closedToday, [&](const core::Check &c) { return c.id == id; }));
                REQUIRE(pos.setTraining(false));
                continue;
            }
            // Now and then, part of a payment given back.
            const core::Check &c = closedCheck(pos, id);
            for (const core::Payment &p : c.payments) {
                if ((p.kind == core::TenderKind::Cash || p.kind == core::TenderKind::Card) && chance(8)) {
                    const qint64 back = std::min<qint64>(100, c.refundable(p, s.tax).cents());
                    if (back > 0 && pos.refundPayment(c.id, p.id, back, u"Test"_s))
                        refunded += Money::fromCents(back);
                    break;
                }
            }
        }

        // --- every check on its own -------------------------------------------------------------
        Money items, giftCards, discounts, net, tax, total, change, rounding, tips, collected;
        std::map<core::TaxClass, Money> taxable, taxBy;
        std::int64_t checks = 0, cash = 0;
        int voids = 0;
        Money voidValue;
        for (const core::Check &c : pos.shared()->closedToday) {
            if (c.training)
                continue;
            ++checks;
            const core::Totals t = c.totals(s.tax);
            INFO("check " << c.id);
            CHECK(t.subtotal == t.items - t.discounts);
            CHECK(t.discounts.cents() >= 0);
            CHECK(t.discounts <= t.items - t.giftCardsSold);
            CHECK(t.total == t.subtotal + t.tax + t.gratuity);
            CHECK(t.balance.cents() <= 0);                          // closed only when paid
            CHECK(t.change == -t.balance);
            // Each tax: its rate on what it's on, rounded once; nothing on a certificate.
            Money taxSum;
            for (const auto &[cls, base] : t.taxableByClass) {
                std::int64_t want = taxOn(base.cents(), s.tax.ratePpm(cls));
                if (cls == core::TaxClass::Food)
                    want += taxOn(t.taxableGratuity.cents(), s.tax.foodPpm);
                CHECK(t.taxByClass.contains(cls));
                if (t.taxByClass.contains(cls))
                    CHECK(t.taxByClass.at(cls).cents() == want);
            }
            for (const auto &[cls, m] : t.taxByClass)
                taxSum += m;
            CHECK(taxSum == t.tax);
            if (!c.taxExempt.empty())
                CHECK(t.tax.cents() == 0);
            // What each class is taxed on: no more than its items, and never
            // less than what may not be discounted.
            std::map<core::TaxClass, Money> lineTotal, notDiscounted;
            for (const core::OrderLine &l : c.lines) {
                if (l.voided) {   // what it would have been
                    core::OrderLine live = l;
                    live.voided = false;
                    voidValue += live.total();
                    ++voids;
                }
                if (l.voided || l.taxClass == core::TaxClass::None)
                    continue;
                lineTotal[l.taxClass] += l.total();
                if (l.noDiscount)
                    notDiscounted[l.taxClass] += l.total();
            }
            for (const auto &[cls, base] : t.taxableByClass) {
                CHECK(base <= lineTotal[cls]);
                CHECK(base >= notDiscounted[cls]);
            }
            // What this check had in it.
            const auto has = [&](bool yes, const char *what) { if (yes) ++seen[what]; };
            has(t.discounts.cents() > 0, "discounts");
            has(t.staffMeals.cents() > 0, "staff meals");
            has(std::ranges::any_of(c.payments, [](const core::Payment &p) { return p.reference.starts_with("promo:"); }), "coupons");
            has(std::ranges::any_of(c.payments, [](const core::Payment &p) { return !p.classShares.empty(); }), "coupons by tax class");
            has(std::ranges::any_of(c.lines, [](const core::OrderLine &l) { return l.voided; }), "voids");
            has(!c.refunds.empty(), "refunds");
            has(!c.taxExempt.empty(), "tax exempt");
            has(t.gratuity.cents() > 0, "gratuity");
            has(t.taxableGratuity.cents() > 0, "taxed gratuity");
            has(t.tips.cents() > 0, "card tips");
            has(t.rounding.cents() != 0, "cash rounding");
            has(t.giftCardsSold.cents() > 0, "gift cards sold");
            has(t.change.cents() > 0, "change given");
            has(std::ranges::count_if(c.payments, [](const core::Payment &p) { return p.kind != core::TenderKind::Discount; }) > 1,
                "split payments");
            has(t.taxableByClass.size() > 1, "two tax classes");
            has(t.taxableByClass.contains(core::TaxClass::Merchandise), "merchandise");
            has(std::ranges::any_of(c.lines, [](const core::OrderLine &l) { return l.quantity > 1; }), "quantities");
            has(std::ranges::any_of(c.lines, [](const core::OrderLine &l) {
                    return std::ranges::any_of(l.modifiers, [](const core::Modifier &m) { return m.unitPrice.cents() > 0; });
                }), "choices that cost extra");
            has(c.type == core::CheckType::Takeout && !s.tax.taxTakeoutFood && t.items.cents() > 0, "untaxed takeout food");
            items += t.items - t.giftCardsSold;
            giftCards += t.giftCardsSold;
            discounts += t.discounts;
            net += t.netSales();
            tax += t.tax;
            total += t.total;
            change += t.change;
            rounding += t.rounding;
            tips += t.tips;
            for (const auto &[cls, m] : t.taxableByClass)
                taxable[cls] += m;
            for (const auto &[cls, m] : t.taxByClass)
                taxBy[cls] += m;
            for (const core::Payment &p : c.payments) {
                if (p.kind != core::TenderKind::Discount)
                    collected += p.amount;
                if (p.kind == core::TenderKind::Cash)
                    cash += p.amount.cents();
            }
        }
        collected -= change;

        // --- the Sales Summary: the same, line by line ------------------------------------------
        const core::Report sales = pos.buildReport(u"sales"_s);
        CHECK(amount(sales, "Checks") == checks);
        CHECK(amount(sales, "Item sales") == items.cents());
        CHECK(amount(sales, "Discounts & comps") == -discounts.cents());
        CHECK(amount(sales, "Net sales") == net.cents());
        CHECK(net == items - discounts);
        Money taxableAll;
        for (const auto &[cls, m] : taxable) {
            CHECK(amount(sales, "Taxable " + core::toString(cls) + " sales") == m.cents());
            taxableAll += m;
        }
        CHECK(amount(sales, "Non-taxable sales") == (net - taxableAll).cents());
        for (const auto &[cls, m] : taxBy) {
            std::string label = core::toString(cls);
            label[0] = char(std::toupper(label[0]));
            CHECK(amount(sales, label + " tax") == m.cents());
        }
        CHECK(amount(sales, "Sales tax") == tax.cents());
        CHECK(amount(sales, "Total with tax") == total.cents());
        CHECK(amount(sales, "Collected") == collected.cents());
        CHECK(collected == total + rounding);                       // every check paid in full
        CHECK(sales.find("Collected and the total differ by").empty());
        if (voids > 0)
            CHECK(amount(sales, "Voided items (" + std::to_string(voids) + ")") == voidValue.cents());
        if (refunded.cents() > 0) {
            CHECK(amount(sales, "Refunds") == -refunded.cents());
            CHECK(amount(sales, "Collected after refunds") == (collected + tips - refunded).cents());
        }

        // --- by item, by category, and the journal -----------------------------------------------
        const core::Report byItem = pos.buildReport(u"items"_s);
        CHECK(amount(byItem, "Net sales") == net.cents());
        CHECK(byItem.find("All items").at(2) == value(sales, "Item sales"));
        CHECK(cents(pos.buildReport(u"categories"_s).find("Net sales").at(2)) == net.cents());
        const std::vector<std::string> journal = pos.buildReport(u"accounting"_s).find("Total");
        REQUIRE(journal.size() == 3);
        CHECK(journal[1] == journal[2]);

        // --- the drawer: what's in it is what came in less what went out --------------------------
        Money cashRefunds;
        for (const core::Check &c : pos.shared()->closedToday)
            for (const core::Refund &r : c.refunds)
                if (r.method == "cash")
                    cashRefunds += r.amount;
        const Money expected = core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday, s.tax);
        CHECK(expected.cents() == 10000 + cash - change.cents() - cashRefunds.cents());
    }
    for (const char *what : {"discounts", "staff meals", "coupons", "coupons by tax class", "voids", "refunds", "tax exempt",
                             "gratuity", "taxed gratuity", "card tips", "cash rounding", "gift cards sold", "change given",
                             "split payments", "two tax classes", "merchandise", "quantities", "choices that cost extra",
                             "untaxed takeout food", "practice checks"}) {
        INFO(what << ": " << seen[what] << " checks");
        CHECK(seen[what] >= 5);
    }
}
