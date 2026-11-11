#include "main_cli.hh"
#include "app/pos_json.hh"
#include "joincontroller.hh"
#include "stationcontroller.hh"
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
#include "net/stress.hh"
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
#include <QInputDevice>
#include <QMouseEvent>
#include <QPointingDevice>
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
#include <QStorageInfo>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <sys/stat.h>   // umask
#include <termios.h>
#include <unistd.h>
#endif
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <execinfo.h>   // the crash handler's backtrace
#include <fcntl.h>
#endif
#include <csignal>
#include <cstring>
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
using namespace vt::cli;

namespace {


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
// On: the screen stays on (tablets, registers). Off: it sleeps like a
// phone's (a handheld; LayoutController::screenStaysOn).
void keepScreenOn(bool on = true)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        const QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        constexpr jint FLAG_KEEP_SCREEN_ON = 0x00000080;
        window.callMethod<void>(on ? "addFlags" : "clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    });
}
#endif

// A kiosk's mouse pointer: hidden on a touch screen, shown while a mouse is
// used (a register with no touch screen, a virtual machine), hidden again
// at the next touch.
class KioskPointer : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::TouchBegin:
            setHidden(true);
            break;
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress: {
            // Mouse events made from a touch come from the touch screen.
            const auto *device = static_cast<QPointerEvent *>(event)->pointingDevice();
            if (device && device->type() == QInputDevice::DeviceType::Mouse)
                setHidden(false);
            break;
        }
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }
    void setHidden(bool hidden)
    {
        if (hidden == hidden_)
            return;
        hidden_ = hidden;
        if (hidden)
            QGuiApplication::setOverrideCursor(Qt::BlankCursor);
        else
            QGuiApplication::restoreOverrideCursor();
    }

private:
    bool hidden_ = false;
};

