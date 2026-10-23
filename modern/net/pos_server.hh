#pragma once

#include <QHostAddress>
#include <QHash>
#include <QObject>
#include <QSslServer>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

namespace vt::app { class PosShared; }

namespace vt::net {

class LayoutHub;

// Serves remote terminals: each connection gets its own PosService session
// on the shared store, and receives its state as it changes. Connections
// are encrypted and only paired devices get in (see net/pairing.hh); a
// device pairing with the manager's code gets its key and is let go.
// A value as compact JSON text (what the updates to the screens are made of).
QByteArray jsonText(const QVariant &v);

class PosServer : public QObject {
    Q_OBJECT

public:
    PosServer(app::PosShared *shared, LayoutHub *layouts, QObject *parent = nullptr);
    ~PosServer() override;

    // False when this system can't make encrypted connections (errorString says why).
    // Where the store keeps updates for its screens (the app for tablets,
    // "ViewTouch-b<build>-arm64-v8a.apk"; packages for Linux screens): a
    // screen that's behind is told, and a manager there can fetch it.
    void setUpdatesDir(const QString &dir) { updatesDir_ = dir; }
    QString updatesDir() const { return updatesDir_; }
    struct UpdateFile {
        QString path;
        int build = 0;
        QString platform;   // "android-arm64", "linux-x86_64"...
    };
    // The update files there, newest build first.
    QList<UpdateFile> updateFiles() const;
    // The newest for that platform, newer than `build` (none: an empty path).
    UpdateFile updateFor(const QString &platform, int build) const;
    // Ping every `pingMs`; drop a terminal silent `silentMs` (tests: short).
    void setHeartbeat(int pingMs, qint64 silentMs) { pingTimer_.setInterval(pingMs); silentMs_ = silentMs; }
    bool listen(const QHostAddress &address, quint16 port);
    quint16 port() const { return server_.serverPort(); }
    QString errorString() const { return error_.isEmpty() ? server_.errorString() : error_; }
    int terminalCount() const;
    // Time spent sending the screens their updates (for the stress test).
    qint64 updateNsecs() const { return updateNsecs_; }
    int updateRounds() const { return updateRounds_; }

    // The store's standby: a full copy of the database when it connects
    // (`snapshot` makes one: the database file's bytes), then every change
    // (`replicate`, from the database writer's mirror).
    void setSnapshotSource(std::function<QByteArray()> snapshot) { snapshot_ = std::move(snapshot); }
    void replicate(const QJsonObject &op);
    int standbyCount() const;
    // Handhelds out of reach with someone logged in: their sessions, kept
    // this long for them to come back to (tests: short).
    void setKeepAway(qint64 ms) { keepAwayMs_ = ms; }
    int awayCount() const { return int(away_.size()); }
    // Who is connected, for the Network screen: [{name, address, since, standby}].
    QVariantList connections() const;

signals:
    void terminalsChanged();
    void standbyChanged();

private:
    struct Connection;
    struct Away;

    void onNewConnection();
    void onPreSharedKey(QSslSocket *socket, class QSslPreSharedKeyAuthenticator *auth);
    void pair(Connection *c, const QJsonObject &message);
    // Drop connections of devices that are no longer paired.
    void dropRevoked();
    void onReadyRead(Connection *c);
    void handle(Connection *c, const QJsonObject &message);
    void drop(Connection *c);
    // Its session's changes go to that connection (and stop going there).
    void attach(Connection *c);
    void detach(Connection *c);
    // A dropped handheld's session, kept for it (or let go: its check is free).
    void keepAway(Connection *c);
    void letGoAway(qint64 olderThan);
    // Send changed state to one / every terminal.
    void flush(Connection *c);
    void scheduleFlushAll();
    // What a screen needs to keep selling if it loses this server (its
    // offline kit): on connecting, and again as the menu, staff, settings,
    // drawers or the day change. The checks it made meanwhile come back
    // as "offlineChecks".
    void sendKit(Connection *c, QByteArray *common = nullptr);   // common: the kits' shared part, made once a round
    void takeOfflineChecks(Connection *c, const QJsonArray &checks);
    QTimer kitSoon_;   // after a change
    QTimer kitTick_;   // and every few minutes

    app::PosShared *shared_;
    qint64 updateNsecs_ = 0;
    // During a round of updates (every screen at once): what's the same for
    // all of them is compared and turned into JSON once (by the shared data
    // it points at; cleared after the round).
    bool inRound_ = false;
    QHash<const void *, QByteArray> roundJson_;
    QHash<QPair<const void *, const void *>, bool> roundSame_;
    int updateRounds_ = 0;
    LayoutHub *layouts_;
    QSslServer server_;
    QString error_;
    QHash<QString, QByteArray> pairingKeys_;   // code -> TLS key (slow to derive)
    std::vector<std::unique_ptr<Connection>> connections_;
    std::vector<std::unique_ptr<Away>> away_;
    qint64 keepAwayMs_ = 3 * 60'000;
    QTimer flushTimer_;
    QTimer tickTimer_;   // refresh time-based fields (minutes open)
    QTimer pingTimer_;   // the standby knows the main is alive; terminals, the store
    qint64 silentMs_ = 15'000;
    QString updatesDir_;
    void sendUpdate(Connection *c);   // a terminal that answered pings, silent this long: gone
    std::function<QByteArray()> snapshot_;
};

} // namespace vt::net
