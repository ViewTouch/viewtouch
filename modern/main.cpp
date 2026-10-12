#include "app/pos_json.hh"
#include "joincontroller.hh"
#include "language.hh"
#include "layoutcontroller.hh"
#include "app/i18n.hh"
#include "net/stripe_api.hh"
#include "net/discovery.hh"
#include "net/layout_hub.hh"
#include "net/build_info.hh"
#include "net/pos_server.hh"
#include "net/standby.hh"
#include "net/protocol.hh"
#include "net/remote_session.hh"
#include "print/printer_monitor.hh"
#include "print/spooler.hh"
#include "print/ticket_printer.hh"
#include "storage/async_writer.hh"
#include "app/pos_demo.hh"
#include "storage/backup.hh"
#include "storage/sealed.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QScreen>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <termios.h>
#include <unistd.h>
#endif
#include <QTemporaryDir>
#include <QtQml/qqmlextensionplugin.h>

#ifdef Q_OS_ANDROID
#include <QFontDatabase>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>
#endif

#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#endif

Q_IMPORT_QML_PLUGIN(ViewTouchPlugin)

using namespace Qt::StringLiterals;

namespace {

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
    QCommandLineOption edit{u"edit"_s, u"Start in edit mode (with --login and a manager's PIN)."_s};
    QCommandLineOption select{u"select"_s, u"In edit mode, select these zones (comma separated)."_s, u"ids"_s};
    QCommandLineOption size{u"size"_s, u"Window size, e.g. 1280x720."_s, u"WxH"_s, u"1280x720"_s};
    QCommandLineOption screenshot{u"screenshot"_s, u"Render, save a PNG to <file>, and exit."_s, u"file"_s};
    QCommandLineOption kiosk{u"kiosk"_s, u"Full screen with no mouse pointer and no way out (touch screens)."_s};
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
        "(phones get phone pages; the default on Android), or timeClock (only clocking in and out "
        "and schedules). Overrides Manager → Terminals."_s, u"mode"_s};
    QCommandLineOption windowed{u"windowed"_s,
        u"Start in a window instead of full screen (also with --size). F11 switches either way."_s};
    QCommandLineOption backupDir{u"backup-dir"_s, u"Where backups go (default: <data dir>/backups)."_s, u"dir"_s};
    QCommandLineOption backupKeep{u"backup-keep"_s, u"Backups to keep (default 30; 0 = all)."_s, u"count"_s, u"30"_s};
    QCommandLineOption backupEvery{u"backup-every"_s,
        u"Hours between automatic backups (default 24; 0 = only at End of Day)."_s, u"hours"_s, u"24"_s};
    QCommandLineOption exportDir{u"export-dir"_s,
        u"Where reports are saved as CSV / PDF (default: <data dir>/exports)."_s, u"dir"_s};
    QCommandLineOption backup{u"backup"_s, u"Back up the database now (safe while ViewTouch runs) and exit."_s};
    QCommandLineOption pairingCode{u"pairing-code"_s,
        u"Ask the ViewTouch server running on this machine (same --data-dir) for a code to pair a "
        "terminal with, print it, and exit."_s};
    QCommandLineOption restore{u"restore"_s,
        u"Put backup <file> in place of the database and exit. ViewTouch must not be running. An encrypted "
         "backup (.vtbak) asks for the backup password (or reads VTM_BACKUP_PASSWORD)."_s, u"file"_s};

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

void say(vt::app::PosSession &pos, const QString &text)
{
    emit pos.notice(text);
}

// Wait (running events) until `condition` holds or `msec` pass.
template <typename Condition>
bool waitUntil(Condition condition, int msec)
{
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (condition())
            loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(20);
    timeout.start(msec);
    if (!condition())
        loop.exec();
    return condition();
}

#ifdef Q_OS_ANDROID
// Phone pages are portrait; standard ones landscape.
void requestOrientation(bool portrait)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([portrait] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        constexpr jint SENSOR_LANDSCAPE = 6;
        constexpr jint SENSOR_PORTRAIT = 7;
        activity.callMethod<void>("setRequestedOrientation", "(I)V", portrait ? SENSOR_PORTRAIT : SENSOR_LANDSCAPE);
    });
}

// A POS screen must not go dark between orders.
void keepScreenOn()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        const QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        constexpr jint FLAG_KEEP_SCREEN_ON = 0x00000080;
        window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    });
}
#endif

// Full screen unless asked for a window (or a window size, or a picture).
void present(QQuickWindow *window, const Args &cli, const Options &o)
{
    const bool inWindow = !cli.isSet(o.kiosk)
                          && (cli.isSet(o.windowed) || cli.isSet(o.size) || cli.isSet(o.screenshot));
    if (!inWindow)
        window->showFullScreen();
    if (cli.isSet(o.kiosk) && !QGuiApplication::overrideCursor())
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);
}

// Startup page/edit options, then the window. Returns the engine (null when
// the window could not be created).
// Exit status of a kiosk a manager closed: vtmodern-kiosk.service does not
// restart on it (RestartPreventExitStatus).
constexpr int kKioskClosed = 64;
// A standby that took over serves the store from then on (headless); a
// server that finds the store already served elsewhere becomes the standby.
constexpr int kTookOver = 66;
constexpr int kBecomeStandby = 67;
bool gServeHeadless = false;
// The store's main server answering for `storeId` on the network, if any.
std::optional<vt::net::FoundServer> findMainServer(const QString &storeId, int waitMs, quint16 port);
// Seconds without word from the main server before the standby takes over.
constexpr int kTakeOverSeconds = 20;
// Exit status of a factory reset from the Manager page: main wipes the
// database and starts again (systemd restarts the services on it).
constexpr int kFactoryReset = 65;

