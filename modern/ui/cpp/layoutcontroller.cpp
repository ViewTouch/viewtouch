#include "layoutcontroller.hh"
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

#include <QGuiApplication>

#include "net/build_info.hh"

#include <QCollator>
#include <QFile>
#include <QDate>
#include <QRegularExpression>
#include <QSaveFile>
#include <climits>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>

#include "app/looks.hh"
#include <QUrl>

#include <QCoreApplication>
#include <QEvent>

#include "core/employee.hh"
#include "core/settings.hh"
#include "app/menu_import.hh"
#include "app/xlsx.hh"
#include "layout/menu_screens.hh"
#include "layout/reflow.hh"
#include "layout/schema.hh"
#include "menuprint.hh"
#include "reportexport.hh"
#include "storage/layout_store.hh"

#include <QPointer>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSettings>

using namespace Qt::StringLiterals;
using vt::app::Navigator;
using vt::app::PosSession;
using vt::layout::Action;
using vt::layout::Layout;
using vt::layout::ZoneState;

Q_LOGGING_CATEGORY(lcLayout, "vt.layout")

namespace {

// Page id of the first push/replace jump, used to light index tabs.
QString primaryJumpTarget(const Layout &layout, const vt::layout::Zone &zone)
{
    for (const Action &a : zone.actions) {
        if (a.type() != u"jump")
            continue;
        const QString mode = a.data.value(u"mode").toString(u"push"_s);
        if (mode == u"push" || mode == u"replace")
            return layout.resolveTarget(a.data);
    }
    return {};
}

// Qualifier a zone sets ("no", "extra"...), used to light it while pending.
QString qualifierOf(const vt::layout::Zone &zone)
{
    for (const Action &a : zone.actions) {
        if (a.type() == u"qualifier")
            return a.str(u"qualifier");
    }
    return {};
}

QString homePageOf(const Layout &layout)
{
    if (const vt::layout::Page *p = layout.pageByRole(u"login"_s))
        return p->id;
    return layout.pages.isEmpty() ? QString() : layout.pages.first().id;
}

} // namespace

LayoutController::LayoutController(Layout layout, QObject *parent)
    : QObject(parent)
    , layout_(std::move(layout))
    , nav_(layout_)
{
    // The card reader asks the store for its connection token, and says how a card went.
    connect(&cardReader_, &CardReader::needToken, this, [this] {
        if (pos_)
            pos_->requestReaderToken();
        else
            cardReader_.provideToken({}, tr("Not connected to the store."));
    });
    connect(&cardReader_, &CardReader::finished, this, &LayoutController::cardTaken);
    // A card taken offline, once Stripe has it (or declined it): the store records it.
    connect(&cardReader_, &CardReader::forwarded, this, [this](const QVariantMap &r) {
        if (pos_)
            pos_->cardForwarded(r);
    });
    connect(&cardReader_, &CardReader::cancelRequested, this, [this] { if (pos_) pos_->cancelCounterCharge(); });
    connect(&cardReader_, &CardReader::testCardRequested, this, [this](const QString &card) {
        if (pos_)
            pos_->presentTestCard(card);
    });
    updateMealPeriod();
    mealTimer_.setInterval(60 * 1000);
    connect(&mealTimer_, &QTimer::timeout, this, &LayoutController::updateMealPeriod);
    mealTimer_.start();
    idleTimer_.setSingleShot(true);
    connect(&idleTimer_, &QTimer::timeout, this, &LayoutController::idleTimeout);
    sleepTimer_.setSingleShot(true);
    connect(&sleepTimer_, &QTimer::timeout, this, &LayoutController::sleepTimeout);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);
    // The screen turning off and on (a phone put away and taken out again).
    if (auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        connect(gui, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive)
                screenOn();
            else if (state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden)
                screenOff();
        });
    nav_.reset(homePageOf(layout_));
    refresh();
}

bool LayoutController::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::TouchBegin:
    case QEvent::KeyPress:
    case QEvent::Wheel:
        restartIdle();
        if (!asleep_)   // asleep: the dim layer takes this touch and wakes it
            restartSleep();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void LayoutController::restartIdle()
{
    const int minutes = pos_ ? pos_->autoLogoutMinutes() : 0;
    const int ms = idleOverrideMs_ > 0 ? idleOverrideMs_ : minutes * 60 * 1000;
    if (ms <= 0 || !pos_ || !pos_->loggedIn()) {
        idleTimer_.stop();
        return;
    }
    idleTimer_.start(ms);
}

void LayoutController::restartSleep()
{
    const int minutes = pos_ ? pos_->screenSaverMinutes() : 0;
    const int ms = sleepOverrideMs_ > 0 ? sleepOverrideMs_ : minutes * 60 * 1000;
    if (ms <= 0) {
        sleepTimer_.stop();
        return;
    }
    sleepTimer_.start(ms);
}

void LayoutController::sleepTimeout()
{
    // Screens people watch stay on: kitchen, bar and expo; the kiosk has its own pictures.
    const vt::layout::Page *page = activeLayout().page(pageId());
    const bool watched = page && page->kind == u"kitchen";
    const bool kiosk = pos_ && pos_->selfOrderInfo().value(u"on"_s).toBool();
    if (watched || kiosk || editing()) {
        restartSleep();
        return;
    }
    asleep_ = true;
    emit asleepChanged();
}

void LayoutController::wake()
{
    if (!asleep_)
        return;
    asleep_ = false;
    emit asleepChanged();
    restartSleep();
}

void LayoutController::screenOff()
{
    if (!offSince_.isValid())
        offSince_.start();
}

void LayoutController::screenOn()
{
    if (!offSince_.isValid())
        return;
    const qint64 offMs = offSince_.elapsed();
    offSince_.invalidate();
    const int lockAfter = pos_ ? pos_->phoneLockSeconds() : -1;
    if (formFactor_ != u"phone" || lockAfter < 0 || !pos_ || !pos_->loggedIn() || editing())
        return;
    if (offMs < qint64(lockAfter) * 1000)
        return;
    pos_->invoke(u"logout"_s, {});   // their check comes back when they log in
    setStatus(tr("Type your PIN to go on"));
}

void LayoutController::idleTimeout()
{
    // Not while a page is being edited (the editor guards its own changes),
    // nor a food truck's counter (nobody to log out: the order stays).
    if (!pos_ || !pos_->loggedIn() || editing() || atCounter())
        return;
    pos_->invoke(u"logout"_s, {});
    setStatus(tr("Logged out after no use"));
}

LayoutController::~LayoutController()
{
    delete editor_.data();
}

QString LayoutController::mealPeriodAt(QTime time)
{
    return QString::fromStdString(vt::core::mealPeriodAt(vt::core::defaultMealPeriods(), time.msecsSinceStartOfDay() / 60000));
}

QString LayoutController::mealPeriodAt(const QVariantList &periods, QTime time)
{
    std::vector<vt::core::MealPeriod> list;
    for (const QVariant &v : periods) {
        const QVariantMap m = v.toMap();
        list.push_back({m.value(u"id"_s).toString().toStdString(), {}, m.value(u"start"_s).toInt()});
    }
    return QString::fromStdString(vt::core::mealPeriodAt(list, time.msecsSinceStartOfDay() / 60000));
}

void LayoutController::setMealPeriod(const QString &period)
{
    mealPeriodFixed_ = true;
    nav_.setMealPeriod(period);
    emit mealPeriodChanged();
}

void LayoutController::updateMealPeriod()
{
    if (mealPeriodFixed_)
        return;
    const QTime now = QTime::currentTime();
    const QString before = nav_.mealPeriod();
    nav_.setMealPeriod(pos_ ? mealPeriodAt(pos_->mealPeriods(), now) : mealPeriodAt(now));
    if (nav_.mealPeriod() != before)
        emit mealPeriodChanged();
}

void LayoutController::setStore(vt::storage::LayoutStore *store)
{
    if (!store) {
        saver_ = {};
        return;
    }
    saver_ = [store](const Layout &layout, QString *error) { return store->save(layout, error); };
}

void LayoutController::replaceLayout(Layout layout)
{
    layout_ = std::move(layout);
    emit menuScreensChanged();
    if (lookedLayout_)
        updateTerminalLook();   // the new pages, in this screen's look
    if (editing_) {
        setStatus(tr("Pages were changed on another terminal. Saving your edits will replace them."));
        return;
    }
    nav_.setHome(loginPage());
    ensureCurrentPageExists();
    refresh();
    emit pageChanged();
}

void LayoutController::setPos(PosSession *pos)
{
    if (pos_ == pos)
        return;
    if (pos_)
        disconnect(pos_, nullptr, this, nullptr);
    pos_ = pos;
    if (pos_) {
        connect(pos_, &PosSession::notice, this, &LayoutController::setStatus);
        connect(pos_, &PosSession::onlineChanged, this, &LayoutController::sendUnsentCards);
        connect(pos_, &PosSession::loggedInChanged, this, &LayoutController::onLoggedInChanged);
        connect(pos_, &PosSession::checkClosed, this, [this] { navigate(Navigator::Mode::Home); });
        connect(pos_, &PosSession::qualifierChanged, this, &LayoutController::refresh);
        // A store font's file arrived (a handheld fetches the store's files).
        connect(pos_, &PosSession::imagesChanged, this, &LayoutController::installStoreFonts);
        connect(pos_, &PosSession::adminChanged, this, [this] {
            cardReader_.setKind(pos_->terminalCardReader());   // Manager -> Terminals: this screen's reader
            if (pos_->terminalLook() != lookedId_)
                updateTerminalLook();   // Manager -> Terminals: this screen's look
            restAtLoginPage();      // a terminal just set to (or from) Time Clock
            installStoreFonts();   // before pages and font lists look for them
            updateMealPeriod();
            updateFormFactor();
            if (!asleep_)
                restartSleep();   // the setting may have changed
            refresh();   // sold-out marks
            if (editor_)
                editor_->setMealPeriods(pos_->mealPeriods());
        });
        // Whoever is editing must stay allowed to: logging out, being
        // deactivated or losing the role closes the editor (unsaved edits
        // are dropped, as in the legacy system).
        connect(pos_, &PosSession::sessionChanged, this, [this] {
            if (pos_->terminalLook() != lookedId_)
                updateTerminalLook();   // someone with their own look logged in, or out
            cardReader_.setCounter(pos_->counterCharge());   // a reader beside the screen, at work
            // The connection token the reader asked for.
            const QVariantMap token = pos_->readerToken();
            if (const int seq = token.value(u"seq"_s).toInt(); seq > readerTokenSeq_) {
                readerTokenSeq_ = seq;
                cardReader_.provideToken(token.value(u"token"_s).toString(), token.value(u"error"_s).toString());
            }
            if (editing_ && !pos_->can(QString::fromLatin1(vt::core::perm::EditLayout))) {
                leaveEditMode(false);
                setStatus(tr("Edit mode closed: the page editor needs a manager logged in."));
            }
        });
    }
    cardReader_.setKind(pos_ ? pos_->terminalCardReader() : QString());
    loadUnsentCards();   // left from before the app last closed
    sendUnsentCards();
    updateMealPeriod();
    updateFormFactor();
    emit posChanged();
    refresh();
    installStoreFonts();
    restartSleep();
    restAtLoginPage();   // a Time Clock terminal starts on its page
    updateTerminalLook();
    if (atCounter() && pos_->loggedIn())
        onLoggedInChanged(true);   // a food truck starts at its menu, nobody to log in
                                   // (main's --page kitchen comes after, for its kitchen screen)
}

void LayoutController::call(const QString &method, const QVariantList &args,
                            std::function<void(const QVariant &)> then)
{
    ++pending_;
    if (pending_ == 1)
        emit busyChanged();
    // QPointer: a remote reply may arrive after this controller is gone.
    QPointer<LayoutController> self(this);
    pos_->invoke(method, args, [self, then = std::move(then)](const QVariant &result) {
        if (!self)
            return;
        if (--self->pending_ == 0)
            emit self->busyChanged();
        if (then)
            then(result);
    });
}

void LayoutController::restAtLoginPage()
{
    // Logged out and resting on the login or Time Clock page: the one this terminal uses.
    if (!pos_ || pos_->loggedIn() || editing())
        return;
    // Only toward the Time Clock (a terminal set to it, resting on the login
    // page); someone who opened the Time Clock from the login page stays there.
    const QString want = loginPage();
    const vt::layout::Page *here = currentPage();
    const vt::layout::Page *target = activeLayout().page(want);
    if (!here || !target || here->id == want || target->role != u"timeClock" || here->role != u"login")
        return;
    nav_.reset(want);
    refresh();
    emit pageChanged();
}

