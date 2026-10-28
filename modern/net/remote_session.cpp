#include "net/remote_session.hh"

#include "app/i18n.hh"
#include "app/pos_service.hh"
#include "net/build_info.hh"
#include "net/offline_store.hh"

#include <QDateTime>
#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>
#endif
#include "net/standby.hh"

#include "net/protocol.hh"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QJsonArray>
#include <QSslPreSharedKeyAuthenticator>
#include <QLoggingCategory>
#include <QMetaMethod>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcRemote, "vt.remote")

namespace vt::net {

namespace {

constexpr int kReconnectMs = 2000;

// What a server taking an order does at the table: safe to keep while the
// Wi-Fi is out and send when it's back. Sending, paying, opening checks,
// logging in or out: not (they need the server's answer then and there).
bool keepable(const QString &method)
{
    static const QSet<QString> methods = {
        u"addItem"_s, u"setQualifier"_s, u"lineMore"_s, u"lineLess"_s, u"repeatLine"_s, u"setLineQuantity"_s,
        u"setSeat"_s, u"setCourse"_s, u"textKey"_s, u"addComment"_s, u"anotherRound"_s, u"adjustGuests"_s,
        u"chooseOption"_s, u"chooseOptionAs"_s, u"setChoice"_s};
    return methods.contains(method);
}

// What a screen working offline can do on its own: take new orders, send
// them, take cash, close. Everything else needs the store's computer (cards,
// gift cards, customers, the time clock, drawers, reports, managing).
bool offlineAllowed(const QString &method)
{
    static const QSet<QString> methods = {
        u"pinKey"_s, u"entryKey"_s, u"adjustGuests"_s, u"textKey"_s, u"clearEntry"_s, u"login"_s, u"loginWithPin"_s,
        u"logout"_s, u"selectTable"_s, u"startCheck"_s, u"openCheck"_s, u"openTab"_s, u"releaseCheck"_s,
        u"newTableCheck"_s, u"splitBySeat"_s, u"printTableChecks"_s, u"combineTableChecks"_s, u"addItem"_s,
        u"storeImage"_s, u"setQualifier"_s, u"selectLine"_s, u"selectPayment"_s, u"setCheckFilter"_s,
        u"voidItem"_s, u"setLineQuantity"_s, u"lineMore"_s, u"lineLess"_s, u"anotherRound"_s, u"customDiscount"_s,
        u"printReceiptOn"_s, u"noReceipt"_s, u"undoLast"_s, u"repeatLine"_s, u"sendOrder"_s, u"addWeighed"_s,
        u"cancelWeighing"_s, u"addComment"_s, u"tender"_s, u"removePayment"_s, u"closeCheck"_s, u"printReceipt"_s,
        u"splitLine"_s, u"shareLine"_s, u"splitEvenly"_s, u"unshareLine"_s, u"setCashTip"_s, u"setGratuity"_s,
        u"approve"_s, u"cancelApproval"_s, u"giveReason"_s, u"scanCode"_s, u"setSeat"_s, u"setAllergies"_s,
        u"setChoice"_s, u"chooseOptionAs"_s, u"chooseOption"_s, u"finishChoosing"_s, u"cancelChoosing"_s,
        u"chooseLine"_s, u"setCourse"_s, u"fireCourse"_s, u"fireCourseIn"_s, u"transferCheck"_s, u"moveCheck"_s,
        u"mergeCheck"_s};
    return methods.contains(method);
}

// Which change signal each state key belongs to.
enum class Group { Session, Admin, Entry, Qualifier, Check, OpenChecks, Kitchen, Drawer, Day, None };

Group groupOf(const QString &key)
{
    static const QHash<QString, Group> groups = {
        {u"loggedIn"_s, Group::Session}, {u"userName"_s, Group::Session}, {u"userRole"_s, Group::Session},
        {u"permissions"_s, Group::Session}, {u"clockedIn"_s, Group::Session}, {u"clockedInSince"_s, Group::Session},
        {u"storeName"_s, Group::Admin}, {u"currencySymbol"_s, Group::Admin}, {u"customColors"_s, Group::Admin}, {u"priceChanges"_s, Group::Admin}, {u"adminRevision"_s, Group::Admin},
        {u"mealPeriods"_s, Group::Admin}, {u"pairing"_s, Group::Admin}, {u"screenMode"_s, Group::Admin}, {u"service"_s, Group::Session}, {u"terminalLook"_s, Group::Admin}, {u"terminalKeyboard"_s, Group::Admin}, {u"terminalStartCategory"_s, Group::Admin}, {u"cardReader"_s, Group::Admin}, {u"readerToken"_s, Group::Session}, {u"counterCharge"_s, Group::Session}, {u"receiptOffer"_s, Group::Session},
        {u"closedChecks"_s, Group::Day}, {u"staff"_s, Group::Session}, {u"checkHistory"_s, Group::Check},
        {u"choosing"_s, Group::Check}, {u"weighing"_s, Group::Check}, {u"setup"_s, Group::Admin}, {u"onBreakSince"_s, Group::Session},
        {u"customers"_s, Group::Check}, {u"customer"_s, Group::Check}, {u"giftCard"_s, Group::Check}, {u"waitlist"_s, Group::Day}, {u"customerPrompt"_s, Group::Check},
        {u"schedule"_s, Group::Session}, {u"nextShift"_s, Group::Session}, {u"rangeReport"_s, Group::Session}, {u"expoTickets"_s, Group::Kitchen}, {u"approval"_s, Group::Session}, {u"training"_s, Group::Session}, {u"autoLogoutMinutes"_s, Group::Admin}, {u"phoneLockSeconds"_s, Group::Admin}, {u"screenSaverMinutes"_s, Group::Admin}, {u"storeImages"_s, Group::Admin}, {u"storeLogo"_s, Group::Admin}, {u"kitchenStations"_s, Group::Admin}, {u"kitchenStation"_s, Group::Admin}, {u"messages"_s, Group::Day}, {u"handoff"_s, Group::Session}, {u"network"_s, Group::Day}, {u"printerAlerts"_s, Group::Day}, {u"language"_s, Group::Session}, {u"userPrefs"_s, Group::Session}, {u"storeLanguage"_s, Group::Admin}, {u"selfOrder"_s, Group::Check}, {u"kioskMenu"_s, Group::Admin}, {u"clockInJobs"_s, Group::Session}, {u"cashTipsAsk"_s, Group::Session}, {u"reasonAsk"_s, Group::Session}, {u"timeClock"_s, Group::Session}, {u"receiving"_s, Group::Admin}, {u"stockCounting"_s, Group::Admin}, {u"wasteLog"_s, Group::Admin}, {u"storeCheck"_s, Group::Day}, {u"prepLabels"_s, Group::Admin}, {u"checkSearch"_s, Group::Session}, {u"soldOut"_s, Group::Admin}, {u"menuItems"_s, Group::Admin}, {u"menuCategories"_s, Group::Admin}, {u"choiceGroups"_s, Group::Admin}, {u"menuTemplates"_s, Group::Admin}, {u"menuProblems"_s, Group::Admin}, {u"menuUndoText"_s, Group::Admin}, {u"cardAlerts"_s, Group::Admin},
        {u"pinLength"_s, Group::Entry}, {u"entry"_s, Group::Entry}, {u"entryAmount"_s, Group::Entry},
        {u"entryGuests"_s, Group::Entry}, {u"textEntry"_s, Group::Entry},
        {u"pendingQualifier"_s, Group::Qualifier},
        {u"pendingTable"_s, Group::Check}, {u"hasCheck"_s, Group::Check}, {u"check"_s, Group::Check}, {u"tableChecks"_s, Group::Check}, {u"undoText"_s, Group::Check},
        {u"lines"_s, Group::Check}, {u"totals"_s, Group::Check}, {u"payments"_s, Group::Check},
        {u"selectedLine"_s, Group::Check}, {u"selectedPayment"_s, Group::Check},
        {u"openChecks"_s, Group::OpenChecks}, {u"openChecksShared"_s, Group::OpenChecks}, {u"openChecksViewer"_s, Group::OpenChecks}, {u"floor"_s, Group::OpenChecks}, {u"deliveries"_s, Group::OpenChecks}, {u"drivers"_s, Group::OpenChecks}, {u"popularItems"_s, Group::OpenChecks}, {u"myUsuals"_s, Group::OpenChecks}, {u"stockLeft"_s, Group::Day}, {u"dashboard"_s, Group::Day}, {u"checklists"_s, Group::Day}, {u"checkFilter"_s, Group::OpenChecks},
        {u"kitchenTickets"_s, Group::Kitchen}, {u"rushRadar"_s, Group::Kitchen}, {u"drawer"_s, Group::Drawer},
        {u"day"_s, Group::Day}, {u"days"_s, Group::Day}, {u"tipsOwed"_s, Group::Day},
    };
    return groups.value(key, Group::None);
}

} // namespace

RemoteSession::RemoteSession(QString terminalName, QObject *parent)
    : PosSession(parent)
    , terminal_(std::move(terminalName))
    , channel_(std::make_unique<LineChannel>(&socket_))
    , resumeToken_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    socket_.setSslConfiguration(tlsConfiguration());
    connect(&socket_, &QSslSocket::preSharedKeyAuthenticationRequired, this, [this](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity(deviceIdentity(credentials_.terminalId));
        a->setPreSharedKey(credentials_.key);
    });
    connect(&socket_, &QSslSocket::encrypted, this, &RemoteSession::onConnected);
    connect(&socket_, &QSslSocket::readyRead, this, &RemoteSession::onReadyRead);
    connect(&socket_, &QSslSocket::disconnected, this, &RemoteSession::onDisconnected);
    connect(&socket_, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError e) {
        // A refused key shows as a failed handshake. Twice in a row counts
        // (once could be a server restarting mid-handshake).
        const bool refused = e == QAbstractSocket::SslHandshakeFailedError
                             || (e == QAbstractSocket::RemoteHostClosedError && !encrypted_ && !welcomed_);
        if (refused && !welcomed_ && ++refusals_ >= 2) {
            qCWarning(lcRemote) << "the server refused this device's key";
            rejected_ = true;
            reconnect_.stop();
            emit notice(tr("This terminal is not paired with the server. Pair it again."));
            emit rejected();
            return;
        }
        // Can't reach the server: every third try, look for it on the
        // network in case its address changed.
        if (!welcomed_ && ++failures_ % 3 == 0 && !credentials_.serverId.isEmpty())
            finder_.search(discoveryPort_);
        if (!welcomed_ && !reconnect_.isActive())
            reconnect_.start();
    });
    connect(&finder_, &ServerFinder::found, this, [this](const FoundServer &s) {
        if (s.role == u"standby") {   // the store's standby: ready to take over
            if (s.id == credentials_.serverId && !welcomed_ && standbyHost_ != s.host) {
                standbyHost_ = s.host;
                standbyPort_ = s.port;
                emit sessionChanged();
            }
            return;
        }
        // The server answers once per network it hears on: act on the first
        // answer, and not while that attempt is still under way.
        if (welcomed_ || s.id != credentials_.serverId || (s.host == host_ && s.port == port_)
            || socket_.state() != QAbstractSocket::UnconnectedState)
            return;
        qCInfo(lcRemote).noquote() << "the server moved to" << s.host;
        host_ = credentials_.host = s.host;
        port_ = credentials_.port = s.port;
        emit credentialsChanged(credentials_);
        reconnect_.stop();
        encrypted_ = false;
        socket_.connectToHostEncrypted(host_, port_);
    });
    // A server that pings and then says nothing for 10 s is out of reach
    // (Wi-Fi gone without the socket closing): say so, give up what was
    // waiting, and reconnect.
    // This app asleep (Android pauses it while the screen is off or another
    // app is in front) isn't the server gone quiet: woken, it waits for the
    // server's next ping (every 3 s) before deciding.
    watchdog_.setInterval(2000);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool woke = lastWatch_ > 0 && now - lastWatch_ > 3 * watchdog_.interval();
        lastWatch_ = now;
        if (woke) {
            heard_ = std::max(heard_, now);
            return;
        }
        if (welcomed_ && serverPings_ && now - heard_ > silentMs_) {
            qCWarning(lcRemote) << "the server stopped answering";
            lastDrop_ = u"the server was silent for %1 s"_s.arg((now - heard_) / 1000);
            socket_.abort();
            if (welcomed_)
                onDisconnected();
        }
    });
    watchdog_.start();
    imagesArrived_.setSingleShot(true);
    imagesArrived_.setInterval(60);
    connect(&imagesArrived_, &QTimer::timeout, this, [this] {
        ++imageRevision_;
        emit imagesChanged();   // the menu's photos, the kiosk's, the customer display's
    });
    localFlush_.setSingleShot(true);
    localFlush_.setInterval(0);
    connect(&localFlush_, &QTimer::timeout, this, [this] { flushLocal(false); });
    reconnect_.setSingleShot(true);
    reconnect_.setInterval(kReconnectMs);
    connect(&reconnect_, &QTimer::timeout, this, [this] {
        if (socket_.state() == QAbstractSocket::UnconnectedState && !rejected_) {
            encrypted_ = false;
            socket_.connectToHostEncrypted(host_, port_);
        }
    });
}

