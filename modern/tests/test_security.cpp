#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;

// What a manager, a paired screen or a file from outside can't do: the
// security review's findings, one test each.

namespace {

// The owner (Morgan, 1234) adds Pat, a manager; Pat is logged in.
void asPlainManager(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap pat = pos.adminNewRecord(u"employees"_s);
    pat[u"name"_s] = u"Pat"_s;
    pat[u"role"_s] = u"manager"_s;
    pat[u"pin"_s] = u"246810"_s;
    REQUIRE(pos.adminSave(u"employees"_s, -1, pat));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"246810"_s));
    REQUIRE_FALSE(pos.can(u"owner"_s));
}

int employeeIndex(PosService &pos, const QString &id)
{
    const QVariantList list = pos.adminRecords(u"employees"_s);
    for (int i = 0; i < list.size(); ++i)
        if (list[i].toMap()[u"id"_s] == id)
            return i;
    return -1;
}

} // namespace

TEST_CASE("Security: a manager can't reach the owner's settings through a new or missing record", "[security][owner]")
{
    PosService pos(test::seedPosData(), nullptr);
    pos.shared()->settings.stripeSecretKey = "sk_test_owners";
    asPlainManager(pos);
    const QVariantMap theirs{{u"storeName"_s, u"Café"_s}, {u"stripeSecretKey"_s, u"sk_live_theirs"_s}};
    CHECK_FALSE(pos.adminSave(u"store"_s, -1, theirs));
    CHECK_FALSE(pos.adminSave(u"store"_s, 99, theirs));
    CHECK_FALSE(pos.adminSave(u"store"_s, 0, theirs));
    CHECK(pos.shared()->settings.stripeSecretKey == "sk_test_owners");
    for (const QVariantMap &m : {QVariantMap{{u"storeName"_s, u"Café"_s}, {u"encryptBackups"_s, pos.shared()->settings.backupKey.empty()}},
                                 QVariantMap{{u"storeName"_s, u"Café"_s}, {u"backupCopyDir"_s, u"/tmp/x"_s}}}) {
        CHECK_FALSE(pos.adminSave(u"store"_s, -1, m));
        CHECK_FALSE(pos.adminSave(u"store"_s, 5, m));
    }
    CHECK(pos.shared()->settings.backupCopyDir.empty());
    // Taxes are the owner's, whatever the index.
    const auto food = pos.shared()->settings.tax.foodPpm;
    QVariantMap taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    taxes[u"food"_s] = 1.0;
    CHECK_FALSE(pos.adminSave(u"taxes"_s, 1, taxes));
    CHECK_FALSE(pos.adminSave(u"taxes"_s, -1, taxes));
    CHECK(pos.shared()->settings.tax.foodPpm == food);
    // A card reader is paired by the owner.
    CHECK_FALSE(pos.adminSave(u"cardReaders"_s, 7, {{u"code"_s, u"abc-def"_s}, {u"label"_s, u"Mine"_s}}));
    // What a manager may change still saves, and is in the Changes report.
    const auto before = pos.changesToday().size();
    CHECK(pos.adminSave(u"store"_s, 3, {{u"storeName"_s, u"Pat's Café"_s}}));
    CHECK(pos.storeName() == u"Pat's Café"_s);
    CHECK(pos.changesToday().size() > before);
}

TEST_CASE("Security: a manager can't set the owner's or another manager's PIN", "[security][owner]")
{
    PosService pos(test::seedPosData(), nullptr);
    asPlainManager(pos);
    const int morgan = employeeIndex(pos, u"manager"_s);
    REQUIRE(morgan >= 0);
    QVariantMap m = pos.adminRecords(u"employees"_s)[morgan].toMap();
    m[u"pin"_s] = u"999999"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, morgan, m));
    pos.logout();
    CHECK_FALSE(pos.loginWithPin(u"999999"_s));
    CHECK(pos.loginWithPin(u"1234"_s));
    pos.logout();
    // Their own: yes. A server's: yes.
    REQUIRE(pos.loginWithPin(u"246810"_s));
    const int pat = employeeIndex(pos, u"pat"_s);
    QVariantMap me = pos.adminRecords(u"employees"_s)[pat].toMap();
    me[u"pin"_s] = u"135792"_s;
    CHECK(pos.adminSave(u"employees"_s, pat, me));
    const int sam = employeeIndex(pos, u"sam"_s);
    QVariantMap s = pos.adminRecords(u"employees"_s)[sam].toMap();
    s[u"pin"_s] = u"8642"_s;
    CHECK(pos.adminSave(u"employees"_s, sam, s));
}

