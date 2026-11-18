#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "print/document.hh"
#include "print/tickets.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QDeadlineTimer>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The food truck (prep counts, a rush, the menu board) and retail (returns
// and exchanges, receipts with a barcode, price checks, sizes and colors).

namespace {

app::PosData truckData()
{
    app::PosData d = test::seedPosData();
    d.settings.serviceMode = "foodTruck";
    d.settings.truckNoLogin = true;
    return d;
}

QVariantMap prepOf(PosService &pos, const QString &item)
{
    for (const QVariant &v : pos.prepCounts())
        if (v.toMap()[u"item"_s] == item)
            return v.toMap();
    return {};
}

const core::MenuItem *itemOf(app::PosShared &shared, const std::string &id)
{
    for (const core::MenuItem &m : shared.menu)
        if (m.id == id)
            return &m;
    return nullptr;
}

// Rung in, sent, paid in cash and closed.
void sell(PosService &pos, const QString &item, int times = 1)
{
    for (int i = 0; i < times; ++i)
        REQUIRE(pos.addItem(item));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Ann"_s}}));   // a truck's unpaid order waits for a name
    REQUIRE(pos.sendOrder());
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
}

} // namespace

TEST_CASE("Truck: prep counts go down as orders go out, and sell out at none", "[truckretail][prep]")
{
    app::PosShared shared(truckData(), nullptr);
    PosService pos(&shared, u"Truck"_s);

    REQUIRE(pos.setPrepCount(u"soda"_s, 5));
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 5);
    // Kept with the settings (a restart keeps the count).
    const core::PosSettings saved = app::settingsFromJson(app::toJson(shared.settings));
    REQUIRE(saved.prepCounts.size() == 1);
    CHECK(saved.prepCounts[0].made == 5);

    sell(pos, u"soda"_s, 2);
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 3);
    CHECK(prepOf(pos, u"soda"_s)[u"low"_s] == true);   // at or under Store Settings' 10
    CHECK(pos.stockLeft()[u"soda"_s] == 3);             // "3 left" on its button

    // A void of one not made yet gives it back.
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Bo"_s}}));
    REQUIRE(pos.sendOrder());
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 2);
    pos.invoke(u"voidItem"_s, {});            // why, and a manager's OK
    REQUIRE(pos.giveReason(u"Rang the wrong one"_s));
    REQUIRE(pos.approve(u"1234"_s));
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 3);
    pos.releaseCheck();

    // More made: +2.
    REQUIRE(pos.addPrepped(u"soda"_s, 2));
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 5);

    // The last ones go: sold out, and everyone's screens hear it.
    sell(pos, u"soda"_s, 5);
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 0);
    CHECK_FALSE(itemOf(shared, "soda")->available);
    bool told = false;
    for (const QVariant &v : pos.messages())
        told = told || v.toMap()[u"text"_s].toString().startsWith(u"86: "_s);
    CHECK(told);
    CHECK_FALSE(pos.addItem(u"soda"_s));

    // Made more: back on.
    REQUIRE(pos.addPrepped(u"soda"_s, 4));
    CHECK(itemOf(shared, "soda")->available);
    CHECK(prepOf(pos, u"soda"_s)[u"left"_s].toInt() == 4);

    // Not counted any more.
    REQUIRE(pos.setPrepCount(u"soda"_s, -1));
    CHECK(pos.prepCounts().isEmpty());
    CHECK(itemOf(shared, "soda")->available);

    CHECK_FALSE(pos.setPrepCount(u"nothing-like-it"_s, 5));
}

TEST_CASE("Truck: zero prepped sells it out, End of Day clears the counts and the rush", "[truckretail][prep][rush]")
{
    app::PosData d = truckData();
    d.settings.cashMode = app::settingsFromJson(test::readSeed("pos/settings.json").object()).cashMode;
    app::PosShared shared(d, nullptr);
    std::int64_t clock = 1'700'000'000'000;
    shared.setClock([&clock] { return clock; });
    PosService pos(&shared, u"Truck"_s);

    REQUIRE(pos.setPrepCount(u"soda"_s, 0));
    CHECK_FALSE(itemOf(shared, "soda")->available);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // Rush is a manager's
    REQUIRE(pos.setRush(true));
    CHECK(pos.serviceInfo()[u"rush"_s] == true);
    clock += 3'600'000;
    INFO(pos.dayInfo()[u"blockers"_s].toStringList().join(u" | "_s).toStdString());
    REQUIRE(pos.endOfDay());
    CHECK(shared.settings.prepCounts.empty());
    CHECK_FALSE(shared.settings.rush);
    CHECK(itemOf(shared, "soda")->available);   // counted out yesterday: back today
}

