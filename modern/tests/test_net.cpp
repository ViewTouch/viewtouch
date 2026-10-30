#include <functional>
#include <limits>
#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "net/build_info.hh"
#include "net/layout_hub.hh"
#include "net/pos_server.hh"
#include "net/remote_session.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QElapsedTimer>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest/QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QThread>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;

// M5: several terminals on one store.

namespace {

template <typename F>
bool waitFor(F condition, int msec = 3000)
{
    QElapsedTimer t;
    t.start();
    while (!condition() && t.elapsed() < msec) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

layout::Layout seedLayout()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

// A server with its shared store, and a way to add remote terminals.
struct Store {
    test::RecordingSink sink;
    PosShared shared{test::seedPosData(), &sink};
    net::LayoutHub hub{seedLayout()};
    net::PosServer server{&shared, &hub};

    Store()
    {
        shared.setClock([] { return std::int64_t(1'700'000'000'000); });
        REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    }

    // A device already paired under `name`.
    net::Credentials pairedDevice(const QString &name)
    {
        core::TerminalConfig t;
        t.name = name.toStdString();
        t.id = net::newDeviceId().toStdString();
        const QByteArray key = net::newDeviceKey();
        t.key = key.toBase64().toStdString();
        shared.settings.terminals.push_back(t);
        net::Credentials c;
        c.host = u"127.0.0.1"_s;
        c.port = server.port();
        c.terminalId = QString::fromStdString(t.id);
        c.terminalName = name;
        c.key = key;
        return c;
    }

    std::unique_ptr<net::RemoteSession> terminal(const QString &name)
    {
        auto t = std::make_unique<net::RemoteSession>(name);
        t->setCredentials(pairedDevice(name));
        t->connectTo(u"127.0.0.1"_s, server.port());
        REQUIRE(t->waitForWelcome(5000));
        return t;
    }
};

// A remote terminal with its page controller.
struct Screen {
    std::unique_ptr<net::RemoteSession> remote;
    LayoutController c;

    explicit Screen(std::unique_ptr<net::RemoteSession> r)
        : remote(std::move(r))
        , c(remote->layout())
    {
        c.setMealPeriod(u"lunch"_s);
        c.setPos(remote.get());
        c.setSaver([this](const layout::Layout &l, QString *) { remote->saveLayout(l); return true; });
        QObject::connect(remote.get(), &net::RemoteSession::layoutReceived, &c,
                         [this](const layout::Layout &l) { c.replaceLayout(l); });
    }

    void settle() { waitFor([&] { return !c.busy(); }); QCoreApplication::processEvents(); }
    void tap(const QString &zone)
    {
        c.activate(zone);
        settle();
    }
    void pin(const char *digits)
    {
        for (const char *d = digits; *d; ++d)
            remote->pinKey(QString(QChar(*d)));
        c.login();
        REQUIRE(waitFor([&] { return remote->loggedIn(); }));
        settle();
    }
};

} // namespace

TEST_CASE("Two terminals on one store: locks and live updates", "[net][shared]")
{
    PosShared shared(test::seedPosData(), nullptr);
    PosService front(&shared, u"Front"_s);
    PosService bar(&shared, u"Bar"_s);
    REQUIRE(front.loginWithPin(u"1111"_s));
    REQUIRE(bar.loginWithPin(u"2222"_s));

    QSignalSpy barSees(&bar, &PosService::openChecksChanged);
    REQUIRE(front.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(front.startCheck(core::CheckType::DineIn));
    front.addItem(u"coffee"_s);
    CHECK(barSees.size() >= 1);
    CHECK(bar.openChecks().size() == 1);
    CHECK(bar.tableStatus(u"T1"_s)[u"busyOn"_s].toString() == u"Front"_s);

    // The check is Front's while it is open there.
    QSignalSpy notices(&bar, &PosService::notice);
    CHECK(bar.selectTable(u"T1"_s) == PosService::TableFailed);
    CHECK(notices.last()[0].toString().contains(u"Front"_s));

    front.releaseCheck();
    CHECK(bar.selectTable(u"T1"_s) == PosService::TableOpened);
    CHECK(bar.lines().size() == 1);
    CHECK(front.tableStatus(u"T1"_s)[u"busyOn"_s].toString() == u"Bar"_s);

    // One sequence of check numbers across terminals.
    front.startCheck(core::CheckType::Takeout);
    CHECK(front.checkInfo()[u"id"_s].toLongLong() == 2);
}

TEST_CASE("A terminal going away frees its check", "[net][shared]")
{
    PosShared shared(test::seedPosData(), nullptr);
    PosService bar(&shared, u"Bar"_s);
    REQUIRE(bar.loginWithPin(u"2222"_s));
    qint64 id = 0;
    {
        PosService front(&shared, u"Front"_s);
        REQUIRE(front.loginWithPin(u"1111"_s));
        front.startCheck(core::CheckType::Takeout);
        id = front.checkInfo()[u"id"_s].toLongLong();
        CHECK_FALSE(bar.openCheck(id));
    }
    CHECK(bar.openCheck(id));
}

TEST_CASE("Deactivating someone logs them out everywhere", "[net][shared]")
{
    PosShared shared(test::seedPosData(), nullptr);
    PosService manager(&shared, u"Office"_s);
    PosService floor(&shared, u"Floor"_s);
    REQUIRE(manager.loginWithPin(u"1234"_s));
    REQUIRE(floor.loginWithPin(u"1111"_s));
    QSignalSpy loggedOut(&floor, &PosService::loggedInChanged);

    int sam = -1;
    for (int i = 0; i < int(shared.employees.size()); ++i)
        if (shared.employees[i].id == "sam") sam = i;
    REQUIRE(manager.adminDelete(u"employees"_s, sam));
    CHECK_FALSE(floor.loggedIn());
    REQUIRE(loggedOut.size() == 1);
    CHECK_FALSE(loggedOut[0][0].toBool());
}

TEST_CASE("Kitchen display: tickets, bump, recall", "[net][kitchen]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.addItem(u"classic-burger"_s);
    pos.addItem(u"medium"_s);
    pos.addItem(u"draft-beer"_s);
    CHECK(pos.kitchenTickets().isEmpty());          // nothing sent yet
    REQUIRE(pos.sendOrder());
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());

    QVariantList tickets = pos.kitchenTickets();
    REQUIRE(tickets.size() == 2);                    // one per send
    const QVariantMap first = tickets[0].toMap();
    const QVariantList lines = first[u"lines"_s].toList();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].toMap()[u"name"_s].toString() == u"Classic Burger"_s);
    CHECK(lines[0].toMap()[u"modifiers"_s].toStringList() == QStringList{u"M"_s});
    CHECK(lines[1].toMap()[u"printer"_s].toString() == u"bar"_s);

    REQUIRE(pos.bumpTicket(first[u"checkId"_s].toLongLong(), first[u"sentAt"_s].toLongLong()));
    CHECK(pos.kitchenTickets().size() == 1);
    CHECK_FALSE(pos.bumpTicket(first[u"checkId"_s].toLongLong(), first[u"sentAt"_s].toLongLong()));
    REQUIRE(pos.recallTicket());
    CHECK(pos.kitchenTickets().size() == 2);

    // Paying does not take tickets off the screen; bumping does.
    pos.entryKey(u"0"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.tender(u"cash"_s, 100000));
    REQUIRE(pos.closeCheck());
    tickets = pos.kitchenTickets();
    CHECK(tickets.size() == 2);
    for (const QVariant &t : tickets)
        REQUIRE(pos.bumpTicket(t.toMap()[u"checkId"_s].toLongLong(), t.toMap()[u"sentAt"_s].toLongLong()));
    CHECK(pos.kitchenTickets().isEmpty());
    CHECK(sink.checks.begin()->second.lines[0].made);
}

TEST_CASE("Takeout and delivery carry customer details", "[net][customer]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Delivery));
    CHECK(pos.checkInfo()[u"label"_s].toString().startsWith(u"Delivery"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u" Ana Ruiz "_s}, {u"phone"_s, u"555-0100"_s},
                             {u"address"_s, u"12 Oak St"_s}, {u"note"_s, u"Ring twice"_s}}));
    const QVariantMap customer = pos.checkInfo()[u"customer"_s].toMap();
    CHECK(customer[u"name"_s].toString() == u"Ana Ruiz"_s);   // trimmed
    CHECK(customer[u"address"_s].toString() == u"12 Oak St"_s);
    CHECK(pos.openChecks()[0].toMap()[u"customer"_s].toString() == u"Ana Ruiz"_s);

    pos.releaseCheck();
    CHECK_FALSE(pos.setCustomer({{u"name"_s, u"x"_s}}));
}