// Full screen unless asked for a window (or a window size, or a picture).
void present(QQuickWindow *window, const Args &cli, const Options &o)
{
    const bool inWindow = !cli.isSet(o.kiosk)
                          && (cli.isSet(o.windowed) || cli.isSet(o.size) || cli.isSet(o.screenshot));
    if (!inWindow)
        window->showFullScreen();
    static KioskPointer *pointer = nullptr;
    if (cli.isSet(o.kiosk) && !pointer) {
        pointer = new KioskPointer(qApp);
        pointer->setHidden(true);
        qApp->installEventFilter(pointer);
    }
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
#ifdef Q_OS_ANDROID
    QObject::connect(&controller, &LayoutController::formFactorChanged, &controller,
                     [&controller] { keepScreenOn(controller.screenStaysOn()); });
    keepScreenOn(controller.screenStaysOn());
#endif
    const bool kiosk = cli.isSet(o.kiosk);
    QObject::connect(&controller, &LayoutController::closeRequested, qApp, [kiosk] {
        qInfo("Closed by a manager");
#ifdef Q_OS_ANDROID
        // A tablet only for ViewTouch (Kiosk.kt): a tablet again until it restarts.
        QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
            QJniObject::callStaticMethod<void>("org/viewtouch/pos/Kiosk", "leave", "(Landroid/app/Activity;)V",
                                               QNativeInterface::QAndroidApplication::context().object());
        }).waitForFinished();
#endif
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
    if (cli.isSet(o.afterLogin))
        controller.setAfterLogin(cli.value(o.afterLogin));
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

// The home screen of a ViewTouch computer: ViewTouch runs as its child.
// A crash: where in the code, for whoever fixes it. Written to
// <data dir>/crashes/crash-<started>-<pid>.txt (and the journal), then
// ViewTouch stops as it would have (systemd or the home screen starts it
// again; Store Check says it closed by itself). Only what a signal handler
// may do: the file's name is made beforehand, and write() it.
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID) && !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_THREAD__)
namespace {
char g_crashFile[1024];
char g_crashHeader[256];

void onCrash(int sig)
{
    const char *name = sig == SIGSEGV ? "SIGSEGV (bad memory access)" : sig == SIGABRT ? "SIGABRT (aborted)"
                       : sig == SIGBUS ? "SIGBUS" : sig == SIGFPE ? "SIGFPE" : sig == SIGILL ? "SIGILL" : "a signal";
    void *frames[64];
    const int n = backtrace(frames, 64);
    const int fd = ::open(g_crashFile, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
    for (const int out : {fd, int(STDERR_FILENO)}) {
        if (out < 0)
            continue;
        [[maybe_unused]] auto w = ::write(out, g_crashHeader, std::strlen(g_crashHeader));
        w = ::write(out, name, std::strlen(name));
        w = ::write(out, "\n", 1);
        backtrace_symbols_fd(frames, n, out);
    }
    if (fd >= 0)
        ::close(fd);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
} // namespace

void installCrashHandler(const QString &dataDir)
{
    const QDir dir(QDir(dataDir).filePath(u"crashes"_s));
    if (!dir.mkpath(u"."_s))
        return;
    // The newest ten are plenty.
    const QFileInfoList old = dir.entryInfoList({u"crash-*.txt"_s}, QDir::Files, QDir::Time);
    for (qsizetype i = 10; i < old.size(); ++i)
        QFile::remove(old[i].filePath());
    const QByteArray file = dir.filePath(u"crash-%1-%2.txt"_s.arg(QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s))
                                             .arg(QCoreApplication::applicationPid())).toLocal8Bit();
    const QByteArray header = u"ViewTouch %1 crashed: "_s.arg(QCoreApplication::applicationVersion()).toUtf8();
    if (file.size() >= qsizetype(sizeof g_crashFile) || header.size() >= qsizetype(sizeof g_crashHeader))
        return;
    std::memcpy(g_crashFile, file.constData(), file.size() + 1);
    std::memcpy(g_crashHeader, header.constData(), header.size() + 1);
    void *warm[1];
    backtrace(warm, 1);   // loads what it needs now, not in the handler
    for (const int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
        std::signal(sig, onCrash);
}
#else
void installCrashHandler(const QString &) {}
#endif

// After an update the updater checks this version came up (vtmodern-device
// update-verify): two minutes running without falling over, it says so in
// <data dir>/healthy (its version). A version that never writes it is
// taken off this computer again.
void markHealthyLater(const QString &dataDir)
{
    const int after = qEnvironmentVariableIsSet("VTM_HEALTHY_AFTER_MS") ? qEnvironmentVariableIntValue("VTM_HEALTHY_AFTER_MS")
                                                                         : 2 * 60'000;   // tests: sooner
    QTimer::singleShot(after, qApp, [dataDir] {
        QSaveFile file(QDir(dataDir).filePath(u"healthy"_s));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QCoreApplication::applicationVersion().toUtf8() + '\n');
            file.commit();
        }
    });
}

