#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDeadlineTimer>
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
std::int64_t sell(app::PosService &pos, const QString &item = u"soda"_s)
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
    REQUIRE(pos.addItem(u"soda"_s));
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

    REQUIRE(pos.addItem(u"soda"_s));
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

    REQUIRE(pos.addItem(u"soda"_s));
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
    const QVariantList terminals = front.adminRecords(u"terminals"_s);
    int at = -1;
    for (int i = 0; i < terminals.size(); ++i)
        if (terminals[i].toMap()[u"name"_s] == u"Truck"_s)
            at = i;
    REQUIRE(at >= 0);
    QVariantMap t = terminals[at].toMap();
    t[u"service"_s] = u"foodTruck"_s;
    REQUIRE(front.adminSave(u"terminals"_s, at, t));
    CHECK(truck.serviceMode() == u"foodTruck"_s);
    CHECK(truck.userName() == u"Counter"_s);
    CHECK(front.serviceMode() == u"restaurant"_s);

    // Kept with the settings.
    const core::PosSettings again = app::settingsFromJson(app::toJson(shared.settings));
    CHECK(std::ranges::any_of(again.terminals, [](const core::TerminalConfig &x) {
        return x.name == "Truck" && x.service == "foodTruck";
    }));
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

    // The order screen's Orders button (where restaurants have Tables), and Manager there.
    REQUIRE(c.ruleShows(QJsonObject{{u"service"_s, u"foodTruck"_s}}));
    CHECK_FALSE(c.ruleShows(QJsonObject{{u"service"_s, u"restaurant,retail"_s}}));
    c.activate(u"flow-orders"_s);
    CHECK(c.pageId() == u"truck-orders"_s);
    c.activate(u"manager"_s);
    CHECK(c.pageId() == u"login"_s);
    for (const QChar d : u"1234"_s)
        pos.pinKey(QString(d));
    c.login();
    CHECK(c.pageId() == u"manager"_s);

    // Done there: Log Out, back at the counter's menu.
    pos.logout();
    CHECK(c.pageId() == u"index-lunch"_s);

    // A sale paid: the menu again, for the next one.
    REQUIRE(pos.addItem(u"soda"_s));
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

TEST_CASE("Food truck: pagers, Ready and Picked Up at the window", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    QSignalSpy notices(&pos, &app::PosService::notice);
    const auto said = [&] {
        QStringList out;
        for (const QList<QVariant> &n : notices)
            out << n.first().toString();
        notices.clear();
        return out;
    };

    // #1 Maria with pager 12, paid: waiting at the window.
    REQUIRE(pos.addItem(u"caesar"_s));
    CHECK_FALSE(pos.setPager(1000));
    REQUIRE(pos.setPager(12));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Maria"_s}}));
    const qint64 first = pos.checkInfo()[u"id"_s].toLongLong();
    CHECK(pos.checkInfo()[u"pager"_s].toInt() == 12);
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());

    // The next order can't have pager 12 while Maria has it.
    REQUIRE(pos.addItem(u"soda"_s));
    CHECK_FALSE(pos.setPager(12));
    REQUIRE(pos.setPager(7));
    REQUIRE(pos.sendOrder());   // not paid yet: on the list too

    QVariantList orders = pos.truckOrders();
    REQUIRE(orders.size() == 2);
    CHECK(orders[0].toMap()[u"label"_s].toString() == u"#1 Maria"_s);
    CHECK(orders[0].toMap()[u"pager"_s].toInt() == 12);
    CHECK(orders[0].toMap()[u"paid"_s].toBool());
    CHECK_FALSE(orders[1].toMap()[u"paid"_s].toBool());

    // Ready: the screens say which pager to buzz on the pager system (again: again).
    said();
    REQUIRE(pos.orderReady(first));
    CHECK(said().contains(u"#1 Maria is ready: buzz pager 12"_s));
    orders = pos.truckOrders();
    CHECK(orders.last().toMap()[u"ready"_s].toBool());   // called ones after those being made
    REQUIRE(pos.orderReady(first));
    CHECK(said().contains(u"#1 Maria is ready: buzz pager 12"_s));

    // Picked up: off the list, and pager 12 is free again.
    REQUIRE(pos.orderPickedUp(first));
    CHECK(pos.truckOrders().size() == 1);
    CHECK_FALSE(pos.orderReady(first));
    CHECK(pos.checkInfo()[u"orderNumber"_s].toInt() == 2);   // still on #2
}

