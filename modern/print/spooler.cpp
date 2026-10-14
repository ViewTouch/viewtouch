#include "print/spooler.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#if QT_CONFIG(process)
#include <QProcess>
#endif
#include <QTcpSocket>
#include <QTimer>

#include <atomic>
#include <deque>

using namespace Qt::StringLiterals;

namespace vt::print {

class PrintSpooler::Worker : public QObject {
public:
    struct Job {
        core::PrinterConfig printer;
        QByteArray data;
        QString description;
        int attempts = 0;
        qint64 notBefore = 0;
    };

    explicit Worker(PrintSpooler *owner) : owner_(owner) {}

    // Owner thread.
    void enqueue(Job job)
    {
        pending_.fetch_add(1);
        hold(job, +1);
        QMetaObject::invokeMethod(this, [this, job = std::move(job)]() mutable {
            queue_.push_back(std::move(job));
            schedule(0);
        }, Qt::QueuedConnection);
    }

    int pending() const { return pending_.load(); }

    bool busy(const QString &host, int port) const
    {
        QMutexLocker lock(&busyMutex_);
        return busy_.value(host + u':' + QString::number(port)) > 0;
    }
    int connectMs = 3000;
    int retryBaseMs = 2000;

private:
    void schedule(int delayMs)
    {
        if (timerArmed_)
            return;
        timerArmed_ = true;
        QTimer::singleShot(delayMs, this, [this] {
            timerArmed_ = false;
            run();
        });
    }

    void run()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::deque<Job> later;
        while (!queue_.empty()) {
            Job job = std::move(queue_.front());
            queue_.pop_front();
            if (job.notBefore > now) {
                later.push_back(std::move(job));
                continue;
            }
            QString error;
            if (deliver(job, &error)) {
                pending_.fetch_sub(1);
                hold(job, -1);
                emit owner_->jobPrinted(QString::fromStdString(job.printer.name), job.description);
                continue;
            }
            if (++job.attempts >= MaxAttempts) {
                pending_.fetch_sub(1);
                hold(job, -1);
                emit owner_->jobFailed(QString::fromStdString(job.printer.name), job.description, error);
                continue;
            }
            job.notBefore = QDateTime::currentMSecsSinceEpoch() + qint64(retryBaseMs) * job.attempts;
            later.push_back(std::move(job));
        }
        queue_ = std::move(later);
        if (!queue_.empty()) {
            qint64 next = queue_.front().notBefore;
            for (const Job &j : queue_)
                next = std::min(next, j.notBefore);
            schedule(int(std::max<qint64>(10, next - QDateTime::currentMSecsSinceEpoch())));
        }
    }

    bool deliver(const Job &job, QString *error)
    {
        const core::PrinterConfig &p = job.printer;
        if (p.type == "none")
            return true;
        if (p.type == "file")
            return toFile(QString::fromStdString(p.path), job.data, error);
        if (p.type == "network")
            return toSocket(QString::fromStdString(p.host), quint16(p.port), job.data, error);
        if (p.type == "cups")
            return toCups(QString::fromStdString(p.path), job.data, error);
        *error = u"unknown printer type '%1'"_s.arg(QString::fromStdString(p.type));
        return false;
    }

    static bool toFile(const QString &path, const QByteArray &data, QString *error)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::Append)) {
            *error = f.errorString();
            return false;
        }
        if (f.write(data) != data.size()) {
            *error = f.errorString();
            return false;
        }
        return true;
    }

    bool toSocket(const QString &host, quint16 port, const QByteArray &data, QString *error)
    {
        if (host.isEmpty()) {
            *error = u"no printer address"_s;
            return false;
        }
        QTcpSocket s;
        s.connectToHost(host, port);
        if (!s.waitForConnected(connectMs)) {
            *error = s.errorString();
            return false;
        }
        s.write(data);
        while (s.bytesToWrite() > 0) {
            if (!s.waitForBytesWritten(connectMs)) {
                *error = s.errorString();
                return false;
            }
        }
        s.disconnectFromHost();
        if (s.state() != QAbstractSocket::UnconnectedState)
            s.waitForDisconnected(connectMs);
        return true;
    }

    bool toCups(const QString &queue, const QByteArray &data, QString *error)
    {
#if !QT_CONFIG(process)
        Q_UNUSED(queue)
        Q_UNUSED(data)
        *error = u"CUPS printing is not available on this system"_s;
        return false;
#else
        QProcess lp;
        QStringList args{u"-o"_s, u"raw"_s};
        if (!queue.isEmpty())
            args << u"-d"_s << queue;
        lp.start(u"lp"_s, args);
        if (!lp.waitForStarted(connectMs)) {
            *error = u"cannot run lp: %1"_s.arg(lp.errorString());
            return false;
        }
        lp.write(data);
        lp.closeWriteChannel();
        if (!lp.waitForFinished(connectMs * 3)) {
            lp.kill();
            *error = u"lp did not finish"_s;
            return false;
        }
        if (lp.exitCode() != 0) {
            *error = QString::fromLocal8Bit(lp.readAllStandardError()).trimmed();
            return false;
        }
        return true;
#endif
    }

    // Network printers with tickets waiting (the status check stays away).
    void hold(const Job &job, int delta)
    {
        if (job.printer.type != "network")
            return;
        QMutexLocker lock(&busyMutex_);
        busy_[QString::fromStdString(job.printer.host) + u':' + QString::number(job.printer.port)] += delta;
    }

    mutable QMutex busyMutex_;
    QHash<QString, int> busy_;
    PrintSpooler *owner_;
    std::deque<Job> queue_;
    std::atomic<int> pending_{0};
    bool timerArmed_ = false;
};

PrintSpooler::PrintSpooler(QObject *parent)
    : QObject(parent)
    , worker_(new Worker(this))
{
    thread_.setObjectName(u"vt-printer"_s);
    worker_->moveToThread(&thread_);
    thread_.start();
}

PrintSpooler::~PrintSpooler()
{
    thread_.quit();
    thread_.wait();
    delete worker_;
}

void PrintSpooler::submit(const core::PrinterConfig &printer, const QByteArray &data, const QString &description)
{
    worker_->enqueue({printer, data, description, 0, 0});
}

int PrintSpooler::pending() const
{
    return worker_->pending();
}

bool PrintSpooler::busy(const QString &host, int port) const
{
    return worker_->busy(host, port);
}

bool PrintSpooler::waitIdle(int msec)
{
    QElapsedTimer t;
    t.start();
    while (worker_->pending() > 0 && t.elapsed() < msec) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();   // deliver queued signals
    return worker_->pending() == 0;
}

void PrintSpooler::setTimings(int connectMs, int retryBaseMs)
{
    QMetaObject::invokeMethod(worker_, [w = worker_, connectMs, retryBaseMs] {
        w->connectMs = connectMs;
        w->retryBaseMs = retryBaseMs;
    }, Qt::BlockingQueuedConnection);
}

} // namespace vt::print
