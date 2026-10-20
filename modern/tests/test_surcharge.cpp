#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "cardreader.hh"
#include "fake_stripe.hh"
#include "pos_fixture.hh"
#include "print/tickets.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// A credit card surcharge (credit cards only: the reader says which kind),
// and a cash discount (dual pricing).

namespace {

void openDrawer(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

bool setPercent(PosService &pos, const char16_t *key, double pct)
{
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    store[QString::fromUtf16(key)] = pct;
    return pos.adminSave(u"store"_s, 0, store);
}

std::string receiptOf(PosService &pos, const core::Check &c)
{
    return print::renderText(print::receipt(c, {pos.shared()->settings, [](std::int64_t) { return std::string("Oct 8"); },
                                                [](std::int64_t) { return std::string("10:31 AM"); }, 0}), 42);
}

QVariantMap approval(qint64 checkId, qint64 cents, const QString &ref, const QString &funding, qint64 surcharge)
{
    return {{u"approved"_s, true}, {u"reference"_s, ref}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
            {u"amountCents"_s, cents}, {u"tipCents"_s, 0}, {u"checkId"_s, checkId}, {u"tenderId"_s, u"credit"_s},
            {u"processor"_s, u"stripe"_s}, {u"funding"_s, funding}, {u"surchargeCents"_s, surcharge}};
}

} // namespace

TEST_CASE("Surcharge: only where the state allows it, and no more than it allows", "[cards][surcharge]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.shared()->settings.laborState = "IL";
    CHECK(setPercent(pos, u"surchargePercent", 3));
    CHECK(pos.shared()->settings.surchargeBp == 300);
    CHECK_FALSE(setPercent(pos, u"surchargePercent", 3.5));   // the card networks' cap
    pos.shared()->settings.laborState = "CO";
    CHECK_FALSE(setPercent(pos, u"surchargePercent", 3));
    CHECK(setPercent(pos, u"surchargePercent", 2));
    for (const char *state : {"CT", "MA", "ME"}) {
        pos.shared()->settings.laborState = state;
        CHECK_FALSE(setPercent(pos, u"surchargePercent", 1));
        CHECK(setPercent(pos, u"surchargePercent", 0));
        CHECK(setPercent(pos, u"cashDiscountPercent", 4));   // a cash discount is fine there
    }
    // A store set before it moved to such a state: none charged.
    pos.shared()->settings.surchargeBp = 300;
    CHECK(pos.surchargeBp() == 0);
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).cashDiscountBp == 400);
}

TEST_CASE("Surcharge: a credit card pays it, a debit card doesn't; receipt, refund, reports", "[cards][surcharge]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.laborState = "IL";
    pos.shared()->settings.surchargeBp = 300;
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                    // $2.98
    const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();

    // The reader is told what a credit card adds, and the guest what it is.
    const QVariantMap charge = pos.cardCharge(u"credit"_s);
    CHECK(charge[u"surchargeCents"_s] == 9);                              // 3% of 2.98
    CHECK(charge[u"surchargeNotice"_s].toString().contains(u"3%"_s));
    CHECK(pos.totals()[u"surchargeNotice"_s].toString().contains(u"Debit cards: none"_s));

    // A credit card: 2.98 for the check, 0.09 surcharge on top.
    REQUIRE(pos.recordCardPayment(approval(check, 307, u"pi_1"_s, u"credit"_s, 9)));
    const core::Check &c = pos.shared()->open.at(check);
    REQUIRE(c.payments.size() == 1);
    CHECK(c.payments[0].amount.cents() == 298);
    CHECK(c.payments[0].surcharge.cents() == 9);
    CHECK(c.payments[0].funding == "credit");
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    CHECK(pos.totals()[u"surcharges"_s] == u"$0.09"_s);
    CHECK(app::checkFromJson(app::toJson(c))->payments == c.payments);
    const std::string receipt = receiptOf(pos, c);
    INFO(receipt);
    CHECK(receipt.find("Credit card surcharge 3%") != std::string::npos);
    CHECK(receipt.find("Charged to the card") != std::string::npos);
    CHECK(receipt.find("$3.07") != std::string::npos);
    const qint64 paymentId = c.payments[0].id;
    REQUIRE(pos.closeCheck());

    // Reports: the surcharge is the store's, apart from sales; in the card batch.
    CHECK(pos.buildReport(u"sales"_s).find("Credit card surcharges (the store's, not sales)").back() == "$0.09");
    CHECK(pos.buildReport(u"sales"_s).find("Net sales").back() == "$2.75");
    CHECK(pos.buildReport(u"deposit"_s).find("Credit card surcharges").back() == "$0.09");
    CHECK(pos.buildReport(u"deposit"_s).find("Card batch").back() == "$3.07");

    // A refund gives its share of the surcharge back too.
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    std::int64_t refunded = 0;
    pos.shared()->stripeRefund = [&](const QString &, std::int64_t cents, auto done) {
        refunded = cents;
        done(u"re_1"_s, {});
    };
    REQUIRE(pos.refundPayment(check, paymentId, 149, u"Cold"_s));   // half
    CHECK(refunded == 149 + 5);                                             // + half the surcharge (4.5 -> 5)

    // A debit card: no surcharge.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));
    const qint64 second = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.recordCardPayment(approval(second, 298, u"pi_2"_s, u"debit"_s, 0)));
    const core::Check &d = pos.shared()->open.at(second);
    CHECK(d.payments[0].amount.cents() == 298);
    CHECK(d.payments[0].surcharge.cents() == 0);
    CHECK(d.payments[0].funding == "debit");
    CHECK(receiptOf(pos, d).find("surcharge") == std::string::npos);
}

