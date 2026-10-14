#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "net/stripe_api.hh"
#include "pos_fixture.hh"
#include "print/tickets.hh"
#include "qt_catch.hh"

#include <QSignalSpy>
#include <QTest>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTcpSocket>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Cards on a reader: preparing the charge, recording the approval (once),
// the reader's connection token, refunds on Undo Payment, the receipt.

namespace {

void openDrawer(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

QVariantMap approval(qint64 checkId, qint64 cents, const QString &ref, qint64 tip = 0)
{
    return {{u"approved"_s, true}, {u"reference"_s, ref}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
            {u"amountCents"_s, cents}, {u"tipCents"_s, tip}, {u"checkId"_s, checkId}, {u"tenderId"_s, u"credit"_s},
            {u"processor"_s, u"stripe"_s}};
}

} // namespace

TEST_CASE("Cards on a reader: charge, record once, refund on Undo Payment", "[cards]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                        // $2.75 + tax = $2.98
    const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();

    // Not a card: paid as usual.
    CHECK(pos.cardCharge(u"cash"_s)[u"notCard"_s].toBool());
    // The whole balance, or what was typed.
    QVariantMap charge = pos.cardCharge(u"credit"_s);
    REQUIRE(charge[u"ok"_s].toBool());
    CHECK(charge[u"amountCents"_s] == 298);
    CHECK(charge[u"currency"_s] == u"usd"_s);
    CHECK(charge[u"checkId"_s] == check);
    pos.entryKey(u"1"_s);
    pos.entryKey(u"0"_s);
    pos.entryKey(u"0"_s);
    CHECK(pos.cardCharge(u"credit"_s)[u"amountCents"_s] == 100);

    // Approved: on the check, with the card.
    REQUIRE(pos.recordCardPayment(approval(check, 100, u"pi_1"_s)));
    REQUIRE(pos.payments().size() == 1);
    CHECK(pos.payments()[0].toMap()[u"name"_s].toString().endsWith(u"Visa •••• 4242"_s));
    CHECK(pos.totals()[u"balance"_s] == u"$1.98"_s);
    // The same approval again (a resend): once.
    REQUIRE(pos.recordCardPayment(approval(check, 100, u"pi_1"_s)));
    CHECK(pos.payments().size() == 1);
    CHECK_FALSE(pos.recordCardPayment(approval(check, 100, u""_s)));     // which payment?

    // Undo Payment: without the store's key, it can't be refunded here.
    pos.selectPayment(pos.payments()[0].toMap()[u"id"_s].toLongLong());
    CHECK_FALSE(pos.removePayment());
    CHECK(pos.payments().size() == 1);

    // With it: the refund goes out, and the payment comes off when Stripe says so.
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    std::function<void(const QString &, const QString &)> answer;
    QString refunded;
    std::int64_t refundCents = 0;
    pos.shared()->stripeRefund = [&](const QString &pi, std::int64_t cents, auto done) {
        refunded = pi;
        refundCents = cents;
        answer = done;
    };
    REQUIRE(pos.removePayment());
    CHECK(refunded == u"pi_1"_s);
    CHECK(refundCents == 100);
    CHECK(pos.payments().size() == 1);                                  // not yet
    CHECK(pos.payments()[0].toMap()[u"name"_s].toString().contains(u"refunding"_s));
    CHECK_FALSE(pos.removePayment());                                   // already on its way
    answer(u"re_1"_s, {});
    CHECK(pos.payments().isEmpty());
    CHECK(pos.totals()[u"balance"_s] == u"$2.98"_s);

    // A refund Stripe turns down: the payment stays.
    REQUIRE(pos.recordCardPayment(approval(check, 298, u"pi_2"_s)));
    REQUIRE(pos.removePayment());
    answer({}, u"Charge already refunded"_s);
    CHECK(pos.payments().size() == 1);

    // A simulated card just comes off.
    pos.selectPayment(0);
    REQUIRE(pos.removePayment());   // pi_2 again: refund...
    answer(u"re_2"_s, {});
    QVariantMap sim = approval(check, 298, u"sim_1"_s);
    sim[u"processor"_s] = u"simulated"_s;
    REQUIRE(pos.recordCardPayment(sim));
    REQUIRE(pos.removePayment());
    CHECK(pos.payments().isEmpty());

    // The receipt shows the card and Stripe's payment id.
    REQUIRE(pos.recordCardPayment(approval(check, 298, u"pi_3"_s, 50)));
    const core::Check &c = pos.shared()->open.at(check);
    const std::string receipt = print::renderText(
        print::receipt(c, {pos.shared()->settings, [](std::int64_t) { return std::string("Oct 8"); },
                           [](std::int64_t) { return std::string("10:31 AM"); }, 0}), 42);
    CHECK(receipt.find("VISA **** 4242") != std::string::npos);
    CHECK(receipt.find("pi_3") != std::string::npos);
    CHECK(c.payments.back().tip.cents() == 50);
    CHECK(c.payments.back().amount.cents() == 248);                    // charged 2.98: 2.48 + 0.50 tip
    CHECK(app::checkFromJson(app::toJson(c))->payments == c.payments);
}

TEST_CASE("Cards on a reader: the connection token, the key kept secret", "[cards]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));

    // No key: the reader hears why.
    pos.requestReaderToken();
    CHECK(pos.readerToken()[u"token"_s].toString().isEmpty());
    CHECK(pos.readerToken()[u"error"_s].toString().contains(u"Stripe key"_s));
    const int first = pos.readerToken()[u"seq"_s].toInt();

    // The key, set on Store Settings: never shown again.
    const QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    const auto save = [&](const QString &key) {
        QVariantMap r = store;
        r[u"stripeSecretKey"_s] = key;
        return pos.adminSave(u"store"_s, 0, r);
    };
    CHECK_FALSE(save(u"pk_live_oops"_s));                       // a publishable key
    REQUIRE(save(u"sk_test_abcd1234"_s));
    CHECK(pos.shared()->settings.stripeSecretKey == "sk_test_abcd1234");
    CHECK(pos.adminRecords(u"store"_s).value(0).toMap()[u"stripeSecretKey"_s].toString().isEmpty());
    bool hinted = false;
    for (const QVariant &f : pos.adminFields(u"store"_s))
        if (f.toMap()[u"path"_s] == u"stripeSecretKey"_s)
            hinted = f.toMap()[u"hint"_s].toString().contains(u"1234"_s);
    CHECK(hinted);
    REQUIRE(save(u""_s));                                       // empty: kept
    CHECK(pos.shared()->settings.stripeSecretKey == "sk_test_abcd1234");

    pos.shared()->stripeConnectionToken = [](auto done) { done(u"pst_test_123"_s, {}); };
    pos.requestReaderToken();
    CHECK(pos.readerToken()[u"token"_s] == u"pst_test_123"_s);
    CHECK(pos.readerToken()[u"seq"_s].toInt() > first);

    REQUIRE(save(u"none"_s));
    CHECK(pos.shared()->settings.stripeSecretKey.empty());
}

TEST_CASE("StripeApi: connection tokens and refunds, with the key", "[cards][stripeapi]")
{
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    QStringList requests;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        QTcpSocket *s = server.nextPendingConnection();
        QObject::connect(s, &QTcpSocket::readyRead, [&, s] {
            const QByteArray req = s->readAll();
            requests << QString::fromUtf8(req);
            QByteArray body;
            if (req.contains("/v1/terminal/connection_tokens"))
                body = R"({"object":"terminal.connection_token","secret":"pst_test_abc"})";
            else if (req.contains("payment_intent=pi_bad"))
                body = R"({"error":{"message":"No such payment_intent: pi_bad"}})";
            else
                body = R"({"id":"re_123","status":"succeeded"})";
            const QByteArray status = body.contains("error") ? "400 Bad Request" : "200 OK";
            s->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: "
                     + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            s->disconnectFromHost();
        });
    });
    const auto wait = [](const std::function<bool()> &ready) {
        for (int i = 0; i < 500 && !ready(); ++i)
            QTest::qWait(10);
    };
    QString key = u"sk_test_key"_s;
    net::StripeApi api([&] { return key; });
    api.setBaseUrl(QUrl(u"http://127.0.0.1:%1"_s.arg(server.serverPort())));

    QString token, error = u"-"_s;
    api.connectionToken([&](const QString &t, const QString &e) { token = t; error = e; });
    wait([&] { return !token.isEmpty(); });
    CHECK(token == u"pst_test_abc"_s);
    CHECK(error.isEmpty());
    CHECK(requests.last().contains(u"Authorization: Bearer sk_test_key"_s, Qt::CaseInsensitive));   // header names: any case

    QString refund;
    api.refund(u"pi_123"_s, 250, [&](const QString &id, const QString &e) { refund = id; error = e; });
    wait([&] { return !refund.isEmpty(); });
    CHECK(refund == u"re_123"_s);
    CHECK(requests.last().contains(u"payment_intent=pi_123&amount=250"_s));

    bool done = false;
    api.refund(u"pi_bad"_s, 0, [&](const QString &, const QString &e) { error = e; done = true; });
    wait([&] { return done; });
    CHECK(done);
    CHECK(error == u"No such payment_intent: pi_bad"_s);

    // No key: it says so, without asking Stripe.
    key.clear();
    const int asked = int(requests.size());
    done = false;
    api.connectionToken([&](const QString &, const QString &e) { error = e; done = true; });
    CHECK(done);
    CHECK(error.contains(u"key"_s));
    CHECK(requests.size() == asked);
}

