#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;
using vt::core::CashMovement;

// Server banks: each person keeps the cash they take, on any terminal, and
// turns it in when they check out.

namespace {

app::PosData bankData()
{
    app::PosData d = test::seedPosData();
    d.settings.cashMode = core::CashMode::ServerBank;
    return d;
}

struct Kicks : app::PosPrinter {
    int drawerKicks = 0;
    void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
    void printReceipt(const core::PosSettings &, const core::Check &, const std::string &) override {}
    void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
    void openDrawer(const core::PosSettings &, const std::string &) override { ++drawerKicks; }
};

// A $6.60 cash sale closed by whoever is logged in on `pos`.
void cashSale(PosService &pos)
{
    pos.addItem(u"draft-beer"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
}

} // namespace

TEST_CASE("Server banks: cash follows the person, on any terminal", "[bank]")
{
    PosShared shared(bankData(), nullptr);
    Kicks kicks;
    shared.printer = &kicks;
    PosService front(&shared, u"Front"_s);
    PosService bar(&shared, u"Bar"_s);

    // Sam takes cash on Front: no drawer needed, Sam's bank starts at $0.
    REQUIRE(front.loginWithPin(u"1111"_s));
    CHECK(front.drawerInfo()[u"mode"_s].toString() == u"serverBank"_s);
    CHECK_FALSE(front.drawerInfo()[u"exists"_s].toBool());
    cashSale(front);
    QVariantMap sam = front.drawerInfo();
    CHECK(sam[u"name"_s].toString() == u"Sam's bank"_s);
    CHECK(sam[u"expected"_s].toString() == u"$6.60"_s);
    CHECK(kicks.drawerKicks == 0);                 // no drawer to open

    // Another person on the same terminal has their own bank.
    front.logout();
    REQUIRE(front.loginWithPin(u"2222"_s));
    CHECK_FALSE(front.drawerInfo()[u"exists"_s].toBool());
    cashSale(front);
    CHECK(front.drawerInfo()[u"expected"_s].toString() == u"$6.60"_s);
    CHECK(front.drawerInfo()[u"name"_s].toString() != sam[u"name"_s].toString());

    // Sam moves to the bar: same bank.
    REQUIRE(bar.loginWithPin(u"1111"_s));
    cashSale(bar);
    CHECK(bar.drawerInfo()[u"expected"_s].toString() == u"$13.20"_s);
    CHECK(shared.drawers.size() == 2);
}

TEST_CASE("Server banks: start with cash on hand, check out, over / short", "[bank]")
{
    PosService pos(bankData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.entryKey(u"2000"_s);
    REQUIRE(pos.openDrawerSession());              // Start Bank with $20 of change
    CHECK_FALSE(pos.openDrawerSession());
    cashSale(pos);
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$26.60"_s);

    // Not while a check is still open.
    pos.addItem(u"draft-beer"_s);
    const qint64 open = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();
    pos.entryKey(u"2660"_s);
    CHECK_FALSE(pos.countDrawer());
    REQUIRE(pos.openCheck(open));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    pos.entryKey(u"3000"_s);                       // turns in $30.00 of $33.20
    REQUIRE(pos.countDrawer());
    const QVariantMap d = pos.drawerInfo();
    CHECK_FALSE(d[u"open"_s].toBool());
    CHECK(d[u"overShortCents"_s].toLongLong() == -320);

    // The next cash sale starts a new bank.
    cashSale(pos);
    CHECK(pos.drawerInfo()[u"open"_s].toBool());
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$6.60"_s);
}

TEST_CASE("Server banks: managers count banks left open; end of day waits for them", "[bank]")
{
    PosShared shared(bankData(), nullptr);
    PosService front(&shared, u"Front"_s);
    PosService office(&shared, u"Office"_s);
    REQUIRE(front.loginWithPin(u"1111"_s));
    cashSale(front);
    front.logout();                                // Sam leaves without checking out

    REQUIRE(office.loginWithPin(u"1234"_s));
    const QStringList blockers = office.dayInfo()[u"blockers"_s].toStringList();
    CHECK(blockers.contains(u"Sam's bank has not been counted"_s));
    const QVariantList others = office.drawerInfo()[u"others"_s].toList();
    REQUIRE(others.size() == 1);
    CHECK(others[0].toMap()[u"name"_s].toString() == u"Sam's bank"_s);
    CHECK(others[0].toMap()[u"expected"_s].toString() == u"$6.60"_s);

    // Servers can't count someone else's bank.
    REQUIRE(front.loginWithPin(u"2222"_s));
    front.entryKey(u"660"_s);
    CHECK_FALSE(front.countDrawerById(others[0].toMap()[u"id"_s].toLongLong()));

    office.entryKey(u"660"_s);
    REQUIRE(office.countDrawerById(others[0].toMap()[u"id"_s].toLongLong()));
    CHECK(office.drawerInfo()[u"others"_s].toList().isEmpty());
    CHECK(office.dayInfo()[u"ready"_s].toBool());
}

TEST_CASE("Server banks: pay outs and tip cash-outs come from your own bank", "[bank]")
{
    app::PosData data = bankData();
    data.settings.gratuityBp = 1800;
    data.settings.gratuityMinGuests = 2;
    PosService pos(data, nullptr);

    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"500"_s);
    pos.setExpenseCategory(u"Other"_s);
    REQUIRE(pos.payout(CashMovement::Kind::Payout));   // the manager's bank goes to -$5
    CHECK(pos.drawerInfo()[u"name"_s].toString() == u"Morgan (Manager)'s bank"_s);
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"-$5.00"_s);
    pos.logout();

