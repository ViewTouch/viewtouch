#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "net/discovery.hh"
#include "net/layout_hub.hh"
#include "net/pairing.hh"
#include "net/pos_server.hh"
#include "net/remote_session.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;

// Pairing devices: encrypted connections that only paired terminals open.

namespace {

layout::Layout seedLayout()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

struct Store {
    std::int64_t clock = 1'700'000'000'000;
    PosShared shared{test::seedPosData(), nullptr};
    net::LayoutHub hub{seedLayout()};
    net::PosServer server{&shared, &hub};
    PosService manager{&shared, u"Office"_s};

    Store()
    {
        shared.setClock([this] { return clock; });
        REQUIRE(server.listen(QHostAddress::LocalHost, 0));
        REQUIRE(manager.loginWithPin(u"1234"_s));
    }

    QString startPairing()
    {
        REQUIRE(manager.startPairing());
        return manager.pairingInfo()[u"code"_s].toString();
    }

    // Pair and return {ok, credentials, error}.
    std::tuple<bool, net::Credentials, QString> pair(const QString &code, const QString &name)
    {
        net::Pairer pairer;
        QSignalSpy done(&pairer, &net::Pairer::finished);
        pairer.start(u"127.0.0.1"_s, server.port(), code, name);
        REQUIRE(done.wait(10000));
        return {done[0][0].toBool(), done[0][1].value<net::Credentials>(), done[0][2].toString()};
    }
};

} // namespace

TEST_CASE("Pairing codes: typed loosely, stretched into a key", "[pairing]")
{
    CHECK(net::normalizePairingCode(u"k7qm4 xhp2w"_s) == u"K7QM4-XHP2W"_s);
    CHECK(net::normalizePairingCode(u"K7QM4-XHP2W"_s) == u"K7QM4-XHP2W"_s);
    CHECK(net::normalizePairingCode(u"o1lIo-00000"_s) == u"01110-00000"_s);   // look-alikes
    CHECK(net::normalizePairingCode(u"K7QM4-XHP2"_s).isEmpty());             // too short
    CHECK(net::normalizePairingCode(u"K7QM4-XHP2U"_s).isEmpty());            // U is not used
    CHECK(net::normalizePairingCode(u"K7QM4-XHP2!"_s).isEmpty());

    const QByteArray key = net::pairingKey(u"K7QM4-XHP2W"_s);
    CHECK(key.size() == 32);
    CHECK(key == net::pairingKey(u"K7QM4-XHP2W"_s));
    CHECK(key != net::pairingKey(u"K7QM4-XHP2X"_s));
    CHECK(net::newDeviceKey().size() == 32);
    CHECK(net::newDeviceKey() != net::newDeviceKey());
    CHECK(net::tlsAvailable());
}

TEST_CASE("A manager's code pairs one device, which then connects with its own key", "[pairing]")
{
    Store store;
    CHECK_FALSE(store.shared.settings.serverId.empty());
    const QString code = store.startPairing();
    REQUIRE(net::normalizePairingCode(code) == code);

    // Servers don't see the code.
    PosService server(&store.shared, u"Front"_s);
    REQUIRE(server.loginWithPin(u"1111"_s));
    CHECK_FALSE(server.pairingInfo()[u"active"_s].toBool());
    CHECK_FALSE(server.startPairing());

    const auto [ok, creds, error] = store.pair(code.toLower().remove(u'-'), u"Patio Tablet"_s);
    REQUIRE(ok);
    CHECK(error.isEmpty());
    CHECK(creds.valid());
    CHECK(creds.terminalName == u"Patio Tablet"_s);
    CHECK(creds.serverId == QString::fromStdString(store.shared.settings.serverId));
    CHECK(creds.serverName == QString::fromStdString(store.shared.settings.storeName));
    const core::TerminalConfig *t = store.shared.settings.pairedTerminal(creds.terminalId.toStdString());
    REQUIRE(t);
    CHECK(t->name == "Patio Tablet");
    CHECK_FALSE(store.shared.activePairing());           // one device per code
    CHECK(store.manager.adminRecords(u"terminals"_s).last().toMap()[u"_detail"_s].toString().contains(u"paired"_s));
    CHECK_FALSE(store.manager.adminRecords(u"terminals"_s).last().toMap().contains(u"key"_s));

    // The code is used up.
    const auto [again, c2, againError] = store.pair(code, u"Second Tablet"_s);
    CHECK_FALSE(again);
    CHECK(againError.contains(u"wrong or has expired"_s));

    // The device connects under the name it was paired with, whatever it says.
    net::RemoteSession remote(u"Spoofed Name"_s);
    net::Credentials anonymous = creds;
    anonymous.terminalName.clear();
    remote.setCredentials(anonymous);
    remote.connectTo(creds.host, creds.port);
    REQUIRE(remote.waitForWelcome(5000));
    CHECK(remote.terminalName() == u"Patio Tablet"_s);
}