bool LayoutController::atCounter() const
{
    return pos_ && pos_->serviceInfo().value(u"counter"_s).toBool();
}

bool LayoutController::steppedIn() const
{
    return pos_ && pos_->serviceInfo().value(u"steppedIn"_s).toBool();
}

QString LayoutController::loginPage() const
{
    // A Time Clock terminal (Manager -> Terminals): its page is where it rests.
    if (timeClockScreen_ || (pos_ && pos_->screenMode() == u"timeClock"))
        if (const vt::layout::Page *p = activeLayout().pageByRole(u"timeClock"_s))
            return p->id;
    return homePageOf(activeLayout());
}

QVariantList LayoutController::pageChoices(const QString &only) const
{
    const auto floor = [](const vt::layout::Page &p) {
        return p.variantOf.isEmpty()
            && (p.kind == u"tables" || std::ranges::any_of(p.zones, [](const vt::layout::Zone &z) {
                    // tables, or the seating screen (not Move Check's panel of plans)
                    return z.kind == u"table" || z.kind == u"tableMap" || z.kind == u"hostStand";
                }));
    };
    QVariantList out{QVariantMap{{u"value"_s, QString()}, {u"text"_s, QString()}}};
    for (const vt::layout::Page &p : activeLayout().pages)
        // "start": what a device may start on: floor plans, Open Checks, menus, tabs, deliveries.
        if (p.kind != u"template" && p.kind != u"library" && (only != u"floor" || floor(p))
            && (only != u"start" || (p.variantOf.isEmpty() && (floor(p) || p.kind == u"items" || p.role == u"checkList"
                                                               || p.id == u"tabs" || p.id == u"deliveries"))))
            out.append(QVariantMap{{u"value"_s, p.id}, {u"text"_s, p.name}});
    return out;
}

QString LayoutController::rolePage(const QString &role) const
{
    const vt::layout::Page *p = activeLayout().pageByRole(role);
    return p ? p->id : QString();
}

void LayoutController::onLoggedInChanged(bool loggedIn)
{
    restartIdle();
    setUnknownCode({});   // offered to the one who scanned it
    // A food truck's kitchen, bar or expo screen (a page nobody logs in to):
    // the counter signing in there leaves it where it is.
    if (const vt::layout::Page *here = currentPage(); loggedIn && atCounter() && !editing() && here
        && here->permission == u"public" && here->role != u"login") {
        refresh();
        return;
    }
    // A new store: the setup guide, for the first manager in, until it's finished.
    if (!loggedIn)
        closeSetup();
    else if (pos_ && !editing() && pos_->can(QString::fromLatin1(vt::core::perm::Manager))
             && !pos_->setupInfo().value(u"done"_s).toBool())
        openSetup();
    // Logged in, "home" is their start page (theirs, or their job's), else the
    // floor (tables); logged out, it is the login page.
    const QString login = loginPage();
    QString tables = rolePage(u"tables"_s);
    if (const QString start = pos_ ? pos_->userPrefs().value(u"startPage"_s).toString() : QString();
        loggedIn && !start.isEmpty() && activeLayout().page(start))
        tables = start;
    // A food truck or a shop: no tables, home is the menu (someone stepped
    // in at a truck too: Log Out comes back to it).
    const bool menuFirst = loggedIn && (atCounter() || steppedIn() || pos_->serviceInfo().value(u"mode"_s) == u"retail"_s);
    if (menuFirst && !nav_.indexPage().isEmpty())
        tables = nav_.indexPage();
    const QString home = loggedIn && !tables.isEmpty() ? tables : login;
    nav_.reset(home);
    if (editing_ && editor_)
        editor_->setPageId(nav_.current());
    refresh();
    emit pageChanged();
    // Back to the check they had open (Switch User): its order screen.
    if (loggedIn && pos_ && pos_->hasCheck() && !editing())
        navigate(Navigator::Mode::Index, {});
    // Stepped in at a food truck for a page the counter may not open: there now.
    if (const QString page = std::exchange(afterStepIn_, QString()); loggedIn && steppedIn() && !page.isEmpty() && !editing())
        navigate(Navigator::Mode::Push, page);
    // Opened for one thing (--after-login): there, once.
    if (loggedIn && !afterLogin_.isEmpty() && !editing()) {
        const QString page = std::exchange(afterLogin_, QString());
        if (page == u"setup")
            openSetup();
        else
            navigate(Navigator::Mode::Push, page);
    }
}

bool LayoutController::mayOpen(const QString &pageId)
{
    if (!pos_ || editing())
        return true;
    const vt::layout::Page *p = activeLayout().page(pageId);
    if (!p)
        return false;
    if (p->permission == u"public")
        return true;   // e.g. kitchen displays that nobody logs in to
    if (!pos_->loggedIn() && p->role != u"login") {
        setStatus(tr("Log in first."));
        return false;
    }
    if (!p->permission.isEmpty() && !pos_->can(p->permission)) {
        // A food truck's counter: someone's PIN (a manager's) opens it.
        if (const QString login = rolePage(u"login"_s); atCounter() && !login.isEmpty() && nav_.current() != login) {
            afterStepIn_ = pageId;
            if (nav_.jump(Navigator::Mode::Push, login)) {
                refresh();
                emit pageChanged();
            }
            setStatus(tr("Enter a manager's PIN for %1.").arg(p->name));
            return false;
        }
        setStatus(tr("%1 cannot open %2.").arg(pos_->userName(), p->name));
        return false;
    }
    return true;
}

void LayoutController::selectTable(const QString &label)
{
    if (!pos_ || editing() || busy())
        return;
    call(u"selectTable"_s, {label}, [this](const QVariant &result) {
        switch (result.toInt()) {
        case PosSession::TableOpened:
            navigate(Navigator::Mode::Index);
            break;
        case PosSession::TableNeedsGuests:
            if (const QString page = rolePage(u"guestCount"_s); !page.isEmpty()) {
                navigate(Navigator::Mode::Push, page);
            } else {
                call(u"startCheck"_s, {u"dineIn"_s}, [this](const QVariant &ok) {
                    if (ok.toBool())
                        navigate(Navigator::Mode::Index);
                });
            }
            break;
        case PosSession::TableChooseCheck:
            // Several checks at the table: pick one from the (filtered) list.
            if (const QString page = rolePage(u"checkList"_s); !page.isEmpty())
                navigate(Navigator::Mode::Push, page);
            break;
        default:
            break;
        }
    });
}

void LayoutController::payTable(const QString &label)
{
    if (!pos_ || editing() || busy())
        return;
    call(u"selectTable"_s, {label}, [this, label](const QVariant &result) {
        switch (result.toInt()) {
        case PosSession::TableOpened:
            if (const QString settle = rolePage(u"settle"_s); !settle.isEmpty())
                navigate(Navigator::Mode::Push, settle);
            else
                navigate(Navigator::Mode::Index);
            break;
        case PosSession::TableChooseCheck:
            if (const QString page = rolePage(u"checkList"_s); !page.isEmpty())
                navigate(Navigator::Mode::Push, page);
            break;
        case PosSession::TableNeedsGuests:
            setStatus(tr("No check at %1.").arg(label));
            break;
        default:
            break;
        }
    });
}

void LayoutController::openCheck(qint64 checkId)
{
    if (!pos_ || editing() || busy())
        return;
    call(u"openCheck"_s, {checkId}, [this](const QVariant &ok) {
        if (ok.toBool())
            navigate(Navigator::Mode::Index);
    });
}

void LayoutController::login()
{
    if (pos_ && !editing())
        call(u"login"_s, {}, {});   // navigation follows loggedInChanged
}

bool LayoutController::requestEditMode()
{
    if (pos_ && !pos_->can(QString::fromLatin1(vt::core::perm::EditLayout))) {
        setStatus(pos_->loggedIn() ? tr("%1 may not edit pages.").arg(pos_->userName())
                                   : tr("Log in as a manager to edit pages."));
        return false;
    }
    // A phone (a handheld) shows pages; they're made on a bigger screen,
    // and reach the phones as soon as they're saved there.
    if (formFactor_ == u"phone") {
        setStatus(tr("Edit pages on a bigger screen (the main register or a tablet); phones show the changes right away."));
        return false;
    }
    enterEditMode();
    return true;
}

namespace {
QList<vt::app::Look> allLooks(vt::app::PosSession *pos);
}

const Layout &LayoutController::activeLayout() const
{
    if (editing_ && editor_)
        return editor_->layout();
    return lookedLayout_ ? *lookedLayout_ : layout_;
}

void LayoutController::updateTerminalLook()
{
    const QString id = pos_ ? pos_->terminalLook() : QString();
    std::optional<Layout> next;
    if (!id.isEmpty()) {
        const QList<vt::app::Look> looks = allLooks(pos_);
        if (const auto it = std::ranges::find_if(looks, [&](const vt::app::Look &l) { return l.id == id; }); it != looks.end()) {
            next = layout_;
            vt::app::applyLook(next->theme, *it);
        }
    }
    const bool was = lookedLayout_.has_value();
    lookedLayout_ = std::move(next);
    lookedId_ = id;
    if (was || lookedLayout_) {
        refresh();
        emit pageChanged();
    }
}

QString LayoutController::pageName() const
{
    const auto *p = currentPage();
    return p ? p->name : QString();
}

QString LayoutController::pageKind() const
{
    const auto *p = currentPage();
    return p ? p->kind : QString();
}

QSize LayoutController::canvasSize() const
{
    return shown().canvas;
}

// --- screens of different sizes ----------------------------------------------------

void LayoutController::setViewAspect(double aspect)
{
    if (std::abs(aspect - viewAspect_) < 0.005)
        return;
    viewAspect_ = aspect;
    if (formFactor_ == u"phone" && !editing()) {
        refresh();
        emit pageChanged();
    }
}

// A phone's page made as tall as its screen (no bands above and below): the
// check (the order screen, Pay) or else the biggest panel (the floor plan)
// takes the difference, what's below it moves with its bottom. A screen shorter than the page: that panel
// gives up room, down to 240.
void LayoutController::fitToScreen(Shown &s) const
{
    if (viewAspect_ <= 0 || s.canvas.width() <= 0)
        return;
    const auto rectOf = [&](const vt::layout::Zone *z) { return s.moved.value(z, z->rect); };
    int extra = int(std::lround(s.canvas.width() * viewAspect_)) - s.canvas.height();
    // Taller: the check gets it. Shorter: the biggest other panel (the menu
    // scrolls) gives it up, so the check stays whole.
    const auto check = [](const vt::layout::Zone *z) { return z->kind == u"orderList" || z->kind == u"paymentPanel"; };
    // (The style may say the menu gets it: Modern's check is a short strip
    // that opens as a sheet.)
    const bool toCheck = extra > 0 && activeLayout().theme.extra.value(u"phoneRoomTo").toString() != u"menu";
    const vt::layout::Zone *main = nullptr;
    for (const Layout::PlacedZone &pz : s.zones) {
        const QString &k = pz.zone->kind;
        if (k == u"button" || k == u"label" || k == u"image" || k == u"table" || k == u"comment" || k == u"decor")
            continue;
        const auto rank = [&](const vt::layout::Zone *z) { return check(z) == toCheck; };
        if (!main || (rank(pz.zone) && !rank(main))
            || (rank(pz.zone) == rank(main) && rectOf(pz.zone).height() > rectOf(main).height()))
            main = pz.zone;
    }
    if (!main)
        return;
    const QRect m = rectOf(main);
    extra = std::clamp(extra, std::min(0, 240 - m.height()), s.canvas.height());
    if (std::abs(extra) < 8)
        return;
    for (const Layout::PlacedZone &pz : s.zones) {
        QRect r = rectOf(pz.zone);
        if (pz.zone == main)
            r.setHeight(r.height() + extra);
        else if (r.top() >= m.bottom())
            r.translate(0, extra);
        else
            continue;
        s.fitted.insert(pz.zone, r);
    }
    s.canvas.setHeight(s.canvas.height() + extra);
}

LayoutController::Shown LayoutController::shown() const
{
    const Layout &l = activeLayout();
    const QString id = nav_.current();
    const vt::layout::Page *page = l.page(id);
    Shown s{id, page ? page->canvas : QSize(1920, 1080), {}, {}};
    // The editor always works on the page as designed.
    if (page && formFactor_ == u"phone" && !editing() && page->variantOf.isEmpty()) {
        vt::layout::PhoneView v = vt::layout::phoneView(l, id);
        Shown phone{v.pageId, v.canvas, std::move(v.zones), std::move(v.moved), {}};
        fitToScreen(phone);
        return phone;
    }
    s.zones = l.effectiveZones(id);
    return s;
}

