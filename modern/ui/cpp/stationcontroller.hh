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

    // Start ViewTouch (on `afterLogin` once someone logs in: a page id, or "setup").
    Q_INVOKABLE void startPos(const QString &afterLogin = {});
    // Don't start it again by itself (a crash): stay on the home screen.
    Q_INVOKABLE void stayHome();
    Q_INVOKABLE bool checkPin(const QString &pin) const;
    Q_INVOKABLE bool setPin(const QString &pin);

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
    Q_INVOKABLE void setRemoteSupport(bool on);
    Q_INVOKABLE void restartComputer();
    Q_INVOKABLE void shutDown();

signals:
    void configChanged();
    void posChanged();
    void networkChanged();
    void wifiChanged();
    void busyChanged();
    void outputChanged();
    void timeZoneChanged();
    void remoteSupportChanged();
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
    QString pinSalt_;
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
};
