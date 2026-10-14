#include "print/printer_monitor.hh"

#include <QHash>
#include <QSet>
#include <QMutex>
#include <QTcpSocket>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace vt::print {

namespace {

// A status byte: bits 1 and 4 set, 0 and 7 clear.
bool validStatus(unsigned char b)
{
    return (b & 0x93) == 0x12;
}

} // namespace

QString problemFromStatus(const QByteArray &offlineCause, const QByteArray &paperSensor)
{
    const bool haveOffline = !offlineCause.isEmpty() && validStatus(static_cast<unsigned char>(offlineCause[0]));
    const bool havePaper = !paperSensor.isEmpty() && validStatus(static_cast<unsigned char>(paperSensor[0]));
    const unsigned char off = haveOffline ? static_cast<unsigned char>(offlineCause[0]) : 0;
    const unsigned char paper = havePaper ? static_cast<unsigned char>(paperSensor[0]) : 0;
    if (off & 0x04)
        return u"coverOpen"_s;
    if ((off & 0x20) || (paper & 0x60))
        return u"paperOut"_s;
    if (off & 0x40)
        return u"error"_s;
    if (paper & 0x0C)
        return u"paperLow"_s;
    return {};
}

class PrinterMonitor::Worker : public QObject {
public:
    Worker()
    {
        timer_.setParent(this);
        connect(&timer_, &QTimer::timeout, this, &Worker::checkAll);
    }

    void start(int intervalMs)
    {
        timer_.start(intervalMs);
    }

    void setPrinters(std::vector<core::PrinterConfig> printers)
    {
        printers_.clear();
        for (core::PrinterConfig &p : printers)
            if (p.type == "network" && p.effectiveFormat() == "escpos" && p.watch && !p.host.empty()) {
                if (p.reportsStatus)
                    answered_.insert(QString::fromStdString(p.id));   // known from before
                printers_.push_back(std::move(p));
            }
        // Printers no longer watched: whatever they had is cleared.
        for (auto it = problems_.begin(); it != problems_.end();) {
            const bool still = std::ranges::any_of(printers_, [&](const core::PrinterConfig &p) {
                return QString::fromStdString(p.id) == it.key();
            });
            if (still) {
                ++it;
            } else {
                if (!it.value().isEmpty())
                    emit owner->statusChanged(it.key(), QString());
                failures_.remove(it.key());
                answered_.remove(it.key());
                silent_.remove(it.key());
                it = problems_.erase(it);
            }
        }
        checkAll();
    }

    void checkAll()
    {
        for (const core::PrinterConfig &p : printers_)
            check(p);
    }

    PrinterMonitor *owner = nullptr;
    std::function<bool(const QString &, int)> busy;
    int timeoutMs = 1500;
    QTimer timer_;

private:
    void check(const core::PrinterConfig &p)
    {
        const QString id = QString::fromStdString(p.id);
        const QString host = QString::fromStdString(p.host);
        if (busy && busy(host, p.port))
            return;   // a ticket is on its way: ask next time
        QTcpSocket s;
        s.connectToHost(host, quint16(p.port));
        QString problem;
        if (!s.waitForConnected(timeoutMs)) {
            // Not answering twice in a row: offline (one miss can be a busy printer).
            // Many Epsons take no connections once out of paper or in error:
            // if its paper was low (or out) just before, that's the likely why.
            const QString before = problems_.value(id);
            const bool paper = before == u"paperLow" || before == u"paperOut" || before == u"offlinePaper";
            problem = ++failures_[id] >= 2 ? (paper ? u"offlinePaper"_s : u"offline"_s) : before;
        } else {
            failures_[id] = 0;
            const QByteArray offline = ask(s, 2);
            const QByteArray paper = ask(s, 4);
            problem = problemFromStatus(offline, paper);
            // Connected but saying nothing, from a printer that has told its
            // status before: stuck (twice in a row; once can be a busy one).
            // Printers that never say are left alone.
            // An Epson that has printed its roll to the end does this too: after
            // paper low, that's the likely why.
            if (offline.isEmpty() && paper.isEmpty()) {
                const QString before = problems_.value(id);
                const bool wasPaper = before == u"paperLow" || before == u"paperOut" || before == u"offlinePaper";
                if (answered_.contains(id))
                    problem = ++silent_[id] >= 2 ? (wasPaper ? u"offlinePaper"_s : u"silent"_s) : before;
            } else {
                if (!answered_.contains(id)) {
                    answered_.insert(id);
                    if (!p.reportsStatus)
                        emit owner->answersStatus(id);
                }
                silent_[id] = 0;
            }
            s.disconnectFromHost();
            if (s.state() != QAbstractSocket::UnconnectedState)
                s.waitForDisconnected(timeoutMs);
        }
        if (problems_.value(id) != problem || !problems_.contains(id)) {
            const bool changed = problems_.value(id) != problem;
            problems_[id] = problem;
            if (changed)
                emit owner->statusChanged(id, problem);
        }
    }

    // DLE EOT n: one byte back (none from printers that don't say).
    QByteArray ask(QTcpSocket &s, char n)
    {
        s.write(QByteArray("\x10\x04", 2) + n);
        if (!s.waitForBytesWritten(timeoutMs))
            return {};
        QByteArray answer;
        while (answer.isEmpty() && s.waitForReadyRead(timeoutMs))
            answer += s.read(1);
        return answer.left(1);
    }

    std::vector<core::PrinterConfig> printers_;
    QHash<QString, QString> problems_;   // printer id -> its problem
    QSet<QString> answered_;             // printers that have told their status
    QHash<QString, int> silent_;         // ... and how many times since they haven't
    QHash<QString, int> failures_;       // printer id -> connections missed in a row
};

PrinterMonitor::PrinterMonitor(QObject *parent)
    : QObject(parent)
    , worker_(new Worker)
{
    worker_->owner = this;
    thread_.setObjectName(u"vt-printer-status"_s);
    worker_->moveToThread(&thread_);
    thread_.start();
    QMetaObject::invokeMethod(worker_, [w = worker_] { w->start(20'000); }, Qt::QueuedConnection);
}

PrinterMonitor::~PrinterMonitor()
{
    QMetaObject::invokeMethod(worker_, [w = worker_] { w->timer_.stop(); }, Qt::BlockingQueuedConnection);
    thread_.quit();
    thread_.wait();
    delete worker_;
}

void PrinterMonitor::setPrinters(const std::vector<core::PrinterConfig> &printers)
{
    QMetaObject::invokeMethod(worker_, [w = worker_, printers] { w->setPrinters(printers); }, Qt::QueuedConnection);
}

void PrinterMonitor::setBusy(std::function<bool(const QString &, int)> busy)
{
    QMetaObject::invokeMethod(worker_, [w = worker_, busy = std::move(busy)] { w->busy = busy; },
                              Qt::BlockingQueuedConnection);
}

void PrinterMonitor::setTimings(int intervalMs, int timeoutMs)
{
    QMetaObject::invokeMethod(worker_, [w = worker_, intervalMs, timeoutMs] {
        w->timeoutMs = timeoutMs;
        w->start(intervalMs);
    }, Qt::BlockingQueuedConnection);
}

void PrinterMonitor::checkNow()
{
    QMetaObject::invokeMethod(worker_, [w = worker_] { w->checkAll(); }, Qt::QueuedConnection);
}

} // namespace vt::print