QString RemoteSession::localImage(const QString &serverPath, const QString &cacheKey) const
{
    if (serverPath.isEmpty() || !(serverPath.startsWith(u'/') || serverPath.startsWith(u"store:")))
        return serverPath;   // a resource or web address: as it is
    const QString key = cacheKey.isEmpty() ? serverPath : cacheKey;
    const auto it = images_.constFind(key);
    if (it != images_.cend())
        return *it;
    const QString dir = imageCache_.isEmpty()
                            ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/images"_s
                            : imageCache_;
    const QString file = QDir(dir).filePath(
        QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex()) + u'.'
        + QFileInfo(serverPath).suffix());
    // Kept from before (this device fetched it on an earlier run): shown at
    // once. A store picture's key has its content's hash, so it is the same
    // picture; one by its path is asked for again (it may have been replaced).
    const bool kept = QFileInfo::exists(file);
    images_.insert(key, kept ? file : QString());   // asked once per run (a changed picture has a new key)
    if (kept && key != serverPath)
        return file;
    auto *self = const_cast<RemoteSession *>(this);
    self->invoke(u"storeImage"_s, {serverPath}, [self, key, file, dir](const QVariant &r) {
        const QByteArray data = QByteArray::fromBase64(r.toString().toLatin1());
        if (data.isEmpty())
            return;
        QFile old(file);
        if (old.open(QIODevice::ReadOnly) && old.readAll() == data)
            return;   // the same as the copy kept: nothing to redraw
        old.close();
        QDir().mkpath(dir);
        QSaveFile f(file);
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
            return;
        self->images_.insert(key, file);
        // Several pictures come together (a menu of photos): one redraw.
        self->imagesArrived_.start();
    });
    return kept ? file : QString();
}

