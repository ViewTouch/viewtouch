#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <functional>
#include <QJsonObject>
#include <QString>
#include <QThread>
#include <QVariantMap>

namespace vt::storage {

// Writes rows to SQLite on a dedicated thread so the UI never waits on the
// disk (the legacy freezes came from doing I/O on the event loop).
//
// upsert() only queues. Repeated writes of the same (table, key) before the
// worker gets to them collapse into the newest, and each batch is one
// transaction. A failed batch stays queued and is retried, less often the
// longer it fails; writeFailed() reports it on the caller's thread. A row the
// database refuses five times running (no such table, a constraint) is
// written to setAsidePath() instead, so it can't hold up every later write;
// rowSetAside() reports it.
class AsyncWriter : public QObject {
    Q_OBJECT

public:
    explicit AsyncWriter(QString databasePath, QObject *parent = nullptr);
    ~AsyncWriter() override;   // flushes, then stops the thread

    // INSERT OR REPLACE INTO table (row keys...) VALUES (...). The table must
    // already exist (created by the store's migrations).
    void upsert(const QString &table, const QString &key, const QVariantMap &row);
    // DELETE FROM table WHERE keyColumn = key. Replaces a queued upsert of
    // the same (table, key).
    void remove(const QString &table, const QString &keyColumn, const QString &key);

    // Block until everything queued so far has been attempted.
    void flush();
    // Everything written, and the journal emptied into the database: old
    // copies of changed rows aren't left in it (forgetting a customer).
    void purge();
    // Every change as it is queued ({t: "op", k: "up"|"rm", table, key,
    // row | column}), for a standby server's copy. On the caller's thread.
    void setMirror(std::function<void(const QJsonObject &)> mirror) { mirror_ = std::move(mirror); }
    int pending() const;
    // Rows set aside, one JSON object a line, next to the database.
    QString setAsidePath() const { return path_ + QStringLiteral(".unsaved.jsonl"); }

signals:
    void writeFailed(const QString &error);
    void rowSetAside(const QString &table, const QString &key, const QString &error);

private:
    struct Row {
        QString table;
        QString key;
        QVariantMap values;
        QString deleteColumn;   // set: this is a delete by that column
    };
    void queue(Row row);
    class Worker;

    void drain();   // worker thread
    bool keepAside(const Row &row, const QString &error);   // worker thread

    std::function<void(const QJsonObject &)> mirror_;
    QString path_;
    QThread thread_;
    Worker *worker_ = nullptr;
    mutable QMutex mutex_;
    QList<Row> queue_;               // guarded by mutex_
    QHash<QString, qsizetype> index_; // table+key -> position in queue_
    bool scheduled_ = false;
    bool retryPending_ = false;
    // Worker thread only:
    QHash<QString, int> failures_;   // table+key -> refused this many times running
    int retryMs_ = 2000;
};

} // namespace vt::storage
