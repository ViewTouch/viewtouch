#pragma once

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>

// The home screen of a computer that is only for ViewTouch (vtmodern
// --station): it starts ViewTouch, starts it again if it stops by itself, and
// has the touch icons for the computer: network, time, updates, restart, shut
// down, reset; on the main register also the store's setup screens. The
// first time, it asks what this computer is.
//
// What needs root (Wi-Fi, the time zone, updates, what this computer is)
// goes through one helper, run with sudo (a rule allows only it):
// /usr/libexec/vtmodern/vtmodern-device.
class StationController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Made by main()")

    // "" (not set up yet), store, terminal, kitchen, selforder, timeclock
    Q_PROPERTY(QString role READ role NOTIFY configChanged)
    Q_PROPERTY(QString computerName READ computerName NOTIFY configChanged)
    Q_PROPERTY(bool hasPin READ hasPin NOTIFY configChanged)
    // stopped | running | crashed (starting again in restartIn s) | gaveUp
    Q_PROPERTY(QString posState READ posState NOTIFY posChanged)
    Q_PROPERTY(int restartIn READ restartIn NOTIFY posChanged)
    // {wired, wifi (its name), address, online, wifiAvailable}
    Q_PROPERTY(QVariantMap network READ network NOTIFY networkChanged)
    // [{name, signal, secure, connected}]
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // What the last helper action said (updates show it as they go).
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString timeZone READ timeZone NOTIFY timeZoneChanged)
    // The clock was set from the internet (and is kept so).
    Q_PROPERTY(bool timeSynced READ timeSynced NOTIFY timeZoneChanged)
    Q_PROPERTY(bool remoteSupport READ remoteSupport NOTIFY remoteSupportChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString model READ model CONSTANT)
    // False when run without the helper (a test on a desktop): those icons say so.
    Q_PROPERTY(bool canManage READ canManage CONSTANT)
    // At a glance, for the top bar: {lastBackupAt (ms, 0: none), backupAgeHours,
    // diskFreePercent, diskFreeGb, crashesToday, problems: [what to look at]}.
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    // The screen's brightness, 0-100 (-1: this screen's can't be set here),
    // and how it's turned (0, 90, 180, 270 degrees).
    Q_PROPERTY(int brightness READ brightness NOTIFY displayChanged)
    Q_PROPERTY(int rotation READ rotation NOTIFY displayChanged)
    // Restart and Shut Down ask for the maintainer's code (staff can't power
    // the computer off in the middle of service).
    Q_PROPERTY(bool lockPower READ lockPower NOTIFY configChanged)