TEST_CASE("Remote terminal: full order flow through the server", "[net][remote]")
{
    Store store;
    Screen front(store.terminal(u"Front"_s));
    CHECK(store.server.terminalCount() == 1);
    CHECK(front.remote->terminalName() == u"Front"_s);
    CHECK(front.c.pageId() == u"login"_s);

    front.pin("1111");
    CHECK(front.remote->userName() == u"Sam"_s);
    CHECK(front.c.pageId() == u"tables"_s);

    front.c.selectTable(u"T2"_s);
    front.settle();
    CHECK(front.c.pageId() == u"guest-count"_s);
    front.remote->entryKey(u"3"_s);
    front.settle();
    front.tap(u"start"_s);                             // startCheck, then the menu
    CHECK(front.c.pageId() == u"index-lunch"_s);
    CHECK(front.remote->checkInfo()[u"guests"_s].toInt() == 3);

    REQUIRE(front.c.jumpTo(u"items-burgers"_s));      // a page of buttons placed by hand (a fixture)
    front.tap(u"item-1"_s);                            // Classic Burger -> its choices
    CHECK(front.c.pageId() == u"modifiers"_s);
    front.remote->chooseOption(u"temperature"_s, 1);   // Medium Rare
    front.settle();
    front.c.finishChoosing();
    front.settle();
    CHECK(front.c.pageId() == u"items-burgers"_s);
    REQUIRE(front.remote->lines().size() == 1);
    CHECK(front.remote->lines()[0].toMap()[u"modifiers"_s].toList().size() == 1);

    // The server holds the truth.
    REQUIRE(store.shared.open.size() == 1);
    CHECK(store.shared.open.begin()->second.label == "T2");

    front.tap(u"flow-send"_s);
    CHECK(front.remote->kitchenTickets().size() == 1);

    // A second terminal sees the table busy on Front.
    Screen bar(store.terminal(u"Bar"_s));
    bar.pin("2222");
    CHECK(bar.remote->tableStatus(u"T2"_s)[u"busyOn"_s].toString() == u"Front"_s);
    bar.c.selectTable(u"T2"_s);
    bar.settle();
    CHECK(bar.c.pageId() == u"tables"_s);
    CHECK(bar.c.statusText().contains(u"Front"_s));

    // Pay and close on Front.
    front.remote->entryKey(u"0"_s);
    front.settle();
    front.remote->openDrawerSession();
    front.settle();
    front.tap(u"flow-pay"_s);
    CHECK(front.c.pageId() == u"settle"_s);
    front.tap(u"tender-credit"_s);
    front.tap(u"close"_s);
    REQUIRE(waitFor([&] { return front.c.pageId() == u"tables"_s; }));
    CHECK(store.shared.open.empty());
    CHECK(store.shared.closedToday.size() == 1);
    CHECK(waitFor([&] { return bar.remote->openChecks().isEmpty(); }));
}

TEST_CASE("Remote terminal: reports and admin answers arrive asynchronously", "[net][remote]")
{
    Store store;
    Screen office(store.terminal(u"Office"_s));
    office.pin("1234");

    QSignalSpy answered(office.remote.get(), &app::PosSession::queriesChanged);
    const QVariantMap first = office.remote->report(u"sales"_s);
    CHECK(first.isEmpty());                            // not here yet
    REQUIRE(waitFor([&] { return !office.remote->report(u"sales"_s).isEmpty(); }));
    CHECK(office.remote->report(u"sales"_s)[u"title"_s].toString() == u"Sales Summary"_s);

    REQUIRE(waitFor([&] { return !office.remote->adminRecords(u"menu"_s).isEmpty(); }));
    const int items = int(office.remote->adminRecords(u"menu"_s).size());
    QVariantMap nachos = {{u"name"_s, u"Nachos"_s}, {u"price"_s, 7.5}, {u"taxClass"_s, u"food"_s}};
    office.remote->adminSave(u"menu"_s, -1, nachos);
    REQUIRE(waitFor([&] { return office.remote->adminRecords(u"menu"_s).size() == items + 1; }));
    CHECK(store.shared.menu.back().name == "Nachos");
}

TEST_CASE("Page edits on one terminal reach the others", "[net][remote]")
{
    Store store;
    Screen a(store.terminal(u"A"_s));
    Screen b(store.terminal(u"B"_s));
    a.pin("1234");
    QSignalSpy bGot(b.remote.get(), &net::RemoteSession::layoutReceived);

    a.c.enterEditMode();
    const QString page = a.c.pageId();
    a.c.editor()->addZone(u"button"_s);
    REQUIRE(a.c.saveEdits());
    REQUIRE(waitFor([&] { return bGot.size() == 1; }));
    CHECK(store.hub.layout().page(page)->zone(u"button"_s));
    CHECK(b.c.layout().page(page)->zone(u"button"_s));

    // Saving needs the layout.edit permission on the server too.
    b.pin("1111");
    QSignalSpy saved(b.remote.get(), &net::RemoteSession::layoutSaved);
    b.remote->saveLayout(b.c.layout());
    REQUIRE(waitFor([&] { return saved.size() == 1; }));
    CHECK_FALSE(saved[0][0].toBool());
}

TEST_CASE("Remote terminal reconnects and starts at the login page", "[net][remote]")
{
    auto store = std::make_unique<Store>();
    const quint16 port = store->server.port();
    Screen t(store->terminal(u"Front"_s));
    t.pin("1111");
    CHECK(t.c.pageId() == u"tables"_s);
    const auto pairedDevices = store->shared.settings.terminals;

    // Server goes away: the terminal says so and keeps trying.
    store.reset();
    REQUIRE(waitFor([&] { return !t.remote->online(); }));
    QSignalSpy notices(t.remote.get(), &app::PosSession::notice);
    t.c.selectTable(u"T1"_s);
    t.settle();
    CHECK_FALSE(notices.isEmpty());

    // The server restarts on the same port (its paired devices are saved):
    // back online, logged out.
    test::RecordingSink sink;
    PosShared shared(test::seedPosData(), &sink);
    shared.settings.terminals = pairedDevices;
    net::LayoutHub hub(seedLayout());
    net::PosServer server(&shared, &hub);
    REQUIRE(server.listen(QHostAddress::LocalHost, port));
    REQUIRE(waitFor([&] { return t.remote->online(); }, 8000));
    CHECK_FALSE(t.remote->loggedIn());
    CHECK(waitFor([&] { return t.c.pageId() == u"login"_s; }));
}

TEST_CASE("Kitchen display: a station bumps only its own lines", "[net][kitchen]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.addItem(u"cobb"_s);          // kitchen
    pos.addItem(u"house-wine"_s);    // bar
    REQUIRE(pos.sendOrder());
    const QVariantMap t = pos.kitchenTickets()[0].toMap();

    REQUIRE(pos.bumpTicket(t[u"checkId"_s].toLongLong(), t[u"sentAt"_s].toLongLong(), u"bar"_s));
    QVariantList left = pos.kitchenTickets();
    REQUIRE(left.size() == 1);
    REQUIRE(left[0].toMap()[u"lines"_s].toList().size() == 1);
    CHECK(left[0].toMap()[u"lines"_s].toList()[0].toMap()[u"name"_s].toString() == u"Cobb"_s);

    REQUIRE(pos.recallTicket());     // brings the wine back only
    CHECK(pos.kitchenTickets()[0].toMap()[u"lines"_s].toList().size() == 2);
}

TEST_CASE("An empty takeout that is put away is discarded", "[net][customer]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.releaseCheck();
    CHECK(pos.openChecks().isEmpty());
    CHECK(sink.checks.begin()->second.status == core::CheckStatus::Discarded);

    // A table keeps its (still empty) check.
    pos.selectTable(u"T1"_s);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.releaseCheck();
    CHECK(pos.openChecks().size() == 1);
}

namespace {

// Between a terminal and the store, like Wi-Fi: passes everything on until
// cut, then swallows everything both ways without closing anything (out of
// range: no socket ever hears the other side has gone).
struct WifiRelay {
    QTcpServer server;
    bool cut = false;
    bool cutReplies = false;   // the store hears the handheld, the handheld hears nothing
    std::vector<std::pair<QTcpSocket *, QTcpSocket *>> pairs;
    explicit WifiRelay(quint16 storePort)
    {
        REQUIRE(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this, storePort] {
            while (QTcpSocket *in = server.nextPendingConnection()) {
                auto *out = new QTcpSocket(&server);
                out->connectToHost(QHostAddress::LocalHost, storePort);
                const auto pass = [this](QTcpSocket *from, QTcpSocket *to, bool back) {
                    QObject::connect(from, &QTcpSocket::readyRead, from, [this, from, to, back] {
                        const QByteArray bytes = from->readAll();
                        if (!cut && !(back && cutReplies))
                            to->write(bytes);
                    });
                };
                pass(in, out, false);
                pass(out, in, true);
                pairs.emplace_back(in, out);
            }
        });
    }
};

} // namespace

