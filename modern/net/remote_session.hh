#pragma once

#include "app/pos_session.hh"
#include "layout/layout.hh"
#include "net/discovery.hh"
#include "net/pairing.hh"

#include <QHash>
#include <QJsonObject>
#include <QSslSocket>
#include <QTimer>

#include <memory>

namespace vt::app { class PosService; }

namespace vt::net {

class LineChannel;
class OfflineStore;

// A terminal whose POS session lives on a server. Mirrors the session state
// the server sends, forwards operations, and reconnects on its own when the
// connection drops (the server then starts a fresh, logged-out session).
// It connects as a paired device (setCredentials); a server that refuses
// the key (the device was unpaired) ends the retrying with `rejected`.
class RemoteSession : public app::PosSession {
    Q_OBJECT

public:
    explicit RemoteSession(QString terminalName, QObject *parent = nullptr);
    // The server, silent this long (after pinging), is gone; checked every
    // `checkMs` (tests: short).
    void setWatchdog(qint64 silentMs, int checkMs) { silentMs_ = silentMs; watchdog_.setInterval(checkMs); }
    // Test hook: say it's another kind of device ("android-arm64").
    void setPlatform(QString platform) { platform_ = std::move(platform); }
    // What this screen is set up as (vtmodern-setup): "selfOrder" or "timeClock".
    // The server takes it when Manager -> Terminals hasn't said otherwise.
    void setRequestedScreen(const QString &screen) { requestedScreen_ = screen; }
    ~RemoteSession() override;

    void setCredentials(const Credentials &credentials);
    const Credentials &credentials() const { return credentials_; }
    void connectTo(const QString &host, quint16 port);
    bool isConnected() const { return welcomed_; }
    bool isRejected() const { return rejected_; }
    // Where to look for a server that moved (tests use their own port).
    void setDiscoveryPort(quint16 port) { discoveryPort_ = port; }
    // Wait (processing events) until the server has welcomed us.
    bool waitForWelcome(int msec);

    // Pages from the server (on connect and after other terminals save).
    const layout::Layout &layout() const { return layout_; }
    // Send edited pages to the server. The answer comes as layoutSaved.
    void saveLayout(const layout::Layout &layout);

    void invoke(const QString &method, const QVariantList &args = {}, Reply reply = {}) override;
    void want(const QString &key, bool on) override;

