#include "net/standby.hh"

#include "app/pos_json.hh"
#include "layout/layout.hh"
#include "storage/async_writer.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QSslPreSharedKeyAuthenticator>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcStandby, "vt.standby")

namespace vt::net {

namespace {
constexpr int kRetryMs = 3'000;
}

// --- the copy -------------------------------------------------------------------------------

ReplicaClient::ReplicaClient(QString databasePath, Credentials credentials, QObject *parent)
    : QObject(parent)
    , db_(std::move(databasePath))
    , credentials_(std::move(credentials))
    , channel_(std::make_unique<LineChannel>(&socket_))
{
    socket_.setSslConfiguration(tlsConfiguration());
    connect(&socket_, &QSslSocket::preSharedKeyAuthenticationRequired, this, [this](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity("r:" + credentials_.serverId.toLatin1());
        a->setPreSharedKey(credentials_.replicaKey);
    });
    connect(&socket_, &QSslSocket::encrypted, this, [this] {
        failures_ = 0;
        setStatus(tr("Connected to the main server; copying the store…"));
        channel_->send({{u"t"_s, u"hello"_s}, {u"role"_s, u"standby"_s}, {u"protocol"_s, ProtocolVersion}});
    });
    connect(&socket_, &QSslSocket::readyRead, this, &ReplicaClient::onReadyRead);
    connect(&socket_, &QSslSocket::disconnected, this, [this] {
        if (inSync_)
            qCWarning(lcStandby) << "lost the main server";
        inSync_ = false;
        snapshot_.clear();
        setStatus(tr("The main server isn't answering. Ready to take over."));
        if (!stopped_)
            retry_.start();
    });
    connect(&socket_, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (stopped_)
            return;
        // Every third try, look for it on the network (its address may have changed).
        if (++failures_ % 3 == 0)
            finder_.search();
        if (!inSync_)
            setStatus(tr("Can't reach the main server (%1).").arg(socket_.errorString()));
        if (!retry_.isActive())
            retry_.start();
    });
    connect(&finder_, &ServerFinder::found, this, [this](const FoundServer &s) {
        if (s.role != u"main" || s.id != credentials_.serverId || s.host == credentials_.host
            || socket_.state() != QAbstractSocket::UnconnectedState)
            return;
        credentials_.host = s.host;
        credentials_.port = s.port;
        emit credentialsChanged(credentials_);
        connectNow();
    });
    retry_.setSingleShot(true);
    retry_.setInterval(kRetryMs);
    connect(&retry_, &QTimer::timeout, this, &ReplicaClient::connectNow);
}

ReplicaClient::~ReplicaClient()
{
    stop();
}

void ReplicaClient::start()
{
    stopped_ = false;
    connectNow();
}

void ReplicaClient::stop()
{
    stopped_ = true;
    retry_.stop();
    socket_.disconnect(this);
    socket_.abort();
    if (writer_)
        writer_->flush();
    writer_.reset();
    inSync_ = false;
}

void ReplicaClient::flush()
{
    if (writer_)
        writer_->flush();
}

void ReplicaClient::connectNow()
{
    if (stopped_ || socket_.state() != QAbstractSocket::UnconnectedState)
        return;
    socket_.connectToHostEncrypted(credentials_.host, credentials_.port);
}

void ReplicaClient::setStatus(const QString &status)
{
    if (status == status_)
        return;
    status_ = status;
    qCInfo(lcStandby).noquote() << status;
    emit statusChanged();
}

void ReplicaClient::onReadyRead()
{
    bool overflow = false;
    for (const QJsonObject &m : channel_->receive(&overflow))
        handle(m);
    if (overflow)
        socket_.abort();
}

void ReplicaClient::handle(const QJsonObject &m)
{
    lastHeard_ = QDateTime::currentMSecsSinceEpoch();
    const QString t = m.value(u"t").toString();
    if (t == u"snap") {
        const int i = m.value(u"i").toInt();
        if (i == 0)
            snapshot_.clear();
        snapshot_.append(QByteArray::fromBase64(m.value(u"data").toString().toLatin1()));
        if (i + 1 == m.value(u"n").toInt())
            installSnapshot();
        return;
    }
    if (t == u"op" && writer_) {
        const QString table = m.value(u"table").toString();
        const QString key = m.value(u"key").toString();
        if (m.value(u"k").toString() == u"up")
            writer_->upsert(table, key, m.value(u"row").toObject().toVariantMap());
        else
            writer_->remove(table, m.value(u"column").toString(), key);
        return;
    }
    if (t == u"layout" && inSync_) {
        QStringList errors;
        if (auto layout = layout::Layout::fromJson(m.value(u"layout").toObject(), &errors)) {
            storage::LayoutStore pages(db_);
            QString error;
            if (!pages.open(&error) || !pages.save(*layout, &error))
                qCWarning(lcStandby).noquote() << "could not keep the pages:" << error;
        }
    }
}

