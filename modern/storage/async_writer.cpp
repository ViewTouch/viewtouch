#include "storage/async_writer.hh"

#include <algorithm>

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace vt::storage {

namespace {
// A failed batch is tried again after 2 s, then 4, 8 ... up to a minute.
constexpr int kRetryMs = 2000;
constexpr int kMaxRetryMs = 60000;
// A row the database refuses this many times in a row is set aside, so the
// sales behind it still save.
constexpr int kMaxRowFailures = 5;

// What the row itself is wrong about (no such table or column, a constraint,
// a value of the wrong kind), as opposed to the disk being full or busy,
// where waiting helps and every row would fail alike.
bool rowsFault(const QSqlError &e)
{
    bool ok = false;
    const int code = e.nativeErrorCode().toInt(&ok) & 0xff;   // the primary SQLite code
    return ok && (code == 1 /*ERROR*/ || code == 19 /*CONSTRAINT*/ || code == 20 /*MISMATCH*/ || code == 25 /*RANGE*/);
}
}

// Lives on the writer thread and owns that thread's SQLite connection.
class AsyncWriter::Worker : public QObject {
public:
    explicit Worker(QString path)
        : path_(std::move(path))
        , connection_(u"vt-writer-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    QSqlDatabase db(QString *error)
    {
        QSqlDatabase db = QSqlDatabase::database(connection_, false);
        if (db.isValid() && db.isOpen())
            return db;
        db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_);
        db.setDatabaseName(path_);
        if (!db.open()) {
            *error = db.lastError().text();
            return db;
        }
        QSqlQuery q(db);
        q.exec(u"PRAGMA journal_mode=WAL"_s);
        // FULL: a sale is on disk once its commit returns, even if the
        // power goes out right after (NORMAL could roll the last ones back).
        // This runs on the writer's thread, so the screen never waits for it.
        q.exec(u"PRAGMA synchronous=FULL"_s);
        q.exec(u"PRAGMA busy_timeout=5000"_s);
        // What's deleted or replaced is overwritten with zeros, not left in
        // free space (a customer who asked to be forgotten).
        q.exec(u"PRAGMA secure_delete=ON"_s);
        return db;
    }

    void close()
    {
        {
            QSqlDatabase db = QSqlDatabase::database(connection_, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(connection_);
    }

private:
    QString path_;
    QString connection_;
};

AsyncWriter::AsyncWriter(QString databasePath, QObject *parent)
    : QObject(parent)
    , path_(std::move(databasePath))
{
    thread_.setObjectName(u"vt-db-writer"_s);
    worker_ = new Worker(path_);
    worker_->moveToThread(&thread_);
    thread_.start();
}

AsyncWriter::~AsyncWriter()
{
    flush();
    QMetaObject::invokeMethod(worker_, [w = worker_] { w->close(); }, Qt::BlockingQueuedConnection);
    thread_.quit();
    thread_.wait();
    delete worker_;
}

// A table or column name: put into the SQL as it is, so only plain names
// (a standby gets them from the network).
static bool plainName(const QString &name)
{
    static const QRegularExpression shape(u"^[A-Za-z_][A-Za-z0-9_]{0,63}$"_s);
    return shape.match(name).hasMatch();
}

void AsyncWriter::upsert(const QString &table, const QString &key, const QVariantMap &row)
{
    if (!plainName(table) || std::ranges::any_of(row.keys(), [](const QString &c) { return !plainName(c); })) {
        emit writeFailed(u"%1: not a table or column name"_s.arg(table.left(64)));
        return;
    }
    if (mirror_)
        mirror_(QJsonObject{{QStringLiteral("t"), QStringLiteral("op")}, {QStringLiteral("k"), QStringLiteral("up")},
                            {QStringLiteral("table"), table}, {QStringLiteral("key"), key},
                            {QStringLiteral("row"), QJsonObject::fromVariantMap(row)}});
    queue({table, key, row, QString()});
}

void AsyncWriter::remove(const QString &table, const QString &keyColumn, const QString &key)
{
    if (!plainName(table) || !plainName(keyColumn)) {
        emit writeFailed(u"%1: not a table or column name"_s.arg(table.left(64)));
        return;
    }
    if (mirror_)
        mirror_(QJsonObject{{QStringLiteral("t"), QStringLiteral("op")}, {QStringLiteral("k"), QStringLiteral("rm")},
                            {QStringLiteral("table"), table}, {QStringLiteral("key"), key},
                            {QStringLiteral("column"), keyColumn}});
    queue({table, key, {}, keyColumn});
}

void AsyncWriter::queue(Row row)
{
    QMutexLocker lock(&mutex_);
    const QString id = row.table + u'\x1f' + row.key;
    if (auto it = index_.find(id); it != index_.end()) {
        queue_[*it] = std::move(row);   // newest wins, keeps its place in line
    } else {
        index_.insert(id, queue_.size());
        queue_.append(std::move(row));
    }
    if (!scheduled_) {
        scheduled_ = true;
        QMetaObject::invokeMethod(worker_, [this] { drain(); }, Qt::QueuedConnection);
    }
}

void AsyncWriter::flush()
{
    QMetaObject::invokeMethod(worker_, [this] { drain(); }, Qt::BlockingQueuedConnection);
}

void AsyncWriter::purge()
{
    flush();
    QMetaObject::invokeMethod(worker_, [this] {
        QString error;
        QSqlDatabase db = worker_->db(&error);
        if (db.isOpen())
            QSqlQuery(db).exec(u"PRAGMA wal_checkpoint(TRUNCATE)"_s);
    }, Qt::BlockingQueuedConnection);
}

int AsyncWriter::pending() const
{
    QMutexLocker lock(&mutex_);
    return int(queue_.size());
}

void AsyncWriter::drain()
{
    QList<Row> batch;
    {
        QMutexLocker lock(&mutex_);
        scheduled_ = false;
        batch.swap(queue_);
        index_.clear();
    }
    if (batch.isEmpty())
        return;

    QString error;
    QString failedId;   // the row the database refused, when it's that row's fault
    QSqlDatabase db = worker_->db(&error);
    bool ok = error.isEmpty() && db.transaction();
    if (ok) {
        // One prepared statement per kind of row in the batch, not one per row.
        // A statement that wouldn't prepare (no such table or column) keeps
        // that error: exec() after it only says the parameters don't match.
        QHash<QString, QSqlQuery> statements;
        QHash<QString, QSqlError> unprepared;
        auto statement = [&](const QString &sql) -> QSqlQuery & {
            auto it = statements.find(sql);
            if (it == statements.end()) {
                it = statements.insert(sql, QSqlQuery(db));
                if (!it->prepare(sql))
                    unprepared.insert(sql, it->lastError());
            }
            return *it;
        };
        for (const Row &row : std::as_const(batch)) {
            QString sql;
            QStringList cols;
            if (!row.deleteColumn.isEmpty()) {
                sql = u"DELETE FROM %1 WHERE %2 = ?"_s.arg(row.table, row.deleteColumn);
            } else {
                cols = row.values.keys();
                QStringList marks;
                marks.fill(u"?"_s, cols.size());
                sql = u"INSERT OR REPLACE INTO %1 (%2) VALUES (%3)"_s.arg(row.table, cols.join(u','), marks.join(u','));
            }
            QSqlQuery &q = statement(sql);
            if (const auto bad = unprepared.constFind(sql); bad != unprepared.cend()) {
                error = u"%1: %2"_s.arg(row.table, bad->text());
                if (rowsFault(*bad))
                    failedId = row.table + u'\x1f' + row.key;
                ok = false;
                break;
            }
            if (row.deleteColumn.isEmpty()) {
                for (qsizetype i = 0; i < cols.size(); ++i)
                    q.bindValue(int(i), row.values.value(cols[i]));
            } else {
                q.bindValue(0, row.key);
            }
            if (!q.exec()) {
                error = u"%1: %2"_s.arg(row.table, q.lastError().text());
                if (rowsFault(q.lastError()))
                    failedId = row.table + u'\x1f' + row.key;
                ok = false;
                break;
            }
        }
        for (QSqlQuery &q : statements)
            q.finish();
        if (ok && !db.commit()) {
            error = db.lastError().text();
            ok = false;
        }
        if (!ok)
            db.rollback();
    } else if (error.isEmpty()) {
        error = db.lastError().text();
    }

    if (ok) {
        failures_.clear();
        retryMs_ = kRetryMs;
        return;
    }

    // The same row refused again and again: write it to the side file, take
    // it out of the batch, and try the rest again at once.
    bool setAside = false;
    if (!failedId.isEmpty() && ++failures_[failedId] >= kMaxRowFailures) {
        for (qsizetype i = 0; i < batch.size(); ++i) {
            const Row &row = batch[i];
            if (row.table + u'\x1f' + row.key != failedId)
                continue;
            if (keepAside(row, error)) {
                emit rowSetAside(row.table, row.key, error);
                batch.removeAt(i);
                failures_.remove(failedId);
                setAside = true;
            }
            break;
        }
    }

    // Put the batch back in front of anything queued meanwhile, unless a
    // newer version of the same row has arrived.
    {
        QMutexLocker lock(&mutex_);
        QList<Row> merged;
        QHash<QString, qsizetype> index;
        for (const Row &row : std::as_const(batch)) {
            const QString id = row.table + u'\x1f' + row.key;
            if (index_.contains(id))
                continue;
            index.insert(id, merged.size());
            merged.append(row);
        }
        for (const Row &row : std::as_const(queue_)) {
            index.insert(row.table + u'\x1f' + row.key, merged.size());
            merged.append(row);
        }
        queue_ = merged;
        index_ = index;
        if (setAside) {
            if (!scheduled_) {
                scheduled_ = true;
                QMetaObject::invokeMethod(worker_, [this] { drain(); }, Qt::QueuedConnection);
            }
        } else if (!retryPending_) {
            retryPending_ = true;
            const int wait = retryMs_;
            retryMs_ = std::min(retryMs_ * 2, kMaxRetryMs);
            QTimer::singleShot(wait, worker_, [this] {
                {
                    QMutexLocker lock(&mutex_);
                    retryPending_ = false;
                }
                drain();
            });
        }
    }
    emit writeFailed(error);   // queued to the owner's thread
}

bool AsyncWriter::keepAside(const Row &row, const QString &error)
{
    QFile f(setAsidePath());
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return false;   // then it stays queued: never dropped without a copy
    QJsonObject line{{u"at"_s, QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                     {u"table"_s, row.table}, {u"key"_s, row.key}, {u"error"_s, error}};
    if (row.deleteColumn.isEmpty())
        line.insert(u"row"_s, QJsonObject::fromVariantMap(row.values));
    else
        line.insert(u"deleteBy"_s, row.deleteColumn);
    const QByteArray bytes = QJsonDocument(line).toJson(QJsonDocument::Compact) + '\n';
    return f.write(bytes) == bytes.size() && f.flush();
}

} // namespace vt::storage