TEST_CASE("Security: \"that PIN is in use\" can't be used to find PINs", "[security][pin]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int sam = employeeIndex(pos, u"sam"_s);
    QVariantMap s = pos.adminRecords(u"employees"_s)[sam].toMap();
    // Five PINs found in use: then a wait, as at the login.
    for (const char *pin : {"2222", "3333", "4444", "5555", "2222"}) {
        s[u"pin"_s] = QString::fromLatin1(pin);
        CHECK_FALSE(pos.adminSave(u"employees"_s, sam, s));
    }
    s[u"pin"_s] = u"9137"_s;   // free, but the screen waits
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, s));
    pos.logout();
    CHECK_FALSE(pos.loginWithPin(u"1234"_s));   // the login waits too
}

TEST_CASE("Security: setup isn't finished while the sample manager (PIN 1234) can log in", "[security][setup]")
{
    app::PosData d = test::seedPosData();
    d.settings.setupDone = false;
    PosService pos(std::move(d), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.setupFinish(true));
    CHECK_FALSE(pos.shared()->settings.setupDone);
    // Their own owner, the samples retired: done.
    QVariantMap me = pos.adminNewRecord(u"employees"_s);
    me[u"name"_s] = u"Ana"_s;
    me[u"role"_s] = u"manager"_s;
    me[u"pin"_s] = u"481516"_s;
    REQUIRE(pos.adminSave(u"employees"_s, -1, me));
    REQUIRE(pos.setupRetireSamples());
    CHECK(pos.setupFinish(true));
    CHECK(pos.shared()->settings.setupDone);
    CHECK_FALSE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.loginWithPin(u"481516"_s));
}

namespace {

QVariantMap cardSaid(qint64 checkId, qint64 cents, const QString &ref)
{
    return {{u"approved"_s, true}, {u"reference"_s, ref}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
            {u"amountCents"_s, cents}, {u"tipCents"_s, 0}, {u"checkId"_s, checkId}, {u"tenderId"_s, u"credit"_s},
            {u"processor"_s, u"stripe"_s}};
}

} // namespace