std::unique_ptr<QQmlApplicationEngine> showUi(const Args &cli, const Options &o, LayoutController &controller)
{
    const bool kiosk = cli.isSet(o.kiosk);
    QObject::connect(&controller, &LayoutController::closeRequested, qApp, [kiosk] {
        qInfo("Closed by a manager");
        QCoreApplication::exit(kiosk ? kKioskClosed : 0);
    });
#ifdef Q_OS_ANDROID
    controller.setAutoFormFactor(true);
    QObject::connect(&controller, &LayoutController::formFactorChanged, &controller,
                     [&controller] { requestOrientation(controller.formFactor() == u"phone"); });
    requestOrientation(controller.formFactor() == u"phone");
#endif
    controller.setExportDirectory(cli.isSet(o.exportDir) ? cli.value(o.exportDir)
                                                         : QDir(dataDirOf(cli, o)).filePath(u"exports"_s));
    if (cli.isSet(o.screen)) {
        controller.setFormFactorOverride(cli.value(o.screen));
        if (cli.value(o.screen) == u"auto")
            controller.setAutoFormFactor(true);   // try it on a desktop by resizing the window
    }
    if (cli.isSet(o.page) && !controller.showPage(cli.value(o.page)))
        qWarning().noquote() << "Cannot open page" << cli.value(o.page);
    if (cli.isSet(o.edit) && controller.requestEditMode()) {   // needs --login with a manager PIN
        if (cli.isSet(o.select))
            controller.editor()->selectOnly(cli.value(o.select).split(u','));
    }

    const QStringList size = cli.value(o.size).split(u'x');
    const int width = size.value(0).toInt();
    const int height = size.value(1).toInt();
    // The customer display: a window of its own, or (a kiosk's one window
    // stretched over two monitors) the second monitor's part of the window.
    QString display = cli.value(o.customerDisplay);
    if (display == u"auto")
        display = cli.isSet(o.kiosk) ? u"split"_s : u"window"_s;
    auto engine = std::make_unique<QQmlApplicationEngine>();
    vt::ui::followLanguage(engine.get(), &controller);
    QObject::connect(engine.get(), &QQmlApplicationEngine::objectCreationFailed,
                     qApp, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine->setInitialProperties({
        {u"controller"_s, QVariant::fromValue(&controller)},
        {u"kiosk"_s, cli.isSet(o.kiosk)},
        {u"keyboardFlag"_s, cli.isSet(o.touchKeyboard) ? (cli.value(o.touchKeyboard) != u"no" ? u"on"_s : u"off"_s) : QString()},
        {u"customerDisplay"_s, display == u"split"},
        {u"customerDisplayAt"_s, [] {
             // The leftmost monitor's width: the POS stays on it.
             QList<QScreen *> screens = QGuiApplication::screens();
             if (screens.size() < 2)
                 return 0;
             std::ranges::sort(screens, {}, [](QScreen *s) { return s->geometry().x(); });
             return screens.first()->geometry().width();
         }()},
        {u"width"_s, width > 0 ? width : 1280},
        {u"height"_s, height > 0 ? height : 720},
    });
    engine->loadFromModule("ViewTouch", "Main");

    auto *window = engine->rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine->rootObjects().first());
    if (!window)
        return nullptr;
    present(window, cli, o);
    if (display == u"window" && controller.pos()) {
        engine->setInitialProperties({{u"pos"_s, QVariant::fromValue(controller.pos())}});
        engine->loadFromModule("ViewTouch", "CustomerDisplayWindow");
        auto *guest = qobject_cast<QQuickWindow *>(engine->rootObjects().constLast());
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (guest && guest != window && screens.size() > 1) {
            // The monitor the POS isn't on.
            QScreen *other = screens.at(0) == window->screen() ? screens.at(1) : screens.at(0);
            guest->setScreen(other);
            guest->setGeometry(other->geometry());
            guest->showFullScreen();
        } else if (!guest) {
            qWarning("Could not open the customer display");
        }
    }
    if (cli.isSet(o.screenshot)) {
        const QString file = cli.value(o.screenshot);
        QTimer::singleShot(1000, window, [window, file] {
            const bool ok = window->grabWindow().save(file);
            if (!ok)
                qCritical().noquote() << "Could not write" << file;
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return engine;
}

// --- a terminal of a remote server ---------------------------------------------------------

// A name to suggest for this terminal: the computer's name, or on a tablet
// its model ("Galaxy Tab A8").
QString suggestedTerminalName(const Args &cli, const Options &o)
{
    if (cli.isSet(o.terminal))
        return cli.value(o.terminal);
#ifdef Q_OS_ANDROID
    const QString model = QJniObject::getStaticObjectField("android/os/Build", "MODEL", "Ljava/lang/String;").toString();
    if (!model.isEmpty())
        return model;
#endif
    return QSysInfo::machineHostName();
}

// The Join screen: find a store, pair with the manager's code. Nothing when
// the window is closed.
std::optional<vt::net::Credentials> joinStore(const Args &cli, const Options &o, const QString &address,
                                              const QString &problem)
{
    JoinController join(suggestedTerminalName(cli, o), address, problem);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"join"_s, QVariant::fromValue(&join)}, {u"kiosk"_s, cli.isSet(o.kiosk)}});
    engine.loadFromModule("ViewTouch", "JoinWindow");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return std::nullopt;
    present(window, cli, o);
    if (cli.isSet(o.screenshot)) {
        const QString file = cli.value(o.screenshot);
        QTimer::singleShot(1500, window, [window, file] {
            window->grabWindow().save(file);
            QCoreApplication::exit(0);
        });
    }
    bool joined = false;
    QEventLoop loop;   // closing the window quits the application, and this loop
    QObject::connect(&join, &JoinController::joined, &loop, [&] { joined = true; loop.quit(); });
    loop.exec();
    return joined ? std::optional(join.credentials()) : std::nullopt;
}

constexpr int kRejoin = 75;   // qApp->exit code: the server refused this device