TEST_CASE("Pairing refuses wrong, expired and stopped codes", "[pairing]")
{
    Store store;
    const QString code = store.startPairing();

    auto [ok, c, error] = store.pair(u"ZZZZZ-ZZZZZ"_s, u"Tablet"_s);
    CHECK_FALSE(ok);
    CHECK(error.contains(u"wrong or has expired"_s));
    std::tie(ok, c, error) = store.pair(u"nonsense"_s, u"Tablet"_s);
    CHECK_FALSE(ok);
    CHECK(error.contains(u"10 letters"_s));
    std::tie(ok, c, error) = store.pair(code, u"   "_s);
    CHECK_FALSE(ok);

    store.clock += 11 * 60 * 1000;                       // past its 10 minutes
    CHECK_FALSE(store.manager.pairingInfo()[u"active"_s].toBool());
    std::tie(ok, c, error) = store.pair(code, u"Tablet"_s);
    CHECK_FALSE(ok);

    const QString fresh = store.startPairing();
    REQUIRE(store.manager.stopPairing());
    std::tie(ok, c, error) = store.pair(fresh, u"Tablet"_s);
    CHECK_FALSE(ok);
    CHECK(store.shared.settings.terminals.empty());
}

TEST_CASE("Unpairing a device disconnects it and keeps it out", "[pairing]")
{
    Store store;
    const auto [ok, creds, error] = store.pair(store.startPairing(), u"Bar Tablet"_s);
    REQUIRE(ok);
    net::RemoteSession remote(u"Bar Tablet"_s);
    remote.setCredentials(creds);
    remote.connectTo(creds.host, creds.port);
    REQUIRE(remote.waitForWelcome(5000));
    QSignalSpy rejected(&remote, &net::RemoteSession::rejected);

    // Manager -> Terminals: remove it.
    // (The manager's own screen is listed too.)
    const QVariantList terminals = store.manager.adminRecords(u"terminals"_s);
    REQUIRE(terminals.size() == 2);
    int tablet = -1;
    for (int i = 0; i < terminals.size(); ++i)
        if (terminals[i].toMap()[u"name"_s] == u"Bar Tablet"_s)
            tablet = i;
    REQUIRE(tablet >= 0);
    REQUIRE(store.manager.adminDelete(u"terminals"_s, tablet));
    REQUIRE(rejected.wait(10000));
    CHECK(remote.isRejected());
    CHECK_FALSE(remote.online());

    // A device with a key the server never gave out gets nowhere either.
    net::Credentials forged = creds;
    forged.key = net::newDeviceKey();
    net::RemoteSession stranger(u"Stranger"_s);
    stranger.setCredentials(forged);
    stranger.connectTo(creds.host, creds.port);
    CHECK_FALSE(stranger.waitForWelcome(8000));
    CHECK(stranger.isRejected());
}

TEST_CASE("Paired credentials are saved for the owner only", "[pairing]")
{
    QTemporaryDir dir;
    net::Credentials c;
    c.serverId = u"abc"_s;
    c.serverName = u"Store"_s;
    c.host = u"10.0.0.5"_s;
    c.port = 7719;
    c.terminalId = u"0123456789abcdef"_s;
    c.terminalName = u"Patio"_s;
    c.key = net::newDeviceKey();
    const QString file = dir.filePath(u"terminal.json"_s);
    REQUIRE(c.save(file));
    CHECK(QFile::permissions(file) == (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser
                                       | QFileDevice::WriteUser));
    const auto back = net::Credentials::load(file);
    REQUIRE(back);
    CHECK(back->key == c.key);
    CHECK(back->terminalName == u"Patio"_s);
    CHECK_FALSE(net::Credentials::load(dir.filePath(u"missing.json"_s)));
}

TEST_CASE("Discovery: servers answer who they are; a terminal finds a moved server", "[pairing][discovery]")
{
    Store store;
    net::DiscoveryResponder responder(
        [&] {
            return std::pair{QString::fromStdString(store.shared.settings.serverId),
                             QString::fromStdString(store.shared.settings.storeName)};
        },
        store.server.port());
    REQUIRE(responder.listen(0));

    net::ServerFinder finder;
    QSignalSpy found(&finder, &net::ServerFinder::found);
    finder.probe(QHostAddress::LocalHost, responder.port());
    REQUIRE(found.wait(3000));
    REQUIRE(finder.servers().size() == 1);
    const net::FoundServer s = finder.servers().first();
    CHECK(s.id == QString::fromStdString(store.shared.settings.serverId));
    CHECK(s.name == QString::fromStdString(store.shared.settings.storeName));
    CHECK(s.host == u"127.0.0.1"_s);
    CHECK(s.port == store.server.port());
    finder.probe(QHostAddress::LocalHost, responder.port());   // answers again: still one entry
    REQUIRE(found.wait(3000));
    CHECK(finder.servers().size() == 1);

    // A paired terminal whose saved address is stale looks the server up by id.
    const auto [ok, creds, error] = store.pair(store.startPairing(), u"Patio Tablet"_s);
    REQUIRE(ok);
    net::Credentials stale = creds;
    stale.port = quint16(store.server.port() == 65535 ? 65534 : store.server.port() + 1);   // nothing there
    net::RemoteSession remote(u"Patio Tablet"_s);
    remote.setDiscoveryPort(responder.port());
    remote.setCredentials(stale);
    QSignalSpy moved(&remote, &net::RemoteSession::credentialsChanged);
    remote.connectTo(stale.host, stale.port);
    REQUIRE(remote.waitForWelcome(20000));
    // It may try another of the server's addresses first (this test server
    // only listens on 127.0.0.1); the one it got in on is the one saved.
    REQUIRE_FALSE(moved.isEmpty());
    CHECK(moved.last()[0].value<net::Credentials>().port == store.server.port());
    CHECK(remote.credentials().port == store.server.port());
}
