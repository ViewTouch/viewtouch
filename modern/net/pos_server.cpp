#include "net/pos_server.hh"

#include "app/i18n.hh"
#include "app/pos_service.hh"
#include "net/layout_hub.hh"
#include "net/pairing.hh"
#include "net/protocol.hh"

#include <QElapsedTimer>
#include <QMetaProperty>
#include <QDateTime>
#include "net/build_info.hh"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSslPreSharedKeyAuthenticator>
#include <QPointer>
#include <QSet>
#include <QSslSocket>
#include <QLocale>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcServer, "vt.server")

namespace vt::net {

struct PosServer::Connection {
    QSslSocket *socket = nullptr;
    QString terminalId;   // the paired device, from its key
    QString pairingCode;  // or: a device pairing with this code
    std::unique_ptr<LineChannel> channel;
    std::unique_ptr<app::PosService> session;   // after hello
    bool standby = false;                       // the store's standby server (key "r:")
    bool synced = false;                        // ...has its copy, gets every change
    qint64 since = 0;
    QVariantMap sent;                           // last state sent
    quint32 dirty = ~0u;                        // what may have changed since (StatePiece::bit)
    bool subscribes = false;                    // asks for the big things it shows (want)
    QSet<QString> wanted;
    QList<QJsonObject> events;                  // queued until the next flush
    QStringList notices;
    qint64 heard = 0;         // when it last sent anything
    int build = 0;            // what it runs (hello); 0: older than build numbers
    QString version, platform;
    bool answersPings = false;   // a terminal that says "pong" (older ones don't)
};

namespace {

// A screen's state, piece by piece, and which change signal says each one
// may have changed (its property's NOTIFY, the same signal the screen on
// this computer redraws by). After a change only those pieces are made
// again; a piece without one is made every time.
struct StatePiece {
    QString key;
    QMetaProperty property;
    quint32 bit;
};
constexpr quint32 kAlways = 1u << 31;

// Big, and shown on few screens: sent only to a screen that asks (a screen
// too old to ask gets everything).
bool onRequest(const QString &key)
{
    return key == u"closedChecks";
}

// Screens that ask for things (this version) get the open checks in two
// parts (the shared part made and sent once a round); older ones the list.
bool notFor(bool subscribes, const QString &key)
{
    return subscribes ? key == u"openChecks" : key == u"openChecksShared" || key == u"openChecksViewer";
}

const std::vector<StatePiece> &statePieces()
{
    static const std::vector<StatePiece> pieces = [] {
        std::vector<StatePiece> out;
        const QMetaObject &mo = app::PosSession::staticMetaObject;
        QList<int> signalIndexes;
        for (const QString &key : app::PosSession::stateKeys()) {
            const int i = mo.indexOfProperty(key.toLatin1().constData());
            Q_ASSERT(i >= 0);
            const QMetaProperty p = mo.property(i);
            quint32 bit = kAlways;
            if (p.hasNotifySignal()) {
                int at = int(signalIndexes.indexOf(p.notifySignalIndex()));
                if (at < 0) {
                    at = int(signalIndexes.size());
                    signalIndexes << p.notifySignalIndex();
                }
                bit = at < 31 ? 1u << at : kAlways;
            }
            out.push_back({key, p, bit});
        }
        return out;
    }();
    return pieces;
}

// The shared data a list or map's copies point at (the same in every copy
// of one value; null for small values, compared as usual).
const void *identity(const QVariant &v)
{
    switch (v.metaType().id()) {
    case QMetaType::QVariantList: {
        const auto &l = *static_cast<const QVariantList *>(v.constData());
        return l.isEmpty() ? nullptr : static_cast<const void *>(l.constData());
    }
    case QMetaType::QStringList: {
        const auto &l = *static_cast<const QStringList *>(v.constData());
        return l.isEmpty() ? nullptr : static_cast<const void *>(l.constData());
    }
    case QMetaType::QVariantMap: {
        const auto &m = *static_cast<const QVariantMap *>(v.constData());
        return m.isEmpty() ? nullptr : static_cast<const void *>(&*m.constBegin());
    }
    default:
        return nullptr;
    }
}

// A value as JSON text, through Qt's JSON classes (anything unusual).
QByteArray qtJsonText(const QVariant &v)
{
    const QJsonValue j = QJsonValue::fromVariant(v);
    if (j.isArray())
        return QJsonDocument(j.toArray()).toJson(QJsonDocument::Compact);
    if (j.isObject())
        return QJsonDocument(j.toObject()).toJson(QJsonDocument::Compact);
    const QByteArray wrapped = QJsonDocument(QJsonArray{j}).toJson(QJsonDocument::Compact);
    return wrapped.mid(1, wrapped.size() - 2);
}

void appendJsonString(QByteArray &out, const QString &s)
{
    out += '"';
    const QByteArray utf8 = s.toUtf8();
    qsizetype from = 0;
    for (qsizetype i = 0; i < utf8.size(); ++i) {
        const auto ch = static_cast<unsigned char>(utf8[i]);
        if (ch >= 0x20 && ch != '"' && ch != '\\')
            continue;
        out.append(utf8.constData() + from, i - from);
        from = i + 1;
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default: {
            char u[7];
            std::snprintf(u, sizeof u, "\\u%04x", ch);
            out += u;
        }
        }
    }
    out.append(utf8.constData() + from, utf8.size() - from);
    out += '"';
}

bool appendJson(QByteArray &out, const QVariant &v)
{
    switch (v.metaType().id()) {
    case QMetaType::UnknownType:
    case QMetaType::Nullptr:
        out += "null";
        return true;
    case QMetaType::Bool:
        out += v.toBool() ? "true" : "false";
        return true;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
    case QMetaType::UShort:
        out += QByteArray::number(v.toLongLong());
        return true;
    case QMetaType::Double:
    case QMetaType::Float: {
        const double d = v.toDouble();
        if (!std::isfinite(d))
            out += "null";
        else if (d == std::trunc(d) && std::abs(d) < 1e15)
            out += QByteArray::number(qint64(d));
        else
            out += QByteArray::number(d, 'g', QLocale::FloatingPointShortest);
        return true;
    }
    case QMetaType::QString:
        appendJsonString(out, *static_cast<const QString *>(v.constData()));
        return true;
    case QMetaType::QStringList: {
        out += '[';
        bool first = true;
        for (const QString &s : *static_cast<const QStringList *>(v.constData())) {
            if (!std::exchange(first, false))
                out += ',';
            appendJsonString(out, s);
        }
        out += ']';
        return true;
    }
    case QMetaType::QVariantList: {
        out += '[';
        bool first = true;
        for (const QVariant &e : *static_cast<const QVariantList *>(v.constData())) {
            if (!std::exchange(first, false))
                out += ',';
            if (!appendJson(out, e))
                return false;
        }
        out += ']';
        return true;
    }
    case QMetaType::QVariantMap: {   // sorted by key, as QJsonObject writes it
        out += '{';
        bool first = true;
        const auto &m = *static_cast<const QVariantMap *>(v.constData());
        for (auto it = m.cbegin(); it != m.cend(); ++it) {
            if (!std::exchange(first, false))
                out += ',';
            appendJsonString(out, it.key());
            out += ':';
            if (!appendJson(out, it.value()))
                return false;
        }
        out += '}';
        return true;
    }
    default:
        return false;   // anything else: Qt's way
    }
}

} // namespace

