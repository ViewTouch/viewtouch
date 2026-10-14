#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Customers, gift cards and house accounts.

namespace {

void openDrawer(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

QStringList names(const QVariantList &list)
{
    QStringList out;
    for (const QVariant &v : list)
        out << v.toMap()[u"name"_s].toString();
    return out;
}

QString rowAmount(const QVariantMap &report, const QString &first)
{
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (!cells.isEmpty() && cells.first() == first)
            return cells.last();
    }
    return {};
}

} // namespace

TEST_CASE("Customers: saved, found by phone or name, counted when their checks close", "[customers]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    CHECK_FALSE(pos.saveCustomer({}));                        // a name or a phone
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana Lee"_s}, {u"phone"_s, u"(555) 010-1234"_s},
                              {u"note"_s, u"no onions"_s}}));
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Alex Kim"_s}, {u"phone"_s, u"555-777-0000"_s}}));
    REQUIRE(pos.shared()->customers.size() == 2);

    REQUIRE(pos.findCustomers(u"0101"_s));                     // digits anywhere in the number
    CHECK(names(pos.customerResults()) == QStringList{u"Dana Lee"_s});
    REQUIRE(pos.findCustomers(u"kim"_s));
    CHECK(names(pos.customerResults()) == QStringList{u"Alex Kim"_s});
    // The same phone written differently is the same person.
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana Lee-Park"_s}, {u"phone"_s, u"555.010.1234"_s}}));
    CHECK(pos.shared()->customers.size() == 2);

    // A takeout for Dana: picked from the list, counted at close.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    const QString dana = QString::fromStdString(pos.shared()->customers.front().id);
    REQUIRE(pos.useCustomer(dana));
    CHECK(pos.checkInfo()[u"customer"_s].toMap()[u"name"_s] == u"Dana Lee-Park"_s);
    pos.addItem(u"cobb"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.selectCustomer(dana));
    CHECK(pos.customerInfo()[u"visits"_s] == 1);
    CHECK(pos.customerInfo()[u"spent"_s] == u"$13.53"_s);

    // A delivery typed in by hand goes on file too.
    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Robin"_s}, {u"phone"_s, u"555 222 3333"_s}, {u"address"_s, u"9 Elm"_s}}));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.findCustomers(u"5552223333"_s));
    REQUIRE(pos.customerResults().size() == 1);
    CHECK(pos.customerResults().first().toMap()[u"address"_s] == u"9 Elm"_s);
    CHECK(pos.customerResults().first().toMap()[u"visits"_s] == 1);

    // Reopening takes the visit back.
    REQUIRE(pos.reopenCheck(1));
    REQUIRE(pos.selectCustomer(dana));
    CHECK(pos.customerInfo()[u"visits"_s] == 0);
}

