#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;

// How a store serves: a food truck opens straight to ordering and paying
// (nobody logs in), and a manager's PIN still opens the Manager screens.

namespace {

app::PosData truckData(bool askStaff = false)
{
    app::PosData d = test::seedPosData();
    d.settings.serviceMode = "foodTruck";
    d.settings.truckAskStaff = askStaff;
    return d;
}

// Rung in and paid in cash, as the counter does it all day.
std::int64_t sell(app::PosService &pos, const QString &item = u"water"_s)
{
    REQUIRE(pos.addItem(item));
    const std::int64_t id = pos.checkInfo()[u"id"_s].toLongLong();
    pos.openDrawerSession();   // the first sale's (already open after)
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    return id;
}

} // namespace

TEST_CASE("Food truck: no login, the Counter takes orders and payments", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);

    CHECK(pos.serviceMode() == u"foodTruck"_s);
    CHECK(pos.loggedIn());
    CHECK(pos.userName() == u"Counter"_s);
    CHECK(pos.serviceInfo()[u"counter"_s].toBool());
    CHECK(pos.can(u"order"_s));
    CHECK(pos.can(u"check.settle"_s));
    CHECK_FALSE(pos.can(u"manager"_s));

    // An item rung in starts the order: called by its number, from 1.
    REQUIRE(pos.addItem(u"water"_s));
    CHECK(pos.checkInfo()[u"orderNumber"_s].toInt() == 1);
    CHECK(pos.checkInfo()[u"label"_s].toString() == u"#1"_s);
    REQUIRE(pos.setCustomer({{u"name"_s, u"Maria"_s}}));
    CHECK(pos.checkInfo()[u"label"_s].toString() == u"#1 Maria"_s);
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(shared.closedToday.back().orderNumber == 1);
    CHECK(shared.closedToday.back().label == "#1 Maria");
    CHECK(shared.closedToday.back().serverName == "Counter");

    sell(pos);
    CHECK(shared.closedToday.back().orderNumber == 2);

    // Logging out (the idle timer, a Log Out button) leaves it as it is.
    pos.logout();
    CHECK(pos.loggedIn());
    CHECK(pos.userName() == u"Counter"_s);
}

TEST_CASE("Food truck: a name before Send or Pay, when the store says so", "[truck]")
{
    app::PosData d = truckData();
    d.settings.truckNeedsName = true;
    app::PosShared shared(d, nullptr);
    app::PosService pos(&shared, u"Truck"_s);

    REQUIRE(pos.addItem(u"water"_s));
    CHECK(pos.checkInfo()[u"needsWho"_s].toBool());
    CHECK_FALSE(pos.sendOrder());
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    CHECK_FALSE(pos.closeCheck());
    REQUIRE(pos.setCustomer({{u"name"_s, u"Lee"_s}}));
    CHECK_FALSE(pos.checkInfo()[u"needsWho"_s].toBool());
    REQUIRE(pos.closeCheck());
    CHECK(shared.closedToday.back().label == "#1 Lee");
}

TEST_CASE("Food truck: the kitchen bumps the order, the window calls it", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    app::PosService kitchen(&shared, u"Kitchen"_s);
    QSignalSpy notices(&pos, &app::PosService::notice);

    REQUIRE(pos.addItem(u"caesar"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Maria"_s}}));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());   // paid first: it goes to the kitchen now

    const QVariantList tickets = pos.kitchenTickets();
    REQUIRE(tickets.size() == 1);
    REQUIRE(kitchen.bumpTicket(id, tickets.first().toMap()[u"sentAt"_s].toLongLong(), {}));
    QStringList said;
    for (const QList<QVariant> &n : notices)
        said << n.first().toString();
    CHECK(said.contains(u"#1 Maria is ready"_s));
}

TEST_CASE("Food truck: a manager's PIN steps in; Log Out goes back to the counter", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);

    REQUIRE(pos.addItem(u"water"_s));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();

    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.can(u"manager"_s));
    CHECK(pos.serviceInfo()[u"steppedIn"_s].toBool());
    CHECK_FALSE(pos.serviceInfo()[u"counter"_s].toBool());
    CHECK(pos.checkInfo()[u"id"_s].toLongLong() == id);   // the order in progress is still here

    pos.logout();
    CHECK(pos.userName() == u"Counter"_s);
    CHECK(pos.serviceInfo()[u"counter"_s].toBool());
    CHECK(pos.checkInfo()[u"id"_s].toLongLong() == id);
}