void LayoutController::setFormFactorOverride(const QString &formFactor)
{
    if (formFactor == u"timeClock") {   // not a page size: where it rests
        timeClockScreen_ = true;
        restAtLoginPage();
        return;
    }
    formFactorOverride_ = formFactor == u"auto" ? QString() : formFactor;
    updateFormFactor();
}

void LayoutController::setAutoFormFactor(bool on)
{
    autoFormFactor_ = on;
    updateFormFactor();
}

void LayoutController::windowResized(qreal width, qreal height)
{
    phoneSizedWindow_ = width > 0 && height > 0 && std::min(width, height) < 600;
    updateFormFactor();
}

void LayoutController::updateFormFactor()
{
    QString mode = formFactorOverride_;
    if (mode.isEmpty() && pos_)
        mode = pos_->screenMode();   // Manager -> Terminals
    if (mode != u"phone" && mode != u"standard")
        mode = autoFormFactor_ && phoneSizedWindow_ ? u"phone"_s : u"standard"_s;
    if (mode == formFactor_)
        return;
    formFactor_ = mode;
    emit formFactorChanged();
    refresh();
    emit pageChanged();
}

void LayoutController::exportReport(const QVariantMap &report, const QString &format)
{
    const QString dir = exportDir_.isEmpty() ? QDir::home().filePath(u"ViewTouch Exports"_s) : exportDir_;
    QString error;
    const QString file = format == u"pdf" ? exportReportPdf(report, dir, &error) : exportReportCsv(report, dir, &error);
    setStatus(file.isEmpty() ? tr("Could not save the report: %1").arg(error) : tr("Saved to %1").arg(file));
}

namespace {

MenuPrint menuToPrint(const PosSession &pos, const QVariantMap &o)
{
    MenuPrint m;
    m.title = o.value(u"title"_s, pos.storeName()).toString();
    m.subtitle = o.value(u"subtitle"_s).toString();
    m.columns = o.value(u"columns"_s, 2).toInt();
    m.pictures = o.value(u"pictures"_s, true).toBool();
    m.descriptions = o.value(u"descriptions"_s, true).toBool();
    m.page = QPageSize(o.value(u"paper"_s).toString() == u"a4" ? QPageSize::A4 : QPageSize::Letter);
    const QStringList only = o.value(u"categories"_s).toStringList();
    const bool soldOut = o.value(u"soldOut"_s, false).toBool();
    const QVariantList items = pos.menuItems();
    for (const QVariant &cv : pos.menuCategories()) {
        const QVariantMap c = cv.toMap();
        const QString id = c.value(u"id"_s).toString();
        if (!only.isEmpty() && !only.contains(id))
            continue;
        MenuPrintCategory cat;
        cat.name = c.value(u"name"_s).toString();
        cat.color = QColor(c.value(u"color"_s).toString());
        for (const QVariant &iv : items) {
            const QVariantMap i = iv.toMap();
            if (i.value(u"family"_s).toString() != id || i.value(u"modifier"_s).toBool())
                continue;
            if (!soldOut && !i.value(u"availableSet"_s, true).toBool())
                continue;
            MenuPrintItem item;
            item.name = i.value(u"name"_s).toString();
            item.description = i.value(u"description"_s).toString();
            item.section = i.value(u"section"_s).toString();
            const QVariantList sizes = i.value(u"sizes"_s).toList();
            for (const QVariant &sv : sizes) {
                const QVariantMap sz = sv.toMap();
                item.sizes << sz.value(u"name"_s).toString() + u" "_s + sz.value(u"priceText"_s).toString();
            }
            if (sizes.isEmpty() && i.value(u"priceValue"_s).toDouble() > 0) {   // free (water): no price
                item.price = i.value(u"regularPrice"_s).toString();
                if (i.value(u"byWeight"_s).toBool())
                    item.price += u" / "_s + i.value(u"unit"_s).toString();
            }
            if (m.pictures) {
                // A picture of its own (not an emoji drawn for it: those are for buttons).
                const QString ref = i.value(u"image"_s).toString();
                const QUrl url(pos.imageUrl(ref));
                if (!ref.isEmpty() && !ref.startsWith(u"store:emoji-"_s))
                    item.picture = QImage(url.isLocalFile() ? url.toLocalFile()
                                          : url.scheme() == u"qrc" ? u":"_s + url.path() : QString());
            }
            cat.items << item;
        }
        if (!cat.items.isEmpty())
            m.categories << cat;
    }
    return m;
}

} // namespace

QString LayoutController::printMenu(const QVariantMap &options)
{
    if (!pos_)
        return {};
    const QString dir = exportDir_.isEmpty() ? QDir::home().filePath(u"ViewTouch Exports"_s) : exportDir_;
    if (!QDir().mkpath(dir)) {
        setStatus(tr("Could not save the menu: %1").arg(tr("Cannot create %1").arg(dir)));
        return {};
    }
    const QString file = QDir(dir).filePath(u"menu-%1.pdf"_s.arg(QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s)));
    QString error;
    if (!exportMenuPdf(menuToPrint(*pos_, options), file, &error)) {
        setStatus(tr("Could not save the menu: %1").arg(error));
        return {};
    }
    setStatus(tr("Saved to %1").arg(file));
    return file;
}

QString LayoutController::menuPreview(const QVariantMap &options, int width)
{
    if (!pos_)
        return {};
    const QImage img = ::menuPreview(menuToPrint(*pos_, options), std::clamp(width, 100, 2000));
    // A new name each time, so the screen shows the new one.
    static int serial = 0;
    const QString dir = QDir::temp().filePath(u"viewtouch-menu-preview"_s);
    QDir().mkpath(dir);
    const QString mine = u"page-%1-"_s.arg(QCoreApplication::applicationPid());
    for (const QString &old : QDir(dir).entryList({mine + u"*.png"_s}, QDir::Files))
        QFile::remove(QDir(dir).filePath(old));
    const QString file = QDir(dir).filePath(mine + QString::number(++serial) + u".png"_s);
    if (!img.save(file))
        return {};
    return QUrl::fromLocalFile(file).toString();
}

int LayoutController::menuPages(const QVariantMap &options)
{
    return pos_ ? menuPageCount(menuToPrint(*pos_, options)) : 0;
}

void LayoutController::chooseLine(qint64 lineId)
{
    if (!pos_ || busy())
        return;
    call(u"chooseLine"_s, {lineId}, [this](const QVariant &ok) {
        const QString page = rolePage(u"modifiers"_s);
        if (ok.toBool() && !page.isEmpty() && nav_.current() != page)
            navigate(Navigator::Mode::Push, page);
    });
}

void LayoutController::finishChoosing()
{
    if (!pos_ || busy())
        return;
    call(u"finishChoosing"_s, {}, [this](const QVariant &ok) {
        if (ok.toBool())
            goBack();
    });
}

QVariantList LayoutController::tables() const
{
    QVariantList out;
    QSet<QString> seen;
    for (const vt::layout::Page &p : layout_.pages) {
        for (const vt::layout::Zone &z : p.zones) {
            const QString name = z.label.trimmed();
            if (z.kind != u"table" || name.isEmpty() || seen.contains(name.toLower()))
                continue;
            seen.insert(name.toLower());
            out.append(QVariantMap{{u"name"_s, name}, {u"seats"_s, z.props.value(u"seats"_s).toInt()}});
        }
    }
    // By name, numbers as numbers (T2 before T10): the same order on every
    // screen, however many floor plans show them.
    QCollator byName;
    byName.setNumericMode(true);
    byName.setCaseSensitivity(Qt::CaseInsensitive);
    std::ranges::stable_sort(out, [&](const QVariant &a, const QVariant &b) {
        return byName.compare(a.toMap().value(u"name"_s).toString(), b.toMap().value(u"name"_s).toString()) < 0;
    });
    return out;
}

QString LayoutController::screenBuild() const
{
    const vt::net::AppBuild me = vt::net::appBuild();
    return vt::net::buildText(me.version, me.number, me.commit, me.modified) + u", "_s + me.platform;
}

QVariantList LayoutController::floorPlans() const
{
    const Layout &l = activeLayout();
    const auto hasTables = [&](const vt::layout::Page &p) {
        return p.variantOf.isEmpty() && p.kind != u"template" && p.kind != u"library"
            && std::ranges::any_of(l.effectiveZones(p.id), [](const Layout::PlacedZone &pz) { return pz.zone->kind == u"table"; });
    };
    QStringList order;
    const vt::layout::Page *floor = l.pageByRole(u"tables"_s);
    if (floor && hasTables(*floor)) {
        order << floor->id;
        // Its tabs: the plans it jumps to, left to right.
        QList<const vt::layout::Zone *> jumps;
        for (const Layout::PlacedZone &pz : l.effectiveZones(floor->id))
            if (const vt::layout::Page *to = l.page(primaryJumpTarget(l, *pz.zone)); to && hasTables(*to))
                jumps << pz.zone;
        std::ranges::stable_sort(jumps, [](const auto *a, const auto *b) {
            return std::pair(a->rect.y(), a->rect.x()) < std::pair(b->rect.y(), b->rect.x());
        });
        for (const vt::layout::Zone *z : jumps)
            if (const QString id = primaryJumpTarget(l, *z); !order.contains(id))
                order << id;
    }
    for (const vt::layout::Page &p : l.pages)
        if (hasTables(p) && !order.contains(p.id))
            order << p.id;
    QVariantList out;
    for (const QString &id : order)
        out.append(QVariantMap{{u"id"_s, id}, {u"name"_s, l.page(id)->name}});
    return out;
}

QVariantMap LayoutController::floorPlan(const QString &pageId) const
{
    QVariantList zones;
    QRect box;
    for (const Layout::PlacedZone &pz : activeLayout().effectiveZones(pageId)) {
        const vt::layout::Zone &z = *pz.zone;
        if (z.kind != u"table" && z.kind != u"decor")
            continue;
        box = box.united(z.rect);
        zones.append(QVariantMap{{u"kind"_s, z.kind}, {u"label"_s, z.label}, {u"x"_s, z.rect.x()}, {u"y"_s, z.rect.y()},
                                 {u"w"_s, z.rect.width()}, {u"h"_s, z.rect.height()}, {u"rot"_s, vt::layout::rotationOf(z)},
                                 {u"shape"_s, z.shape}, {u"props"_s, z.props.toVariantMap()}});
    }
    return {{u"x"_s, box.x()}, {u"y"_s, box.y()}, {u"w"_s, box.width()}, {u"h"_s, box.height()}, {u"zones"_s, zones}};
}

int LayoutController::pageGrid() const
{
    const auto *p = currentPage();
    return p ? std::max(1, p->grid) : 8;
}

QVariantMap LayoutController::stylePalette() const
{
    const vt::layout::Theme &t = activeLayout().theme;
    QVariantMap p = t.extra.value(u"palette").toObject().toVariantMap();
    if (const QString font = t.style.normal.value(u"font").toString(); !font.isEmpty() && !p.contains(u"font"_s))
        p.insert(u"font"_s, font);
    return p;
}

QVariantMap LayoutController::background() const
{
    return activeLayout().resolveBackground(shown().pageId).toVariantMap();
}

void LayoutController::activate(const QString &zoneId)
{
    if (editing())
        return;   // the editor overlay owns touches
    clearExplanation();
    if (busy())
        return;   // still waiting for the server on the previous touch
    for (const Layout::PlacedZone &pz : shown().zones) {
        if (pz.zone->id != zoneId)
            continue;
        if (!pz.zone->enabled)
            return;
        runChain(pz.zone->actions, 0, nav_.current());
        return;
    }
    qCWarning(lcLayout) << "activate: no zone" << zoneId << "on page" << nav_.current();
}

void LayoutController::runChain(QList<Action> actions, int index, QString startPage)
{
    if (index >= actions.size())
        return;
    const Action action = actions.at(index);
    runAction(action, [this, actions = std::move(actions), index, startPage = std::move(startPage)](bool ok) {
        // A page change ends the chain: the rest belonged to the page left.
        if (!ok || nav_.current() != startPage || editing())
            return;
        runChain(actions, index + 1, startPage);
    });
}

void LayoutController::goBack()
{
    navigate(Navigator::Mode::Back);
}

void LayoutController::goHome()
{
    navigate(Navigator::Mode::Home);
}

bool LayoutController::jumpTo(const QString &pageId)
{
    if (!activeLayout().page(pageId))
        return false;
    return navigate(Navigator::Mode::Push, pageId) || nav_.current() == pageId;
}

void LayoutController::setMenuCategory(const QString &id)
{
    if (id == menuCategory_)
        return;
    menuCategory_ = id;
    emit menuCategoryChanged();
}