QString RemoteSession::imageUrl(const QString &ref) const
{
    if (ref == u"logo:") {
        const QString logo = storeLogo();
        return logo == u"logo:" ? QString() : imageUrl(logo);
    }
    QString key = ref;
    if (ref.startsWith(u"store:")) {   // by its content: a replaced picture is fetched again
        for (const QVariant &v : this->v(u"storeImages").toList())
            if (v.toMap().value(u"ref"_s).toString() == ref)
                key = ref + u'#' + v.toMap().value(u"hash"_s).toString();
    } else if (!ref.startsWith(u'/')) {
        return ref;
    }
    const QString local = localImage(ref, key);
    return local.isEmpty() ? QString() : QUrl::fromLocalFile(local).toString();
}

QVariantList RemoteSession::storeImages() const
{
    QVariantList out = v(u"storeImages").toList();
    for (QVariant &item : out) {
        QVariantMap m = item.toMap();
        m.insert(u"url"_s, imageUrl(m.value(u"ref"_s).toString()));
        item = m;
    }
    return out;
}


void RemoteSession::takeOver(const QString &pin)
{
    if (!standbyReady()) {
        emit notice(tr("No standby server is ready."));
        return;
    }
    auto *request = new TakeOverRequest(credentials_, standbyHost_, standbyPort_, pin, this);
    connect(request, &TakeOverRequest::finished, this, [this, request](bool ok, const QString &message) {
        emit notice(message);
        if (ok) {   // it serves the store from now on: look for it
            standbyHost_.clear();
            emit sessionChanged();
            QTimer::singleShot(1500, this, [this] { finder_.search(discoveryPort_); });
            QTimer::singleShot(4000, this, [this] { finder_.search(discoveryPort_); });
        }
        request->deleteLater();
    });
}

