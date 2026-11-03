// The one-shot commands: vtmodern --backup, --snapshot, --restore,
// --factory-reset, --demo-data, --pairing-code, --maintainer-pin. Each does
// its job and exits.

#include "main_cli.hh"
#include "app/pos_demo.hh"
#include "app/pos_json.hh"
#include "storage/async_writer.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"
#include "storage/sealed.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdio>
#include <iostream>
#include <string>

#ifdef Q_OS_UNIX
#include <termios.h>
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

namespace vt::cli {

// Left by a factory reset to an empty store: the next start, on no database,
// begins with the starter set emptied (app::emptyStarter, Layout::clearFloors).
QString startEmptyMarker(const Args &cli, const Options &o)
{
    return QDir(dataDirOf(cli, o)).filePath(u"start-empty"_s);
}

bool markStartEmpty(const Args &cli, const Options &o)
{
    QDir().mkpath(dataDirOf(cli, o));
    QFile f(startEmptyMarker(cli, o));
    return f.open(QIODevice::WriteOnly) && f.write("Factory reset: start with an empty store.\n") > 0;
}

// Delete the database (and printouts): the next start is a fresh install.
void wipeStore(const Args &cli, const Options &o)
{
    const QString dbPath = dbPathOf(cli, o);
    for (const QString &suffix : {u""_s, u"-wal"_s, u"-shm"_s})
        QFile::remove(dbPath + suffix);
    QDir(QDir(dataDirOf(cli, o)).filePath(u"printouts"_s)).removeRecursively();
}

QString backupBeforeReset(const Args &cli, const Options &o, QString *error)
{
    const QString dir = backupDirOf(cli, o);
    const QString target = QDir(dir).filePath(vt::storage::backupFileName(QDateTime::currentDateTime())
                                                  .replace(u".db"_s, u"-before-reset.db"_s));
    if (!QDir().mkpath(dir) || !vt::storage::backupDatabase(dbPathOf(cli, o), target, error))
        return {};
    return target;
}
// --- commands to the server running on this machine ------------------------------------

int runPairingCode(const Args &cli, const Options &o)
{
    QLocalSocket socket;
    socket.connectToServer(controlSocketOf(cli, o));
    if (!socket.waitForConnected(3000)) {
        qCritical().noquote() << "No ViewTouch server is running with the data in" << dataDirOf(cli, o)
                              << "(it must run with --serve or --headless):" << socket.errorString();
        return 1;
    }
    socket.write("pair\n");
    socket.flush();
    QByteArray answer;
    while (!answer.contains('\n') && socket.waitForReadyRead(5000))
        answer += socket.readAll();
    const QJsonObject o2 = QJsonDocument::fromJson(answer.trimmed()).object();
    if (o2.value(u"code").toString().isEmpty()) {
        qCritical().noquote() << "The server did not give a code.";
        return 1;
    }
    qInfo().noquote() << u"Pairing code: %1  (one device, until %2)"_s.arg(o2.value(u"code").toString(),
                                                                          o2.value(u"until").toString());
    qInfo().noquote() << "Type it on the new terminal's Join screen.";
    return 0;
}

// `vtmodern --maintainer-pin CODE`: the store's maintainer code, set on its
// computer only (the running store takes it at once; else it's stored).
int runMaintainerPin(const Args &cli, const Options &o)
{
    QString pin = cli.value(o.maintainerPin).trimmed();
    if (pin == u"-") {   // from the input (not on a command line others can see)
        QTextStream in(stdin);
        pin = in.readLine().trimmed();
    }
    static const QRegularExpression digits(u"^[0-9]{4,8}$"_s);
    if (pin != u"off" && !digits.match(pin).hasMatch()) {
        qCritical().noquote() << "The maintainer code is 4 to 8 digits (or \"off\").";
        return 1;
    }
    if (pin != u"off" && pin.size() < 6)
        qWarning().noquote() << "A code of 6 to 8 digits is much harder to guess (managers' PINs are 6 to 8).";
    QLocalSocket socket;
    socket.connectToServer(controlSocketOf(cli, o));
    if (socket.waitForConnected(1500)) {
        socket.write("maintainer " + pin.toLatin1() + '\n');
        socket.flush();
        QByteArray answer;
        while (!answer.contains('\n') && socket.waitForReadyRead(5000))
            answer += socket.readAll();
        if (!answer.contains("\"ok\"")) {
            qCritical().noquote() << "The running store didn't take it.";
            return 1;
        }
    } else {
        const QString dbPath = dbPathOf(cli, o);
        QLockFile lock(dbPath + u".lock"_s);
        lock.setStaleLockTime(0);
        if (!lock.tryLock(0)) {
            qCritical().noquote() << u"ViewTouch is starting or stopping on %1: try again in a moment."_s.arg(dbPath);
            return 1;
        }
        vt::storage::PosStore store(dbPath);
        QString error;
        if (!store.open(&error)) {
            qCritical().noquote() << "Cannot open the database:" << error;
            return 1;
        }
        // A store not started yet would take the starter settings over it.
        if (!store.started()) {
            qCritical().noquote() << "This store hasn't started yet: start it once, then set the code.";
            return 2;
        }
        auto data = store.load();
        if (!data) {
            qCritical("Cannot read the store.");
            return 1;
        }
        vt::storage::AsyncWriter writer(dbPath);
        vt::storage::SqlPosSink sink(writer);
        vt::app::PosShared shared(std::move(*data), &sink);
        shared.setMaintainerPin(pin == u"off" ? QString() : pin);
        writer.flush();
    }
    qInfo().noquote() << (pin == u"off" ? "The maintainer code is removed." : "The maintainer code is set: log in with it on any screen.");
    return 0;
}

// --- backups from the command line ---------------------------------------------------------

int runBackup(const Args &cli, const Options &o)
{
    const QString dir = backupDirOf(cli, o);
    QString target = QDir(dir).filePath(vt::storage::backupFileName(QDateTime::currentDateTime()));
    QString error;
    if (!QDir().mkpath(dir) || !vt::storage::backupDatabase(dbPathOf(cli, o), target, &error)) {
        qCritical().noquote() << "Backup failed:" << (error.isEmpty() ? u"cannot create "_s + dir : error);
        return 1;
    }
    // The store encrypts its backups: this one too.
    if (const auto [key, salt] = vt::storage::backupKeyOf(target); !key.isEmpty()) {
        QString sealed = target;
        sealed.replace(u".db"_s, u".vtbak"_s);
        QTemporaryDir check;
        const bool ok = vt::storage::sealFile(target, sealed, key, salt, &error)
                        && vt::storage::openSealedFileWithKey(sealed, check.filePath(u"check.db"_s), key, &error)
                        && vt::storage::verifyDatabase(check.filePath(u"check.db"_s), &error);
        QFile::remove(target);
        if (!ok) {
            QFile::remove(sealed);
            qCritical().noquote() << "Encrypting the backup failed:" << error;
            return 1;
        }
        target = sealed;
    }
    vt::storage::pruneBackups(dir, cli.value(o.backupKeep).toInt());
    qInfo().noquote() << "Backed up to" << target;
    return 0;
}

// The database as it is now, for an update to go back to: a plain copy
// (the updater keeps it where only root reads it).
int runSnapshot(const Args &cli, const Options &o)
{
    const QString target = cli.value(o.snapshot);
    QString error;
    if (!vt::storage::backupDatabase(dbPathOf(cli, o), target, &error)) {
        qCritical().noquote() << "Snapshot failed:" << error;
        return 1;
    }
    qInfo().noquote() << "Snapshot in" << target;
    return 0;
}

// --- factory reset and demo data -----------------------------------------------------------

int runFactoryReset(const Args &cli, const Options &o)
{
    const QString dbPath = dbPathOf(cli, o);
    QLockFile lock(dbPath + u".lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qCritical().noquote() << u"ViewTouch is running on %1. Stop it first "
                                 "(installed as a service: sudo systemctl stop vtmodern vtmodern-kiosk)."_s.arg(dbPath);
        return 1;
    }
    if (QFile::exists(dbPath)) {
        QString error;
        const QString target = backupBeforeReset(cli, o, &error);
        if (target.isEmpty()) {
            qCritical().noquote() << "Not reset: the backup failed:" << error;
            return 1;
        }
        qInfo().noquote() << "Backed up to" << target;
    }
    wipeStore(cli, o);
    if (cli.isSet(o.empty)) {
        if (!markStartEmpty(cli, o)) {
            qCritical().noquote() << "Reset, but couldn't mark it to start empty: the next start has the starter set.";
            return 1;
        }
        qInfo().noquote() << "Factory reset done. The next start begins with an empty store.";
        return 0;
    }
    QFile::remove(startEmptyMarker(cli, o));
    qInfo().noquote() << "Factory reset done. The next start begins with the starter pages, menu, staff and settings.";
    return 0;
}

int runDemoData(const Args &cli, const Options &o)
{
    const QString dbPath = dbPathOf(cli, o);
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    QLockFile lock(dbPath + u".lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qCritical().noquote() << u"ViewTouch is running on %1. Stop it first."_s.arg(dbPath);
        return 1;
    }
    vt::storage::PosStore store(dbPath);
    QString error;
    if (!store.open(&error)) {
        qCritical().noquote() << "Cannot open the database:" << error;
        return 1;
    }
    vt::storage::LayoutStore pages(dbPath);   // so backups and checks see a whole store
    pages.open();
    auto readSeed = [](const QString &name) {
        QFile f(u":/seed/pos/"_s + name);
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()) : QJsonDocument();
    };
    if (!store.started() || cli.isSet(o.resetMenu)) {
        if (!store.seed(vt::app::settingsFromJson(readSeed(u"settings.json"_s).object()),
                        vt::app::menuFromJson(readSeed(u"menu.json"_s).array()),
                        vt::app::employeesFromJson(readSeed(u"employees.json"_s).array()), &error,
                        vt::app::ingredientsFromJson(readSeed(u"ingredients.json"_s).array()))) {
            qCritical().noquote() << "Could not store the starter menu:" << error;
            return 1;
        }
    }
    auto data = store.load();
    if (!data) {
        qCritical("Cannot read the store.");
        return 1;
    }
    vt::storage::AsyncWriter writer(dbPath);
    vt::storage::SqlPosSink sink(writer);
    vt::app::PosService pos(std::move(*data), &sink);
    qInfo("Playing two months of service (the same months last year, and a few days years back)...");
    const QString result = vt::app::fillDemoData(pos, QDateTime::currentMSecsSinceEpoch());
    writer.flush();
    if (result.startsWith(u"This store"_s)) {
        qCritical().noquote() << result;
        return 1;
    }
    qInfo().noquote() << result;
    return 0;
}