    // Sam's card tip is paid from Sam's own cash.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.addItem(u"draft-beer"_s);
    REQUIRE(pos.tender(u"credit"_s));
    pos.entryKey(u"200"_s);
    REQUIRE(pos.addTip(0));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.cashOutTips());
    CHECK(pos.drawerInfo()[u"name"_s].toString() == u"Sam's bank"_s);
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"-$2.00"_s);   // the house owes Sam's bank $2
}

TEST_CASE("Cash handling is a store setting", "[bank][admin]")
{
    PosService pos(test::seedPosData(), nullptr);   // drawer per terminal
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap store = pos.adminRecords(u"store"_s)[0].toMap();
    CHECK(store[u"cashMode"_s].toString() == u"drawer"_s);
    store[u"cashMode"_s] = u"serverBank"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.cashMode == core::CashMode::ServerBank);
    cashSale(pos);                                   // no drawer needed now
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).cashMode == core::CashMode::ServerBank);
}

TEST_CASE("Per-employee cash handling: banks and a counter drawer in one store", "[bank][admin]")
{
    app::PosData data = bankData();                     // store: server banks
    for (core::Employee &e : data.employees) {
        if (e.role == "cashier")
            e.cashMode = "drawer";                      // the counter cashier uses the drawer
    }
    PosShared shared(data, nullptr);
    Kicks kicks;
    shared.printer = &kicks;
    PosService counter(&shared, u"Counter"_s);

    // The cashier needs the counter's drawer open; the sale opens it.
    REQUIRE(counter.loginWithPin(u"2222"_s));
    CHECK(counter.drawerInfo()[u"mode"_s].toString() == u"drawer"_s);
    counter.addItem(u"draft-beer"_s);
    REQUIRE(counter.tender(u"cash"_s));
    CHECK_FALSE(counter.closeCheck());
    counter.entryKey(u"10000"_s);
    REQUIRE(counter.openDrawerSession());
    REQUIRE(counter.closeCheck());
    CHECK(counter.drawerInfo()[u"name"_s].toString() == u"Counter drawer"_s);
    CHECK(counter.drawerInfo()[u"expected"_s].toString() == u"$106.60"_s);
    const int kicksForCashier = kicks.drawerKicks;
    CHECK(kicksForCashier == 2);                        // start + cash sale
    counter.logout();

    // A server on the same terminal keeps their own cash.
    REQUIRE(counter.loginWithPin(u"1111"_s));
    CHECK(counter.drawerInfo()[u"mode"_s].toString() == u"serverBank"_s);
    cashSale(counter);
    CHECK(counter.drawerInfo()[u"name"_s].toString() == u"Sam's bank"_s);
    CHECK(counter.drawerInfo()[u"expected"_s].toString() == u"$6.60"_s);
    CHECK(kicks.drawerKicks == kicksForCashier);        // the drawer stays shut
    counter.logout();

    // The manager changes it per person on the Employees screen.
    REQUIRE(counter.loginWithPin(u"1234"_s));
    const QVariantList staff = counter.adminRecords(u"employees"_s);
    int samIndex = -1;
    for (int i = 0; i < staff.size(); ++i) {
        if (staff[i].toMap()[u"name"_s] == u"Sam"_s)
            samIndex = i;
    }
    REQUIRE(samIndex >= 0);
    QVariantMap sam = staff[samIndex].toMap();
    CHECK(sam[u"cashMode"_s].toString().isEmpty());     // store setting
    sam[u"cashMode"_s] = u"bogus"_s;
    CHECK_FALSE(counter.adminSave(u"employees"_s, samIndex, sam));
    sam[u"cashMode"_s] = u"drawer"_s;
    REQUIRE(counter.adminSave(u"employees"_s, samIndex, sam));
    CHECK(shared.employees[samIndex].cashMode == "drawer");
    CHECK(app::employeeFromJson(app::toJson(shared.employees[samIndex])).cashMode == "drawer");
}