TEST_CASE("Truck: a rush skips the receipt, and Pay sends first", "[truckretail][rush]")
{
    app::PosShared shared(truckData(), nullptr);
    PosService pos(&shared, u"Truck"_s);
    PosService other(&shared, u"Truck 2"_s);

    CHECK_FALSE(pos.setRush(true));                  // the Counter: a manager's call
    pos.invoke(u"setRush"_s, {true});
    REQUIRE(pos.approve(u"1234"_s));                 // the manager's PIN
    CHECK(other.serviceInfo()[u"rush"_s] == true);   // every screen

    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Cy"_s}}));
    REQUIRE(pos.rushSend());
    for (const QVariant &v : pos.lines())
        CHECK(v.toMap()[u"sent"_s] == true);
    REQUIRE(pos.rushSend());   // nothing new: nothing happens

    // Not a rush: Pay doesn't send.
    pos.invoke(u"setRush"_s, {false});
    REQUIRE(pos.approve(u"1234"_s));
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Cy"_s}}));
    REQUIRE(pos.rushSend());
    CHECK(pos.lines().last().toMap()[u"sent"_s] == false);
}

TEST_CASE("Truck: the menu board shows the menu, crossed off when sold out", "[truckretail][menuboard]")
{
    app::PosData d = truckData();
    for (core::MenuItem &m : d.menu)
        if (m.id == "coffee")
            m.kioskHide = true;
    app::PosShared shared(d, nullptr);
    PosService pos(&shared, u"Board"_s);
    pos.logout();

    REQUIRE(pos.kitchenSoldOut(u"soda"_s));
    const QVariantMap board = pos.menuBoard();
    CHECK_FALSE(board[u"families"_s].toStringList().isEmpty());
    bool soda = false, coffee = false, modifier = false;
    for (const QVariant &v : board[u"items"_s].toList()) {
        const QVariantMap i = v.toMap();
        if (i[u"id"_s] == u"soda"_s) {
            soda = true;
            CHECK(i[u"available"_s] == false);
            CHECK_FALSE(i[u"price"_s].toString().isEmpty());
        }
        coffee = coffee || i[u"id"_s] == u"coffee"_s;
        const core::MenuItem *m = itemOf(shared, i[u"id"_s].toString().toStdString());
        modifier = modifier || (m && m->isModifier);
    }
    CHECK(soda);
    CHECK_FALSE(coffee);     // kept off the kiosk: off the board
    CHECK_FALSE(modifier);   // choices aren't on it
    CHECK(board[u"showOrders"_s] == false);
}

// --- retail ---
namespace {
struct TagPrinter : app::PosPrinter {
    std::vector<std::pair<std::string, std::vector<std::string>>> labels;
    std::vector<std::string> barcodes;
    void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
    void printReceipt(const core::PosSettings &, const core::Check &, const std::string &) override {}
    void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
    void openDrawer(const core::PosSettings &, const std::string &) override {}
    void printLabels(const core::PosSettings &, const std::vector<std::pair<std::string, std::vector<std::string>>> &l,
                     const std::string &, const std::vector<std::string> &b = {}) override
    {
        labels = l;
        barcodes = b;
    }
};
} // namespace

