#include "stationcontroller.hh"

#include <QDateTime>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

// ViewTouch's exit statuses (main.cpp): a manager closed it; a factory reset
// that wants to be started again.
constexpr int kClosed = 64;
constexpr int kFactoryReset = 65;
// Crashes this close together: stop starting it again.
constexpr int kCrashesToGiveUp = 3;
constexpr int kCrashWindowSeconds = 180;
constexpr int kRestartSeconds = 5;

QString helperPath()
{
    const QString env = qEnvironmentVariable("VTM_STATION_HELPER");
    return env.isEmpty() ? u"/usr/libexec/vtmodern/vtmodern-device"_s : env;
}

// One line of `nmcli -t`: fields split at ':' (a "\:" is a colon in a value).
QStringList nmFields(const QString &line)
{
    QStringList out{QString()};
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == u'\\' && i + 1 < line.size()) {
            out.last() += line.at(++i);
        } else if (c == u':') {
            out << QString();
        } else {
            out.last() += c;
        }
    }
    return out;
}

QString runQuick(const QString &program, const QStringList &args, int ms = 4000)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(ms))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

QString hashPin(const QString &salt, const QString &pin)
{
    return QString::fromLatin1(
        QCryptographicHash::hash((salt + u':' + pin).toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace

StationController::StationController(QString dataDir, QString configFile, bool kiosk, QObject *parent)
    : QObject(parent), dataDir_(std::move(dataDir)), configFile_(std::move(configFile)), kiosk_(kiosk)
{
    QDir().mkpath(dataDir_);
    load();
    restartTimer_.setInterval(1000);
    connect(&restartTimer_, &QTimer::timeout, this, [this] {
        if (--restartIn_ <= 0) {
            restartTimer_.stop();
            startPos();
        } else {
            emit posChanged();
        }
    });
    networkTimer_.setInterval(10000);
    connect(&networkTimer_, &QTimer::timeout, this, &StationController::refreshNetwork);
    connect(&networkTimer_, &QTimer::timeout, this, &StationController::readTimeZone);
    networkTimer_.start();
    connect(this, &StationController::finished, this, [this](const QString &action, bool ok, const QString &) {
        if (action == u"timezone")
            readTimeZone();
        else if (action == u"ssh")
            readRemoteSupport();
        else if (action == u"wifi")
            refreshNetwork();
        else if (action == u"forget" && ok) {
            role_.clear();
            QFile::remove(QDir(dataDir_).filePath(u"terminal.json"_s));   // its pairing with the store
            save();
            emit configChanged();
        }
    });
    readTimeZone();
    readRemoteSupport();
    refreshNetwork();
    // What's at a glance: now, every minute, and after a backup or a restore.
    connect(&networkTimer_, &QTimer::timeout, this, [this, n = 0]() mutable {
        if (++n % 6 == 0)
            refreshStatus();
    });
    connect(this, &StationController::finished, this, [this](const QString &action, bool, const QString &) {
        if (action == u"backup" || action == u"restore")
            refreshStatus();
        else if (action == u"brightness")
            emit displayChanged();
    });
    connect(this, &StationController::posChanged, this, &StationController::refreshStatus);
    connect(this, &StationController::networkChanged, this, &StationController::refreshStatus);
    refreshStatus();
    if (rotation_ != 0)
        QTimer::singleShot(1000, this, &StationController::applyRotation);   // the screen as it was turned
}

StationController::~StationController()
{
    stopPos();
}

QString StationController::version() const
{
    return QCoreApplication::applicationVersion();
}

QString StationController::model() const
{
    QFile f(u"/proc/device-tree/model"_s);
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).remove(QChar(0)).trimmed();
    return QSysInfo::prettyProductName();
}

bool StationController::canManage() const
{
    return QFileInfo(helperPath()).isExecutable();
}

// --- what this computer is, and its PIN --------------------------------------------

void StationController::load()
{
    QFile f(QDir(dataDir_).filePath(u"station.json"_s));
    QJsonObject o;
    if (f.open(QIODevice::ReadOnly))
        o = QJsonDocument::fromJson(f.readAll()).object();
    role_ = o.value(u"role").toString();
    name_ = o.value(u"name").toString(QSysInfo::machineHostName());
    pinHash_ = o.value(u"pinHash").toString();
    pinSalt_ = o.value(u"pinSalt").toString();
    rotation_ = o.value(u"rotation").toInt();
    if (rotation_ != 90 && rotation_ != 180 && rotation_ != 270)
        rotation_ = 0;
    lockPower_ = o.value(u"lockPower").toBool();
}

bool StationController::save()
{
    QFile f(QDir(dataDir_).filePath(u"station.json"_s));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QJsonObject o{{u"role"_s, role_}, {u"name"_s, name_}, {u"pinHash"_s, pinHash_}, {u"pinSalt"_s, pinSalt_},
                        {u"rotation"_s, rotation_}, {u"lockPower"_s, lockPower_}};
    return f.write(QJsonDocument(o).toJson()) > 0;
}

bool StationController::checkPin(const QString &pin) const
{
    if (pinHash_.isEmpty())
        return true;
    if (pinWait() > 0)
        return false;   // guessing: wait first
    if (hashPin(pinSalt_, pin) == pinHash_) {
        wrongPins_ = 0;
        return true;
    }
    if (++wrongPins_ >= 5)
        pinWaitUntil_ = QDateTime::currentMSecsSinceEpoch()
                        + std::min<qint64>(15 * 60'000, 60'000LL << std::min(wrongPins_ - 5, 4));
    return false;
}

int StationController::pinWait() const
{
    return int(std::max<qint64>(0, pinWaitUntil_ - QDateTime::currentMSecsSinceEpoch()) / 1000);
}

bool StationController::setPin(const QString &pin)
{
    static const QRegularExpression digits(u"^[0-9]{4,8}$"_s);
    if (!digits.match(pin).hasMatch())
        return false;
    pinSalt_ = QString::number(QRandomGenerator::system()->generate64(), 36);
    pinHash_ = hashPin(pinSalt_, pin);
    save();
    emit configChanged();
    return true;
}

bool StationController::setMaintainerCode(const QString &pin)
{
    if (!setPin(pin))
        return false;
    if (role_ == u"store")
        handToStore(pin);
    else
        emit finished(u"maintainer"_s, true, tr("This computer's settings now ask for the new code."));
    return true;
}

void StationController::handToStore(const QString &pin, int triesLeft)
{
    // vtmodern --maintainer-pin -: the running store takes it (the code on its
    // input, not on a command line). Starting up, or not up yet: again shortly.
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(p, &QProcess::finished, this, [this, p, pin, triesLeft](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        if (status == QProcess::NormalExit && code == 0) {
            emit finished(u"maintainer"_s, true, tr("The maintainer code is set, for this computer's settings and the store."));
            return;
        }
        if (triesLeft <= 1) {
            emit finished(u"maintainer"_s, false, tr("The store didn't take the maintainer code: set it again from Maintainer Code."));
            return;
        }
        QTimer::singleShot(2000, this, [this, pin, triesLeft] { handToStore(pin, triesLeft - 1); });
    });
    p->start(QCoreApplication::applicationFilePath(), storeArgs() << u"--maintainer-pin"_s << u"-"_s);
    p->write(pin.toLatin1() + '\n');
    p->closeWriteChannel();
}

void StationController::setup(const QString &role, const QString &name, bool guestDisplay, const QString &pin)
{
    static const QStringList roles{u"store"_s, u"terminal"_s, u"kitchen"_s, u"selforder"_s, u"timeclock"_s};
    static const QRegularExpression hostName(u"^[A-Za-z0-9][A-Za-z0-9-]{0,62}$"_s);
    if (!roles.contains(role)) {
        emit finished(u"setup"_s, false, tr("Choose what this computer is."));
        return;
    }
    const QString cleanName = name.trimmed();
    if (!hostName.match(cleanName).hasMatch()) {
        emit finished(u"setup"_s, false, tr("Use letters, numbers and dashes for the name (like Register-2)."));
        return;
    }
    if (!pin.isEmpty() && !setPin(pin)) {
        emit finished(u"setup"_s, false, tr("The PIN is 4 to 8 digits."));
        return;
    }
    auto done = [this, role, cleanName, pin] {
        role_ = role;
        name_ = cleanName;
        save();
        emit configChanged();
        startPos(role == u"store" ? u"setup"_s : QString());
        // The main register: the same code is the store's maintainer code.
        if (role == u"store" && !pin.isEmpty())
            handToStore(pin);
    };
    if (!canManage()) {   // trying it out on a desktop
        done();
        emit finished(u"setup"_s, true, QString());
        return;
    }
    auto *once = new QObject(this);
    connect(this, &StationController::finished, once, [once, done](const QString &action, bool ok, const QString &) {
        if (action != u"setup")
            return;
        once->deleteLater();
        if (ok)
            done();
    });
    QStringList args{role, cleanName};
    if (guestDisplay)
        args << u"--guest-display"_s;
    run(u"setup"_s, args);
}

void StationController::startOver()
{
    stopPos();
    if (!canManage()) {
        emit finished(u"forget"_s, true, QString());
        return;
    }
    run(u"forget"_s, {});
}

// --- ViewTouch itself ----------------------------------------------------------------

void StationController::startPos(const QString &afterLogin)
{
    restartTimer_.stop();
    if (pos_ && pos_->state() != QProcess::NotRunning)
        return;
    if (role_.isEmpty())
        return;   // the first-time setup comes first
    delete pos_;
    pos_ = new QProcess(this);
    pos_->setProcessChannelMode(QProcess::ForwardedChannels);   // its messages go to the journal too
    // A factory reset from its Manager page exits for this to start it again.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"VTM_STATION"_s, u"1"_s);
    pos_->setProcessEnvironment(env);
    QStringList args{kiosk_ ? u"--kiosk"_s : u"--windowed"_s};
    args << storeArgs();
    if (!afterLogin.isEmpty())
        args << u"--after-login"_s << afterLogin;
    connect(pos_, &QProcess::finished, this, &StationController::posExited);
    connect(pos_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            posExited(-1, QProcess::CrashExit);
    });
    pos_->start(QCoreApplication::applicationFilePath(), args);
    setPosState(u"running"_s);
}

QStringList StationController::storeArgs() const
{
    QStringList args{u"--data-dir"_s, dataDir_};
    if (!configFile_.isEmpty())
        args << u"--config"_s << configFile_;
    return args;
}

void StationController::stopPos()
{
    restartTimer_.stop();
    if (!pos_ || pos_->state() == QProcess::NotRunning)
        return;
    disconnect(pos_, nullptr, this, nullptr);
    pos_->terminate();   // ViewTouch saves and closes on SIGTERM
    if (!pos_->waitForFinished(10000)) {
        pos_->kill();
        pos_->waitForFinished(2000);
    }
    setPosState(u"stopped"_s);
}

void StationController::posExited(int code, QProcess::ExitStatus status)
{
    if (status == QProcess::NormalExit && code == kFactoryReset) {
        startPos();   // erased: start fresh
        return;
    }
    if (status == QProcess::NormalExit && (code == kClosed || code == 0)) {
        setPosState(u"stopped"_s);
        return;
    }
    // It stopped by itself: start it again, unless it keeps doing that.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    crashes_.append(now);
    crashes_.erase(std::remove_if(crashes_.begin(), crashes_.end(),
                                  [&](const QDateTime &t) { return t.secsTo(now) > kCrashWindowSeconds; }),
                   crashes_.end());
    if (crashes_.size() >= kCrashesToGiveUp) {
        setPosState(u"gaveUp"_s);
        return;
    }
    restartIn_ = kRestartSeconds;
    restartTimer_.start();
    setPosState(u"crashed"_s);
}

void StationController::stayHome()
{
    restartTimer_.stop();
    setPosState(u"stopped"_s);
}

void StationController::setPosState(const QString &state)
{
    posState_ = state;
    emit posChanged();
}

// --- the network -----------------------------------------------------------------------

void StationController::refreshNetwork()
{
    auto *p = new QProcess(this);
    // Read nmcli's answer; without nmcli (no NetworkManager), just the address.
    const auto read = [this, p] {
        p->deleteLater();
        bool wired = false, wifiAvailable = false;
        QString wifi;
        for (const QString &line : QString::fromUtf8(p->readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts)) {
            const QStringList f = nmFields(line);   // device, type, state, connection
            if (f.size() < 4)
                continue;
            // "connected (externally)": set up outside NetworkManager, still connected.
            if (f[1] == u"ethernet" && f[2].startsWith(u"connected"))
                wired = true;
            if (f[1] == u"wifi") {
                wifiAvailable = wifiAvailable || f[2] != u"unavailable";
                if (f[2].startsWith(u"connected"))
                    wifi = f[3];
            }
        }
        QString address;
        for (const QNetworkInterface &i : QNetworkInterface::allInterfaces()) {
            if (!(i.flags() & QNetworkInterface::IsUp) || (i.flags() & QNetworkInterface::IsLoopBack))
                continue;
            for (const QNetworkAddressEntry &e : i.addressEntries()) {
                if (e.ip().protocol() != QAbstractSocket::IPv4Protocol)
                    continue;
                if (address.isEmpty())
                    address = e.ip().toString();
                // A cable NetworkManager doesn't manage (Debian's own network
                // settings, a server install): connected all the same.
                if (i.type() == QNetworkInterface::Ethernet && wifi.isEmpty())
                    wired = true;
            }
        }
        network_ = {{u"wired"_s, wired}, {u"wifi"_s, wifi}, {u"address"_s, address},
                    {u"online"_s, wired || !wifi.isEmpty() || !address.isEmpty()}, {u"wifiAvailable"_s, wifiAvailable}};
        emit networkChanged();
    };
    connect(p, &QProcess::finished, this, read);
    connect(p, &QProcess::errorOccurred, this, [read](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            read();
    });
    p->start(u"nmcli"_s, {u"-t"_s, u"-f"_s, u"DEVICE,TYPE,STATE,CONNECTION"_s, u"device"_s});
}

void StationController::scanWifi()
{
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p] {
        p->deleteLater();
        QHash<QString, QVariantMap> best;
        for (const QString &line : QString::fromUtf8(p->readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts)) {
            const QStringList f = nmFields(line);   // in use, name, signal, security
            if (f.size() < 4 || f[1].isEmpty())
                continue;
            const int signal = f[2].toInt();
            if (best.contains(f[1]) && best[f[1]][u"signal"_s].toInt() >= signal && f[0] != u"*")
                continue;
            best[f[1]] = {{u"name"_s, f[1]}, {u"signal"_s, signal},
                          {u"secure"_s, !f[3].isEmpty() && f[3] != u"--"}, {u"connected"_s, f[0] == u"*"}};
        }
        QVariantList list;
        for (const QVariantMap &m : std::as_const(best))
            list << m;
        std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap()[u"signal"_s].toInt() > b.toMap()[u"signal"_s].toInt();
        });
        wifi_ = list;
        emit wifiChanged();
    });
    p->start(u"nmcli"_s, {u"-t"_s, u"-f"_s, u"IN-USE,SSID,SIGNAL,SECURITY"_s, u"device"_s, u"wifi"_s, u"list"_s,
                         u"--rescan"_s, u"yes"_s});
}

