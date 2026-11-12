#pragma once

// The command line (and --config files) shared by main.cpp and the one-shot
// commands in main_commands.cpp.

#include "net/protocol.hh"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QHash>
#include <QStandardPaths>
#include <QString>

namespace vt::cli {

using namespace Qt::StringLiterals;

struct Options {
    QCommandLineOption config{u"config"_s,
        u"Read options from <file>: one \"option = value\" per line (\"serve = yes\" for switches)."_s, u"file"_s};
    QCommandLineOption dataDir{u"data-dir"_s,
        u"Where the database, backups and printouts live (default: %1)."_s.arg(defaultDataDir()), u"dir"_s};
    QCommandLineOption db{u"db"_s, u"SQLite database (default: <data dir>/viewtouch.db)."_s, u"file"_s};
    QCommandLineOption layout{u"layout"_s,
        u"Show pages from <dir> instead of the database. Saving in the editor still writes to the database."_s, u"dir"_s};
    QCommandLineOption resetLayout{u"reset-layout"_s, u"Replace the saved pages with the built-in starter pages."_s};
    QCommandLineOption resetMenu{u"reset-menu"_s,
        u"Replace the menu, employees and settings with the built-in starter data (checks are kept)."_s};
    QCommandLineOption serve{u"serve"_s, u"Also serve remote terminals."_s};
    QCommandLineOption port{u"port"_s, u"Port to serve terminals on (default %1)."_s.arg(vt::net::DefaultPort), u"port"_s};
    QCommandLineOption listen{u"listen"_s, u"Address to serve on (default: all)."_s, u"address"_s};
    QCommandLineOption headless{u"headless"_s, u"Serve without a screen of its own (use with --serve)."_s};
    QCommandLineOption connect{u"connect"_s,
        u"Run as a terminal of the server at <host[:port]>, or \"auto\": the one it paired with, else "
        "find and pair with one on screen."_s, u"host"_s};
    QCommandLineOption pair{u"pair"_s,
        u"Pair this terminal with the --connect server using <code> from Manager → Terminals."_s, u"code"_s};
    QCommandLineOption terminal{u"terminal"_s, u"This terminal's name (default: the computer's name)."_s, u"name"_s};
    QCommandLineOption login{u"login"_s, u"Log in with <pin> at startup (testing)."_s, u"pin"_s};
    QCommandLineOption page{u"page"_s, u"Open page <id> at startup."_s, u"id"_s};
    QCommandLineOption station{u"station"_s,
        u"The home screen of a computer only for ViewTouch: starts ViewTouch (again if it stops), with icons for "
        "the network, updates, restarting... (vtmodern-station.service)."_s};
    QCommandLineOption afterLogin{u"after-login"_s,
        u"After the first login, open page <id> (\"setup\": the setup guide)."_s, u"id"_s};
    QCommandLineOption edit{u"edit"_s, u"Start in edit mode (with --login and a manager's PIN)."_s};
    QCommandLineOption select{u"select"_s, u"In edit mode, select these zones (comma separated)."_s, u"ids"_s};
    QCommandLineOption size{u"size"_s, u"Window size, e.g. 1280x720."_s, u"WxH"_s, u"1280x720"_s};
    QCommandLineOption screenshot{u"screenshot"_s, u"Render, save a PNG to <file>, and exit."_s, u"file"_s};
    QCommandLineOption kiosk{u"kiosk"_s, u"Full screen with no way out (touch screens; the mouse pointer shows only while a mouse is used)."_s};
    QCommandLineOption standby{u"standby"_s,
        u"Be the store's standby server: keep a live copy of the main server's data, ready to take over if it "
         "stops (a manager does it from any screen). <server>: the main server's address, or auto. The first "
         "time, add --pair with a code from Manager -> Terminals."_s, u"server"_s};
    QCommandLineOption pairOnly{u"pair-only"_s,
        u"With --standby and --pair: pair, save it, and exit (vtmodern-setup; the service then runs it)."_s};
    QCommandLineOption factoryReset{u"factory-reset"_s,
        u"Back to a fresh install: back up the database, then delete it (sales, customers, staff, menu, pages, "
         "settings). The next start begins with the starter set. Backups and saved exports stay. ViewTouch must "
         "not be running."_s};
    QCommandLineOption empty{u"empty"_s,
        u"With --factory-reset: the next start begins with an empty store (no menu, tables, sample staff, "
         "printers or tax rates; one sample manager to run the setup guide)."_s};
    QCommandLineOption legacyPages{u"legacy-pages"_s,
                                   u"Make the Legacy style's pages from the legacy ViewTouch's own files in <dir> "
                                   "(vt_data, zone_db.dat, tables.dat) into --legacy-out (default seed/legacy)."_s,
                                   u"dir"_s};
    QCommandLineOption legacyOut{u"legacy-out"_s, u"Where --legacy-pages writes its pages."_s, u"dir"_s};
    QCommandLineOption demoData{u"demo-data"_s,
        u"Fill a store that has no sales yet with demo history (two months, and the same months last year), "
         "customers, gift cards, a schedule and a waitlist, then exit."_s};
    QCommandLineOption customerDisplay{u"customer-display"_s,
        u"Show the order and total to the guest: window (a full-screen window on the second monitor), split (one "
         "window across two monitors, as a kiosk has), auto (split with --kiosk, else window) or off."_s,
        u"mode"_s, u"off"_s};
    QCommandLineOption selfOrder{u"self-order"_s,
        u"This screen is a self-order kiosk: guests order on their own and pay at the counter. A manager's PIN "
         "(hold the top-left corner) ends it. Also set per terminal in Manager -> Terminals."_s};
    QCommandLineOption touchKeyboard{u"touch-keyboard"_s,
        u"Show an on-screen keyboard for text fields: yes or no (default: the terminal's setting, "
          "Manager -> Terminals; on, except on Android, which has its own)."_s, u"yes|no"_s};
    QCommandLineOption screen{u"screen"_s,
        u"Pages for this screen: phone (phone versions, portrait), standard, auto "
        "(phones get phone pages; the default on Android), timeClock (only clocking in and out "
        "and schedules), or nowServing (a TV for guests: orders being made and ready). "
        "Overrides Manager → Terminals."_s, u"mode"_s};
    QCommandLineOption windowed{u"windowed"_s,
        u"Start in a window instead of full screen (also with --size). F11 switches either way."_s};
    QCommandLineOption backupDir{u"backup-dir"_s, u"Where backups go (default: <data dir>/backups)."_s, u"dir"_s};
    QCommandLineOption backupKeep{u"backup-keep"_s, u"Backups to keep (default 30; 0 = all)."_s, u"count"_s, u"30"_s};
    QCommandLineOption backupEvery{u"backup-every"_s,
        u"Hours between automatic backups (default 24; 0 = only at End of Day)."_s, u"hours"_s, u"24"_s};
    QCommandLineOption exportDir{u"export-dir"_s,
        u"Where reports are saved as CSV / PDF (default: <data dir>/exports)."_s, u"dir"_s};
    QCommandLineOption backup{u"backup"_s, u"Back up the database now (safe while ViewTouch runs) and exit."_s};
    QCommandLineOption snapshot{u"snapshot"_s,
        u"Copy the database to <file> now (safe while ViewTouch runs), never encrypted, and exit: what an update "
         "goes back to if the new ViewTouch doesn't start properly (vtmodern-device)."_s, u"file"_s};
    QCommandLineOption maintainerPin{u"maintainer-pin"_s,
        u"Set this store's maintainer code (4 to 8 digits; \"off\" removes it; \"-\": read from the input) and exit: the store's "
        "technician logs in with it, for what managers and the owner can't do (erasing the store, updates, "
        "the network and the standby, templates, backups). It's only set here, on the store's computer."_s,
        u"pin"_s};
    QCommandLineOption pairingCode{u"pairing-code"_s,
        u"Ask the ViewTouch server running on this machine (same --data-dir) for a code to pair a "
        "terminal with, print it, and exit."_s};
    QCommandLineOption restore{u"restore"_s,
        u"Put backup <file> in place of the database and exit. ViewTouch must not be running. An encrypted "
         "backup (.vtbak) asks for the backup password (or reads VTM_BACKUP_PASSWORD)."_s, u"file"_s};

