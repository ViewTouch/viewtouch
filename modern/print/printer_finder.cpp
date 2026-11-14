#include "print/printer_finder.hh"

#include "app/pos_service.hh"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QMutex>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QTimer>

#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

namespace vt::print {

namespace {

QMutex testMutex;
QList<QHostAddress> testHosts;
int testPort = 0;

bool privateV4(const QHostAddress &a)
{
    if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback())
        return false;
    const quint32 ip = a.toIPv4Address();
    return (ip >> 24) == 10 || (ip >> 20) == ((172u << 4) | 1u) || (ip >> 16) == ((192u << 8) | 168u);
}

} // namespace

QList<QHostAddress> nearbyHosts()
{
    QList<QHostAddress> out;
    QList<quint32> nets, own;
    for (const QNetworkInterface &i : QNetworkInterface::allInterfaces()) {
        const auto f = i.flags();
        if (!(f & QNetworkInterface::IsUp) || !(f & QNetworkInterface::IsRunning) || (f & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &e : i.addressEntries()) {
            if (!privateV4(e.ip()))
                continue;
            own << e.ip().toIPv4Address();
            const quint32 net = e.ip().toIPv4Address() & 0xFFFFFF00u;
            if (!nets.contains(net) && nets.size() < 4)
                nets << net;
        }
    }
    for (const quint32 net : nets)
        for (quint32 h = 1; h < 255; ++h)
            if (!own.contains(net | h))
                out << QHostAddress(net | h);
    return out;
}

QList<FoundPrinter> findPrinters(const QList<QHostAddress> &hosts, int port, int connectMs, int answerMs)
{
    QList<FoundPrinter> found;
    if (hosts.isEmpty())
        return found;
    QEventLoop loop;
    std::vector<std::unique_ptr<QTcpSocket>> sockets;
    int pending = int(hosts.size());
    QList<QTcpSocket *> open;
    for (const QHostAddress &h : hosts) {
        auto s = std::make_unique<QTcpSocket>();
        QTcpSocket *raw = s.get();
        QObject::connect(raw, &QTcpSocket::connected, &loop, [&, raw] {
            open << raw;
            if (--pending == 0)
                loop.quit();
        });
        QObject::connect(raw, &QTcpSocket::errorOccurred, &loop, [&] {
            if (--pending == 0)
                loop.quit();
        });
        raw->connectToHost(h, quint16(port));
        sockets.push_back(std::move(s));
    }
    QTimer::singleShot(connectMs, &loop, &QEventLoop::quit);
    if (pending > 0)
        loop.exec();

    // Each that took the connection: its status (one byte back from an
    // ESC/POS printer; nothing from anything else on that port).
    QHash<QTcpSocket *, bool> answered;
    int waiting = int(open.size());
    QEventLoop answers;
    for (QTcpSocket *s : open) {
        QObject::connect(s, &QTcpSocket::readyRead, &answers, [&, s] {
            if (!answered.value(s) && s->bytesAvailable() > 0) {
                answered[s] = true;
                if (--waiting == 0)
                    answers.quit();
            }
        });
        s->write(QByteArray("\x10\x04\x01", 3));
    }
    if (waiting > 0) {
        QTimer::singleShot(answerMs, &answers, &QEventLoop::quit);
        answers.exec();
    }
    for (QTcpSocket *s : open)
        found.append({s->peerAddress().toString(), port, answered.value(s)});
    for (auto &s : sockets)
        s->abort();
    // In address order (192.168.1.9 before 192.168.1.10).
    std::ranges::sort(found, [](const FoundPrinter &a, const FoundPrinter &b) {
        return QHostAddress(a.host).toIPv4Address() < QHostAddress(b.host).toIPv4Address();
    });
    return found;
}

bool printWhoAmI(const QString &host, int port, const QString &title, const QString &line, QString *error)
{
    QTcpSocket s;
    s.connectToHost(host, quint16(port));
    if (!s.waitForConnected(3000)) {
        if (error)
            *error = s.errorString();
        return false;
    }
    // (Each command with its length: some end in a zero byte.)
    const QByteArray normal("\x1d\x21\x00", 3), big("\x1d\x21\x11", 3);
    QByteArray page;
    page += QByteArray("\x1b\x40", 2);           // start over
    page += QByteArray("\x1b\x61\x01", 3);       // centered
    page += normal + title.toUtf8() + "\n\n";
    page += big + line.toUtf8() + "\n";        // twice as tall and wide
    page += normal + "\n\n\n\n";
    page += QByteArray("\x1d\x56\x42\x00", 4);   // feed and cut
    s.write(page);
    const bool ok = s.waitForBytesWritten(3000);
    if (!ok && error)
        *error = s.errorString();
    s.disconnectFromHost();
    if (s.state() != QAbstractSocket::UnconnectedState)
        s.waitForDisconnected(1000);
    return ok;
}

void installFinder(app::PosShared &shared)
{
    shared.searchPrinters = [] {
        const QList<QHostAddress> forTests = searchHostsForTests();
        const int port = forTests.isEmpty() ? 9100 : searchPortForTests();
        QVariantList out;
        for (const FoundPrinter &f : findPrinters(forTests.isEmpty() ? nearbyHosts() : forTests, port))
            out.append(QVariantMap{{u"host"_s, f.host}, {u"port"_s, f.port}, {u"answered"_s, f.answered}});
        return out;
    };
    shared.printWhoAmI = [](const QString &host, int port, const QString &title, const QString &line) {
        QString error;
        return printWhoAmI(host, port, title, line, &error) ? QString() : (error.isEmpty() ? u"?"_s : error);
    };
}

void setSearchForTests(const QList<QHostAddress> &hosts, int port)
{
    const QMutexLocker lock(&testMutex);
    testHosts = hosts;
    testPort = port;
}

QList<QHostAddress> searchHostsForTests()
{
    const QMutexLocker lock(&testMutex);
    return testHosts;
}

int searchPortForTests()
{
    const QMutexLocker lock(&testMutex);
    return testPort;
}

} // namespace vt::print