void StationController::connectWifi(const QString &name, const QString &password)
{
    // The password goes in on stdin, never on a command line others can see.
    run(u"wifi"_s, {name}, password.toUtf8() + '\n');
}

// --- time ------------------------------------------------------------------------------

QStringList StationController::timeZones() const
{
    QStringList zones = runQuick(u"timedatectl"_s, {u"list-timezones"_s}).split(u'\n', Qt::SkipEmptyParts);
    if (zones.isEmpty())
        for (const QByteArray &z : QTimeZone::availableTimeZoneIds())
            zones << QString::fromLatin1(z);
    return zones;
}

void StationController::setTimeZone(const QString &zone)
{
    run(u"timezone"_s, {zone});
}

void StationController::readTimeZone()
{
    QString zone = runQuick(u"timedatectl"_s, {u"show"_s, u"-p"_s, u"Timezone"_s, u"--value"_s}).trimmed();
    if (zone.isEmpty())
        zone = QString::fromLatin1(QTimeZone::systemTimeZoneId());
    const bool synced = runQuick(u"timedatectl"_s, {u"show"_s, u"-p"_s, u"NTPSynchronized"_s, u"--value"_s}).trimmed() == u"yes";
    if (zone != timeZone_ || synced != timeSynced_) {
        timeZone_ = zone;
        timeSynced_ = synced;
        emit timeZoneChanged();
    }
}