namespace {

// A shop: the soda counts its own stock (10 on the shelf).
app::PosData shopData()
{
    app::PosData d = test::seedPosData();
    d.settings.serviceMode = "retail";
    core::Ingredient own{"item-soda", "Soda", "each"};
    own.onHand = 10;
    d.ingredients.push_back(own);
    for (core::MenuItem &m : d.menu)
        if (m.id == "soda")
            m.recipe = {{"item-soda", 1}};
    return d;
}

struct Shop {
    app::PosShared shared{shopData(), nullptr};
    PosService pos{&shared, u"Shop"_s};
    Shop()
    {
        REQUIRE(pos.loginWithPin(u"1234"_s));   // a manager: returns are theirs
        pos.openDrawerSession();
    }
    double onHand(const std::string &id) { return shared.ingredient(id)->onHand; }
    // Rung in (n of the item) and paid: the sale's id.
    qint64 sell(const QString &item, int n, const QString &tender = u"cash"_s)
    {
        if (n > 1)
            REQUIRE(pos.setNextQuantity(n));
        REQUIRE(pos.addItem(item));
        const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
        REQUIRE(pos.tender(tender));
        REQUIRE(pos.closeCheck());
        return id;
    }
    QVariantMap saleLine(int index = 0) { return pos.returnSale()[u"lines"_s].toList().value(index).toMap(); }
    core::Check &lastClosed() { return shared.closedToday.back(); }
};

} // namespace

TEST_CASE("Retail: a return from the receipt, cash back, and back on the shelf", "[truckretail][returns]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 3);
    CHECK(s.onHand("item-soda") == 7);

    REQUIRE(s.pos.startReturn(sale));
    REQUIRE(s.saleLine()[u"left"_s].toInt() == 3);
    const qint64 line = s.saleLine()[u"id"_s].toLongLong();

    CHECK_FALSE(s.pos.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 2}}}, QString()));   // why?
    CHECK_FALSE(s.pos.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 4}}}, u"Changed mind"_s));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 2}}}, u"Changed mind"_s));
    const QVariantMap info = s.pos.checkInfo();
    CHECK_FALSE(info[u"refundDue"_s].toString().isEmpty());
    CHECK(info[u"refundCard"_s].toString().isEmpty());   // paid in cash
    CHECK(s.pos.lines().first().toMap()[u"quantity"_s].toInt() == -2);
    // The sale's tax comes back too.
    const core::Totals t = s.shared.open.at(info[u"id"_s].toLongLong()).totals(s.shared.settings.tax);
    CHECK(t.items.cents() == -590);
    CHECK(t.tax.cents() < 0);
    CHECK(t.total == t.items + t.tax);   // nothing "discounted" off a return

    CHECK_FALSE(s.pos.closeCheck());   // owed back first
    CHECK_FALSE(s.pos.refundReturn(u"card"_s));
    REQUIRE(s.pos.refundReturn(u"cash"_s));
    CHECK(s.lastClosed().hasReturns());
    CHECK(s.lastClosed().payments.back().amount == t.total);   // cash out: below zero
    CHECK(s.onHand("item-soda") == 9);

    // One left to bring back.
    REQUIRE(s.pos.startReturn(sale));
    CHECK(s.saleLine()[u"left"_s].toInt() == 1);
    CHECK_FALSE(s.pos.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 2}}}, u"Again"_s));
    s.pos.cancelReturn();
    CHECK(s.pos.returnSale().isEmpty());

    // An open check isn't a sale.
    REQUIRE(s.pos.addItem(u"soda"_s));
    CHECK_FALSE(s.pos.startReturn(s.pos.checkInfo()[u"id"_s].toLongLong()));
}

TEST_CASE("Retail: store credit, damaged items, and a return with no receipt", "[truckretail][returns]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 2);
    REQUIRE(s.pos.startReturn(sale));
    const qint64 line = s.saleLine()[u"id"_s].toLongLong();
    // Damaged: not back on the shelf.
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}, {u"restock"_s, false}}}, u"Damaged"_s));
    const std::size_t cards = s.shared.giftCards.size();
    REQUIRE(s.pos.refundReturn(u"credit"_s));
    REQUIRE(s.shared.giftCards.size() == cards + 1);
    // The new card: what came back, with its tax.
    const core::Check &credit = s.lastClosed();
    const auto cardLine = std::ranges::find_if(credit.lines, [](const core::OrderLine &l) { return l.isGiftCard(); });
    REQUIRE(cardLine != credit.lines.end());
    CHECK(s.shared.giftCard(cardLine->giftCardNumber())->balance.cents() > 295);
    CHECK(s.onHand("item-soda") == 8);   // still 8: thrown away
    CHECK(s.lastClosed().totals(s.shared.settings.tax).balance.cents() == 0);

    // No receipt: a manager's, at today's price.
    REQUIRE(s.pos.returnItem(u"soda"_s, 0, u"Gift"_s));
    REQUIRE(s.pos.refundReturn(u"cash"_s));
    CHECK(s.onHand("item-soda") == 9);
    CHECK(s.lastClosed().lines.front().unitPrice.cents() == 295);
}