TEST_CASE("Wi-Fi gone without a word: the handheld says so; the store frees its check", "[net][remote][heartbeat]")
{
    Store store;
    store.server.setHeartbeat(200, 1500);
    store.server.setKeepAway(1500);   // kept for it a while (below: the taps it keeps)
    WifiRelay wifi(store.server.port());
    auto remote = std::make_unique<net::RemoteSession>(u"Handheld"_s);
    net::Credentials creds = store.pairedDevice(u"Handheld"_s);
    creds.port = wifi.server.serverPort();
    remote->setCredentials(creds);
    remote->setWatchdog(1000, 100);
    remote->connectTo(u"127.0.0.1"_s, wifi.server.serverPort());
    REQUIRE(remote->waitForWelcome(5000));
    Screen h(std::move(remote));
    h.pin("1111");
    h.c.selectTable(u"T2"_s);
    h.settle();
    h.remote->entryKey(u"2"_s);
    h.settle();
    h.tap(u"start"_s);
    REQUIRE(waitFor([&] { return h.remote->hasCheck(); }));
    const qint64 id = h.remote->checkInfo()[u"id"_s].toLongLong();
    PosService bar(&store.shared, u"Bar"_s);
    REQUIRE(bar.loginWithPin(u"2222"_s));
    CHECK_FALSE(bar.openCheck(id));                     // open on the handheld
    waitFor([] { return false; }, 600);                 // pings answered: it's a terminal that pongs

    wifi.cut = true;                                    // out of range
    CHECK(waitFor([&] { return !h.remote->online(); }, 4000));    // the banner, not a hang
    QSignalSpy notices(h.remote.get(), &app::PosSession::notice);
    h.remote->invoke(u"releaseCheck"_s);                // a tap now: refused at once
    CHECK_FALSE(notices.isEmpty());
    CHECK(waitFor([&] { return store.server.awayCount() == 1; }, 4000));   // kept for it a while...
    CHECK_FALSE(bar.openCheck(id));
    CHECK(waitFor([&] { return bar.openCheck(id); }, 6000));     // ...then the store let go of it
    wifi.cut = false;                                   // (it keeps trying: a new connection, not this one)
}

namespace {

// A handheld at a table, through Wi-Fi that can drop, with a check open.
struct TableSide {
    Store store;
    WifiRelay wifi{store.server.port()};
    std::unique_ptr<Screen> h;
    qint64 checkId = 0;

    TableSide()
    {
        store.server.setHeartbeat(200, 1500);
        auto remote = std::make_unique<net::RemoteSession>(u"Handheld"_s);
        net::Credentials creds = store.pairedDevice(u"Handheld"_s);
        creds.port = wifi.server.serverPort();
        remote->setCredentials(creds);
        remote->setWatchdog(1000, 100);
        remote->connectTo(u"127.0.0.1"_s, wifi.server.serverPort());
        REQUIRE(remote->waitForWelcome(5000));
        h = std::make_unique<Screen>(std::move(remote));
        h->pin("1111");
        h->c.selectTable(u"T2"_s);
        h->settle();
        h->remote->entryKey(u"2"_s);
        h->settle();
        h->tap(u"start"_s);
        REQUIRE(waitFor([&] { return h->remote->hasCheck(); }));
        checkId = h->remote->checkInfo()[u"id"_s].toLongLong();
        waitFor([] { return false; }, 600);   // pings answered: it pongs
    }
    std::size_t storeLines() const
    {
        const auto it = store.shared.open.find(checkId);
        return it == store.shared.open.end() ? 0 : it->second.lines.size();
    }
    int waiting() const
    {
        int n = 0;
        for (const QVariant &l : h->remote->lines())
            n += l.toMap().value(u"waiting"_s).toBool();
        return n;
    }
};

} // namespace

TEST_CASE("Wi-Fi out at the table: the taps are kept, shown waiting, and go in once it's back", "[net][remote][heartbeat][held]")
{
    TableSide t;
    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));

    // The server keeps tapping: nothing refused, nothing stuck busy; a buzz
    // and "+ Water (waiting)" for each.
    QSignalSpy flashes(&t.h->c, &LayoutController::addedFlash);
    const int buzzed = t.h->c.buzzes();
    t.h->c.orderItem(u"water"_s);
    t.h->c.orderItem(u"caesar"_s);
    CHECK_FALSE(t.h->c.busy());
    CHECK(t.h->c.buzzes() == buzzed + 2);
    REQUIRE(flashes.size() == 2);
    CHECK(flashes[0][0].toString() == u"+ Water"_s);
    CHECK(flashes[0][1].toBool());   // waiting
    CHECK(t.h->remote->heldCount() == 2);
    CHECK(t.waiting() == 2);
    CHECK(t.h->remote->lines().last().toMap()[u"name"_s].toString() == u"Caesar"_s);
    // Sending to the kitchen waits for the server: refused, not kept.
    QSignalSpy notices(t.h->remote.get(), &app::PosSession::notice);
    t.h->tap(u"flow-send"_s);
    CHECK(t.h->remote->heldCount() == 2);
    CHECK_FALSE(notices.isEmpty());

    // The store keeps the check for it meanwhile.
    REQUIRE(waitFor([&] { return t.store.server.awayCount() == 1; }, 4000));
    PosService bar(&t.store.shared, u"Bar"_s);
    REQUIRE(bar.loginWithPin(u"2222"_s));
    CHECK_FALSE(bar.openCheck(t.checkId));

    t.wifi.cut = false;   // back in range: the same session, logged in, the check open
    REQUIRE(waitFor([&] { return t.h->remote->online(); }, 8000));
    CHECK(t.h->remote->loggedIn());
    REQUIRE(waitFor([&] { return t.storeLines() == 2; }));
    CHECK(waitFor([&] { return t.waiting() == 0 && t.h->remote->lines().size() == 2; }));
    CHECK(t.h->remote->heldCount() == 0);
    CHECK(t.store.server.awayCount() == 0);
}

TEST_CASE("Wi-Fi out before the answer came: sent again, done once", "[net][remote][heartbeat][held]")
{
    TableSide t;
    t.wifi.cutReplies = true;   // the store gets the tap; its answer is lost
    t.h->c.orderItem(u"water"_s);
    REQUIRE(waitFor([&] { return t.storeLines() == 1; }));
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));
    CHECK_FALSE(t.h->c.busy());
    CHECK(t.waiting() == 1);
    t.wifi.cutReplies = false;
    REQUIRE(waitFor([&] { return t.h->remote->online(); }, 8000));
    CHECK(waitFor([&] { return t.waiting() == 0 && t.h->remote->lines().size() == 1; }));
    waitFor([] { return false; }, 300);
    CHECK(t.storeLines() == 1);   // not twice
}

TEST_CASE("Wi-Fi out too long: the taps made meanwhile are said not to have gone in", "[net][remote][heartbeat][held]")
{
    TableSide t;
    t.store.server.setKeepAway(300);
    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));
    t.h->c.orderItem(u"water"_s);
    CHECK(t.h->remote->heldCount() == 1);
    REQUIRE(waitFor([&] { return t.store.server.awayCount() == 1; }, 4000));
    REQUIRE(waitFor([&] { return t.store.server.awayCount() == 0; }, 4000));   // let go
    QSignalSpy notices(t.h->remote.get(), &app::PosSession::notice);
    t.wifi.cut = false;
    REQUIRE(waitFor([&] { return t.h->remote->online(); }, 8000));
    CHECK_FALSE(t.h->remote->loggedIn());
    CHECK(t.waiting() == 0);
    CHECK(t.h->remote->heldCount() == 0);
    bool told = false;
    for (const auto &n : notices)
        told = told || n.first().toString().contains(u"didn't go in"_s);
    CHECK(told);
    CHECK(t.storeLines() == 0);
}

TEST_CASE("Updates: a screen behind the store is told, and a manager there gets the update", "[net][remote][updates]")
{
    Store store;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    store.server.setUpdatesDir(dir.path());
    const net::AppBuild me = net::appBuild();
    // An update for screens like this one, newer than this build; one for another kind; one unnamed.
    const QString arch = me.platform.section(u'-', 1);
    const QString mine = u"viewtouch-modern_0.7.0_%1-b%2.deb"_s.arg(arch == u"x86_64"_s ? u"amd64"_s : u"arm64"_s).arg(me.number + 5);
    QByteArray payload(3 * 1024 * 1024 + 123, '\0');   // more than one part
    for (int i = 0; i < payload.size(); ++i)
        payload[i] = char(i * 7);
    for (const auto &[name, bytes] : std::initializer_list<std::pair<QString, QByteArray>>{
             {mine, payload}, {u"ViewTouch-b%1-armeabi-v7a.apk"_s.arg(me.number + 9), "apk"}, {u"notes.txt"_s, "x"}}) {
        QFile f(dir.filePath(name));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(bytes);
    }
    REQUIRE(me.platform.startsWith(u"linux-"_s));
    CHECK(store.server.updateFiles().size() == 2);
    CHECK(store.server.updateFor(me.platform, me.number).build == me.number + 5);
    CHECK(store.server.updateFor(me.platform, me.number + 5).path.isEmpty());   // nothing newer than that

    Screen h(store.terminal(u"Handheld"_s));
    // Same build as the store: not behind (the store's computer runs this build too).
    QVariantMap info = h.remote->updateInfo();
    CHECK(info[u"build"_s] == me.number);
    CHECK(info[u"storeBuild"_s] == me.number);
    CHECK_FALSE(info[u"behind"_s].toBool());
    CHECK(info[u"update"_s].toMap()[u"name"_s] == mine);   // newer than this screen: offered
    // The store's list of screens: the build, and that an update is ready.
    const QVariantList screens = store.server.connections();
    REQUIRE(screens.size() == 1);
    CHECK(screens[0].toMap()[u"build"_s] == me.number);
    CHECK(screens[0].toMap()[u"platform"_s] == me.platform);
    CHECK(screens[0].toMap()[u"hasUpdate"_s].toBool());

    // A server can't fetch it; the maintainer can.
    h.pin("1111");
    QSignalSpy notices(h.remote.get(), &app::PosSession::notice);
    h.remote->getUpdate();
    REQUIRE(waitFor([&] { return h.remote->updateInfo().contains(u"error"_s); }));
    CHECK(h.remote->updateInfo()[u"error"_s].toString().contains(u"maintainer"_s));
    h.remote->invoke(u"logout"_s);
    REQUIRE(waitFor([&] { return !h.remote->loggedIn(); }));
    h.settle();
    store.shared.setMaintainerPin(u"86420"_s);
    h.pin("86420");
    h.remote->getUpdate();
    REQUIRE(waitFor([&] { return h.remote->updateInfo().contains(u"ready"_s); }, 10000));
    QFile got(h.remote->updateInfo()[u"ready"_s].toString());
    REQUIRE(got.open(QIODevice::ReadOnly));
    CHECK(QFileInfo(got).fileName() == mine);
    CHECK(got.readAll() == payload);
    got.remove();
}