void ReplicaClient::installSnapshot()
{
    // The whole store, checked, then in place of the old copy.
    writer_.reset();
    const QString part = db_ + u".incoming"_s;
    QFile f(part);
    if (!f.open(QIODevice::WriteOnly) || f.write(snapshot_) != snapshot_.size()) {
        setStatus(tr("Could not write the copy: %1").arg(f.errorString()));
        return;
    }
    f.close();
    snapshot_.clear();
    QString error;
    if (!storage::verifyDatabase(part, &error)) {
        QFile::remove(part);
        setStatus(tr("The copy arrived damaged (%1); asking again.").arg(error));
        socket_.abort();
        return;
    }
    for (const QString &suffix : {u""_s, u"-wal"_s, u"-shm"_s})
        QFile::remove(db_ + suffix);
    if (!QFile::rename(part, db_)) {
        setStatus(tr("Could not put the copy in place."));
        return;
    }
    writer_ = std::make_unique<storage::AsyncWriter>(db_);
    inSync_ = true;
    setStatus(tr("In sync with the main server at %1.").arg(credentials_.host));
    emit synced();
}

// --- taking over -----------------------------------------------------------------------------

StandbyListener::StandbyListener(QString databasePath, QString storeId, QObject *parent)
    : QObject(parent)
    , db_(std::move(databasePath))
    , storeId_(std::move(storeId))
{
    server_.setSslConfiguration(tlsConfiguration());
    // Paired screens connect with their own keys: they are in the copy.
    connect(&server_, &QSslServer::preSharedKeyAuthenticationRequired, this,
            [this](QSslSocket *socket, QSslPreSharedKeyAuthenticator *auth) {
        QByteArray key = newDeviceKey();   // nobody has it: unknown devices fail
        const QByteArray identity = auth->identity();
        if (identity.startsWith("t:")) {
            storage::PosStore store(db_);
            if (store.open()) {
                if (const auto data = store.load()) {
                    if (const core::TerminalConfig *t = data->settings.pairedTerminal(identity.mid(2).toStdString())) {
                        key = QByteArray::fromBase64(QByteArray::fromStdString(t->key));
                        socket->setProperty("vtTerminal", QString::fromStdString(t->name));
                    }
                }
            }
        }
        auth->setPreSharedKey(key);
    });
    connect(&server_, &QTcpServer::pendingConnectionAvailable, this, [this] {
        while (auto *socket = qobject_cast<QSslSocket *>(server_.nextPendingConnection())) {
            auto *channel = new LineChannel(socket);
            connect(socket, &QTcpSocket::disconnected, socket, [socket, channel] {
                delete channel;
                socket->deleteLater();
            });
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, channel] {
                for (const QJsonObject &m : channel->receive()) {
                    if (m.value(u"t").toString() != u"takeOver")
                        continue;
                    if (!ready_) {
                        channel->send({{u"t"_s, u"error"_s}, {u"text"_s, tr("The standby doesn't have a complete copy yet.")}});
                        continue;
                    }
                    // A manager's PIN, checked against the copy.
                    storage::PosStore store(db_);
                    const auto data = store.open() ? store.load() : std::nullopt;
                    const QString pin = m.value(u"pin").toString();
                    // (A named list: `manager` points into it after the loop.)
                    const std::vector<core::Employee> staff = data ? data->employees : std::vector<core::Employee>{};
                    const core::Employee *manager = nullptr;
                    for (const core::Employee &e : staff) {
                        if (e.active && e.can(core::perm::Manager) && !e.pinSalt.empty()
                            && app::hashPin(pin, e.pinSalt) == e.pinHash)
                            manager = &e;
                    }
                    if (!manager) {
                        channel->send({{u"t"_s, u"error"_s}, {u"text"_s, tr("That isn't a manager's PIN.")}});
                        continue;
                    }
                    const QString by = QString::fromStdString(manager->name);
                    qCWarning(lcStandby).noquote() << "taking over as the main server, asked by" << by << "on"
                                                   << socket->property("vtTerminal").toString();
                    channel->send({{u"t"_s, u"tookOver"_s}});
                    socket->flush();
                    emit takeOverRequested(u"asked by "_s + by);
                }
            });
        }
    });
}

bool StandbyListener::listen(quint16 port)
{
    return server_.listen(QHostAddress::Any, port);
}

TakeOverRequest::TakeOverRequest(const Credentials &terminal, QString host, quint16 port, QString pin, QObject *parent)
    : QObject(parent)
    , channel_(std::make_unique<LineChannel>(&socket_))
    , pin_(std::move(pin))
{
    socket_.setSslConfiguration(tlsConfiguration());
    const QByteArray identity = deviceIdentity(terminal.terminalId);
    const QByteArray key = terminal.key;
    connect(&socket_, &QSslSocket::preSharedKeyAuthenticationRequired, this,
            [identity, key](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity(identity);
        a->setPreSharedKey(key);
    });
    connect(&socket_, &QSslSocket::encrypted, this, [this] {
        channel_->send({{u"t"_s, u"takeOver"_s}, {u"pin"_s, pin_}});
    });
    connect(&socket_, &QSslSocket::readyRead, this, [this] {
        for (const QJsonObject &m : channel_->receive()) {
            if (done_)
                return;
            done_ = true;
            const bool ok = m.value(u"t").toString() == u"tookOver";
            emit finished(ok, ok ? tr("The standby is taking over. This screen will reconnect to it.")
                                 : m.value(u"text").toString());
            socket_.disconnectFromHost();
        }
    });
    connect(&socket_, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (done_)
            return;
        done_ = true;
        emit finished(false, tr("Couldn't reach the standby: %1").arg(socket_.errorString()));
    });
    socket_.connectToHostEncrypted(host, port);
}

} // namespace vt::net