#include "fake_stripe.hh"


TEST_CASE("Countertop reader: pair, charge from the store, declined, canceled, a tip on the reader", "[cards][counter]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    test::FakeStripe stripe;
    stripe.install(pos);
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });

    // Pairing needs the key, then a code; the store's address the first time.
    QVariantMap pair = pos.adminNewRecord(u"cardReaders"_s);
    pair[u"label"_s] = u"Front counter"_s;
    pair[u"code"_s] = u"simulated-wpe"_s;
    CHECK_FALSE(pos.adminSave(u"cardReaders"_s, -1, pair));                 // no Stripe key yet
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    REQUIRE(pos.adminSave(u"cardReaders"_s, -1, pair));
    CHECK(notices.last().contains(u"address"_s));                          // Stripe has no location
    CHECK(pos.shared()->settings.stripeReaders.empty());
    pair[u"line1"_s] = u"123 Main St"_s;
    pair[u"city"_s] = u"Springfield"_s;
    pair[u"state"_s] = u"IL"_s;
    pair[u"postalCode"_s] = u"62701"_s;
    REQUIRE(pos.adminSave(u"cardReaders"_s, -1, pair));
    REQUIRE(pos.shared()->settings.stripeReaders.size() == 1);
    CHECK(pos.shared()->settings.stripeLocation == "tml_1");
    CHECK(pos.shared()->settings.stripeReaders[0].deviceType == "simulated_wisepos_e");
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).stripeReaders == pos.shared()->settings.stripeReaders);
    pair[u"code"_s] = u"wrong-code"_s;
    REQUIRE(pos.adminSave(u"cardReaders"_s, -1, pair));
    CHECK(notices.last().contains(u"Invalid registration code"_s));

    // This screen uses it.
    bool offered = false;
    for (const QVariant &f : pos.adminFields(u"terminals"_s))
        if (f.toMap()[u"path"_s] == u"cardReader"_s)
            for (const QVariant &o : f.toMap()[u"options"_s].toList())
                offered = offered || o.toMap()[u"value"_s] == u"counter:tmr_1"_s;
    CHECK(offered);
    REQUIRE(pos.adminSave(u"terminals"_s, -1, {{u"name"_s, pos.terminalName()}, {u"cardReader"_s, u"counter:tmr_1"_s}}));
    CHECK(pos.counterReaderId() == u"tmr_1"_s);

    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                     // $2.98
    const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();

    // Approved.
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    CHECK(pos.counterCharge()[u"status"_s] == u"waiting"_s);
    CHECK(pos.counterCharge()[u"test"_s].toBool());
    CHECK(stripe.amount == 298);
    CHECK(stripe.calls.filter(u"process_payment_intent"_s).last().endsWith(u"=true"_s));   // ...skip_tipping, customer cancel
    CHECK_FALSE(pos.startCounterCharge(u"credit"_s));                      // one at a time
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    REQUIRE(pos.payments().size() == 1);
    CHECK(pos.payments()[0].toMap()[u"name"_s].toString().endsWith(u"Mastercard •••• 4444"_s));
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    CHECK(pos.shared()->open.at(check).payments[0].reference == "pi_1");

    // Declined: nothing on the check, the payment intent canceled.
    pos.selectPayment(0);
    pos.shared()->open.at(check).payments.clear();
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    REQUIRE(pos.presentTestCard(true));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    CHECK(pos.payments().isEmpty());
    CHECK(stripe.canceled);
    CHECK(notices.last().contains(u"declined"_s));

    // Canceled from the register (the reader busy reading: wait).
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    stripe.busy = true;
    REQUIRE(pos.cancelCounterCharge());
    CHECK(notices.last().contains(u"wait"_s));
    CHECK_FALSE(pos.counterCharge().isEmpty());
    stripe.busy = false;
    REQUIRE(pos.cancelCounterCharge());
    CHECK(pos.counterCharge().isEmpty());
    CHECK(pos.payments().isEmpty());

    // Tips asked on the reader: its tip comes with the payment.
    pos.shared()->settings.cardTipOn = "reader";
    stripe.readerTip = 50;
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    CHECK(stripe.calls.filter(u"process_payment_intent"_s).last().contains(u"process_config[skip_tipping]=false"_s));
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    REQUIRE(pos.payments().size() == 1);
    const core::Payment &paid = pos.shared()->open.at(check).payments.back();
    CHECK(paid.amount.cents() == 298);
    CHECK(paid.tip.cents() == 50);
}