RemoteSession::~RemoteSession()
{
    socket_.disconnect(this);
    socket_.abort();
}

void RemoteSession::setCredentials(const Credentials &credentials)
{
    credentials_ = credentials;
    if (!credentials.terminalName.isEmpty())
        terminal_ = credentials.terminalName;
}

void RemoteSession::connectTo(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;
    rejected_ = false;
    encrypted_ = false;
    socket_.connectToHostEncrypted(host_, port_);
}

bool RemoteSession::waitForWelcome(int msec)
{
    if (welcomed_)
        return true;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &PosSession::onlineChanged, &loop, &QEventLoop::quit);
    connect(this, &RemoteSession::rejected, &loop, &QEventLoop::quit);
    timeout.start(msec);
    loop.exec();
    return welcomed_;
}

void RemoteSession::onConnected()
{
    encrypted_ = true;
    const AppBuild me = appBuild();
    QJsonObject hello{{u"t"_s, u"hello"_s}, {u"terminal"_s, terminal_}, {u"protocol"_s, ProtocolVersion},
                      {u"version"_s, me.version}, {u"build"_s, me.number}, {u"commit"_s, me.commit},
                      {u"platform"_s, platform_.isEmpty() ? me.platform : platform_},
                      // Big things (the closed checks) only while they're shown: see want().
                      {u"subscribes"_s, true},
                      // Back after the Wi-Fi dropped: the session it had, if the server kept it.
                      {u"resume"_s, resumeToken_}};
    if (!requestedScreen_.isEmpty())
        hello.insert(u"screen"_s, requestedScreen_);
    // Why it was cut off last time, for the store's log.
    if (!lastDrop_.isEmpty())
        hello.insert(u"lastDrop"_s, std::exchange(lastDrop_, {}));
    if (!updateReady_.isEmpty() && QFile::exists(updateReady_))
        hello.insert(u"updateReady"_s, updateReadyBuild_);   // not sent again
    send(hello);
}

void RemoteSession::onReadyRead()
{
    bool overflow = false;
    for (const QJsonObject &m : channel_->receive(&overflow))
        handle(m);
    if (overflow)
        socket_.abort();
}

void RemoteSession::onDisconnected()
{
    const bool was = welcomed_;
    welcomed_ = false;
    uploading_ = false;
    goingBack_ = false;
    // Nothing pending will be answered now. Order taps among them go again
    // when it's back: taken as done (the server keeps going), and shown
    // waiting in the check meanwhile.
    const auto pending = std::exchange(replies_, {});
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        if (it.value())
            it.value()(QVariant(!kept_.isEmpty() && kept_.contains(it.key())));
    }
    if (was) {
        if (lastDrop_.isEmpty())
            lastDrop_ = socket_.error() != QAbstractSocket::UnknownSocketError ? socket_.errorString() : u"closed"_s;
        qCWarning(lcRemote).noquote() << "lost the server (" + lastDrop_ + "); reconnecting";
        emit onlineChanged();
        if (!offlineMode_)
            emit notice(tr("Lost the connection to the server. Reconnecting…"));
    }
    encrypted_ = false;
    if (!rejected_)
        reconnect_.start();
}

void RemoteSession::send(const QJsonObject &m)
{
    channel_->send(m);
}

