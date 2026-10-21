#pragma once

#include "app/pos_service.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>

#include <map>
#include <memory>

namespace vt::storage { class AsyncWriter; }
namespace vt::print { class PrintSpooler; class TicketPrinter; }

namespace vt::net {

// How a screen keeps selling while it can't reach the store's computer.
//
// The store sends each screen an offline kit (PosService::offlineKit) and a
// fresh one as things change. The kit is kept in memory only, never on disk:
// it holds the checks of the staff's PINs. Started, this is a store of the
// screen's own on that kit, for new checks (the ones open at the store are
// locked: they're the store's). What it makes is kept on disk (offline.db,
// should the screen restart) until the store's computer has taken it in.
class OfflineStore : public QObject {
    Q_OBJECT

public:
    // `journal`: the file for the checks made offline; empty: memory only (tests).
    OfflineStore(QString terminalName, QString journal, QObject *parent = nullptr);
    ~OfflineStore() override;

    void setKit(const QJsonObject &kit);
    bool hasKit() const { return !kit_.isEmpty(); }
    // A store of this screen's own on the kit (nullptr: no kit yet), until stop().
    app::PosService *start();
    void stop();
    app::PosService *service() const { return service_.get(); }

    // Made offline and not taken in yet: [{uid, check}], in the order made.
    QJsonArray pending() const;
    int pendingCount() const { return int(journal_.size()); }
    // The store's computer has it (or had nothing to keep).
    void taken(const QString &uid);

signals:
    void pendingChanged();
    // A ticket the offline store sent to a printer didn't print.
    void printFailed(const QString &printer, const QString &what, const QString &error);

private:
    class Sink;
    void keep(const core::Check &check);
    void forget(std::int64_t checkId);

    QString terminal_;
    QJsonObject kit_;
    std::int64_t firstId_ = 0;
    std::map<std::int64_t, QString> uids_;   // a check made offline -> its uid
    QMap<QString, QJsonObject> journal_;      // uid -> {uid, check}
    std::unique_ptr<storage::AsyncWriter> writer_;
    std::unique_ptr<Sink> sink_;
    QObject store_;   // holds the store's own open checks (no one opens them here)
    QString printouts_;   // where "file" printers write
    std::unique_ptr<print::PrintSpooler> spooler_;   // made the first time it works offline
    std::unique_ptr<print::TicketPrinter> printer_;
    std::unique_ptr<app::PosShared> shared_;
    std::unique_ptr<app::PosService> service_;
};

} // namespace vt::net