    // The name the server knows this device by (it was paired under it).
    QString terminalName() const override
    {
        const QString server = v(u"terminalName").toString();
        return server.isEmpty() ? terminal_ : server;
    }
    QVariantMap updateInfo() const override;
    void getUpdate() override;
    bool online() const override { return welcomed_; }
    // Where the checks made offline are kept (offline.db in the data folder).
    // Set before connecting: without it this screen can't work offline.
    void setOfflineJournal(const QString &path);
    OfflineStore *offlineStore() const { return offline_.get(); }
    bool offlineReady() const override;
    bool offlineMode() const override { return offlineMode_; }
    int offlinePending() const override;
    // Working offline: a store of its own on the offline kit (new checks,
    // cash), until the store's computer is back and this screen has no
    // check open; then the checks made meanwhile go to it.
    void workOffline() override;
    int heldCount() const override { return welcomed_ ? 0 : int(kept_.size()); }
    bool loggedIn() const override { return v(u"loggedIn").toBool(); }
    QString userName() const override { return v(u"userName").toString(); }
    QString userRole() const override { return v(u"userRole").toString(); }
    QStringList permissions() const override { return v(u"permissions").toStringList(); }
    bool clockedIn() const override { return v(u"clockedIn").toBool(); }
    QString clockedInSince() const override { return v(u"clockedInSince").toString(); }
    QString storeName() const override { return v(u"storeName").toString(); }
    QString currencySymbol() const override { return v(u"currencySymbol").toString(); }
    QStringList customColors() const override { return v(u"customColors").toStringList(); }
    QVariantList priceChanges() const override { return v(u"priceChanges").toList(); }
    int pinLength() const override { return v(u"pinLength").toInt(); }
    QString entry() const override { return v(u"entry").toString(); }
    QString entryAmount() const override { return v(u"entryAmount").toString(); }
    int entryGuests() const override { return v(u"entryGuests").toInt(); }
    QString textEntry() const override { return v(u"textEntry").toString(); }
    QString pendingQualifier() const override { return v(u"pendingQualifier").toString(); }
    QString pendingTable() const override { return v(u"pendingTable").toString(); }
    bool hasCheck() const override { return v(u"hasCheck").toBool(); }
    QVariantMap checkInfo() const override { return v(u"check").toMap(); }
    QVariantList tableChecks() const override { return v(u"tableChecks").toList(); }
    QString undoText() const override { return v(u"undoText").toString(); }
    QVariantList lines() const override;
    QVariantMap totals() const override { return v(u"totals").toMap(); }
    QVariantList payments() const override { return v(u"payments").toList(); }
    qint64 selectedLine() const override { return v(u"selectedLine").toLongLong(); }
    qint64 selectedPayment() const override { return v(u"selectedPayment").toLongLong(); }
    QVariantList openChecks() const override;
    QVariantList openChecksShared() const override { return v(u"openChecksShared").toList(); }
    QVariantMap openChecksViewer() const override { return v(u"openChecksViewer").toMap(); }
    QVariantMap floor() const override { return v(u"floor").toMap(); }
    QVariantList deliveries() const override { return v(u"deliveries").toList(); }
    QVariantList drivers() const override { return v(u"drivers").toList(); }
    QStringList popularItems() const override { return v(u"popularItems").toStringList(); }
    QStringList myUsuals() const override { return v(u"myUsuals").toStringList(); }
    QVariantMap rushRadar() const override { return v(u"rushRadar").toMap(); }
    QVariantMap stockLeft() const override { return v(u"stockLeft").toMap(); }
    QVariantMap dashboard() const override { return v(u"dashboard").toMap(); }
    QVariantMap checklists() const override { return v(u"checklists").toMap(); }
    QString checkFilter() const override { return v(u"checkFilter").toString(); }
    QVariantList kitchenTickets() const override { return v(u"kitchenTickets").toList(); }
    QVariantMap drawerInfo() const override { return v(u"drawer").toMap(); }
    QVariantMap dayInfo() const override { return v(u"day").toMap(); }
    QVariantList days() const override { return v(u"days").toList(); }
    int adminRevision() const override { return v(u"adminRevision").toInt(); }
    QString tipsOwed() const override { return v(u"tipsOwed").toString(); }
    QVariantList mealPeriods() const override { return v(u"mealPeriods").toList(); }
    QVariantMap pairingInfo() const override { return v(u"pairing").toMap(); }
    QString screenMode() const override { return v(u"screenMode").toString(); }
    QVariantMap serviceInfo() const override { return v(u"service").toMap(); }
    QVariantList truckOrders() const override { return v(u"truckOrders").toList(); }
    QString terminalLook() const override { return v(u"terminalLook").toString(); }
    QString terminalKeyboard() const override { return v(u"terminalKeyboard").toString(); }
    QString terminalStartCategory() const override { return v(u"terminalStartCategory").toString(); }
    QString terminalCardReader() const override { return v(u"cardReader").toString(); }
    QVariantMap readerToken() const override { return v(u"readerToken").toMap(); }
    QVariantMap counterCharge() const override { return v(u"counterCharge").toMap(); }
    QVariantMap receiptOffer() const override { return v(u"receiptOffer").toMap(); }
    QVariantList closedChecks() const override { return v(u"closedChecks").toList(); }
    QVariantList staff() const override { return v(u"staff").toList(); }
    QVariantList checkHistory() const override { return v(u"checkHistory").toList(); }
    QVariantMap choosingInfo() const override { return v(u"choosing").toMap(); }
    QVariantMap weighingInfo() const override { return v(u"weighing").toMap(); }
    QVariantMap setupInfo() const override { return v(u"setup").toMap(); }
    QString onBreakSince() const override { return v(u"onBreakSince").toString(); }
    QVariantList customerResults() const override { return v(u"customers").toList(); }
    QVariantMap customerInfo() const override { return v(u"customer").toMap(); }
    QVariantMap giftCardInfo() const override { return v(u"giftCard").toMap(); }
    QVariantMap waitlistInfo() const override { return v(u"waitlist").toMap(); }
    QVariantMap scheduleInfo() const override { return v(u"schedule").toMap(); }
    QString nextShift() const override { return v(u"nextShift").toString(); }
    QVariantMap rangeReport() const override { return v(u"rangeReport").toMap(); }
    QVariantList expoTickets() const override { return v(u"expoTickets").toList(); }
    QVariantMap approvalInfo() const override { return v(u"approval").toMap(); }
    bool training() const override { return v(u"training").toBool(); }
    int autoLogoutMinutes() const override { return v(u"autoLogoutMinutes").toInt(); }
    int phoneLockSeconds() const override
    {
        const QVariant x = v(u"phoneLockSeconds");
        return x.isValid() ? x.toInt() : 30;   // an older server: the default
    }
    QString imageUrl(const QString &ref) const override;
    QVariantList storeImages() const override;
    QString storeLogo() const override { return v(u"storeLogo").toString(); }
    QVariantMap kioskMenu() const override { return v(u"kioskMenu").toMap(); }
    QVariantMap customerPrompt() const override { return v(u"customerPrompt").toMap(); }
    int imageRevision() const override { return adminRevision() + imageRevision_; }
    int screenSaverMinutes() const override { return v(u"screenSaverMinutes").toInt(); }
    QVariantList kitchenStations() const override { return v(u"kitchenStations").toList(); }
    QString kitchenStation() const override { return v(u"kitchenStation").toString(); }
    QVariantList messages() const override { return v(u"messages").toList(); }
    QVariantList handoff() const override { return v(u"handoff").toList(); }
    bool standbyReady() const override { return !welcomed_ && !standbyHost_.isEmpty(); }
    QVariantMap networkInfo() const override { return v(u"network").toMap(); }
    QVariantList printerAlerts() const override { return v(u"printerAlerts").toList(); }
    QString language() const override { return v(u"language").toString(); }
    QVariantMap userPrefs() const override { return v(u"userPrefs").toMap(); }
    QString storeLanguage() const override { return v(u"storeLanguage").toString(); }
    QVariantMap selfOrderInfo() const override { return v(u"selfOrder").toMap(); }
    QVariantMap clockInJobs() const override { return v(u"clockInJobs").toMap(); }
    QVariantMap cashTipsAsk() const override { return v(u"cashTipsAsk").toMap(); }
    QVariantMap reasonAsk() const override { return v(u"reasonAsk").toMap(); }
    QVariantMap timeClock() const override { return v(u"timeClock").toMap(); }
    QVariantMap receiving() const override { return v(u"receiving").toMap(); }
    QVariantMap stockCounting() const override { return v(u"stockCounting").toMap(); }
    QVariantMap wasteLog() const override { return v(u"wasteLog").toMap(); }
    QVariantMap storeCheck() const override { return v(u"storeCheck").toMap(); }
    QVariantMap prepLabels() const override { return v(u"prepLabels").toMap(); }
    QVariantMap checkSearch() const override { return v(u"checkSearch").toMap(); }
    // Where pictures from the server are kept on this device (tests set it).
    void setImageCache(const QString &dir) { imageCache_ = dir; }
    void takeOver(const QString &pin) override;
    QStringList soldOut() const override { return v(u"soldOut").toStringList(); }
    QVariantList menuItems() const override { return v(u"menuItems").toList(); }
    QVariantList menuCategories() const override { return v(u"menuCategories").toList(); }
    QVariantList choiceGroups() const override { return v(u"choiceGroups").toList(); }
    QVariantList menuTemplates() const override { return v(u"menuTemplates").toList(); }
    QVariantList menuProblems() const override { return v(u"menuProblems").toList(); }
    QString menuUndoText() const override { return v(u"menuUndoText").toString(); }
    QVariantList cardAlerts() const override { return v(u"cardAlerts").toList(); }
    int queryRevision() const override { return queryRevision_; }