TEST_CASE("Setup guide: a food truck from the first page", "[truck]")
{
    app::PosData d = test::seedPosData();
    d.settings.setupDone = false;
    app::PosShared shared(d, nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.setupInfo()[u"serviceMode"_s].toString().isEmpty());
    REQUIRE(pos.setupStore(u"Taco Wheels"_s, {}, u"foodTruck"_s));
    CHECK(shared.settings.serviceMode == "foodTruck");
    CHECK(pos.userName() == u"Morgan (Manager)"_s);   // the manager setting it up stays
    pos.logout();
    CHECK(pos.userName() == u"Counter"_s);           // then the counter, nobody to log in
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.setupStore(u"Taco Wheels"_s, {}));   // the name only: still a truck
    CHECK(shared.settings.serviceMode == "foodTruck");
}

// --- more of a truck's day ----------------------------------------------------------

namespace {

struct TicketPrinter : app::PosPrinter {
    std::vector<std::string> kitchen, receipts;
    void printKitchen(const core::PosSettings &, const core::Check &c, const std::vector<core::OrderLine> &, bool) override
    {
        kitchen.push_back(c.label);
    }
    void printReceipt(const core::PosSettings &, const core::Check &c, const std::string &) override { receipts.push_back(c.label); }
    void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
    void openDrawer(const core::PosSettings &, const std::string &) override {}
};

// Process events for a while (timers fire).
void spin(int msec)
{
    QDeadlineTimer deadline(msec);
    while (!deadline.hasExpired())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

} // namespace

TEST_CASE("Food truck: order numbers go on after a restart, and start again at #1 the next day", "[truck]")
{
    test::RecordingSink sink;
    app::PosShared shared(truckData(), &sink);
    std::int64_t clock = 1'700'000'000'000;
    shared.setClock([&clock] { return clock; });
    app::PosService pos(&shared, u"Truck"_s);
    sell(pos);
    sell(pos);
    CHECK(shared.closedToday.back().orderNumber == 2);

    // The computer restarts: today's checks come back from the database.
    app::PosData again = truckData();
    for (const auto &[id, c] : sink.checks)
        if (c.status == core::CheckStatus::Closed)
            again.closedToday.push_back(*app::checkFromJson(app::toJson(c)));   // as stored
    again.lastCheckId = shared.lastCheckId;
    again.currentDay = shared.day;
    app::PosShared restarted(again, nullptr);
    restarted.setClock([&clock] { return clock; });
    app::PosService after(&restarted, u"Truck"_s);
    CHECK(after.serviceInfo()[u"counter"_s].toBool());
    REQUIRE(after.addItem(u"soda"_s));
    CHECK(after.checkInfo()[u"orderNumber"_s].toInt() == 3);
    after.openDrawerSession();
    REQUIRE(after.tender(u"cash"_s));
    REQUIRE(after.closeCheck());

    // End of Day (a manager steps in), then tomorrow's first order is #1.
    REQUIRE(after.loginWithPin(u"1234"_s));
    after.entryKey(u"0"_s);                      // counted (short: it's a test)
    REQUIRE(after.countDrawer());
    clock += 3'600'000;
    REQUIRE(after.endOfDay());
    after.logout();
    CHECK(after.userName() == u"Counter"_s);
    REQUIRE(after.addItem(u"soda"_s));
    CHECK(after.checkInfo()[u"orderNumber"_s].toInt() == 1);
    CHECK(after.checkInfo()[u"label"_s].toString() == u"#1"_s);
}

TEST_CASE("Food truck: kitchen tickets and receipts say #1 Maria", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    TicketPrinter printer;
    app::PosService pos(&shared, u"Truck"_s);
    pos.setPrinter(&printer);
    REQUIRE(pos.addItem(u"caesar"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Maria"_s}}));
    REQUIRE(pos.sendOrder());
    REQUIRE(printer.kitchen.size() == 1);
    CHECK(printer.kitchen.back() == "#1 Maria");
    REQUIRE(pos.printReceipt());
    REQUIRE(printer.receipts.size() == 1);
    CHECK(printer.receipts.back() == "#1 Maria");
}

TEST_CASE("Food truck: practice orders get no number and never reach the Orders list", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // a manager steps in to turn practice on
    REQUIRE(pos.setTraining(true));
    pos.logout();
    REQUIRE(pos.serviceInfo()[u"counter"_s].toBool());
    REQUIRE(pos.addItem(u"caesar"_s));
    CHECK(pos.checkInfo()[u"orderNumber"_s].toInt() == 0);
    CHECK_FALSE(pos.setPager(3));
    REQUIRE(pos.sendOrder());
    CHECK(pos.truckOrders().isEmpty());
}

TEST_CASE("Food truck: an order still being rung in isn't on the Orders list", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    REQUIRE(pos.addItem(u"caesar"_s));
    CHECK(pos.truckOrders().isEmpty());          // nothing sent, not paid
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.truckOrders().size() == 1);
    CHECK_FALSE(pos.truckOrders().first().toMap()[u"paid"_s].toBool());
    CHECK(pos.truckOrders().first().toMap()[u"items"_s].toString() == u"Caesar"_s);
}

TEST_CASE("Food truck: two screens share the day's numbers and the pagers", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService window(&shared, u"Window"_s);
    app::PosService second(&shared, u"Second"_s);
    REQUIRE(window.addItem(u"soda"_s));
    REQUIRE(second.addItem(u"soda"_s));
    CHECK(window.checkInfo()[u"orderNumber"_s].toInt() == 1);
    CHECK(second.checkInfo()[u"orderNumber"_s].toInt() == 2);
    REQUIRE(window.setPager(5));
    CHECK_FALSE(second.setPager(5));             // still out with #1
    REQUIRE(second.setPager(6));
    // Each screen sees every waiting order.
    REQUIRE(window.sendOrder());
    REQUIRE(second.sendOrder());
    CHECK(window.truckOrders().size() == 2);
    CHECK(second.truckOrders().size() == 2);
}

TEST_CASE("Food truck: the kitchen calls an order only when every station is done", "[truck]")
{
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    app::PosService kitchen(&shared, u"Kitchen"_s);
    QSignalSpy notices(&pos, &app::PosService::notice);
    REQUIRE(pos.addItem(u"caesar"_s));   // the kitchen's
    REQUIRE(pos.addItem(u"soda"_s));     // the bar's
    REQUIRE(pos.setPager(8));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.sendOrder());
    const qint64 sentAt = pos.kitchenTickets().first().toMap()[u"sentAt"_s].toLongLong();
    const auto called = [&] {
        for (const QList<QVariant> &n : notices)
            if (n.first().toString() == u"#1 is ready: buzz pager 8"_s)
                return true;
        return false;
    };
    REQUIRE(kitchen.bumpTicket(id, sentAt, u"kitchen"_s));
    CHECK_FALSE(called());
    CHECK_FALSE(pos.truckOrders().first().toMap()[u"ready"_s].toBool());
    REQUIRE(kitchen.bumpTicket(id, sentAt, u"bar"_s));
    CHECK(called());
    CHECK(pos.truckOrders().first().toMap()[u"ready"_s].toBool());
}

TEST_CASE("Food truck: pager rules", "[truck]")
{
    // Not on a restaurant's checks.
    {
        app::PosShared shared(test::seedPosData(), nullptr);
        app::PosService pos(&shared, u"Front"_s);
        REQUIRE(pos.loginWithPin(u"2222"_s));
        REQUIRE(pos.addItem(u"soda"_s));
        CHECK_FALSE(pos.setPager(4));
        CHECK(pos.checkInfo()[u"orderNumber"_s].toInt() == 0);
        CHECK(pos.checkInfo()[u"label"_s].toString().startsWith(u"Quick"_s));
    }
    app::PosShared shared(truckData(), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    CHECK_FALSE(pos.setPager(4));                // no order yet
    REQUIRE(pos.addItem(u"soda"_s));
    CHECK_FALSE(pos.setPager(-1));
    REQUIRE(pos.setPager(4));
    REQUIRE(pos.setPager(0));                    // 0: none, after all
    CHECK(pos.checkInfo()[u"pager"_s].toInt() == 0);
    REQUIRE(pos.setPager(4));
    pos.openDrawerSession();
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());                   // paid, waiting with pager 4
    REQUIRE(pos.addItem(u"soda"_s));
    CHECK_FALSE(pos.setPager(4));
    CHECK_FALSE(pos.orderPickedUp(999999));      // no such order
    REQUIRE(pos.orderPickedUp(shared.closedToday.back().id));
    CHECK_FALSE(pos.orderPickedUp(shared.closedToday.back().id));   // already
    REQUIRE(pos.setPager(4));                    // back for the next guest
}

TEST_CASE("Food truck: the maintainer's code doesn't become who's working", "[truck]")
{
    app::PosShared shared(truckData(true), nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    REQUIRE(test::loginMaintainer(pos));
    CHECK(pos.serviceInfo()[u"person"_s].toString().isEmpty());
    CHECK_FALSE(pos.serviceInfo()[u"counter"_s].toBool());
    pos.logout();
    CHECK_FALSE(pos.loggedIn());                 // still asking who's working
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.serviceInfo()[u"person"_s].toString() == u"Sam"_s);
}

TEST_CASE("Food truck: idle never logs the counter out; someone who stepped in goes back to it", "[truck][ui]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    app::PosData d = truckData();
    d.settings.autoLogoutMinutes = 1;
    app::PosShared shared(d, nullptr);
    app::PosService pos(&shared, u"Truck"_s);
    LayoutController c(*l);
    c.setMealPeriod(u"lunch"_s);
    c.setPos(&pos);
    c.setIdleTimeoutForTesting(50);
    REQUIRE(pos.addItem(u"soda"_s));
    spin(250);
    CHECK(pos.userName() == u"Counter"_s);
    CHECK(pos.hasCheck());                       // the order in progress stays

    REQUIRE(pos.loginWithPin(u"1234"_s));        // a manager steps in, then walks away
    c.setIdleTimeoutForTesting(50);
    spin(250);
    CHECK(pos.userName() == u"Counter"_s);
    CHECK(pos.hasCheck());
    CHECK(c.pageId() == u"index-lunch"_s);
}

TEST_CASE("Restaurant and retail screens keep their own buttons", "[truck][ui]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    const auto rule = [&](const char *page, const char *zone) {
        const layout::Page *p = l->page(QString::fromLatin1(page));
        REQUIRE(p);
        const layout::Zone *z = p->zone(QString::fromLatin1(zone));
        REQUIRE(z);
        return z->extra.value(u"showWhen").toObject();
    };
    const QJsonObject tables = rule("order-template", "flow-tables"), orders = rule("order-template", "flow-orders"),
                      logOut = rule("order-template", "flow-logout"), back = rule("login", "back-to-order"),
                      switchPerson = rule("login", "switch-person"), manager = rule("order-template", "flow-manager");