TEST_CASE("Retail: an exchange pays the difference; a card sale's return goes back to the card", "[truckretail][returns]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 1, u"credit"_s);
    REQUIRE(s.pos.startReturn(sale));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 1}}}, u"Wrong one"_s));
    CHECK_FALSE(s.pos.checkInfo()[u"refundCard"_s].toString().isEmpty());
    // Exchanged for a juice (3.50): the difference is owed, paid as usual.
    REQUIRE(s.pos.addItem(u"juice"_s));
    CHECK(s.pos.checkInfo()[u"refundDue"_s].toString().isEmpty());
    REQUIRE(s.pos.tender(u"cash"_s));
    REQUIRE(s.pos.closeCheck());
    CHECK(s.lastClosed().hasReturns());

    // Another, sold by card and brought back: to the card (recorded here).
    const qint64 second = s.sell(u"soda"_s, 1, u"credit"_s);
    REQUIRE(s.pos.startReturn(second));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    REQUIRE(s.pos.refundReturn(u"card"_s));
    CHECK(s.lastClosed().payments.back().kind == core::TenderKind::Card);
    CHECK(s.lastClosed().payments.back().amount.cents() < 0);
}

TEST_CASE("Retail: a cashier needs a manager for a return, unless the store lets them", "[truckretail][returns]")
{
    app::PosData d = shopData();
    app::PosShared shared(d, nullptr);
    PosService boss(&shared, u"Shop"_s);
    REQUIRE(boss.loginWithPin(u"1234"_s));
    boss.openDrawerSession();
    REQUIRE(boss.addItem(u"soda"_s));
    const qint64 sale = boss.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(boss.tender(u"cash"_s));
    REQUIRE(boss.closeCheck());

    PosService cashier(&shared, u"Shop 2"_s);
    REQUIRE(cashier.loginWithPin(u"1111"_s));
    REQUIRE(cashier.startReturn(sale));
    const qint64 line = cashier.returnSale()[u"lines"_s].toList().first().toMap()[u"id"_s].toLongLong();
    CHECK_FALSE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    CHECK_FALSE(cashier.returnItem(u"soda"_s, 0, u"No receipt"_s));
    shared.settings.cashierReturns = true;
    // The sale's number typed (a guess would do): still a manager's.
    CHECK_FALSE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    CHECK(cashier.scanCode(u"R%1-AAAAAA"_s.arg(sale)) == u"return"_s);   // a made-up code: the same
    CHECK_FALSE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    // The receipt's own barcode: the cashier may.
    const QString code = QString::fromStdString(app::receiptCode(shared.settings.receiptKey, sale));
    REQUIRE(code.size() == 6);
    CHECK(cashier.scanCode(u"R%1-%2"_s.arg(sale).arg(code)) == u"return"_s);
    REQUIRE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    CHECK_FALSE(cashier.returnItem(u"soda"_s, 0, u"No receipt"_s));   // still a manager's
}