// A value as JSON text: written straight from the data (the updates to every
// screen are mostly this), or Qt's way for anything unusual.
QByteArray jsonText(const QVariant &v)
{
    QByteArray out;
    out.reserve(256);
    return appendJson(out, v) ? out : qtJsonText(v);
}

namespace {

// The bit for a change signal (kAlways for one no piece names).
quint32 bitOfSignal(const QMetaMethod &signal)
{
    for (const StatePiece &p : statePieces())
        if (p.property.hasNotifySignal() && p.property.notifySignalIndex() == signal.methodIndex())
            return p.bit;
    return kAlways;
}

} // namespace

PosServer::PosServer(app::PosShared *shared, LayoutHub *layouts, QObject *parent)
    : QObject(parent)
    , shared_(shared)
    , layouts_(layouts)
{
    server_.setSslConfiguration(tlsConfiguration());
    connect(&server_, &QTcpServer::pendingConnectionAvailable, this, &PosServer::onNewConnection);
    connect(&server_, &QSslServer::preSharedKeyAuthenticationRequired, this, &PosServer::onPreSharedKey);
    connect(&server_, &QSslServer::errorOccurred, this, [](QSslSocket *socket, QAbstractSocket::SocketError e) {
        if (e == QAbstractSocket::SslHandshakeFailedError)
            qCInfo(lcServer).noquote() << "refused a device without a valid key from"
                                       << socket->peerAddress().toString();
    });
    connect(shared_, &app::PosShared::adminChanged, this, &PosServer::dropRevoked);
    // Terminals find this server again by its id after an address change;
    // the standby connects with the store's server key.
    if (shared_->settings.serverId.empty() || shared_->settings.replicaKey.empty()) {
        if (shared_->settings.serverId.empty())
            shared_->settings.serverId = newDeviceId().toStdString();
        if (shared_->settings.replicaKey.empty())
            shared_->settings.replicaKey = newDeviceKey().toBase64().toStdString();
        shared_->saveSettings();
    }
    // Every 3 s: the standby knows the main is alive, and each terminal
    // that it still has the store. A terminal that answers pings and then
    // says nothing for 15 s is gone (out of Wi-Fi range: its socket never
    // closes by itself), so the check it had open is free again.
    pingTimer_.setInterval(3'000);
    connect(&pingTimer_, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::vector<Connection *> silent;
        for (auto &c : connections_) {
            if ((c->standby && c->synced) || c->session)
                c->channel->send({{u"t"_s, u"ping"_s}});
            if (c->session && c->answersPings && now - c->heard > silentMs_)
                silent.push_back(c.get());
        }
        for (Connection *c : silent) {
            qCWarning(lcServer) << "terminal stopped answering:" << c->session->terminalName();
            c->socket->abort();
            drop(c);
        }
    });
    pingTimer_.start();
    flushTimer_.setSingleShot(true);
    // Every screen's update at most every 80 ms (busy: many changes in one
    // round); the screen that made a change gets its own at once. A round
    // that took long (many screens, a big day) waits twice that before the
    // next, so the updates never take more than a third of this computer's
    // time and its own screen stays quick.
    flushTimer_.setInterval(80);
    connect(&flushTimer_, &QTimer::timeout, this, [this] {
        QElapsedTimer t;
        t.start();
        shared_->beginMemo();
        inRound_ = true;
        for (auto &c : connections_)
            flush(c.get());
        inRound_ = false;
        roundJson_.clear();
        roundSame_.clear();
        shared_->endMemo();
        updateNsecs_ += t.nsecsElapsed();
        ++updateRounds_;
        flushTimer_.setInterval(std::clamp(int(2 * t.elapsed()), 80, 2000));
    });
    tickTimer_.setInterval(30'000);
    // Now and then everything (times that move on their own: minutes seated).
    connect(&tickTimer_, &QTimer::timeout, this, [this] {
        for (auto &c : connections_)
            c->dirty = ~0u;
        scheduleFlushAll();
    });
    tickTimer_.start();

    // Pages saved anywhere reach every other terminal.
    connect(layouts_, &LayoutHub::layoutChanged, this, [this](const layout::Layout &layout, const void *origin) {
        const QJsonObject msg{{u"t"_s, u"layout"_s}, {u"layout"_s, layout.toJson()}};
        for (auto &c : connections_) {
            if ((c->session && c.get() != origin) || (c->standby && c->synced))
                c->channel->send(msg);
        }
    });
}

PosServer::~PosServer()
{
    for (auto &c : connections_) {
        c->socket->disconnect(this);
        c->socket->abort();
    }
}

bool PosServer::listen(const QHostAddress &address, quint16 port)
{
    if (!tlsAvailable(&error_))
        return false;
    error_.clear();
    return server_.listen(address, port);
}

void PosServer::onPreSharedKey(QSslSocket *socket, QSslPreSharedKeyAuthenticator *auth)
{
    const QByteArray identity = auth->identity();
    QByteArray key;
    if (identity == PairingIdentity) {
        if (const app::PosShared::Pairing *p = shared_->activePairing()) {
            if (!pairingKeys_.contains(p->code))
                pairingKeys_.insert(p->code, pairingKey(p->code));
            key = pairingKeys_.value(p->code);
            socket->setProperty("vtPairingCode", p->code);
        }
    } else if (identity.startsWith("r:")
               && QString::fromLatin1(identity.mid(2)) == QString::fromStdString(shared_->settings.serverId)) {
        key = QByteArray::fromBase64(QByteArray::fromStdString(shared_->settings.replicaKey));
        socket->setProperty("vtStandby", true);
    } else if (identity.startsWith("t:")) {
        const QString id = QString::fromLatin1(identity.mid(2));
        if (const core::TerminalConfig *t = shared_->settings.pairedTerminal(id.toStdString())) {
            key = QByteArray::fromBase64(QByteArray::fromStdString(t->key));
            socket->setProperty("vtTerminalId", id);
        }
    }
    // Unknown device or no pairing open: a key nobody has, so the handshake fails.
    auth->setPreSharedKey(key.isEmpty() ? newDeviceKey() : key);
}

int PosServer::terminalCount() const
{
    return int(std::ranges::count_if(connections_, [](const auto &c) { return c->session != nullptr; }));
}

void PosServer::onNewConnection()
{
    while (auto *socket = qobject_cast<QSslSocket *>(server_.nextPendingConnection())) {
        auto c = std::make_unique<Connection>();
        c->socket = socket;
        c->terminalId = socket->property("vtTerminalId").toString();
        c->pairingCode = socket->property("vtPairingCode").toString();
        c->standby = socket->property("vtStandby").toBool();
        c->since = QDateTime::currentMSecsSinceEpoch();
        c->channel = std::make_unique<LineChannel>(socket);
        Connection *raw = c.get();
        connections_.push_back(std::move(c));
        connect(socket, &QTcpSocket::readyRead, this, [this, raw] { onReadyRead(raw); });
        connect(socket, &QTcpSocket::disconnected, this, [this, raw] { drop(raw); });
    }
}

void PosServer::onReadyRead(Connection *c)
{
    c->heard = QDateTime::currentMSecsSinceEpoch();
    bool overflow = false;
    const QList<QJsonObject> messages = c->channel->receive(&overflow);
    if (overflow) {
        qCWarning(lcServer) << "message too large; dropping terminal";
        c->socket->abort();
        return;
    }
    for (const QJsonObject &m : messages) {
        handle(c, m);
        // handle() may have dropped the connection.
        if (std::ranges::none_of(connections_, [c](const auto &x) { return x.get() == c; }))
            return;
    }
}

void PosServer::handle(Connection *c, const QJsonObject &m)
{
    const QString type = m.value(u"t").toString();

    if (!c->pairingCode.isEmpty()) {   // a device pairing: nothing else is allowed
        if (type == u"pair")
            pair(c, m);
        else
            c->socket->abort();
        return;
    }

    if (c->standby) {   // the store's standby: its copy, then every change
        if (type == u"hello" && !c->synced && snapshot_) {
            const QByteArray db = snapshot_();
            constexpr qsizetype chunk = 1 << 20;
            const int parts = int((db.size() + chunk - 1) / chunk);
            for (int i = 0; i < parts; ++i)
                c->channel->send({{u"t"_s, u"snap"_s}, {u"i"_s, i}, {u"n"_s, parts},
                                  {u"data"_s, QString::fromLatin1(db.mid(i * chunk, chunk).toBase64())}});
            c->channel->send({{u"t"_s, u"layout"_s}, {u"layout"_s, layouts_->layout().toJson()}});
            c->synced = true;
            qCInfo(lcServer) << "standby connected; sent the database," << db.size() << "bytes";
            emit standbyChanged();
        }
        return;
    }

    if (type == u"pong") {
        c->answersPings = true;
        return;
    }
    if (type == u"hello") {
        if (c->session)
            return;
        // The name the device was paired under, not what it says.
        const core::TerminalConfig *paired = shared_->settings.pairedTerminal(c->terminalId.toStdString());
        if (!paired) {
            c->socket->abort();
            return;
        }
        // Set up as a self-order kiosk or a time clock (vtmodern-setup): taken
        // when Manager -> Terminals has no screen for it yet.
        const std::string wanted = m.value(u"screen").toString().toStdString();
        if (paired->screen.empty() && (wanted == "selfOrder" || wanted == "timeClock"))
            for (core::TerminalConfig &t : shared_->settings.terminals)
                if (t.id == paired->id) {
                    t.screen = wanted;
                    shared_->saveSettings();
                    emit shared_->adminChanged();
                }
        const QString name = QString::fromStdString(paired->name);
        c->build = m.value(u"build").toInt();
        c->version = m.value(u"version").toString();
        c->platform = m.value(u"platform").toString();
        c->session = std::make_unique<app::PosService>(shared_, name);
        app::PosService *s = c->session.get();
        if (paired->screen == "selfOrder")   // Manager -> Terminals: a kiosk for guests
            s->enableSelfOrder();
        // Any change to this session's state is sent out on the next flush
        // (the pieces that signal names).
        for (auto signal : {&app::PosSession::sessionChanged, &app::PosSession::entryChanged,
                            &app::PosSession::qualifierChanged, &app::PosSession::checkChanged,
                            &app::PosSession::openChecksChanged, &app::PosSession::kitchenChanged,
                            &app::PosSession::drawerChanged, &app::PosSession::dayChanged,
                            &app::PosSession::adminChanged}) {
            const quint32 bit = bitOfSignal(QMetaMethod::fromSignal(signal));
            connect(s, signal, this, [this, c, bit] {
                c->dirty |= bit;
                scheduleFlushAll();
            });
        }
        connect(s, &app::PosSession::notice, this, [c](const QString &text) { c->notices << text; });
        connect(s, &app::PosSession::checkClosed, this, [c](qint64 id) {
            c->events << QJsonObject{{u"t"_s, u"event"_s}, {u"e"_s, u"checkClosed"_s}, {u"v"_s, id}};
        });
        c->subscribes = m.value(u"subscribes").toBool();
        c->sent = s->snapshot();
        for (auto it = c->sent.begin(); it != c->sent.end();)
            it = (c->subscribes && onRequest(it.key())) || notFor(c->subscribes, it.key()) ? c->sent.erase(it) : std::next(it);
        c->dirty = 0;
        // What the store runs, and an update for this screen if it has one.
        const AppBuild me = appBuild();
        QJsonObject welcome{{u"t"_s, u"welcome"_s}, {u"protocol"_s, ProtocolVersion},
                            {u"version"_s, me.version}, {u"build"_s, me.number},
                            {u"layout"_s, layouts_->layout().toJson()},
                            {u"state"_s, QJsonObject::fromVariantMap(c->sent)}};
        if (const UpdateFile u = updateFor(c->platform, c->build); !u.path.isEmpty())
            welcome.insert(u"update"_s, QJsonObject{{u"name"_s, QFileInfo(u.path).fileName()}, {u"build"_s, u.build},
                                                    {u"size"_s, QFileInfo(u.path).size()}});
        c->channel->send(welcome);
        qCInfo(lcServer) << "terminal connected:" << name;
        emit terminalsChanged();
        return;
    }
    if (!c->session) {
        c->channel->send({{u"t"_s, u"error"_s}, {u"text"_s, u"say hello first"_s}});
        return;
    }
    if (type == u"want") {
        // A screen showing (or not any more) something sent only on request.
        const QString key = m.value(u"k").toString();
        if (!onRequest(key))
            return;
        c->sent.remove(key);
        if (m.value(u"on").toBool()) {
            c->wanted.insert(key);
            c->dirty = ~0u;
            flush(c);
        } else {
            c->wanted.remove(key);
        }
        return;
    }
    if (type == u"getUpdate") {
        sendUpdate(c);
        return;
    }

    if (type == u"call") {
        const qint64 id = m.value(u"id").toInteger();
        QVariant result;
        c->session->invoke(m.value(u"m").toString(), m.value(u"a").toArray().toVariantList(),
                           [&result](const QVariant &r) { result = r; });
        // State (and events) before the reply (nothing changes meanwhile: shared parts made once).
        shared_->beginMemo();
        inRound_ = true;
        flush(c);
        inRound_ = false;
        roundJson_.clear();
        roundSame_.clear();
        shared_->endMemo();
        c->channel->send({{u"t"_s, u"reply"_s}, {u"id"_s, id}, {u"r"_s, QJsonValue::fromVariant(result)}});
        for (const QString &n : std::exchange(c->notices, {}))
            c->channel->send({{u"t"_s, u"notice"_s}, {u"text"_s, n}});
        scheduleFlushAll();
        return;
    }

    if (type == u"saveLayout") {
        const qint64 id = m.value(u"id").toInteger();
        QStringList errors;
        QString error;
        bool ok = false;
        if (!c->session->can(u"layout.edit"_s)) {
            error = tr("%1 may not edit pages.").arg(c->session->userName());
        } else if (auto layout = layout::Layout::fromJson(m.value(u"layout").toObject(), &errors)) {
            ok = layouts_->save(*layout, &error, c);
        } else {
            error = errors.join(u'\n');
        }
        c->channel->send({{u"t"_s, u"reply"_s}, {u"id"_s, id},
                          {u"r"_s, QJsonObject{{u"ok"_s, ok}, {u"error"_s, error}}}});
        return;
    }
}

void PosServer::pair(Connection *c, const QJsonObject &m)
{
    // The code must still be the open one: used once, and not after it expired.
    const app::PosShared::Pairing *open = shared_->activePairing();
    const QString name = m.value(u"name").toString().trimmed();
    const bool asStandby = m.value(u"role").toString() == u"standby";
    if (open && open->code == c->pairingCode && asStandby && !open->standby) {
        // A standby gets a full copy of the data: only with a code opened for it.
        c->channel->send({{u"t"_s, u"error"_s},
                          {u"text"_s, tr("That code is for screens. For a standby computer: Manager -> Terminals -> Pair a Standby Computer.")}});
        c->socket->disconnectFromHost();
        return;
    }
    if (!open || open->code != c->pairingCode || name.isEmpty()) {
        c->channel->send({{u"t"_s, u"error"_s},
                          {u"text"_s, name.isEmpty() ? tr("The terminal needs a name.")
                                                     : tr("That pairing code is no longer open.")}});
        c->socket->disconnectFromHost();
        return;
    }
    // A device paired under an existing terminal's name takes its place (a
    // replaced tablet keeps that terminal's printer and drawer settings).
    auto &list = shared_->settings.terminals;
    auto it = std::ranges::find_if(list, [&](const core::TerminalConfig &t) { return t.name == name.toStdString(); });
    if (it == list.end()) {
        list.push_back({});
        it = list.end() - 1;
    }
    it->name = name.toStdString();
    it->id = newDeviceId().toStdString();
    const QByteArray key = newDeviceKey();
    it->key = key.toBase64().toStdString();
    it->pairedAt = shared_->now();
    const QString id = QString::fromStdString(it->id);
    shared_->pairing.reset();   // one device per code
    pairingKeys_.clear();
    shared_->saveSettings();

    QJsonObject paired{{u"t"_s, u"paired"_s}, {u"id"_s, id}, {u"key"_s, QString::fromLatin1(key.toBase64())},
                       {u"name"_s, name}, {u"serverId"_s, QString::fromStdString(shared_->settings.serverId)},
                       {u"serverName"_s, QString::fromStdString(shared_->settings.storeName)}};
    if (asStandby)   // the store's standby server
        paired.insert(u"replicaKey"_s, QString::fromStdString(shared_->settings.replicaKey));
    c->channel->send(paired);
    c->socket->disconnectFromHost();
    qCInfo(lcServer).noquote() << "paired a new device:" << name;
}

void PosServer::replicate(const QJsonObject &op)
{
    for (auto &c : connections_)
        if (c->standby && c->synced)
            c->channel->send(op);
}

int PosServer::standbyCount() const
{
    return int(std::ranges::count_if(connections_, [](const auto &c) { return c->standby && c->synced; }));
}

QVariantList PosServer::connections() const
{
    QVariantList out;
    for (const auto &c : connections_) {
        if (!c->session && !c->standby)
            continue;
        const int mine = appBuild().number;
        out.append(QVariantMap{{u"name"_s, c->standby ? tr("Standby server") : c->session->terminalName()},
                               {u"address"_s, c->socket->peerAddress().toString().remove(u"::ffff:"_s)},
                               {u"since"_s, c->since}, {u"standby"_s, c->standby},
                               {u"user"_s, c->session ? c->session->userName() : QString()},
                               {u"build"_s, c->build}, {u"version"_s, c->version}, {u"platform"_s, c->platform},
                               {u"behind"_s, !c->standby && mine > 0 && c->build < mine},
                               {u"ahead"_s, !c->standby && mine > 0 && c->build > mine},
                               {u"hasUpdate"_s, !c->standby && !updateFor(c->platform, c->build).path.isEmpty()}});
    }
    return out;
}

// --- updates for the screens ---------------------------------------------------------

namespace {

// What a file is for, from its name: the app for a tablet's processor, or a
// package for a Linux screen's ("" if neither).
QString platformOf(const QString &name)
{
    const QString n = name.toLower();
    if (n.endsWith(u".apk"_s)) {
        if (n.contains(u"arm64-v8a"_s)) return u"android-arm64"_s;
        if (n.contains(u"armeabi-v7a"_s)) return u"android-arm"_s;
        if (n.contains(u"x86_64"_s)) return u"android-x86_64"_s;
        return {};
    }
    if (n.endsWith(u".deb"_s) || n.endsWith(u".rpm"_s)) {
        if (n.contains(u"x86_64"_s) || n.contains(u"amd64"_s)) return u"linux-x86_64"_s;
        if (n.contains(u"aarch64"_s) || n.contains(u"arm64"_s)) return u"linux-arm64"_s;
    }
    return {};
}

} // namespace

QList<PosServer::UpdateFile> PosServer::updateFiles() const
{
    QList<UpdateFile> out;
    if (updatesDir_.isEmpty())
        return out;
    static const QRegularExpression buildIn(uR"((?:^|[-_.])b(\d+)(?:[-_.]|$))"_s);
    for (const QFileInfo &f : QDir(updatesDir_).entryInfoList(QDir::Files, QDir::Name)) {
        const auto m = buildIn.match(f.completeBaseName());
        const QString platform = platformOf(f.fileName());
        if (m.hasMatch() && !platform.isEmpty())
            out.append({f.absoluteFilePath(), m.captured(1).toInt(), platform});
    }
    std::ranges::sort(out, [](const UpdateFile &a, const UpdateFile &b) { return a.build > b.build; });
    return out;
}

PosServer::UpdateFile PosServer::updateFor(const QString &platform, int build) const
{
    for (const UpdateFile &u : updateFiles())
        if (u.platform == platform && u.build > build)
            return u;   // newest first
    return {};
}

// The update for this screen, in parts (like the standby's copy), the last
// with its fingerprint. Managers only: installing it is theirs to decide.
void PosServer::sendUpdate(Connection *c)
{
    const auto refuse = [c](const QString &why) { c->channel->send({{u"t"_s, u"updateError"_s}, {u"text"_s, why}}); };
    if (!c->session->can(QString::fromLatin1(core::perm::Manager)))
        return refuse(tr("Updating a screen needs a manager."));
    const UpdateFile u = updateFor(c->platform, c->build);
    if (u.path.isEmpty())
        return refuse(tr("The store has no newer version for this screen."));
    QFile f(u.path);
    if (!f.open(QIODevice::ReadOnly))
        return refuse(tr("Can't read %1: %2").arg(QFileInfo(u.path).fileName(), f.errorString()));
    const QByteArray data = f.readAll();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    constexpr qsizetype chunk = 1 << 20;
    const int parts = std::max(1, int((data.size() + chunk - 1) / chunk));
    for (int i = 0; i < parts; ++i) {
        QJsonObject part{{u"t"_s, u"updatePart"_s}, {u"i"_s, i}, {u"n"_s, parts},
                         {u"data"_s, QString::fromLatin1(data.mid(i * chunk, chunk).toBase64())}};
        if (i == parts - 1)
            part.insert(u"done"_s, QJsonObject{{u"name"_s, QFileInfo(u.path).fileName()}, {u"build"_s, u.build},
                                               {u"size"_s, qint64(data.size())}, {u"sha256"_s, hash}});
        c->channel->send(part);
    }
    qCInfo(lcServer) << "sent" << QFileInfo(u.path).fileName() << "to" << c->session->terminalName();
}

void PosServer::dropRevoked()
{
    // abort() can drop the connection at once: pick them out first.
    QList<QPointer<QSslSocket>> revoked;
    for (auto &c : connections_) {
        if (!c->terminalId.isEmpty() && !shared_->settings.pairedTerminal(c->terminalId.toStdString())) {
            qCInfo(lcServer).noquote() << "device unpaired; disconnecting"
                                       << (c->session ? c->session->terminalName() : c->terminalId);
            revoked << c->socket;
        }
    }
    for (const QPointer<QSslSocket> &s : revoked) {
        if (s)
            s->abort();
    }
}

void PosServer::flush(Connection *c)
{
    if (!c->session)
        return;
    // The pieces that may have changed, made again and compared.
    const quint32 dirty = std::exchange(c->dirty, 0) | kAlways;
    app::PosService *s = c->session.get();
    const i18n::Scope language([s] { return s->language(); });
    QByteArray changed;
    for (const StatePiece &p : statePieces()) {
        if (!(p.bit & dirty))
            continue;
        if ((c->subscribes && onRequest(p.key) && !c->wanted.contains(p.key)) || notFor(c->subscribes, p.key))
            continue;   // not shown there, or not this way
        const QVariant value = p.property.read(s);
        const void *id = inRound_ ? identity(value) : nullptr;
        auto it = c->sent.find(p.key);
        if (it != c->sent.end()) {
            const void *was = id ? identity(*it) : nullptr;
            bool same;
            if (id && was == id) {
                same = true;   // the very same data
            } else if (id && was) {
                const auto pair = qMakePair(was, id);   // compared once for every screen that had it
                const auto known = roundSame_.constFind(pair);
                same = known != roundSame_.cend() ? *known : *roundSame_.insert(pair, *it == value);
            } else {
                same = *it == value;
            }
            if (same)
                continue;
        }
        QByteArray text;
        if (id) {
            auto cached = roundJson_.constFind(id);
            text = cached != roundJson_.cend() ? *cached : *roundJson_.insert(id, jsonText(value));
        } else {
            text = jsonText(value);
        }
        changed += (changed.isEmpty() ? QByteArray() : QByteArray(",")) + '"' + p.key.toLatin1() + "\":" + text;
        c->sent.insert(p.key, value);
    }
    if (!changed.isEmpty())
        c->channel->sendRaw("{\"t\":\"state\",\"set\":{" + changed + "}}");
    for (const QJsonObject &e : std::exchange(c->events, {}))
        c->channel->send(e);
}

void PosServer::scheduleFlushAll()
{
    if (!flushTimer_.isActive())
        flushTimer_.start();
}

void PosServer::drop(Connection *c)
{
    const auto it = std::ranges::find_if(connections_, [c](const auto &x) { return x.get() == c; });
    if (it == connections_.end())
        return;
    if (c->session)
        qCInfo(lcServer) << "terminal disconnected:" << c->session->terminalName();
    if (c->standby && c->synced) {
        qCWarning(lcServer) << "the standby server disconnected";
        QMetaObject::invokeMethod(this, &PosServer::standbyChanged, Qt::QueuedConnection);
    }
    c->socket->deleteLater();
    // Destroying the session releases the check it had open; tell the rest.
    const bool hadSession = c->session != nullptr;
    connections_.erase(it);
    if (hadSession)
        emit shared_->checksChanged();
    emit terminalsChanged();
    scheduleFlushAll();
}

} // namespace vt::net