// --- keeping the computer --------------------------------------------------------------

void StationController::eraseStore()
{
    stopPos();
    run(u"erase"_s, storeArgs() << u"--factory-reset"_s, {}, QCoreApplication::applicationFilePath());
}

void StationController::backupNow()
{
    run(u"backup"_s, storeArgs() << u"--backup"_s, {}, QCoreApplication::applicationFilePath());
}

void StationController::runUpdates()
{
    run(u"update"_s, {});
}

void StationController::updateFromUsb()
{
    run(u"update-usb"_s, {});
}

void StationController::checkOnline()
{
    run(u"online-check"_s, {});
}

void StationController::installOnline()
{
    run(u"online-update"_s, {});
}

void StationController::setRemoteSupport(bool on)
{
    run(u"ssh"_s, {on ? u"on"_s : u"off"_s});
}

void StationController::readRemoteSupport()
{
    const bool on = runQuick(u"systemctl"_s, {u"is-active"_s, u"ssh"_s}).trimmed() == u"active";
    if (on != remoteSupport_) {
        remoteSupport_ = on;
        emit remoteSupportChanged();
    }
}

void StationController::restartComputer()
{
    stopPos();
    run(u"reboot"_s, {});
}

void StationController::shutDown()
{
    stopPos();
    run(u"poweroff"_s, {});
}