    QCommandLineOption stress{u"stress"_s,
        u"A live stress test: a busy dinner on a throwaway store (a new folder, unless --data-dir names an empty "
         "one), pretend staff working this screen and handhelds, measured as it runs; a report at the end."_s};
    QCommandLineOption stressMinutes{u"stress-minutes"_s, u"How long the stress test runs (default 10)."_s,
        u"minutes"_s, u"10"_s};
    QCommandLineOption stressSpeed{u"stress-speed"_s,
        u"Times faster than real life (default 12: a two-hour dinner in ten minutes)."_s, u"times"_s, u"12"_s};
    QCommandLineOption stressServers{u"stress-servers"_s, u"Pretend servers (default 6)."_s, u"count"_s, u"6"_s};
    QCommandLineOption stressHours{u"stress-hours"_s,
        u"A whole business day this long instead (6 or more: a lunch rush and a dinner rush), at --stress-speed; "
         "ends with everyone counting their bank and End of Day."_s, u"hours"_s};
    QCommandLineOption stressLoad{u"stress-load"_s,
        u"Times the staff, tables, guests and handhelds (default 1; up to 5)."_s, u"times"_s, u"1"_s};
    QCommandLineOption stressExit{u"stress-exit"_s, u"Quit when the stress test is done (exit code 0: passed)."_s};

