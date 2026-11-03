#include "main_cli.hh"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

namespace vt::cli {

QString dataDirOf(const Args &cli, const Options &o)
{
    return cli.isSet(o.dataDir) ? cli.value(o.dataDir) : Options::defaultDataDir();
}

QString dbPathOf(const Args &cli, const Options &o)
{
    return cli.isSet(o.db) ? cli.value(o.db) : QDir(dataDirOf(cli, o)).filePath(u"viewtouch.db"_s);
}

// Where a running server listens for commands from this machine: in its data
// folder, or (socket paths are limited to ~100 bytes) a short name for it in
// the runtime folder.
QString controlSocketOf(const Args &cli, const Options &o)
{
    const QString dir = QDir(dataDirOf(cli, o)).absolutePath();
    const QString inData = QDir(dir).filePath(u"control.sock"_s);
    if (QFile::encodeName(inData).size() < 100)
        return inData;
    const QByteArray id = QCryptographicHash::hash(dir.toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
        .filePath(u"vtmodern-%1.sock"_s.arg(QString::fromLatin1(id)));
}

QString backupDirOf(const Args &cli, const Options &o)
{
    return cli.isSet(o.backupDir) ? cli.value(o.backupDir) : QDir(dataDirOf(cli, o)).filePath(u"backups"_s);
}

} // namespace vt::cli
