#include "net/pairing.hh"

#include <QFile>
#include <QFileDevice>
#include <QJsonDocument>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSslCipher>
#include <QSslPreSharedKeyAuthenticator>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace vt::net {

namespace {

constexpr const char *kCipher = "ECDHE-PSK-CHACHA20-POLY1305";
constexpr int kPairingRounds = 600'000;   // PBKDF2-SHA256, as OWASP recommends
constexpr int kPairTimeoutMs = 15'000;

QByteArray randomBytes(int n)
{
    QByteArray out(n, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(out.data()), n / 4);
    return out;
}

} // namespace

QSslConfiguration tlsConfiguration()
{
    QSslConfiguration c = QSslConfiguration::defaultConfiguration();
    c.setProtocol(QSsl::TlsV1_2);   // Qt offers pre-shared keys up to TLS 1.2
    c.setCiphers(QString::fromLatin1(kCipher));
    c.setPeerVerifyMode(QSslSocket::VerifyNone);   // the key is the proof, not a certificate
    return c;
}

bool tlsAvailable(QString *why)
{
    if (!QSslSocket::supportsSsl()) {
        if (why)
            *why = u"This Qt has no TLS support (OpenSSL)."_s;
        return false;
    }
    if (tlsConfiguration().ciphers().isEmpty()) {
        if (why)
            *why = u"The TLS library lacks %1."_s.arg(QString::fromLatin1(kCipher));
        return false;
    }
    return true;
}

QString normalizePairingCode(const QString &typed)
{
    QString s;
    for (QChar ch : typed.toUpper()) {
        if (ch == u'O')
            ch = u'0';
        else if (ch == u'I' || ch == u'L')
            ch = u'1';
        if (ch.isDigit() || (ch >= u'A' && ch <= u'Z' && ch != u'U'))
            s += ch;
        else if (!ch.isSpace() && ch != u'-')
            return {};
    }
    if (s.size() != 10)
        return {};
    return s.left(5) + u'-' + s.mid(5);
}

QByteArray pairingKey(const QString &code)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, code.toLatin1(),
                                              QByteArrayLiteral("viewtouch-pairing-v1"), kPairingRounds, 32);
}

QByteArray newDeviceKey()
{
    return randomBytes(32);
}

QString newDeviceId()
{
    return QString::fromLatin1(randomBytes(8).toHex());
}

QByteArray deviceIdentity(const QString &terminalId)
{
    return "t:" + terminalId.toLatin1();
}

// --- credentials -------------------------------------------------------------------------

QJsonObject Credentials::toJson() const
{
    return {{u"serverId"_s, serverId}, {u"serverName"_s, serverName}, {u"host"_s, host}, {u"port"_s, port},
            {u"terminalId"_s, terminalId}, {u"terminalName"_s, terminalName},
            {u"key"_s, QString::fromLatin1(key.toBase64())}, {u"replicaKey"_s, QString::fromLatin1(replicaKey.toBase64())}};
}

Credentials Credentials::fromJson(const QJsonObject &o)
{
    Credentials c;
    c.serverId = o.value(u"serverId").toString();
    c.serverName = o.value(u"serverName").toString();
    c.host = o.value(u"host").toString();
    c.port = quint16(o.value(u"port").toInt(DefaultPort));
    c.terminalId = o.value(u"terminalId").toString();
    c.terminalName = o.value(u"terminalName").toString();
    c.key = QByteArray::fromBase64(o.value(u"key").toString().toLatin1());
    c.replicaKey = QByteArray::fromBase64(o.value(u"replicaKey").toString().toLatin1());
    return c;
}

std::optional<Credentials> Credentials::load(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const Credentials c = fromJson(QJsonDocument::fromJson(f.readAll()).object());
    // A terminal's, or a standby server's (the store's server key is enough).
    const bool standby = !c.host.isEmpty() && !c.serverId.isEmpty() && c.replicaKey.size() >= 32;
    return c.valid() || standby ? std::optional(c) : std::nullopt;
}

