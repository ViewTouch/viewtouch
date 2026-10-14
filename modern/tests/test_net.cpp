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
    std::vector<std::pair<QTcpSocket *, QTcpSocket *>> pairs;
    explicit WifiRelay(quint16 storePort)
    {
        REQUIRE(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this, storePort] {
            while (QTcpSocket *in = server.nextPendingConnection()) {
                auto *out = new QTcpSocket(&server);
                out->connectToHost(QHostAddress::LocalHost, storePort);
                const auto pass = [this](QTcpSocket *from, QTcpSocket *to) {
                    QObject::connect(from, &QTcpSocket::readyRead, from, [this, from, to] {
                        const QByteArray bytes = from->readAll();
                        if (!cut)
                            to->write(bytes);
                    });
                };
                pass(in, out);
                pass(out, in);
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
    CHECK(waitFor([&] { return bar.openCheck(id); }, 5000));     // the store let go of it
    wifi.cut = false;                                   // (it keeps trying: a new connection, not this one)
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

    // A server can't fetch it; a manager can.
    h.pin("1111");
    QSignalSpy notices(h.remote.get(), &app::PosSession::notice);
    h.remote->getUpdate();
    REQUIRE(waitFor([&] { return h.remote->updateInfo().contains(u"error"_s); }));
    CHECK(h.remote->updateInfo()[u"error"_s].toString().contains(u"manager"_s));
    h.remote->invoke(u"logout"_s);
    REQUIRE(waitFor([&] { return !h.remote->loggedIn(); }));
    h.settle();
    h.pin("1234");
    h.remote->getUpdate();
    REQUIRE(waitFor([&] { return h.remote->updateInfo().contains(u"ready"_s); }, 10000));
    QFile got(h.remote->updateInfo()[u"ready"_s].toString());
    REQUIRE(got.open(QIODevice::ReadOnly));
    CHECK(QFileInfo(got).fileName() == mine);
    CHECK(got.readAll() == payload);
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
