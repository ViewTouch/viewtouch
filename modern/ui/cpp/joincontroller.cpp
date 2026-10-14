#include "joincontroller.hh"

#include <QHostAddress>

using namespace Qt::StringLiterals;

namespace {

// "10.0.0.5", "10.0.0.5:7720", "server.local" -> host and port.
std::pair<QString, quint16> splitAddress(const QString &address)
{
    QString host = address.trimmed();
    quint16 port = vt::net::DefaultPort;
    const qsizetype colon = host.lastIndexOf(u':');
    if (colon > 0 && host.count(u':') == 1) {
        bool ok = false;
        const uint p = host.mid(colon + 1).toUInt(&ok);
        if (ok && p > 0 && p < 65536) {
            port = quint16(p);
            host = host.left(colon);
        }
    }
    return {host, port};
}

} // namespace

JoinController::JoinController(QString suggestedName, QString address, QString problem, QObject *parent)
    : QObject(parent)
    , suggestedName_(std::move(suggestedName))
    , address_(std::move(address))
    , problem_(std::move(problem))
{
    connect(&finder_, &vt::net::ServerFinder::serversChanged, this, &JoinController::serversChanged);
    connect(&pairer_, &vt::net::Pairer::finished, this,
            [this](bool ok, const vt::net::Credentials &c, const QString &error) {
        emit busyChanged();
        if (!ok) {
            setError(error);
            return;
        }
        credentials_ = c;
        setError({});
        emit joined();
    });
    searchTimer_.setInterval(3000);
    connect(&searchTimer_, &QTimer::timeout, this, &JoinController::search);
    searchTimer_.start();
    search();
}

QVariantList JoinController::servers() const
{
    QVariantList out;
    for (const vt::net::FoundServer &s : finder_.servers()) {
        out.append(QVariantMap{{u"id"_s, s.id}, {u"name"_s, s.name}, {u"machine"_s, s.machine},
                               {u"host"_s, s.host}, {u"port"_s, s.port},
                               {u"address"_s, s.port == vt::net::DefaultPort ? s.host : u"%1:%2"_s.arg(s.host).arg(s.port)}});
    }
    return out;
}

void JoinController::search()
{
    finder_.search();
    // A server typed on the command line may be on another network.
    if (!address_.isEmpty()) {
        const auto [host, port] = splitAddress(address_);
        if (const QHostAddress ip(host); !ip.isNull())
            finder_.probe(ip, port);
    }
}

void JoinController::join(const QString &address, const QString &code, const QString &name)
{
    const auto [host, port] = splitAddress(address);
    if (host.isEmpty()) {
        setError(tr("Choose a store above, or type the server's address."));
        return;
    }
    setError({});
    pairer_.start(host, port, code, name);
    emit busyChanged();
}

void JoinController::setError(const QString &e)
{
    if (error_ == e)
        return;
    error_ = e;
    emit errorChanged();
}