TEST_CASE("Checking out with open checks: store rule, per-employee override", "[bank][checkout]")
{
    auto samWithOpenCheck = [](app::PosData data, const std::string &samRule) {
        for (core::Employee &e : data.employees) {
            if (e.name == "Sam")
                e.checkout = samRule;
        }
        auto pos = std::make_unique<PosService>(data, nullptr);
        REQUIRE(pos->loginWithPin(u"1111"_s));
        cashSale(*pos);                          // opens Sam's bank
        pos->addItem(u"draft-beer"_s);           // ...and leaves a check open
        pos->releaseCheck();
        pos->entryKey(u"660"_s);
        return pos;
    };

    app::PosData strict = bankData();            // store: close checks first (the default)
    CHECK(strict.settings.checkoutNeedsClosedChecks);
    CHECK_FALSE(samWithOpenCheck(strict, "")->countDrawer());
    CHECK(samWithOpenCheck(strict, "anyTime")->countDrawer());

    app::PosData relaxed = bankData();
    relaxed.settings.checkoutNeedsClosedChecks = false;
    CHECK(samWithOpenCheck(relaxed, "")->countDrawer());
    CHECK_FALSE(samWithOpenCheck(relaxed, "closeChecks")->countDrawer());

    // Set on the manager screens.
    PosService pos(bankData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap store = pos.adminRecords(u"store"_s)[0].toMap();
    store[u"checkoutNeedsClosedChecks"_s] = false;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK_FALSE(pos.shared()->settings.checkoutNeedsClosedChecks);
    QVariantMap first = pos.adminRecords(u"employees"_s)[0].toMap();
    first[u"checkout"_s] = u"closeChecks"_s;
    REQUIRE(pos.adminSave(u"employees"_s, 0, first));
    CHECK(pos.shared()->employees[0].checkout == "closeChecks");
    first[u"checkout"_s] = u"never"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, 0, first));
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK_FALSE(back.checkoutNeedsClosedChecks);
    CHECK(app::employeeFromJson(app::toJson(pos.shared()->employees[0])).checkout == "closeChecks");
}

TEST_CASE("Which terminals have a cash drawer", "[bank][drawers]")
{
    app::PosData data = bankData();
    for (core::Employee &e : data.employees) {
        if (e.role == "cashier")
            e.cashMode = "drawer";
    }
    data.settings.terminals = {{"Host", "", "no"}};   // the host stand has no drawer
    PosShared shared(data, nullptr);
    Kicks kicks;
    shared.printer = &kicks;
    PosService host(&shared, u"Host"_s);
    PosService counter(&shared, u"Counter"_s);         // not listed: store setting (has one)
    CHECK(shared.settings.hasDrawer("Counter"));
    CHECK_FALSE(shared.settings.hasDrawer("Host"));

    // The drawer cashier can't take cash at the host stand...
    REQUIRE(host.loginWithPin(u"2222"_s));
    CHECK_FALSE(host.drawerInfo()[u"hasDrawer"_s].toBool());
    host.entryKey(u"10000"_s);
    CHECK_FALSE(host.openDrawerSession());
    CHECK_FALSE(host.noSale());
    host.addItem(u"draft-beer"_s);
    REQUIRE(host.tender(u"cash"_s));
    QSignalSpy notices(&host, &PosService::notice);
    CHECK_FALSE(host.closeCheck());
    CHECK(notices.last()[0].toString().contains(u"Host has no cash drawer"_s));
    host.logout();
    CHECK(kicks.drawerKicks == 0);

    // ...but a server with a bank can.
    REQUIRE(host.loginWithPin(u"1111"_s));
    cashSale(host);
    CHECK(host.drawerInfo()[u"name"_s].toString() == u"Sam's bank"_s);

    // With "no drawers" as the store setting, only terminals marked yes have one.
    shared.settings.terminalsHaveDrawer = false;
    shared.settings.terminals.push_back({"Counter", "", "yes"});
    CHECK(shared.settings.hasDrawer("Counter"));
    CHECK_FALSE(shared.settings.hasDrawer("Patio"));

    // Terminals screen.
    REQUIRE(counter.loginWithPin(u"1234"_s));
    QVariantMap t = counter.adminNewRecord(u"terminals"_s);
    t[u"name"_s] = u"Patio"_s;
    t[u"drawer"_s] = u"maybe"_s;
    CHECK_FALSE(counter.adminSave(u"terminals"_s, -1, t));
    t[u"drawer"_s] = u"yes"_s;
    REQUIRE(counter.adminSave(u"terminals"_s, -1, t));
    CHECK(shared.settings.hasDrawer("Patio"));
    CHECK(app::settingsFromJson(app::toJson(shared.settings)).terminals.back().drawer == "yes");
}

