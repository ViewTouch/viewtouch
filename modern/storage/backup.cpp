#include "storage/backup.hh"

#include "storage/sealed.hh"

#include <QAtomicInt>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcBackup, "vt.backup")

namespace vt::storage {

namespace {

QString nextConnectionName()
{
    static QAtomicInt counter;
    return u"vt-backup-%1"_s.arg(counter.fetchAndAddRelaxed(1));
}

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

// Runs `work` with a connection of its own to `file`, then drops it.
template <typename Work>
bool withConnection(const QString &file, bool readOnly, QString *error, Work work)
{
    const QString name = nextConnectionName();
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, name);
        db.setDatabaseName(file);
        if (readOnly)
            db.setConnectOptions(u"QSQLITE_OPEN_READONLY"_s);
        if (!db.open()) {
            setError(error, db.lastError().text());
        } else {
            QSqlQuery q(db);
            q.exec(u"PRAGMA busy_timeout=5000"_s);
            ok = work(q);
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

bool moveAside(const QString &from, const QString &to)
{
    return !QFile::exists(from) || QFile::rename(from, to);
}

// A restore run with sudo must leave the database to the account the POS
// runs as: give `file` the owner of `like` (the old database, or the folder).
void takeOwnership(const QString &file, const QString &like)
{
#ifdef Q_OS_UNIX
    const QFileInfo ref(like);
    if (ref.exists() && ::geteuid() == 0) {
        if (::chown(QFile::encodeName(file).constData(), ref.ownerId(), ref.groupId()) != 0)
            qCWarning(lcBackup).noquote() << "Could not hand" << file << "to its owner";
    }
#else
    Q_UNUSED(file)
    Q_UNUSED(like)
#endif
}

} // namespace

// An encrypted copy can't be opened without the key: it is checked byte for byte.
bool sameBytes(const QString &a, const QString &b, QString *error)
{
    QFile fa(a);
    QFile fb(b);
    if (fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly) && fa.readAll() == fb.readAll())
        return true;
    if (error)
        *error = u"the copy differs from the backup"_s;
    return false;
}

bool copyBackup(const QString &backup, const QString &dir, QString *note)
{
    const QString target = QDir(dir).filePath(QFileInfo(backup).fileName());
    const QString part = target + u".part"_s;
    QString error;
    if (!QDir().mkpath(dir))
        error = u"cannot create %1 (is the drive plugged in?)"_s.arg(dir);
    else if (QFile::remove(part), !QFile::copy(backup, part))
        error = u"cannot write to %1"_s.arg(dir);
    else if (!(isSealed(backup) ? sameBytes(backup, part, &error) : verifyDatabase(part, &error))
             || !QFile::rename(part, target))
        QFile::remove(part);
    else {
        if (note)
            *note = u"Copied to %1."_s.arg(dir);
        return true;
    }
    if (note)
        *note = u"Second copy failed: %1."_s.arg(error.isEmpty() ? u"cannot rename the copy"_s : error);
    return false;
}

std::pair<QByteArray, QByteArray> backupKeyOf(const QString &db)
{
    QJsonObject settings;
    withConnection(db, true, nullptr, [&](QSqlQuery &q) {
        if (q.exec(u"SELECT json FROM settings WHERE key = 'pos'"_s) && q.next())
            settings = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
        return true;
    });
    return {QByteArray::fromBase64(settings.value(u"backupKey").toString().toLatin1()),
            QByteArray::fromBase64(settings.value(u"backupSalt").toString().toLatin1())};
}

QString backupFileName(const QDateTime &when)
{
    return u"viewtouch-%1.db"_s.arg(when.toString(u"yyyyMMdd-HHmmss"_s));
}

bool backupDatabase(const QString &db, const QString &target, QString *error)
{
    if (!QFile::exists(db)) {
        setError(error, u"There is no database at %1."_s.arg(db));
        return false;
    }
    if (QFile::exists(target)) {
        setError(error, u"%1 already exists."_s.arg(target));
        return false;
    }
    const QString part = target + u".part"_s;
    QFile::remove(part);
    const bool copied = withConnection(db, false, error, [&](QSqlQuery &q) {
        q.prepare(u"VACUUM INTO ?"_s);
        q.addBindValue(part);
        if (!q.exec()) {
            setError(error, q.lastError().text());
            return false;
        }
        return true;
    });
    if (!copied || !verifyDatabase(part, error) || !QFile::rename(part, target)) {
        if (copied && error && error->isEmpty())
            *error = u"Could not rename %1."_s.arg(part);
        QFile::remove(part);
        return false;
    }
    return true;
}

bool databaseIntact(const QString &file, QString *error)
{
    return withConnection(file, false, error, [&](QSqlQuery &q) {
        if (!q.exec(u"PRAGMA quick_check"_s) || !q.next()) {
            setError(error, q.lastError().text().isEmpty() ? u"not a database"_s : q.lastError().text());
            return false;
        }
        if (q.value(0).toString() != u"ok") {
            setError(error, q.value(0).toString());
            return false;
        }
        return true;
    });
}

bool verifyDatabase(const QString &file, QString *error)
{
    if (!QFileInfo(file).isFile()) {
        setError(error, u"%1 is not a file."_s.arg(file));
        return false;
    }
    return withConnection(file, true, error, [&](QSqlQuery &q) {
        if (!q.exec(u"PRAGMA quick_check"_s) || !q.next() || q.value(0).toString() != u"ok") {
            setError(error, u"%1 is damaged or not a database."_s.arg(QFileInfo(file).fileName()));
            return false;
        }
        int tables = 0;
        if (q.exec(u"SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN ('meta', 'layout_pages', 'settings')"_s)
            && q.next())
            tables = q.value(0).toInt();
        if (tables < 3) {
            setError(error, u"%1 is not a ViewTouch database."_s.arg(QFileInfo(file).fileName()));
            return false;
        }
        return true;
    });
}

bool restoreDatabase(const QString &backup, const QString &db, QString *keptAs, QString *error)
{
    if (!verifyDatabase(backup, error))
        return false;
    QString kept;
    if (QFile::exists(db)) {
        kept = db + u".before-restore-"_s + QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s);
        // The -wal file can hold the latest sales: it moves with the database.
        if (!moveAside(db, kept) || !moveAside(db + u"-wal"_s, kept + u"-wal"_s)
            || !moveAside(db + u"-shm"_s, kept + u"-shm"_s)) {
            setError(error, u"Could not move the current database aside."_s);
            return false;
        }
    }
    if (!QFile::copy(backup, db)) {
        setError(error, u"Could not copy %1 to %2."_s.arg(backup, db));
        if (!kept.isEmpty()) {   // put the old one back
            QFile::rename(kept, db);
            QFile::rename(kept + u"-wal"_s, db + u"-wal"_s);
            QFile::rename(kept + u"-shm"_s, db + u"-shm"_s);
        }
        return false;
    }
    QFile(db).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
                             | QFileDevice::WriteGroup);
    takeOwnership(db, !kept.isEmpty() ? kept : QFileInfo(db).absolutePath());
    if (keptAs)
        *keptAs = kept;
    return true;
}