    static QString defaultDataDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }
};

// Command-line options, with defaults from a --config file. The command line
// wins over the file.
class Args {
public:
    explicit Args(const QCommandLineParser &cli) : cli_(cli) {}

    bool loadConfig(const QString &file, const QList<QCommandLineOption> &known, QString *error)
    {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            *error = u"Cannot read %1: %2"_s.arg(file, f.errorString());
            return false;
        }
        QStringList names;
        for (const QCommandLineOption &o : known)
            names += o.names();
        int lineNo = 0;
        while (!f.atEnd()) {
            ++lineNo;
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';') || line.startsWith(u'['))
                continue;
            const qsizetype eq = line.indexOf(u'=');
            const QString key = (eq < 0 ? line : line.left(eq)).trimmed();
            QString value = eq < 0 ? u"yes"_s : line.mid(eq + 1).trimmed();
            if (value.size() >= 2 && value.startsWith(u'"') && value.endsWith(u'"'))
                value = value.mid(1, value.size() - 2);
            if (!names.contains(key) || key == u"config") {
                *error = u"%1 line %2: unknown option \"%3\""_s.arg(file).arg(lineNo).arg(key);
                return false;
            }
            config_.insert(key, value);
        }
        return true;
    }

    bool isSet(const QCommandLineOption &o) const
    {
        if (cli_.isSet(o))
            return true;
        const QString name = o.names().constFirst();
        if (!config_.contains(name))
            return false;
        const QString v = config_.value(name).toLower();
        if (o.valueName().isEmpty())   // a switch
            return !(v == u"no" || v == u"false" || v == u"off" || v == u"0");
        return !v.isEmpty();
    }

    // A default for an option given neither on the command line nor in the file.
    void setDefault(const QCommandLineOption &o, const QString &value)
    {
        if (!cli_.isSet(o) && !config_.contains(o.names().constFirst()))
            config_.insert(o.names().constFirst(), value);
    }

    QString value(const QCommandLineOption &o) const
    {
        if (cli_.isSet(o))
            return cli_.value(o);
        const QString v = config_.value(o.names().constFirst());
        return !v.isEmpty() ? v : o.defaultValues().value(0);
    }

private:
    const QCommandLineParser &cli_;
    QHash<QString, QString> config_;
};

QString dataDirOf(const Args &cli, const Options &o);
QString dbPathOf(const Args &cli, const Options &o);
// Where a running server listens for commands from this machine.
QString controlSocketOf(const Args &cli, const Options &o);
QString backupDirOf(const Args &cli, const Options &o);
// A factory reset's pieces (main_commands.cpp): the marker left for a reset
// to an empty store, the backup before it, and the store deleted.
QString startEmptyMarker(const Args &cli, const Options &o);
bool markStartEmpty(const Args &cli, const Options &o);
QString backupBeforeReset(const Args &cli, const Options &o, QString *error);
void wipeStore(const Args &cli, const Options &o);

// The one-shot commands (main_commands.cpp): each returns the exit code.
int runPairingCode(const Args &cli, const Options &o);
int runMaintainerPin(const Args &cli, const Options &o);
int runBackup(const Args &cli, const Options &o);
int runSnapshot(const Args &cli, const Options &o);
int runFactoryReset(const Args &cli, const Options &o);
int runDemoData(const Args &cli, const Options &o);
int runLegacyPages(const Args &cli, const Options &o);
int runRestore(const Args &cli, const Options &o);

} // namespace vt::cli