TEST_CASE("Updates: phones and tablets get the store's newer app by themselves while nobody is on them", "[net][remote][updates]")
{
    Store store;
    store.server.setHeartbeat(100, 15'000);   // looks every ping
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    store.server.setUpdatesDir(dir.path());
    const net::AppBuild me = net::appBuild();
    const QString apk = u"ViewTouch-b%1-arm64-v8a.apk"_s.arg(me.number + 3);
    QByteArray payload(2 * 1024 * 1024 + 77, '\0');
    for (int i = 0; i < payload.size(); ++i)
        payload[i] = char(i * 13);
    {
        QFile f(dir.filePath(apk));
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write(payload);
    }
    WifiRelay wifi(store.server.port());
    const auto phone = [&](const QString &name, quint16 port = 0) {
        auto t = std::make_unique<net::RemoteSession>(name);
        t->setPlatform(u"android-arm64"_s);
        net::Credentials creds = store.pairedDevice(name);
        creds.port = port ? port : store.server.port();
        t->setCredentials(creds);
        t->connectTo(u"127.0.0.1"_s, creds.port);
        REQUIRE(t->waitForWelcome(5000));
        return t;
    };

    // Someone logged in: not while they work. (Logged in with the setting
    // off: connected and not logged in yet, it's nobody's for a moment.)
    store.shared.settings.screensUpdate = false;
    auto busy = phone(u"Phone 1"_s);
    for (const char *d = "1111"; *d; ++d)
        busy->pinKey(QString(QChar(*d)));
    busy->invoke(u"login"_s);
    REQUIRE(waitFor([&] { return busy->loggedIn(); }));
    store.shared.settings.screensUpdate = true;
    // Nobody on it: it comes by itself, and waits for someone to touch Install.
    auto idle = phone(u"Phone 2"_s, wifi.server.serverPort());
    CHECK(idle->updateInfo()[u"byItself"_s].toBool());   // it says so (no "the maintainer can")
    REQUIRE(waitFor([&] { return idle->updateInfo().contains(u"ready"_s); }, 10000));
    QFile got(idle->updateInfo()[u"ready"_s].toString());
    REQUIRE(got.open(QIODevice::ReadOnly));
    CHECK(got.readAll() == payload);
    waitFor([] { return false; }, 400);
    CHECK_FALSE(busy->updateInfo().contains(u"ready"_s));
    CHECK_FALSE(busy->updateInfo().contains(u"downloading"_s));
    // Logged out: its turn.
    busy->invoke(u"logout"_s);
    REQUIRE(waitFor([&] { return busy->updateInfo().contains(u"ready"_s); }, 10000));

    // Back after a dropped connection with it already here: not sent again.
    for (auto &[in, out] : wifi.pairs)
        in->abort();
    REQUIRE(waitFor([&] { return !idle->isConnected(); }));
    REQUIRE(waitFor([&] { return idle->isConnected(); }, 8000));
    waitFor([] { return false; }, 500);
    CHECK_FALSE(idle->updateInfo().contains(u"downloading"_s));

    // Turned off in Store Settings: the maintainer updates each one.
    store.shared.settings.screensUpdate = false;
    auto off = phone(u"Phone 3"_s);
    waitFor([] { return false; }, 500);
    CHECK_FALSE(off->updateInfo().contains(u"ready"_s));
    CHECK_FALSE(off->updateInfo()[u"byItself"_s].toBool());
    got.remove();
}

TEST_CASE("Versions: MAJOR.MINOR and the build number, by every build; the recent builds built in", "[net][version]")
{
    const net::AppBuild me = net::appBuild();
    // 0.9.1283: the build number is the version's last part.
    CHECK(me.version.endsWith(u'.' + QString::number(me.number)));
    CHECK(me.version.count(u'.') == 2);
    CHECK(net::buildText(u"0.9.1283"_s, 1283, u"688654a"_s) == u"0.9.1283 (688654a)"_s);
    CHECK(net::buildText(u"0.9.1283"_s, 1283, u"688654a"_s, true) == u"0.9.1283 (688654a, modified)"_s);
    CHECK(net::buildText(u"0.7.0"_s, 812, u"a13bde2"_s) == u"0.7.0, build 812 (a13bde2)"_s);   // an older screen

    // The recent builds, newest first, each with its version and what changed.
    const QVariantList history = net::appHistory();
    REQUIRE(history.size() >= 2);
    int last = std::numeric_limits<int>::max();
    for (const QVariant &v : history) {
        const QVariantMap b = v.toMap();
        CHECK(b[u"number"_s].toInt() < last);
        last = b[u"number"_s].toInt();
        CHECK(b[u"version"_s].toString().endsWith(u'.' + b[u"number"_s].toString()));
        CHECK_FALSE(b[u"change"_s].toString().isEmpty());
        CHECK(b[u"at"_s].toLongLong() > 0);
    }
    CHECK(history.first().toMap()[u"number"_s].toInt() <= me.number);
}

TEST_CASE("Versions: the builds a store has run are kept with its settings", "[net][version]")
{
    core::PosSettings s;
    s.builds = {{"0.9.1283", 1283, "688654a", 1'760'000'000'000}, {"0.9.1284", 1284, "abc1234", 1'760'100'000'000}};
    const core::PosSettings back = app::settingsFromJson(app::toJson(s));
    CHECK(back.builds == s.builds);
}

TEST_CASE("Handhelds get the closed checks only while a list shows them", "[net][remote][want]")
{
    Store store;
    Screen office(store.terminal(u"Office"_s));
    office.pin("1234");
    // A check closed on the main computer's own session.
    PosService counter(&store.shared, u"Counter"_s);
    REQUIRE(counter.loginWithPin(u"1234"_s));
    counter.entryKey(u"10000"_s);
    REQUIRE(counter.openDrawerSession());
    const auto sell = [&] {
        REQUIRE(counter.startCheck(core::CheckType::Quick));
        counter.addItem(u"coffee"_s);
        REQUIRE(counter.tender(u"cash"_s));
        if (counter.hasCheck())
            REQUIRE(counter.closeCheck());
    };
    sell();
    waitFor([] { return false; }, 300);
    CHECK(office.remote->closedChecks().isEmpty());          // not asked for

    office.remote->want(u"closedChecks"_s, true);            // the Closed Checks list is shown
    REQUIRE(waitFor([&] { return office.remote->closedChecks().size() == 1; }));
    sell();                                                   // and kept up to date
    REQUIRE(waitFor([&] { return office.remote->closedChecks().size() == 2; }));

    office.remote->want(u"closedChecks"_s, false);           // gone from the screen
    CHECK(office.remote->closedChecks().isEmpty());
    sell();
    waitFor([] { return false; }, 300);
    CHECK(office.remote->closedChecks().isEmpty());
}

TEST_CASE("Handhelds put the open checks together from the shared part and their own", "[net][remote][want]")
{
    Store store;
    Screen front(store.terminal(u"Front"_s));
    Screen bar(store.terminal(u"Bar"_s));
    front.pin("1111");
    bar.pin("2222");
    front.c.selectTable(u"T2"_s);
    front.settle();
    front.remote->entryKey(u"3"_s);
    front.settle();
    front.tap(u"start"_s);                                   // Sam's check, open on Front
    REQUIRE(waitFor([&] { return bar.remote->openChecks().size() == 1; }));
    const QVariantMap mineOnFront = front.remote->openChecks().first().toMap();
    const QVariantMap seenAtBar = bar.remote->openChecks().first().toMap();
    CHECK(mineOnFront[u"mine"_s].toBool());
    CHECK(mineOnFront[u"current"_s].toBool());
    CHECK(mineOnFront[u"forMe"_s].toBool());
    CHECK(mineOnFront[u"label"_s] == u"T2"_s);
    CHECK_FALSE(seenAtBar[u"mine"_s].toBool());
    CHECK_FALSE(seenAtBar[u"current"_s].toBool());
    CHECK(seenAtBar[u"busyOn"_s] == u"Front"_s);
    // The same as the store works out for that person on that screen.
    PosService casey(&store.shared, u"Bar"_s);
    REQUIRE(casey.loginWithPin(u"2222"_s));
    const QVariantMap store_ = casey.openChecks().first().toMap();
    for (const QString &k : store_.keys())
        if (k != u"minutes"_s)
            CHECK(seenAtBar.value(k) == store_.value(k));
}

TEST_CASE("The screens' updates as JSON text: the same as Qt's, written straight", "[net][json]")
{
    const auto same = [](const QVariant &v) {
        const QByteArray mine = net::jsonText(v);
        const QJsonValue want = QJsonValue::fromVariant(v);
        // Read back, it's the very value Qt would have sent.
        const QJsonDocument doc = QJsonDocument::fromJson("[" + mine + "]");
        INFO(mine.toStdString());
        REQUIRE(doc.isArray());
        CHECK(doc.array().at(0) == want);
    };
    same(QVariant());
    same(true);
    same(false);
    same(0);
    same(-42);
    same(qint64(9'007'199'254'740'993));   // past a double's whole numbers
    same(3.5);
    same(2.0);
    same(0.1 + 0.2);
    same(-1e-7);
    same(std::numeric_limits<double>::quiet_NaN());
    same(u""_s);
    same(u"Café \"Ana\" \\ tab\there\nnew line\r\x01\x1f / 日本 🍔"_s);
    same(QStringList{u"a"_s, u"b\"c"_s});
    same(QVariantList{});
    same(QVariantMap{});
    same(QVariantList{1, u"x"_s, QVariantMap{{u"z"_s, 1}, {u"a"_s, QVariantList{true, QVariant()}}}, 2.25});
    same(QVariantMap{{u"total"_s, u"$12.50"_s}, {u"guests"_s, 4}, {u"lines"_s, QVariantList{QVariantMap{{u"name"_s, u"Burger"_s}}}}});
    // Anything unusual goes Qt's way, the same.
    same(QVariantMap{{u"at"_s, QDateTime::fromMSecsSinceEpoch(1'700'000'000'000, QTimeZone::UTC)}});
    same(QVariantHash{{u"b"_s, 1}, {u"a"_s, 2}});
}

TEST_CASE("Kitchen and expo screens skip finished closed checks, and see recalled ones again", "[net][kitchen]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    PosShared &shared = *pos.shared();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.addItem(u"classic-burger"_s);
    pos.addItem(u"medium"_s);
    REQUIRE(pos.sendOrder());
    const QVariantMap ticket = pos.kitchenTicketsNow().constFirst().toMap();
    const qint64 id = ticket[u"checkId"_s].toLongLong(), sentAt = ticket[u"sentAt"_s].toLongLong();
    // Paid before it's made (a counter): still on both screens.
    pos.entryKey(u"0"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.tender(u"cash"_s, 100000));
    REQUIRE(pos.closeCheck());
    CHECK(pos.kitchenTicketsNow().size() == 1);
    CHECK(pos.expoTicketsNow().size() == 1);
    CHECK_FALSE(shared.kitchenDone.contains(id));
    // Made: off the kitchen's screen, and remembered as done there.
    REQUIRE(pos.bumpTicket(id, sentAt));
    CHECK(pos.kitchenTicketsNow().isEmpty());
    CHECK(shared.kitchenDone.contains(id));
    CHECK(pos.expoTicketsNow().size() == 1);   // not run to the table yet
    // Recalled: back on the kitchen's screen.
    REQUIRE(pos.recallTicket());
    CHECK_FALSE(shared.kitchenDone.contains(id));
    CHECK(pos.kitchenTicketsNow().size() == 1);
    // A change to the menu or settings looks at every closed check afresh.
    REQUIRE(pos.bumpTicket(id, sentAt));
    CHECK(pos.kitchenTicketsNow().isEmpty());
    emit shared.adminChanged();
    CHECK(shared.kitchenDone.empty());
    CHECK(pos.kitchenTicketsNow().isEmpty());
}

TEST_CASE("Taxes set on a handheld: saved, and every screen totals with them", "[net][remote][taxes]")
{
    Store store;
    Screen office(store.terminal(u"Office"_s));
    office.pin("1234");
    Screen front(store.terminal(u"Front"_s));
    front.pin("1111");

    // An open check on Front with a burger.
    front.c.selectTable(u"T2"_s);
    front.settle();
    front.remote->entryKey(u"2"_s);
    front.settle();
    front.tap(u"start"_s);
    REQUIRE(front.c.jumpTo(u"items-burgers"_s));
    front.tap(u"item-1"_s);
    front.remote->chooseOption(u"temperature"_s, 1);
    front.settle();
    front.c.finishChoosing();
    front.settle();
    REQUIRE(front.remote->lines().size() == 1);
    const QString taxBefore = front.remote->totals()[u"tax"_s].toString();

    REQUIRE(waitFor([&] { return !office.remote->adminRecords(u"taxes"_s).isEmpty(); }));
    QVariantMap taxes = office.remote->adminRecords(u"taxes"_s).value(0).toMap();
    INFO(QJsonDocument::fromVariant(taxes).toJson().toStdString());
    taxes[u"food"_s] = 10.0;
    office.remote->adminSave(u"taxes"_s, 0, taxes);

    // The server keeps it (and saves it).
    REQUIRE(waitFor([&] { return store.shared.settings.tax.foodPpm == 100'000; }));
    // The screen that set it shows it.
    CHECK(waitFor([&] { return office.remote->adminRecords(u"taxes"_s).value(0).toMap()[u"food"_s].toDouble() == 10.0; }));
    // Another screen's open check is totaled with it.
    INFO("tax before: " << taxBefore.toStdString());
    CHECK(waitFor([&] { return front.remote->totals()[u"tax"_s].toString() != taxBefore; }));
}

TEST_CASE("Store Settings -> Taxes on a handheld: the form fills in, saves, and shows what was saved", "[net][remote][taxes][ui]")
{
    Store store;
    store.shared.settings.tax.alcoholPpm = 90'000;   // 9%: must survive a save of the food rate
    Screen office(store.terminal(u"Office"_s));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&office.c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    office.pin("1234");
    REQUIRE(office.c.showPage(u"admin-taxes"_s));

    const auto panel = [&]() -> QQuickItem * {
        const std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *root) -> QQuickItem * {
            for (QQuickItem *i : root->childItems()) {
                if (QByteArray(i->metaObject()->className()).startsWith("WidgetAdminPanel"))
                    return i;
                if (QQuickItem *hit = find(i))
                    return hit;
            }
            return nullptr;
        };
        return find(window->contentItem());
    };
    const auto draft = [&] { return panel() ? panel()->property("draft").toMap() : QVariantMap(); };
    REQUIRE(waitFor([&] { return panel() != nullptr; }));
    // The form shows the store's rates once they come from the server.
    REQUIRE(waitFor([&] { return draft().value(u"alcohol"_s).toDouble() == 9.0; }));

    // The manager types a food rate and saves.
    QVariantMap d = draft();
    d[u"food"_s] = 7.5;
    panel()->setProperty("draft", d);
    panel()->setProperty("dirty", true);
    QMetaObject::invokeMethod(panel(), "save");
    REQUIRE(waitFor([&] { return store.shared.settings.tax.foodPpm == 75'000; }));
    CHECK(store.shared.settings.tax.alcoholPpm == 90'000);
    // The form shows what was saved, not the rates from before.
    CHECK(waitFor([&] { return draft().value(u"food"_s).toDouble() == 7.5 && !panel()->property("dirty").toBool(); }));
}

TEST_CASE("Taxes saved with only one rate keep the others", "[net][remote][taxes]")
{
    Store store;
    store.shared.settings.tax.alcoholPpm = 90'000;
    store.shared.settings.tax.merchandisePpm = 60'000;
    Screen office(store.terminal(u"Office"_s));
    office.pin("1234");
    office.remote->adminSave(u"taxes"_s, 0, {{u"food"_s, 8.0}});
    REQUIRE(waitFor([&] { return store.shared.settings.tax.foodPpm == 80'000; }));
    CHECK(store.shared.settings.tax.alcoholPpm == 90'000);
    CHECK(store.shared.settings.tax.merchandisePpm == 60'000);
}

TEST_CASE("Handheld photos: no menu rebuild as each arrives, and kept for the next start", "[net][remote][images]")
{
    Store store;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // Three menu photos on the server.
    QStringList photos;
    for (int i = 0; i < 3; ++i) {
        QImage img(20, 20, QImage::Format_RGB32);
        img.fill(QColor::fromHsv(i * 100, 200, 200));
        photos << dir.filePath(u"photo%1.png"_s.arg(i));
        REQUIRE(img.save(photos.back()));
    }
    const QString cache = dir.filePath(u"cache"_s);
    // The store's: on its menu (only those are handed out).
    for (int i = 0; i < 3; ++i)
        store.shared.menu[std::size_t(i)].image = photos[i].toStdString();

    {
        auto t = store.terminal(u"Handheld"_s);
        t->setImageCache(cache);
        QSignalSpy admin(t.get(), &app::PosSession::adminChanged);
        QSignalSpy images(t.get(), &app::PosSession::imagesChanged);
        for (const QString &p : photos)
            CHECK(t->imageUrl(p).isEmpty());   // not here yet: fetched
        REQUIRE(waitFor([&] { return std::ranges::all_of(photos, [&](const QString &p) { return !t->imageUrl(p).isEmpty(); }); }));
        QTest::qWait(100);
        CHECK(admin.count() == 0);             // the menu isn't rebuilt for pictures
        CHECK(images.count() >= 1);
        CHECK(images.count() < 3);             // together, not one by one
    }

    // The next start: shown at once from this device's copies.
    auto again = store.terminal(u"Handheld2"_s);
    again->setImageCache(cache);
    QSignalSpy images(again.get(), &app::PosSession::imagesChanged);
    for (const QString &p : photos)
        CHECK_FALSE(again->imageUrl(p).isEmpty());
    QTest::qWait(300);                         // asked again (by path: it may have changed)…
    CHECK(images.count() == 0);                // …the same: nothing redrawn
}

TEST_CASE("A handheld has the menu as soon as someone logs in", "[net][remote][menu]")
{
    Store store;
    Screen front(store.terminal(u"Front"_s));
    CHECK(front.remote->menuItems().isEmpty());   // nobody logged in: not sent
    QElapsedTimer t;
    t.start();
    front.pin("1111");
    // At once, not at the server's next round of everything (30 s).
    REQUIRE(waitFor([&] { return !front.remote->menuItems().isEmpty(); }, 2000));
    CHECK(t.elapsed() < 2000);
    CHECK(front.remote->menuItems().size() == qsizetype(store.shared.menu.size()));

    // Taking an order doesn't make the handheld redo the menu.
    QSignalSpy admin(front.remote.get(), &app::PosSession::adminChanged);
    front.c.selectTable(u"T2"_s);
    front.settle();
    front.remote->entryKey(u"2"_s);
    front.settle();
    front.tap(u"start"_s);
    REQUIRE(front.c.jumpTo(u"items-burgers"_s));
    for (int i = 0; i < 3; ++i) {
        front.tap(u"item-1"_s);
        front.remote->chooseOption(u"temperature"_s, 1);
        front.settle();
        front.c.finishChoosing();
        front.settle();
    }
    front.tap(u"flow-send"_s);
    QTest::qWait(200);
    CHECK(admin.count() == 0);
}

// --- working offline --------------------------------------------------------------------

#include "net/offline_store.hh"

namespace {

// A handheld paired to a store through Wi-Fi that can drop, set up to work offline.
struct OfflineSide {
    Store store;
    WifiRelay wifi{store.server.port()};
    QTemporaryDir dir;
    std::unique_ptr<Screen> h;

    OfflineSide()
    {
        store.server.setHeartbeat(200, 1500);
    }
    void connect()
    {
        auto remote = std::make_unique<net::RemoteSession>(u"Handheld"_s);
        net::Credentials creds = store.pairedDevice(u"Handheld"_s);
        creds.port = wifi.server.serverPort();
        remote->setCredentials(creds);
        remote->setOfflineJournal(dir.filePath(u"offline.db"_s));
        remote->setWatchdog(1000, 100);
        remote->connectTo(u"127.0.0.1"_s, wifi.server.serverPort());
        REQUIRE(remote->waitForWelcome(5000));
        h = std::make_unique<Screen>(std::move(remote));
        REQUIRE(waitFor([&] { return h->remote->offlineReady(); }));
        waitFor([] { return false; }, 600);   // pings answered: it pongs
    }
    const core::Check *takenIn(bool closed) const
    {
        if (closed) {
            for (const core::Check &c : store.shared.closedToday)
                if (!c.offlineId.empty())
                    return &c;
        } else {
            for (const auto &[id, c] : store.shared.open)
                if (!c.offlineId.empty())
                    return &c;
        }
        return nullptr;
    }
};

} // namespace

TEST_CASE("A handheld asleep (screen off) longer than its silence limit stays connected when it wakes", "[net][remote]")
{
    OfflineSide t;   // pings every 200 ms; the handheld gives up after 1 s of silence
    t.store.server.setHeartbeat(200, 60'000);   // the store's computer doesn't sleep (here it shares the thread)
    t.connect();
    REQUIRE(t.h->remote->online());
    QSignalSpy online(t.h->remote.get(), &net::RemoteSession::onlineChanged);
    // Android pausing the app: nothing runs here for 2.5 s (the server neither).
    QThread::msleep(2500);
    waitFor([] { return false; }, 1200);   // awake: the watchdog looks, the pings come again
    CHECK(t.h->remote->online());
    CHECK(online.isEmpty());   // never dropped

    // Really cut off: still noticed, after the limit.
    t.wifi.cut = true;
    CHECK(waitFor([&] { return !t.h->remote->online(); }, 4000));
}

TEST_CASE("Working offline: a handheld keeps selling, and the store takes its checks in", "[net][remote][offline]")
{
    OfflineSide t;
    t.store.shared.settings.cashMode = core::CashMode::ServerBank;   // the servers keep their cash
    // A table going at the store: the store's, not the handheld's to open offline.
    PosService bar(&t.store.shared, u"Bar"_s);
    REQUIRE(bar.loginWithPin(u"2222"_s));
    REQUIRE(bar.selectTable(u"T5"_s) == PosService::TableNeedsGuests);
    bar.entryKey(u"2"_s);
    REQUIRE(bar.startCheck(core::CheckType::DineIn));
    bar.addItem(u"water"_s);
    bar.releaseCheck();
    t.connect();
    const std::int64_t before = t.store.shared.lastCheckId;

    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));
    CHECK_FALSE(t.h->remote->offlineMode());
    t.h->remote->workOffline();
    REQUIRE(t.h->remote->offlineMode());
    CHECK_FALSE(t.h->remote->loggedIn());
    t.h->pin("1111");   // Sam: the PIN checked on the handheld
    CHECK(t.h->remote->userName() == u"Sam"_s);

    // The store's table: can't be opened here.
    QSignalSpy notices(t.h->remote.get(), &app::PosSession::notice);
    t.h->c.selectTable(u"T5"_s);
    t.h->settle();
    CHECK_FALSE(t.h->remote->hasCheck());
    // A new one: ordered, sent, paid in cash, closed.
    t.h->c.selectTable(u"T2"_s);
    t.h->settle();
    t.h->remote->entryKey(u"2"_s);
    t.h->settle();
    t.h->tap(u"start"_s);
    REQUIRE(t.h->remote->hasCheck());
    const qint64 offlineNumber = t.h->remote->checkInfo()[u"id"_s].toLongLong();
    CHECK(offlineNumber > before);
    t.h->c.orderItem(u"water"_s);
    t.h->c.orderItem(u"cobb"_s);
    t.h->settle();
    t.h->remote->invoke(u"finishChoosing"_s);
    t.h->remote->invoke(u"sendOrder"_s);
    CHECK(t.h->remote->offlinePending() == 1);   // kept on the handheld
    // Only cash or a discount.
    bool paid = false;
    t.h->remote->invoke(u"tender"_s, {u"cash"_s}, [&](const QVariant &r) { paid = r.toBool(); });
    CHECK(paid);
    t.h->remote->invoke(u"closeCheck"_s);
    t.h->settle();
    INFO([&] {
        QStringList said;
        for (const auto &n : notices)
            said << n.first().toString();
        return said.join(u" | "_s).toStdString();
    }());
    CHECK_FALSE(t.h->remote->hasCheck());
    // What needs the store's computer: said, not done.
    notices.clear();
    t.h->remote->invoke(u"clockIn"_s);
    t.h->remote->invoke(u"lookupGiftCard"_s, {u"6000 2026 1001"_s});
    REQUIRE(notices.size() == 2);
    CHECK(notices[0].first().toString().contains(u"offline"_s));
    // A takeout left open when the store comes back.
    t.h->remote->invoke(u"startCheck"_s, {u"takeout"_s});
    t.h->c.orderItem(u"water"_s);
    t.h->settle();
    CHECK(t.h->remote->offlinePending() == 2);
    CHECK(t.store.shared.closedToday.empty());   // nothing reached the store meanwhile

    // Back: the handheld finishes the check it has open first.
    t.wifi.cut = false;
    REQUIRE(waitFor([&] { return t.h->remote->online(); }, 8000));
    waitFor([] { return false; }, 300);
    CHECK(t.h->remote->offlineMode());
    t.h->remote->invoke(u"releaseCheck"_s);
    REQUIRE(waitFor([&] { return !t.h->remote->offlineMode(); }));
    REQUIRE(waitFor([&] { return t.h->remote->offlinePending() == 0; }));
    CHECK_FALSE(t.h->remote->loggedIn());   // the store's own session: log in again

    // In the store's sales: its number if it's the next, Sam's, its cash in his bank.
    const core::Check *closed = t.takenIn(true);
    REQUIRE(closed);
    CHECK(closed->status == core::CheckStatus::Closed);
    CHECK(closed->serverName == "Sam");
    CHECK(closed->lines.size() == 2);
    CHECK(std::ranges::all_of(closed->lines, [](const core::OrderLine &l) { return l.sent && l.made; }));
    REQUIRE(closed->payments.size() == 1);
    CHECK(closed->payments[0].kind == core::TenderKind::Cash);
    CHECK(closed->businessDay == t.store.shared.day.id);
    const core::DrawerSession *bank = t.store.shared.openBankFor(closed->serverId);
    REQUIRE(bank);
    CHECK(closed->drawerSession == bank->id);
    CHECK(std::ranges::any_of(closed->events, [](const core::CheckEvent &e) { return e.kind == "offline"; }));
    CHECK(closed->id > before);
    // The open one: open at the store now, to go on with.
    const core::Check *open = t.takenIn(false);
    REQUIRE(open);
    CHECK(open->type == core::CheckType::Takeout);
    CHECK(open->id == closed->id + 1);
    // The store's own table is as it was.
    CHECK(std::ranges::count_if(t.store.shared.open, [](const auto &kv) { return kv.second.label == "T5"; }) == 1);
}

TEST_CASE("Working offline: every screen's kit, made once for all of them, is still its own", "[net][remote][offline]")
{
    OfflineSide t;
    t.connect();
    const auto same = [&](net::RemoteSession &screen, const QString &name) {
        QJsonObject got = screen.offlineStore()->kit(), want = PosService(&t.store.shared, name).offlineKit();
        got.remove(u"made"_s);
        want.remove(u"made"_s);
        return got == want;
    };
    CHECK(same(*t.h->remote, u"Handheld"_s));
    auto other = std::make_unique<net::RemoteSession>(u"Handheld 2"_s);
    other->setCredentials(t.store.pairedDevice(u"Handheld 2"_s));
    other->setOfflineJournal(t.dir.filePath(u"offline2.db"_s));
    other->connectTo(u"127.0.0.1"_s, t.store.server.port());
    REQUIRE(other->waitForWelcome(5000));
    REQUIRE(waitFor([&] { return other->offlineReady(); }));
    CHECK(same(*other, u"Handheld 2"_s));   // (the first one's is a terminal behind: a new kit comes with the next change)
    CHECK(t.h->remote->offlineStore()->kit()[u"firstCheckId"_s] != other->offlineStore()->kit()[u"firstCheckId"_s]);

    // A change: a new kit for both a few seconds later, from one shared part.
    t.store.shared.settings.storeName = "Renamed";
    emit t.store.shared.adminChanged();
    REQUIRE(waitFor([&] { return other->offlineStore()->kit()[u"settings"_s].toObject()[u"storeName"_s] == u"Renamed"_s
                                 && t.h->remote->offlineStore()->kit()[u"settings"_s].toObject()[u"storeName"_s] == u"Renamed"_s; }, 8000));
    CHECK(same(*t.h->remote, u"Handheld"_s));
    CHECK(same(*other, u"Handheld 2"_s));
}

TEST_CASE("Working offline: the kit keeps the store's secrets, and a check is taken in once", "[net][offline]")
{
    test::RecordingSink sink;
    PosShared shared(test::seedPosData(), &sink);
    shared.settings.stripeSecretKey = "sk_test_secret";
    shared.settings.backupKey = "backup-key";
    shared.settings.replicaKey = "replica-key";
    shared.settings.textWebhook = "https://example.invalid/sms";
    shared.setMaintainerPin(u"86420"_s);
    core::TerminalConfig bar;
    bar.name = "Bar";
    bar.id = "dev-bar";
    bar.key = "device-key";
    bar.cardReader = "stripe";
    shared.settings.terminals.push_back(bar);
    PosService store(&shared, u"Bar"_s);
    const QJsonObject kit = store.offlineKit();
    const QByteArray text = QJsonDocument(kit).toJson();
    for (const char *secret : {"sk_test_secret", "backup-key", "replica-key", "example.invalid", "device-key"})
        CHECK_FALSE(text.contains(secret));
    const auto data = app::offlineData(kit);
    REQUIRE(data);
    CHECK(data->settings.maintainerPinHash.empty());
    CHECK(std::ranges::all_of(data->settings.tenders, [](const core::Tender &t) {
        return t.kind == core::TenderKind::Cash || t.kind == core::TenderKind::Discount;
    }));
    CHECK(std::ranges::none_of(data->settings.terminals, [](const core::TerminalConfig &t) { return !t.cardReader.empty(); }));
    CHECK_FALSE(data->employees.empty());
    CHECK(!data->employees.front().pinHash.empty());   // PINs are checked there: memory only

    // A check made offline, kept until taken in, the journal surviving a restart.
    QTemporaryDir dir;
    const QString journal = dir.filePath(u"offline.db"_s);
    qint64 made = 0;
    {
        net::OfflineStore offline(u"Bar"_s, journal);
        offline.setKit(kit);
        app::PosService *pos = offline.start();
        REQUIRE(pos);
        REQUIRE(pos->loginWithPin(u"1111"_s));
        REQUIRE(pos->startCheck(core::CheckType::Quick));
        pos->addItem(u"water"_s);
        made = pos->checkInfo()[u"id"_s].toLongLong();
        CHECK(offline.pendingCount() == 1);
    }
    net::OfflineStore again(u"Bar"_s, journal);
    REQUIRE(again.pendingCount() == 1);
    const QJsonObject item = again.pending().first().toObject();
    CHECK(item[u"check"_s].toObject()[u"id"_s].toInteger() == made);
    CHECK(!item[u"check"_s].toObject()[u"offlineId"_s].toString().isEmpty());

    // Taken in: its number if it's the next one here, once however often it's sent.
    const QVariantMap first = store.takeOfflineCheck(item[u"check"_s].toObject());
    REQUIRE(first.contains(u"id"_s));
    const QVariantMap second = store.takeOfflineCheck(item[u"check"_s].toObject());
    CHECK(second[u"id"_s] == first[u"id"_s]);
    CHECK(second[u"again"_s].toBool());
    CHECK(std::ranges::count_if(shared.open, [](const auto &kv) { return !kv.second.offlineId.empty(); }) == 1);
    // Another screen's check with the same number: the next one instead.
    QJsonObject other = item[u"check"_s].toObject();
    other[u"offlineId"_s] = u"another-screen"_s;
    const QVariantMap third = store.takeOfflineCheck(other);
    CHECK(third[u"id"_s].toLongLong() == first[u"id"_s].toLongLong() + 1);
    again.taken(item[u"uid"_s].toString());
    CHECK(again.pendingCount() == 0);

    // Sent again after End of Day put it in the past days: still the same check.
    QJsonObject late = item[u"check"_s].toObject();
    late[u"offlineId"_s] = u"ended-day"_s;
    core::Check saved = *app::checkFromJson(late);
    saved.id = 777;
    shared.history = [&](std::int64_t, std::int64_t) { return std::vector<core::Check>{saved}; };
    const QVariantMap fourth = store.takeOfflineCheck(late);
    CHECK(fourth[u"id"_s].toLongLong() == 777);
    CHECK(fourth[u"again"_s].toBool());
}

TEST_CASE("Working offline on the screen: Work Offline, the bar says so, and back", "[net][remote][offline][ui]")
{
    OfflineSide t;
    t.connect();
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&t.h->c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    engine.loadFromModule("ViewTouch", "Main");
    REQUIRE_FALSE(engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    const auto find = [&](const QString &name) -> QQuickItem * {
        const std::function<QQuickItem *(QQuickItem *)> look = [&](QQuickItem *root) -> QQuickItem * {
            for (QQuickItem *i : root->childItems()) {
                if (i->objectName() == name && i->isVisible())
                    return i;
                if (QQuickItem *hit = look(i))
                    return hit;
            }
            return nullptr;
        };
        return look(window->contentItem());
    };
    const auto shot = [&](const char *name) {
        if (const QByteArray dir = qgetenv("VTM_SHOTS"); !dir.isEmpty())
            window->grabWindow().save(QString::fromLocal8Bit(dir) + u'/' + QString::fromLatin1(name) + u".png"_s);
    };
    CHECK_FALSE(find(u"offlineBar"_s));

    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return find(u"workOffline"_s) != nullptr; }, 4000));
    shot("offline-1-cut");
    QQuickItem *key = find(u"workOffline"_s);
    QTest::mouseClick(window, Qt::LeftButton, {}, key->mapToScene(QPointF(key->width() / 2, key->height() / 2)).toPoint());
    REQUIRE(waitFor([&] { return t.h->remote->offlineMode(); }));
    CHECK(waitFor([&] { return find(u"workOffline"_s) == nullptr; }));
    REQUIRE(find(u"offlineBar"_s));
    t.h->pin("1111");
    t.h->remote->invoke(u"startCheck"_s, {u"quick"_s});
    t.h->c.orderItem(u"water"_s);
    t.h->settle();
    QTest::qWait(150);
    shot("offline-2-working");
    t.h->remote->invoke(u"tender"_s, {u"cash"_s});
    t.h->remote->invoke(u"closeCheck"_s);
    t.h->settle();
    QTest::qWait(100);
    shot("offline-3-closed");

    t.wifi.cut = false;
    REQUIRE(waitFor([&] { return !t.h->remote->offlineMode() && t.h->remote->offlinePending() == 0; }, 8000));
    CHECK(waitFor([&] { return find(u"offlineBar"_s) == nullptr; }));
    QTest::qWait(150);
    shot("offline-4-back");
    CHECK(t.takenIn(true));
}

TEST_CASE("Working offline: a screen restarted before the store came back still hands its checks in", "[net][remote][offline]")
{
    OfflineSide t;
    t.connect();
    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));
    t.h->remote->workOffline();
    REQUIRE(t.h->remote->offlineMode());
    t.h->pin("1111");
    t.h->remote->invoke(u"startCheck"_s, {u"quick"_s});
    t.h->c.orderItem(u"water"_s);
    t.h->remote->invoke(u"tender"_s, {u"cash"_s});
    t.h->remote->invoke(u"closeCheck"_s);
    t.h->settle();
    REQUIRE(t.h->remote->offlinePending() == 1);
    // The screen restarts (its kit is gone with it: memory only)...
    const net::Credentials creds = t.h->remote->credentials();
    t.h.reset();
    auto remote = std::make_unique<net::RemoteSession>(u"Handheld"_s);
    remote->setCredentials(creds);
    remote->setOfflineJournal(t.dir.filePath(u"offline.db"_s));
    CHECK(remote->offlinePending() == 1);
    CHECK_FALSE(remote->offlineReady());
    // ...and once the store answers, the check goes to it.
    t.wifi.cut = false;
    remote->connectTo(u"127.0.0.1"_s, creds.port);
    REQUIRE(remote->waitForWelcome(5000));
    REQUIRE(waitFor([&] { return remote->offlinePending() == 0; }));
    CHECK(t.takenIn(true));
}

TEST_CASE("Store style: the hub switches the pages' theme and tells every screen", "[net][style]")
{
    net::LayoutHub hub(seedLayout());
    QSignalSpy changed(&hub, &net::LayoutHub::layoutChanged);
    layout::Theme modern;
    modern.name = u"Modern"_s;
    REQUIRE(hub.useStyle(u"modern"_s, modern));
    CHECK(changed.size() == 1);
    CHECK(layout::Layout::styleOf(hub.layout().theme) == u"modern"_s);
    CHECK(hub.layout().theme.name == u"Modern"_s);
    CHECK_FALSE(hub.useStyle(u"modern"_s, modern));   // nothing to do
    CHECK(changed.size() == 1);

    // The setting keeps its value through JSON (Store Settings).
    core::PosSettings s;
    CHECK(s.style == "classic");
    s.style = "modern";
    CHECK(app::settingsFromJson(app::toJson(s)).style == "modern");
}

TEST_CASE("Remote terminal: a food truck screen opens as the counter; a manager steps in and back out", "[net][remote][truck]")
{
    Store store;
    store.shared.settings.serviceMode = "foodTruck";
    Screen truck(store.terminal(u"Truck"_s));
    REQUIRE(waitFor([&] { return truck.remote->loggedIn(); }));
    truck.settle();
    CHECK(truck.remote->userName() == u"Counter"_s);
    CHECK(waitFor([&] { return truck.c.pageId() == u"index-lunch"_s; }));

    // An order: numbered, named, a pager; sent.
    truck.remote->invoke(u"addItem"_s, {u"caesar"_s});
    truck.settle();
    REQUIRE(waitFor([&] { return truck.remote->checkInfo()[u"orderNumber"_s].toInt() == 1; }));
    truck.remote->setCustomer({{u"name"_s, u"Maria"_s}});
    truck.remote->setPager(12);
    truck.tap(u"flow-send"_s);
    REQUIRE(waitFor([&] { return truck.remote->truckOrders().size() == 1; }));
    const QVariantMap order = truck.remote->truckOrders().first().toMap();
    CHECK(order[u"label"_s].toString() == u"#1 Maria"_s);
    CHECK(order[u"pager"_s].toInt() == 12);

    // Orders -> Manager asks for a PIN; a manager's opens it.
    truck.tap(u"flow-orders"_s);
    CHECK(truck.c.pageId() == u"truck-orders"_s);
    truck.tap(u"manager"_s);
    REQUIRE(waitFor([&] { return truck.c.pageId() == u"login"_s; }));
    truck.pin("1234");
    REQUIRE(waitFor([&] { return truck.c.pageId() == u"manager"_s; }));
    CHECK(truck.remote->userName() == u"Morgan (Manager)"_s);

    // Log Out: the counter again, Maria's order still open, the menu.
    truck.remote->invoke(u"logout"_s, {});
    REQUIRE(waitFor([&] { return truck.remote->userName() == u"Counter"_s; }));
    CHECK(waitFor([&] { return truck.c.pageId() == u"index-lunch"_s; }));
    CHECK(truck.remote->checkInfo()[u"label"_s].toString() == u"#1 Maria"_s);

    // Ready from the screen: called, its pager named.
    QSignalSpy notices(truck.remote.get(), &net::RemoteSession::notice);
    truck.remote->orderReady(truck.remote->checkInfo()[u"id"_s].toLongLong());
    CHECK(waitFor([&] {
        for (const QList<QVariant> &n : notices)
            if (n.first().toString() == u"#1 Maria is ready: buzz pager 12"_s)
                return true;
        return false;
    }));
    CHECK(waitFor([&] { return truck.remote->truckOrders().first().toMap()[u"ready"_s].toBool(); }));
}

TEST_CASE("Working offline: a food truck's second screen keeps selling as the counter; its numbers don't clash", "[net][remote][offline][truck]")
{
    OfflineSide t;
    t.store.shared.settings.serviceMode = "foodTruck";
    t.store.shared.settings.cashMode = core::CashMode::ServerBank;
    // The window (the store's computer) has sold #1 and #2 already.
    PosService window(&t.store.shared, u"Window"_s);
    REQUIRE(window.serviceInfo()[u"counter"_s].toBool());
    for (int i = 0; i < 2; ++i) {
        REQUIRE(window.addItem(u"soda"_s));
        REQUIRE(window.tender(u"cash"_s));
        REQUIRE(window.closeCheck());
    }
    t.connect();
    REQUIRE(waitFor([&] { return t.h->remote->loggedIn(); }));
    CHECK(t.h->remote->userName() == u"Counter"_s);

    // The cable comes out: the second screen works on its own, still as the counter.
    t.wifi.cut = true;
    REQUIRE(waitFor([&] { return !t.h->remote->online(); }, 4000));
    t.h->remote->workOffline();
    REQUIRE(t.h->remote->offlineMode());
    REQUIRE(waitFor([&] { return t.h->remote->loggedIn(); }));
    CHECK(t.h->remote->userName() == u"Counter"_s);
    t.h->c.orderItem(u"soda"_s);
    t.h->settle();
    REQUIRE(t.h->remote->hasCheck());
    const int offlineNumber = t.h->remote->checkInfo()[u"orderNumber"_s].toInt();
    CHECK(offlineNumber > 2);
    t.h->remote->invoke(u"setCustomer"_s, {QVariantMap{{u"name"_s, u"Ana"_s}}});
    t.h->remote->invoke(u"setPager"_s, {4});
    t.h->remote->invoke(u"tender"_s, {u"cash"_s});
    t.h->remote->invoke(u"closeCheck"_s);
    t.h->settle();
    CHECK_FALSE(t.h->remote->hasCheck());

    // Meanwhile the window goes on: its next number isn't the one given offline.
    REQUIRE(window.addItem(u"soda"_s));
    const int windowNumber = window.checkInfo()[u"orderNumber"_s].toInt();
    CHECK(windowNumber == 3);
    CHECK(windowNumber != offlineNumber);
    window.releaseCheck();

    // Back: Ana's order reaches the store with her number, her name and her pager, on the Orders list.
    t.wifi.cut = false;
    REQUIRE(waitFor([&] { return t.h->remote->online(); }, 8000));
    REQUIRE(waitFor([&] { return !t.h->remote->offlineMode(); }, 8000));
    REQUIRE(waitFor([&] { return t.h->remote->offlinePending() == 0; }));
    const core::Check *closed = t.takenIn(true);
    REQUIRE(closed);
    CHECK(closed->orderNumber == offlineNumber);
    CHECK(closed->label == "#" + std::to_string(offlineNumber) + " Ana");
    CHECK(closed->pager == 4);
    CHECK(closed->serverName == "Counter");
    bool listed = false;
    for (const QVariant &o : window.truckOrders())
        listed = listed || o.toMap()[u"number"_s].toInt() == offlineNumber;
    CHECK(listed);
    // And the second screen is the counter again, on the store's computer.
    CHECK(waitFor([&] { return t.h->remote->userName() == u"Counter"_s; }));
}
