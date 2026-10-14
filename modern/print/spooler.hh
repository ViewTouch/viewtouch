#pragma once

#include "core/settings.hh"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

namespace vt::print {

// Sends print jobs from a worker thread. submit() returns immediately; a slow
// or unplugged printer can never freeze the screen (the legacy vt_main ran
// lpr and socket writes on its event loop and froze under load).
//
// Delivery: network = raw TCP (port 9100 / JetDirect), file = append,
// cups = `lp -d <queue> -o raw`, none = discard. Failed jobs are retried
// with a growing delay, other printers' jobs keep flowing meanwhile, and
// jobFailed() is reported once a job is given up.
class PrintSpooler : public QObject {
    Q_OBJECT

public:
    static constexpr int MaxAttempts = 3;

    explicit PrintSpooler(QObject *parent = nullptr);
    ~PrintSpooler() override;

    void submit(const core::PrinterConfig &printer, const QByteArray &data, const QString &description);
    int pending() const;
    // A ticket is waiting for, or on its way to, this network printer.
    bool busy(const QString &host, int port) const;
    // Tests: process events until the queue is empty or `msec` passes.
    bool waitIdle(int msec);

    // Shortened network timeouts / retry delays, for tests.
    void setTimings(int connectMs, int retryBaseMs);

signals:
    void jobPrinted(const QString &printer, const QString &description);
    void jobFailed(const QString &printer, const QString &description, const QString &error);

private:
    class Worker;
    QThread thread_;
    Worker *worker_;
};

} // namespace vt::print