void StationController::setBusy(bool busy)
{
    if (busy_ != busy) {
        busy_ = busy;
        emit busyChanged();
    }
}

QString StationController::backupDir() const
{
    return QDir(dataDir_).filePath(u"backups"_s);
}

void StationController::refreshStatus()
{
    QVariantMap s;
    QStringList problems;
    // The newest backup on this computer.
    const QFileInfoList backups = QDir(backupDir()).entryInfoList({u"viewtouch-*.db"_s, u"viewtouch-*.vtbak"_s},
                                                                  QDir::Files, QDir::Time);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!backups.isEmpty()) {
        const qint64 at = backups.first().lastModified().toMSecsSinceEpoch();
        const int hours = int((now - at) / 3'600'000);
        s.insert(u"lastBackupAt"_s, at);
        s.insert(u"backupAgeHours"_s, hours);
        if (hours > 36 && (role_ == u"store" || role_ == u"server"))
            problems << tr("No backup in %n day(s)", nullptr, hours / 24);
    } else {
        s.insert(u"lastBackupAt"_s, 0);
        if (role_ == u"store" || role_ == u"server")
            problems << tr("No backup yet");
    }
    // Room left for the store's data (and its backups).
    const QStorageInfo disk(dataDir_);
    if (disk.isValid() && disk.bytesTotal() > 0) {
        const int free = int(100 * disk.bytesAvailable() / disk.bytesTotal());
        s.insert(u"diskFreePercent"_s, free);
        s.insert(u"diskFreeGb"_s, double(disk.bytesAvailable()) / 1e9);
        if (free < 10)
            problems << tr("Only %1% of the disk is free").arg(free);
    }
    // ViewTouch stopping by itself today.
    int today = 0;
    for (const QDateTime &t : crashes_)
        if (t.date() == QDate::currentDate())
            ++today;
    s.insert(u"crashesToday"_s, today);
    if (today > 0)
        problems << tr("ViewTouch stopped by itself %n time(s) today", nullptr, today);
    if (posState_ == u"gaveUp")
        problems << tr("ViewTouch isn't running");
    if (!network_.value(u"online"_s).toBool())
        problems << tr("No network");
    s.insert(u"problems"_s, problems);
    if (s != status_) {
        status_ = s;
        emit statusChanged();
    }
}