int runStation(const Args &cli, const Options &o)
{
    const QString dataDir = dataDirOf(cli, o);
    vt::i18n::install(QDir(dataDir).filePath(u"translations"_s));
    StationController station(dataDir, cli.isSet(o.config) ? cli.value(o.config) : QString(), cli.isSet(o.kiosk));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"station"_s, QVariant::fromValue(&station)}, {u"kiosk"_s, cli.isSet(o.kiosk)}});
    engine.loadFromModule("ViewTouch", "StationWindow");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    if (cli.isSet(o.size)) {
        const QStringList wh = cli.value(o.size).split(u'x');
        if (wh.size() == 2)
            window->resize(wh[0].toInt(), wh[1].toInt());
    }
    present(window, cli, o);
    if (cli.isSet(o.screenshot)) {
        const QString file = cli.value(o.screenshot);
        QTimer::singleShot(1500, window, [window, file] {
            window->grabWindow().save(file);
            QCoreApplication::exit(0);
        });
    } else if (!station.role().isEmpty()) {
        station.startPos();   // set up already: straight into ViewTouch (which says it's healthy)
    } else {
        markHealthyLater(dataDir);
    }
    return QCoreApplication::exec();
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
        // Checks made while the store's computer is out of reach, until it takes them in.
        remote.setOfflineJournal(QDir(dataDir).filePath(u"offline.db"_s));
        if (cli.isSet(o.selfOrder))
            remote.setRequestedScreen(u"selfOrder"_s);
        else if (cli.value(o.screen) == u"timeClock" || cli.value(o.screen) == u"nowServing")
            remote.setRequestedScreen(cli.value(o.screen));
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

    // A factory reset to an empty store, starting now (on no database only).
    const bool startEmpty = QFile::exists(startEmptyMarker(cli, o)) && !QFile::exists(dbPath) && !cli.isSet(o.layout);
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
            // New and updated starter pages come arranged for Classic: moved to the store's style.
            if (const QStringList fresh = update.added + update.updated; !fresh.isEmpty())
                layout->arrangePages(fresh);
            QString error;
            if ((!update.added.isEmpty() || !update.updated.isEmpty() || !update.buttonsAdded.isEmpty())
                && !store.save(*layout, &error))
                qWarning().noquote() << "Could not save the updated pages:" << error;
            if (!update.added.isEmpty())
                qInfo().noquote() << "New starter pages added:" << update.added.join(u", ");
            if (!update.updated.isEmpty())
                qInfo().noquote() << "Starter pages updated (they were never edited):" << update.updated.join(u", ");
            if (!update.buttonsAdded.isEmpty())
                qInfo().noquote() << "New starter buttons added to edited pages:" << update.buttonsAdded.join(u", ");
            if (!update.buttonsNoRoom.isEmpty())
                qInfo().noquote() << "New starter buttons with no room (add them in the editor):" << update.buttonsNoRoom.join(u", ");
            for (const vt::layout::Page &p : starter->pages) {
                if (!state.seen.contains(p.id))
                    state.seen << p.id;
                for (const vt::layout::Zone &z : p.zones)   // its buttons too: one taken off later stays off
                    if (const QString key = p.id + u'#' + z.id; !state.seen.contains(key))
                        state.seen << key;
            }
            state.installed = update.installed;
            store.setStarterState(state);
        }
    }
    if (!layout && !cli.isSet(o.layout)) {
        layout = vt::layout::Layout::loadDirectory(u":/seed"_s, &errors);
        if (layout && haveStore) {
            // Remember them as given, as installed: later versions update them.
            // (Started empty: the floors taken out stay out, and the one
            // emptied counts as changed, so no update brings the tables back.)
            vt::storage::LayoutStore::StarterState state;
            for (const vt::layout::Page &p : layout->pages) {
                state.seen << p.id;
                for (const vt::layout::Zone &z : p.zones)
                    state.seen << p.id + u'#' + z.id;
                state.installed.insert(p.id, vt::layout::Layout::fingerprint(p));
            }
            if (startEmpty)
                qInfo().noquote() << "Starting empty: floor plans taken out:" << layout->clearFloors().join(u", ");
            QString error;
            if (!store.save(*layout, &error))
                qWarning().noquote() << "Could not store starter pages:" << error;
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
    if (startEmpty)
        vt::app::emptyStarter(seedData);

    std::optional<vt::app::PosData> posData;
    if (havePosStore) {
        if (!posStore.started() || cli.isSet(o.resetMenu)) {
            QString error;
            if (!posStore.seed(seedData.settings, seedData.menu, seedData.employees, &error, seedData.ingredients))
                qWarning().noquote() << "Could not store starter menu:" << error;
            else if (startEmpty)
                QFile::remove(startEmptyMarker(cli, o));
        }
        QStringList posErrors;
        posData = posStore.load(&posErrors);
        for (const QString &e : std::as_const(posErrors))
            qWarning().noquote() << "pos:" << e;
    }
    if (!posData)
        posData = seedData;
    if (cli.isSet(o.stress) && (!posData->closedToday.empty() || !posData->pastDays.empty() || !posData->openChecks.empty())) {
        qCritical().noquote() << "This store has sales: a stress test runs only on a new folder (leave out --data-dir).";
        return 1;
    }

    std::unique_ptr<vt::storage::AsyncWriter> writer;
    std::unique_ptr<vt::storage::SqlPosSink> sink;
    if (havePosStore) {
        writer = std::make_unique<vt::storage::AsyncWriter>(dbPath);
        sink = std::make_unique<vt::storage::SqlPosSink>(*writer);
    }
    vt::app::PosService pos(std::move(*posData), sink.get());
    vt::app::PosShared *shared = pos.shared();
    shared->imageCacheDir = QDir(dataDirOf(cli, o)).filePath(u"cache/store-images"_s);
    // Running: a marker left behind means ViewTouch didn't close normally last
    // time (a crash, the power went off): kept for Store Check.
    // (Removed whichever way this returns; only a crash leaves it.)
    struct RunningMarker {
        QString file;
        ~RunningMarker() { if (!file.isEmpty()) QFile::remove(file); }
    } runningMarker;
    if (havePosStore) {
        const QString running = dbPath + u".running"_s;
        if (QFile::exists(running))
            shared->noteProblem("crash", QFileInfo(running).lastModified().toString(u"ddd h:mm AP"_s).toStdString());
        QFile marker(running);
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
            runningMarker.file = running;
        }
    }
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
    // The builds this store has run: this one, the first time it starts here.
    {
        const vt::net::AppBuild me = vt::net::appBuild();
        auto &runs = shared->settings.builds;
        if (runs.empty() || runs.back().number != me.number || runs.back().commit != me.commit.toStdString()) {
            runs.push_back({me.version.toStdString(), me.number, me.commit.toStdString(),
                            QDateTime::currentMSecsSinceEpoch()});
            if (runs.size() > 50)
                runs.erase(runs.begin(), runs.end() - 50);
            shared->saveSettings();
        }
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
        shared->customerTrail = [dbPath, &writer](const std::string &id, const std::string &digits) {
            if (writer)
                writer->flush();   // today's saved first
            return vt::storage::customerTrail(dbPath, id, digits);
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
            pos.shared()->noteProblem("database", {}, error.toStdString());
        });
        QObject::connect(writer.get(), &vt::storage::AsyncWriter::rowSetAside, &pos,
                         [&pos, w = writer.get()](const QString &table, const QString &key, const QString &error) {
            say(pos, QCoreApplication::translate("main", "One change could not be saved and was kept in %1: %2")
                         .arg(w->setAsidePath(), error));
            pos.shared()->noteProblem("database", {}, (table + u' ' + key + u": "_s + error).toStdString());
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
            if (!ok) {
                say(pos, QCoreApplication::translate("main", "The database backup failed: %1").arg(error));
                shared->noteProblem("backup", {}, error.toStdString());
            } else if (!copyOk) {
                say(pos, QCoreApplication::translate("main", "The backup worked, but %1").arg(copy));
                shared->noteProblem("backupCopy", {}, copy.toStdString());
            }
        });
        QObject::connect(backups.get(), &vt::storage::BackupScheduler::checked, &pos,
                         [shared](const QString &drill, const QString &damage) {
            if (!drill.isEmpty())
                shared->noteProblem("drill", {}, drill.toStdString());
            if (!damage.isEmpty())
                shared->noteProblem("integrity", {}, damage.toStdString());
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
    // Once a day: is there a newer ViewTouch on the release page? (Store Check
    // says so; the store computer's home screen installs it.)
    QNetworkAccessManager updateCheck;
    QTimer updateTimer;
    {
        // This computer's choice, else the page this ViewTouch was built for
        // (packaging/release-channel.conf; a fork sets its own). None: no check.
        QString base;
        for (const QString &path : {u"/etc/viewtouch/updates.conf"_s, u"/usr/share/vtmodern/release-channel.conf"_s}) {
            QFile conf(path);
            if (!conf.open(QIODevice::ReadOnly))
                continue;
            bool said = false;
            for (const QByteArray &line : conf.readAll().split('\n'))
                if (line.startsWith("url=")) {
                    said = true;
                    const QString url = QString::fromUtf8(line.mid(4)).trimmed();
                    base = url.startsWith(u"https://"_s) ? url : QString();
                }
            if (said)
                break;
        }
        while (base.endsWith(u'/'))
            base.chop(1);
        const auto check = [&updateCheck, shared, base] {
            if (base.isEmpty())
                return;
            QNetworkRequest request(QUrl(base + u"/latest.json"_s));
            request.setTransferTimeout(30000);
            QNetworkReply *reply = updateCheck.get(request);
            QObject::connect(reply, &QNetworkReply::finished, shared, [reply, shared] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                    return;   // offline: next time
                const QString version = QJsonDocument::fromJson(reply->readAll()).object().value(u"version").toString();
                const int build = version.section(u'.', 2, 2).toInt();
                const QString newer = build > vt::net::appBuild().number ? version : QString();
                if (newer != shared->newVersion) {
                    shared->newVersion = newer;
                    emit shared->dayChanged();
                }
            });
        };
        updateTimer.setInterval(24 * 3600 * 1000);
        QObject::connect(&updateTimer, &QTimer::timeout, &updateCheck, check);
        updateTimer.start();
        QTimer::singleShot(2 * 60 * 1000, &updateCheck, check);   // soon after starting
    }
    QNetworkAccessManager texting;
    // A text that couldn't go out (the network or the internet down) waits and
    // is tried again every minute, for a day; the texting service saying no
    // (a wrong number) is told at once instead.
    struct WaitingText { QString phone, message; qint64 since = 0; };
    QList<WaitingText> waitingTexts;
    QTimer textRetry;
    textRetry.setInterval(60'000);
    std::function<void(const QString &, const QString &, qint64)> postText;
    postText = [shared, &texting, &pos, &waitingTexts, &textRetry, &postText](const QString &phone, const QString &message,
                                                                             qint64 since) {
        const QUrl url(QString::fromStdString(shared->settings.textWebhook));
        if (!url.isValid() || url.scheme() != u"https")   // guests' phone numbers: encrypted only
            return;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
        request.setTransferTimeout(15000);
        QNetworkReply *reply = texting.post(request, QJsonDocument(QJsonObject{{u"to"_s, phone}, {u"message"_s, message}})
                                                         .toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, &pos, [reply, phone, message, since, &pos, &waitingTexts, &textRetry] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                return;
            const bool answered = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
            const bool young = QDateTime::currentMSecsSinceEpoch() - since < 24LL * 3600 * 1000;
            if (answered || !young) {
                say(pos, QCoreApplication::translate("main", "The text to %1 didn't go out: %2").arg(phone, reply->errorString()));
                return;
            }
            if (waitingTexts.isEmpty())
                say(pos, QCoreApplication::translate("main", "The text to %1 waits for the internet; it goes by itself.").arg(phone));
            waitingTexts.append({phone, message, since});
            textRetry.start();
        });
    };
    QObject::connect(&textRetry, &QTimer::timeout, &pos, [&waitingTexts, &textRetry, &postText] {
        const QList<WaitingText> again = std::exchange(waitingTexts, {});
        if (again.isEmpty())
            textRetry.stop();
        for (const WaitingText &t : again)
            postText(t.phone, t.message, t.since);
    });
    shared->sendText = [&postText](const QString &phone, const QString &message) {
        postText(phone, message, QDateTime::currentMSecsSinceEpoch());
    };

    // Factory reset from the Manager page: back up now, then leave; main
    // deletes the database once it is closed and starts again.
    if (havePosStore) {
        shared->requestFactoryReset = [&cli, &o, &writer](bool empty) {
            if (writer)
                writer->flush();
            QString error;
            const QString target = backupBeforeReset(cli, o, &error);
            if (target.isEmpty()) {
                qWarning().noquote() << "Factory reset: the backup failed:" << error;
                return false;
            }
            if (!empty)
                QFile::remove(startEmptyMarker(cli, o));
            else if (!markStartEmpty(cli, o)) {
                qWarning().noquote() << "Factory reset: couldn't mark it to start empty; nothing was reset.";
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
    if (!cli.isSet(o.stress) && (cli.isSet(o.serve) || cli.isSet(o.headless) || gServeHeadless)) {
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
    }
    // This computer's command line: `vtmodern --pairing-code` (the first
    // terminal of a server without a screen gets paired this way; serving
    // only) and `vtmodern --maintainer-pin` (any store).
    control = std::make_unique<QLocalServer>();
    control->setSocketOptions(QLocalServer::UserAccessOption);
    const QString controlPath = controlSocketOf(cli, o);
    QLocalServer::removeServer(controlPath);   // left over from a crash (we hold the lock)
    if (!control->listen(controlPath))
        qWarning().noquote() << "No local control socket:" << control->errorString();
    QObject::connect(control.get(), &QLocalServer::newConnection, control.get(), [&control, &server, shared] {
        while (QLocalSocket *s = control->nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
            QObject::connect(s, &QLocalSocket::readyRead, s, [s, shared, &server] {
                if (!s->canReadLine())
                    return;
                const QByteArray line = s->readLine().trimmed();
                // The maintainer's code, set from this computer's command line.
                if (line.startsWith("maintainer ")) {
                    const QString pin = QString::fromLatin1(line.mid(11)).trimmed();
                    shared->setMaintainerPin(pin == u"off" ? QString() : pin);
                    qInfo().noquote() << (pin == u"off" ? "Maintainer code removed" : "Maintainer code set")
                                      << "from this machine's command line";
                    s->write("{\"ok\":true}\n");
                    s->disconnectFromServer();
                    return;
                }
                if (line == "pair" && server) {
                    const QString code = shared->startPairing(true);   // on this computer: may be the standby
                    const QString until = vt::i18n::timeText(
                        QDateTime::fromMSecsSinceEpoch(shared->pairing->expires).time());
                    qInfo().noquote() << "Pairing opened from this machine's command line";
                    s->write(QJsonDocument(QJsonObject{{u"code"_s, code}, {u"until"_s, until}})
                                 .toJson(QJsonDocument::Compact) + '\n');
                }
                s->disconnectFromServer();
            });
        }
    });
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
                           {u"build"_s, vt::net::buildText(me.version, me.number, me.commit, me.modified)}, {u"buildNumber"_s, me.number},
                           {u"updates"_s, updates}, {u"updatesDir"_s, server ? server->updatesDir() : QString()},
                           {u"platform"_s, me.platform}, {u"qt"_s, QString::fromLatin1(qVersion())},
                           {u"database"_s, aboutDb}, {u"databaseBytes"_s, QFileInfo(aboutDb).size()},
                           {u"backups"_s, aboutBackups}, {u"startedAt"_s, startedAt},
                           {u"history"_s, vt::net::appHistory()},
                           {u"diskFree"_s, double(QStorageInfo(QFileInfo(aboutDb).absolutePath()).bytesAvailable())},
                           {u"lastBackup"_s, [&] {
                                const QFileInfoList files = QDir(aboutBackups).entryInfoList(QDir::Files, QDir::Time);
                                return files.isEmpty() ? qint64(0) : files.first().lastModified().toMSecsSinceEpoch();
                            }()}};
    };
    if (server) {
        QObject::connect(server.get(), &vt::net::PosServer::terminalsChanged, shared, &vt::app::PosShared::networkChanged);
        QObject::connect(server.get(), &vt::net::PosServer::standbyChanged, shared, &vt::app::PosShared::networkChanged);
        // The standby gone: kept for Store Check (it was keeping a copy).
        auto standbyThere = std::make_shared<bool>(false);
        QObject::connect(server.get(), &vt::net::PosServer::standbyChanged, shared, [raw = server.get(), shared, standbyThere] {
            bool now = false;
            for (const QVariant &c : raw->connections())
                now = now || c.toMap().value(u"standby"_s).toBool();
            if (*standbyThere && !now)
                shared->noteProblem("standby", {});
            *standbyThere = now;
        });
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
    // The store's style (Store Settings -> Screens -> Style): its theme on
    // every screen, now and whenever it's changed.
    const auto applyStyle = [&hub, shared] {
        const QString id = QString::fromStdString(shared->settings.style);
        QFile f(id == u"classic" ? u":/seed/theme.json"_s : u":/seed/themes/"_s + id + u".json"_s);
        if (!f.open(QIODevice::ReadOnly))
            return;
        const auto builtIn = vt::layout::Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object());
        if (vt::layout::Layout::styleOf(hub.layout().theme) == id) {
            if (hub.refreshStyle(builtIn))   // a newer starter theme for it
                qInfo().noquote() << "Style" << id << "updated";
        } else if (hub.useStyle(id, builtIn, vt::layout::Layout::stylePages(id))) {
            qInfo().noquote() << "Style:" << id;
        }
    };
    applyStyle();
    QObject::connect(shared, &vt::app::PosShared::adminChanged, &hub, applyStyle);

    // A self-order kiosk for guests: by option, or this terminal's setting.
    if (cli.isSet(o.selfOrder) || pos.screenMode() == u"selfOrder")
        pos.enableSelfOrder();
    if (cli.isSet(o.login) && !pos.loginWithPin(cli.value(o.login))) {
        qCritical().noquote() << "That PIN is not recognized.";
        return 1;
    }
    if (cli.isSet(o.stress)) {
        shared->settings.setupDone = true;   // no setup guide over the dinner
        // The watching manager stays logged in (nobody touches this screen),
        // and the screen stays on: the pages keep going around.
        shared->settings.autoLogoutMinutes = 0;
        shared->settings.screenSaverMinutes = 0;
        pos.loginWithPin(u"1234"_s);         // the manager watches
    }
    const auto engine = showUi(cli, o, controller);
    if (!engine)
        return 1;

    // The stress test: pretend staff on this store while the screen runs.
    std::unique_ptr<vt::net::StressRunner> stress;
    if (cli.isSet(o.stress)) {
        vt::net::StressOptions so;
        so.minutes = cli.value(o.stressMinutes).toInt();
        so.speed = cli.value(o.stressSpeed).toDouble();
        so.servers = cli.value(o.stressServers).toInt();
        so.hours = cli.isSet(o.stressHours) ? cli.value(o.stressHours).toDouble() : 0;
        so.load = cli.value(o.stressLoad).toInt();
        so.dbPath = dbPath;
        so.writer = writer.get();
        vt::net::StressRunner::tablesOn(*layout, so.tables, so.barSeats);
        stress = std::make_unique<vt::net::StressRunner>(shared, &hub, so);
        QString error;
        if (!stress->start(&error)) {
            qCritical().noquote() << "The stress test could not start:" << error;
            return 1;
        }
        qInfo().noquote() << "Stress test: the store is in" << dataDirOf(cli, o);
        if (auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().value(0))) {
            window->setProperty("stress", QVariant::fromValue(static_cast<QObject *>(stress.get())));
            vt::net::StressRunner *raw = stress.get();
            QObject::connect(window, &QQuickWindow::frameSwapped, raw, [raw] { raw->frameDrawn(); });
        }
        const QString reportFile = QDir(dataDirOf(cli, o)).filePath(u"stress-report.txt"_s);
        const bool exitAfter = cli.isSet(o.stressExit);
        QObject::connect(stress.get(), &vt::net::StressRunner::done, qApp, [&stress, reportFile, exitAfter](bool passed) {
            QFile f(reportFile);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                f.write(stress->report().toUtf8());
            qInfo().noquote() << stress->report();
            qInfo().noquote() << "Report saved:" << reportFile;
            if (exitAfter)
                QTimer::singleShot(500, qApp, [passed] { QCoreApplication::exit(passed ? 0 : 3); });
        });
    }
    return qApp->exec();
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


// Environment that must be set before the application starts:
// - a server without a screen (and --help, --backup...) needs no display, so
//   it gets the offscreen platform unless the user chose one;
// - a kiosk must exit when its compositor dies (systemd then restarts both)
//   instead of waiting to reconnect, as KDE asks Qt apps to.
void prepareEnvironment(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        const QByteArrayView arg(argv[i]);
        const bool command = arg == "--backup" || arg == "--snapshot" || arg.startsWith("--snapshot=") || arg == "--restore" || arg.startsWith("--restore=")
                             || arg == "--factory-reset" || arg == "--demo-data"
                             || arg == "--pairing-code" || arg == "--maintainer-pin" || arg.startsWith("--maintainer-pin=") || arg == "-h"
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
#ifdef Q_OS_UNIX
    // What ViewTouch writes (the database, backups, crash reports, exports)
    // is its account's only, whatever folder it's in: a store on a shared
    // desktop computer isn't readable by the computer's other users.
    ::umask(0077);
#endif
    prepareEnvironment(argc, argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationVersion(vt::net::appBuild().version);   // 0.9.1283
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
        o.port, o.listen, o.headless, o.connect, o.pair, o.terminal, o.kiosk, o.selfOrder, o.touchKeyboard, o.customerDisplay, o.factoryReset, o.empty, o.demoData, o.standby, o.pairOnly, o.windowed, o.screen, o.login, o.page, o.afterLogin, o.station, o.edit, o.select, o.size,
        o.screenshot, o.backupDir, o.backupKeep, o.backupEvery, o.backup, o.snapshot, o.restore, o.pairingCode, o.maintainerPin, o.exportDir,
        o.stress, o.stressMinutes, o.stressSpeed, o.stressServers, o.stressHours, o.stressLoad, o.stressExit};
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
    if (args.isSet(o.station)) {
        installCrashHandler(dataDirOf(args, o));
        return runStation(args, o);
    }
    // A stress test never touches a real store: a folder of its own.
    if (args.isSet(o.stress))
        args.setDefault(o.dataDir, QDir(Options::defaultDataDir()).filePath(
                                       u"stress-tests/"_s + QDateTime::currentDateTime().toString(u"yyyy-MM-dd-HHmmss"_s)));
    if (args.isSet(o.factoryReset)) {
        const int code = runFactoryReset(args, o);
        if (code != 0 || !args.isSet(o.demoData))
            return code;
    }
    if (args.isSet(o.demoData))
        return runDemoData(args, o);
    if (args.isSet(o.backup))
        return runBackup(args, o);
    if (args.isSet(o.snapshot))
        return runSnapshot(args, o);
    if (args.isSet(o.restore))
        return runRestore(args, o);
    if (args.isSet(o.pairingCode))
        return runPairingCode(args, o);
    if (args.isSet(o.maintainerPin))
        return runMaintainerPin(args, o);
    if (!args.isSet(o.stress) && !args.isSet(o.screenshot)) {
        installCrashHandler(dataDirOf(args, o));
        markHealthyLater(dataDirOf(args, o));
    }
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
    if (qEnvironmentVariableIsSet("INVOCATION_ID") || qEnvironmentVariableIsSet("VTM_STATION"))
        return kFactoryReset;
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    return 0;
}