QVariantMap LayoutController::readMenuFile(const QUrl &file) const
{
    QFile f(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (f.size() > 4 * 1024 * 1024)
        return {{u"error"_s, tr("That file is too big for a menu.")}};
    if (!f.open(QIODevice::ReadOnly))
        return {{u"error"_s, tr("Can't open it: %1").arg(f.errorString())}};
    QByteArray bytes = f.readAll();
    // Another store's menu (Export...): shown like a spreadsheet's rows.
    if (const QJsonDocument doc = QJsonDocument::fromJson(bytes); doc.isArray() && vt::app::isToastMenu(doc)) {
        return menuRows(vt::app::readToastMenu(doc), {});
    } else if (doc.isObject()) {
        const QVariantMap menu = doc.object().toVariantMap();
        if (vt::app::isToastMenu(doc))
            return menuRows(vt::app::readToastMenu(doc), {});
        if (menu.value(u"format"_s) != u"viewtouch-menu"_s)
            return {{u"error"_s, tr("That isn't a ViewTouch, Toast or spreadsheet menu file.")}};
        QHash<QString, QString> categoryName;
        for (const QVariant &c : menu.value(u"categories"_s).toList())
            categoryName.insert(c.toMap().value(u"id"_s).toString(), c.toMap().value(u"name"_s).toString());
        QVariantList items;
        for (const QVariant &v : menu.value(u"items"_s).toList()) {
            const QVariantMap i = v.toMap();
            if (i.value(u"modifier"_s).toBool())
                continue;
            const QString family = i.value(u"family"_s).toString();
            const QString image = i.value(u"image"_s).toString();
            items.append(QVariantMap{{u"name"_s, i.value(u"name"_s)}, {u"price"_s, i.value(u"price"_s).toDouble()},
                                     {u"category"_s, categoryName.value(family, family)},
                                     {u"photo"_s, image.startsWith(u"store:"_s)
                                                      && menu.value(u"pictures"_s).toMap().contains(image.mid(6))}});
        }
        return {{u"items"_s, items}, {u"menuFile"_s, menu}, {u"problems"_s, QStringList()},
                {u"from"_s, menu.value(u"store"_s)}};
    }
    vt::app::MenuImport read;
    if (bytes.startsWith("PK")) {   // an Excel workbook (Square's and Clover's exports)
        QString error;
        const auto sheets = vt::app::readXlsx(bytes, &error);
        if (!sheets)
            return {{u"error"_s, error}};
        read = vt::app::readMenuSheets(*sheets);
    } else {
        // UTF-8, else Latin-1 (older spreadsheets).
        QString text = QString::fromUtf8(bytes);
        if (text.contains(QChar::ReplacementCharacter))
            text = QString::fromLatin1(bytes);
        read = vt::app::readMenuCsv(text);
    }
    return menuRows(read, QFileInfo(f.fileName()).absoluteDir());
}

// What a file read: rows for importMenuRows (photos beside the file read
// in), the choice groups, what's left out.
QVariantMap LayoutController::menuRows(vt::app::MenuImport read, const QDir &beside) const
{
    QVariantList items;
    int photos = 0;
    for (const vt::app::ImportedItem &i : read.items) {
        QVariantMap row{{u"row"_s, i.row}, {u"name"_s, i.name}, {u"price"_s, i.price}, {u"category"_s, i.category},
                        {u"description"_s, i.description}, {u"onIt"_s, i.onIt}, {u"barcodes"_s, i.barcode}};
        if (!i.section.isEmpty())
            row.insert(u"section"_s, i.section);
        if (!i.number.isEmpty())
            row.insert(u"number"_s, i.number);
        if (i.alcohol)
            row.insert(u"alcohol"_s, true);
        if (!i.groups.isEmpty())
            row.insert(u"groups"_s, i.groups);
        if (!i.sizes.isEmpty()) {
            QVariantList sizes;
            for (const vt::app::ImportedPrice &s : i.sizes)
                sizes.append(QVariantMap{{u"name"_s, s.name}, {u"price"_s, s.price}});
            row.insert(u"sizes"_s, sizes);
        }
        if (!i.photo.isEmpty()) {
            QString path = QDir::isAbsolutePath(i.photo) ? i.photo : beside.filePath(i.photo);
            if (!QFileInfo::exists(path))   // "Tacos.JPG" for tacos.jpg
                for (const QString &n : beside.entryList(QDir::Files))
                    if (n.compare(QFileInfo(i.photo).fileName(), Qt::CaseInsensitive) == 0)
                        path = beside.filePath(n);
            QFile picture(path);
            if (vt::app::PosSession::storeImageRef(path).isEmpty())
                read.problems << tr("Row %1: %2 isn't a picture (PNG, JPEG, WebP…)").arg(i.row).arg(i.photo);
            else if (picture.size() > 8 * 1024 * 1024)
                read.problems << tr("Row %1: %2 is too big (8 MB at most)").arg(i.row).arg(i.photo);
            else if (!picture.open(QIODevice::ReadOnly))
                read.problems << tr("Row %1: no picture %2 next to the spreadsheet").arg(i.row).arg(i.photo);
            else {
                row.insert(u"photoName"_s, QFileInfo(path).fileName());
                row.insert(u"photoData"_s, QString::fromLatin1(picture.readAll().toBase64()));
                ++photos;
            }
        }
        items.append(row);
    }
    QVariantList groups;
    for (const vt::app::ImportedGroup &g : read.groups) {
        QVariantList options;
        for (const vt::app::ImportedPrice &o : g.options)
            options.append(QVariantMap{{u"name"_s, o.name}, {u"price"_s, QString::number(o.price, 'f', 2)}});
        groups.append(QVariantMap{{u"name"_s, g.name}, {u"min"_s, g.min}, {u"max"_s, g.max}, {u"options"_s, options}});
    }
    return {{u"items"_s, items}, {u"groups"_s, groups}, {u"problems"_s, read.problems}, {u"columns"_s, read.columns},
            {u"photos"_s, photos}, {u"from"_s, read.from}};
}

QString LayoutController::emojiPicture(const QString &emoji)
{
    if (!pos_ || emoji.isEmpty())
        return {};
    QStringList code;
    for (const char32_t c : emoji.toUcs4())
        if (c != 0xfe0f)
            code << QString::number(c, 16);
    const QString name = u"emoji-"_s + code.join(u'-') + u".png"_s;
    QImage img(256, 256, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        QFont f(u"Noto Color Emoji"_s);
        f.setFamilies({u"Noto Color Emoji"_s, u"Apple Color Emoji"_s, u"Segoe UI Emoji"_s, u"Noto Emoji"_s});
        f.setPixelSize(196);
        p.setFont(f);
        p.drawText(img.rect(), Qt::AlignCenter, emoji);
    }
    const QString dir = QDir::temp().filePath(u"viewtouch-pictures"_s);
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(name);
    if (!img.save(path, "PNG"))
        return {};
    const QString ref = pos_->addImageFile(path);
    QFile::remove(path);
    return ref;
}

QString LayoutController::exportMenuSheet()
{
    if (!pos_ || !pos_->can(QString::fromLatin1(vt::core::perm::Manager)))
        return {};
    // The headings Import… reads back.
    const auto field = [](QString s) {
        if (s.contains(u',') || s.contains(u'"') || s.contains(u'\n')) {
            s.replace(u"\""_s, u"\"\""_s);
            s = u'"' + s + u'"';
        }
        return s;
    };
    QStringList lines{u"Name,Category,Price,Cost,Tax,PLU,Barcode,On Hand,Low At,Sold Out"_s};
    for (const QVariant &v : pos_->menuItems()) {
        const QVariantMap i = v.toMap();
        if (i.value(u"modifier"_s).toBool())
            continue;
        lines << QStringList{field(i.value(u"name"_s).toString()), field(i.value(u"familyName"_s).toString()),
                             QString::number(i.value(u"priceValue"_s).toDouble(), 'f', 2), i.value(u"cost"_s).toString(),
                             i.value(u"taxClass"_s).toString(), i.value(u"number"_s).toString(),
                             field(i.value(u"barcodes"_s).toString()), i.value(u"stockOnHand"_s).toString(),
                             i.value(u"stockLowAt"_s).toString(),
                             i.value(u"availableSet"_s).toBool() ? QString() : u"Yes"_s}.join(u',');
    }
    const QString dir = exportDir_.isEmpty() ? QDir::home().filePath(u"ViewTouch Exports"_s) : exportDir_;
    QString store = pos_->storeName();
    store.replace(QRegularExpression(u"[\\/:*?\"<>|]"_s), u"-"_s);
    const QString path = QDir(dir).filePath(u"Items - %1 - %2.csv"_s.arg(store.isEmpty() ? u"ViewTouch"_s : store,
                                                                        QDate::currentDate().toString(Qt::ISODate)));
    QSaveFile f(path);
    // With a byte-order mark: Excel then reads the names' accents right.
    if (!QDir().mkpath(dir) || !f.open(QIODevice::WriteOnly)
        || f.write("\xEF\xBB\xBF" + lines.join(u"\r\n"_s).toUtf8() + "\r\n") < 0 || !f.commit()) {
        setStatus(tr("Could not save the menu: %1").arg(f.errorString()));
        return {};
    }
    setStatus(tr("Saved to %1").arg(path));
    return path;
}

void LayoutController::exportMenu()
{
    if (!pos_)
        return;
    call(u"menuExport"_s, {}, [this](const QVariant &result) {
        const QVariantMap menu = result.toMap();
        if (menu.isEmpty())
            return;   // not allowed: already said
        const QString dir = exportDir_.isEmpty() ? QDir::home().filePath(u"ViewTouch Exports"_s) : exportDir_;
        QString store = menu.value(u"store"_s).toString();
        store.replace(QRegularExpression(u"[\\/:*?\"<>|]"_s), u"-"_s);
        const QString path = QDir(dir).filePath(u"Menu - %1 - %2.vtmenu.json"_s
                                                    .arg(store.isEmpty() ? u"ViewTouch"_s : store,
                                                         QDate::currentDate().toString(Qt::ISODate)));
        QSaveFile f(path);
        if (!QDir().mkpath(dir) || !f.open(QIODevice::WriteOnly)
            || f.write(QJsonDocument(QJsonObject::fromVariantMap(menu)).toJson()) < 0 || !f.commit()) {
            setStatus(tr("Could not save the menu: %1").arg(f.errorString()));
            return;
        }
        setStatus(tr("Saved to %1").arg(path));
    });
}

namespace {

QString orders(const vt::layout::Zone &z)
{
    for (const vt::layout::Action &a : z.actions)
        if (a.type() == u"addItem")
            return a.str(u"item");
    return {};
}

} // namespace

QVariantList LayoutController::handBuiltPages(const QString &itemId) const
{
    QVariantList out;
    for (const vt::layout::Page &p : activeLayout().pages) {
        if (p.kind != u"items" && p.kind != u"custom" && p.kind != u"index")
            continue;
        const bool selfFilling = std::ranges::any_of(p.zones, [](const vt::layout::Zone &z) {
            return z.kind == u"menuGrid" || z.kind == u"menuCategories";
        });
        const bool itemButtons = std::ranges::any_of(p.zones, [](const vt::layout::Zone &z) { return !orders(z).isEmpty(); });
        if (selfFilling || !itemButtons)
            continue;
        const bool has = std::ranges::any_of(p.zones, [&](const vt::layout::Zone &z) { return orders(z) == itemId; });
        out.append(QVariantMap{{u"id"_s, p.id}, {u"name"_s, p.name}, {u"has"_s, has}});
    }
    return out;
}

bool LayoutController::addItemButton(const QString &pageId, const QString &itemId, const QString &label)
{
    if (!pos_ || !pos_->can(QString::fromLatin1(vt::core::perm::EditLayout))) {
        setStatus(tr("Changing pages needs a manager."));
        return false;
    }
    if (editing()) {
        setStatus(tr("Finish editing pages first."));
        return false;
    }
    Layout l = layout_;
    vt::layout::Page *p = l.page(pageId);
    if (!p)
        return false;
    // Like the page's usual item button (its most common size): its size,
    // shape and look, on the grid they're on, spaced as they are.
    QHash<QPair<int, int>, int> sizes;
    for (const vt::layout::Zone &z : p->zones)
        if (!orders(z).isEmpty())
            ++sizes[{z.rect.width(), z.rect.height()}];
    if (sizes.isEmpty())
        return false;
    QPair<int, int> common = sizes.keyBegin().operator*();
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it)
        if (it.value() > sizes.value(common))
            common = it.key();
    const QSize size(common.first, common.second);
    const vt::layout::Zone *like = nullptr;
    int left = INT_MAX, top = INT_MAX, gapX = INT_MAX, gapY = INT_MAX;
    for (const vt::layout::Zone &z : p->zones) {
        if (orders(z).isEmpty() || z.rect.size() != size)
            continue;
        if (!like)
            like = &z;
        left = std::min(left, z.rect.left());
        top = std::min(top, z.rect.top());
        for (const vt::layout::Zone &o : p->zones) {
            if (&o == &z || orders(o).isEmpty() || o.rect.size() != size)
                continue;
            if (o.rect.top() == z.rect.top() && o.rect.left() > z.rect.right())
                gapX = std::min(gapX, o.rect.left() - z.rect.left() - size.width());
            if (o.rect.left() == z.rect.left() && o.rect.top() > z.rect.bottom())
                gapY = std::min(gapY, o.rect.top() - z.rect.top() - size.height());
        }
    }
    if (gapX == INT_MAX) gapX = 16;
    if (gapY == INT_MAX) gapY = 16;
    const vt::layout::Page *tmpl = p->templateId.isEmpty() ? nullptr : l.page(p->templateId);
    const auto clear = [&](const QList<vt::layout::Zone> &zones, const QRect &r) {
        return std::ranges::none_of(zones, [&](const vt::layout::Zone &z) {
            return z.kind != u"comment" && z.rect.intersects(r);   // notes show only while editing
        });
    };
    // The next free place, row by row.
    QRect at;
    for (int y = top; y + size.height() <= p->canvas.height() && at.isNull(); y += size.height() + gapY)
        for (int x = left; x + size.width() <= p->canvas.width(); x += size.width() + gapX) {
            const QRect r(QPoint(x, y), size);
            if (clear(p->zones, r) && (!tmpl || clear(tmpl->zones, r))) {   // the order bar, the check
                at = r;
                break;
            }
        }
    if (at.isNull()) {
        setStatus(tr("%1 is full: make room in the page editor.").arg(p->name));
        return false;
    }
    vt::layout::Zone z = *like;
    z.label = label;
    z.rect = at;
    z.actions.clear();
    vt::layout::Action add;
    add.data = {{u"type"_s, u"addItem"_s}, {u"item"_s, itemId}};
    z.actions.append(add);
    QString id = u"item-"_s + itemId;
    for (int n = 2; p->zone(id); ++n)
        id = u"item-"_s + itemId + u'-' + QString::number(n);
    z.id = id;
    p->zones.append(z);
    if (saver_) {
        QString error;
        if (!saver_(l, &error)) {
            setStatus(tr("Could not save: %1").arg(error));
            return false;
        }
    }
    replaceLayout(l);
    setStatus(tr("%1 is on %2 now").arg(label, p->name));
    return true;
}

