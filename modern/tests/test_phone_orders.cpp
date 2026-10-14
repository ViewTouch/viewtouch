#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Phone orders and deliveries: a regular's last order, a name before Send,
// the ready time, the delivery fee, drivers.

namespace {

void openDrawer(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

QString customerId(PosService &pos, const QString &name)
{
    for (const core::CustomerRecord &c : pos.shared()->customers)
        if (QString::fromStdString(c.name) == name)
            return QString::fromStdString(c.id);
    return {};
}

QStringList lineNames(const PosService &pos)
{
    QStringList out;
    for (const QVariant &v : pos.lines())
        out << v.toMap()[u"name"_s].toString();
    return out;
}

} // namespace

TEST_CASE("Phone orders: same as last time, for a regular", "[phoneorders]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana Ruiz"_s}, {u"phone"_s, u"555-0142"_s}}));
    const QString dana = customerId(pos, u"Dana Ruiz"_s);

    // Their first order: nothing to repeat yet.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.useCustomer(dana));
    CHECK(pos.checkInfo()[u"lastOrder"_s].toString().isEmpty());
    CHECK_FALSE(pos.sameAsLastTime());
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.addItem(u"water"_s));
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // Kept with the customer, through a save and load.
    const core::CustomerRecord *r = pos.shared()->customer(dana.toStdString());
    REQUIRE(r);
    CHECK(r->lastOrder.size() == 3);
    CHECK(app::customerFromJson(app::toJson(*r)) == *r);

    // Next time: one touch.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.useCustomer(dana));
    CHECK(pos.checkInfo()[u"lastOrder"_s] == u"2 × Coffee, Water"_s);
    REQUIRE(pos.sameAsLastTime());
    CHECK(lineNames(pos) == QStringList{u"Coffee"_s, u"Coffee"_s, u"Water"_s});
    CHECK_FALSE(pos.lines()[0].toMap()[u"sent"_s].toBool());     // new, not yet sent

    // Sold out since: left off, and said so.
    pos.releaseCheck();
    for (core::MenuItem &m : pos.shared()->menu)
        if (m.id == "water")
            m.available = false;
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.useCustomer(dana));
    REQUIRE(pos.sameAsLastTime());
    CHECK(lineNames(pos) == QStringList{u"Coffee"_s, u"Coffee"_s});
}

TEST_CASE("Phone orders: a name before Send, by store, person and screen", "[phoneorders]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));

    // The store doesn't ask: it goes.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.sendOrder());
    pos.releaseCheck();

    pos.shared()->settings.requireOrderName = true;
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.checkInfo()[u"needsWho"_s].toBool());
    CHECK_FALSE(pos.sendOrder());
    CHECK(pos.lines()[0].toMap()[u"sent"_s] == false);
    REQUIRE(pos.setCustomer({{u"name"_s, u"Sam"_s}}));
    CHECK(pos.sendOrder());
    pos.releaseCheck();

    // A delivery needs the address too.
    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Sam"_s}}));
    CHECK_FALSE(pos.sendOrder());
    REQUIRE(pos.setCustomer({{u"name"_s, u"Sam"_s}, {u"address"_s, u"9 Elm"_s}}));
    CHECK(pos.sendOrder());
    pos.releaseCheck();

    // Not for dine-in.
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.sendOrder());
    pos.releaseCheck();

    // This person: no. This screen: yes (the screen wins).
    for (core::Employee &e : pos.shared()->employees)
        if (e.id == "manager")
            e.requireName = "no";
    CHECK_FALSE(pos.nameRequired());
    pos.shared()->settings.terminals.push_back({});
    pos.shared()->settings.terminals.back().name = pos.terminalName().toStdString();
    pos.shared()->settings.terminals.back().requireName = "yes";
    CHECK(pos.nameRequired());
}

TEST_CASE("Phone orders: the ready time follows the kitchen", "[phoneorders]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = QDateTime(QDate::currentDate(), QTime(17, 0)).toMSecsSinceEpoch();
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    auto &st = pos.shared()->settings;
    st.takeoutMinutes = 15;
    st.deliveryMinutes = 35;
    st.minutesPerOrderWaiting = 3;

    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.checkInfo()[u"quote"_s] == 15);
    pos.releaseCheck();

    // Three orders cooking: 15 + 3 × 3 = 24, rounded to 25.
    for (int i = 0; i < 3; ++i) {
        REQUIRE(pos.startCheck(core::CheckType::Quick));
        REQUIRE(pos.addItem(u"coffee"_s));
        REQUIRE(pos.sendOrder());
        pos.releaseCheck();
    }
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.checkInfo()[u"quote"_s] == 25);
    // Sent: the time they were told stays, whatever the kitchen does next.
    REQUIRE(pos.sendOrder());
    CHECK(pos.checkInfo()[u"quote"_s] == 0);
    CHECK(pos.checkInfo()[u"promised"_s].toString().startsWith(u"5:25"_s));
    pos.releaseCheck();

    // A delivery: its own minutes.
    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(pos.checkInfo()[u"quote"_s] == 50);   // 35 + 4 cooking × 3 = 47 -> 50
}