QVariantList StationController::backups() const
{
    struct Found {
        QFileInfo file;
        QString where;
    };
    std::vector<Found> found;
    const auto look = [&](const QString &dir, const QString &where) {
        for (const QFileInfo &f : QDir(dir).entryInfoList({u"viewtouch-*.db"_s, u"viewtouch-*.vtbak"_s}, QDir::Files))
            found.push_back({f, where});
    };
    look(backupDir(), tr("This computer"));
    // USB sticks (usbOpen() mounts them, read only): at their top or in a backups folder.
    for (const QFileInfo &stick : QDir(u"/run/vtmodern-usb"_s).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        look(stick.filePath(), tr("USB stick"));
        look(QDir(stick.filePath()).filePath(u"backups"_s), tr("USB stick"));
    }
    std::ranges::sort(found, [](const Found &a, const Found &b) { return a.file.lastModified() > b.file.lastModified(); });
    QVariantList out;
    for (const Found &f : found)
        out.append(QVariantMap{{u"path"_s, f.file.absoluteFilePath()}, {u"name"_s, f.file.fileName()},
                               {u"at"_s, f.file.lastModified().toMSecsSinceEpoch()}, {u"size"_s, f.file.size()},
                               {u"encrypted"_s, f.file.suffix() == u"vtbak"}, {u"where"_s, f.where}});
    return out;
}

