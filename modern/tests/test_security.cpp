#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"

#include <QJsonArray>
#include <QJsonObject>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

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