int LayoutController::removeItemButtons(const QString &itemId)
{
    if (!pos_ || editing() || itemId.isEmpty() || !pos_->can(QString::fromLatin1(vt::core::perm::EditLayout)))
        return 0;
    Layout l = layout_;
    int removed = 0;
    for (vt::layout::Page &p : l.pages)
        removed += int(p.zones.removeIf([&](const vt::layout::Zone &z) {
            // Only buttons that do nothing else.
            return orders(z) == itemId && z.actions.size() == 1;
        }));
    if (removed == 0)
        return 0;
    if (saver_) {
        QString error;
        if (!saver_(l, &error)) {
            setStatus(tr("Could not save: %1").arg(error));
            return 0;
        }
    }
    replaceLayout(l);
    return removed;
}

bool LayoutController::menuScreensHandBuilt() const
{
    return vt::layout::hasHandBuiltMenu(layout_);
}

bool LayoutController::switchToSelfFillingMenu()
{
    if (!vt::layout::hasHandBuiltMenu(layout_))
        return false;
    if (!requestEditMode())
        return false;
    const vt::layout::Layout before = editor_->layout();
    const vt::layout::Layout switched = vt::layout::withSelfFillingMenu(before);
    if (!editor_->editor().replaceLayout(switched, tr("Self-filling menu screens")))
        return false;
    // Shown: the first meal page that changed.
    for (const vt::layout::Page &p : switched.pages) {
        const vt::layout::Page *was = before.page(p.id);
        if (p.kind == u"index" && was && !(was->zones == p.zones)) {
            showPage(p.id);
            break;
        }
    }
    setStatus(tr("Your menu pages now fill themselves. Look them over, then Save (Undo puts them back)."));
    return true;
}

bool LayoutController::openCategory(const QString &categoryId)
{
    setMenuCategory(categoryId);
    QString page = rolePage(u"menu"_s);
    if (page.isEmpty())
        page = u"menu-all"_s;
    return jumpTo(page);
}

bool LayoutController::showPage(const QString &pageId)
{
    if (!activeLayout().page(pageId))
        return false;
    return navigate(Navigator::Mode::Replace, pageId) || nav_.current() == pageId;
}

// A zone's show/hide rules (its "showWhen"), as ZoneItem.qml applies them.
bool LayoutController::ruleShows(const QJsonObject &rule) const
{
    if (rule.isEmpty())
        return true;
    const QString login = rule.value(u"login").toString();
    const bool in = pos_ && pos_->loggedIn();
    if ((login == u"loggedIn" && !in) || (login == u"loggedOut" && in)
        || (login == u"manager" && !(pos_ && pos_->can(QString::fromLatin1(vt::core::perm::Manager)))))
        return false;
    const QString check = rule.value(u"check").toString();
    const bool open = pos_ && pos_->hasCheck();
    if ((check == u"open" && !open) || (check == u"none" && open))
        return false;
    const QString type = rule.value(u"checkType").toString();
    if (!type.isEmpty() && (!open || pos_->checkInfo().value(u"type"_s).toString() != type))
        return false;
    const QString period = rule.value(u"mealPeriod").toString();
    if (!period.isEmpty() && period != mealPeriod())
        return false;
    const QString screen = rule.value(u"screen").toString();
    if (!screen.isEmpty() && screen != formFactor())
        return false;
    // How the screen serves ("restaurant", "foodTruck", "retail"; several: commas between).
    const QString service = rule.value(u"service").toString();
    const QString mode = pos_ ? pos_->serviceInfo().value(u"mode"_s).toString() : QString();
    if (!service.isEmpty() && !service.split(u',').contains(mode.isEmpty() ? u"restaurant"_s : mode))
        return false;
    // The store's style (Store Settings -> Screens -> Style): a button only one style has.
    const QString style = rule.value(u"style").toString();
    return style.isEmpty() || style == vt::layout::Layout::styleOf(activeLayout().theme);
}

bool LayoutController::triggerHotkey(const QString &key)
{
    if (key.isEmpty() || editing())
        return false;
    const auto zones = shown().zones;
    // Topmost zone wins, matching touch order.
    for (auto it = zones.rbegin(); it != zones.rend(); ++it) {
        if (it->zone->enabled && it->zone->hotkey.compare(key, Qt::CaseInsensitive) == 0
            && ruleShows(it->zone->extra.value(u"showWhen").toObject())) {
            activate(it->zone->id);
            return true;
        }
    }
    return false;
}

// --- edit mode -------------------------------------------------------------------

void LayoutController::enterEditMode()
{
    if (editing_)
        return;
    editor_ = new EditorController(layout_, this);
    editor_->setPos(pos_);
    if (pos_)
        editor_->setMealPeriods(pos_->mealPeriods());
    connect(editor_, &EditorController::layoutChanged, this, &LayoutController::onDraftChanged);
    connect(editor_, &EditorController::showPageRequested, this, &LayoutController::showPage);
    emit editorChanged();
    editing_ = true;
    nav_.setLayout(editor_->layout());
    editor_->setPageId(nav_.current());
    emit editingChanged();
    refresh();
    emit pageChanged();
}

bool LayoutController::saveEdits()
{
    if (!editing_ || !editor_)
        return false;
    const Layout draft = editor_->layout();
    // Templates are the owner's; the store's own pages, the maintainer's.
    if (pos_) {
        bool system = false;
        if (const QString what = vt::layout::protectedEdit(layout_, draft, &system); !what.isEmpty()) {
            const char *needs = system ? vt::core::perm::Maintain : vt::core::perm::Owner;
            if (!pos_->can(QString::fromLatin1(needs))) {
                setStatus((system ? tr("Only the maintainer can change %1. Undo that change, or Discard.")
                                  : tr("Only the owner can change %1. Undo that change, or Discard.")).arg(what));
                return false;
            }
        }
    }
    if (saver_) {
        QString error;
        if (!saver_(draft, &error)) {
            setStatus(tr("Could not save: %1").arg(error));
            return false;
        }
    }
    // For the Changes report: the pages added, removed and changed.
    if (pos_) {
        QStringList added, removed, changed;
        for (const vt::layout::Page &p : draft.pages) {
            const vt::layout::Page *was = layout_.page(p.id);
            if (!was)
                added << p.name;
            else if (!(*was == p))
                changed << p.name;
        }
        for (const vt::layout::Page &p : layout_.pages)
            if (!draft.page(p.id))
                removed << p.name;
        if (layout_.theme != draft.theme)
            changed << tr("Theme");
        if (!added.isEmpty() || !removed.isEmpty() || !changed.isEmpty())
            pos_->logPageEdits(added, removed, changed);
    }
    layout_ = draft;
    emit menuScreensChanged();
    if (lookedLayout_)
        updateTerminalLook();
    editor_->editor().markClean();
    setStatus(saver_ ? tr("Saved") : tr("Applied (not saved to disk)"));
    return true;
}

bool LayoutController::leaveEditMode(bool save)
{
    if (!editing_)
        return true;
    if (save && editor_->dirty() && !saveEdits())
        return false;

    nav_.setLayout(layout_);
    // Left on a phone version: the page itself (a phone shows its version by itself).
    if (const vt::layout::Page *p = layout_.page(nav_.current()); p && !p->variantOf.isEmpty() && layout_.page(p->variantOf))
        nav_.jump(Navigator::Mode::Replace, p->variantOf);
    editing_ = false;
    emit editingChanged();   // editor panels unload while the editor still exists

    EditorController *old = editor_;
    disconnect(old, nullptr, this, nullptr);
    connect(old, &QObject::destroyed, this, &LayoutController::editorChanged);
    old->deleteLater();

    nav_.setHome(loginPage());
    ensureCurrentPageExists();
    refresh();
    emit pageChanged();
    return true;
}

void LayoutController::onDraftChanged()
{
    nav_.setHome(loginPage());
    ensureCurrentPageExists();
    refresh();
    emit pageChanged();
}

void LayoutController::ensureCurrentPageExists()
{
    if (!activeLayout().page(nav_.current()))
        nav_.jump(Navigator::Mode::Back);   // skips vanished pages, falls back home
    if (editing_ && editor_)
        editor_->setPageId(nav_.current());
}

// --- actions ---------------------------------------------------------------------

void LayoutController::openSetup()
{
    if (!pos_ || !pos_->can(QString::fromLatin1(vt::core::perm::Manager))) {
        setStatus(tr("The setup guide is for managers."));
        return;
    }
    if (!setupOpen_) {
        setupOpen_ = true;
        emit setupOpenChanged();
    }
}

void LayoutController::closeSetup()
{
    if (setupOpen_) {
        setupOpen_ = false;
        emit setupOpenChanged();
    }
}

QStringList LayoutController::fontFamilies() const
{
    return QFontDatabase::families();
}

void LayoutController::installStoreFonts()
{
    if (!pos_)
        return;
    bool added = false;
    for (const QVariant &v : pos_->storeImages()) {
        const QVariantMap f = v.toMap();
        const QString hash = f.value(u"hash"_s).toString();
        if (f.value(u"kind"_s).toString() != u"font" || installedFonts_.contains(hash))
            continue;
        // Its file here (a paired screen gets it from the store first).
        const QString url = pos_->imageUrl(f.value(u"ref"_s).toString());
        if (url.isEmpty())
            continue;
        QFile file(QUrl(url).toLocalFile());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        if (QFontDatabase::addApplicationFontFromData(file.readAll()) >= 0) {
            installedFonts_.insert(hash);
            added = true;
        }
    }
    if (added)
        emit fontsChanged();
}

namespace {
QList<vt::app::Look> allLooks(vt::app::PosSession *pos)
{
    QList<vt::app::Look> looks = vt::app::builtInLooks();
    if (!pos)
        return looks;
    const QString url = pos->imageUrl(u"logo:"_s);
    if (url.isEmpty())
        return looks;
    const QList<QColor> colors = vt::app::mainColors(QImage(QUrl(url).toLocalFile()));
    if (!colors.isEmpty()) {
        looks.prepend(vt::app::lookFromColors(colors, false));
        looks.prepend(vt::app::lookFromColors(colors, true));
    }
    return looks;
}
} // namespace