void RemoteSession::handle(const QJsonObject &m)
{
    const QString type = m.value(u"t").toString();
    heard_ = QDateTime::currentMSecsSinceEpoch();
    if (type == u"ping") {
        serverPings_ = true;
        send({{u"t"_s, u"pong"_s}});
        return;
    }
    if (type == u"welcome") {
        if (auto l = layout::Layout::fromJson(m.value(u"layout").toObject())) {
            layout_ = *l;
            emit layoutReceived(layout_);
        }
        // Working offline: the store's state waits until this screen goes back to it.
        if (offlineMode_)
            serverState_ = m.value(u"state").toObject().toVariantMap();
        else
            applyState(m.value(u"state").toObject(), true);
        cache_.clear();
        storeBuild_ = m.value(u"build").toInt();
        storeVersion_ = m.value(u"version").toString();
        storeUpdate_ = m.value(u"update").toObject().toVariantMap();
        emit updateChanged();
        welcomed_ = true;
        for (auto it = wanted_.cbegin(); it != wanted_.cend(); ++it)
            if (it.value() > 0)
                send({{u"t"_s, u"want"_s}, {u"k"_s, it.key()}, {u"on"_s, true}});
        resendKept(m.value(u"resumed").toBool());
        refusals_ = 0;
        failures_ = 0;
        emit onlineChanged();
        if (offlineMode_) {
            goingBack_ = true;
            maybeGoBack(true);
        } else {
            sendOfflineChecks();   // made before this screen restarted
        }
    } else if (type == u"state") {
        if (offlineMode_) {
            const QJsonObject set = m.value(u"set").toObject();
            for (auto it = set.begin(); it != set.end(); ++it)
                serverState_.insert(it.key(), it.value().toVariant());
        } else {
            applyState(m.value(u"set").toObject(), false);
        }
    } else if (type == u"offlineKit") {
        if (offline_) {
            const bool was = offline_->hasKit();
            offline_->setKit(m.value(u"kit").toObject());
            if (offline_->hasKit() != was)
                emit onlineChanged();   // offlineReady
        }
    } else if (type == u"offlineTaken") {
        tookOfflineChecks(m.value(u"results").toArray());
    } else if (type == u"updatePart") {
        // The store's update for this screen, a part at a time.
        if (m.value(u"i").toInt() == 0)
            updateData_.clear();
        updateData_ += QByteArray::fromBase64(m.value(u"data").toString().toLatin1());
        const int n = std::max(1, m.value(u"n").toInt());
        updateProgress_ = std::min(100, (m.value(u"i").toInt() + 1) * 100 / n);
        if (const QJsonObject done = m.value(u"done").toObject(); !done.isEmpty())
            finishUpdate(done);
        emit updateChanged();
    } else if (type == u"updateError") {
        updateProgress_ = -1;
        updateError_ = m.value(u"text").toString();
        emit notice(updateError_);
        emit updateChanged();
    } else if (type == u"reply") {
        if (kept_.remove(m.value(u"id").toInteger()) > 0)
            emit checkChanged();   // no longer waiting
        const Reply r = replies_.take(m.value(u"id").toInteger());
        if (r)
            r(m.value(u"r").toVariant());
    } else if (type == u"notice") {
        emit notice(m.value(u"text").toString());
    } else if (type == u"event") {
        if (m.value(u"e").toString() == u"checkClosed")
            emit checkClosed(m.value(u"v").toInteger());
    } else if (type == u"layout") {
        if (auto l = layout::Layout::fromJson(m.value(u"layout").toObject())) {
            layout_ = *l;
            emit layoutReceived(layout_);
        }
    } else if (type == u"error") {
        emit notice(m.value(u"text").toString());
    }
}

QVariantList RemoteSession::openChecks() const
{
    // A store that sends the two parts (this one), or the whole list (older).
    if (!state_.contains(u"openChecksShared"_s))
        return v(u"openChecks").toList();
    if (!openMergedValid_) {
        openMerged_ = mergeOpenChecks(openChecksShared(), openChecksViewer());
        openMergedValid_ = true;
    }
    return openMerged_;
}

void RemoteSession::want(const QString &key, bool on)
{
    int &n = wanted_[key];
    const bool was = n > 0;
    n = std::max(0, n + (on ? 1 : -1));
    if ((n > 0) == was)
        return;
    if (n == 0)
        state_.remove(key);   // not kept up to date any more: nothing rather than old
    if (offlineMode_ && n > 0) {
        localDirty_.insert(-1);
        localFlush_.start();
    }
    if (welcomed_)
        send({{u"t"_s, u"want"_s}, {u"k"_s, key}, {u"on"_s, n > 0}});
}

void RemoteSession::applyState(const QJsonObject &set, bool replaceAll)
{
    const bool wasLoggedIn = loggedIn();
    const QString wasUser = userName();
    QSet<Group> changed;
    if (replaceAll) {
        state_.clear();
        for (int g = 0; g < int(Group::None); ++g)
            changed.insert(Group(g));
    }
    openMergedValid_ = openMergedValid_ && !replaceAll && !set.contains(u"openChecksShared"_s)
                       && !set.contains(u"openChecksViewer"_s);
    for (auto it = set.begin(); it != set.end(); ++it) {
        state_.insert(it.key(), it.value().toVariant());
        changed.insert(groupOf(it.key()));
    }

    // Live reports follow sales; admin answers follow admin changes.
    const bool salesMoved = changed.contains(Group::OpenChecks) || changed.contains(Group::Day)
                            || changed.contains(Group::Drawer);
    bool invalidated = false;
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if ((salesMoved && it.key().startsWith(u"report|")) ||
            (changed.contains(Group::Admin) && (it.key().startsWith(u"adminRecords|") || it.key().startsWith(u"adminFields|")))) {
            it->stale = true;
            invalidated = true;
        }
    }

    if (changed.contains(Group::Session)) emit sessionChanged();
    if (changed.contains(Group::Admin)) emit adminChanged();
    if (changed.contains(Group::Entry)) emit entryChanged();
    if (changed.contains(Group::Qualifier)) emit qualifierChanged();
    if (changed.contains(Group::Check)) emit checkChanged();
    if (changed.contains(Group::OpenChecks)) emit openChecksChanged();
    if (changed.contains(Group::Kitchen)) emit kitchenChanged();
    if (changed.contains(Group::Drawer)) emit drawerChanged();
    if (changed.contains(Group::Day)) emit dayChanged();
    if (invalidated) {
        ++queryRevision_;
        emit queriesChanged();
    }
    // Someone else now, still logged in (a manager stepping in at a food
    // truck, and back to its counter): as the service says it too.
    if (loggedIn() != wasLoggedIn || (loggedIn() && userName() != wasUser))
        emit loggedInChanged(loggedIn());
}