    // Answered from a cache; fetched from the server when missing or stale.
    QVariantMap report(const QString &id, qint64 dayId = 0) override;
    QVariantList adminFields(const QString &panel) override;
    QVariantList adminRecords(const QString &panel) override;
    QVariantMap adminNewRecord(const QString &panel) override;

signals:
    void layoutReceived(const vt::layout::Layout &layout);
    void layoutSaved(bool ok, const QString &error);
    // The server does not accept this device's key (unpaired, or another store).
    void rejected();
    // The server was found at a new address (its id answered from there);
    // save the credentials so the next start goes straight there.
    void credentialsChanged(const vt::net::Credentials &credentials);

private:
    QVariant v(QStringView key) const { return state_.value(key.toString()); }
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void handle(const QJsonObject &m);
    void applyState(const QJsonObject &set, bool replaceAll);
    void send(const QJsonObject &m);
    // Cached query: returns what is known now and asks the server if needed.
    QVariant query(const QString &key, const QString &method, const QVariantList &args);

    QString terminal_;
    QString requestedScreen_;
    QString host_;
    quint16 port_ = 0;
    QSslSocket socket_;
    Credentials credentials_;
    bool rejected_ = false;
    int refusals_ = 0;
    int failures_ = 0;       // connection attempts in a row that got nowhere
    qint64 connectingSince_ = 0;   // when the try under way began
    ServerFinder finder_;    // to find the server again after an address change
    QString standbyHost_;    // the store's standby, heard while the server was out of reach
    quint16 standbyPort_ = 0;
    quint16 discoveryPort_ = DiscoveryPort;
    bool encrypted_ = false;
    std::unique_ptr<LineChannel> channel_;
    QTimer reconnect_;
    QTimer watchdog_;            // the server gone quiet: reconnect
    // The store's build, and the update it has for this screen (welcome).
    int storeBuild_ = 0;
    QString storeVersion_;
    QVariantMap storeUpdate_;
    // An update being fetched: its parts so far, then the file.
    QByteArray updateData_;
    int updateParts_ = 0;
    int updateProgress_ = -1;
    QString updateReady_;
    QString updateError_;
    void finishUpdate(const QJsonObject &done);
    void openUpdate();
    qint64 silentMs_ = 10'000;   // a server that pings, heard from no longer than this: gone
    qint64 heard_ = 0;           // when the server last said anything
    qint64 lastWatch_ = 0;       // the watchdog's last look (a long gap: this app was asleep)
    QString lastDrop_;           // why the connection was lost (told to the server on the way back)
    int updateReadyBuild_ = 0;   // the build of updateReady_
    QString platform_;           // test hook (setPlatform); empty: this build's
    bool serverPings_ = false;   // it pings (older servers don't)
    bool welcomed_ = false;
    qint64 nextId_ = 1;
    QHash<qint64, Reply> replies_;
    // Order taps (an item, a seat, a qualifier...) not answered yet: sent
    // again when the server is back, in order, by the same id (one it did
    // already is answered, not done twice). Made while it's out of reach,
    // they're kept here and answered at once, so the server can keep going.
    struct Kept {
        QString method;
        QVariantList args;
    };
    QMap<qint64, Kept> kept_;
    // This run of the app: the server keeps its session for it a while.
    QString resumeToken_;
    void resendKept(bool resumed);
    QVariantMap state_;
    // Working offline (workOffline): the screen shows the offline store's
    // state; what the store's computer says meanwhile waits here.
    std::unique_ptr<OfflineStore> offline_;
    app::PosService *local_ = nullptr;
    bool offlineMode_ = false;
    bool goingBack_ = false;   // the store is back: going back to it once no check is open here
    bool uploading_ = false;   // the checks made offline, sent and not answered yet
    QVariantMap serverState_;
    QSet<int> localDirty_;     // the offline store's change signals since the last flush
    QTimer localFlush_;
    void flushLocal(bool all);
    void maybeGoBack(bool tell);
    void goBackOnline();
    void sendOfflineChecks();
    void tookOfflineChecks(const QJsonArray &results);
    QHash<QString, int> wanted_;   // what's asked for, by how many on the screen
    mutable QVariantList openMerged_;   // the open checks put together (until either part changes)
    mutable bool openMergedValid_ = false;
    layout::Layout layout_;

    struct Cached {
        QVariant value;
        bool stale = true;
        bool inFlight = false;
    };
    QHash<QString, Cached> cache_;
    // The store's pictures are files on the server: fetched once, kept here.
    // Returns the local file, or empty until it has arrived.
    QString localImage(const QString &serverPath, const QString &cacheKey = {}) const;
    QString imageCache_;
    mutable QHash<QString, QString> images_;   // server path -> local file ("" while fetching)
    int imageRevision_ = 0;                     // pictures that have arrived
    QTimer imagesArrived_;                      // a few together: one redraw
    int queryRevision_ = 0;
};

} // namespace vt::net
