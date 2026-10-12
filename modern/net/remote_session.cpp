#include "net/remote_session.hh"

#include "net/build_info.hh"

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
#include <QJsonArray>
#include <QSslPreSharedKeyAuthenticator>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcRemote, "vt.remote")

namespace vt::net {

namespace {

constexpr int kReconnectMs = 2000;

// Which change signal each state key belongs to.
enum class Group { Session, Admin, Entry, Qualifier, Check, OpenChecks, Kitchen, Drawer, Day, None };

Group groupOf(const QString &key)
{
    static const QHash<QString, Group> groups = {
        {u"loggedIn"_s, Group::Session}, {u"userName"_s, Group::Session}, {u"userRole"_s, Group::Session},
        {u"permissions"_s, Group::Session}, {u"clockedIn"_s, Group::Session}, {u"clockedInSince"_s, Group::Session},
        {u"storeName"_s, Group::Admin}, {u"currencySymbol"_s, Group::Admin}, {u"customColors"_s, Group::Admin}, {u"priceChanges"_s, Group::Admin}, {u"adminRevision"_s, Group::Admin},
        {u"mealPeriods"_s, Group::Admin}, {u"pairing"_s, Group::Admin}, {u"screenMode"_s, Group::Admin}, {u"terminalLook"_s, Group::Admin}, {u"terminalKeyboard"_s, Group::Admin}, {u"terminalStartCategory"_s, Group::Admin}, {u"cardReader"_s, Group::Admin}, {u"readerToken"_s, Group::Session}, {u"counterCharge"_s, Group::Session}, {u"receiptOffer"_s, Group::Session},
        {u"closedChecks"_s, Group::Day}, {u"staff"_s, Group::Session}, {u"checkHistory"_s, Group::Check},
        {u"choosing"_s, Group::Check}, {u"weighing"_s, Group::Check}, {u"setup"_s, Group::Admin}, {u"onBreakSince"_s, Group::Session},
        {u"customers"_s, Group::Check}, {u"customer"_s, Group::Check}, {u"giftCard"_s, Group::Check}, {u"waitlist"_s, Group::Day}, {u"customerPrompt"_s, Group::Check},
        {u"schedule"_s, Group::Session}, {u"nextShift"_s, Group::Session}, {u"rangeReport"_s, Group::Session}, {u"expoTickets"_s, Group::Kitchen}, {u"approval"_s, Group::Session}, {u"training"_s, Group::Session}, {u"autoLogoutMinutes"_s, Group::Admin}, {u"screenSaverMinutes"_s, Group::Admin}, {u"storeImages"_s, Group::Admin}, {u"storeLogo"_s, Group::Admin}, {u"kitchenStations"_s, Group::Admin}, {u"kitchenStation"_s, Group::Admin}, {u"messages"_s, Group::Day}, {u"network"_s, Group::Day}, {u"printerAlerts"_s, Group::Day}, {u"language"_s, Group::Session}, {u"userPrefs"_s, Group::Session}, {u"storeLanguage"_s, Group::Admin}, {u"selfOrder"_s, Group::Check}, {u"kioskMenu"_s, Group::Admin}, {u"clockInJobs"_s, Group::Session}, {u"cashTipsAsk"_s, Group::Session}, {u"timeClock"_s, Group::Session}, {u"receiving"_s, Group::Admin}, {u"checkSearch"_s, Group::Session}, {u"soldOut"_s, Group::Admin}, {u"menuItems"_s, Group::Admin}, {u"menuCategories"_s, Group::Admin}, {u"choiceGroups"_s, Group::Admin}, {u"menuTemplates"_s, Group::Admin}, {u"menuProblems"_s, Group::Admin}, {u"menuUndoText"_s, Group::Admin}, {u"cardAlerts"_s, Group::Admin},
        {u"pinLength"_s, Group::Entry}, {u"entry"_s, Group::Entry}, {u"entryAmount"_s, Group::Entry},
        {u"entryGuests"_s, Group::Entry}, {u"textEntry"_s, Group::Entry},
        {u"pendingQualifier"_s, Group::Qualifier},
        {u"pendingTable"_s, Group::Check}, {u"hasCheck"_s, Group::Check}, {u"check"_s, Group::Check}, {u"tableChecks"_s, Group::Check}, {u"undoText"_s, Group::Check},
        {u"lines"_s, Group::Check}, {u"totals"_s, Group::Check}, {u"payments"_s, Group::Check},
        {u"selectedLine"_s, Group::Check}, {u"selectedPayment"_s, Group::Check},
        {u"openChecks"_s, Group::OpenChecks}, {u"floor"_s, Group::OpenChecks}, {u"deliveries"_s, Group::OpenChecks}, {u"drivers"_s, Group::OpenChecks}, {u"popularItems"_s, Group::OpenChecks}, {u"stockLeft"_s, Group::Day}, {u"dashboard"_s, Group::Day}, {u"checklists"_s, Group::Day}, {u"checkFilter"_s, Group::OpenChecks},
        {u"kitchenTickets"_s, Group::Kitchen}, {u"drawer"_s, Group::Drawer},
        {u"day"_s, Group::Day}, {u"days"_s, Group::Day}, {u"tipsOwed"_s, Group::Day},
    };
    return groups.value(key, Group::None);
}

} // namespace

RemoteSession::RemoteSession(QString terminalName, QObject *parent)
    : PosSession(parent)
    , terminal_(std::move(terminalName))
    , channel_(std::make_unique<LineChannel>(&socket_))
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
    watchdog_.setInterval(2000);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        if (welcomed_ && serverPings_ && QDateTime::currentMSecsSinceEpoch() - heard_ > silentMs_) {
            qCWarning(lcRemote) << "the server stopped answering";
            socket_.abort();
            if (welcomed_)
                onDisconnected();
        }
    });
    watchdog_.start();
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
    images_.insert(key, QString());   // asked once per run (a changed picture has a new key)
    auto *self = const_cast<RemoteSession *>(this);
    self->invoke(u"storeImage"_s, {serverPath}, [self, serverPath, key](const QVariant &r) {
        const QByteArray data = QByteArray::fromBase64(r.toString().toLatin1());
        if (data.isEmpty())
            return;
        const QString dir = self->imageCache_.isEmpty()
                                ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/images"_s
                                : self->imageCache_;
        QDir().mkpath(dir);
        const QString name = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex())
                             + u'.' + QFileInfo(serverPath).suffix();
        QSaveFile f(QDir(dir).filePath(name));
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
            return;
        self->images_.insert(key, QDir(dir).filePath(name));
        ++self->imageRevision_;
        emit self->adminChanged();   // the kiosk's menu
        emit self->checkChanged();   // the customer display
    });
    return {};
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
                      {u"platform"_s, me.platform}};
    if (!requestedScreen_.isEmpty())
        hello.insert(u"screen"_s, requestedScreen_);
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
    // Nothing pending will be answered now.
    const auto pending = std::exchange(replies_, {});
    for (const Reply &r : pending) {
        if (r)
            r(QVariant(false));
    }
    if (was) {
        qCWarning(lcRemote) << "lost the server; reconnecting";
        emit onlineChanged();
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
        applyState(m.value(u"state").toObject(), true);
        cache_.clear();
        storeBuild_ = m.value(u"build").toInt();
        storeVersion_ = m.value(u"version").toString();
        storeUpdate_ = m.value(u"update").toObject().toVariantMap();
        emit updateChanged();
        welcomed_ = true;
        refusals_ = 0;
        failures_ = 0;
        emit onlineChanged();
    } else if (type == u"state") {
        applyState(m.value(u"set").toObject(), false);
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

void RemoteSession::applyState(const QJsonObject &set, bool replaceAll)
{
    const bool wasLoggedIn = loggedIn();
    QSet<Group> changed;
    if (replaceAll) {
        state_.clear();
        for (int g = 0; g < int(Group::None); ++g)
            changed.insert(Group(g));
    }
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
    if (loggedIn() != wasLoggedIn)
        emit loggedInChanged(loggedIn());
}

void RemoteSession::invoke(const QString &method, const QVariantList &args, Reply reply)
{
    if (!welcomed_) {
        emit notice(tr("Not connected to the server."));
        if (reply)
            reply(QVariant(false));
        return;
    }
    const qint64 id = nextId_++;
    if (reply)
        replies_.insert(id, std::move(reply));
    send({{u"t"_s, u"call"_s}, {u"id"_s, id}, {u"m"_s, method}, {u"a"_s, QJsonArray::fromVariantList(args)}});
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
    if ((c.stale || !c.value.isValid()) && !c.inFlight && welcomed_) {
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
    if (!storeUpdate_.isEmpty())
        out.insert(u"update"_s, storeUpdate_);
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

} // namespace vt::net