TEST_CASE("Deliveries: the fee, a driver takes orders out, back, paid into their bank", "[phoneorders][delivery]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.deliveryFee = vt::Money::fromCents(350);

    // A driver.
    QVariantMap e = pos.adminNewRecord(u"employees"_s);
    e[u"name"_s] = u"Lou"_s;
    e[u"role"_s] = u"driver"_s;
    e[u"pin"_s] = u"7777"_s;
    REQUIRE(pos.adminSave(u"employees"_s, -1, e));
    QString lou;
    for (const QVariant &v : pos.drivers())
        if (v.toMap()[u"name"_s] == u"Lou"_s)
            lou = v.toMap()[u"id"_s].toString();
    REQUIRE_FALSE(lou.isEmpty());

    // The fee comes with the first item, and goes if the order is emptied.
    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    CHECK(pos.lines().isEmpty());
    REQUIRE(pos.addItem(u"coffee"_s));
    CHECK(lineNames(pos) == QStringList{u"Coffee"_s, u"Delivery fee"_s});
    CHECK_FALSE(pos.lines()[1].toMap()[u"countable"_s].toBool());
    pos.selectLine(pos.lines()[0].toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.voidItem());
    CHECK(pos.lines().isEmpty());
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Ana"_s}, {u"address"_s, u"12 Oak St"_s}}));
    CHECK(pos.totals()[u"subtotal"_s] == u"$6.25"_s);     // 2.75 + 3.50
    REQUIRE(pos.sendOrder());
    const qint64 ana = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();

    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Bo"_s}, {u"address"_s, u"3 Pine"_s}}));
    const qint64 bo = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();

    // On the board: Ana cooking, Bo not sent.
    QVariantList board = pos.deliveries();
    REQUIRE(board.size() == 2);
    const auto stateOf = [&](qint64 id) {
        for (const QVariant &v : pos.deliveries())
            if (v.toMap()[u"id"_s].toLongLong() == id)
                return v.toMap()[u"state"_s].toString();
        return QString();
    };
    CHECK(stateOf(ana) == u"cooking"_s);
    CHECK(stateOf(bo) == u"new"_s);
    CHECK_FALSE(pos.sendOut({bo}, lou));                    // not sent to the kitchen yet
    CHECK_FALSE(pos.sendOut({ana}, u""_s));                 // who's driving?

    // Out with Lou: still the check of whoever took the order.
    REQUIRE(pos.sendOut({ana}, lou));
    CHECK(stateOf(ana) == u"out"_s);
    CHECK_FALSE(pos.sendOut({ana}, lou));                   // already out
    for (const QVariant &v : pos.drivers())
        if (v.toMap()[u"id"_s] == lou)
            CHECK(v.toMap()[u"out"_s] == 1);
    CHECK(QString::fromStdString(pos.shared()->open.at(ana).serverName) == u"Morgan (Manager)"_s);
    CHECK(QString::fromStdString(pos.shared()->open.at(ana).driverName) == u"Lou"_s);

    // Back.
    REQUIRE(pos.deliveryBack(ana));
    CHECK(stateOf(ana) == u"back"_s);

    // Lou pays it in: into Lou's own bank, not the terminal's drawer.
    REQUIRE(pos.loginWithPin(u"7777"_s));
    pos.entryKey(u"5000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.openCheck(ana));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(stateOf(ana).isEmpty());                          // closed: off the board
    const core::Check &paid = pos.shared()->closedToday.back();
    CHECK(paid.serverName == "Morgan (Manager)");           // the sale is the order taker's
    const auto bank = std::ranges::find(pos.shared()->drawers, paid.drawerSession, &core::DrawerSession::id);
    REQUIRE(bank != pos.shared()->drawers.end());
    CHECK(bank->employeeId == lou.toStdString());           // the cash is in Lou's bank

    // The Drivers report.
    const QVariantMap report = pos.report(u"drivers"_s);
    bool found = false;
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QVariantList cells = v.toMap()[u"cells"_s].toList();
        if (cells.value(0) == u"Lou"_s) {
            found = true;
            CHECK(cells.value(1) == u"1"_s);
            CHECK(cells.value(3) == u"$3.50"_s);
        }
    }
    CHECK(found);
}