int runTerminal(const Args &cli, const Options &o)
{
    const QString dataDir = dataDirOf(cli, o);
    QDir().mkpath(dataDir);
    const QString credentialFile = QDir(dataDir).filePath(u"terminal.json"_s);
    vt::i18n::install(QDir(dataDir).filePath(u"translations"_s));   // the store's own phrases, if any
    const QString address = cli.value(o.connect) == u"auto" ? QString() : cli.value(o.connect);
    auto split = [](const QString &a) {
        QString host = a;
        quint16 port = vt::net::DefaultPort;
        if (const qsizetype colon = host.lastIndexOf(u':'); colon > 0) {
            port = quint16(host.mid(colon + 1).toUInt());
            host = host.left(colon);
        }
        return std::pair{host, port};
    };
    auto save = [&](const vt::net::Credentials &c) {
        QString error;
        if (!c.save(credentialFile, &error))
            qWarning().noquote() << "Could not save the pairing to" << credentialFile << ":" << error;
    };

    std::optional<vt::net::Credentials> creds = vt::net::Credentials::load(credentialFile);
    if (creds && !creds->valid())   // a terminal needs its own key
        creds.reset();

    // --pair CODE: pair from the command line (setup scripts).
    if (cli.isSet(o.pair)) {
        if (address.isEmpty()) {
            qCritical().noquote() << "--pair needs --connect <server address>.";
            return 1;
        }
        const auto [host, port] = split(address);
        vt::net::Pairer pairer;
        bool ok = false;
        QString error;
        QEventLoop loop;
        QObject::connect(&pairer, &vt::net::Pairer::finished, &loop,
                         [&](bool success, const vt::net::Credentials &c, const QString &e) {
            ok = success;
            error = e;
            if (success)
                creds = c;
            loop.quit();
        });
        pairer.start(host, port, cli.value(o.pair), suggestedTerminalName(cli, o));
        loop.exec();
        if (!ok) {
            qCritical().noquote() << "Pairing failed:" << error;
            return 1;
        }
        save(*creds);
        qInfo().noquote() << "Paired with" << creds->serverName << "as" << creds->terminalName;
    }

    QString problem;
    for (;;) {
        if (!creds) {
            creds = joinStore(cli, o, address, problem);
            if (!creds)
                return 0;   // the Join window was closed
            save(*creds);
        }
        // An address on the command line wins over the saved one.
        if (!address.isEmpty())
            std::tie(creds->host, creds->port) = split(address);

        vt::net::RemoteSession remote(creds->terminalName);
        remote.setCredentials(*creds);
        if (cli.isSet(o.selfOrder))
            remote.setRequestedScreen(u"selfOrder"_s);
        else if (cli.value(o.screen) == u"timeClock")
            remote.setRequestedScreen(u"timeClock"_s);
        QObject::connect(&remote, &vt::net::RemoteSession::credentialsChanged, &remote, save);
        remote.connectTo(creds->host, creds->port);
        qInfo().noquote() << "Connecting to" << creds->host << "port" << creds->port << "as" << creds->terminalName
                          << "...";
        // The pages as last received, so the terminal can start (showing that
        // it is offline) while the server is still out of reach.
        const QString pagesFile = QDir(dataDir).filePath(u"pages-cache.json"_s);
        auto cachePages = [pagesFile](const vt::layout::Layout &l) {
            QSaveFile f(pagesFile);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(QJsonDocument(l.toJson()).toJson(QJsonDocument::Compact));
                f.commit();
            }
        };
        std::optional<vt::layout::Layout> pages;
        for (bool announced = false; !pages;) {
            if (remote.waitForWelcome(announced ? 60000 : 10000)) {
                pages = remote.layout();
                cachePages(*pages);
                break;
            }
            if (remote.isRejected())
                break;
            QFile cached(pagesFile);
            if (cached.open(QIODevice::ReadOnly))
                pages = vt::layout::Layout::fromJson(QJsonDocument::fromJson(cached.readAll()).object());
            if (!pages && !announced) {
                qWarning().noquote() << "No ViewTouch server answers at" << creds->host << "port" << creds->port
                                     << "yet; still trying.";
                announced = true;
            }
        }
        if (remote.isRejected()) {
            QFile::remove(credentialFile);
            QFile::remove(pagesFile);
            problem = QCoreApplication::translate("main",
                "%1 no longer accepts this terminal (it was removed in Manager → Terminals). Pair it again.")
                          .arg(creds->serverName.isEmpty() ? creds->host : creds->serverName);
            creds.reset();
            continue;
        }

        LayoutController controller(*pages);
        controller.setPos(&remote);
        // Page edits are saved on the server, which passes them on to everyone.
        controller.setSaver([&remote](const vt::layout::Layout &layout, QString *) {
            remote.saveLayout(layout);
            return true;
        });
        QObject::connect(&remote, &vt::net::RemoteSession::layoutSaved, &controller, [&remote](bool ok, const QString &error) {
            say(remote, ok ? QCoreApplication::translate("main", "Pages saved on the server")
                           : QCoreApplication::translate("main", "The server did not save the pages: %1").arg(error));
        });
        QObject::connect(&remote, &vt::net::RemoteSession::layoutReceived, &controller,
                         [&controller, cachePages](const vt::layout::Layout &layout) {
            controller.replaceLayout(layout);
            cachePages(layout);
        });
        // Unpaired while running: back to the Join screen.
        QObject::connect(&remote, &vt::net::RemoteSession::rejected, qApp, [] { QCoreApplication::exit(kRejoin); });

        if (cli.isSet(o.login)) {
            remote.invoke(u"loginWithPin"_s, {cli.value(o.login)});
            waitUntil([&] { return remote.loggedIn(); }, 5000);
        }
        const auto engine = showUi(cli, o, controller);
        if (!engine)
            return 1;
        const int result = qApp->exec();
        if (result != kRejoin)
            return result;
        QFile::remove(credentialFile);
        QFile::remove(pagesFile);
        problem = QCoreApplication::translate("main",
            "%1 no longer accepts this terminal (it was removed in Manager → Terminals). Pair it again.")
                      .arg(creds->serverName.isEmpty() ? creds->host : creds->serverName);
        creds.reset();
    }
}

// A damaged database: say so (and which backup to restore) instead of
// running on it. On screen when there is one; the kiosk then stays closed.
int refuseDamagedDatabase(const Args &cli, const Options &o, const QString &db, const QString &problem)
{
    const QStringList backups = vt::storage::listBackups(backupDirOf(cli, o));
    QString newest;
    for (const QString &b : backups) {
        if (vt::storage::verifyDatabase(b)) {
            newest = b;
            break;
        }
    }
    // SQLite's findings go to the log; the screen says what to do.
    qCritical().noquote() << "The database" << db << "is damaged:" << problem;
    const QString text = newest.isEmpty()
        ? u"The database is damaged:\n%1\n\nThere is no good backup in %2. Keep the file and get help."_s
              .arg(db, backupDirOf(cli, o))
        : u"The database is damaged:\n%1\n\nThe newest good backup is:\n%2\n\nTo go back to it (sales made since "
          "then will be missing), close this and run:\n    vtmodern %3--restore %2\n\nThe damaged file is kept beside it, "
          "and the details are in the log."_s.arg(db, newest,
              cli.isSet(o.dataDir) ? u"--data-dir %1 "_s.arg(cli.value(o.dataDir)) : QString());
    qCritical().noquote() << text;
    if (cli.isSet(o.headless))
        return 1;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"message"_s, text}});
    engine.loadData(R"(
import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts
ApplicationWindow {
    required property string message
    width: 1024; height: 640; visible: true
    title: "ViewTouch - database problem"
    color: "#1b1e24"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 20
        Label { text: "ViewTouch can't open its database"; color: "#ff9a9e"; font.pixelSize: 30; font.bold: true }
        Label { Layout.fillWidth: true; Layout.fillHeight: true; text: message; color: "white"
                font.pixelSize: 18; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText; clip: true }
        Button { text: "Close"; font.pixelSize: 22; implicitHeight: 64; implicitWidth: 200
                 onClicked: Qt.exit(0) }
    }
}
)");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    present(window, cli, o);
    if (cli.isSet(o.screenshot)) {
        const QString file = cli.value(o.screenshot);
        QTimer::singleShot(800, window, [window, file] {
            window->grabWindow().save(file);
            QCoreApplication::exit(0);
        });
    }
    QCoreApplication::exec();
    return cli.isSet(o.kiosk) ? kKioskClosed : 1;   // a kiosk must not restart into it
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

// --- this machine holds the data (standalone, or serving terminals) -----------------------