TEST_CASE("Gift cards: sold on a check, live once it's paid, spent, and given back", "[customers][giftcards]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);

    // Sell $50 on card 6000 1111 2222 (no check open: a quick one starts).
    CHECK_FALSE(pos.sellGiftCard(u"6000 1111 2222"_s, 0));    // no amount
    REQUIRE(pos.sellGiftCard(u"6000 1111 2222"_s, 5000));
    CHECK(pos.lines().size() == 1);
    CHECK(pos.totals()[u"tax"_s].toString() == u"$0.00"_s);              // no tax on gift cards
    CHECK(pos.kitchenTickets().isEmpty());
    REQUIRE(pos.lookupGiftCard(u"600011112222"_s));
    CHECK_FALSE(pos.giftCardInfo()[u"found"_s].toBool());     // not before it's paid
    REQUIRE(pos.tender(u"cash"_s));
    pos.sendOrder();                                           // nothing prints, nothing to make
    REQUIRE(pos.closeCheck());
    CHECK(pos.kitchenTickets().isEmpty());
    REQUIRE(pos.lookupGiftCard(u"600011112222"_s));
    CHECK(pos.giftCardInfo()[u"found"_s].toBool());
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$50.00"_s);

    // Spend $13.53 of it on a Cobb.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    REQUIRE(pos.tender(u"gift"_s));                           // the card looked up
    CHECK(pos.totals()[u"balance"_s].toString() == u"$0.00"_s);
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$36.47"_s);
    // Undo the payment: the money goes back on the card.
    REQUIRE(pos.removePayment());
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$50.00"_s);
    REQUIRE(pos.payWithGiftCard({}, 1000));                    // $10 of it
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$40.00"_s);

    // A card can't pay more than it holds, and an unknown one isn't accepted.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 5; ++i)
        pos.addItem(u"cobb"_s);                                // $67.66
    REQUIRE(pos.payWithGiftCard(u"600011112222"_s));
    CHECK(pos.totals()[u"balance"_s].toString() == u"$27.66"_s);
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$0.00"_s);
    CHECK_FALSE(pos.payWithGiftCard(u"600011112222"_s));       // empty now
    CHECK_FALSE(pos.payWithGiftCard(u"999999"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // Reload it; a card sold on a check that's reopened is taken back.
    REQUIRE(pos.sellGiftCard(u"600011112222"_s, 2500));
    CHECK(pos.lines().first().toMap()[u"name"_s].toString().contains(u"reload"_s));
    const qint64 reload = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$25.00"_s);
    REQUIRE(pos.reopenCheck(reload));
    CHECK(pos.giftCardInfo()[u"balance"_s] == u"$0.00"_s);

    // A new card with no number gets one.
    pos.releaseCheck();
    REQUIRE(pos.sellGiftCard({}, 2000));
    CHECK(pos.lines().last().toMap()[u"name"_s].toString().startsWith(u"Gift Card"_s));

    // The report: sold, spent, still on cards.
    const QVariantMap report = pos.report(u"accounts"_s);
    CHECK(rowAmount(report, u"Sold and reloaded"_s) == u"$50.00"_s);   // $50 + $25 - $25 taken back
    CHECK(rowAmount(report, u"Spent"_s) == u"$50.00"_s);
    CHECK(rowAmount(report, u"Still on cards (owed by the store)"_s) == u"$0.00"_s);

    // Saved with their history.
    const core::GiftCard &card = pos.shared()->giftCards.front();
    CHECK(app::giftCardFromJson(app::toJson(card)) == card);
    CHECK(card.history.size() >= 6);
}

TEST_CASE("House accounts: managers open them, charges stay under the limit, payments", "[customers][house]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));                      // Sam, a server
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Acme Office"_s}, {u"phone"_s, u"5550001111"_s}}));
    const QString acme = QString::fromStdString(pos.shared()->customers.front().id);
    CHECK_FALSE(pos.saveCustomer({{u"id"_s, acme}, {u"name"_s, u"Acme Office"_s}, {u"houseAccount"_s, true}}));
    pos.logout();

    openDrawer(pos);                                           // Morgan, a manager
    REQUIRE(pos.saveCustomer({{u"id"_s, acme}, {u"name"_s, u"Acme Office"_s}, {u"phone"_s, u"5550001111"_s},
                              {u"houseAccount"_s, true}, {u"accountLimit"_s, 3000}}));

    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    CHECK_FALSE(pos.tender(u"house"_s));                       // whose account?
    REQUIRE(pos.useCustomer(acme));
    REQUIRE(pos.tender(u"house"_s));                           // $13.53
    REQUIRE(pos.closeCheck());

    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"cobb"_s);                                    // $27.06: over the $30 limit
    REQUIRE(pos.useCustomer(acme));
    CHECK_FALSE(pos.tender(u"house"_s));
    REQUIRE(pos.tender(u"house"_s, 1000));                     // part of it fits
    REQUIRE(pos.removePayment());                              // and comes off again
    REQUIRE(pos.selectCustomer(acme));
    CHECK(pos.customerInfo()[u"balance"_s] == u"$13.53"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // A manager can't close the account while it's owed.
    CHECK_FALSE(pos.saveCustomer({{u"id"_s, acme}, {u"name"_s, u"Acme Office"_s}, {u"houseAccount"_s, false}}));

    // Paying: $5 in cash (into the drawer), the rest by card.
    const QString before = pos.drawerInfo()[u"expected"_s].toString();
    REQUIRE(pos.payOnAccount(u"cash"_s, 500));
    CHECK(pos.drawerInfo()[u"expected"_s].toString() != before);
    CHECK(pos.customerInfo()[u"balance"_s] == u"$8.53"_s);
    REQUIRE(pos.payOnAccount(u"card"_s));
    CHECK(pos.customerInfo()[u"balance"_s] == u"$0.00"_s);
    CHECK_FALSE(pos.payOnAccount(u"card"_s));                  // nothing owed
    CHECK(pos.customerInfo()[u"account"_s].toList().size() == 5);

    const QVariantMap report = pos.report(u"accounts"_s);
    CHECK(rowAmount(report, u"Charged"_s) == u"$13.53"_s);
    CHECK(rowAmount(report, u"Paid"_s) == u"$13.53"_s);
    CHECK(rowAmount(report, u"Owed to the store"_s) == u"$0.00"_s);

    const core::CustomerRecord &r = pos.shared()->customers.front();
    CHECK(app::customerFromJson(app::toJson(r)) == r);
}

TEST_CASE("Customers and gift cards are saved and come back", "[customers][store]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        openDrawer(pos);
        REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana"_s}, {u"phone"_s, u"5550101"_s}}));
        REQUIRE(pos.sellGiftCard(u"1234567"_s, 3000));
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->customers.size() == 1);
    CHECK(data->customers[0].name == "Dana");
    REQUIRE(data->giftCards.size() == 1);
    CHECK(data->giftCards[0].balance == Money::fromCents(3000));
}

TEST_CASE("Customer display: the guest chooses a tip; it goes on their card", "[customers][display]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);                                   // $12.50 + tax = $13.53
    CHECK_FALSE(pos.customerTip(u"percent"_s, 2000));         // nobody asked
    CHECK_FALSE(pos.customerPrompt()[u"askingTip"_s].toBool());
    REQUIRE(pos.askForTip());
    QVariantMap prompt = pos.customerPrompt();
    CHECK(prompt[u"askingTip"_s].toBool());
    REQUIRE(prompt[u"choices"_s].toList().size() == 4);
    CHECK(prompt[u"choices"_s].toList()[1].toMap()[u"amount"_s] == u"$2.44"_s);   // 18% of $13.53

    REQUIRE(pos.customerTip(u"percent"_s, 1800));
    prompt = pos.customerPrompt();
    CHECK_FALSE(prompt[u"askingTip"_s].toBool());
    CHECK(prompt[u"tip"_s] == u"$2.44"_s);
    REQUIRE(pos.tender(u"credit"_s));                         // the card takes it
    CHECK(pos.totals()[u"tips"_s].toString() == u"$2.44"_s);
    REQUIRE(pos.closeCheck());

    // A card already there takes the tip right away; "no tip" is fine too.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.askForTip());
    REQUIRE(pos.customerTip(u"amount"_s, 100));
    CHECK(pos.totals()[u"tips"_s].toString() == u"$1.00"_s);
    REQUIRE(pos.askForTip());
    REQUIRE(pos.customerTip(u"none"_s, 0));
    CHECK_FALSE(pos.totals()[u"hasTips"_s].toBool());
    REQUIRE(pos.closeCheck());
    CHECK_FALSE(pos.customerPrompt()[u"tipChosen"_s].toBool());   // the next check starts clean
}

TEST_CASE("Older stores: the gift tender becomes a gift card tender, House Account is added, once", "[customers][upgrade]")
{
    QJsonObject old{{u"tenders"_s, QJsonArray{QJsonObject{{u"id"_s, u"cash"_s}, {u"name"_s, u"Cash"_s}, {u"kind"_s, u"cash"_s}},
                                               QJsonObject{{u"id"_s, u"gift"_s}, {u"name"_s, u"Gift Card"_s}, {u"kind"_s, u"card"_s}}}}};
    core::PosSettings s = app::settingsFromJson(old);
    REQUIRE(s.tender("gift"));
    CHECK(s.tender("gift")->kind == core::TenderKind::GiftCard);
    REQUIRE(s.tender("house"));
    CHECK(s.tender("house")->kind == core::TenderKind::HouseAccount);

    // Saved once, a manager's later choices stand (House Account removed).
    s.tenders.pop_back();
    const core::PosSettings again = app::settingsFromJson(app::toJson(s));
    CHECK_FALSE(again.tender("house"));
}

TEST_CASE("Customer detail report: visits, spending, last visit and favorite, best first", "[customers][reports]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana Lee"_s}, {u"phone"_s, u"555-0101"_s}}));
    const QString dana = QString::fromStdString(pos.shared()->customers.front().id);
    for (int visit = 0; visit < 2; ++visit) {
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        REQUIRE(pos.useCustomer(dana));
        pos.addItem(u"cobb"_s);
        REQUIRE(pos.tender(u"credit"_s));
        REQUIRE(pos.closeCheck());
    }
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.setCustomer({{u"name"_s, u"Walk-in Sam"_s}});
    pos.addItem(u"soda"_s);
    pos.chooseOption(u"drink-size"_s, 0);
    pos.finishChoosing();
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());

    const core::Report r = pos.buildReport(u"customers"_s);
    std::vector<std::vector<std::string>> lines;
    for (const core::ReportRow &x : r.rows)
        if (x.kind == core::ReportRow::Kind::Line)
            lines.push_back(x.cells);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0][0] == "Dana Lee  ·  Cobb");        // spent the most: first; what they order most
    CHECK(lines[0][1] == "2");
    CHECK(lines[1][0].starts_with("Walk-in Sam"));
    CHECK(lines[1][1] == "1");
}