TEST_CASE("Food truck: asking who's working credits them, until Switch Person", "[truck]")
{
    app::PosShared shared(truckData(true), nullptr);
    app::PosService pos(&shared, u"Truck"_s);

    // Nobody yet: the login page asks.
    CHECK_FALSE(pos.loggedIn());
    REQUIRE(pos.loginWithPin(u"2222"_s));   // Casey, a cashier
    CHECK(pos.serviceInfo()[u"counter"_s].toBool());
    CHECK(pos.serviceInfo()[u"person"_s].toString() == u"Casey"_s);
    CHECK(pos.clockedIn());
    sell(pos);
    CHECK(shared.closedToday.back().serverName == "Casey");

    // A manager steps in and out: Casey is still working.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.logout();
    CHECK(pos.userName() == u"Casey"_s);

    // Idle doesn't log Casey out (the screen keeps her), Switch Person does.
    REQUIRE(pos.switchCounterPerson());
    CHECK_FALSE(pos.loggedIn());
    REQUIRE(pos.loginWithPin(u"1111"_s));   // Sam works now
    CHECK(pos.serviceInfo()[u"person"_s].toString() == u"Sam"_s);
}

TEST_CASE("Food truck: switched on and off in Store Settings, or for one screen", "[truck]")
{
    app::PosData d = test::seedPosData();
    d.settings.terminals.push_back({});
    d.settings.terminals.back().name = "Truck";
    app::PosShared shared(d, nullptr);
    app::PosService truck(&shared, u"Truck"_s);
    app::PosService front(&shared, u"Front"_s);
    CHECK(truck.serviceMode() == u"restaurant"_s);
    CHECK_FALSE(truck.loggedIn());

    // The whole store: every screen opens as the counter at once.
    REQUIRE(front.loginWithPin(u"1234"_s));
    QVariantMap store = front.adminRecords(u"store"_s).first().toMap();
    store[u"serviceMode"_s] = u"foodTruck"_s;
    REQUIRE(front.adminSave(u"store"_s, 0, store));
    CHECK(truck.loggedIn());
    CHECK(truck.userName() == u"Counter"_s);
    CHECK(front.userName() == u"Morgan (Manager)"_s);   // the manager here stays
    store[u"serviceMode"_s] = u"bus"_s;
    CHECK_FALSE(front.adminSave(u"store"_s, 0, store));

    // Back to a restaurant: the Counter is nobody to stay logged in as.
    store[u"serviceMode"_s] = u"restaurant"_s;
    REQUIRE(front.adminSave(u"store"_s, 0, store));
    CHECK(shared.settings.serviceMode.empty());
    CHECK_FALSE(truck.loggedIn());

    // One screen only (a restaurant's truck).
    QVariantMap t = front.adminRecords(u"terminals"_s).first().toMap();
    REQUIRE(t[u"name"_s].toString() == u"Truck"_s);
    t[u"service"_s] = u"foodTruck"_s;
    REQUIRE(front.adminSave(u"terminals"_s, 0, t));
    CHECK(truck.serviceMode() == u"foodTruck"_s);
    CHECK(truck.userName() == u"Counter"_s);
    CHECK(front.serviceMode() == u"restaurant"_s);

    // Kept with the settings.
    const core::PosSettings again = app::settingsFromJson(app::toJson(shared.settings));
    CHECK(again.terminals.front().service == "foodTruck");
    CHECK(again.serviceMode.empty());
}

TEST_CASE("Food truck screens: the menu first, Manager asks for a PIN", "[truck][ui]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    LayoutController c(*l);
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);

    CHECK(c.pageId() == u"index-lunch"_s);   // no login page

    // The order screen's Manager button (where restaurants have Tables).
    REQUIRE(c.ruleShows(QJsonObject{{u"service"_s, u"foodTruck"_s}}));
    CHECK_FALSE(c.ruleShows(QJsonObject{{u"service"_s, u"restaurant,retail"_s}}));
    c.activate(u"flow-manager"_s);
    CHECK(c.pageId() == u"login"_s);
    for (const QChar d : u"1234"_s)
        pos.pinKey(QString(d));
    c.login();
    CHECK(c.pageId() == u"manager"_s);

    // Done there: Log Out, back at the counter's menu.
    pos.logout();
    CHECK(c.pageId() == u"index-lunch"_s);

    // A sale paid: the menu again, for the next one.
    REQUIRE(pos.addItem(u"water"_s));
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(c.pageId() == u"index-lunch"_s);
}

TEST_CASE("Food truck: its kitchen screen stays on the kitchen", "[truck][ui]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    app::PosShared shared(test::seedPosData(), nullptr);
    app::PosService pos(&shared, u"Kitchen"_s);
    LayoutController c(*l);
    c.setPos(&pos);
    REQUIRE(c.showPage(u"kitchen"_s));

    // The store becomes a food truck: the kitchen screen's session is the counter's now.
    shared.settings.serviceMode = "foodTruck";
    emit shared.adminChanged();
    REQUIRE(pos.serviceInfo()[u"counter"_s].toBool());
    CHECK(c.pageId() == u"kitchen"_s);
}