// Against Stripe's test mode itself (run by hand, with a key in the
// environment: VTM_STRIPE_TEST_KEY=sk_test_... vtm_tests "[stripelive]").
TEST_CASE("Stripe test mode, live: pair a simulated reader, approved, declined, refunded", "[.][stripelive]")
{
    const QString key = qEnvironmentVariable("VTM_STRIPE_TEST_KEY");
    if (!key.startsWith(u"sk_test_"_s))
        SKIP("Set VTM_STRIPE_TEST_KEY to a Stripe test key (sk_test_...).");
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.stripeSecretKey = key.toStdString();
    net::StripeApi api([&] { return key; });
    pos.shared()->stripeCall = [&](const QString &m, const QString &p, const QString &f, auto done) { api.call(m, p, f, done); };
    pos.shared()->stripeRefund = [&](const QString &pi, std::int64_t c, auto done) { api.refund(pi, c, done); };
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });
    const auto wait = [](const std::function<bool()> &ready, int ms = 30000) {
        for (int i = 0; i < ms / 50 && !ready(); ++i)
            QTest::qWait(50);
        return ready();
    };

    // Pair Stripe's simulated reader (a location from the address, if Stripe has none).
    QVariantMap pair = pos.adminNewRecord(u"cardReaders"_s);
    pair[u"label"_s] = u"ViewTouch test reader"_s;
    pair[u"code"_s] = u"simulated-wpe"_s;
    pair[u"line1"_s] = u"123 Main St"_s;
    pair[u"city"_s] = u"Springfield"_s;
    pair[u"state"_s] = u"IL"_s;
    pair[u"postalCode"_s] = u"62701"_s;
    pair[u"country"_s] = u"US"_s;
    REQUIRE(pos.adminSave(u"cardReaders"_s, -1, pair));
    INFO("notices: " << notices.join(u" | "_s).toStdString());
    REQUIRE(wait([&] { return !pos.shared()->settings.stripeReaders.empty(); }));
    const auto &reader = pos.shared()->settings.stripeReaders.back();
    INFO("reader " << reader.id << " " << reader.deviceType << " at " << pos.shared()->settings.stripeLocation);
    REQUIRE(pos.adminSave(u"terminals"_s, -1, {{u"name"_s, pos.terminalName()},
                                               {u"cardReader"_s, u"counter:"_s + QString::fromStdString(reader.id)}}));

    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                    // $2.98

    // Declined.
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    REQUIRE(wait([&] { return pos.counterCharge()[u"status"_s] == u"waiting"_s; }));
    REQUIRE(pos.presentTestCard(true));
    REQUIRE(wait([&] { return pos.counterCharge().isEmpty(); }));
    CHECK(pos.payments().isEmpty());
    CHECK(notices.last().contains(u"declined"_s, Qt::CaseInsensitive));

    // Approved.
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    REQUIRE(wait([&] { return pos.counterCharge()[u"status"_s] == u"waiting"_s; }));
    REQUIRE(pos.presentTestCard(false));
    REQUIRE(wait([&] { return pos.counterCharge().isEmpty(); }));
    REQUIRE(pos.payments().size() == 1);
    const QString name = pos.payments()[0].toMap()[u"name"_s].toString();
    INFO("payment: " << name.toStdString());
    CHECK(name.contains(u"4242"_s));
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);

    // Refunded through Stripe, then off the check.
    pos.selectPayment(pos.payments()[0].toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.removePayment());
    REQUIRE(wait([&] { return pos.payments().isEmpty(); }));
    CHECK(notices.last().startsWith(u"Refunded"_s));

    // Paid again and closed; then part of it refunded from the closed check.
    REQUIRE(pos.startCounterCharge(u"credit"_s));
    REQUIRE(wait([&] { return pos.counterCharge()[u"status"_s] == u"waiting"_s; }));
    REQUIRE(pos.presentTestCard(false));
    REQUIRE(wait([&] { return pos.counterCharge().isEmpty(); }));
    const qint64 check = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.closeCheck());
    const qint64 paid = pos.shared()->closedToday.back().payments.front().id;
    REQUIRE(pos.refundPayment(check, paid, 100, u"Wrong item"_s));
    REQUIRE(wait([&] { return !pos.shared()->refundsToday.empty(); }));
    INFO("refund " << pos.shared()->refundsToday.back().reference);
    CHECK(pos.shared()->refundsToday.back().reference.starts_with("re_"));
    CHECK(pos.shared()->closedToday.back().refunded(paid).cents() == 100);
}

