#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QJsonObject>

#include "app/i18n.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The reports an accountant does the books from: the Sales Summary, the
// Deposit and the Accounting journal.

namespace {

std::string value(const core::Report &r, const std::string &label)
{
    const std::vector<std::string> cells = r.find(label);
    return cells.empty() ? std::string() : cells.back();
}

// The journal's debits and credits add up.
void balanced(const core::Report &journal)
{
    const std::vector<std::string> total = journal.find("Total");
    REQUIRE(total.size() == 3);
    CHECK(total[1] == total[2]);
}

void cashSale(PosService &pos, const QString &item)
{
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(item);
    pos.finishChoosing();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
}

} // namespace

TEST_CASE("Books: gift cards sold aren't sales, and take no share of a discount", "[books]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());

    // A Cobb ($12.50) with 10% off: $11.25, $0.93 tax.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"cobb"_s);
    pos.finishChoosing();
    REQUIRE(pos.tender(u"discount"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(value(pos.buildReport(u"sales"_s), "Food tax") == "$0.93");

    // The same with a $20 gift card sold on the check: the same tax.
    REQUIRE(pos.sellGiftCard({}, 2000));
    pos.addItem(u"cobb"_s);
    pos.finishChoosing();
    REQUIRE(pos.tender(u"discount"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    const core::Report sales = pos.buildReport(u"sales"_s);
    CHECK(value(sales, "Item sales") == "$25.00");
    CHECK(value(sales, "Discounts & comps") == "-$2.50");
    CHECK(value(sales, "Net sales") == "$22.50");                              // no gift card in it
    CHECK(value(sales, "Taxable food sales") == "$22.50");
    CHECK(value(sales, "Non-taxable sales") == "$0.00");
    CHECK(value(sales, "Food tax") == "$1.86");                                 // $0.93 each
    CHECK(value(sales, "Gift cards sold (owed to guests, not sales)") == "$20.00");
    CHECK(value(sales, "Total with tax") == "$44.36");
    CHECK(value(sales, "Collected") == "$44.36");

    const core::Report journal = pos.buildReport(u"accounting"_s);
    balanced(journal);
    CHECK(journal.find("2400 Gift card liability").at(2) == "$20.00");                     // a liability, not sales
}

TEST_CASE("Books: house account payments, paid outs, refunds and a short drawer all balance", "[books]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    core::applyStateRules(pos.shared()->settings, "CA");   // gift cards under $10 cash out

    // Acme charges a Cobb ($13.53) and pays it: $5 in cash, the rest by card.
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Acme Office"_s}, {u"phone"_s, u"5550001111"_s},
                              {u"houseAccount"_s, true}, {u"accountLimit"_s, 5000}}));
    const QString acme = QString::fromStdString(pos.shared()->customers.front().id);
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    pos.finishChoosing();
    REQUIRE(pos.useCustomer(acme));
    REQUIRE(pos.tender(u"house"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.selectCustomer(acme));
    REQUIRE(pos.payOnAccount(u"cash"_s, 500));
    REQUIRE(pos.payOnAccount(u"card"_s));

    // $12 of produce paid out of the drawer.
    pos.entryKey(u"1200"_s);
    pos.setExpenseCategory(u"Produce"_s);
    REQUIRE(pos.payout(core::CashMovement::Kind::Payout));

    // A cash sale, $3 of it given back.
    cashSale(pos, u"cobb"_s);
    const core::Check &sold = pos.shared()->closedToday.back();
    REQUIRE(pos.refundPayment(sold.id, sold.payments.front().id, 300, u"Cold"_s));

    // A $7 gift card cashed out.
    {
        core::GiftCard g;
        g.number = "600000000007";
        g.post(1, Money::fromCents(700), "Sold", 1, "sale");
        pos.shared()->giftCards.push_back(g);
    }
    REQUIRE(pos.lookupGiftCard(u"600000000007"_s));
    REQUIRE(pos.cashOutGiftCard());

    // Counted $1 short.
    const std::int64_t expected = core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday,
                                                     pos.shared()->settings.tax).cents();
    pos.entryKey(QString::number(expected - 100));
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.shared()->drawers.back().overShort().cents() == -100);

    const core::Report deposit = pos.buildReport(u"deposit"_s);
    CHECK(value(deposit, "House account payments by card") == "$8.53");
    CHECK(value(deposit, "Card batch") == "$8.53");                             // in the processor's deposit
    CHECK(value(deposit, "House account payments in cash") == "$5.00");
    CHECK(value(deposit, "Paid out (expenses)") == "-$12.00");
    CHECK(value(deposit, "Gift cards cashed out") == "-$7.00");
    CHECK(value(deposit, "Cash refunds") == "-$3.00");

    // Refunds and gift cards cashed out aren't expenses.
    CHECK(value(pos.buildReport(u"expenses"_s), "Total paid out") == "$12.00");

    const core::Report journal = pos.buildReport(u"accounting"_s);
    balanced(journal);
    CHECK(journal.find("4910 Refunds").at(1) == "$3.00");
    CHECK(journal.find("6000 Paid outs (Produce)").at(1) == "$12.00");
    CHECK(journal.find("6900 Cash over and short").at(1) == "$1.00");
}