    // A restaurant: Tables, no truck or retail buttons; logging in goes to the floor.
    {
        app::PosShared shared(test::seedPosData(), nullptr);
        app::PosService pos(&shared, u"Front"_s);
        LayoutController c(*l);
        c.setMealPeriod(u"lunch"_s);
        c.setPos(&pos);
        CHECK(c.pageId() == u"login"_s);
        REQUIRE(pos.loginWithPin(u"1111"_s));
        CHECK(c.pageId() == u"tables"_s);
        CHECK(c.ruleShows(tables));
        CHECK_FALSE(c.ruleShows(orders));
        CHECK_FALSE(c.ruleShows(logOut));
        CHECK_FALSE(c.ruleShows(manager));
        CHECK_FALSE(c.ruleShows(back));
        CHECK_FALSE(c.ruleShows(switchPerson));
    }
    // Retail: a login still, then the menu; Log Out where Tables were.
    {
        app::PosData d = test::seedPosData();
        d.settings.serviceMode = "retail";
        app::PosShared shared(d, nullptr);
        app::PosService pos(&shared, u"Shop"_s);
        CHECK_FALSE(pos.loggedIn());
        LayoutController c(*l);
        c.setMealPeriod(u"lunch"_s);
        c.setPos(&pos);
        CHECK(c.pageId() == u"login"_s);
        REQUIRE(pos.loginWithPin(u"2222"_s));
        CHECK(c.pageId() == u"index-lunch"_s);
        CHECK_FALSE(c.ruleShows(tables));
        CHECK_FALSE(c.ruleShows(orders));
        CHECK(c.ruleShows(logOut));
        CHECK_FALSE(c.ruleShows(manager));   // a cashier: no way into the manager's pages
        c.activate(u"flow-logout"_s);
        CHECK_FALSE(pos.loggedIn());
        CHECK(c.pageId() == u"login"_s);
        // A manager: Manager on the order page, to the manager's pages (and back to restaurant from there).
        REQUIRE(pos.loginWithPin(u"1234"_s));
        CHECK(c.pageId() == u"index-lunch"_s);
        CHECK(c.ruleShows(manager));
        c.activate(u"flow-manager"_s);
        CHECK(c.pageId() == u"manager"_s);
    }
    // A truck: Orders; on the PIN page, Back to Ordering and Switch Person.
    {
        app::PosShared shared(truckData(), nullptr);
        app::PosService pos(&shared, u"Truck"_s);
        LayoutController c(*l);
        c.setPos(&pos);
        CHECK_FALSE(c.ruleShows(tables));
        CHECK(c.ruleShows(orders));
        CHECK_FALSE(c.ruleShows(logOut));
        CHECK(c.ruleShows(back));
        CHECK(c.ruleShows(switchPerson));
    }
}