public:
    // ViewTouch is started with this data folder and settings file (--config),
    // full screen with no way out (`kiosk`) or in a window (trying it out).
    StationController(QString dataDir, QString configFile, bool kiosk, QObject *parent = nullptr);
    ~StationController() override;

    QString role() const { return role_; }
    QString computerName() const { return name_; }
    bool hasPin() const { return !pinHash_.isEmpty(); }
    QString posState() const { return posState_; }
    int restartIn() const { return restartIn_; }
    QVariantMap network() const { return network_; }
    QVariantList wifiNetworks() const { return wifi_; }
    bool busy() const { return busy_; }
    QString output() const { return output_; }
    QString timeZone() const { return timeZone_; }
    bool timeSynced() const { return timeSynced_; }
    bool remoteSupport() const { return remoteSupport_; }
    QString version() const;
    QString model() const;
    bool canManage() const;
    QVariantMap status() const { return status_; }
    int brightness() const;
    int rotation() const { return rotation_; }
    bool lockPower() const { return lockPower_; }

    // Start ViewTouch (on `afterLogin` once someone logs in: a page id, or "setup").
    Q_INVOKABLE void startPos(const QString &afterLogin = {});
    // Don't start it again by itself (a crash): stay on the home screen.
    Q_INVOKABLE void stayHome();
    Q_INVOKABLE bool checkPin(const QString &pin) const;
    // After 5 wrong settings PINs in a row: seconds to wait (a minute, doubling to 15).
    Q_INVOKABLE int pinWait() const;
    Q_INVOKABLE bool setPin(const QString &pin);
    // The maintainer's code: this computer's settings PIN, and on the main
    // register the store's maintainer code too (handed to the running store;
    // finished("maintainer") says how it went).
    Q_INVOKABLE bool setMaintainerCode(const QString &pin);

    Q_INVOKABLE void refreshNetwork();
    Q_INVOKABLE void scanWifi();
    Q_INVOKABLE void connectWifi(const QString &name, const QString &password);
    Q_INVOKABLE QStringList timeZones() const;
    Q_INVOKABLE void setTimeZone(const QString &zone);

    // The first-time setup: what this computer is (store, terminal, kitchen,
    // selforder, timeclock), its name, a second monitor for guests, the PIN.
    // setupFinished follows; then ViewTouch starts.
    Q_INVOKABLE void setup(const QString &role, const QString &name, bool guestDisplay, const QString &pin);
    // Ask again what this computer is (its store data is kept).
    Q_INVOKABLE void startOver();
    // The main register: erase the store (a backup is made first) and start fresh.
    Q_INVOKABLE void eraseStore();
    Q_INVOKABLE void backupNow();
    Q_INVOKABLE void runUpdates();
    Q_INVOKABLE void updateFromUsb();
    // A newer ViewTouch on the release page: look (online-check), or download
    // and install it (online-update). Both signed; never an older one.
    Q_INVOKABLE void checkOnline();
    Q_INVOKABLE void installOnline();
    Q_INVOKABLE void setRemoteSupport(bool on);
    Q_INVOKABLE void restartComputer();
    Q_INVOKABLE void shutDown();

    // The store's backups, newest first: this computer's (its backups
    // folder) and any on a USB stick plugged in (after usbOpen()), as
    // [{path, name, at (ms), size, encrypted, where}].
    Q_INVOKABLE QVariantList backups() const;
    Q_INVOKABLE void usbOpen();
    // That backup in place of the store's data: ViewTouch stops, the data
    // now is kept beside it, and finished("restore") says how it went. An
    // encrypted one from another computer needs its backup password.
    Q_INVOKABLE void restoreBackup(const QString &path, const QString &password = {});
    // The screen: brightness (0-100), turned (0, 90, 180, 270).
    Q_INVOKABLE void setBrightness(int percent);
    Q_INVOKABLE void setRotation(int degrees);
    Q_INVOKABLE void setLockPower(bool on);
    // The last week of ViewTouch's logs, onto a USB stick (for whoever helps).
    Q_INVOKABLE void saveLogs();
    Q_INVOKABLE void refreshStatus();

signals:
    void configChanged();
    void posChanged();
    void networkChanged();
    void wifiChanged();
    void busyChanged();
    void outputChanged();
    void timeZoneChanged();
    void remoteSupportChanged();
    void statusChanged();
    void displayChanged();
    // Each action's end: ok, and what to say.
    void finished(const QString &action, bool ok, const QString &message);

private:
    void load();
    bool save();
    void setPosState(const QString &state);
    void posExited(int code, QProcess::ExitStatus status);
    // Run the helper (or, with `program`, that program): output collected,
    // finished(action, ...) at the end. `input`: written to its stdin.
    void run(const QString &action, const QStringList &args, const QByteArray &input = {},
             const QString &program = {});
    void setBusy(bool busy);
    void readTimeZone();
    void readRemoteSupport();
    void stopPos();
    // --data-dir and --config, for ViewTouch's one-off commands (backup, erase).
    QStringList storeArgs() const;

    QString dataDir_;
    QString configFile_;
    bool kiosk_ = false;
    QString role_;
    QString name_;
    QString pinHash_;
    mutable int wrongPins_ = 0;
    mutable qint64 pinWaitUntil_ = 0;
    QString pinSalt_;
    // To the store (the main register): tried until it's running.
    void handToStore(const QString &pin, int triesLeft = 30);
    QProcess *pos_ = nullptr;
    QString posState_ = QStringLiteral("stopped");
    QList<QDateTime> crashes_;
    QTimer restartTimer_;
    int restartIn_ = 0;
    QVariantMap network_;
    QVariantList wifi_;
    bool busy_ = false;
    QString output_;
    QString timeZone_;
    bool timeSynced_ = false;
    bool remoteSupport_ = false;
    QTimer networkTimer_;
    QVariantMap status_;
    int rotation_ = 0;
    bool lockPower_ = false;
    void applyRotation();
    QString backupDir() const;
};