TEST_CASE("Retail: the receipt's barcode brings the sale up for a return", "[truckretail][receiptbarcode]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 1);
    print::TicketContext ctx{s.shared.settings, [](std::int64_t) { return std::string("1/1"); },
                             [](std::int64_t) { return std::string("12:00"); }, 0};
    const print::Document doc = print::receipt(s.lastClosed(), ctx);
    const auto barcode = std::ranges::find(doc.lines, print::Document::Kind::Barcode, &print::Document::Line::kind);
    REQUIRE(barcode != doc.lines.end());
    // The sale's number and its code, made with the store's own secret.
    CHECK(barcode->text == "R" + std::to_string(sale) + "-" + app::receiptCode(s.shared.settings.receiptKey, sale));
    CHECK(app::receiptCode(s.shared.settings.receiptKey, sale) != app::receiptCode(s.shared.settings.receiptKey, sale + 1));
    CHECK(app::receiptCode("another store", sale) != app::receiptCode(s.shared.settings.receiptKey, sale));

    CHECK(s.pos.scanCode(QString::fromStdString(barcode->text)) == u"return"_s);
    CHECK(s.pos.returnSale()[u"id"_s].toLongLong() == sale);

    // A restaurant's receipt: none.
    s.shared.settings.serviceMode = "restaurant";
    const print::Document plain = print::receipt(s.lastClosed(), ctx);
    CHECK(std::ranges::none_of(plain.lines, [](const print::Document::Line &l) { return l.kind == print::Document::Kind::Barcode; }));
}

TEST_CASE("Retail: price check shows the price and stock, rings nothing in", "[truckretail][pricecheck]")
{
    Shop s;
    REQUIRE(s.pos.setPriceCheck(true));
    CHECK(s.pos.priceCheck()[u"on"_s] == true);
    REQUIRE(s.pos.priceCheckCode(u"303"_s));   // the soda's number
    const QVariantMap p = s.pos.priceCheck();
    CHECK(p[u"name"_s] == u"Soda"_s);
    CHECK(p[u"price"_s] == u"$2.95"_s);
    CHECK(p[u"stock"_s].toInt() == 10);
    CHECK_FALSE(s.pos.hasCheck());
    CHECK_FALSE(s.pos.priceCheckCode(u"nothing-1234"_s));
    CHECK_FALSE(s.pos.priceCheck()[u"error"_s].toString().isEmpty());
    REQUIRE(s.pos.setPriceCheck(false));
    CHECK(s.pos.priceCheck().isEmpty());
}

TEST_CASE("Retail: sizes and colors, each with its barcode, price and stock", "[truckretail][variants]")
{
    Shop s;
    const QVariantList axes{QVariantMap{{u"name"_s, u"Size"_s}, {u"values"_s, QVariantList{u"S"_s, u"M"_s}}},
                            QVariantMap{{u"name"_s, u"Color"_s}, {u"values"_s, QVariantList{u"Red"_s, u"Blue"_s}}}};
    const QVariantList sizes{QVariantMap{{u"key"_s, u"M|Red"_s}, {u"barcode"_s, u"0001234500"_s}, {u"price"_s, u"3.25"_s}},
                             QVariantMap{{u"key"_s, u"S|Blue"_s}, {u"hidden"_s, true}}};
    REQUIRE(s.pos.setItemVariants(u"soda"_s, axes, sizes));
    const core::MenuItem *soda = itemOf(s.shared, "soda");
    REQUIRE(soda->variants.size() == 4);
    REQUIRE(soda->variant("M|Red"));
    CHECK(soda->variant("M|Red")->price.cents() == 325);
    // Its own stock each (the item counts its own).
    REQUIRE(s.shared.ingredient("item-soda|M|Red"));
    REQUIRE(s.pos.setVariantStock(u"soda"_s, u"M|Red"_s, 4));

    // Kept as it was saved.
    const core::MenuItem back = app::menuFromJson(QJsonArray{app::toJson(*soda)}).front();
    CHECK(back.variants == soda->variants);
    CHECK(back.variantAxes == soda->variantAxes);

    // Touched: which one? (the grid); then M Red.
    REQUIRE(s.pos.addItem(u"soda"_s));
    CHECK(s.pos.lines().isEmpty());
    REQUIRE(s.pos.variantAsk()[u"cells"_s].toList().size() == 4);
    CHECK_FALSE(s.pos.addVariant(u"soda"_s, u"S|Blue"_s));   // not sold
    REQUIRE(s.pos.addVariant(u"soda"_s, u"M|Red"_s));
    CHECK(s.pos.variantAsk().isEmpty());
    CHECK(s.pos.lines().first().toMap()[u"name"_s].toString().contains(u"M, Red"_s));
    // Scanned: that one, straight in.
    REQUIRE(s.pos.addItemByCode(u"1234500"_s));   // a scanner dropped the leading zeros
    CHECK(s.pos.lines().size() == 2);
    REQUIRE(s.pos.tender(u"cash"_s));
    REQUIRE(s.pos.closeCheck());
    CHECK(s.onHand("item-soda|M|Red") == 2);
    CHECK(s.onHand("item-soda") == 10);   // the item's own count isn't used

    // Price check: every size's stock.
    REQUIRE(s.pos.priceCheckCode(u"0001234500"_s));
    CHECK(s.pos.priceCheck()[u"price"_s] == u"$3.25"_s);
    CHECK(s.pos.priceCheck()[u"stock"_s].toInt() == 2);

    // Price tags: one for each size and color sold, with its barcode and price.
    TagPrinter tags;
    s.pos.setPrinter(&tags);
    REQUIRE(s.pos.printPriceTags({u"soda"_s}, 1));
    REQUIRE(tags.labels.size() == 3);   // S Blue isn't sold
    const auto red = std::ranges::find(tags.barcodes, std::string("0001234500"));
    REQUIRE(red != tags.barcodes.end());
    const auto &redTag = tags.labels[std::size_t(red - tags.barcodes.begin())];
    CHECK(redTag.first == "Soda (M, Red)");
    CHECK(redTag.second.front() == "$3.25");

    // Another item's barcode isn't one of these.
    CHECK_FALSE(s.pos.setItemVariants(u"juice"_s, {QVariantMap{{u"values"_s, QVariantList{u"Big"_s}}}},
                                      {QVariantMap{{u"key"_s, u"Big"_s}, {u"barcode"_s, u"0001234500"_s}}}));
}