TEST_CASE("Food truck: a whole day with the store's cash setting as shipped, to End of Day", "[truck]")
{
    app::PosData d = truckData();
    d.settings.cashMode = app::settingsFromJson(test::readSeed("pos/settings.json").object()).cashMode;
    test::RecordingSink sink;
    app::PosShared shared(d, &sink);
    std::int64_t clock = 1'700'000'000'000;
    shared.setClock([&clock] { return clock; });
    app::PosService pos(&shared, u"Truck"_s);

    // Cash and card sales by the counter, no drawer opened by hand first.
    for (const char *how : {"cash", "credit", "cash"}) {
        REQUIRE(pos.addItem(u"soda"_s));
        INFO(how << ": " << pos.drawerInfo()[u"text"_s].toString().toStdString());
        REQUIRE(pos.tender(QString::fromLatin1(how)));
        REQUIRE(pos.closeCheck());
    }
    CHECK(shared.closedToday.size() == 3);

    // A manager steps in for End of Day: what's in the way is something they can do.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QVariantMap day = pos.dayInfo();
    INFO(day[u"blockers"_s].toStringList().join(u" | "_s).toStdString());
    for (const QVariant &b : pos.drawerInfo()[u"others"_s].toList())
        UNSCOPED_INFO(QJsonDocument::fromVariant(b).toJson(QJsonDocument::Compact).toStdString());
    if (!day[u"ready"_s].toBool()) {
        // The Counter's cash: counted and checked out like anyone's.
        for (const QVariant &b : pos.drawerInfo()[u"others"_s].toList()) {
            pos.entryKey(u"0"_s);
            REQUIRE(pos.countDrawerById(b.toMap()[u"id"_s].toLongLong()));
        }
    }
    clock += 3'600'000;
    REQUIRE(pos.endOfDay());
    pos.logout();
    CHECK(pos.userName() == u"Counter"_s);
}