// Read a password from the terminal without showing it.
QString askPassword(const QString &prompt)
{
    QTextStream out(stdout);
    out << prompt << Qt::flush;
#ifdef Q_OS_UNIX
    termios old {};
    const bool tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old) == 0;
    if (tty) {
        termios quiet = old;
        quiet.c_lflag &= ~tcflag_t(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &quiet);
    }
#endif
    QTextStream in(stdin);
    const QString line = in.readLine();
#ifdef Q_OS_UNIX
    if (tty)
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
#endif
    out << Qt::endl;
    return line;
}

int runRestore(const Args &cli, const Options &o)
{
    const QString dbPath = dbPathOf(cli, o);
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    QLockFile lock(dbPath + u".lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qCritical().noquote() << u"ViewTouch is running on %1. Stop it first "
                                 "(installed as a service: sudo systemctl stop vtmodern vtmodern-kiosk)."_s.arg(dbPath);
        return 1;
    }
    QString keptAs, error;
    QString backup = cli.value(o.restore);
    QTemporaryDir opened;
    if (vt::storage::isSealed(backup)) {
        // Encrypted: this store's own key opens it (same computer); else the password.
        const QString plain = opened.filePath(u"restore.db"_s);
        bool ok = false;
        if (QFile::exists(dbPath)) {
            if (const QByteArray key = vt::storage::backupKeyOf(dbPath).first; !key.isEmpty())
                ok = vt::storage::openSealedFileWithKey(backup, plain, key);
        }
        if (!ok) {
            QString password = qEnvironmentVariable("VTM_BACKUP_PASSWORD");
            if (password.isEmpty())
                password = askPassword(u"Backup password: "_s);
            QFile::remove(plain);
            if (!vt::storage::openSealedFile(backup, plain, password, &error)) {
                qCritical().noquote() << "Restore failed:" << error;
                return 1;
            }
        }
        backup = plain;
    }
    if (!vt::storage::restoreDatabase(backup, dbPath, &keptAs, &error)) {
        qCritical().noquote() << "Restore failed:" << error;
        return 1;
    }
    qInfo().noquote() << "Restored" << cli.value(o.restore) << "to" << dbPath;
    if (!keptAs.isEmpty())
        qInfo().noquote() << "The previous database is kept as" << keptAs;
    return 0;
}

} // namespace vt::cli