void RemoteSession::invoke(const QString &method, const QVariantList &args, Reply reply)
{
    if (offlineMode_) {
        if (!offlineAllowed(method)) {
            if (method != u"refreshDay")   // the dashboard's own, on a timer: not said each time
                emit notice(tr("Not while working offline: that needs the store's computer."));
            if (reply)
                reply(QVariant(false));
            return;
        }
        local_->invoke(method, args, std::move(reply));
        return;
    }
    // At the table with the Wi-Fi out: kept for when it's back (only while
    // someone is logged in: the server keeps that session for this device).
    const bool keep = keepable(method) && loggedIn();
    if (!welcomed_) {
        if (keep && !rejected_) {
            kept_.insert(nextId_++, {method, args});
            emit onlineChanged();   // heldCount
            emit checkChanged();    // shown waiting
            if (reply)
                reply(QVariant(true));
            return;
        }
        emit notice(keepable(method) || method == u"sendOrder" ? tr("Not connected to the server. Try again when it's back.")
                                                               : tr("Not connected to the server."));
        if (reply)
            reply(QVariant(false));
        return;
    }
    const qint64 id = nextId_++;
    if (keep)
        kept_.insert(id, {method, args});
    if (reply)
        replies_.insert(id, std::move(reply));
    send({{u"t"_s, u"call"_s}, {u"id"_s, id}, {u"m"_s, method}, {u"a"_s, QJsonArray::fromVariantList(args)}});
}

void RemoteSession::resendKept(bool resumed)
{
    if (kept_.isEmpty())
        return;
    if (!resumed) {
        // The server started over (or the standby took over, or it was out
        // of reach too long): the check those taps were for isn't open here.
        const int lost = int(kept_.size());
        kept_.clear();
        emit checkChanged();
        emit notice(tr("The connection was out too long: %n tap(s) made meanwhile didn't go in. Open the check and add them again.",
                       "", lost));
        return;
    }
    for (auto it = kept_.cbegin(); it != kept_.cend(); ++it)
        send({{u"t"_s, u"call"_s}, {u"id"_s, it.key()}, {u"m"_s, it->method},
              {u"a"_s, QJsonArray::fromVariantList(it->args)}});
    emit notice(tr("Back online: sent %n tap(s) made while it was out.", "", int(kept_.size())));
}

QVariantList RemoteSession::lines() const
{
    QVariantList out = v(u"lines").toList();
    if (kept_.isEmpty())
        return out;
    // Items tapped and not in yet: shown at the end, waiting.
    QHash<QString, QVariantMap> items;
    for (auto it = kept_.cbegin(); it != kept_.cend(); ++it) {
        if (it->method != u"addItem" || it->args.isEmpty())
            continue;
        if (items.isEmpty())
            for (const QVariant &x : menuItems())
                items.insert(x.toMap().value(u"id"_s).toString(), x.toMap());
        const QString id = it->args.first().toString();
        const QVariantMap item = items.value(id);
        out.append(QVariantMap{{u"id"_s, -it.key()}, {u"name"_s, item.value(u"name"_s, id)}, {u"quantity"_s, 1},
                               {u"price"_s, QString()}, {u"waiting"_s, true}, {u"modifiers"_s, QVariantList()},
                               {u"seat"_s, checkInfo().value(u"seat"_s)}});
    }
    return out;
}

void RemoteSession::saveLayout(const layout::Layout &layout)
{
    if (!welcomed_) {
        emit layoutSaved(false, tr("Not connected to the server."));
        return;
    }
    const qint64 id = nextId_++;
    replies_.insert(id, [this, layout](const QVariant &r) {
        const QVariantMap m = r.toMap();
        const bool ok = m.value(u"ok"_s).toBool();
        if (ok)
            layout_ = layout;
        emit layoutSaved(ok, m.value(u"error"_s).toString());
    });
    send({{u"t"_s, u"saveLayout"_s}, {u"id"_s, id}, {u"layout"_s, layout.toJson()}});
}

QVariant RemoteSession::query(const QString &key, const QString &method, const QVariantList &args)
{
    Cached &c = cache_[key];
    if ((c.stale || !c.value.isValid()) && !c.inFlight && welcomed_ && !offlineMode_) {
        c.inFlight = true;
        invoke(method, args, [this, key](const QVariant &r) {
            Cached &entry = cache_[key];
            entry.inFlight = false;
            entry.stale = false;
            entry.value = r;
            ++queryRevision_;
            emit queriesChanged();
        });
    }
    return c.value;
}

QVariantMap RemoteSession::report(const QString &id, qint64 dayId)
{
    return query(u"report|%1|%2"_s.arg(id).arg(dayId), u"report"_s, {id, dayId}).toMap();
}

