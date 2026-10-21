#include "net/offline_store.hh"

#include "app/pos_json.hh"
#include "print/spooler.hh"
#include "print/ticket_printer.hh"
#include "storage/async_writer.hh"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcOffline, "vt.offline")

namespace vt::net {

namespace {
const QString kTable = u"offline_checks"_s;
}

// What the store of this screen's own saves: the checks it made, into the journal.
class OfflineStore::Sink : public app::PosSink {
public:
    explicit Sink(OfflineStore *store) : store_(store) {}
    void saveCheck(const core::Check &check) override { store_->keep(check); }
    void savePunch(const core::TimePunch &) override {}

private:
    OfflineStore *store_;
};

OfflineStore::OfflineStore(QString terminalName, QString journal, QObject *parent)
    : QObject(parent)
    , terminal_(std::move(terminalName))
    , sink_(std::make_unique<Sink>(this))
    , printouts_(journal.isEmpty() ? QDir::temp().filePath(u"viewtouch-printouts"_s)
                                   : QFileInfo(journal).absoluteDir().filePath(u"printouts"_s))
{
    if (journal.isEmpty())
        return;
    // The checks made offline before (the screen restarted before the
    // store's computer took them in): read once, then written in the background.
    {
        const QString name = u"offline-journal-%1"_s.arg(quintptr(this));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, name);
            db.setDatabaseName(journal);
            if (db.open()) {
                QSqlQuery q(db);
                q.exec(u"PRAGMA journal_mode=WAL"_s);
                if (!q.exec(u"CREATE TABLE IF NOT EXISTS %1 (uid TEXT PRIMARY KEY, record TEXT NOT NULL)"_s.arg(kTable)))
                    qCWarning(lcOffline) << "offline journal:" << q.lastError().text();
                q.exec(u"SELECT uid, record FROM %1"_s.arg(kTable));
                while (q.next()) {
                    const QJsonObject item = QJsonDocument::fromJson(q.value(1).toByteArray()).object();
                    if (!item.isEmpty())
                        journal_.insert(q.value(0).toString(), item);
                }
            } else {
                qCWarning(lcOffline) << "offline journal:" << db.lastError().text();
            }
        }
        QSqlDatabase::removeDatabase(name);
    }
    writer_ = std::make_unique<storage::AsyncWriter>(journal);
    for (auto it = journal_.cbegin(); it != journal_.cend(); ++it)
        uids_[it->value(u"check").toObject().value(u"id").toInteger()] = it.key();
}

OfflineStore::~OfflineStore()
{
    stop();
}

void OfflineStore::setKit(const QJsonObject &kit)
{
    if (app::offlineData(kit))
        kit_ = kit;
}

app::PosService *OfflineStore::start()
{
    if (service_)
        return service_.get();
    std::optional<app::PosData> data = app::offlineData(kit_);
    if (!data)
        return nullptr;
    firstId_ = data->lastCheckId + 1;
    // The checks made offline before and not taken in yet: here again, the
    // open ones to go on with, the closed ones in the drawer's cash.
    const std::vector<std::int64_t> storeChecks = [&] {
        std::vector<std::int64_t> ids;
        for (const core::Check &c : data->openChecks)
            ids.push_back(c.id);
        return ids;
    }();
    for (const QJsonObject &item : journal_) {
        std::optional<core::Check> c = app::checkFromJson(item.value(u"check").toObject());
        if (!c || std::ranges::find(storeChecks, c->id) != storeChecks.end())
            continue;
        data->lastCheckId = std::max(data->lastCheckId, c->id);
        if (c->status == core::CheckStatus::Open)
            data->openChecks.push_back(*c);
        else
            data->closedToday.push_back(*c);
    }
    // Its kitchen tickets and receipts: to the store's printers it can still reach.
    if (!spooler_) {
        spooler_ = std::make_unique<print::PrintSpooler>();
        printer_ = std::make_unique<print::TicketPrinter>(*spooler_, printouts_);
        connect(spooler_.get(), &print::PrintSpooler::jobFailed, this, &OfflineStore::printFailed);
    }
    shared_ = std::make_unique<app::PosShared>(std::move(*data), sink_.get());
    shared_->printer = printer_.get();
    for (const std::int64_t id : storeChecks)
        shared_->lockedBy[id] = &store_;
    // By the name the store knows this screen by (its drawer, its printer).
    const QString name = kit_.value(u"terminal").toString();
    service_ = std::make_unique<app::PosService>(shared_.get(), name.isEmpty() ? terminal_ : name);
    qCInfo(lcOffline).noquote() << "working offline, new checks from #" + QString::number(firstId_);
    return service_.get();
}

void OfflineStore::stop()
{
    service_.reset();
    shared_.reset();
}

void OfflineStore::keep(const core::Check &check)
{
    // Only the checks made here (the store's own are locked, never saved here).
    if (check.id < firstId_ && !uids_.contains(check.id))
        return;
    if (check.status == core::CheckStatus::Discarded || check.training) {
        forget(check.id);   // practice, or thrown away: nothing for the store
        return;
    }
    QString &uid = uids_[check.id];
    if (uid.isEmpty())
        uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject record = app::toJson(check);
    record.insert(u"offlineId"_s, uid);
    const QJsonObject item{{u"uid"_s, uid}, {u"check"_s, record}};
    journal_.insert(uid, item);
    if (writer_)
        writer_->upsert(kTable, uid, {{u"uid"_s, uid},
                                      {u"record"_s, QString::fromUtf8(QJsonDocument(item).toJson(QJsonDocument::Compact))}});
    emit pendingChanged();
}

void OfflineStore::forget(std::int64_t checkId)
{
    const auto it = uids_.find(checkId);
    if (it == uids_.end())
        return;
    taken(it->second);
}

QJsonArray OfflineStore::pending() const
{
    QList<QJsonObject> items = journal_.values();
    std::ranges::sort(items, {}, [](const QJsonObject &i) { return i.value(u"check").toObject().value(u"id").toInteger(); });
    QJsonArray out;
    for (const QJsonObject &i : items)
        out.append(i);
    return out;
}

void OfflineStore::taken(const QString &uid)
{
    if (journal_.remove(uid) == 0)
        return;
    std::erase_if(uids_, [&](const auto &kv) { return kv.second == uid; });
    if (writer_)
        writer_->remove(kTable, u"uid"_s, uid);
    emit pendingChanged();
}

} // namespace vt::net