int runStore(const Args &cli, const Options &o)
{
    const QString dbPath = dbPathOf(cli, o);
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    // One ViewTouch per database: a second one would split the sales.
    QLockFile lock(dbPath + u".lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qint64 pid = 0;
        QString host, app;
        lock.getLockInfo(&pid, &host, &app);
        qCritical().noquote() << u"The database %1 is already in use by another ViewTouch (process %2). "
                                 "Join it as a terminal with --connect instead."_s.arg(dbPath).arg(pid);
        return 1;
    }

    // A damaged database would lose more with every sale: check it first.
    if (QFileInfo(dbPath).size() > 0) {
        QString problem;
        if (!vt::storage::databaseIntact(dbPath, &problem))
            return refuseDamagedDatabase(cli, o, dbPath, problem);
    }

    vt::storage::LayoutStore store(dbPath);
    QString dbError;
    const bool haveStore = store.open(&dbError);
    if (!haveStore)
        qWarning().noquote() << "Pages will not be saved; cannot open database" << store.path() << ":" << dbError;

    // Pages: --layout dir, else the database, else the built-in seed.
    QStringList errors;
    std::optional<vt::layout::Layout> layout;
    if (cli.isSet(o.layout)) {
        layout = vt::layout::Layout::loadDirectory(cli.value(o.layout), &errors);
    } else if (haveStore && store.hasLayout() && !cli.isSet(o.resetLayout)) {
        layout = store.load(&errors);
        // Pages saved by an older ViewTouch: add the starter pages this
        // version brings, and update the ones nobody has edited.
        const auto starter = vt::layout::Layout::loadDirectory(u":/seed"_s);
        if (layout && starter) {
            vt::storage::LayoutStore::StarterState state = store.starterState();
            if (state.seen.isEmpty()) {   // from before this was kept: what it has counts as given
                for (const vt::layout::Page &p : layout->pages)
                    state.seen << p.id;
            }
            const auto update = layout->updateFromStarter(*starter, state.seen, state.installed);
            QString error;
            if ((!update.added.isEmpty() || !update.updated.isEmpty()) && !store.save(*layout, &error))
                qWarning().noquote() << "Could not save the updated pages:" << error;
            if (!update.added.isEmpty())
                qInfo().noquote() << "New starter pages added:" << update.added.join(u", ");
            if (!update.updated.isEmpty())
                qInfo().noquote() << "Starter pages updated (they were never edited):" << update.updated.join(u", ");
            for (const vt::layout::Page &p : starter->pages) {
                if (!state.seen.contains(p.id))
                    state.seen << p.id;
            }
            state.installed = update.installed;
            store.setStarterState(state);
        }
    }
    if (!layout && !cli.isSet(o.layout)) {
        layout = vt::layout::Layout::loadDirectory(u":/seed"_s, &errors);
        if (layout && haveStore) {
            QString error;
            if (!store.save(*layout, &error))
                qWarning().noquote() << "Could not store starter pages:" << error;
            // Remember them as given, as installed: later versions update them.
            vt::storage::LayoutStore::StarterState state;
            for (const vt::layout::Page &p : layout->pages) {
                state.seen << p.id;
                state.installed.insert(p.id, vt::layout::Layout::fingerprint(p));
            }
            store.setStarterState(state);
        }
    }
    for (const QString &e : std::as_const(errors))
        qWarning().noquote() << "layout:" << e;
    if (!layout) {
        qCritical().noquote() << "Could not load any pages.";
        return 1;
    }
    for (const QString &issue : layout->validate())
        qWarning().noquote() << "layout issue:" << issue;

    // POS data: same database; writes go through a background thread.
    vt::storage::PosStore posStore(dbPath);
    QString posError;
    const bool havePosStore = posStore.open(&posError);
    if (!havePosStore)
        qWarning().noquote() << "Sales will not be saved; cannot open database:" << posError;

    auto readSeed = [](const QString &name) {
        QFile f(u":/seed/pos/"_s + name);
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()) : QJsonDocument();
    };
    vt::app::PosData seedData;
    seedData.settings = vt::app::settingsFromJson(readSeed(u"settings.json"_s).object());
    seedData.menu = vt::app::menuFromJson(readSeed(u"menu.json"_s).array());
    seedData.employees = vt::app::employeesFromJson(readSeed(u"employees.json"_s).array());
    seedData.ingredients = vt::app::ingredientsFromJson(readSeed(u"ingredients.json"_s).array());

    std::optional<vt::app::PosData> posData;
    if (havePosStore) {
        if (!posStore.hasMenu() || cli.isSet(o.resetMenu)) {
            QString error;
            if (!posStore.seed(seedData.settings, seedData.menu, seedData.employees, &error, seedData.ingredients))
                qWarning().noquote() << "Could not store starter menu:" << error;
        }
        QStringList posErrors;
        posData = posStore.load(&posErrors);
        for (const QString &e : std::as_const(posErrors))
            qWarning().noquote() << "pos:" << e;
    }
    if (!posData)
        posData = seedData;

    std::unique_ptr<vt::storage::AsyncWriter> writer;
    std::unique_ptr<vt::storage::SqlPosSink> sink;
    if (havePosStore) {
        writer = std::make_unique<vt::storage::AsyncWriter>(dbPath);
        sink = std::make_unique<vt::storage::SqlPosSink>(*writer);
    }
    vt::app::PosService pos(std::move(*posData), sink.get());
    vt::app::PosShared *shared = pos.shared();
    shared->imageCacheDir = QDir(dataDirOf(cli, o)).filePath(u"cache/store-images"_s);
    // Orders for later go to the kitchen by themselves, from the store's computer.
    QTimer laterOrders;
    laterOrders.setInterval(30'000);
    QObject::connect(&laterOrders, &QTimer::timeout, &pos, [&pos] { pos.fireDueOrders(); });
    laterOrders.start();
    if (QFile::exists(dbPath + u".took-over"_s)) {   // this was the standby until a moment ago
        QFile marker(dbPath + u".took-over"_s);
        const QString by = marker.open(QIODevice::ReadOnly) ? QString::fromUtf8(marker.readAll()) : QString();
        marker.close();
        marker.remove();
        ++shared->settings.serverTerm;
        shared->saveSettings();
        qWarning().noquote() << "Serving the store as the main server now (taken over" << by + u')';
    }
    // Languages: each screen its user's; the customer display the store's.
    vt::i18n::install(QDir(dataDirOf(cli, o)).filePath(u"translations"_s));
    vt::i18n::setGuestLanguage(QString::fromStdString(shared->settings.language));
    QObject::connect(shared, &vt::app::PosShared::adminChanged, shared, [shared] {
        vt::i18n::setGuestLanguage(QString::fromStdString(shared->settings.language));
    });
    if (havePosStore) {   // reports over a range read the closed checks back
        shared->history = [dbPath](std::int64_t from, std::int64_t to) {
            return vt::storage::closedChecksBetween(dbPath, from, to);
        };
        shared->refundHistory = [dbPath](std::int64_t from, std::int64_t to) {
            return vt::storage::refundsBetween(dbPath, from, to);
        };
        shared->findChecks = [dbPath](const vt::app::PosShared::CheckFind &f) {
            return vt::storage::findClosedChecks(dbPath, f.from, f.to, f.words, f.limit, f.offset);
        };
        shared->punchHistory = [dbPath](std::int64_t from, std::int64_t to) {
            return vt::storage::punchesBetween(dbPath, from, to);
        };
    }

    // Printing: a worker thread delivers tickets; "file" printers write under
    // <app data>/printouts so tickets are visible without hardware.
    vt::print::PrintSpooler spooler;
    vt::print::TicketPrinter ticketPrinter(spooler, QDir(dataDirOf(cli, o)).filePath(u"printouts"_s));
    // The logo on receipts: one of the store's pictures, or a file here.
    ticketPrinter.setImageSource([shared](const QString &ref) -> QByteArray {
        if (ref.startsWith(u"store:")) {
            const auto it = shared->images.find(ref.mid(6).toStdString());
            return it == shared->images.end() ? QByteArray() : it->second;
        }
        QFile f(ref.startsWith(u"file:") ? QUrl(ref).toLocalFile() : ref);
        return f.size() <= 8 * 1024 * 1024 && f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    });
    pos.setPrinter(&ticketPrinter);
    // How each printer did last, for Manager -> Network.
    auto printerStatus = std::make_shared<QHash<QString, QVariantMap>>();
    QObject::connect(&spooler, &vt::print::PrintSpooler::jobFailed, &pos,
                     [&pos, printerStatus, shared](const QString &printer, const QString &what, const QString &error) {
        say(pos, QCoreApplication::translate("main", "%1 did not print on %2: %3").arg(what, printer, error));
        printerStatus->insert(printer, {{u"status"_s, u"failed"_s}, {u"error"_s, error},
                                        {u"at"_s, QDateTime::currentMSecsSinceEpoch()}});
        emit shared->networkChanged();
    });
    QObject::connect(&spooler, &vt::print::PrintSpooler::jobPrinted, &pos,
                     [&pos, printerStatus, shared](const QString &printer, const QString &what) {
        if (what == u"Test page")   // Manager -> Printers: it reached the printer
            say(pos, QCoreApplication::translate("main", "%1 took the test page").arg(printer));
        const bool was = printerStatus->value(printer).value(u"status"_s) == u"ok"_s;
        printerStatus->insert(printer, {{u"status"_s, u"ok"_s}, {u"at"_s, QDateTime::currentMSecsSinceEpoch()}});
        if (!was)
            emit shared->networkChanged();
    });
    // Out of paper, cover open, not answering: asked of each network ESC/POS
    // printer (not while a ticket is on its way), told to every screen.
    vt::print::PrinterMonitor printerMonitor;
    printerMonitor.setBusy([&spooler](const QString &host, int port) { return spooler.busy(host, port); });
    printerMonitor.setPrinters(shared->settings.printers);
    QObject::connect(shared, &vt::app::PosShared::adminChanged, &printerMonitor,
                     [shared, &printerMonitor] { printerMonitor.setPrinters(shared->settings.printers); });
    QObject::connect(&printerMonitor, &vt::print::PrinterMonitor::statusChanged, shared,
                     [shared, &pos](const QString &id, const QString &problem) {
        shared->setPrinterProblem(id.toStdString(), problem.toStdString());
        for (const QVariant &a : pos.printerAlerts())
            if (a.toMap().value(u"id"_s).toString() == id)
                qWarning().noquote() << "printer:" << a.toMap().value(u"text"_s).toString();
    });
    // It answers status questions: remembered, so silence from it later
    // (even right after a restart) is a warning.
    QObject::connect(&printerMonitor, &vt::print::PrinterMonitor::answersStatus, shared, [shared](const QString &id) {
        for (vt::core::PrinterConfig &p : shared->settings.printers)
            if (QString::fromStdString(p.id) == id && !p.reportsStatus) {
                p.reportsStatus = true;
                shared->saveSettings();
            }
    });
    // A ticket that didn't go: ask that printer what's wrong now.
    QObject::connect(&spooler, &vt::print::PrintSpooler::jobFailed, &printerMonitor, [&printerMonitor] { printerMonitor.checkNow(); });
    if (writer) {
        QObject::connect(writer.get(), &vt::storage::AsyncWriter::writeFailed, &pos, [&pos](const QString &error) {
            say(pos, QCoreApplication::translate("main", "Could not save to the database (will retry): %1").arg(error));
        });
    }

    // Backups: every --backup-every hours and after each End of Day.
    std::unique_ptr<vt::storage::BackupScheduler> backups;
    if (havePosStore) {
        backups = std::make_unique<vt::storage::BackupScheduler>(
            dbPath, backupDirOf(cli, o), cli.value(o.backupKeep).toInt(), cli.value(o.backupEvery).toInt());
        // Encrypted backups once a manager sets a backup password.
        auto applyBackupSettings = [shared, &backups] {
            backups->setCopyDirectory(QString::fromStdString(shared->settings.backupCopyDir));
            backups->setSealing(QByteArray::fromBase64(QByteArray::fromStdString(shared->settings.backupKey)),
                                QByteArray::fromBase64(QByteArray::fromStdString(shared->settings.backupSalt)));
        };
        applyBackupSettings();
        QObject::connect(shared, &vt::app::PosShared::adminChanged, backups.get(), applyBackupSettings);
        QObject::connect(backups.get(), &vt::storage::BackupScheduler::finished, &pos,
                         [&pos, shared](bool ok, const QString &, const QString &error, const QString &copy, bool copyOk) {
            shared->setBackupStatus({{u"at"_s, QTime::currentTime().toString(u"h:mm AP"_s)}, {u"ok"_s, ok},
                                     {u"error"_s, error}, {u"copy"_s, copy}, {u"copyOk"_s, copyOk}});
            if (!ok)
                say(pos, QCoreApplication::translate("main", "The database backup failed: %1").arg(error));
            else if (!copyOk)
                say(pos, QCoreApplication::translate("main", "The backup worked, but %1").arg(copy));
        });
        shared->backupKeyFor = [](const QString &password) {
            QString why;
            if (!vt::storage::sealingAvailable(&why)) {
                qWarning().noquote() << "Can't encrypt backups:" << why;
                return std::pair<QByteArray, QByteArray>{};
            }
            const QByteArray salt = vt::storage::newSalt();
            return std::pair{vt::storage::sealingKey(password, salt), salt};
        };
        shared->requestBackup = [&backups, &writer] {
            if (backups->running())
                return false;
            if (writer)
                writer->flush();
            backups->backupNow();
            return true;
        };
        QObject::connect(shared, &vt::app::PosShared::dayChanged, backups.get(),
                         [shared, &backups, &writer, closedDays = shared->pastDays.size()]() mutable {
            if (shared->pastDays.size() > closedDays) {
                closedDays = shared->pastDays.size();
                writer->flush();   // the day's final writes go in the backup
                backups->backupNow();
            }
        });
        backups->start();
    }

    // Card readers: Stripe, with the store's secret key, from this computer
    // only (connection tokens for the readers, and refunds).
    vt::net::StripeApi stripe([shared] { return QString::fromStdString(shared->settings.stripeSecretKey); });
    shared->stripeConnectionToken = [&stripe](std::function<void(const QString &, const QString &)> done) {
        stripe.connectionToken(std::move(done));
    };
    shared->stripeRefund = [&stripe](const QString &paymentId, std::int64_t cents,
                                     std::function<void(const QString &, const QString &)> done) {
        stripe.refund(paymentId, cents, std::move(done));
    };
    shared->stripeCall = [&stripe](const QString &method, const QString &path, const QString &form,
                                   std::function<void(const QJsonObject &, const QString &)> done) {
        stripe.call(method, path, form, std::move(done));
    };

    // Texts to guests ("your table is ready") go to the store's texting
    // service: a JSON POST of {to, message}, never blocking the screen.
    QNetworkAccessManager texting;
    shared->sendText = [shared, &texting, &pos](const QString &phone, const QString &message) {
        const QUrl url(QString::fromStdString(shared->settings.textWebhook));
        if (!url.isValid() || url.scheme().isEmpty())
            return;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
        request.setTransferTimeout(15000);
        QNetworkReply *reply = texting.post(request, QJsonDocument(QJsonObject{{u"to"_s, phone}, {u"message"_s, message}})
                                                         .toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, &pos, [reply, phone, &pos] {
            if (reply->error() != QNetworkReply::NoError)
                say(pos, QCoreApplication::translate("main", "The text to %1 didn't go out: %2").arg(phone, reply->errorString()));
            reply->deleteLater();
        });
    };

    // Factory reset from the Manager page: back up now, then leave; main
    // deletes the database once it is closed and starts again.
    if (havePosStore) {
        shared->requestFactoryReset = [&cli, &o, &writer] {
            if (writer)
                writer->flush();
            QString error;
            const QString target = backupBeforeReset(cli, o, &error);
            if (target.isEmpty()) {
                qWarning().noquote() << "Factory reset: the backup failed:" << error;
                return false;
            }
            qInfo().noquote() << "Factory reset: backed up to" << target;
            QTimer::singleShot(1500, qApp, [] { QCoreApplication::exit(kFactoryReset); });   // the notice shows first
            return true;
        };
    }

    // Every save of the pages goes through the hub, which tells the terminals.
    vt::net::LayoutHub hub(*layout, haveStore ? &store : nullptr);
    std::unique_ptr<vt::net::PosServer> server;
    std::unique_ptr<vt::net::DiscoveryResponder> discovery;
    std::unique_ptr<QLocalServer> control;
    if (cli.isSet(o.serve) || cli.isSet(o.headless) || gServeHeadless) {
        // Is the store already being served (by the computer that took over
        // while this one was away)? Then this one becomes the standby.
        // Hands this store over to `other` and becomes its standby: this
        // computer's data is kept as a backup, then replaced by the copy.
        auto stepDown = [&](const vt::net::FoundServer &other) {
            writer->flush();
            QString error;
            QDir().mkpath(backupDirOf(cli, o));
            const QString kept = QDir(backupDirOf(cli, o)).filePath(
                vt::storage::backupFileName(QDateTime::currentDateTime()).replace(u".db"_s, u"-before-standby.db"_s));
            if (!vt::storage::backupDatabase(dbPath, kept, &error))
                qWarning().noquote() << "Could not keep a copy of this computer's data:" << error;
            vt::net::Credentials standby;
            standby.serverId = QString::fromStdString(shared->settings.serverId);
            standby.serverName = QString::fromStdString(shared->settings.storeName);
            standby.host = other.host;
            standby.port = other.port;
            standby.replicaKey = QByteArray::fromBase64(QByteArray::fromStdString(shared->settings.replicaKey));
            standby.save(QDir(dataDirOf(cli, o)).filePath(u"standby.json"_s), &error);
        };
        // Is the store already being served (by the standby that took over
        // while this computer was away)? Then this one becomes the standby.
        if (const auto other = findMainServer(QString::fromStdString(shared->settings.serverId), 1500,
                                               cli.isSet(o.port) ? quint16(cli.value(o.port).toUInt()) : vt::net::DefaultPort)) {
            qWarning().noquote() << "The store is already served by" << other->host
                                 << "- this computer becomes its standby.";
            stepDown(*other);
            return kBecomeStandby;
        }
        server = std::make_unique<vt::net::PosServer>(shared, &hub);
        // Updates for the screens: put the new tablet app (and Linux packages) here.
        server->setUpdatesDir(QDir(dataDirOf(cli, o)).filePath(u"updates"_s));
        QDir().mkpath(server->updatesDir());
        // The standby's copy: the whole database when it connects, then
        // every change the writer saves.
        if (writer) {
            server->setSnapshotSource([&writer, dbPath] {
                writer->flush();
                QTemporaryDir tmp;
                const QString file = tmp.filePath(u"copy.db"_s);
                QString error;
                if (!vt::storage::backupDatabase(dbPath, file, &error)) {
                    qWarning().noquote() << "Could not copy the database for the standby:" << error;
                    return QByteArray();
                }
                QFile f(file);
                return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            });
            vt::net::PosServer *raw = server.get();
            writer->setMirror([raw](const QJsonObject &op) { raw->replicate(op); });
        }
        const quint16 port = cli.isSet(o.port) ? quint16(cli.value(o.port).toUInt()) : vt::net::DefaultPort;
        const QHostAddress address = cli.isSet(o.listen) ? QHostAddress(cli.value(o.listen)) : QHostAddress::Any;
        if (!server->listen(address, port)) {
            qCritical().noquote() << "Cannot serve terminals on port" << port << ":" << server->errorString();
            return 1;
        }
        qInfo().noquote() << "Serving terminals on port" << server->port();
        // Terminals looking for a server on the network find this one.
        discovery = std::make_unique<vt::net::DiscoveryResponder>(
            [shared] { return std::pair{QString::fromStdString(shared->settings.serverId),
                                        QString::fromStdString(shared->settings.storeName)}; },
            server->port());
        if (!discovery->listen(server->port()))
            qWarning().noquote() << "Terminals can't find this server by themselves (UDP port" << server->port()
                                 << "):" << discovery->errorString();
        discovery->setTerm(shared->settings.serverTerm);
        // A standby that took over while this server was cut off (a broken
        // cable) serves a newer term: the newest main serves, this one steps down.
        auto *watch = new QTimer(server.get());
        auto *newer = new vt::net::ServerFinder(server.get());
        QObject::connect(newer, &vt::net::ServerFinder::found, server.get(),
                         [stepDown, shared](const vt::net::FoundServer &f) {
            if (f.id != QString::fromStdString(shared->settings.serverId) || f.role != u"main"
                || f.term <= shared->settings.serverTerm)
                return;
            qWarning().noquote() << "The standby at" << f.host << "took over this store;"
                                 << "this computer steps down and becomes its standby.";
            stepDown(f);
            QCoreApplication::exit(kBecomeStandby);
        });
        watch->setInterval(15'000);
        QObject::connect(watch, &QTimer::timeout, newer, [newer, port = server->port()] { newer->search(port); });
        watch->start();
        // `vtmodern --pairing-code` on this machine: the first terminal of a
        // server without a screen gets paired this way.
        control = std::make_unique<QLocalServer>();
        control->setSocketOptions(QLocalServer::UserAccessOption);
        const QString controlPath = controlSocketOf(cli, o);
        QLocalServer::removeServer(controlPath);   // left over from a crash (we hold the lock)
        if (!control->listen(controlPath))
            qWarning().noquote() << "No local control socket:" << control->errorString();
        QObject::connect(control.get(), &QLocalServer::newConnection, control.get(), [&control, shared] {
            while (QLocalSocket *s = control->nextPendingConnection()) {
                QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
                QObject::connect(s, &QLocalSocket::readyRead, s, [s, shared] {
                    if (!s->canReadLine())
                        return;
                    if (s->readLine().trimmed() == "pair") {
                        const QString code = shared->startPairing();
                        const QString until = QLocale().toString(
                            QDateTime::fromMSecsSinceEpoch(shared->pairing->expires).time(), QLocale::ShortFormat);
                        qInfo().noquote() << "Pairing opened from this machine's command line";
                        s->write(QJsonDocument(QJsonObject{{u"code"_s, code}, {u"until"_s, until}})
                                     .toJson(QJsonDocument::Compact) + '\n');
                    }
                    s->disconnectFromServer();
                });
            }
        });
    }
    // Manager -> Network: this computer's role, the screens, the standby, the
    // printers; and for Store Settings -> About, where the store's data is.
    const QString aboutDb = QFileInfo(dbPathOf(cli, o)).absoluteFilePath();
    const QString aboutBackups = QFileInfo(backupDirOf(cli, o)).absoluteFilePath();
    const qint64 startedAt = QDateTime::currentMSecsSinceEpoch();
    shared->network = [shared, &server, printerStatus, aboutDb, aboutBackups, startedAt] {
        QVariantList terminals;
        QVariant standby;
        if (server) {
            for (const QVariant &c : server->connections()) {
                if (c.toMap().value(u"standby"_s).toBool())
                    standby = c;
                else
                    terminals << c;
            }
        }
        QVariantList printers;
        for (const vt::core::PrinterConfig &p : shared->settings.printers) {
            const QString name = QString::fromStdString(p.name);
            QVariantMap row = printerStatus->value(name, {{u"status"_s, u"unknown"_s}});
            row[u"name"_s] = name;
            if (const auto it = shared->printerProblems.find(p.id); it != shared->printerProblems.end())
                row[u"problem"_s] = QString::fromStdString(it->second);
            row[u"type"_s] = QString::fromStdString(p.type);
            row[u"where"_s] = p.type == "network" ? u"%1:%2"_s.arg(QString::fromStdString(p.host)).arg(p.port)
                                                  : QString::fromStdString(p.path);
            printers << row;
        }
        // What the store runs, and the updates it has for its screens.
        const vt::net::AppBuild me = vt::net::appBuild();
        QVariantList updates;
        if (server)
            for (const auto &u : server->updateFiles())
                updates << QVariantMap{{u"name"_s, QFileInfo(u.path).fileName()}, {u"build"_s, u.build},
                                       {u"platform"_s, u.platform}};
        return QVariantMap{{u"role"_s, server ? u"main"_s : u"single"_s}, {u"term"_s, shared->settings.serverTerm},
                           {u"machine"_s, QSysInfo::machineHostName()}, {u"terminals"_s, terminals},
                           {u"standby"_s, standby}, {u"printers"_s, printers},
                           {u"build"_s, vt::net::buildText(me.version, me.number, me.commit)}, {u"buildNumber"_s, me.number},
                           {u"updates"_s, updates}, {u"updatesDir"_s, server ? server->updatesDir() : QString()},
                           {u"platform"_s, me.platform}, {u"qt"_s, QString::fromLatin1(qVersion())},
                           {u"database"_s, aboutDb}, {u"databaseBytes"_s, QFileInfo(aboutDb).size()},
                           {u"backups"_s, aboutBackups}, {u"startedAt"_s, startedAt},
                           {u"lastBackup"_s, [&] {
                                const QFileInfoList files = QDir(aboutBackups).entryInfoList(QDir::Files, QDir::Time);
                                return files.isEmpty() ? qint64(0) : files.first().lastModified().toMSecsSinceEpoch();
                            }()}};
    };
    if (server) {
        QObject::connect(server.get(), &vt::net::PosServer::terminalsChanged, shared, &vt::app::PosShared::networkChanged);
        QObject::connect(server.get(), &vt::net::PosServer::standbyChanged, shared, &vt::app::PosShared::networkChanged);
    }
    if (cli.isSet(o.headless) || gServeHeadless)
        return qApp->exec();

    LayoutController controller(*layout);
    controller.setPos(&pos);
    controller.setSaver([&hub, &controller](const vt::layout::Layout &l, QString *error) {
        return hub.save(l, error, &controller);
    });
    QObject::connect(&hub, &vt::net::LayoutHub::layoutChanged, &controller,
                     [&controller](const vt::layout::Layout &l, const void *origin) {
        if (origin != &controller)
            controller.replaceLayout(l);
    });

    // A self-order kiosk for guests: by option, or this terminal's setting.
    if (cli.isSet(o.selfOrder) || pos.screenMode() == u"selfOrder")
        pos.enableSelfOrder();
    if (cli.isSet(o.login) && !pos.loginWithPin(cli.value(o.login))) {
        qCritical().noquote() << "That PIN is not recognized.";
        return 1;
    }
    const auto engine = showUi(cli, o, controller);
    return engine ? qApp->exec() : 1;
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

// --- the standby server --------------------------------------------------------------------

// The store's main server answering for `storeId` on the network, if any.
std::optional<vt::net::FoundServer> findMainServer(const QString &storeId, int waitMs, quint16 port)
{
    if (storeId.isEmpty())
        return std::nullopt;
    vt::net::ServerFinder finder;
    std::optional<vt::net::FoundServer> found;
    QEventLoop loop;
    QObject::connect(&finder, &vt::net::ServerFinder::found, &loop, [&](const vt::net::FoundServer &s) {
        if (s.id == storeId && s.role == u"main") {
            found = s;
            loop.quit();
        }
    });
    QTimer::singleShot(waitMs, &loop, &QEventLoop::quit);
    finder.search(port);
    loop.exec();
    return found;
}

int runStandby(const Args &cli, const Options &o)
{
    const QString dataDir = dataDirOf(cli, o);
    QDir().mkpath(dataDir);
    const QString dbPath = dbPathOf(cli, o);
    QLockFile lock(dbPath + u".lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qCritical().noquote() << u"Another ViewTouch is using %1."_s.arg(dbPath);
        return 1;
    }
    const QString credentialFile = QDir(dataDir).filePath(u"standby.json"_s);
    std::optional<vt::net::Credentials> creds = vt::net::Credentials::load(credentialFile);
    QString where = cli.isSet(o.standby) ? cli.value(o.standby) : u"auto"_s;
    if (cli.isSet(o.pair) || !creds || creds->replicaKey.size() < 32) {
        if (!cli.isSet(o.pair)) {
            qCritical().noquote() << "The first time, add --pair <code> (Manager -> Terminals -> Pair a Device).";
            return 1;
        }
        QString host = where;
        quint16 port = vt::net::DefaultPort;
        if (where == u"auto") {   // the first store on the network
            vt::net::ServerFinder finder;
            QEventLoop loop;
            QObject::connect(&finder, &vt::net::ServerFinder::found, &loop, [&](const vt::net::FoundServer &s) {
                if (s.role == u"main") {
                    host = s.host;
                    port = s.port;
                    loop.quit();
                }
            });
            QTimer::singleShot(3000, &loop, &QEventLoop::quit);
            finder.search();
            loop.exec();
            if (host == u"auto") {
                qCritical().noquote() << "No ViewTouch store answered on the network; give its address.";
                return 1;
            }
        } else if (const qsizetype colon = host.lastIndexOf(u':'); colon > 0) {
            port = quint16(host.mid(colon + 1).toUInt());
            host = host.left(colon);
        }
        vt::net::Pairer pairer;
        pairer.setStandby(true);
        bool ok = false;
        QString error;
        QEventLoop loop;
        QObject::connect(&pairer, &vt::net::Pairer::finished, &loop,
                         [&](bool success, const vt::net::Credentials &c, const QString &e) {
            ok = success;
            error = e;
            if (success)
                creds = c;
            loop.quit();
        });
        pairer.start(host, port, cli.value(o.pair), u"Standby (%1)"_s.arg(QSysInfo::machineHostName()));
        loop.exec();
        if (!ok) {
            qCritical().noquote() << "Pairing the standby failed:" << error;
            return 1;
        }
        creds->save(credentialFile, &error);
        qInfo().noquote() << "This computer is the standby of" << creds->serverName << "at" << creds->host;
        if (cli.isSet(o.pairOnly))
            return 0;
    }

    vt::net::ReplicaClient replica(dbPath, *creds);
    QObject::connect(&replica, &vt::net::ReplicaClient::credentialsChanged, &replica,
                     [credentialFile](const vt::net::Credentials &c) { c.save(credentialFile); });
    const quint16 port = cli.isSet(o.port) ? quint16(cli.value(o.port).toUInt()) : vt::net::DefaultPort;
    vt::net::StandbyListener listener(dbPath, creds->serverId);
    if (!listener.listen(port))
        qWarning().noquote() << "Screens can't ask this standby to take over (port" << port << "):"
                             << listener.errorString();
    QObject::connect(&replica, &vt::net::ReplicaClient::synced, &listener, [&listener] { listener.setReady(true); });
    vt::net::DiscoveryResponder discovery(
        [&creds] { return std::pair{creds->serverId, creds->serverName}; }, listener.port());
    discovery.setRole(u"standby"_s);
    if (!discovery.listen(port))
        qWarning().noquote() << "Screens can't find this standby by themselves:" << discovery.errorString();
    bool tookOver = false;
    QObject::connect(&listener, &vt::net::StandbyListener::takeOverRequested, &replica, [&](const QString &by) {
        qWarning().noquote() << "Taking over as the main server:" << by;
        replica.stop();
        tookOver = true;
        QFile marker(dbPath + u".took-over"_s);
        if (marker.open(QIODevice::WriteOnly))
            marker.write(by.toUtf8());
        QTimer::singleShot(300, qApp, [] { QCoreApplication::exit(kTookOver); });
    });
    // No word from the main server for a while (it pings every few
    // seconds): take over by itself, as if a manager had asked.
    QTimer silence;
    silence.setInterval(1000);
    QObject::connect(&silence, &QTimer::timeout, &replica, [&] {
        const qint64 heard = replica.lastHeard();
        if (heard > 0 && !replica.inSync() && !tookOver
            && QDateTime::currentMSecsSinceEpoch() - heard > kTakeOverSeconds * 1000)
            emit listener.takeOverRequested(u"by itself (the main server stopped answering)"_s);
    });
    silence.start();
    replica.start();
    const int code = QCoreApplication::exec();
    return tookOver ? kTookOver : code;
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
    if (!store.hasMenu() || cli.isSet(o.resetMenu)) {
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

// Environment that must be set before the application starts:
// - a server without a screen (and --help, --backup...) needs no display, so
//   it gets the offscreen platform unless the user chose one;
// - a kiosk must exit when its compositor dies (systemd then restarts both)
//   instead of waiting to reconnect, as KDE asks Qt apps to.
void prepareEnvironment(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QByteArrayView arg(argv[i]);
        const bool command = arg == "--backup" || arg == "--restore" || arg.startsWith("--restore=")
                             || arg == "--factory-reset" || arg == "--demo-data"
                             || arg == "--pairing-code" || arg == "-h"
                             || arg == "--help" || arg == "--help-all" || arg == "-v" || arg == "--version";
        // One-shot commands answer on the terminal, even through a pipe
        // (Qt would otherwise send the messages to the journal).
        if (command)
            qputenv("QT_FORCE_STDERR_LOGGING", "1");
        if ((command || arg == "--headless") && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        if (arg == "--kiosk")
            qunsetenv("QT_WAYLAND_RECONNECT");
    }
}


#ifdef Q_OS_UNIX
// systemctl stop (SIGTERM), Ctrl+C and SIGHUP end the event loop normally, so
// queued database writes are flushed and the database lock is released. The
// handler only writes to a socket; the event loop does the rest.
int signalPipe[2] = {-1, -1};

void onSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const auto n = ::write(signalPipe[0], &byte, 1);
}

void quitOnSignals(QCoreApplication &app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalPipe) != 0)
        return;
    auto *notifier = new QSocketNotifier(signalPipe[1], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, &app, [notifier] {
        notifier->setEnabled(false);
        char byte;
        [[maybe_unused]] const auto n = ::read(signalPipe[1], &byte, 1);
        qInfo("Stopping");
        QCoreApplication::quit();
    });
    struct sigaction sa = {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : {SIGTERM, SIGINT, SIGHUP})
        ::sigaction(sig, &sa, nullptr);
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    prepareEnvironment(argc, argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationVersion(QStringLiteral(VTM_VERSION));
    QGuiApplication::setApplicationName(u"ViewTouch"_s);
    QGuiApplication::setOrganizationName(u"ViewTouch"_s);
    QQuickStyle::setStyle(u"Fusion"_s);   // editor chrome; POS pages draw themselves
#ifdef Q_OS_UNIX
    quitOnSignals(app);
#endif

    const Options o;
    QCommandLineParser cli;
    cli.setApplicationDescription(u"ViewTouch point of sale.\n\n"
        "One machine keeps the data (standalone, or --serve for other terminals);\n"
        "other terminals run with --connect <server>."_s);
    cli.addHelpOption();
    cli.addVersionOption();
    const QList<QCommandLineOption> all = {o.config, o.dataDir, o.db, o.layout, o.resetLayout, o.resetMenu, o.serve,
        o.port, o.listen, o.headless, o.connect, o.pair, o.terminal, o.kiosk, o.selfOrder, o.touchKeyboard, o.customerDisplay, o.factoryReset, o.demoData, o.standby, o.pairOnly, o.windowed, o.screen, o.login, o.page, o.edit, o.select, o.size,
        o.screenshot, o.backupDir, o.backupKeep, o.backupEvery, o.backup, o.restore, o.pairingCode, o.exportDir};
    cli.addOptions(all);
    cli.process(app);

    Args args(cli);
    if (cli.isSet(o.config)) {
        QString error;
        if (!args.loadConfig(cli.value(o.config), all, &error)) {
            qCritical().noquote() << error;
            return 1;
        }
    }
#ifdef Q_OS_ANDROID
    // A tablet is a terminal of a store: find it and pair on screen, then
    // connect to it on every start.
    args.setDefault(o.connect, u"auto"_s);
    keepScreenOn();
    for (const char *font : {":/fonts/DejaVuSans.ttf", ":/fonts/DejaVuSans-Bold.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(font));
#endif
    if (args.isSet(o.factoryReset)) {
        const int code = runFactoryReset(args, o);
        if (code != 0 || !args.isSet(o.demoData))
            return code;
    }
    if (args.isSet(o.demoData))
        return runDemoData(args, o);
    if (args.isSet(o.backup))
        return runBackup(args, o);
    if (args.isSet(o.restore))
        return runRestore(args, o);
    if (args.isSet(o.pairingCode))
        return runPairingCode(args, o);
    if (args.isSet(o.connect))
        return runTerminal(args, o);
    // The store's servers hand over to each other: a standby that took over
    // serves from then on; a server that finds the store served elsewhere
    // becomes the standby.
    int code = args.isSet(o.standby) ? runStandby(args, o) : runStore(args, o);
    while (code == kTookOver || code == kBecomeStandby) {
        if (code == kTookOver) {
            gServeHeadless = true;
            code = runStore(args, o);
        } else {
            code = runStandby(args, o);
        }
    }
    if (code != kFactoryReset)
        return code;
    // The stores are closed now: wipe, then start again - systemd does that
    // for the services; otherwise start a new ViewTouch with the same options.
    wipeStore(args, o);
    qInfo("Factory reset done; starting again.");
    if (qEnvironmentVariableIsSet("INVOCATION_ID"))
        return kFactoryReset;
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    return 0;
}