QVariantList RemoteSession::adminFields(const QString &panel)
{
    return query(u"adminFields|"_s + panel, u"adminFields"_s, {panel}).toList();
}

QVariantList RemoteSession::adminRecords(const QString &panel)
{
    return query(u"adminRecords|"_s + panel, u"adminRecords"_s, {panel}).toList();
}

QVariantMap RemoteSession::adminNewRecord(const QString &panel)
{
    return query(u"adminNewRecord|"_s + panel, u"adminNewRecord"_s, {panel}).toMap();
}

// --- updates -----------------------------------------------------------------------

QVariantMap RemoteSession::updateInfo() const
{
    const AppBuild me = appBuild();
    QVariantMap out{{u"build"_s, me.number}, {u"version"_s, me.version}, {u"platform"_s, me.platform},
                    {u"text"_s, buildText(me.version, me.number, me.commit, me.modified)},
                    {u"storeBuild"_s, storeBuild_}, {u"storeText"_s, buildText(storeVersion_, storeBuild_)},
                    // Build 0: made without git (or a store older than build numbers): not compared.
                    {u"behind"_s, me.number > 0 && storeBuild_ > me.number},
                    {u"ahead"_s, me.number > 0 && storeBuild_ > 0 && storeBuild_ < me.number}};
    if (!storeUpdate_.isEmpty()) {
        out.insert(u"update"_s, storeUpdate_);
        out.insert(u"byItself"_s, storeUpdate_.value(u"byItself"_s).toBool());
    }
    if (updateProgress_ >= 0)
        out.insert(u"downloading"_s, updateProgress_);
    if (!updateReady_.isEmpty())
        out.insert(u"ready"_s, updateReady_);
    if (!updateError_.isEmpty())
        out.insert(u"error"_s, updateError_);
    return out;
}

void RemoteSession::getUpdate()
{
    // Already here (Android asked to allow installs first): open it again.
    if (!updateReady_.isEmpty() && QFile::exists(updateReady_)) {
        openUpdate();
        return;
    }
    if (!welcomed_ || storeUpdate_.isEmpty()) {
        emit notice(tr("The store has no newer version for this screen."));
        return;
    }
    updateError_.clear();
    updateReady_.clear();
    updateProgress_ = 0;
    emit updateChanged();
    send({{u"t"_s, u"getUpdate"_s}});
}

void RemoteSession::finishUpdate(const QJsonObject &done)
{
    updateProgress_ = -1;
    const QByteArray data = std::exchange(updateData_, {});
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    if (data.size() != done.value(u"size").toInteger() || hash != done.value(u"sha256").toString()) {
        updateError_ = tr("The update didn't arrive whole. Try again.");
        emit notice(updateError_);
        return;
    }
    // Only a plain file name, kept in this app's own folder.
    const QString name = QFileInfo(done.value(u"name").toString()).fileName();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/updates"_s;
    QDir().mkpath(dir);
    QSaveFile f(dir + u'/' + name);
    if (name.isEmpty() || !f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        updateError_ = tr("Couldn't save the update: %1").arg(f.errorString());
        emit notice(updateError_);
        return;
    }
    updateReady_ = f.fileName();
    updateReadyBuild_ = done.value(u"build").toInt();
    emit updateChanged();
    // Got by itself (nobody here): Install waits for whoever uses it next.
    if (!done.value(u"byItself").toBool())
        openUpdate();
}

void RemoteSession::openUpdate()
{
#if defined(Q_OS_ANDROID)
    // Android's installer asks whoever is here to install it.
    QJniObject::callStaticMethod<void>("org/viewtouch/pos/Updater", "install",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       QNativeInterface::QAndroidApplication::context().object(),
                                       QJniObject::fromString(updateReady_).object<jstring>());
#else
    emit notice(tr("Update saved: %1").arg(updateReady_));
#endif
}

// --- working offline -----------------------------------------------------------------

void RemoteSession::setOfflineJournal(const QString &path)
{
    offline_ = std::make_unique<OfflineStore>(terminal_, path);
    connect(offline_.get(), &OfflineStore::pendingChanged, this, &PosSession::onlineChanged);
    connect(offline_.get(), &OfflineStore::printFailed, this,
            [this](const QString &printer, const QString &what, const QString &error) {
        emit notice(tr("%1 did not print on %2: %3").arg(what, printer, error));
    });
}

bool RemoteSession::offlineReady() const
{
    return offline_ && offline_->hasKit();
}

int RemoteSession::offlinePending() const
{
    return offline_ ? offline_->pendingCount() : 0;
}

