#include "net/discovery.hh"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSysInfo>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcDiscovery, "vt.discovery")

namespace vt::net {

DiscoveryResponder::DiscoveryResponder(std::function<std::pair<QString, QString>()> describe, quint16 tcpPort,
                                       QObject *parent)
    : QObject(parent)
    , describe_(std::move(describe))
    , tcpPort_(tcpPort)
{
    connect(&socket_, &QUdpSocket::readyRead, this, &DiscoveryResponder::onReadyRead);
}

bool DiscoveryResponder::listen(quint16 udpPort)
{
    return socket_.bind(QHostAddress::AnyIPv4, udpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

void DiscoveryResponder::onReadyRead()
{
    while (socket_.hasPendingDatagrams()) {
        const QNetworkDatagram probe = socket_.receiveDatagram(256);
        if (probe.data().trimmed() != DiscoveryProbe)
            continue;
        const auto [id, name] = describe_();
        const QJsonObject answer{{u"t"_s, u"viewtouch"_s}, {u"id"_s, id}, {u"name"_s, name},
                                 {u"machine"_s, QSysInfo::machineHostName()}, {u"port"_s, tcpPort_},
                                 {u"protocol"_s, ProtocolVersion}, {u"role"_s, role_}, {u"term"_s, term_}};
        socket_.writeDatagram(probe.makeReply(QJsonDocument(answer).toJson(QJsonDocument::Compact)));
    }
}

// --- terminal side -----------------------------------------------------------------------

ServerFinder::ServerFinder(QObject *parent)
    : QObject(parent)
{
    socket_.bind(QHostAddress::AnyIPv4, 0);
    connect(&socket_, &QUdpSocket::readyRead, this, &ServerFinder::onReadyRead);
}

void ServerFinder::search(quint16 udpPort)
{
    probe(QHostAddress::Broadcast, udpPort);
    probe(QHostAddress::LocalHost, udpPort);   // a server on this same machine
    // Some networks drop the all-ones broadcast: also each network's own.
    for (const QNetworkInterface &nic : QNetworkInterface::allInterfaces()) {
        const auto flags = nic.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast))
            continue;
        for (const QNetworkAddressEntry &e : nic.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && !e.broadcast().isNull())
                probe(e.broadcast(), udpPort);
        }
    }
}

void ServerFinder::probe(const QHostAddress &address, quint16 udpPort)
{
    if (socket_.writeDatagram(DiscoveryProbe.toByteArray(), address, udpPort) < 0)
        qCDebug(lcDiscovery).noquote() << "probe to" << address.toString() << "failed:" << socket_.errorString();
}

void ServerFinder::clear()
{
    servers_.clear();
    emit serversChanged();
}

void ServerFinder::onReadyRead()
{
    while (socket_.hasPendingDatagrams()) {
        const QNetworkDatagram d = socket_.receiveDatagram(4096);
        const QJsonObject o = QJsonDocument::fromJson(d.data()).object();
        if (o.value(u"t").toString() != u"viewtouch" || o.value(u"id").toString().isEmpty())
            continue;
        FoundServer s;
        s.id = o.value(u"id").toString();
        s.name = o.value(u"name").toString();
        s.machine = o.value(u"machine").toString();
        QHostAddress from = d.senderAddress();
        bool v4 = false;
        const quint32 ip = from.toIPv4Address(&v4);   // "::ffff:10.0.0.5" -> "10.0.0.5"
        if (v4)
            from = QHostAddress(ip);
        s.host = from.toString();
        s.port = quint16(o.value(u"port").toInt(DefaultPort));
        s.role = o.value(u"role").toString(u"main"_s);
        s.term = o.value(u"term").toInt();
        // The same server can answer on several networks: keep one entry
        // each (a store's main and its standby share the id).
        auto it = std::ranges::find_if(servers_, [&](const FoundServer &x) { return x.id == s.id && x.role == s.role; });
        if (it == servers_.end())
            servers_.append(s);
        else
            *it = s;
        emit found(s);
        emit serversChanged();
    }
}

} // namespace vt::net