void StationController::usbOpen()
{
    run(u"usb-open"_s, {});
}

void StationController::restoreBackup(const QString &path, const QString &password)
{
    // Only one of the backups listed: never a path from elsewhere.
    const QVariantList list = backups();
    if (std::ranges::none_of(list, [&](const QVariant &v) { return v.toMap().value(u"path"_s).toString() == path; })) {
        emit finished(u"restore"_s, false, tr("That backup isn't there any more."));
        return;
    }
    stopPos();
    // Its password on stdin, for an encrypted one this store's key doesn't open.
    run(u"restore"_s, storeArgs() << u"--restore"_s << path, (password + u'\n').toUtf8(),
        QCoreApplication::applicationFilePath());
}

namespace {
// The screen's backlight (a Raspberry Pi's display, a laptop's), if any.
QString backlight()
{
    const QFileInfoList lights = QDir(u"/sys/class/backlight"_s).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    return lights.isEmpty() ? QString() : lights.first().filePath();
}
int readNumber(const QString &file)
{
    QFile f(file);
    return f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll()).trimmed().toInt() : -1;
}
} // namespace

int StationController::brightness() const
{
    const QString dir = backlight();
    if (dir.isEmpty())
        return -1;
    const int max = readNumber(dir + u"/max_brightness"_s), now = readNumber(dir + u"/brightness"_s);
    return max > 0 && now >= 0 ? int(std::lround(100.0 * now / max)) : -1;
}

void StationController::setBrightness(int percent)
{
    run(u"brightness"_s, {QString::number(std::clamp(percent, 5, 100))});   // never quite dark
}

void StationController::setRotation(int degrees)
{
    if (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270)
        return;
    rotation_ = degrees;
    save();
    emit displayChanged();
    applyRotation();
}

// The screen turned, by the compositor it runs in (cage, through wlr-randr);
// touch turns with it.
void StationController::applyRotation()
{
    const QString outputs = runQuick(u"wlr-randr"_s, {});
    QString output;
    for (const QString &line : outputs.split(u'\n'))
        if (!line.isEmpty() && !line.front().isSpace()) {
            output = line.section(u' ', 0, 0);
            break;
        }
    if (output.isEmpty()) {
        emit finished(u"rotate"_s, false, tr("This screen can't be turned from here."));
        return;
    }
    const QString transform = rotation_ == 0 ? u"normal"_s : QString::number(rotation_);
    run(u"rotate"_s, {u"--output"_s, output, u"--transform"_s, transform}, {}, u"wlr-randr"_s);
}

void StationController::setLockPower(bool on)
{
    lockPower_ = on;
    save();
    emit configChanged();
}

void StationController::saveLogs()
{
    run(u"logs-usb"_s, {});
}

void StationController::run(const QString &action, const QStringList &args, const QByteArray &input,
                            const QString &program)
{
    if (busy_) {
        emit finished(action, false, tr("Wait for the last one to finish."));
        return;
    }
    if (program.isEmpty() && !canManage()) {
        emit finished(action, false, tr("This needs the ViewTouch computer setup (vtmodern-lockdown)."));
        return;
    }
    output_.clear();
    emit outputChanged();
    setBusy(true);
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::readyRead, this, [this, p] {
        output_ += QString::fromUtf8(p->readAll());
        if (output_.size() > 20000)
            output_ = output_.right(20000);
        emit outputChanged();
    });
    connect(p, &QProcess::finished, this, [this, p, action](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        output_ += QString::fromUtf8(p->readAll());
        setBusy(false);
        const QStringList lines = output_.trimmed().split(u'\n', Qt::SkipEmptyParts);
        emit finished(action, status == QProcess::NormalExit && code == 0, lines.isEmpty() ? QString() : lines.last());
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, action](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        p->deleteLater();
        setBusy(false);
        emit finished(action, false, p->errorString());
    });
    if (program.isEmpty())
        p->start(u"sudo"_s, QStringList{u"-n"_s, helperPath(), action} + args);
    else
        p->start(program, args);
    if (!input.isEmpty()) {
        p->write(input);
        p->closeWriteChannel();
    }
}