TEST_CASE("Counting by bills and coins, and counting blind", "[bank][drawerCount]")
{
    // Sam's bank: $6.60 of cash sales.
    app::PosData data = bankData();
    data.settings.blindDrawerCount = true;
    PosShared shared(data, nullptr);
    PosService pos(&shared, u"Front"_s);
    QSignalSpy notices(&pos, &PosService::notice);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    cashSale(pos);

    // Blind: what should be there is hidden from Sam until (and after) the count.
    QVariantMap d = pos.drawerInfo();
    CHECK(d[u"blind"_s].toBool());
    CHECK(d[u"expected"_s].toString().isEmpty());
    CHECK(d[u"cashSales"_s].toString().isEmpty());
    const QVariantList worth = d[u"denominations"_s].toList();
    REQUIRE(worth.size() == 10);
    CHECK(worth[0].toMap()[u"text"_s].toString() == u"$100.00"_s);
    CHECK(worth[6].toMap()[u"cents"_s].toLongLong() == 25);

    // 1 × $5, 1 × $1, 2 × 25¢, 1 × 5¢ = $6.55: five cents short, not said.
    QVariantList counts(10, 0);
    counts[4] = 1;
    counts[5] = 1;
    counts[6] = 2;
    counts[8] = 1;
    REQUIRE(pos.countDrawerPieces(0, counts));
    d = pos.drawerInfo();
    CHECK_FALSE(d[u"open"_s].toBool());
    CHECK(d[u"counted"_s].toString() == u"$6.55"_s);
    CHECK(d[u"hideResult"_s].toBool());
    CHECK(d[u"overShort"_s].toString().isEmpty());
    CHECK(d[u"countedPieces"_s].toList().size() == 4);
    CHECK(notices.last()[0].toString() == u"Sam's bank counted: $6.55"_s);
    const core::DrawerSession &bank = shared.drawers.back();
    CHECK(bank.blind);
    CHECK(bank.overShort().cents() == -5);
    CHECK(bank.countedPieces == std::vector<std::pair<std::int64_t, int>>{{500, 1}, {100, 1}, {25, 2}, {5, 1}});
    CHECK(app::drawerFromJson(app::toJson(bank)) == bank);

    // A manager sees it, and the printout lists the bills and coins.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.blindCount());
    const core::Report r = pos.buildReport(u"drawer"_s);
    CHECK(!r.find("   2 x $0.25").empty());
    CHECK(!r.find("Counted by Sam (blind)").empty());
    CHECK(!r.find("Short").empty());

    // Per employee: the manager counts blind too; Sam sees.
    const QString me = pos.userName();
    QVariantList staff = pos.adminRecords(u"employees"_s);
    for (qsizetype i = 0; i < staff.size(); ++i) {
        QVariantMap e = staff[i].toMap();
        const QString name = e[u"name"_s].toString();
        if (name != me && name != u"Sam"_s)
            continue;
        e[u"blindCount"_s] = name == me ? u"blind"_s : u"sees"_s;
        REQUIRE(pos.adminSave(u"employees"_s, int(i), e));
        CHECK(app::employeeFromJson(app::toJson(shared.employees[i])).blindCount == (name == me ? "blind" : "sees"));
    }
    CHECK(pos.blindCount());
    QVariantMap wrong = pos.adminRecords(u"employees"_s)[0].toMap();
    wrong[u"blindCount"_s] = u"maybe"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, 0, wrong));

    // Store Settings: blind off, and a till of 20s, 10s, 1s and quarters.
    QVariantMap store = pos.adminRecords(u"store"_s)[0].toMap();
    CHECK(store[u"cashDenominations"_s].toString() == u"100, 50, 20, 10, 5, 1, 0.25, 0.10, 0.05, 0.01"_s);
    store[u"blindDrawerCount"_s] = false;
    store[u"cashDenominations"_s] = u"$10, 20, 0.25, 1, 20"_s;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK_FALSE(shared.settings.blindDrawerCount);
    CHECK(shared.settings.cashDenominations == std::vector<std::int64_t>{2000, 1000, 100, 25});
    const auto back = app::settingsFromJson(app::toJson(shared.settings));
    CHECK(back.cashDenominations == shared.settings.cashDenominations);
}