TEST_CASE("Books: the chart of accounts matches a menu family whatever its capitals", "[books]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    std::string family;
    for (const core::MenuItem &m : pos.shared()->menu)
        if (m.id == "cobb")
            family = m.family;
    REQUIRE_FALSE(family.empty());
    std::string key = "sales:" + family;
    std::ranges::transform(key, key.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    pos.shared()->settings.accounts[key] = "4010 Salads";   // as Store Settings saves it: lower case
    cashSale(pos, u"cobb"_s);
    CHECK(pos.buildReport(u"accounting"_s).find("4010 Salads").at(2) == "$12.50");
}

TEST_CASE("Books: End of Day prints the sales and the deposit", "[books]")
{
    struct Printer : app::PosPrinter {
        std::vector<std::string> reports;
        void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
        void printReceipt(const core::PosSettings &, const core::Check &, const std::string &) override {}
        void printReport(const core::PosSettings &, const core::Report &r, const std::string &) override { reports.push_back(r.id); }
        void openDrawer(const core::PosSettings &, const std::string &) override {}
    } printer;
    PosService pos(test::seedPosData(), nullptr);
    pos.shared()->printer = &printer;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    cashSale(pos, u"cobb"_s);
    pos.entryKey(QString::number(core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday,
                                                    pos.shared()->settings.tax).cents()));
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.endOfDay());
    REQUIRE(printer.reports.size() >= 2);
    CHECK(std::vector<std::string>(printer.reports.end() - 2, printer.reports.end())
          == std::vector<std::string>{"sales", "deposit"});
}

TEST_CASE("Books: a day's reports say its date and hours, kept and printed later", "[books]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    cashSale(pos, u"cobb"_s);
    const QString open = pos.report(u"sales"_s)[u"subtitle"_s].toString();
    CHECK(open.contains(u", since "_s));
    CHECK_FALSE(open.startsWith(u"Today"_s));
    pos.entryKey(QString::number(core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday,
                                                    pos.shared()->settings.tax).cents()));
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.endOfDay());

    app::PastDay &past = pos.shared()->pastDays.front();
    const QString kept = pos.report(u"sales"_s, past.day.id)[u"subtitle"_s].toString();
    CHECK(kept.contains(u" to "_s));                                   // opened to closed
    CHECK(kept.startsWith(open.left(open.indexOf(u", since "_s))));     // the same date

    // One saved the old way ("Today, since ...") shows its date too.
    QJsonObject old = past.reports.value(u"deposit"_s).toObject();
    old.insert(u"subtitle"_s, u"Today, since 10:31 AM"_s);
    past.reports.insert(u"deposit"_s, old);
    const QString dated = pos.report(u"deposit"_s, past.day.id)[u"subtitle"_s].toString();
    CHECK(dated.startsWith(kept.left(kept.indexOf(u", "_s, kept.indexOf(u", "_s) + 2))));   // its date
    CHECK(dated.contains(u"closed at"_s));                              // its opening hour isn't known
}

TEST_CASE("Books: a day that closes after midnight is the night before's", "[books]")
{
    PosService pos(test::seedPosData(), nullptr);
    // Tuesday 6 PM local: the day's first check; it closes Wednesday 1:30 AM.
    const QDateTime tuesday(QDate(2026, 10, 13), QTime(18, 0));
    std::int64_t clock = tuesday.toMSecsSinceEpoch();
    pos.shared()->setClock([&clock] { return clock; });
    pos.shared()->day.openedAt = clock - 20 * 3'600'000;   // when Monday closed
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    cashSale(pos, u"cobb"_s);
    clock = QDateTime(QDate(2026, 10, 14), QTime(1, 30)).toMSecsSinceEpoch();
    const QString tue = i18n::locale().toString(QDate(2026, 10, 13), QLocale::LongFormat);
    CHECK(pos.report(u"sales"_s)[u"subtitle"_s].toString().startsWith(tue + u", since "_s));
}