bool Credentials::save(const QString &file, QString *error) const
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(QJsonDocument(toJson()).toJson());
    if (!f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

// --- pairing a device ----------------------------------------------------------------------

Pairer::Pairer(QObject *parent)
    : QObject(parent)
{
}

Pairer::~Pairer()
{
    if (socket_) {
        socket_->disconnect(this);
        socket_->abort();
    }
}

void Pairer::start(const QString &host, quint16 port, const QString &code, const QString &terminalName)
{
    if (socket_)
        return;
    const QString normalized = normalizePairingCode(code);
    const QString name = terminalName.trimmed();
    QString why;
    if (normalized.isEmpty() || name.isEmpty() || !tlsAvailable(&why)) {
        const QString error = !why.isEmpty()        ? why
                              : normalized.isEmpty() ? tr("That is not a pairing code. It has 10 letters and digits.")
                                                     : tr("Give this terminal a name.");
        QTimer::singleShot(0, this, [this, error] { emit finished(false, {}, error); });
        return;
    }
    host_ = host;
    port_ = port;
    name_ = name;
    encrypted_ = false;
    const QByteArray key = pairingKey(normalized);

    socket_ = std::make_unique<QSslSocket>();
    channel_ = std::make_unique<LineChannel>(socket_.get());
    socket_->setSslConfiguration(tlsConfiguration());
    connect(socket_.get(), &QSslSocket::preSharedKeyAuthenticationRequired, this,
            [key](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity(PairingIdentity);
        a->setPreSharedKey(key);
    });
    connect(socket_.get(), &QSslSocket::encrypted, this, [this] {
        encrypted_ = true;
        QJsonObject hello{{u"t"_s, u"pair"_s}, {u"name"_s, name_}, {u"protocol"_s, ProtocolVersion}};
        if (standby_)
            hello.insert(u"role"_s, u"standby"_s);
        channel_->send(hello);
    });
    connect(socket_.get(), &QSslSocket::readyRead, this, [this] {
        for (const QJsonObject &m : channel_->receive()) {
            const QString type = m.value(u"t").toString();
            if (type == u"paired") {
                Credentials c;
                c.serverId = m.value(u"serverId").toString();
                c.serverName = m.value(u"serverName").toString();
                c.host = host_;
                c.port = port_;
                c.terminalId = m.value(u"id").toString();
                c.terminalName = m.value(u"name").toString(name_);
                c.key = QByteArray::fromBase64(m.value(u"key").toString().toLatin1());
                c.replicaKey = QByteArray::fromBase64(m.value(u"replicaKey").toString().toLatin1());
                const bool ok = c.valid() && (!standby_ || c.replicaKey.size() >= 32);
                done(ok, c, ok ? QString() : tr("The server sent an incomplete answer."));
                return;
            }
            if (type == u"error") {
                done(false, {}, m.value(u"text").toString());
                return;
            }
        }
    });
    connect(socket_.get(), &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
        if (!socket_)
            return;
        const QString where = u"%1:%2"_s.arg(host_).arg(port_);
        QString error;
        switch (e) {
        case QAbstractSocket::SslHandshakeFailedError:
            error = tr("The code is wrong or has expired. Start a new pairing in Manager → Terminals.");
            break;
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::HostNotFoundError:
            error = tr("No ViewTouch server answered at %1.").arg(where);
            break;
        case QAbstractSocket::RemoteHostClosedError:
            // Without encryption the server refused the key: the same as a wrong code.
            error = encrypted_ ? tr("The server closed the connection.")
                               : tr("The code is wrong or has expired. Start a new pairing in Manager → Terminals.");
            break;
        default:
            error = tr("Could not pair with %1: %2").arg(where, socket_->errorString());
        }
        done(false, {}, error);
    });
    QTimer::singleShot(kPairTimeoutMs, this, [this] {
        if (socket_)
            done(false, {}, tr("The server did not answer in time."));
    });
    socket_->connectToHostEncrypted(host, port);
}

void Pairer::done(bool ok, const Credentials &c, const QString &error)
{
    if (!socket_)
        return;
    // Finish with the socket after this signal handler returns.
    QSslSocket *s = socket_.release();
    s->disconnect(this);
    s->disconnectFromHost();
    s->deleteLater();
    channel_.reset();
    emit finished(ok, c, error);
}

} // namespace vt::net