void RemoteSession::workOffline()
{
    if (offlineMode_ || welcomed_ || !offline_)
        return;
    local_ = offline_->start();
    if (!local_) {
        emit notice(tr("This screen can't work offline yet: it gets what it needs from the store's computer."));
        return;
    }
    // Taps kept for a check at the store: that check stays the store's.
    if (!kept_.isEmpty()) {
        emit notice(tr("%n tap(s) waiting for the store's computer were let go: add them again once it's back.", "",
                       int(kept_.size())));
        kept_.clear();
    }
    // Its changes reach the screen as the store's do: the same pieces, by the same signals.
    for (auto signal : {&app::PosSession::sessionChanged, &app::PosSession::entryChanged,
                        &app::PosSession::qualifierChanged, &app::PosSession::checkChanged,
                        &app::PosSession::openChecksChanged, &app::PosSession::kitchenChanged,
                        &app::PosSession::drawerChanged, &app::PosSession::dayChanged,
                        &app::PosSession::adminChanged, &app::PosSession::imagesChanged}) {
        const int index = QMetaMethod::fromSignal(signal).methodIndex();
        connect(local_, signal, this, [this, index] {
            localDirty_.insert(index);
            localFlush_.start();
        });
    }
    connect(local_, &app::PosSession::loggedInChanged, this, [this] {
        localDirty_.insert(-1);   // everything
        localFlush_.start();
    });
    connect(local_, &app::PosSession::notice, this, &app::PosSession::notice);
    connect(local_, &app::PosSession::checkClosed, this, &app::PosSession::checkClosed);
    connect(local_, &app::PosSession::checkChanged, this, [this] { maybeGoBack(false); });
    offlineMode_ = true;
    goingBack_ = false;
    serverState_.clear();
    flushLocal(true);
    qCInfo(lcRemote) << "working offline";
    emit onlineChanged();
    emit notice(tr("Working offline: new checks, paid in cash. They go to the store's computer once it's back."));
}

// The offline store's state, the pieces its signals said may have changed
// (all: every one), as the store's computer would send them.
void RemoteSession::flushLocal(bool all)
{
    if (!local_)
        return;
    static const auto pieces = [] {
        std::vector<std::pair<QString, QMetaProperty>> out;
        const QMetaObject &mo = app::PosSession::staticMetaObject;
        for (const QString &key : app::PosSession::stateKeys())
            if (key != u"openChecks")   // in two parts, as a store sends it
                out.emplace_back(key, mo.property(mo.indexOfProperty(key.toLatin1().constData())));
        return out;
    }();
    const QSet<int> dirty = std::exchange(localDirty_, {});
    all = all || dirty.contains(-1);
    // The store's pictures stay as the store said: this screen has them in
    // its own cache (the offline store has none).
    static const QStringList pictures{u"storeImages"_s, u"storeLogo"_s};
    const i18n::Scope language([s = local_] { return s->language(); });
    local_->shared()->beginMemo();
    QJsonObject set;
    for (const auto &[key, p] : pieces) {
        if (pictures.contains(key)) {
            if (all && state_.contains(key))
                set.insert(key, QJsonValue::fromVariant(state_.value(key)));
            continue;
        }
        if (!all && p.hasNotifySignal() && !dirty.contains(p.notifySignalIndex()))
            continue;
        if ((key == u"closedChecks" || key == u"rushRadar") && wanted_.value(key) <= 0)
            continue;   // only while shown
        const QJsonValue value = QJsonValue::fromVariant(p.read(local_));
        if (!all && QJsonValue::fromVariant(state_.value(key)) == value)
            continue;
        set.insert(key, value);
    }
    local_->shared()->endMemo();
    if (all || !set.isEmpty())
        applyState(set, all);
}

// The store's computer is back: this screen goes back to it once nobody is
// in the middle of a check here.
void RemoteSession::maybeGoBack(bool tell)
{
    if (!offlineMode_ || !goingBack_ || !welcomed_)
        return;
    if (local_ && local_->hasCheck()) {
        if (tell)
            emit notice(tr("The store's computer is back: finish this check, and this screen goes back to it."));
        return;
    }
    // Not from inside the offline store's own signal: it's let go of.
    QTimer::singleShot(0, this, [this] {
        if (offlineMode_ && goingBack_ && welcomed_ && !(local_ && local_->hasCheck()))
            goBackOnline();
    });
}

void RemoteSession::goBackOnline()
{
    if (local_)
        disconnect(local_, nullptr, this, nullptr);
    localFlush_.stop();
    localDirty_.clear();
    local_ = nullptr;
    offline_->stop();
    offlineMode_ = false;
    goingBack_ = false;
    applyState(QJsonObject::fromVariantMap(std::exchange(serverState_, {})), true);
    qCInfo(lcRemote) << "back online";
    emit onlineChanged();
    sendOfflineChecks();
}

void RemoteSession::sendOfflineChecks()
{
    if (!welcomed_ || offlineMode_ || uploading_ || !offline_ || offline_->pendingCount() == 0)
        return;
    uploading_ = true;
    send({{u"t"_s, u"offlineChecks"_s}, {u"checks"_s, offline_->pending()}});
}

void RemoteSession::tookOfflineChecks(const QJsonArray &results)
{
    uploading_ = false;
    int taken = 0;
    QStringList problems;
    for (const QJsonValue &v : results) {
        const QJsonObject r = v.toObject();
        if (r.contains(u"id") || r.value(u"skipped").toBool()) {
            if (offline_)
                offline_->taken(r.value(u"uid").toString());
            taken += r.contains(u"id") && !r.value(u"again").toBool();
        } else {
            problems << r.value(u"error").toString();   // kept, sent again next time
        }
    }
    if (taken > 0)
        emit notice(tr("Back online: the store's computer has the %n check(s) made offline.", "", taken));
    if (!problems.isEmpty())
        emit notice(tr("Not taken in yet: %1").arg(problems.join(u"; "_s)));
}

} // namespace vt::net
