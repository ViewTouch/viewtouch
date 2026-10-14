#pragma once

#include "net/protocol.hh"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QUdpSocket>

#include <functional>

// Finding the server on the local network. A terminal broadcasts
// "VTM-DISCOVER 1" to UDP port 7719; each server answers with who it is:
//   {t: "viewtouch", id, name (the store), machine, port, protocol}
// The id stays the same when the server's address changes, so a paired
// terminal finds its server again.
namespace vt::net {

inline constexpr quint16 DiscoveryPort = DefaultPort;   // UDP, beside the TCP port
inline constexpr QByteArrayView DiscoveryProbe = "VTM-DISCOVER 1";

struct FoundServer {
    QString id;
    QString name;      // the store
    QString machine;   // the server's computer name
    QString host;      // the address it answered from
    quint16 port = DefaultPort;
    QString role = QStringLiteral("main");   // "main", or "standby" (a copy, ready to take over)
    int term = 0;   // goes up each time a standby takes over: the newest main wins
};

// Server side: answers probes.
class DiscoveryResponder : public QObject {
    Q_OBJECT

public:
    // `describe` gives the current {id, name}; `tcpPort` is where terminals connect.
    DiscoveryResponder(std::function<std::pair<QString, QString>()> describe, quint16 tcpPort,
                       QObject *parent = nullptr);

    bool listen(quint16 udpPort = DiscoveryPort);
    // "main" (serves the store) or "standby".
    void setRole(const QString &role) { role_ = role; }
    void setTerm(int term) { term_ = term; }
    quint16 port() const { return socket_.localPort(); }
    QString errorString() const { return socket_.errorString(); }

private:
    void onReadyRead();

    std::function<std::pair<QString, QString>()> describe_;
    quint16 tcpPort_;
    QString role_ = QStringLiteral("main");
    int term_ = 0;
    QUdpSocket socket_;
};

// Terminal side: asks, and collects the answers.
class ServerFinder : public QObject {
    Q_OBJECT

public:
    explicit ServerFinder(QObject *parent = nullptr);

    // Broadcast on every network this device is on.
    void search(quint16 udpPort = DiscoveryPort);
    // Ask one address (typed in, or a server known to be there).
    void probe(const QHostAddress &address, quint16 udpPort = DiscoveryPort);
    void clear();
    const QList<FoundServer> &servers() const { return servers_; }

signals:
    void found(const vt::net::FoundServer &server);
    void serversChanged();

private:
    void onReadyRead();

    QUdpSocket socket_;
    QList<FoundServer> servers_;
};

} // namespace vt::net