TEST_CASE("A bar tab's card, held: set aside on open; charged with a tip, raised if need be; let go", "[cards][hold]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    test::FakeStripe stripe;
    stripe.install(pos);
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    pos.shared()->settings.stripeReaders.push_back({"tmr_1", "Bar", "simulated_wisepos_e"});
    REQUIRE(pos.adminSave(u"terminals"_s, -1, {{u"name"_s, pos.terminalName()}, {u"cardReader"_s, u"counter:tmr_1"_s}}));
    CHECK(pos.shared()->settings.tabHold == 5000);

    // Open a tab and hold the card: $50 set aside, nothing paid.
    REQUIRE(pos.openTab(u"Mike"_s));
    const qint64 tab = pos.checkInfo()[u"id"_s].toLongLong();
    CHECK(pos.checkInfo()[u"hold"_s].toMap().isEmpty());
    REQUIRE(pos.startCounterHold());
    CHECK(stripe.amount == 5000);
    CHECK(stripe.captureMethod == u"manual"_s);
    CHECK(stripe.incremental == u"true"_s);
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    QVariantMap hold = pos.checkInfo()[u"hold"_s].toMap();
    CHECK(hold[u"card"_s] == u"Mastercard •••• 4444"_s);
    CHECK(hold[u"amount"_s] == u"$50.00"_s);
    CHECK(pos.payments().isEmpty());
    CHECK_FALSE(pos.startCounterHold());                                   // one is enough
    // Kept with the check.
    const core::Check &held = pos.shared()->open.at(tab);
    CHECK(app::checkFromJson(app::toJson(held))->hold == held.hold);

    // Drinks; closed on the card with 20%: within the hold, charged as is.
    for (int i = 0; i < 3; ++i)
        REQUIRE(pos.addItem(u"draft-beer"_s));                             // 3 x $6.00
    const qint64 owed = pos.totals()[u"balanceCents"_s].toLongLong();
    REQUIRE(pos.chargeHold(2000, 0));
    test::waitFor([&] { return pos.payments().size() == 1; });
    REQUIRE(pos.payments().size() == 1);
    CHECK(stripe.captured > owed);
    const core::Payment &paid = pos.shared()->open.at(tab).payments.back();
    CHECK(paid.reference == "pi_1");
    CHECK(paid.amount.cents() == owed);
    CHECK(paid.tip.cents() == stripe.captured - owed);
    CHECK(pos.checkInfo()[u"hold"_s].toMap().isEmpty());
    CHECK(stripe.calls.filter(u"increment_authorization"_s).isEmpty());
    REQUIRE(pos.closeCheck());
    CHECK_FALSE(stripe.canceled);

    // A bigger tab: the bank is asked to hold more, then it's charged.
    REQUIRE(pos.openTab(u"Ana"_s));
    REQUIRE(pos.startCounterHold());
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    for (int i = 0; i < 10; ++i)
        REQUIRE(pos.addItem(u"draft-beer"_s));                             // $60 and tax
    const qint64 big = pos.totals()[u"balanceCents"_s].toLongLong();
    REQUIRE(pos.chargeHold(0, 1000));                                       // a $10 tip
    test::waitFor([&] { return pos.payments().size() == 1; });
    CHECK(stripe.calls.filter(u"increment_authorization"_s).size() == 1);
    CHECK(stripe.captured == big + 1000);
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    REQUIRE(pos.closeCheck());

    // The bank won't: what's held is charged, the rest is still owed.
    REQUIRE(pos.openTab(u"Sam"_s));
    REQUIRE(pos.startCounterHold());
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    for (int i = 0; i < 10; ++i)
        REQUIRE(pos.addItem(u"draft-beer"_s));
    stripe.incrementFails = true;
    REQUIRE(pos.chargeHold(0, 0));
    test::waitFor([&] { return pos.payments().size() == 1; });
    CHECK(stripe.captured == 5000);
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == big - 5000);
    CHECK(notices.join(u" "_s).contains(u"take the rest another way"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    stripe.incrementFails = false;

    // Paid another way (cash): the hold is let go when it closes.
    REQUIRE(pos.openTab(u"Lee"_s));
    REQUIRE(pos.startCounterHold());
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    REQUIRE(pos.addItem(u"draft-beer"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(stripe.canceled);
    // Or by hand, before anything is owed.
    REQUIRE(pos.openTab(u"Kim"_s));
    REQUIRE(pos.startCounterHold());
    REQUIRE(pos.presentTestCard(false));
    test::waitFor([&] { return pos.counterCharge().isEmpty(); });
    stripe.canceled = false;
    REQUIRE(pos.releaseHold());
    CHECK(stripe.canceled);
    CHECK(pos.checkInfo()[u"hold"_s].toMap().isEmpty());
    CHECK_FALSE(pos.chargeHold(0, 0));

    // A reader on the screen itself (simulated): its answer is recorded;
    // charging needs no bank.
    REQUIRE(pos.addItem(u"draft-beer"_s));
    const QVariantMap charge = pos.holdCharge();
    REQUIRE(charge[u"ok"_s].toBool());
    CHECK(charge[u"hold"_s].toBool());
    CHECK(charge[u"amountCents"_s] == 5000);
    CHECK(charge[u"offlineMax"_s] == 0);                                   // never offline
    QVariantMap answer{{u"approved"_s, true}, {u"hold"_s, true}, {u"reference"_s, u"sim_1"_s}, {u"brand"_s, u"visa"_s},
                       {u"last4"_s, u"4242"_s}, {u"amountCents"_s, 5000}, {u"checkId"_s, charge[u"checkId"_s]},
                       {u"processor"_s, u"simulated"_s}};
    REQUIRE(pos.recordCardHold(answer));
    REQUIRE(pos.recordCardHold(answer));                                   // sent twice: once
    CHECK(pos.checkInfo()[u"hold"_s].toMap()[u"card"_s] == u"Visa •••• 4242"_s);
    REQUIRE(pos.chargeHold(1500, 0));
    CHECK(pos.totals()[u"balanceCents"_s].toLongLong() == 0);
    CHECK(pos.payments()[0].toMap()[u"tip"_s].toString() != QString());

    // Off: no holding.
    pos.shared()->settings.tabHold = 0;
    REQUIRE(pos.openTab(u"Jo"_s));
    CHECK_FALSE(pos.holdCharge()[u"ok"_s].toBool());
}

TEST_CASE("Stripe test mode, live: a tab's card held, raised, charged; another let go", "[.][stripelive]")
{
    const QString key = qEnvironmentVariable("VTM_STRIPE_TEST_KEY");
    if (!key.startsWith(u"sk_test_"_s))
        SKIP("Set VTM_STRIPE_TEST_KEY to a Stripe test key (sk_test_...).");
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.stripeSecretKey = key.toStdString();
    net::StripeApi api([&] { return key; });
    pos.shared()->stripeCall = [&](const QString &m, const QString &p, const QString &f, auto done) { api.call(m, p, f, done); };
    QStringList notices;
    QObject::connect(&pos, &app::PosSession::notice, [&](const QString &n) { notices << n; });
    const auto wait = [](const std::function<bool()> &ready, int ms = 30000) {
        for (int i = 0; i < ms / 50 && !ready(); ++i)
            QTest::qWait(50);
        return ready();
    };
    QVariantMap pair = pos.adminNewRecord(u"cardReaders"_s);
    pair[u"label"_s] = u"ViewTouch test bar"_s;
    pair[u"code"_s] = u"simulated-wpe"_s;
    pair[u"line1"_s] = u"123 Main St"_s;
    pair[u"city"_s] = u"Springfield"_s;
    pair[u"state"_s] = u"IL"_s;
    pair[u"postalCode"_s] = u"62701"_s;
    pair[u"country"_s] = u"US"_s;
    REQUIRE(pos.adminSave(u"cardReaders"_s, -1, pair));
    INFO("notices: " << notices.join(u" | "_s).toStdString());
    REQUIRE(wait([&] { return !pos.shared()->settings.stripeReaders.empty(); }));
    REQUIRE(pos.adminSave(u"terminals"_s, -1, {{u"name"_s, pos.terminalName()},
                                               {u"cardReader"_s, u"counter:"_s + QString::fromStdString(pos.shared()->settings.stripeReaders.back().id)}}));
    const auto holdCard = [&] {
        REQUIRE(pos.startCounterHold());
        REQUIRE(wait([&] { return pos.counterCharge()[u"status"_s] == u"waiting"_s; }));
        REQUIRE(pos.presentTestCard(false));
        REQUIRE(wait([&] { return pos.counterCharge().isEmpty(); }));
        REQUIRE_FALSE(pos.checkInfo()[u"hold"_s].toMap().isEmpty());
    };

    // Held, then more than that: raised, charged with a tip.
    REQUIRE(pos.openTab(u"Live Tab"_s));
    holdCard();
    for (int i = 0; i < 10; ++i)
        REQUIRE(pos.addItem(u"draft-beer"_s));
    const qint64 owed = pos.totals()[u"balanceCents"_s].toLongLong();
    REQUIRE(owed > 5000);
    REQUIRE(pos.chargeHold(0, 500));
    REQUIRE(wait([&] { return pos.payments().size() == 1; }));
    const core::Payment &paid = pos.currentCheck()->payments.back();
    INFO("charged " << paid.reference << " " << paid.amount.cents() << " + " << paid.tip.cents());
    CHECK(paid.reference.starts_with("pi_"));
    // The simulated card may not allow raising the hold: then what's held is charged.
    if (pos.totals()[u"balanceCents"_s].toLongLong() == 0) {
        CHECK(paid.amount.cents() == owed);
        CHECK(paid.tip.cents() == 500);
    } else {
        CHECK(paid.amount.cents() + paid.tip.cents() == 5000);
        REQUIRE(pos.tender(u"cash"_s));
    }
    REQUIRE(pos.closeCheck());

    // Held and let go.
    REQUIRE(pos.openTab(u"Live Tab 2"_s));
    holdCard();
    REQUIRE(pos.releaseHold());
    QTest::qWait(3000);
    CHECK_FALSE(notices.last().contains(u"couldn't"_s));
}