// --- returns: what a cashier (or anyone) must not be able to do --------------------------------

TEST_CASE("Returns: a card return counts against the sale's refunds, and the other way round", "[truckretail][returns][security]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 2, u"credit"_s);
    const qint64 cardId = s.lastClosed().payments.front().id;
    REQUIRE(s.pos.startReturn(sale));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 2}}}, u"Changed mind"_s));
    REQUIRE(s.pos.refundReturn(u"card"_s));
    // Everything came back to the card: Find a Check -> Refund has nothing left to give.
    const std::optional<core::Check> sold = s.shared.saleById(sale);
    REQUIRE(sold);
    CHECK(sold->payments.front().returnedBack.cents() > 0);
    CHECK_FALSE(s.pos.refundPayment(sale, cardId, 0, u"Again"_s));

    // The other way: a sale refunded first, then brought back: a manager's call.
    const qint64 second = s.sell(u"soda"_s, 1, u"credit"_s);
    REQUIRE(s.pos.refundPayment(second, s.lastClosed().payments.front().id, 0, u"Refunded"_s));
    s.shared.settings.cashierReturns = true;
    PosService cashier(&s.shared, u"Shop 2"_s);
    REQUIRE(cashier.loginWithPin(u"1111"_s));
    const QString code = QString::fromStdString(app::receiptCode(s.shared.settings.receiptKey, second));
    REQUIRE(cashier.scanCode(u"R%1-%2"_s.arg(second).arg(code)) == u"return"_s);
    const qint64 line = cashier.returnSale()[u"lines"_s].toList().first().toMap()[u"id"_s].toLongLong();
    CHECK_FALSE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
}

TEST_CASE("Returns: nothing else happens on the check while its card refund is on its way", "[truckretail][returns][security]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 1, u"credit"_s);
    // As if taken on a Stripe reader.
    for (core::Check &c : s.shared.closedToday)
        if (c.id == sale) {
            c.payments.front().processor = "stripe";
            c.payments.front().reference = "pi_test";
        }
    s.shared.settings.stripeSecretKey = "sk_test";
    std::function<void(const QString &, const QString &)> answer;
    s.shared.stripeRefund = [&](const QString &, std::int64_t , const QString &, std::function<void(const QString &, const QString &)> done) {
        answer = std::move(done);
    };
    REQUIRE(s.pos.startReturn(sale));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    REQUIRE(s.pos.refundReturn(u"card"_s));
    REQUIRE(answer);
    CHECK_FALSE(s.pos.refundReturn(u"cash"_s));    // not the cash too
    CHECK_FALSE(s.pos.refundReturn(u"credit"_s));
    CHECK_FALSE(s.pos.closeCheck());
    CHECK_FALSE(s.pos.addItem(u"juice"_s));
    CHECK_FALSE(s.pos.voidItem());
    answer(u"re_1"_s, QString());
    CHECK_FALSE(s.pos.hasCheck());   // refunded, then closed
    CHECK(s.lastClosed().payments.back().reference == "re_1");
}