QVariantList LayoutController::looks() const
{
    QVariantList out;
    for (const vt::app::Look &l : allLooks(pos_))
        out.append(QVariantMap{{u"id"_s, l.id}, {u"name"_s, l.name},
                               {u"colors"_s, QStringList{l.background.name(), l.surface.name(), l.panel.name(),
                                                         l.text.name(), l.accent.name()}}});
    return out;
}

bool LayoutController::applyLook(const QString &id)
{
    const QList<vt::app::Look> looks = allLooks(pos_);
    const auto it = std::ranges::find_if(looks, [&](const vt::app::Look &l) { return l.id == id; });
    if (it == looks.end())
        return false;
    if (editing_ && editor_) {
        vt::layout::Theme theme = editor_->editor().layout().theme;
        vt::app::applyLook(theme, *it);
        const bool ok = editor_->editor().setTheme(theme, tr("Look: %1").arg(it->name));
        if (ok)
            setStatus(tr("Look: %1 (Save keeps it)").arg(it->name));
        return ok;
    }
    if (pos_ && !pos_->can(QString::fromLatin1(vt::core::perm::EditLayout))) {
        setStatus(tr("Changing the look needs a manager."));
        return false;
    }
    Layout changed = layout_;
    vt::app::applyLook(changed.theme, *it);
    if (saver_) {
        QString error;
        if (!saver_(changed, &error)) {
            setStatus(tr("Could not save: %1").arg(error));
            return false;
        }
    }
    replaceLayout(changed);
    setStatus(tr("Look: %1").arg(it->name));
    return true;
}

bool LayoutController::numberKey(const QString &key)
{
    const auto *page = currentPage();
    if (editing() || !pos_ || !pos_->loggedIn() || !page
        || (page->kind != u"index" && page->kind != u"items" && page->kind != u"modifier"))
        return false;
    if (key.size() == 1 && key[0].isDigit()) {
        if (typedNumber_.size() < 6)
            typedNumber_ += key;
    } else if (key == u"back" && !typedNumber_.isEmpty()) {
        typedNumber_.chop(1);
    } else if (key == u"escape" && !typedNumber_.isEmpty()) {
        typedNumber_.clear();
        setStatus(QString());
        return true;
    } else if (key == u"enter" && !typedNumber_.isEmpty()) {
        const QString number = std::exchange(typedNumber_, QString());
        for (const QVariant &v : pos_->menuItems())
            if (v.toMap().value(u"number"_s).toString() == number) {
                orderItem(v.toMap().value(u"id"_s).toString());
                return true;
            }
        setStatus(tr("No item has number %1.").arg(number));
        return true;
    } else {
        return false;
    }
    setStatus(typedNumber_.isEmpty() ? QString() : tr("#%1, then Enter").arg(typedNumber_));
    return true;
}

void LayoutController::scanned(const QString &code)
{
    const QString c = code.trimmed();
    if (editing() || !pos_ || !pos_->loggedIn() || c.isEmpty())
        return;
    typedNumber_.clear();
    setUnknownCode({});
    const auto *page = currentPage();
    const bool ordering = page && (page->kind == u"index" || page->kind == u"items" || page->kind == u"modifier");
    // One barcode, but for leading zeros (a scanner may read a UPC as EAN-13).
    const auto sameCode = [](const QString &a, const QString &b) {
        static const QRegularExpression digits(u"^[0-9]{6,}$"_s);
        if (a == b)
            return true;
        if (!digits.match(a).hasMatch() || !digits.match(b).hasMatch())
            return false;
        const auto strip = [](const QString &s) { qsizetype i = 0; while (i < s.size() && s[i] == u'0') ++i; return s.mid(i); };
        return strip(a) == strip(b);
    };
    for (const QVariant &v : pos_->menuItems()) {
        const QVariantMap m = v.toMap();
        const QStringList codes = m.value(u"barcodes"_s).toString().split(u", "_s, Qt::SkipEmptyParts);
        if (m.value(u"number"_s).toString() != c
            && std::ranges::none_of(codes, [&](const QString &b) { return sameCode(b, c); }))
            continue;
        if (ordering) {
            orderItem(m.value(u"id"_s).toString());
        } else if (pos_->serviceInfo().value(u"mode"_s) == u"retail"_s) {
            // A shop scans from anywhere (the Pay screen, the next customer's): back to the sale, rung in.
            navigate(Navigator::Mode::Index);
            orderItem(m.value(u"id"_s).toString());
        } else {
            setStatus(tr("%1: ring it in on the order screen.").arg(m.value(u"name"_s).toString()));
        }
        return;
    }
    call(u"scanCode"_s, {c}, [this, c](const QVariant &kind) {
        if (kind.toString() == u"giftCard" && pageId() != u"gift-card" && activeLayout().page(u"gift-card"_s))
            navigate(Navigator::Mode::Push, u"gift-card"_s);
        // Nothing has it: a manager may make it a new item's barcode.
        else if (kind.toString().isEmpty() && pos_ && pos_->can(QString::fromLatin1(vt::core::perm::Manager))
                 && activeLayout().page(u"menu-builder"_s))
            setUnknownCode(c);
    });
}

void LayoutController::setUnknownCode(const QString &code)
{
    if (unknownCode_ == code)
        return;
    unknownCode_ = code;
    emit unknownCodeChanged();
}

void LayoutController::addItemForCode()
{
    const QString code = unknownCode_;
    setUnknownCode({});
    if (code.isEmpty())
        return;
    keep(u"menuBuilderNewCode"_s, code);   // the Menu Builder opens a new card with it
    navigate(Navigator::Mode::Push, u"menu-builder"_s);
}

void LayoutController::dismissUnknownCode()
{
    setUnknownCode({});
}

void LayoutController::orderItem(const QString &itemId, bool clearTyped)
{
    if (editing() || busy())
        return;
    if (clearTyped && pos_)
        pos_->invoke(u"textKey"_s, {u"clear"_s});
    // As a button that adds it would: then its choices, or its weight.
    Action a;
    a.data = QJsonObject{{u"type"_s, u"addItem"_s}, {u"item"_s, itemId}};
    runAction(a, [](bool) {});
}

void LayoutController::buzz(bool refused)
{
    if (!pos_ || !pos_->userPrefs().value(u"buzz"_s, true).toBool())
        return;
    ++buzzes_;
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("org/viewtouch/pos/Haptics", "buzz", "(Landroid/content/Context;Z)V",
                                       QNativeInterface::QAndroidApplication::context().object(), jboolean(refused));
#else
    Q_UNUSED(refused)
#endif
}

void LayoutController::runAction(const Action &a, Done done)
{
    const QString type = a.type();

    if (type == u"jump") {
        const QString modeName = a.str(u"mode");
        const auto mode = Navigator::parseMode(modeName);
        if (!mode) {
            setStatus(tr("Unknown jump mode '%1'").arg(modeName));
            return done(false);
        }
        const QString target = layout_.resolveTarget(a.data);
        if ((*mode == Navigator::Mode::Push || *mode == Navigator::Mode::Replace) && target.isEmpty()) {
            setStatus(tr("This button's page does not exist"));
            return done(false);
        }
        return done(navigate(*mode, target));
    }

    if (type == u"addItem") {
        const QString item = a.str(u"item");
        QStringList sequence;
        for (const QJsonValue &v : a.data.value(u"modifierSequence").toArray())
            sequence.append(v.toString());
        auto added = [this, item, sequence, done](bool ok) {
            if (!ok) {
                buzz(true);
                return done(false);
            }
            emit itemAdded(item);
            buzz();
            if (pos_) {
                QString name = item;
                for (const QVariant &v : pos_->menuItems())
                    if (v.toMap().value(u"id"_s).toString() == item)
                        name = v.toMap().value(u"name"_s).toString();
                const int seat = pos_->checkInfo().value(u"seat"_s).toInt();
                emit addedFlash(seat > 0 ? tr("+ %1 · Seat %2").arg(name).arg(seat) : tr("+ %1").arg(name), !pos_->online() && !pos_->offlineMode());
            }
            if (!sequence.isEmpty() && nav_.startSequence(sequence)) {
                refresh();
                emit pageChanged();
            } else if (pos_ && pos_->weighingInfo().value(u"active"_s).toBool()) {
                // Sold by weight: how much, on the Weigh page.
                if (const QString page = rolePage(u"weigh"_s); !page.isEmpty())
                    navigate(Navigator::Mode::Push, page);
            } else if (pos_ && pos_->choosingInfo().value(u"active"_s).toBool()) {
                // Its modifier groups: choose on the modifiers page.
                if (const QString page = rolePage(u"modifiers"_s); !page.isEmpty())
                    navigate(Navigator::Mode::Push, page);
            }
            done(true);
        };
        if (!pos_) {
            setStatus(tr("Added %1").arg(item));
            return added(true);
        }
        return call(u"addItem"_s, {item}, [added](const QVariant &ok) { added(ok.toBool()); });
    }

    if (type == u"qualifier") {
        if (!pos_) {
            setStatus(tr("Qualifier: %1").arg(a.str(u"qualifier")));
            return done(true);
        }
        return call(u"setQualifier"_s, {a.str(u"qualifier")}, [this, done](const QVariant &) {
            done(true);
            // No / Lite / Side go on a choice: the touched item's (or the newest
            // one's) Choose page opens, if it has choices and isn't sent. (Extra
            // can also be a bigger portion of the next item; Sub swaps one in.)
            const QString q = pos_ ? pos_->pendingQualifier() : QString();
            if ((q != u"no" && q != u"lite" && q != u"side") || nav_.current() == rolePage(u"modifiers"_s))
                return;
            qint64 line = 0;
            for (const QVariant &v : pos_->lines()) {
                const QVariantMap l = v.toMap();
                const bool choices = l.value(u"choices"_s).toBool();
                if (l.value(u"selected"_s).toBool()) {   // the touched item, or nothing
                    line = choices ? l.value(u"id"_s).toLongLong() : 0;
                    break;
                }
                if (choices)
                    line = l.value(u"id"_s).toLongLong();   // else the newest with choices
            }
            if (line)
                QMetaObject::invokeMethod(this, [this, line] { chooseLine(line); }, Qt::QueuedConnection);
        });
    }

    if (type == u"tender") {
        if (!pos_) {
            setStatus(tr("Tender: %1").arg(a.str(u"tender")));
            return done(true);
        }
        const QJsonValue amount = a.data.value(u"amount");
        // A card, on a screen with a card reader: the reader takes it (one
        // beside the screen is run by the store's computer).
        if (cardReader_.kind() == u"counter" && !amount.isDouble())
            return call(u"startCounterCharge"_s, {a.str(u"tender")}, [done](const QVariant &ok) { done(ok.toBool()); });
        if (!cardReader_.kind().isEmpty() && !amount.isDouble())
            return takeCard(a.str(u"tender"), std::move(done));
        return call(u"tender"_s, {a.str(u"tender"), amount.isDouble() ? QVariant(amount.toInteger()) : QVariant()},
                    [done](const QVariant &ok) { done(ok.toBool()); });
    }

    if (type == u"command")
        return runCommand(a.str(u"name"), a.data.value(u"args").toObject().toVariantMap(), std::move(done));

    setStatus(tr("Action '%1' is not supported yet").arg(type));
    done(false);
}

void LayoutController::takeCard(const QString &tenderId, Done done)
{
    // The store prepares the charge (amount, tip, which check); not a card
    // payment type: paid as usual.
    call(u"cardCharge"_s, {tenderId}, [this, tenderId, done](const QVariant &v) {
        const QVariantMap charge = v.toMap();
        if (charge.value(u"notCard"_s).toBool())
            return call(u"tender"_s, {tenderId, QVariant()}, [done](const QVariant &ok) { done(ok.toBool()); });
        if (!charge.value(u"ok"_s).toBool())
            return done(false);
        cardReader_.charge(charge);
        done(true);
    });
}

void LayoutController::cardTaken(const QVariantMap &result)
{
    if (!result.value(u"approved"_s).toBool()) {
        setStatus(result.value(u"message"_s).toString().isEmpty() ? tr("The card didn't go through.")
                                                                  : result.value(u"message"_s).toString());
        return;
    }
    // The money is taken: it must reach the check. Kept until it does (sent
    // again, it counts once).
    unsentCards_.append(result);
    saveUnsentCards();
    sendUnsentCards();
}

// Cards approved but not on a check yet, kept on this device (they survive
// the app closing): the money was taken.
void LayoutController::saveUnsentCards()
{
    QJsonArray a;
    for (const QVariantMap &c : unsentCards_)
        a.append(QJsonObject::fromVariantMap(c));
    QSettings settings;
    if (a.isEmpty())
        settings.remove(u"cards/unsent"_s);
    else
        settings.setValue(u"cards/unsent"_s, QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)));
}

