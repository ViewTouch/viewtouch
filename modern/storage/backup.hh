#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

namespace vt::storage {

// Database backups. A backup is a complete, consistent copy of the SQLite
// database (VACUUM INTO), safe to take while the POS is running: it uses its
// own connection and never blocks the writer for long.

// Copy the database at `db` to `target` (which must not exist yet). The copy
// is written beside the target and renamed into place when complete.
bool backupDatabase(const QString &db, const QString &target, QString *error = nullptr);

// SQLite's quick check of the database in use (WAL included); `error`
// says what is wrong.
bool databaseIntact(const QString &file, QString *error = nullptr);
// Whether `file` is a ViewTouch database that SQLite can read in full.
bool verifyDatabase(const QString &file, QString *error = nullptr);

// Put `backup` in place of the database at `db`. The current database (with
// its -wal/-shm files) is kept beside it as <db>.before-restore-<time>.
// Only while no ViewTouch has the database open (main.cpp holds a lock file).
bool restoreDatabase(const QString &backup, const QString &db, QString *keptAs = nullptr, QString *error = nullptr);

// Copy `backup` into `dir` as well (written aside, checked, then renamed).
// `note` says where it went or why it couldn't.
bool copyBackup(const QString &backup, const QString &dir, QString *note = nullptr);
// Backups in `dir` (plain .db and encrypted .vtbak), newest first.
QStringList listBackups(const QString &dir);
// Remove all but the newest `keep` backups in `dir`; returns what was removed.
QStringList pruneBackups(const QString &dir, int keep);
// The store's backup key and salt from the database at `db` (read only);
// both empty when its backups aren't encrypted.
std::pair<QByteArray, QByteArray> backupKeyOf(const QString &db);
// The file name for a backup taken at `when`.
QString backupFileName(const QDateTime &when);

// Takes a backup when the newest one is older than `every`, checking at
// start and every hour, and on request (end of day). Backups run on a worker
// thread; `finished` arrives on this object's thread.
class BackupScheduler : public QObject {
    Q_OBJECT

public:
    // keep: backups to keep (0 = all); everyHours: 0 = only on request.
    BackupScheduler(QString db, QString dir, int keep, int everyHours, QObject *parent = nullptr);
    ~BackupScheduler() override;   // waits for a running backup

    QString directory() const { return dir_; }
    // Also copy each backup to `dir` (empty: no second copy), keeping as many.
    void setCopyDirectory(const QString &dir) { copyDir_ = dir; }
    // Encrypt backups with this key (see sealed.hh); empty: plain copies.
    // Encrypted backups are named viewtouch-<time>.vtbak.
    void setSealing(const QByteArray &key, const QByteArray &salt)
    {
        sealKey_ = key;
        sealSalt_ = salt;
    }
    // Begin the hourly checks (and check now).
    void start();
    // Take a backup now unless one is already running.
    void backupNow();
    bool running() const { return running_; }

signals:
    // copy: what happened to the second copy ("" when there is none).
    void finished(bool ok, const QString &file, const QString &error, const QString &copy, bool copyOk);

private:
    void check();

    QString db_;
    QString dir_;
    QString copyDir_;
    QByteArray sealKey_;
    QByteArray sealSalt_;
    int keep_;
    int everyHours_;
    QTimer timer_;
    QThreadPool pool_;
    bool running_ = false;
};

} // namespace vt::storage