QStringList listBackups(const QString &dir)
{
    QStringList out;
    const QFileInfoList files = QDir(dir).entryInfoList({u"viewtouch-*.db"_s, u"viewtouch-*.vtbak"_s}, QDir::Files,
                                                        QDir::Name | QDir::Reversed);
    for (const QFileInfo &f : files)
        out.append(f.absoluteFilePath());
    return out;
}

QStringList pruneBackups(const QString &dir, int keep)
{
    QStringList removed;
    if (keep <= 0)
        return removed;
    const QStringList all = listBackups(dir);
    for (qsizetype i = keep; i < all.size(); ++i) {
        if (QFile::remove(all[i]))
            removed.append(all[i]);
    }
    return removed;
}

// --- scheduler -----------------------------------------------------------------------

BackupScheduler::BackupScheduler(QString db, QString dir, int keep, int everyHours, QObject *parent)
    : QObject(parent)
    , db_(std::move(db))
    , dir_(std::move(dir))
    , keep_(keep)
    , everyHours_(everyHours)
{
    timer_.setInterval(60 * 60 * 1000);
    connect(&timer_, &QTimer::timeout, this, &BackupScheduler::check);
}

BackupScheduler::~BackupScheduler()
{
    pool_.waitForDone();
}

void BackupScheduler::start()
{
    if (everyHours_ <= 0)
        return;
    timer_.start();
    check();
}

void BackupScheduler::check()
{
    const QStringList existing = listBackups(dir_);
    const QDateTime due = QDateTime::currentDateTime().addSecs(-qint64(everyHours_) * 3600);
    if (existing.isEmpty() || QFileInfo(existing.first()).lastModified() <= due)
        backupNow();
}

void BackupScheduler::backupNow()
{
    if (running_)
        return;
    running_ = true;
    pool_.start([this, db = db_, dir = dir_, copyDir = copyDir_, keep = keep_, key = sealKey_, salt = sealSalt_] {
        QString error;
        QString target;
        bool ok = QDir().mkpath(dir);
        if (!ok) {
            error = u"Cannot create %1."_s.arg(dir);
        } else {
            const QDateTime now = QDateTime::currentDateTime();
            target = QDir(dir).filePath(backupFileName(now));
            for (int n = 2; QFile::exists(target); ++n)
                target = QDir(dir).filePath(backupFileName(now).replace(u".db"_s, u"-%1.db"_s.arg(n)));
            ok = backupDatabase(db, target, &error);
            if (ok && !key.isEmpty()) {
                // Encrypted: sealed, opened again and checked, then the plain copy goes.
                QString sealed = target;
                sealed.replace(u".db"_s, u".vtbak"_s);
                const QString check = target + u".check"_s;
                QFile::remove(check);
                ok = sealFile(target, sealed, key, salt, &error) && openSealedFileWithKey(sealed, check, key, &error)
                     && verifyDatabase(check, &error);
                QFile::remove(check);
                QFile::remove(target);
                if (!ok)
                    QFile::remove(sealed);
                target = sealed;
            }
            if (ok)
                pruneBackups(dir, keep);
        }
        QString copy;
        bool copyOk = true;
        if (ok && !copyDir.isEmpty()) {
            copyOk = copyBackup(target, copyDir, &copy);
            if (copyOk)
                pruneBackups(copyDir, keep);
        }
        QMetaObject::invokeMethod(this, [this, ok, target, error, copy, copyOk] {
            running_ = false;
            if (ok)
                qCInfo(lcBackup).noquote() << "Backed up the database to" << target;
            else
                qCWarning(lcBackup).noquote() << "Backup failed:" << error;
            if (!copyOk)
                qCWarning(lcBackup).noquote() << copy;
            emit finished(ok, target, error, copy, copyOk);
        }, Qt::QueuedConnection);
    });
}

} // namespace vt::storage