void LayoutController::loadUnsentCards()
{
    const QString kept = QSettings().value(u"cards/unsent"_s).toString();
    for (const QJsonValue &v : QJsonDocument::fromJson(kept.toUtf8()).array())
        if (!unsentCards_.contains(v.toObject().toVariantMap()))
            unsentCards_.append(v.toObject().toVariantMap());
}

void LayoutController::sendUnsentCards()
{
    if (!pos_ || unsentCards_.isEmpty())
        return;
    if (!pos_->online()) {
        setStatus(tr("A card was approved; it goes on the check when the store is back."));
        return;
    }
    const QVariantMap result = unsentCards_.first();
    // A tab's card, held: on the tab, not paid.
    call(result.value(u"hold"_s).toBool() ? u"recordCardHold"_s : u"recordCardPayment"_s, {result},
         [this, result](const QVariant &ok) {
        if (!pos_ || !pos_->online())
            return;   // still offline: tried again when it's back
        unsentCards_.removeOne(result);
        saveUnsentCards();
        if (!ok.toBool())
            setStatus(tr("The card was approved (%1) but isn't on the check: add it by hand or refund it in Stripe.")
                          .arg(result.value(u"reference"_s).toString()));
        sendUnsentCards();
    });
}

void LayoutController::runCommand(const QString &name, const QVariantMap &args, Done done)
{
    if (name == u"editMode")
        return done(requestEditMode());

    // Close ViewTouch: a manager's way off a kiosk screen (it then stays
    // closed until the next boot; see vtmodern-kiosk.service).
    if (name == u"closeApp") {
        if (pos_ && !pos_->can(QString::fromLatin1(vt::core::perm::Manager))) {
            setStatus(tr("Closing ViewTouch needs a manager."));
            return done(false);
        }
        done(true);
        emit closeRequested();
        return;
    }

    // Manager screens are pages ("admin-menu", "reports"...). Kept as a
    // command so buttons made before those pages existed still work.
    if (name == u"openAdmin") {
        static const QHash<QString, QString> pages = {
            {u"menu"_s, u"admin-menu"_s}, {u"employees"_s, u"admin-employees"_s},
            {u"tenders"_s, u"admin-tenders"_s}, {u"printers"_s, u"admin-printers"_s},
            {u"taxes"_s, u"admin-taxes"_s}, {u"settings"_s, u"admin-store"_s},
            {u"reports"_s, u"reports"_s}, {u"drawers"_s, u"drawer"_s}, {u"endOfDay"_s, u"end-of-day"_s},
            {u"terminals"_s, u"admin-terminals"_s}, {u"mealPeriods"_s, u"admin-meal-periods"_s},
            {u"modifierGroups"_s, u"admin-modifier-groups"_s}, {u"inventory"_s, u"admin-inventory"_s},
            {u"schedule"_s, u"admin-schedule"_s}, {u"promotions"_s, u"admin-promotions"_s},
            {u"vendors"_s, u"admin-vendors"_s}, {u"punches"_s, u"admin-punches"_s},
            {u"cardReaders"_s, u"admin-card-readers"_s},
            {u"requests"_s, u"admin-requests"_s},
        };
        const QString page = pages.value(args.value(u"panel"_s).toString());
        if (!page.isEmpty() && activeLayout().page(page))
            return done(navigate(Navigator::Mode::Push, page));
        setStatus(tr("This screen is not in your pages yet (start with --reset-layout to get it)."));
        return done(false);
    }

    // Commands that are POS operations: name -> (operation, arguments).
    static const QHash<QString, std::pair<QString, QVariantList>> operations = {
        {u"login"_s, {u"login"_s, {}}}, {u"logout"_s, {u"logout"_s, {}}},
        {u"switchCounterPerson"_s, {u"switchCounterPerson"_s, {}}},
        {u"clockIn"_s, {u"clockIn"_s, {}}}, {u"clockOut"_s, {u"clockOut"_s, {}}},
        {u"startCheck"_s, {u"startCheck"_s, {u"dineIn"_s}}}, {u"startQuick"_s, {u"startCheck"_s, {u"quick"_s}}},
        {u"startTakeout"_s, {u"startCheck"_s, {u"takeout"_s}}},
        {u"openTab"_s, {u"openTab"_s, {}}},
        {u"startDelivery"_s, {u"startCheck"_s, {u"delivery"_s}}},
        {u"releaseCheck"_s, {u"releaseCheck"_s, {}}}, {u"newTableCheck"_s, {u"newTableCheck"_s, {}}},
        {u"setCashTip"_s, {u"setCashTip"_s, {}}},
        {u"splitBySeat"_s, {u"splitBySeat"_s, {}}}, {u"printTableChecks"_s, {u"printTableChecks"_s, {}}},
        {u"combineTableChecks"_s, {u"combineTableChecks"_s, {}}}, {u"sendOrder"_s, {u"sendOrder"_s, {}}},
        {u"voidItem"_s, {u"voidItem"_s, {}}}, {u"lineMore"_s, {u"lineMore"_s, {0}}},
        {u"lineLess"_s, {u"lineLess"_s, {0}}}, {u"repeatLine"_s, {u"repeatLine"_s, {0}}},
        {u"undoLast"_s, {u"undoLast"_s, {}}}, {u"anotherRound"_s, {u"anotherRound"_s, {}}}, {u"timeClockDone"_s, {u"timeClockDone"_s, {}}},
        {u"amountOff"_s, {u"customDiscount"_s, {false}}}, {u"percentOff"_s, {u"customDiscount"_s, {true}}}, {u"addComment"_s, {u"addComment"_s, {}}},
        {u"removePayment"_s, {u"removePayment"_s, {}}}, {u"closeCheck"_s, {u"closeCheck"_s, {}}},
        {u"printReceipt"_s, {u"printReceipt"_s, {}}}, {u"noSale"_s, {u"noSale"_s, {}}},
        {u"openDrawer"_s, {u"noSale"_s, {}}}, {u"openDrawerSession"_s, {u"openDrawerSession"_s, {}}},
        {u"countDrawer"_s, {u"countDrawer"_s, {}}}, {u"endOfDay"_s, {u"endOfDay"_s, {}}},
        {u"recallTicket"_s, {u"recallTicket"_s, {}}}, {u"cashOutTips"_s, {u"cashOutTips"_s, {}}},
        {u"clearText"_s, {u"textKey"_s, {u"clear"_s}}},
        {u"backupNow"_s, {u"backupNow"_s, {}}},
        {u"askForTip"_s, {u"askForTip"_s, {}}},
        {u"toggleTraining"_s, {u"toggleTraining"_s, {}}},
        {u"selfOrder"_s, {u"setSelfOrder"_s, {true}}},
        {u"rush"_s, {u"toggleFlag"_s, {u"rush"_s}}}, {u"vip"_s, {u"toggleFlag"_s, {u"vip"_s}}},
        {u"startBreak"_s, {u"toggleBreak"_s, {}}}, {u"toggleBreak"_s, {u"toggleBreak"_s, {}}},
        // What widgets' own buttons do, for buttons placed anywhere.
        {u"expoRecall"_s, {u"expoRecall"_s, {}}}, {u"fireCourse"_s, {u"fireCourse"_s, {}}},
        {u"guestsMore"_s, {u"adjustGuests"_s, {1}}}, {u"guestsFewer"_s, {u"adjustGuests"_s, {-1}}},
    };
    // Widgets' own buttons that change the screen, not the store: the widget does them.
    if (name == u"kitchenStation" || name == u"kitchenAllDay" || name == u"allergies" || name == u"coupon"
        || name == u"cardOnFile" || name == u"taxExempt") {
        emit widgetCommand(name, args);
        return done(true);
    }
    // Hold the guest's card for the tab: on this screen's reader (one
    // beside the screen is run by the store's computer).
    if (pos_ && name == u"holdCard") {
        // An extra: the buttons after it go on either way (the tab is open).
        if (cardReader_.kind() == u"counter")
            return call(u"startCounterHold"_s, {}, [done](const QVariant &) { done(true); });
        if (cardReader_.kind().isEmpty()) {
            setStatus(tr("This screen has no card reader (Manager -> Terminals)."));
            return done(true);
        }
        return call(u"holdCharge"_s, {}, [this, done](const QVariant &v) {
            const QVariantMap charge = v.toMap();
            if (charge.value(u"ok"_s).toBool())
                cardReader_.charge(charge);
            done(true);
        });
    }
    if (name == u"setupGuide") {
        openSetup();
        return done(true);
    }
    if (name == u"finishChoosing") {
        finishChoosing();
        return done(true);
    }
    if (pos_ && (name == u"addWeighed" || name == u"cancelWeighing"))
        return call(name, {}, [this, done, name](const QVariant &ok) {
            if (ok.toBool() || name == u"cancelWeighing")
                goBack();
            done(ok.toBool());
        });
    if (pos_ && name == u"cancelChoosing")
        return call(u"cancelChoosing"_s, {}, [this, done](const QVariant &ok) {
            goBack();
            done(ok.toBool());
        });
    if (pos_ && (name == u"seatNext" || name == u"seatPrev")) {
        const int seat = pos_->checkInfo().value(u"seat"_s).toInt();
        return call(u"setSeat"_s, {std::max(0, seat + (name == u"seatNext" ? 1 : -1))},
                    [done](const QVariant &ok) { done(ok.toBool()); });
    }
    if (pos_ && name == u"courseNext") {   // 1, 2, 3 and around
        const int course = std::max(1, pos_->checkInfo().value(u"course"_s).toInt());
        return call(u"setCourse"_s, {course % 3 + 1}, [done](const QVariant &ok) { done(ok.toBool()); });
    }
    // Commands that carry arguments.
    if (pos_ && name == u"addTip")   // args.percent: 15, 18...; none = keypad amount
        return call(u"addTip"_s, {qint64(args.value(u"percent"_s).toDouble() * 100 + 0.5)},
                    [done](const QVariant &ok) { done(ok.toBool()); });
    if (pos_ && name == u"gratuity")
        return call(u"setGratuity"_s, {qint64(args.value(u"percent"_s).toDouble() * 100 + 0.5)},
                    [done](const QVariant &ok) { done(ok.toBool()); });
    // Send: a required choice missing opens it (not only says so); sent from
    // a table, back to the floor plan if the store wants (Store Settings).
    if (pos_ && name == u"sendOrder")
        return call(u"sendOrder"_s, {}, [this, done](const QVariant &ok) {
            if (!ok.toBool()) {
                for (const QVariant &v : pos_->lines())
                    if (v.toMap().value(u"needsChoice"_s).toBool()) {
                        const qint64 line = v.toMap().value(u"id"_s).toLongLong();
                        QMetaObject::invokeMethod(this, [this, line] { chooseLine(line); }, Qt::QueuedConnection);
                        break;
                    }
                return done(false);
            }
            const QVariantMap check = pos_->checkInfo();
            const bool toTables = pos_->userPrefs().value(u"sendToTables"_s).toBool()
                                  && check.value(u"type"_s).toString() == u"dineIn"
                                  && !check.value(u"label"_s).toString().isEmpty();
            done(true);
            if (toTables)
                call(u"releaseCheck"_s, {}, [this](const QVariant &released) {
                    if (!released.toBool())
                        return;
                    Action back;
                    back.data = QJsonObject{{u"type"_s, u"jump"_s}, {u"mode"_s, u"replace"_s}, {u"role"_s, u"tables"_s}};
                    runAction(back, [](bool) {});
                });
        });
    if (pos_ && (name == u"payout" || name == u"paidIn"))
        return call(u"payout"_s, {name}, [done](const QVariant &ok) { done(ok.toBool()); });

    if (pos_) {
        if (const auto it = operations.constFind(name); it != operations.cend())
            return call(it->first, it->second, [done](const QVariant &ok) { done(ok.toBool()); });
    }

    // Not a command this version knows.
    setStatus(tr("'%1' is not available yet").arg(name));
    done(true);
}

bool LayoutController::navigate(Navigator::Mode mode, const QString &target)
{
    // To the menu from outside the order screens (tables, a new order, open
    // checks): this screen's start category, if it has one (the bar: Drinks).
    // From the order screens (‹ Menu), the meal's page as usual.
    if (mode == Navigator::Mode::Index && pos_ && !editing()) {
        const vt::layout::Page *here = currentPage();
        const bool ordering = here && (here->kind == u"index" || here->kind == u"items" || here->kind == u"modifier"
                                       || here->kind == u"settle");
        const QString start = pos_->terminalStartCategory();
        QString menu = rolePage(u"menu"_s);
        if (menu.isEmpty() && activeLayout().page(u"menu-all"_s))
            menu = u"menu-all"_s;
        if (!ordering && !start.isEmpty() && !menu.isEmpty()) {
            setMenuCategory(start);
            return navigate(Navigator::Mode::Push, menu);
        }
    }
    if ((mode == Navigator::Mode::Push || mode == Navigator::Mode::Replace) && !mayOpen(target))
        return false;
    if (nav_.jump(mode, target)) {
        if (editing_ && editor_)
            editor_->setPageId(nav_.current());
        refresh();
        emit pageChanged();
        return true;
    }
    return false;
}

