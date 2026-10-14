#pragma once

#include "net/protocol.hh"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QString>

#include <memory>
#include <optional>

// Terminals talk to the server over TLS 1.2 with a pre-shared key
// (ECDHE-PSK-CHACHA20-POLY1305): encrypted, forward secret, and both sides
// prove they hold the key, so no certificates are needed.
//
// Pairing: a manager starts it in Manager -> Terminals and gets a
// 10-character code. The new device connects with the identity "pair" and a
// key stretched from the code (PBKDF2), sends its name, and receives its own
// random 256-bit key and id. From then on it connects as "t:<id>" with that
// key. Removing the terminal in Manager -> Terminals revokes the key.
namespace vt::net {

inline constexpr const char *PairingIdentity = "pair";

QSslConfiguration tlsConfiguration();
// Whether this Qt/OpenSSL can make these connections; `why` says what's missing.
bool tlsAvailable(QString *why = nullptr);

// "k7qm4 xhp2w" -> "K7QM4-XHP2W". Mistyped look-alikes are fixed (O -> 0,
// I and L -> 1). Empty when it can't be a pairing code.
QString normalizePairingCode(const QString &typed);
// The TLS key for pairing with `code` (normalized).
QByteArray pairingKey(const QString &code);
QByteArray newDeviceKey();
QString newDeviceId();
QByteArray deviceIdentity(const QString &terminalId);

// What a paired terminal keeps: which server, and its id and key there.
struct Credentials {
    QString serverId;
    QString serverName;
    QString host;
    quint16 port = DefaultPort;
    QString terminalId;
    QString terminalName;
    QByteArray key;
    QByteArray replicaKey;   // a standby server: the store's key between servers

    bool valid() const { return !host.isEmpty() && !terminalId.isEmpty() && key.size() >= 32; }
    QJsonObject toJson() const;
    static Credentials fromJson(const QJsonObject &o);
    static std::optional<Credentials> load(const QString &file);
    // Written readable by the owner only.
    bool save(const QString &file, QString *error = nullptr) const;
};

// Pairs this device with the server at host:port using a code from
// Manager -> Terminals. `finished` reports the new credentials or why not.
class Pairer : public QObject {
    Q_OBJECT

public:
    explicit Pairer(QObject *parent = nullptr);
    ~Pairer() override;

    void start(const QString &host, quint16 port, const QString &code, const QString &terminalName);
    // Pair as the store's standby server (gets the store's server key too).
    void setStandby(bool standby) { standby_ = standby; }
    bool busy() const { return socket_ != nullptr; }

signals:
    void finished(bool ok, const vt::net::Credentials &credentials, const QString &error);

private:
    void done(bool ok, const Credentials &c, const QString &error);

    std::unique_ptr<QSslSocket> socket_;
    std::unique_ptr<LineChannel> channel_;
    QString host_;
    quint16 port_ = 0;
    QString name_;
    bool encrypted_ = false;
    bool standby_ = false;
};

} // namespace vt::net