TEST_CASE("Returns: an earlier day's sale can't come back twice from two screens", "[truckretail][returns][security]")
{
    test::RecordingSink sink;
    app::PosData d = shopData();
    app::PosShared shared(d, &sink);
    PosService a(&shared, u"Shop"_s), b(&shared, u"Shop 2"_s);
    REQUIRE(a.loginWithPin(u"1234"_s));
    REQUIRE(b.loginWithPin(u"1234"_s));
    a.openDrawerSession();
    b.openDrawerSession();
    REQUIRE(a.addItem(u"soda"_s));
    const qint64 sale = a.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(a.tender(u"cash"_s));
    REQUIRE(a.closeCheck());
    // Yesterday's now: only in the database.
    shared.closedToday.clear();
    shared.findChecks = [&sink](const app::PosShared::CheckFind &) {
        std::vector<core::Check> out;
        for (const auto &[id, c] : sink.checks)
            if (c.status == core::CheckStatus::Closed)
                out.push_back(c);
        return out;
    };
    const auto line = [&](PosService &p) { return p.returnSale()[u"lines"_s].toList().first().toMap()[u"id"_s].toLongLong(); };
    REQUIRE(a.startReturn(sale));
    REQUIRE(b.startReturn(sale));
    REQUIRE(a.returnLines({QVariantMap{{u"line"_s, line(a)}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    // The other screen: it's coming back on the first one already.
    CHECK_FALSE(b.returnLines({QVariantMap{{u"line"_s, line(b)}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    REQUIRE(a.refundReturn(u"cash"_s));
    CHECK(sink.checks.at(sale).lines.front().returned == 1);   // saved where it's kept
    REQUIRE(b.startReturn(sale));
    CHECK(b.returnSale()[u"lines"_s].toList().first().toMap()[u"left"_s].toInt() == 0);
    CHECK_FALSE(b.returnLines({QVariantMap{{u"line"_s, line(b)}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    // And again later (a search on the screen meanwhile changes nothing).
    REQUIRE(a.searchChecks(u"zz"_s, 0, 0));
    QDeadlineTimer done(10000);   // (the search runs away from the screen: let it finish)
    while (a.checkSearch()[u"loading"_s].toBool() && !done.hasExpired())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    REQUIRE_FALSE(a.checkSearch()[u"loading"_s].toBool());
    REQUIRE(a.startReturn(sale));
    CHECK(a.returnSale()[u"lines"_s].toList().first().toMap()[u"left"_s].toInt() == 0);
}

TEST_CASE("Returns: no choices or add-ons on what comes back; no-receipt prices are today's", "[truckretail][returns][security]")
{
    Shop s;
    const qint64 sale = s.sell(u"soda"_s, 1);
    REQUIRE(s.pos.startReturn(sale));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    const qint64 back = s.pos.lines().first().toMap()[u"id"_s].toLongLong();
    const QString due = s.pos.checkInfo()[u"refundDue"_s].toString();
    CHECK_FALSE(s.pos.chooseLine(back));
    for (const core::MenuItem &m : s.shared.menu)
        if (m.isModifier && m.price.cents() > 0) {
            s.pos.selectLine(back);
            s.pos.addItem(QString::fromStdString(m.id));
            break;
        }
    CHECK(s.pos.checkInfo()[u"refundDue"_s].toString() == due);   // not a cent more
    s.pos.releaseCheck();

    // A manager may type a price, within reason.
    CHECK_FALSE(s.pos.returnItem(u"soda"_s, 50'000, u"Gift"_s));
    REQUIRE(s.pos.returnItem(u"soda"_s, 500, u"Gift"_s));
    CHECK(s.pos.lines().last().toMap()[u"price"_s].toString() == u"-$5.00"_s);
    s.pos.releaseCheck();

    // A cashier's typed price is ignored, even with the manager's OK.
    PosService cashier(&s.shared, u"Shop 2"_s);
    REQUIRE(cashier.loginWithPin(u"1111"_s));
    cashier.invoke(u"returnItem"_s, {u"soda"_s, 50'000, u"Gift"_s, true});
    REQUIRE(cashier.approve(u"1234"_s));
    REQUIRE(cashier.hasCheck());
    CHECK(cashier.lines().last().toMap()[u"price"_s].toString() == u"-$2.95"_s);
}

TEST_CASE("Returns: cash back for a gift card sale is a manager's; cash is rounded where pennies are gone", "[truckretail][returns][security]")
{
    Shop s;
    s.shared.settings.cashierReturns = true;
    const qint64 sale = s.sell(u"soda"_s, 1);
    for (core::Check &c : s.shared.closedToday)
        if (c.id == sale) {
            c.payments.front().kind = core::TenderKind::GiftCard;
            c.payments.front().tenderName = "Gift Card";
        }
    PosService cashier(&s.shared, u"Shop 2"_s);
    REQUIRE(cashier.loginWithPin(u"1111"_s));
    cashier.openDrawerSession();
    const QString code = QString::fromStdString(app::receiptCode(s.shared.settings.receiptKey, sale));
    REQUIRE(cashier.scanCode(u"R%1-%2"_s.arg(sale).arg(code)) == u"return"_s);
    const qint64 line = cashier.returnSale()[u"lines"_s].toList().first().toMap()[u"id"_s].toLongLong();
    REQUIRE(cashier.returnLines({QVariantMap{{u"line"_s, line}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    CHECK_FALSE(cashier.refundReturn(u"cash"_s));     // a manager's
    REQUIRE(cashier.refundReturn(u"credit"_s));       // store credit: fine

    // Pennies gone: $3.19 back is handed over as $3.20, and the check closes.
    s.shared.settings.tax.cashRoundingCents = 5;
    const qint64 other = s.sell(u"soda"_s, 1);
    REQUIRE(s.pos.startReturn(other));
    REQUIRE(s.pos.returnLines({QVariantMap{{u"line"_s, s.saleLine()[u"id"_s]}, {u"qty"_s, 1}}}, u"Changed mind"_s));
    REQUIRE(s.pos.refundReturn(u"cash"_s));
    CHECK(s.lastClosed().payments.back().amount.cents() % 5 == 0);
    CHECK(s.lastClosed().totals(s.shared.settings.tax).balance.cents() == 0);
}

TEST_CASE("Returns: a coupon on an exchange is taxed on what was bought, not what came back", "[truckretail][returns][security]")
{
    core::TaxRates rates;
    rates.foodPpm = 100'000;   // 10%
    core::Check c;
    core::OrderLine sold;
    sold.itemId = "shirt";
    sold.unitPrice = Money::fromCents(5000);
    sold.taxClass = core::TaxClass::Food;
    c.lines.push_back(sold);
    core::OrderLine back = sold;
    back.unitPrice = Money::fromCents(3000);
    back.quantity = -1;
    back.noDiscount = true;
    c.lines.push_back(back);
    core::Tender coupon;
    coupon.kind = core::TenderKind::Discount;
    c.addPayment(coupon, Money()).amount = Money::fromCents(1000);
    const core::Totals t = c.totals(rates);
    CHECK(t.subtotal.cents() == 1000);
    CHECK(t.tax.cents() == 100);   // 10% of the $10 left, not of $16
}

TEST_CASE("Returns: without a receipt, on the Loss Watch", "[truckretail][returns][security]")
{
    Shop s;
    s.shared.settings.watch = true;
    REQUIRE(s.pos.returnItem(u"soda"_s, 0, u"Gift"_s));
    bool alerted = false;
    for (const QVariant &v : s.pos.lossAlertsNow())
        alerted = alerted || v.toMap()[u"kind"_s] == u"returnNoReceipt"_s;
    CHECK(alerted);
}
