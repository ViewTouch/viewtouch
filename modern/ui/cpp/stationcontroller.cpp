#include "stationcontroller.hh"

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
}

bool StationController::save()
{
    QFile f(QDir(dataDir_).filePath(u"station.json"_s));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QJsonObject o{{u"role"_s, role_}, {u"name"_s, name_}, {u"pinHash"_s, pinHash_}, {u"pinSalt"_s, pinSalt_}};
    return f.write(QJsonDocument(o).toJson()) > 0;
}

bool StationController::checkPin(const QString &pin) const
{
    return pinHash_.isEmpty() || hashPin(pinSalt_, pin) == pinHash_;
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
    auto done = [this, role, cleanName] {
        role_ = role;
        name_ = cleanName;
        save();
        emit configChanged();
        startPos(role == u"store" ? u"setup"_s : QString());
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
    connect(p, &QProcess::finished, this, [this, p] {
        p->deleteLater();
        bool wired = false, wifiAvailable = false;
        QString wifi;
        for (const QString &line : QString::fromUtf8(p->readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts)) {
            const QStringList f = nmFields(line);   // device, type, state, connection
            if (f.size() < 4)
                continue;
            if (f[1] == u"ethernet" && f[2] == u"connected")
                wired = true;
            if (f[1] == u"wifi") {
                wifiAvailable = wifiAvailable || f[2] != u"unavailable";
                if (f[2] == u"connected")
                    wifi = f[3];
            }
        }
        QString address;
        for (const QNetworkInterface &i : QNetworkInterface::allInterfaces()) {
            if (!(i.flags() & QNetworkInterface::IsUp) || (i.flags() & QNetworkInterface::IsLoopBack))
                continue;
            for (const QNetworkAddressEntry &e : i.addressEntries())
                if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && address.isEmpty())
                    address = e.ip().toString();
        }
        network_ = {{u"wired"_s, wired}, {u"wifi"_s, wifi}, {u"address"_s, address},
                    {u"online"_s, wired || !wifi.isEmpty() || !address.isEmpty()}, {u"wifiAvailable"_s, wifiAvailable}};
        emit networkChanged();
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