void LayoutController::refresh()
{
    const Layout &l = activeLayout();
    const QString pageId = nav_.current();
    const auto *page = currentPage();
    const bool onItemPage = page && (page->kind == u"items" || page->kind == u"modifier");

    const Shown s = shown();
    // Left-handed: on the order and Pay screens the check's column moves to the
    // right and what was right of it moves left, each row keeping its order.
    // What's above or below the check (the bottom buttons) stays.
    int colLeft = -1, colRight = -1, nextLeft = -1, colTop = 0, colBottom = 0;
    if (!editing() && pos_ && page && pos_->userPrefs().value(u"leftHanded"_s).toBool()
        && (page->kind == u"index" || page->kind == u"items" || page->kind == u"modifier" || page->kind == u"settle")) {
        for (const Layout::PlacedZone &pz : s.zones)
            if ((pz.zone->kind == u"orderList" || pz.zone->kind == u"paymentPanel") && !s.moved.contains(pz.zone)) {
                colLeft = pz.zone->rect.x();
                colRight = pz.zone->rect.x() + pz.zone->rect.width();
                colTop = pz.zone->rect.y();
                colBottom = pz.zone->rect.y() + pz.zone->rect.height();
                break;
            }
        for (const Layout::PlacedZone &pz : s.zones)
            if (colRight >= 0 && pz.zone->rect.x() >= colRight && pz.zone->rect.y() >= colTop
                && pz.zone->rect.y() + pz.zone->rect.height() <= colBottom && (nextLeft < 0 || pz.zone->rect.x() < nextLeft))
                nextLeft = pz.zone->rect.x();
    }
    const int canvasW = page ? page->canvas.width() : 1920;
    QList<ZoneModel::Row> rows;
    const QString style = vt::layout::Layout::styleOf(l.theme);
    for (const Layout::PlacedZone &pz : s.zones) {
        const vt::layout::Zone &z = *pz.zone;
        // Another style's button (showWhen.style): not on this screen at all.
        if (const QString only = z.extra.value(u"showWhen").toObject().value(u"style").toString();
            !only.isEmpty() && only != style)
            continue;
        // Laid out again for a phone: its own rect there, a plain shape, and
        // the look it has on its own page.
        const bool moved = s.moved.contains(pz.zone);
        QRect rect = s.fitted.value(pz.zone, moved ? s.moved.value(pz.zone) : z.rect);
        if (nextLeft >= 0 && !moved && rect.y() >= colTop && rect.y() + rect.height() <= colBottom) {
            if (rect.x() >= colLeft && rect.x() + rect.width() <= colRight)
                rect.translate(canvasW - colLeft - colRight, 0);          // the check's column: right
            else if (rect.x() >= colRight)
                rect.translate(colLeft - nextLeft, 0);                    // the rest: left
        }
        const QString shape = moved && z.shape != u"rect" ? u"rounded"_s : z.shape;
        const QString stylePage = moved ? pageId : s.pageId;
        // A button that orders an 86'd item: marked, and touching it does nothing.
        bool soldOut = false;
        const QString orders = !z.actions.isEmpty() && z.actions.first().type() == u"addItem"
                                   ? z.actions.first().str(u"item") : QString();
        if (pos_ && !editing() && !z.actions.isEmpty() && z.actions.first().type() == u"addItem") {
            const QString item = z.actions.first().str(u"item");
            const QStringList out = pos_->soldOut();
            soldOut = out.contains(item) || out.contains(item.toLower());
        }
        const QString target = primaryJumpTarget(l, z);
        const QString qualifier = pos_ ? qualifierOf(z) : QString();
        const bool current = !editing()
            && ((!target.isEmpty() && (target == pageId || (onItemPage && target == nav_.lastIndex())))
                || (!qualifier.isEmpty() && qualifier == pos_->pendingQualifier()));

        rows.append({
            {ZoneModel::ZoneIdRole, z.id},
            {ZoneModel::KindRole, z.kind},
            {ZoneModel::ZoneNameRole, z.name},
            {ZoneModel::LabelRole, z.label},
            {ZoneModel::ZoneXRole, rect.x()},
            {ZoneModel::ZoneYRole, rect.y()},
            {ZoneModel::ZoneWRole, rect.width()},
            {ZoneModel::ZoneHRole, rect.height()},
            {ZoneModel::ShapeRole, shape},
            {ZoneModel::BehaviorRole, z.behavior},
            {ZoneModel::ZoneEnabledRole, z.enabled && !soldOut},
            {ZoneModel::InheritedRole, pz.inherited},
            {ZoneModel::CurrentRole, current},
            {ZoneModel::HotkeyRole, z.hotkey},
            {ZoneModel::GroupRole, z.group},
            {ZoneModel::ImagePathRole, z.imagePath},
            {ZoneModel::StyleNormalRole, l.resolveStyle(z, stylePage, ZoneState::Normal).toVariantMap()},
            {ZoneModel::StyleSelectedRole, l.resolveStyle(z, stylePage, ZoneState::Selected).toVariantMap()},
            {ZoneModel::StyleDisabledRole, l.resolveStyle(z, stylePage, ZoneState::Disabled).toVariantMap()},
            {ZoneModel::PropsRole, moved ? [&] { QVariantMap p = z.props.toVariantMap(); p.remove(u"chairs"_s); return p; }()
                                         : z.props.toVariantMap()},
            {ZoneModel::SoldOutRole, soldOut},
            {ZoneModel::ShowWhenRole, z.extra.value(u"showWhen").toObject().toVariantMap()},
            {ZoneModel::ItemIdRole, orders},
            {ZoneModel::RotationRole, moved ? 0 : vt::layout::rotationOf(z)},   // in rows on a phone: straight
        });
    }
    zones_.setRows(std::move(rows));
    if (const QVariantMap colors = l.theme.extra.value(u"status").toObject().toVariantMap(); colors != statusColors_) {
        statusColors_ = colors;
        emit statusColorsChanged();
    }
}

QString LayoutController::describeAction(const Action &a) const
{
    const QString type = a.type();
    if (type == u"addItem") {
        const QString id = a.str(u"item");
        if (pos_)
            for (const QVariant &v : pos_->menuItems()) {
                const QVariantMap m = v.toMap();
                if (m.value(u"id"_s).toString() == id || m.value(u"name"_s).toString().compare(id, Qt::CaseInsensitive) == 0)
                    return tr("Adds %1 (%2) to the check.").arg(m.value(u"name"_s).toString(), m.value(u"price"_s).toString());
            }
        return tr("Adds %1 to the check.").arg(id);
    }
    if (type == u"qualifier") {
        const QString q = a.str(u"qualifier");
        if (q == u"no") return tr("The next choice you touch is left off (NO onions).");
        if (q == u"extra") return tr("The next choice you touch is extra (EXTRA cheese), and may cost more.");
        if (q == u"lite") return tr("The next choice you touch is light (LITE ice).");
        if (q == u"side") return tr("The next choice you touch comes on the side.");
        if (q == u"sub") return tr("The next item you touch replaces part of the item before it (a substitute).");
        return tr("Changes the next choice you touch (%1).").arg(q);
    }
    if (type == u"tender")
        return tr("Pays the check with %1 (the amount typed, or all that's due).").arg(a.str(u"tender"));
    if (type == u"jump") {
        const QString mode = a.data.value(u"mode").toString(u"push"_s);
        if (mode == u"back") return tr("Goes back to the screen before.");
        if (mode == u"home") return tr("Goes to the start screen.");
        if (mode == u"index") return tr("Goes to the menu.");
        const QString target = activeLayout().resolveTarget(a.data);
        const vt::layout::Page *p = activeLayout().page(target);
        return p ? tr("Opens the %1 screen.").arg(p->name) : tr("Opens another screen.");
    }
    if (type == u"command") {
        static const QHash<QString, const char *> plain{
            {u"sendOrder"_s, QT_TR_NOOP("Sends the new items to the kitchen and bar.")},
            {u"voidItem"_s, QT_TR_NOOP("Takes the touched item off the check (after it was sent: a void, which may need a manager).")},
            {u"printReceipt"_s, QT_TR_NOOP("Prints the check for the guest.")},
            {u"closeCheck"_s, QT_TR_NOOP("Closes the paid check.")},
            {u"removePayment"_s, QT_TR_NOOP("Takes off the touched payment.")},
            {u"noSale"_s, QT_TR_NOOP("Opens the cash drawer without a sale (it's recorded).")},
            {u"logout"_s, QT_TR_NOOP("Logs you out. Your open check opens again when you log back in.")},
            {u"clockIn"_s, QT_TR_NOOP("Starts your shift.")},
            {u"clockOut"_s, QT_TR_NOOP("Ends your shift.")},
            {u"startQuick"_s, QT_TR_NOOP("Starts a quick check (no table).")},
            {u"startTakeout"_s, QT_TR_NOOP("Starts a takeout order.")},
            {u"startDelivery"_s, QT_TR_NOOP("Starts a delivery order.")},
            {u"releaseCheck"_s, QT_TR_NOOP("Puts the check away (it stays open).")},
            {u"newTableCheck"_s, QT_TR_NOOP("Opens another check at this table.")},
            {u"anotherRound"_s, QT_TR_NOOP("Orders the drinks sent last again.")},
            {u"undoLast"_s, QT_TR_NOOP("Puts back the item just taken off.")},
            {u"repeatLine"_s, QT_TR_NOOP("Adds one more of the touched item, made the same way.")},
            {u"addComment"_s, QT_TR_NOOP("Adds a note for the kitchen.")},
            {u"rush"_s, QT_TR_NOOP("Tells the kitchen to make this check first.")},
            {u"allergies"_s, QT_TR_NOOP("Marks what the guest is allergic to: the kitchen sees it, and items with it are marked.")},
            {u"vip"_s, QT_TR_NOOP("Marks this check VIP for the kitchen.")},
            {u"fireCourse"_s, QT_TR_NOOP("Tells the kitchen to start the next course.")},
            {u"toggleBreak"_s, QT_TR_NOOP("Starts or ends your break.")},
            {u"cashOutTips"_s, QT_TR_NOOP("Pays you the card tips you're owed, from the drawer.")},
            {u"askForTip"_s, QT_TR_NOOP("Shows tip choices to the guest on the customer screen.")},
            {u"openTab"_s, QT_TR_NOOP("Opens a bar tab under the name typed.")},
            {u"coupon"_s, QT_TR_NOOP("Gives the check a coupon's discount: its code typed or scanned.")},
            {u"holdCard"_s, QT_TR_NOOP("Holds the guest's card for the tab on the card reader (nothing is charged yet).")},
            {u"cardOnFile"_s, QT_TR_NOOP("The tab's held card: hold one, charge it with a tip, or let it go.")},
        };
        const QString name = a.str(u"name");
        if (const auto it = plain.constFind(name); it != plain.constEnd())
            return tr(it.value());
        // Else the editor's name for it.
        for (const QJsonValue &t : vt::layout::schema::actionTypes())
            for (const QJsonValue &f : t.toObject().value(u"fields"_s).toArray())
                for (const QJsonValue &o : f.toObject().value(u"options"_s).toArray())
                    if (o.toObject().value(u"value"_s).toString() == name)
                        return QCoreApplication::translate("Schema", o.toObject().value(u"text"_s).toString().toUtf8().constData()) + u'.';
        return name;
    }
    return {};
}

void LayoutController::explain(const QString &zoneId)
{
    if (editing())
        return;
    for (const Layout::PlacedZone &pz : shown().zones) {
        if (pz.zone->id != zoneId)
            continue;
        QStringList steps;
        for (const Action &a : pz.zone->actions)
            if (const QString d = describeAction(a); !d.isEmpty())
                steps << d;
        const QString title = pz.zone->label.isEmpty() ? pz.zone->name : pz.zone->label;
        explanation_ = {{u"title"_s, QString(title).replace(u'\n', u' ')},
                        {u"text"_s, steps.isEmpty() ? tr("This button doesn't do anything yet.")
                                                    : steps.join(u'\n')}};
        emit explanationChanged();
        return;
    }
}

void LayoutController::clearExplanation()
{
    if (explanation_.isEmpty())
        return;
    explanation_.clear();
    emit explanationChanged();
}

void LayoutController::setStatus(const QString &text)
{
    status_ = text;
    emit statusChanged();
}