TEST_CASE("Security: a card payment a screen reports is checked with Stripe", "[security][cards]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    // Stripe knows pi_good (paid $2.98) and nothing else.
    QStringList asked;
    pos.shared()->stripeCall = [&](const QString &method, const QString &path, const QString &, auto done) {
        asked << method + u' ' + path;
        if (path == u"/v1/payment_intents/pi_good")
            return done({{u"status"_s, u"succeeded"_s}, {u"amount_received"_s, 298}, {u"currency"_s, u"usd"_s}}, {});
        if (path == u"/v1/payment_intents/pi_small")
            return done({{u"status"_s, u"succeeded"_s}, {u"amount_received"_s, 100}, {u"currency"_s, u"usd"_s}}, {});
        return done({{u"error"_s, QJsonObject{{u"message"_s, u"No such payment_intent"_s}}}}, u"No such payment_intent"_s);
    };
    const auto check = [&] {
        REQUIRE(pos.startCheck(core::CheckType::Quick));
        REQUIRE(pos.addItem(u"coffee"_s));
        return pos.checkInfo()[u"id"_s].toLongLong();
    };
    const auto flagged = [&] {
        int n = 0;
        for (const QVariant &a : pos.cardAlerts())
            n += a.toMap()[u"text"_s].toString().contains(u"Stripe doesn't have"_s);
        return n;
    };

    // Not an id that can go in Stripe's address.
    qint64 id = check();
    CHECK_FALSE(pos.recordCardPayment(cardSaid(id, 298, u"pi_x/../../v1/refunds"_s)));
    CHECK_FALSE(pos.recordCardPayment(cardSaid(id, 298, u"pi_x?a=b"_s)));
    // A pretend reader, on a store with Stripe: no.
    QVariantMap sim = cardSaid(id, 298, u"sim_1"_s);
    sim[u"processor"_s] = u"simulated"_s;
    CHECK_FALSE(pos.recordCardPayment(sim));
    // Offline cards where the store doesn't take them: no.
    QVariantMap off = cardSaid(id, 298, {});
    off[u"offline"_s] = true;
    off[u"localRef"_s] = u"local-1"_s;
    pos.shared()->settings.offlineCards = false;
    CHECK_FALSE(pos.recordCardPayment(off));
    CHECK(pos.payments().isEmpty());

    // Real: on the check, nothing flagged.
    REQUIRE(pos.recordCardPayment(cardSaid(id, 298, u"pi_good"_s)));
    CHECK(asked.contains(u"GET /v1/payment_intents/pi_good"_s));
    CHECK(flagged() == 0);
    pos.releaseCheck();
    // Made up, or less than said: flagged for a manager.
    id = check();
    REQUIRE(pos.recordCardPayment(cardSaid(id, 298, u"pi_madeup"_s)));
    CHECK(flagged() == 1);
    pos.releaseCheck();
    id = check();
    REQUIRE(pos.recordCardPayment(cardSaid(id, 298, u"pi_small"_s)));
    CHECK(flagged() == 2);

    // A card "taken offline" said to be sent: its id is checked too.
    pos.releaseCheck();
    pos.shared()->settings.offlineCards = true;
    id = check();
    off[u"checkId"_s] = id;
    REQUIRE(pos.recordCardPayment(off));
    CHECK_FALSE(pos.cardForwarded({{u"localRef"_s, u"local-1"_s}, {u"status"_s, u"ok"_s}, {u"id"_s, u"pi_1/../x"_s}}));
    REQUIRE(pos.cardForwarded({{u"localRef"_s, u"local-1"_s}, {u"status"_s, u"ok"_s}, {u"id"_s, u"pi_forged"_s}}));
    CHECK(flagged() == 3);
}

TEST_CASE("Security: a screen working alone can't log a manager in (their PINs stay on the store's computer)", "[security][offline]")
{
    PosShared store(test::seedPosData(), nullptr);
    PosService bar(&store, u"Bar"_s);
    const auto data = app::offlineData(bar.offlineKit());
    REQUIRE(data);
    PosShared alone(*data, nullptr);
    PosService screen(&alone, u"Bar"_s);
    CHECK_FALSE(screen.loginWithPin(u"1234"_s));   // Morgan, the owner
    CHECK(screen.loginWithPin(u"1111"_s));         // Sam, a server
}

TEST_CASE("Security: pictures are pictures: no web addresses, no other files from the store's computer", "[security][pictures]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    for (const QString &bad : {u"/etc/passwd"_s, u"http://tracker.example/x.png"_s, u"file:///home/x/.ssh/id_rsa"_s,
                               u"/var/lib/vtmodern/../vtmodern/pos.db"_s, u"data:image/png;base64,AAAA"_s}) {
        store[u"displayLogo"_s] = bad;
        CHECK_FALSE(pos.adminSave(u"store"_s, 0, store));
        CHECK(pos.imageUrl(bad).isEmpty());
    }
    store[u"displayLogo"_s] = u"logo:"_s;
    store[u"displaySlides"_s] = u"image:https://tracker.example/a.png"_s;
    CHECK_FALSE(pos.adminSave(u"store"_s, 0, store));
    // A logo set (an older store, a restored database) to a file that isn't a
    // picture, even named like one: never sent to the screens.
    QTemporaryDir dir;
    const QString fake = dir.filePath(u"secrets.png"_s);
    {
        QFile f(fake);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("SQLite format 3\0 sk_live_...");
    }
    pos.shared()->settings.displayLogo = fake.toStdString();
    CHECK(pos.storeImage(fake).isEmpty());
    const QString real = dir.filePath(u"logo.png"_s);
    {
        QFile f(real);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("\x89PNG\r\n\x1a\n....");
    }
    pos.shared()->settings.displayLogo = real.toStdString();
    CHECK_FALSE(pos.storeImage(real).isEmpty());
    CHECK(pos.storeImage(u"/etc/hostname"_s).isEmpty());   // not the store's picture at all
}
