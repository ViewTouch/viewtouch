#pragma once

#include "core/settings.hh"

#include <QObject>
#include <QString>
#include <QThread>

#include <functional>
#include <vector>

namespace vt::print {

// What a printer says is wrong: "" (nothing), "coverOpen", "paperOut",
// "error" (cutter jam, overheating...), "paperLow" (near the end of the
// roll), or "offline" (not answering).
//
// ESC/POS real-time status (DLE EOT): n = 2 is why the printer is offline
// (bit 2 cover open, bit 5 stopped at the end of the paper, bit 6 an
// error); n = 4 is the paper sensor (bits 2-3 near the end, bits 5-6 out).
// Each answer is one byte with bits 1 and 4 set and 0 and 7 clear.
// Empty answers: the printer doesn't say (no problem known).
QString problemFromStatus(const QByteArray &offlineCause, const QByteArray &paperSensor);

// Asks each watched network ESC/POS printer for its status every so often,
// from its own thread (an unplugged printer never holds up the screens or
// the tickets), and never while a ticket is on its way to that printer.
// statusChanged() says when a printer's problem changes.
class PrinterMonitor : public QObject {
    Q_OBJECT

public:
    explicit PrinterMonitor(QObject *parent = nullptr);
    ~PrinterMonitor() override;

    // The printers to watch: network, ESC/POS, `watch` on.
    void setPrinters(const std::vector<core::PrinterConfig> &printers);
    // True while a ticket is on its way to host:port (the status check waits).
    void setBusy(std::function<bool(const QString &host, int port)> busy);
    // How often (and, for tests, how long to wait for an answer).
    void setTimings(int intervalMs, int timeoutMs);
    // Ask every watched printer now.
    void checkNow();

signals:
    void statusChanged(const QString &printerId, const QString &problem);
    // The first time a printer (PrinterConfig::reportsStatus false) answers.
    void answersStatus(const QString &printerId);

private:
    class Worker;
    QThread thread_;
    Worker *worker_;
};

} // namespace vt::print