TEST_CASE("Surcharge: the simulated reader adds it for a credit card only", "[cards][surcharge][ui]")
{
    CardReader reader;
    reader.setKind(u"simulated"_s);
    reader.setSimulatedDelay(0);
    QSignalSpy done(&reader, &CardReader::finished);
    const QVariantMap charge{{u"amountCents"_s, 298}, {u"tipCents"_s, 50}, {u"surchargeCents"_s, 9}, {u"checkId"_s, 7}};
    reader.charge(charge);
    REQUIRE(done.wait(2000));
    QVariantMap r = done.takeFirst().first().toMap();
    CHECK(r[u"approved"_s].toBool());
    CHECK(r[u"funding"_s] == u"credit"_s);
    CHECK(r[u"surchargeCents"_s] == 9);
    CHECK(r[u"amountCents"_s] == 298 + 50 + 9);
    CHECK(r[u"tipCents"_s] == 50);

    reader.setSimulatedFunding(u"debit"_s);
    reader.charge(charge);
    REQUIRE(done.wait(2000));
    r = done.takeFirst().first().toMap();
    CHECK(r[u"funding"_s] == u"debit"_s);
    CHECK(r[u"surchargeCents"_s] == 0);
    CHECK(r[u"amountCents"_s] == 298 + 50);
}

TEST_CASE("Surcharge: the reader beside the register reads the card first, then charges it", "[cards][surcharge][counter]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    test::FakeStripe stripe;
    stripe.install(pos);
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    pos.shared()->settings.laborState = "IL";
    pos.shared()->settings.surchargeBp = 300;
    core::PosSettings::StripeReader r;
    r.id = "tmr_1";
    r.label = "Front counter";
    pos.shared()->settings.stripeReaders.push_back(r);
    REQUIRE(pos.adminSave(u"terminals"_s, -1, {{u"name"_s, pos.terminalName()}, {u"cardReader"_s, u"counter:tmr_1"_s}}));

    for (const QString funding : {u"credit"_s, u"debit"_s}) {
        stripe.updatedAmount = 0;
        REQUIRE(pos.startCheck(core::CheckType::Quick));
        REQUIRE(pos.addItem(u"coffee"_s));
        const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();
        REQUIRE(pos.startCounterCharge(u"credit"_s));
        CHECK(stripe.calls.filter(u"collect_payment_method"_s).size() >= 1);
        REQUIRE(pos.presentTestCard(funding));   // Stripe's test card of that kind
        CHECK(stripe.calls.last().contains(funding == u"debit" ? u"4000056655665556"_s : u"4242424242424242"_s));
        test::waitFor([&] { return pos.counterCharge().isEmpty(); });
        REQUIRE(pos.shared()->open.at(check).payments.size() == 1);
        const core::Payment &p = pos.shared()->open.at(check).payments[0];
        CHECK(p.amount.cents() == 298);
        CHECK(p.funding == funding.toStdString());
        CHECK(p.surcharge.cents() == (funding == u"credit" ? 9 : 0));
        CHECK(stripe.updatedAmount == (funding == u"credit" ? 307 : 0));   // only a credit card's grew
        CHECK(stripe.calls.filter(u"confirm_payment_intent"_s).size() >= 1);
        REQUIRE(pos.closeCheck());
    }
}

