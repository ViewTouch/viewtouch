#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "net/layout_hub.hh"
#include "net/pairing.hh"
#include "net/pos_server.hh"
#include "net/standby.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QDeadlineTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;

// The store's standby server: a live copy of the main server's data, and
// taking over when asked (a manager's PIN) or when the main falls silent.

namespace {

layout::Layout seedLayout()
{
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    return *l;
}

// Process events until `done` or the time is up.
bool waitFor(const std::function<bool()> &done, int msec = 5000)
{
    QDeadlineTimer deadline(msec);
    while (!done()) {
        if (deadline.hasExpired())
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return true;
}

// A main server with its database, as main.cpp sets it up.
struct MainServer {
    QString path;
    std::unique_ptr<storage::AsyncWriter> writer;
    std::unique_ptr<storage::SqlPosSink> sink;
    std::unique_ptr<app::PosShared> shared;
    net::LayoutHub hub{seedLayout()};
    std::unique_ptr<net::PosServer> server;

    explicit MainServer(QString dbPath)
        : path(std::move(dbPath))
    {
        const auto seed = test::seedPosData();
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
        storage::LayoutStore pages(path);   // a whole store: pages too
        REQUIRE(pages.open());
        REQUIRE(pages.save(seedLayout()));
        writer = std::make_unique<storage::AsyncWriter>(path);
        sink = std::make_unique<storage::SqlPosSink>(*writer);
        shared = std::make_unique<app::PosShared>(*store.load(), sink.get());
        server = std::make_unique<net::PosServer>(shared.get(), &hub);
        server->setSnapshotSource([this] {
            writer->flush();
            QTemporaryDir tmp;
            const QString file = tmp.filePath(u"copy.db"_s);
            QString error;
            const bool ok = storage::backupDatabase(path, file, &error);
            INFO(error.toStdString());
            REQUIRE(ok);
            QFile f(file);
            REQUIRE(f.open(QIODevice::ReadOnly));
            return f.readAll();
        });
        writer->setMirror([this](const QJsonObject &op) { server->replicate(op); });
        REQUIRE(server->listen(QHostAddress::LocalHost, 0));
    }

    net::Credentials standbyCredentials() const
    {
        net::Credentials c;
        c.serverId = QString::fromStdString(shared->settings.serverId);
        c.serverName = QString::fromStdString(shared->settings.storeName);
        c.host = u"127.0.0.1"_s;
        c.port = server->port();
        c.replicaKey = QByteArray::fromBase64(QByteArray::fromStdString(shared->settings.replicaKey));
        return c;
    }
};

std::optional<app::PosData> load(const QString &path)
{
    storage::PosStore store(path);
    return store.open() ? store.load() : std::nullopt;
}

} // namespace

TEST_CASE("Standby: a full copy, then every change as it is saved", "[standby]")
{
    QTemporaryDir dir;
    MainServer main(dir.filePath(u"main.db"_s));
    const QString copy = dir.filePath(u"standby.db"_s);

    net::ReplicaClient replica(copy, main.standbyCredentials());
    QSignalSpy synced(&replica, &net::ReplicaClient::synced);
    replica.start();
    REQUIRE(synced.wait(10000));
    CHECK(replica.inSync());
    CHECK(main.server->standbyCount() == 1);
    {
        const auto data = load(copy);
        REQUIRE(data);
        CHECK(data->employees.size() == main.shared->employees.size());
        CHECK(data->openChecks.empty());
    }

    // An order on the main server reaches the copy.
    app::PosService bar(main.shared.get(), u"Bar"_s);
    REQUIRE(bar.loginWithPin(u"1111"_s));
    REQUIRE(bar.clockIn());
    bar.selectTable(u"T2"_s);
    bar.startCheck(core::CheckType::DineIn);
    bar.addItem(u"caesar"_s);
    bar.sendOrder();
    bar.releaseCheck();
    main.writer->flush();
    REQUIRE(waitFor([&] {
        replica.flush();
        const auto data = load(copy);
        return data && data->openChecks.size() == 1;
    }));
    const auto data = load(copy);
    CHECK(data->openChecks[0].label == "T2");
    CHECK(data->openChecks[0].lines[0].sent);
    CHECK(data->punches.size() == 1);
}

TEST_CASE("Standby: refused without the store's server key", "[standby]")
{
    QTemporaryDir dir;
    MainServer main(dir.filePath(u"main.db"_s));
    auto wrong = main.standbyCredentials();
    wrong.replicaKey = net::newDeviceKey();
    net::ReplicaClient replica(dir.filePath(u"standby.db"_s), wrong);
    QSignalSpy synced(&replica, &net::ReplicaClient::synced);
    replica.start();
    CHECK_FALSE(synced.wait(1500));
    CHECK(main.server->standbyCount() == 0);
    CHECK_FALSE(QFile::exists(dir.filePath(u"standby.db"_s)));

    // A standby pairing hands out the key; a terminal's pairing doesn't.
    app::PosService office(main.shared.get(), u"Office"_s);
    REQUIRE(office.loginWithPin(u"1234"_s));
    // A screen's code doesn't pair a standby (it would get a copy of everything).
    {
        REQUIRE(office.startPairing(false));
        net::Pairer pairer;
        pairer.setStandby(true);
        QSignalSpy done(&pairer, &net::Pairer::finished);
        pairer.start(u"127.0.0.1"_s, main.server->port(), office.pairingInfo()[u"code"_s].toString(), u"Sneaky"_s);
        REQUIRE(done.wait(10000));
        CHECK_FALSE(done[0][0].toBool());
    }
    for (bool standby : {false, true}) {
        REQUIRE(office.startPairing(standby));
        net::Pairer pairer;
        pairer.setStandby(standby);
        QSignalSpy done(&pairer, &net::Pairer::finished);
        pairer.start(u"127.0.0.1"_s, main.server->port(), office.pairingInfo()[u"code"_s].toString(),
                     standby ? u"Standby"_s : u"Bar"_s);
        REQUIRE(done.wait(10000));
        REQUIRE(done[0][0].toBool());
        const auto c = done[0][1].value<net::Credentials>();
        CHECK(c.replicaKey.isEmpty() != standby);
    }
}

TEST_CASE("Standby: a screen asks it to take over with a manager's PIN", "[standby]")
{
    QTemporaryDir dir;
    MainServer main(dir.filePath(u"main.db"_s));

    // A paired screen (its key is in the store's data, so in the copy).
    app::PosService office(main.shared.get(), u"Office"_s);
    REQUIRE(office.loginWithPin(u"1234"_s));
    REQUIRE(office.startPairing());
    net::Pairer pairer;
    QSignalSpy paired(&pairer, &net::Pairer::finished);
    pairer.start(u"127.0.0.1"_s, main.server->port(), office.pairingInfo()[u"code"_s].toString(), u"Bar"_s);
    REQUIRE(paired.wait(10000));
    const auto screen = paired[0][1].value<net::Credentials>();

    const QString copy = dir.filePath(u"standby.db"_s);
    net::ReplicaClient replica(copy, main.standbyCredentials());
    QSignalSpy synced(&replica, &net::ReplicaClient::synced);
    replica.start();
    REQUIRE(synced.wait(10000));
    replica.stop();

    net::StandbyListener listener(copy, main.standbyCredentials().serverId);
    REQUIRE(listener.listen(0));
    QSignalSpy takeOver(&listener, &net::StandbyListener::takeOverRequested);
    auto ask = [&](const net::Credentials &who, const QString &pin) {
        net::TakeOverRequest request(who, u"127.0.0.1"_s, listener.port(), pin);
        QSignalSpy done(&request, &net::TakeOverRequest::finished);
        REQUIRE(done.wait(10000));
        return std::pair{done[0][0].toBool(), done[0][1].toString()};
    };

    // Not before it has a whole copy.
    CHECK_FALSE(ask(screen, u"1234"_s).first);
    listener.setReady(true);
    // Not a manager (a server's PIN), nor a stranger device.
    const auto [serverOk, why] = ask(screen, u"1111"_s);
    CHECK_FALSE(serverOk);
    CHECK(why.contains(u"manager"_s));
    auto stranger = screen;
    stranger.key = net::newDeviceKey();
    CHECK_FALSE(ask(stranger, u"1234"_s).first);
    CHECK(takeOver.isEmpty());

    CHECK(ask(screen, u"1234"_s).first);
    REQUIRE(takeOver.size() == 1);
}
