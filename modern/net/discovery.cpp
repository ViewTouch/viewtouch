#include "net/discovery.hh"

#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSysInfo>

#include <algorithm>
#include <cctype>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcDiscovery, "vt.discovery")

namespace vt::net {

QByteArray discoverySignature(const QByteArray &key, const QByteArray &nonce, const QString &id, const QString &role,
                              int term, int port)
{
    const QByteArray what = nonce + '|' + id.toUtf8() + '|' + role.toUtf8() + '|' + QByteArray::number(term) + '|'
                            + QByteArray::number(port);
    return QMessageAuthenticationCode::hash(what, key, QCryptographicHash::Sha256).toHex();
}

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
        // "VTM-DISCOVER 1", or with a nonce to sign: "VTM-DISCOVER 1 <32 hex>".
        const QByteArray data = probe.data().trimmed();
        QByteArray nonce;
        if (data.size() == DiscoveryProbe.size() + 33 && data.startsWith(DiscoveryProbe) && data[DiscoveryProbe.size()] == ' ') {
            nonce = data.mid(DiscoveryProbe.size() + 1);
            if (!std::ranges::all_of(nonce, [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); }))
                continue;
        } else if (data != DiscoveryProbe) {
            continue;
        }
        const auto [id, name] = describe_();
        QJsonObject answer{{u"t"_s, u"viewtouch"_s}, {u"id"_s, id}, {u"name"_s, name},
                           {u"machine"_s, QSysInfo::machineHostName()}, {u"port"_s, tcpPort_},
                           {u"protocol"_s, ProtocolVersion}, {u"role"_s, role_}, {u"term"_s, term_}};
        if (const QByteArray key = key_ ? key_() : QByteArray(); !nonce.isEmpty() && !key.isEmpty())
            answer.insert(u"sig"_s, QString::fromLatin1(discoverySignature(key, nonce, id, role_, term_, tcpPort_)));
        socket_.writeDatagram(probe.makeReply(QJsonDocument(answer).toJson(QJsonDocument::Compact)));
    }
}

// The most stores a search lists (a real network has a few).
constexpr qsizetype kMaxFound = 32;

// --- terminal side -----------------------------------------------------------------------

ServerFinder::ServerFinder(QObject *parent)
    : QObject(parent)
{
    socket_.bind(QHostAddress::AnyIPv4, 0);
    connect(&socket_, &QUdpSocket::readyRead, this, &ServerFinder::onReadyRead);
}

void ServerFinder::newNonce()
{
    // Each search its own (an answer recorded and sent again later isn't
    // signed for it); the last few still count (answers come late).
    quint32 words[4];
    QRandomGenerator::system()->fillRange(words);
    nonces_.prepend(QByteArray(reinterpret_cast<const char *>(words), sizeof words).toHex());
    while (nonces_.size() > 3)
        nonces_.removeLast();
}

void ServerFinder::search(quint16 udpPort)
{
    if (key_)
        newNonce();
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
    // With the store's key: a second probe, with a nonce the store signs for.
    if (!key_)
        return;
    if (nonces_.isEmpty())
        newNonce();
    socket_.writeDatagram(DiscoveryProbe.toByteArray() + ' ' + nonces_.front(), address, udpPort);
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
        const int port = o.value(u"port").toInt(DefaultPort);
        if (port < 1 || port > 65535)
            continue;   // not a port: not a ViewTouch answering
        s.port = quint16(port);
        s.role = o.value(u"role").toString(u"main"_s);
        s.term = o.value(u"term").toInt();
        if (const QByteArray key = key_ ? key_() : QByteArray(); !key.isEmpty()) {
            const QByteArray sig = o.value(u"sig").toString().toLatin1();
            for (const QByteArray &nonce : std::as_const(nonces_))
                s.proven = s.proven
                           || (!sig.isEmpty() && sig == discoverySignature(key, nonce, s.id, s.role, s.term, port));
        }
        // The same server can answer on several networks: keep one entry
        // each (a store's main and its standby share the id).
        auto it = std::ranges::find_if(servers_, [&](const FoundServer &x) { return x.id == s.id && x.role == s.role; });
        if (it == servers_.end() && servers_.size() >= kMaxFound)
            continue;   // anyone on the network can answer: a flood of made-up stores stops here
        if (it == servers_.end())
            servers_.append(s);
        else
            *it = s;
        emit found(s);
        emit serversChanged();
    }
}

} // namespace vt::net