TEST_CASE("Cash discount: all in cash pays the cash price; part in cash, the card price", "[cash][dualpricing]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.cashDiscountBp = 400;                          // 4%
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                    // card price $2.98
    const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();
    // 2.75 - 4% = 2.64, + 8.25% tax = 2.86.
    CHECK(pos.totals()[u"cardPrice"_s] == u"$2.98"_s);
    CHECK(pos.totals()[u"cashPrice"_s] == u"$2.86"_s);
    const std::string before = receiptOf(pos, pos.shared()->open.at(check));
    CHECK(before.find("Card price") != std::string::npos);
    CHECK(before.find("Cash price") != std::string::npos);

    // Part in cash: no discount, and they're told what all in cash would be.
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });
    pos.entryKey(u"1"_s);
    pos.entryKey(u"0"_s);
    pos.entryKey(u"0"_s);
    REQUIRE(pos.tender(u"cash"_s));                                       // $1.00
    CHECK(notices.join(u" "_s).contains(u"$2.86"_s));
    CHECK(pos.totals()[u"balance"_s] == u"$1.98"_s);
    CHECK_FALSE(pos.totals().contains(u"cashPrice"_s));                   // something's paid: card price now
    pos.selectPayment(pos.payments()[0].toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.removePayment());

    // All in cash (nothing typed): the cash price, the discount on the check.
    REQUIRE(pos.tender(u"cash"_s));
    const core::Check &c = pos.shared()->open.at(check);
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    CHECK(c.totals(pos.shared()->settings.tax).total.cents() == 286);
    CHECK(std::ranges::count_if(c.payments, [](const core::Payment &p) { return p.reference == "cashDiscount"; }) == 1);
    // A $5 bill for it: change from the cash price.
    pos.selectPayment(c.payments.back().id);
    REQUIRE(pos.removePayment());
    CHECK(c.payments.empty());                                            // the discount went with the cash
    pos.entryKey(u"5"_s);
    pos.entryKey(u"0"_s);
    pos.entryKey(u"0"_s);
    REQUIRE(pos.tender(u"cash"_s));
    CHECK(pos.totals()[u"change"_s] == u"$2.14"_s);
    const std::string paid = receiptOf(pos, c);
    INFO(paid);
    CHECK(paid.find("Cash discount 4%") != std::string::npos);
    REQUIRE(pos.closeCheck());
    CHECK(pos.buildReport(u"sales"_s).find("Net sales").back() == "$2.64");
}

TEST_CASE("Practice reader: waits for a test card; debit pays no surcharge, declined pays nothing", "[cards][surcharge][ui]")
{
    CardReader reader;
    reader.setKind(u"simulated"_s);   // as in the store: no delay set, it waits
    QSignalSpy done(&reader, &CardReader::finished);
    const QVariantMap charge{{u"amountCents"_s, 298}, {u"tipCents"_s, 0}, {u"surchargeCents"_s, 9}, {u"checkId"_s, 7}};
    CHECK_FALSE(reader.testCards());
    reader.charge(charge);
    CHECK(reader.testCards());
    CHECK_FALSE(done.wait(400));          // nothing by itself
    reader.presentTestCard(u"debit"_s);
    REQUIRE(done.size() == 1);
    QVariantMap r = done.takeFirst().first().toMap();
    CHECK(r[u"approved"_s].toBool());
    CHECK(r[u"funding"_s] == u"debit"_s);
    CHECK(r[u"amountCents"_s] == 298);
    CHECK_FALSE(reader.testCards());

    reader.charge(charge);
    reader.presentTestCard(u"credit"_s);
    r = done.takeFirst().first().toMap();
    CHECK(r[u"funding"_s] == u"credit"_s);
    CHECK(r[u"amountCents"_s] == 307);

    reader.charge(charge);
    reader.presentTestCard(u"declined"_s);
    r = done.takeFirst().first().toMap();
    CHECK(r[u"declined"_s].toBool());
    CHECK_FALSE(r[u"approved"_s].toBool());
}
