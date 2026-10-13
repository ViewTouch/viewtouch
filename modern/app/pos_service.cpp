#include "app/pos_service.hh"

#include "app/i18n.hh"

#include "app/pos_json.hh"

#include <QDateTime>
#include <QLocale>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr int kMaxPin = 8;
constexpr int kMaxEntryDigits = 9;
constexpr int kMaxGuests = 99;

QString timeOfDay(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

QDate dateOf(std::int64_t ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).date();
}

} // namespace

PosShared::PosShared(PosData data, PosSink *sink, QObject *parent)
    : QObject(parent)
    , settings(std::move(data.settings))
    , menu(std::move(data.menu))
    , employees(std::move(data.employees))
    , punches(std::move(data.punches))
    , earlierPunches(std::move(data.earlierPunches))
    , customers(std::move(data.customers))
    , giftCards(std::move(data.giftCards))
    , parties(std::move(data.parties))
    , lastPartyId(data.lastPartyId)
    , ingredients(std::move(data.ingredients))
    , shifts(std::move(data.shifts))
    , lastShiftId(data.lastShiftId)
    , deliveries(std::move(data.deliveries))
    , lastDeliveryId(data.lastDeliveryId)
    , images(std::move(data.images))
    , lastCheckId(data.lastCheckId)
    , lastPunchId(data.lastPunchId)
    , sink(sink)
    , lastDayId(data.lastDayId)
    , closedToday(std::move(data.closedToday))
    , drawers(std::move(data.drawers))
    , lastDrawerId(data.lastDrawerId)
    , pastDays(std::move(data.pastDays))
    , refundsToday(std::move(data.refundsToday))
    , lastRefundId(data.lastRefundId)
    , changesToday(std::move(data.changesToday))
    , lastChangeId(data.lastChangeId)
    , stockCounts(std::move(data.stockCounts))
    , lastStockCountId(data.lastStockCountId)
    , waste(std::move(data.waste))
    , lastWasteId(data.lastWasteId)
    , problems(std::move(data.problems))
    , lastProblemId(data.lastProblemId)
    , now_([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    for (Check &c : data.openChecks) {
        lastCheckId = std::max(lastCheckId, c.id);
        open.emplace(c.id, std::move(c));
    }
    for (const Check &c : closedToday)
        lastCheckId = std::max(lastCheckId, c.id);
    for (const DrawerSession &d : drawers)
        lastDrawerId = std::max(lastDrawerId, d.id);
    if (data.currentDay && data.currentDay->open()) {
        day = *data.currentDay;
        lastDayId = std::max(lastDayId, day.id);
    } else {
        startDay();
    }
}

const Employee *PosShared::employee(const std::string &id) const
{
    if (id.empty())
        return nullptr;
    if (id == kioskEmployee().id)
        return &kioskEmployee();
    for (const Employee &e : employees) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

PosService::PosService(PosData data, PosSink *sink, QObject *parent)
    : PosSession(parent)
    , owned_(std::make_unique<PosShared>(std::move(data), sink))
    , terminal_(tr("Terminal"))
{
    s_ = owned_.get();
    connectShared();
}

PosService::PosService(PosShared *shared, QString terminalName, QObject *parent)
    : PosSession(parent)
    , s_(shared)
    , terminal_(std::move(terminalName))
{
    connectShared();
}

PosService::~PosService()
{
    if (s_->receiptSender == this)
        s_->receiptSender = nullptr;
    // A card on a reader beside this terminal, still being taken: the store
    // keeps watching it and puts it on the check when Stripe answers.
    if (!counter_.value(u"paymentIntent"_s).toString().isEmpty() && !counterHeir_) {
        auto *heir = new PosService(s_, terminal_, s_);
        heir->adoptCounterCharge(counter_);
    }
    // A terminal going away (or disconnecting) lets go of its check.
    std::erase_if(s_->lockedBy, [this](const auto &kv) { return kv.second == this; });
    disconnect(s_, nullptr, this, nullptr);
}

void PosService::connectShared()
{
    // Price changes set for later: put on the menu when they're due.
    priceClock_.setInterval(20'000);
    connect(&priceClock_, &QTimer::timeout, this, &PosService::applyDuePriceChanges);
    priceClock_.start();
    QTimer::singleShot(0, this, &PosService::applyDuePriceChanges);
    // Receipts waiting for Stripe: one service sends them (the first; the next when it goes).
    connect(&priceClock_, &QTimer::timeout, this, &PosService::sendWaitingReceipts);
    if (!s_->receiptSender)
        s_->receiptSender = this;
    // A refund recorded (here or on another terminal): checks found here show it.
    connect(s_, &PosShared::refundRecorded, this, [this](const Refund &r, const QString &what) {
        bool shown = false;
        for (Check &c : searchHits_)
            if (c.id == r.checkId && std::ranges::none_of(c.refunds, [&](const Refund &x) { return x.id == r.id; })) {
                c.refunds.push_back(r);
                c.note(r.at, r.by, ss(what), "refund", r.amount);
                shown = true;
            }
        if (shown || r.by == (user() ? user()->name : std::string()))
            emit sessionChanged();
    });
    // Changes made by any terminal refresh every terminal's view.
    connect(s_, &PosShared::checksChanged, this, [this] {
        emit openChecksChanged();
        emit checkChanged();
        emit kitchenChanged();
        emit dayChanged();
    });
    connect(s_, &PosShared::dayChanged, this, &PosSession::dayChanged);
    connect(s_, &PosShared::networkChanged, this, &PosSession::dayChanged);
    connect(s_, &PosShared::drawerChanged, this, &PosSession::drawerChanged);
    connect(s_, &PosShared::adminChanged, this, [this] {
        emit adminChanged();
        emit checkChanged();         // tax changes re-total
        emit openChecksChanged();
    });
    connect(s_, &PosShared::customersChanged, this, &PosSession::checkChanged);
    connect(s_, &PosShared::staffChanged, this, [this] {
        if (!userId_.empty() && !user()) {   // deactivated or removed elsewhere
            userId_.clear();
            emit loggedInChanged(false);
        }
        emit sessionChanged();
    });
}

const Employee *PosService::user() const
{
    const Employee *e = s_->employee(userId_);
    return e && e->active ? e : nullptr;
}

bool PosService::lockCheck(std::int64_t checkId)
{
    auto it = s_->lockedBy.find(checkId);
    if (it != s_->lockedBy.end() && it->second != this)
        return false;
    s_->lockedBy[checkId] = this;
    return true;
}

void PosService::unlockCheck(std::int64_t checkId)
{
    auto it = s_->lockedBy.find(checkId);
    if (it != s_->lockedBy.end() && it->second == this)
        s_->lockedBy.erase(it);
}

QString PosService::lockHolder(std::int64_t checkId) const
{
    auto it = s_->lockedBy.find(checkId);
    if (it == s_->lockedBy.end() || it->second == this)
        return {};
    const auto *other = qobject_cast<const PosService *>(it->second);
    return other ? other->terminalName() : tr("another terminal");
}

// --- helpers ---------------------------------------------------------------------

const Check *PosService::currentCheck() const
{
    auto it = s_->open.find(currentId_);
    return it == s_->open.end() ? nullptr : &it->second;
}

Check *PosService::current()
{
    auto it = s_->open.find(currentId_);
    return it == s_->open.end() ? nullptr : &it->second;
}

const MenuItem *PosService::findItem(const QString &idOrName) const
{
    const std::string key = ss(idOrName);
    for (const MenuItem &m : s_->menu) {
        if (m.id == key)
            return &m;
    }
    for (const MenuItem &m : s_->menu) {
        if (QString::compare(qs(m.name), idOrName, Qt::CaseInsensitive) == 0)
            return &m;
    }
    // Its number (a PLU), typed; a barcode, scanned.
    if (!key.empty()) {
        for (const MenuItem &m : s_->menu)
            if (m.number == key)
                return &m;
        for (const MenuItem &m : s_->menu)
            if (std::ranges::find(m.barcodes, key) != m.barcodes.end())
                return &m;
    }
    return nullptr;
}

QStringList PosService::barcodeList(const QString &text)
{
    QStringList out;
    for (const QString &c : text.split(QRegularExpression(u"[\\s,;]+"_s), Qt::SkipEmptyParts))
        if (!out.contains(c))
            out << c;
    return out;
}

QString PosService::barcodeProblem(const QStringList &codes, const std::string &itemId) const
{
    static const QRegularExpression shape(u"^[0-9A-Za-z-]{4,40}$"_s);
    for (const QString &c : codes) {
        if (!shape.match(c).hasMatch())
            return tr("A barcode is 4 to 40 letters and digits: \"%1\".").arg(c);
        for (const MenuItem &m : s_->menu)
            if (m.id != itemId && std::ranges::find(m.barcodes, ss(c)) != m.barcodes.end())
                return tr("%1 already has the barcode %2.").arg(qs(m.name), c);
    }
    return {};
}

bool PosService::require(const char *permission, const QString &action, Check *noteOn)
{
    if (!user())
        return fail(tr("Log in first."));
    if (permissionsOf(*user()).contains(permission))
        return true;
    // A manager approved this one, a moment ago.
    if (approved_ && approved_->permission == permission) {
        const std::string by = approved_->by;
        approved_.reset();
        if (Check *c = noteOn ? noteOn : current())
            noteEvent(*c, tr("%1: approved by %2").arg(action, qs(by)), "approval");
        emit notice(tr("Approved by %1").arg(qs(by)));
        return true;
    }
    // Voids, discounts and manager actions can be approved on the spot.
    const std::string p = permission;
    if (running_ && !selfOrder_
        && (p == perm::Void || p == perm::Discount || p == perm::Manager || p == perm::Settle || p == perm::OthersChecks)) {
        approval_ = {{u"needed"_s, true}, {u"action"_s, action}, {u"permission"_s, QString::fromLatin1(permission)},
                     {u"who"_s, qs(user()->name)}};
        approvalMethod_ = running_->method;
        approvalArgs_ = running_->args;
        emit sessionChanged();
        return fail(tr("%1 needs a manager's approval.").arg(action));
    }
    return fail(tr("%1 is not allowed for %2.").arg(action, qs(user()->name)));
}

bool PosService::approve(const QString &pin)
{
    if (!approval_.value(u"needed"_s).toBool())
        return fail(tr("Nothing is waiting for approval."));
    const Employee *m = employeeByPin(pin);
    const std::string permission = ss(approval_.value(u"permission"_s).toString());
    if (!m || !m->active || !m->can(permission))
        return fail(tr("That PIN can't approve this."));
    approved_ = Approved{permission, m->name};
    const QString method = approvalMethod_;
    const QVariantList args = approvalArgs_;
    approval_.clear();
    emit sessionChanged();
    invoke(method, args);   // once more, approved
    approved_.reset();      // used or not, it was for that one operation
    return true;
}

bool PosService::cancelApproval()
{
    approval_.clear();
    approved_.reset();
    reason_.reset();
    emit sessionChanged();
    return true;
}

bool PosService::haveReason(const char *kind, const QString &what)
{
    // Only on a screen (the operation can run again once answered).
    if (!running_ || selfOrder_)
        return true;
    if (reason_ && reason_->method == running_->method)
        return true;
    const Employee *e = user();
    const bool ask = e && !e->reasons.empty() ? e->reasons == "ask" : s_->settings.askReasons;
    const auto &list = std::string_view(kind) == "void" ? s_->settings.voidReasons : s_->settings.compReasons;
    if (!ask || list.empty())
        return true;
    // The starting lists, in the screen's language (a store's own reasons as typed).
    static constexpr const char *kStarting[] = {
        QT_TR_NOOP("Wrong item rung in"), QT_TR_NOOP("Guest changed their mind"), QT_TR_NOOP("Took too long"),
        QT_TR_NOOP("Guest didn't like it"), QT_TR_NOOP("Kitchen mistake"), QT_TR_NOOP("Guest unhappy"),
        QT_TR_NOOP("Manager's guest"), QT_TR_NOOP("Regular / VIP"),
    };
    QStringList reasons;
    for (const std::string &r : list)
        reasons << (std::ranges::find(kStarting, r, [](const char *k) { return std::string(k); }) != std::end(kStarting)
                        ? tr(r.c_str()) : qs(r));
    reasonAsk_ = {{u"kind"_s, QString::fromLatin1(kind)}, {u"what"_s, what}, {u"reasons"_s, reasons}};
    reasonMethod_ = running_->method;
    reasonArgs_ = running_->args;
    emit sessionChanged();
    return false;
}

std::string PosService::takeReason()
{
    if (!reason_ || !running_ || reason_->method != running_->method)
        return {};
    const std::string r = reason_->reason;
    reason_.reset();
    return r;
}

bool PosService::giveReason(const QString &reason)
{
    if (reasonAsk_.isEmpty())
        return fail(tr("Nothing is waiting for a reason."));
    const QString method = reasonMethod_;
    const QVariantList args = reasonArgs_;
    reasonAsk_.clear();
    emit sessionChanged();
    if (reason.trimmed().isEmpty())
        return true;   // not after all
    reason_ = GivenReason{method, ss(reason.trimmed().left(80))};
    invoke(method, args);
    // Used, or kept only while a manager's approval is waiting for it.
    if (!approval_.value(u"needed"_s).toBool())
        reason_.reset();
    return true;
}

bool PosService::sendMessage(const QString &to, const QString &text, qint64 until)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return fail(tr("Type the message."));
    const QString target = to.trimmed().isEmpty() ? u"all"_s : to.trimmed();
    if (until > 0) {   // posted: kept, and shown until then
        if (until <= now())
            return fail(tr("Pick a time that hasn't passed."));
        auto &list = s_->settings.notices;
        std::erase_if(list, [&](const PosSettings::Notice &n) { return n.until <= now(); });
        std::int64_t id = 1;
        for (const PosSettings::Notice &n : list)
            id = std::max(id, n.id + 1);
        list.push_back({id, now(), until, ss(user() ? qs(user()->name) : terminal_), ss(target), ss(t.left(200))});
        s_->saveSettings();
        emit notice(tr("Posted until %1").arg(dueText(until)));
        emit s_->dayChanged();
        return true;
    }
    PosShared::Message m{++s_->lastMessageId, now(), ss(user() ? qs(user()->name) : terminal_), ss(target), ss(t.left(200))};
    s_->messages.push_back(m);
    if (s_->messages.size() > 50)
        s_->messages.erase(s_->messages.begin());
    emit notice(tr("Message sent"));
    emit s_->dayChanged();   // every screen looks
    return true;
}

QString PosService::language() const
{
    if (const core::Employee *e = user(); e && !e->language.empty())
        return qs(e->language);
    return qs(s_->settings.language);
}

QVariantMap PosService::userPrefs() const
{
    // This screen's text size, unless the person has their own.
    int screenText = 0;
    std::string deviceStart;
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_) {
            screenText = t.textSize;
            deviceStart = t.startPage;
        }
    const core::Employee *e = user();
    if (!e)
        return screenText ? QVariantMap{{u"textSize"_s, screenText}} : QVariantMap();
    // Their own start page, else their job's: the one they're clocked in as, or their role.
    std::string start = e->startPage;
    if (start.empty()) {
        std::string job = e->role;
        for (const TimePunch &p : s_->punches)
            if (p.employeeId == e->id && p.open() && !p.job.empty())
                job = p.job;
        if (const auto it = s_->settings.startPages.find(job); it != s_->settings.startPages.end())
            start = it->second;
        else if (job == "host")
            start = "seating";   // hosts: the seating screen (when the layout has it)
    }
    // This device's start screen wins: the patio handheld opens on the patio, whoever has it.
    if (!deviceStart.empty())
        start = deviceStart;
    return {{u"textSize"_s, e->textSize != 100 || !screenText ? e->textSize : screenText},
            {u"leftHanded"_s, e->leftHanded}, {u"startPage"_s, qs(start)}};
}

void PosShared::noteProblem(const std::string &kind, const std::string &subject, const std::string &detail)
{
    const std::int64_t at = now();
    for (auto it = problems.rbegin(); it != problems.rend() && it->at > at - 10 * 60'000; ++it)
        if (it->kind == kind && it->subject == subject && it->detail == detail)
            return;
    Problem p{++lastProblemId, at, kind, subject, detail};
    problems.push_back(p);
    std::erase_if(problems, [&](const Problem &x) { return x.at < at - 7LL * 24 * 3'600'000; });
    if (sink)
        sink->saveProblem(p);
    emit networkChanged();
}

void PosShared::setPrinterProblem(const std::string &printerId, const std::string &problem)
{
    const auto it = printerProblems.find(printerId);
    if (problem.empty() ? it == printerProblems.end() : it != printerProblems.end() && it->second == problem)
        return;
    if (problem.empty())
        printerProblems.erase(it);
    else
        printerProblems[printerId] = problem;
    // Kept for the managers (paper running low is only a heads-up).
    if (!problem.empty() && problem != "paperLow")
        if (const PrinterConfig *p = settings.printer(printerId); p && p->watch)
            noteProblem("printer", p->name, problem);
    emit networkChanged();   // every screen's printerAlerts, and Manager -> Network
}

QVariantList PosService::printerAlerts() const
{
    QVariantList out;
    for (const PrinterConfig &p : s_->settings.printers) {
        const auto it = s_->printerProblems.find(p.id);
        if (it == s_->printerProblems.end() || !p.watch)
            continue;
        const QString name = qs(p.name);
        const std::string &what = it->second;
        const QString text = printerProblemText(name, what);
        out.append(QVariantMap{{u"id"_s, qs(p.id)}, {u"name"_s, name}, {u"problem"_s, qs(what)}, {u"text"_s, text},
                               {u"urgent"_s, what != "paperLow"}});
    }
    return out;
}

QVariantMap PosService::networkInfo() const
{
    if (!can(u"manager"_s))
        return {};
    if (s_->network)
        return s_->network();
    return {{u"role"_s, u"single"_s}};
}

bool PosService::removeMessage(const QString &id)
{
    // A posted message: whoever posted it, or a manager, takes it down.
    auto &list = s_->settings.notices;
    const auto it = std::ranges::find_if(list, [&](const PosSettings::Notice &n) { return u"n%1"_s.arg(n.id) == id; });
    if (it == list.end())
        return fail(tr("That message is gone."));
    if (!(user() && (user()->name == it->from || can(QString::fromLatin1(perm::Manager)))))
        return fail(tr("Only whoever posted it, or a manager, can take it down."));
    list.erase(it);
    s_->saveSettings();
    emit notice(tr("Message taken down"));
    emit s_->dayChanged();
    return true;
}

QVariantList PosService::messages() const
{
    // Posted messages until they expire, then the last hour's, newest first.
    QVariantList out;
    for (auto it = s_->settings.notices.rbegin(); it != s_->settings.notices.rend(); ++it) {
        if (it->until <= now())
            continue;
        out.append(QVariantMap{{u"id"_s, u"n%1"_s.arg(it->id)}, {u"from"_s, qs(it->from)}, {u"to"_s, qs(it->to)},
                               {u"text"_s, qs(it->text)}, {u"time"_s, timeOfDay(it->at)}, {u"posted"_s, true},
                               {u"until"_s, dueText(it->until)}});
    }
    for (auto it = s_->messages.rbegin(); it != s_->messages.rend() && out.size() < 20; ++it) {
        if (now() - it->at > 60 * 60'000)
            break;
        out.append(QVariantMap{{u"id"_s, qint64(it->id)}, {u"from"_s, qs(it->from)}, {u"to"_s, qs(it->to)},
                               {u"text"_s, qs(it->text)}, {u"time"_s, timeOfDay(it->at)}});
    }
    return out;
}

bool PosService::training() const
{
    return user() && (user()->training || trainingOn_);
}

bool PosService::setTraining(bool on)
{
    if (!require(perm::Manager, tr("Practice mode")))
        return false;
    trainingOn_ = on;
    emit notice(on ? tr("Practice mode: nothing on this screen is a real sale") : tr("Practice mode off"));
    emit sessionChanged();
    return true;
}

bool PosService::fail(const QString &message)
{
    emit notice(message);
    return false;
}

QString PosService::format(Money amount) const
{
    const QString s = qs(amount.toString());
    return s.startsWith(u'-') ? u"-"_s + qs(s_->settings.currencySymbol) + s.mid(1)
                              : qs(s_->settings.currencySymbol) + s;
}

void PosService::changed(Check &check)
{
    applyDeliveryFee(check);
    applyPromotions(check);
    if (s_->sink)
        s_->sink->saveCheck(check);
    emit checkChanged();
    emit s_->checksChanged();
}

// --- session ---------------------------------------------------------------------

void PosService::pinKey(const QString &key)
{
    if (key == u"clear")
        pin_.clear();
    else if (key == u"back")
        pin_.chop(1);
    else if (key.size() == 1 && key[0].isDigit() && pin_.size() < kMaxPin)
        pin_ += key;
    else
        return;
    emit entryChanged();
}

const Employee *PosService::employeeByPin(const QString &pin) const
{
    if (pin.isEmpty())
        return nullptr;
    for (const Employee &e : s_->employees) {
        if (e.active && e.pinHash == hashPin(pin, e.pinSalt))
            return &e;
    }
    return nullptr;
}

bool PosService::login()
{
    const QString pin = pin_;
    pin_.clear();
    emit entryChanged();
    return loginWithPin(pin);
}

bool PosService::loginWithPin(const QString &pin)
{
    const Employee *e = employeeByPin(pin);
    if (!e)
        return fail(tr("That PIN is not recognized."));
    userId_ = e->id;
    // Back from someone else's turn: the check they were on, if nobody has it.
    if (const auto it = s_->resumeChecks.find(e->id); it != s_->resumeChecks.end()) {
        const std::int64_t id = it->second;
        s_->resumeChecks.erase(it);
        if (s_->open.contains(id) && !s_->lockedBy.contains(id) && openCheck(id))
            emit notice(tr("Welcome back, %1: %2 is open again").arg(qs(e->name), qs(s_->open.at(id).label)));
    }
    emit sessionChanged();
    emit loggedInChanged(true);
    if (!hasCheck())
        emit notice(tr("Welcome, %1").arg(qs(e->name)));
    return true;
}

void PosService::logout()
{
    if (selfOrder_) {   // idle: the guest walked away
        kioskCancel();
        return;
    }
    if (!user())
        return;
    // Their check, to pick up again when they're back (Switch User).
    const std::int64_t mine = currentId_;
    releaseCheck();
    if (mine != 0 && s_->open.contains(mine))
        s_->resumeChecks[userId_] = mine;
    else
        s_->resumeChecks.erase(userId_);
    userId_.clear();
    pin_.clear();
    jobChoice_.clear();
    trainingOn_ = false;
    approval_.clear();
    approved_.reset();
    clearEntry();
    qualifier_ = Qualifier::None;
    emit qualifierChanged();
    emit sessionChanged();
    emit loggedInChanged(false);
}

TimePunch *PosService::openPunch(const std::string &employeeId)
{
    for (TimePunch &p : s_->punches) {
        if (p.employeeId == employeeId && p.open())
            return &p;
    }
    return nullptr;
}

bool PosService::clockIn()
{
    const Employee *e = user() ? user() : employeeByPin(pin_);
    pin_.clear();
    emit entryChanged();
    if (!e)
        return fail(tr("Enter your PIN, then Clock In."));
    if (openPunch(e->id))
        return fail(tr("%1 is already clocked in.").arg(qs(e->name)));
    if (const QString why = scheduleCheck(*e); !why.isEmpty())
        return fail(why);
    const std::vector<Job> jobs = e->jobs();
    if (jobs.size() > 1) {   // which job today?
        jobChoice_ = e->id;
        emit sessionChanged();
        return true;
    }
    return punchIn(*e, jobs.front());
}

bool PosService::clockInAs(const QString &role)
{
    const Employee *e = s_->employee(jobChoice_);
    jobChoice_.clear();
    emit sessionChanged();
    if (!e)
        return fail(tr("Enter your PIN, then Clock In."));
    for (const Job &job : e->jobs()) {
        if (qs(job.role) == role)
            return openPunch(e->id) ? fail(tr("%1 is already clocked in.").arg(qs(e->name))) : punchIn(*e, job);
    }
    return fail(tr("Choose one of your jobs."));
}

void PosService::cancelClockIn()
{
    jobChoice_.clear();
    emit sessionChanged();
}

QVariantMap PosService::cashTipsAsk() const
{
    for (const TimePunch &p : s_->punches)
        if (p.id == cashTipsPunch_)
            if (const Employee *e = s_->employee(p.employeeId))
                return {{u"who"_s, qs(e->name)}, {u"punchId"_s, qint64(p.id)}};
    return {};
}

bool PosService::declareCashTips(const QString &amount)
{
    const auto p = std::ranges::find_if(s_->punches, [&](const TimePunch &x) { return x.id == cashTipsPunch_; });
    if (cashTipsPunch_ == 0 || p == s_->punches.end())
        return fail(tr("Nobody just clocked out here."));
    const qint64 cents = amount.trimmed().isEmpty() ? 0 : priceCents(amount, qs(s_->settings.currencySymbol));
    if (cents < 0 || cents > 1'000'000)
        return fail(tr("Type the cash tips, like 42.00 (or 0)."));
    p->cashTipsDeclared = true;
    p->cashTips = Money::fromCents(cents);
    if (s_->sink)
        s_->sink->savePunch(*p);
    cashTipsPunch_ = 0;
    emit sessionChanged();
    emit s_->dayChanged();
    emit notice(cents ? tr("Cash tips: %1").arg(format(p->cashTips)) : tr("No cash tips"));
    return true;
}

QVariantMap PosService::clockInJobs() const
{
    const Employee *e = s_->employee(jobChoice_);
    if (!e)
        return {};
    QVariantList jobs;
    for (const Job &job : e->jobs())
        jobs.append(QVariantMap{{u"role"_s, qs(job.role)}, {u"name"_s, roleName(qs(job.role))}});
    return {{u"who"_s, qs(e->name)}, {u"jobs"_s, jobs}};
}

bool PosService::punchIn(const Employee &e, const Job &job, const QString &by)
{
    TimePunch p{++s_->lastPunchId, e.id, now(), 0, {}, job.role, job.rate};
    s_->punches.push_back(p);
    if (s_->sink)
        s_->sink->savePunch(p);
    emit sessionChanged();
    emit s_->dayChanged();
    emit s_->staffChanged();
    const QString as = e.jobs().size() > 1 ? u" (%1)"_s.arg(roleName(qs(job.role))) : QString();
    emit notice(by.isEmpty() ? tr("%1 clocked in at %2").arg(qs(e.name) + as, timeOfDay(p.clockIn))
                             : tr("%1 clocked in by %2").arg(qs(e.name) + as, by));
    // Close to overtime: say so now, not on the paycheck.
    const QVariantMap ot = overtimeFor(e.id);
    if (ot.value(u"state"_s).toString() == u"over")
        emit notice(tr("%1 is in overtime this week (%2 h).").arg(qs(e.name), ot.value(u"weekHours"_s).toString()));
    else if (ot.value(u"state"_s).toString() == u"soon")
        emit notice(tr("%1 reaches overtime in %2 h.").arg(qs(e.name), ot.value(u"left"_s).toString()));
    return true;
}

bool PosService::clockOut()
{
    const Employee *e = user() ? user() : employeeByPin(pin_);
    pin_.clear();
    emit entryChanged();
    if (!e)
        return fail(tr("Enter your PIN, then Clock Out."));
    return clockOutFor(*e);
}

bool PosService::clockOutFor(const Employee &employee)
{
    const Employee *e = &employee;
    TimePunch *p = openPunch(e->id);
    if (!p)
        return fail(tr("%1 is not clocked in.").arg(qs(e->name)));
    p->clockOut = now();
    if (p->onBreak())   // clocking out ends a break
        p->breaks.back().end = p->clockOut;
    if (s_->sink)
        s_->sink->savePunch(*p);
    // Tipped jobs: the cash tips they kept (asked on this screen next).
    if (s_->settings.declareCashTips && p->job != "manager" && p->job != "admin")
        cashTipsPunch_ = p->id;
    const double hours = double(p->workedMs(p->clockOut, s_->settings.paidBreaks)) / 3'600'000.0;
    emit sessionChanged();
    emit s_->dayChanged();
    emit notice(tr("%1 clocked out (%2 hours)").arg(qs(e->name), i18n::locale().toString(hours, 'f', 2)));
    return true;
}

// --- keypads ----------------------------------------------------------------------

void PosService::entryKey(const QString &key)
{
    if (key == u"clear")
        entry_.clear();
    else if (key == u"back")
        entry_.chop(1);
    else if (!key.isEmpty() && std::ranges::all_of(key, [](QChar c) { return c.isDigit(); })) {
        if (entry_.size() + key.size() > kMaxEntryDigits)
            return;
        entry_ += key;
        while (entry_.size() > 1 && entry_.startsWith(u'0'))
            entry_.remove(0, 1);
    } else {
        return;
    }
    if (!weighing_.isEmpty())
        emit checkChanged();   // the Weigh page shows what the weight comes to
    emit entryChanged();
}

void PosService::adjustGuests(int delta)
{
    entry_ = QString::number(std::clamp(entryGuests() + delta, 1, kMaxGuests));
    emit entryChanged();
}

void PosService::textKey(const QString &key)
{
    if (key == u"clear")
        text_.clear();
    else if (key == u"back")
        text_.chop(1);
    else if (key == u"space")
        text_ += u' ';
    else if (key.size() == 1)
        text_ += key;
    else
        return;
    emit entryChanged();
}

void PosService::clearEntry()
{
    if (entry_.isEmpty() && text_.isEmpty())
        return;
    entry_.clear();
    text_.clear();
    emit entryChanged();
}

// --- checks ------------------------------------------------------------------------

PosSession::TableResult PosService::selectTable(const QString &label)
{
    if (!require(perm::Order, tr("Opening tables")))
        return TableFailed;
    std::vector<std::int64_t> atTable;
    for (const auto &[id, c] : s_->open) {
        if (c.type == CheckType::DineIn && qs(c.label) == label)
            atTable.push_back(id);
    }
    if (atTable.size() == 1)
        return openCheck(atTable.front()) ? TableOpened : TableFailed;
    if (atTable.size() > 1) {
        releaseCheck();
        setCheckFilter(label);
        return TableChooseCheck;
    }
    releaseCheck();
    pendingTable_ = label;
    entry_.clear();
    emit entryChanged();
    emit checkChanged();
    return TableNeedsGuests;
}

bool PosService::startCheck(CheckType type)
{
    if (!require(perm::Order, tr("Starting a check")))
        return false;
    if (type == CheckType::DineIn && pendingTable_.isEmpty())
        return fail(tr("Choose a table first."));

    Check c;
    c.id = ++s_->lastCheckId;
    c.type = type;
    c.serverId = user()->id;
    c.serverName = user()->name;
    c.openedAt = now();
    c.training = training();
    switch (type) {
    case CheckType::DineIn:
        // A practice check leaves the real table free.
        c.label = ss(c.training ? tr("%1 (practice)").arg(pendingTable_) : pendingTable_);
        c.guests = entryGuests();
        if (s_->settings.gratuityBp > 0 && c.guests >= s_->settings.gratuityMinGuests) {
            c.gratuityBp = s_->settings.gratuityBp;
            c.autoGratuity = true;
        }
        break;
    case CheckType::Takeout:
        c.label = ss(tr("Takeout %1").arg(c.id));
        break;
    case CheckType::Quick:
        c.label = ss(tr("Quick %1").arg(c.id));
        break;
    case CheckType::Delivery:
        c.label = ss(tr("Delivery %1").arg(c.id));
        break;
    case CheckType::Tab:
        c.label = ss(tr("Tab %1").arg(c.id));   // openTab names it
        break;
    }
    pendingTable_.clear();
    entry_.clear();
    emit entryChanged();

    const auto id = c.id;
    s_->open.emplace(id, std::move(c));
    if (currentId_ != 0)
        unlockCheck(currentId_);
    lockCheck(id);
    currentId_ = id;
    selectedLine_ = 0;
    selectedPayment_ = 0;
    changed(s_->open.at(id));
    return true;
}

bool PosService::openTab(const QString &name)
{
    // The name typed on the keyboard page, or given.
    const QString who = (name.isEmpty() ? text_ : name).trimmed();
    if (who.isEmpty())
        return fail(tr("Type the name for the tab."));
    if (!startCheck(CheckType::Tab))
        return false;
    Check &c = *current();
    c.label = ss(who.left(30));
    c.customer.name = c.label;
    text_.clear();
    emit entryChanged();
    emit notice(tr("Tab open: %1").arg(who.left(30)));
    changed(c);
    return true;
}

bool PosService::openCheck(std::int64_t checkId)
{
    if (!s_->open.contains(checkId))
        return fail(tr("That check is no longer open."));
    // Someone else's: those who may (managers, the counter), or a manager's PIN.
    if (Check &c = s_->open.at(checkId); user() && !mayOpen(c)
        && !require(perm::OthersChecks, tr("Opening %1's check").arg(qs(c.serverName)), &c))
        return false;
    if (const QString holder = lockHolder(checkId); !holder.isEmpty())
        return fail(tr("%1 is open on %2.").arg(qs(s_->open.at(checkId).label), holder));
    if (currentId_ != 0 && currentId_ != checkId)
        unlockCheck(currentId_);
    lockCheck(checkId);
    currentId_ = checkId;
    seat_ = 0;
    course_ = 1;
    choosingLine_ = 0;
    selectedLine_ = 0;
    selectedPayment_ = 0;
    pendingTable_.clear();
    checkFilter_.clear();
    emit checkChanged();
    emit s_->checksChanged();
    return true;
}

void PosService::releaseCheck()
{
    if (currentId_ == 0 && pendingTable_.isEmpty() && checkFilter_.isEmpty())
        return;
    checkFilter_.clear();
    // An empty takeout / delivery / quick check that is put away was never
    // really started: discard it instead of leaving it open. (An empty table
    // check stays: the guests may be seated before they order.)
    if (Check *c = current(); c && c->type != CheckType::DineIn && c->type != CheckType::Tab && c->lines.empty()
                              && c->payments.empty()) {
        c->status = CheckStatus::Discarded;
        c->closedAt = now();
        if (s_->sink)
            s_->sink->saveCheck(*c);
        s_->open.erase(c->id);
    }
    unlockCheck(currentId_);
    currentId_ = 0;
    seat_ = 0;
    course_ = 1;
    choosingLine_ = 0;
    selectedLine_ = 0;
    selectedPayment_ = 0;
    pendingTable_.clear();
    qualifier_ = Qualifier::None;
    emit qualifierChanged();
    emit checkChanged();
    emit s_->checksChanged();
}

QVariantMap PosService::weighingInfo() const
{
    const MenuItem *item = weighing_.isEmpty() ? nullptr : findItem(weighing_);
    if (!item)
        return {{u"active"_s, false}};
    // What it comes to for the weight typed so far (hundredths: 125 = 1.25).
    OrderLine probe;
    probe.unitPrice = item->price;
    probe.weight = entry_.toLongLong() * 10;
    return {{u"active"_s, true}, {u"item"_s, qs(item->name)}, {u"unit"_s, qs(item->weightUnit)},
            {u"price"_s, format(item->price)}, {u"comesTo"_s, probe.weight > 0 ? format(probe.total()) : QString()}};
}

bool PosService::addWeighed()
{
    if (weighing_.isEmpty())
        return fail(tr("Nothing is being weighed."));
    if (entry_.isEmpty() || entry_.toLongLong() <= 0)
        return fail(tr("Type the weight: 125 is 1.25."));
    return addItem(weighing_);
}

bool PosService::cancelWeighing()
{
    weighing_.clear();
    entry_.clear();
    emit entryChanged();
    emit checkChanged();
    return true;
}

bool PosService::addItem(const QString &idOrName)
{
    if (!require(perm::Order, tr("Ordering")))
        return false;
    const MenuItem *item = findItem(idOrName);
    if (!item)
        return fail(tr("'%1' is not on the menu.").arg(idOrName));
    if (!item->available)
        return fail(tr("%1 is sold out.").arg(qs(item->name)));
    if (item->ticketCapacity > 0) {   // tickets to an event: only as many as there are seats
        if (item->eventAt > 0 && now() > item->eventAt)
            return fail(tr("%1 has already taken place.").arg(qs(item->name)));
        if (ticketsLeft(*item) < 1)
            return fail(tr("%1 is sold out (%2 tickets).").arg(qs(item->name)).arg(item->ticketCapacity));
    }

    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check &c = *current();

    const Qualifier q = qualifier_;
    // No / Lite / Side go on a choice or an add-on, not a whole item: still
    // waiting for one. (Extra on an item is a bigger portion; Sub swaps one in.)
    if ((q == Qualifier::No || q == Qualifier::Lite || q == Qualifier::Side) && !item->isModifier)
        return fail(tr("%1 goes on a choice: touch Choose on the item, then what to change.")
                        .arg(qs(qualifierPrefix(q)).trimmed()));
    // "Extra" costs what the store says (Store Settings), on top of the price.
    const auto extra = [&](Money price) { return q == Qualifier::Extra ? s_->settings.withExtra(price) : price; };
    if (q == Qualifier::Sub && !item->isModifier) {
        // In place of part of the item before it, at its substitute price.
        if (!item->substitute)
            return fail(tr("%1 can't be a substitute (Manager -> Menu).").arg(qs(item->name)));
        OrderLine *target = c.line(selectedLine_);
        if (!target || target->sent || target->isComment())
            target = c.lastItemLine();
        MenuItem sub = *item;
        sub.price = item->substitutePrice;
        if (!target || !c.addModifier(target->id, sub, q))
            return fail(tr("Order the item it goes with first."));
        selectedLine_ = target->id;
    } else if (item->isModifier) {
        OrderLine *target = c.line(selectedLine_);
        if (!target || target->sent || target->isComment())
            target = c.lastItemLine();
        MenuItem modifier = *item;
        modifier.price = extra(item->price);
        if (!target || !c.addModifier(target->id, modifier, q))
            return fail(tr("Order an item before adding %1.").arg(qs(item->name)));
        selectedLine_ = target->id;
    } else if (item->byWeight && entry_.isEmpty()) {
        // Sold by weight: the Weigh page asks how much (then addWeighed).
        weighing_ = qs(item->id);
        emit checkChanged();
        return true;
    } else {
        MenuItem priced = *item;   // the price for this meal period (dinner, happy hour...) and order type
        priced.price = extra(item->priceFor(currentMealPeriod(), c.type == CheckType::Takeout, c.type == CheckType::Delivery));
        if (item->eventAt > 0)   // the ticket says when
            priced.name += " (" + ss(i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(item->eventAt),
                                                         u"ddd MMM d, h:mm AP"_s)) + ")";
        OrderLine &line = c.addItem(priced, q);
        if (const auto hits = allergyHits(c, line); !hits.empty())
            emit notice(tr("⚠ %1 contains %2: the guest is allergic. The kitchen is told.")
                            .arg(qs(item->name), allergenNames(hits).join(u", "_s)));
        if (item->ticketCapacity > 0)
            emit notice(tr("%1: %2 tickets left").arg(qs(item->name)).arg(ticketsLeft(*item)));
        if (item->byWeight) {   // the weight typed: hundredths (125 = 1.25 lb)
            line.weight = entry_.toLongLong() * 10;
            entry_.clear();
            weighing_.clear();
            emit entryChanged();
            if (line.weight <= 0) {
                c.removeLine(line.id);
                return fail(tr("Type the weight first."));
            }
        }
        line.seat = seat_;
        line.course = course_;
        selectedLine_ = line.id;
        lineTouched_ = false;
        // Items with modifier groups ask for their choices next.
        choosingLine_ = 0;
        choosingBefore_.reset();
        for (const std::string &g : item->modifierGroups) {
            if (s_->settings.modifierGroup(g))
                choosingLine_ = line.id;
        }
    }
    if (qualifier_ != Qualifier::None) {
        qualifier_ = Qualifier::None;
        emit qualifierChanged();
    }
    changed(c);
    return true;
}

void PosService::setQualifier(const QString &qualifier)
{
    const Qualifier q = qualifierFromString(ss(qualifier));
    qualifier_ = (q == qualifier_) ? Qualifier::None : q;
    emit qualifierChanged();
    // What it's waiting for.
    switch (qualifier_) {
    case Qualifier::No: emit notice(tr("No: now touch what to leave off")); break;
    case Qualifier::Lite: emit notice(tr("Lite: now touch what to go easy on")); break;
    case Qualifier::Extra: emit notice(tr("Extra: now touch a choice, or an item for a bigger portion")); break;
    case Qualifier::Side: emit notice(tr("Side: now touch what goes on the side")); break;
    case Qualifier::Sub: emit notice(tr("Sub: now touch the item to swap in")); break;
    default: break;
    }
}

void PosService::selectLine(qint64 lineId)
{
    lineTouched_ = lineId != 0;   // seat / course now apply to it
    if (lineId == selectedLine_)
        return;
    selectedLine_ = lineId;
    emit checkChanged();
}

void PosService::selectPayment(qint64 paymentId)
{
    if (paymentId == selectedPayment_)
        return;
    selectedPayment_ = paymentId;
    emit checkChanged();
}

bool PosService::voidItem()
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    OrderLine *l = c->line(selectedLine_);
    if (!l || l->voided) {
        // Default to the newest line still counting toward the total.
        l = nullptr;
        for (auto it = c->lines.rbegin(); it != c->lines.rend() && !l; ++it) {
            if (!it->voided)
                l = &*it;
        }
    }
    if (!l)
        return fail(tr("Nothing to void."));
    const QString name = qs(l->displayName());
    if (!l->sent) {
        rememberChange(*c, *l, true, tr("Removed %1").arg(name));
        c->removeLine(l->id);
    } else {
        if (!haveReason("void", tr("Void %1").arg(name)))
            return false;
        if (!require(perm::Void, tr("Voiding sent items")))
            return false;
        const Money was = l->total();
        c->voidLine(l->id);
        if (!c->training)
            takeStock({*l}, -1);   // not made: back on the shelf
        noteEvent(*c, tr("Voided %1 (%2)").arg(name, format(was)), "void", was, takeReason());
        if (s_->printer && !c->training)
            s_->printer->printKitchen(s_->settings, *c, {*l}, true);
        emit notice(tr("Voided %1").arg(name));
    }
    selectedLine_ = 0;
    changed(*c);
    return true;
}

bool PosService::setLineQuantity(qint64 lineId, int quantity)
{
    if (!require(perm::Order, tr("Ordering")))
        return false;
    Check *c = current();
    OrderLine *l = c ? c->line(lineId ? lineId : selectedLine_) : nullptr;
    if (!l || l->isComment() || l->voided)
        return fail(tr("Touch an item on the check first."));
    if (l->sent)
        return fail(tr("%1 was sent: Again adds more, Void takes it off.").arg(qs(l->displayName())));
    if (l->weight > 0 || l->isGiftCard() || l->isShare())
        return fail(tr("%1 can't have a quantity.").arg(qs(l->displayName())));
    quantity = std::clamp(quantity, 1, 99);
    if (quantity == l->quantity)
        return true;
    if (quantity > l->quantity)
        if (const MenuItem *m = findItem(qs(l->itemId)); m && m->ticketCapacity > 0
            && ticketsLeft(*m) < quantity - l->quantity)
            return fail(tr("%1 is sold out (%2 tickets).").arg(qs(m->name)).arg(m->ticketCapacity));
    if (quantity < l->quantity)
        rememberChange(*c, *l, false, tr("%1: %2 instead of %3").arg(qs(l->displayName())).arg(quantity).arg(l->quantity));
    l->quantity = quantity;
    selectedLine_ = l->id;
    changed(*c);
    return true;
}

std::vector<const OrderLine *> PosService::lastRound(const Check &c) const
{
    // Drinks: the bar makes them, or they're in a drinks family.
    const auto drink = [&](const OrderLine &l) {
        if (l.isComment() || l.voided || !l.sent || l.isGiftCard())
            return false;
        if (l.printerOf() == "bar")
            return true;
        const MenuItem *m = findItem(qs(l.itemId));
        return m && QString::fromStdString(m->family).contains(u"drink"_s, Qt::CaseInsensitive);
    };
    std::int64_t last = 0;
    for (const OrderLine &l : c.lines)
        if (drink(l))
            last = std::max(last, l.sentAt);
    std::vector<const OrderLine *> out;
    for (const OrderLine &l : c.lines)
        if (drink(l) && l.sentAt == last)
            out.push_back(&l);
    return out;
}

bool PosService::anotherRound()
{
    if (!require(perm::Order, tr("Ordering")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    std::vector<OrderLine> round;
    for (const OrderLine *l : lastRound(*c))
        round.push_back(*l);
    if (round.empty())
        return fail(tr("No drinks have been sent on this check yet."));
    QStringList skipped;
    int added = 0;
    for (OrderLine copy : round) {
        const MenuItem *m = findItem(qs(copy.itemId));
        if (!m || !m->available) {
            skipped << qs(copy.name);
            continue;
        }
        copy.id = c->nextLineId++;
        copy.unshare();   // another of it, whole
        copy.sent = copy.voided = copy.made = copy.served = false;
        copy.sentAt = copy.madeAt = copy.servedAt = 0;
        c->lines.push_back(copy);
        selectedLine_ = copy.id;
        added += copy.quantity;
    }
    if (added == 0)
        return fail(tr("Sold out: %1").arg(skipped.join(u", "_s)));
    emit notice(skipped.isEmpty() ? tr("Another round: %n drink(s)", nullptr, added)
                                  : tr("Another round: %n drink(s) (sold out: %1)", nullptr, added).arg(skipped.join(u", "_s)));
    changed(*c);
    return true;
}

void PosService::rememberChange(const Check &c, const OrderLine &before, bool removed, const QString &text)
{
    const auto it = std::ranges::find(c.lines, before.id, &OrderLine::id);
    lastChange_ = LastChange{c.id, before, std::size_t(it - c.lines.begin()), removed, now(), text};
}

QString PosService::undoText() const
{
    // For a short while, and only on the check it happened on.
    if (!lastChange_ || lastChange_->checkId != currentId_ || now() - lastChange_->at > 30'000)
        return {};
    return lastChange_->text;
}

bool PosService::undoLast()
{
    Check *c = current();
    if (undoText().isEmpty() || !c)
        return fail(tr("Nothing to undo."));
    const LastChange u = *lastChange_;
    lastChange_.reset();
    if (u.removed) {
        if (c->line(u.before.id))
            return fail(tr("Nothing to undo."));
        c->lines.insert(c->lines.begin() + std::ptrdiff_t(std::min(u.index, c->lines.size())), u.before);
    } else {
        OrderLine *l = c->line(u.before.id);
        if (!l || l->sent)
            return fail(tr("Nothing to undo."));
        l->quantity = u.before.quantity;
    }
    selectedLine_ = u.before.id;
    emit notice(tr("Put back %1").arg(qs(u.before.displayName())));
    changed(*c);
    return true;
}

bool PosService::changeLineQuantity(qint64 lineId, int by)
{
    Check *c = current();
    const OrderLine *l = c ? c->line(lineId ? lineId : selectedLine_) : nullptr;
    if (l && l->sent && by > 0)
        return repeatLine(l->id);
    if (l && !l->sent && !l->voided && l->quantity + by < 1)   // the last one: off the check
        return voidItem();
    return setLineQuantity(l ? l->id : 0, (l ? l->quantity : 1) + by);
}

bool PosService::repeatLine(qint64 lineId)
{
    if (!require(perm::Order, tr("Ordering")))
        return false;
    Check *c = current();
    const OrderLine *l = c ? c->line(lineId ? lineId : selectedLine_) : nullptr;
    if (!l || l->isComment() || l->isGiftCard())
        return fail(tr("Touch an item on the check first."));
    const MenuItem *m = findItem(qs(l->itemId));
    if (!m || !m->available)
        return fail(tr("%1 is sold out.").arg(qs(l->name)));
    if (m->ticketCapacity > 0 && ticketsLeft(*m) < 1)
        return fail(tr("%1 is sold out (%2 tickets).").arg(qs(m->name)).arg(m->ticketCapacity));
    OrderLine copy = *l;
    copy.id = c->nextLineId++;
    copy.quantity = 1;
    copy.unshare();
    copy.sent = copy.voided = copy.made = copy.served = false;
    copy.sentAt = copy.madeAt = copy.servedAt = 0;
    c->lines.push_back(copy);
    selectedLine_ = copy.id;
    choosingLine_ = 0;
    emit notice(tr("One more %1").arg(qs(copy.displayName())));
    changed(*c);
    return true;
}

QString PosService::dueText(std::int64_t at) const
{
    const QDate today = dateOf(now());
    const QDate day = dateOf(at);
    if (day == today)
        return timeOfDay(at);
    if (day == today.addDays(1))
        return tr("tomorrow %1").arg(timeOfDay(at));
    return tr("%1, %2").arg(i18n::locale().toString(day, u"ddd MMM d"_s), timeOfDay(at));
}

bool PosService::waitingForLater(const Check &c) const
{
    return c.dueAt > 0 && now() < c.dueAt - std::int64_t(s_->settings.laterLeadMinutes) * 60'000;
}

bool PosService::forAnotherDay(const Check &c) const
{
    return c.dueAt > 0 && dateOf(c.dueAt) > dateOf(now());
}

bool PosService::setDueAt(qint64 at)
{
    if (!require(perm::Order, tr("Orders for later")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("Start the order first."));
    if (at <= 0) {
        c->dueAt = 0;
        emit notice(tr("As soon as possible"));
        changed(*c);
        return true;
    }
    if (at <= now())
        return fail(tr("Pick a time that hasn't passed."));
    if (at > now() + std::int64_t(60) * 24 * 3600 * 1000)
        return fail(tr("Orders can be taken up to 60 days ahead."));
    if (forAnotherDay(Check{.dueAt = at}) && !c->payments.empty())
        return fail(tr("This order is already paid: it can only be for today."));
    if (std::ranges::any_of(c->lines, [](const OrderLine &l) { return l.sent && !l.voided; }))
        return fail(tr("Part of this order is already in the kitchen."));
    c->dueAt = at;
    emit notice(tr("Ready %1. It goes to the kitchen %2 minutes before.").arg(dueText(at)).arg(s_->settings.laterLeadMinutes));
    changed(*c);
    return true;
}

int PosService::fireDueOrders()
{
    int fired = 0;
    // Paced courses whose time has come.
    for (auto &[id, c] : s_->open) {
        if (c.fireAt <= 0 || c.fireAt > now())
            continue;
        if (c.heldCount() == 0) {
            c.fireAt = 0;
            continue;
        }
        if (!fireCourseOn(c, true)) {
            c.fireAt = 0;   // can't (a choice is missing): don't keep trying; the server fires it
            emit notice(tr("%1: the next course didn't fire (see the check).").arg(qs(c.label)));
            changed(c);
        } else {
            ++fired;
        }
    }
    for (auto &[id, c] : s_->open) {
        if (c.dueAt <= 0 || c.unsentCount() == 0 || waitingForLater(c) || c.training)
            continue;
        if (s_->lockedBy.contains(id))   // someone is still on it: when they let go
            continue;
        const std::vector<OrderLine> fresh = c.sendable(true);
        if (const QString missing = missingChoice(fresh); !missing.isEmpty()) {
            emit notice(tr("Order for later %1 can't go to the kitchen: %2").arg(qs(c.label), missing));
            continue;
        }
        c.sendAll(now(), true);
        takeStock(fresh, 1);
        if (s_->printer)
            s_->printer->printKitchen(s_->settings, c, fresh, false);
        emit notice(tr("Order for later sent to the kitchen: %1 (ready %2)").arg(qs(c.label), dueText(c.dueAt)));
        changed(c);
        ++fired;
    }
    return fired;
}

bool PosService::sendOrder()
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (const QString who = missingWho(*c); !who.isEmpty() && c->unsentCount() > 0)
        return fail(who);
    if (waitingForLater(*c) && c->unsentCount() > 0) {   // the kitchen gets it in time, by itself
        if (const QString missing = missingChoice(c->sendable(true)); !missing.isEmpty())
            return fail(missing);
        emit notice(tr("Saved for later: ready %1, it goes to the kitchen at %2.")
                        .arg(dueText(c->dueAt), timeOfDay(c->dueAt - std::int64_t(s_->settings.laterLeadMinutes) * 60'000)));
        return true;
    }
    const std::vector<OrderLine> fresh = c->sendable();
    if (const QString missing = missingChoice(fresh); !missing.isEmpty())
        return fail(missing);   // the kitchen needs the whole order
    const int n = c->sendAll(now());
    if (!c->training)
        takeStock(fresh, 1);
    const int held = c->heldCount();
    if (n == 0)
        return fail(held > 0 ? tr("The rest is on hold: Fire the next course when it's time.")
                             : tr("Nothing new to send."));
    if (c->training) {   // practice: the kitchen never sees it
        emit notice(tr("Practice: %1 items marked sent (not sent to the kitchen)").arg(n));
        changed(*c);
        return true;
    }
    // A phone order: the time they were told, from how busy the kitchen is now.
    if ((c->type == CheckType::Takeout || c->type == CheckType::Delivery) && !c->promisedAt && !c->dueAt)
        c->promisedAt = now() + std::int64_t(readyQuote(*c)) * 60'000;
    if (s_->printer)
        s_->printer->printKitchen(s_->settings, *c, fresh, false);
    const QString sent = n == 1 ? tr("Sent 1 item to the kitchen") : tr("Sent %1 items to the kitchen").arg(n);
    emit notice(held > 0 ? tr("%1; %2 on hold for a later course").arg(sent).arg(held) : sent);
    changed(*c);
    return true;
}

bool PosService::addComment()
{
    if (!require(perm::Order, tr("Adding notes")))
        return false;
    const QString text = text_.trimmed();
    if (text.isEmpty())
        return fail(tr("Type the note first."));
    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check &c = *current();
    OrderLine &note = c.addComment(ss(text));
    note.seat = seat_;
    note.course = course_;
    selectedLine_ = note.id;
    lineTouched_ = false;
    text_.clear();
    emit entryChanged();
    changed(c);
    return true;
}

bool PosService::tender(const QString &tenderId, std::optional<std::int64_t> amountCents)
{
    if (!require(perm::Settle, tr("Taking payments")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const Tender *t = s_->settings.tender(ss(tenderId));
    if (!t)
        return fail(tr("Payment type '%1' is not set up.").arg(tenderId));
    if (forAnotherDay(*c))   // the money goes in that day's drawer
        return fail(tr("This order is for %1: take the payment that day.").arg(dueText(c->dueAt)));
    if (t->kind == TenderKind::GiftCard)   // needs the card: the Gift Card page
        return giftCardNumber_.isEmpty() ? fail(tr("Open Gift Card and enter or swipe the card first."))
                                         : payWithGiftCard({}, amountCents.value_or(0));

    const Totals before = c->totals(s_->settings.tax);
    Money amount;
    if (t->kind != TenderKind::Discount) {
        if (before.balance.cents() <= 0)
            return fail(tr("Nothing is owed on this check."));
        if (amountCents)
            amount = Money::fromCents(*amountCents);
        else if (!entry_.isEmpty())
            amount = Money::fromCents(entry_.toLongLong());
        else
            amount = before.balance;
        // Cash where pennies are gone: the balance, rounded.
        if (t->kind == TenderKind::Cash && !amountCents && entry_.isEmpty() && s_->settings.tax.cashRoundingCents > 1) {
            const std::int64_t step = s_->settings.tax.cashRoundingCents;
            amount = Money::fromCents((amount.cents() + step / 2) / step * step);
        }
        if (amount.cents() <= 0)
            return fail(tr("Enter an amount."));
        // Only cash can be over-tendered (to give change).
        if (t->kind != TenderKind::Cash && amount > before.balance)
            amount = before.balance;
    }
    if (t->kind == TenderKind::HouseAccount) {
        entry_.clear();
        emit entryChanged();
        return chargeHouseAccount(*c, *t, amount);
    }
    if (t->kind == TenderKind::Discount && !t->staffMeal && !haveReason("comp", qs(t->name)))
        return false;
    if (t->kind == TenderKind::Discount && !require(perm::Discount, tr("Discounts and comps")))
        return false;
    Payment &paid = c->addPayment(*t, amount);
    if (t->staffMeal) {   // whose meal: the person ringing it in
        paid.reference = user()->name;
        noteEvent(*c, tr("Staff meal: %1").arg(qs(user()->name)), "discount");
    }
    // A tip the guest chose on the customer display goes on their card.
    if (t->kind == TenderKind::Card && tipChoice_.chosen && tipChoice_.checkId == c->id)
        paid.tip = tipFor(*c);
    entry_.clear();
    emit entryChanged();
    const Totals after = c->totals(s_->settings.tax);
    if (t->kind == TenderKind::Discount)   // for the audit trail and the exceptions report
        noteEvent(*c, tr("Discount: %1").arg(qs(t->name)), "discount", after.discounts - before.discounts, takeReason());
    if (after.change.cents() > 0)
        emit notice(tr("Change due: %1").arg(format(after.change)));
    else
        emit notice(tr("%1 applied").arg(qs(t->name)));
    changed(*c);
    return true;
}

bool PosService::customDiscount(bool percent)
{
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const std::int64_t typed = entry_.toLongLong();
    if (typed <= 0)
        return fail(percent ? tr("Type the percent (15 for 15%), then % Off.")
                            : tr("Type the amount off, then $ Off."));
    if (percent && typed > 100)
        return fail(tr("A discount is 100% at most."));
    const Totals before = c->totals(s_->settings.tax);
    if (!percent && Money::fromCents(typed) > before.items - before.discounts)
        return fail(tr("That's more than the items on the check."));
    if (!haveReason("comp", percent ? tr("%1% off").arg(typed) : tr("%1 off").arg(format(Money::fromCents(typed)))))
        return false;
    if (!require(perm::Discount, tr("Discounts and comps")))
        return false;
    Tender t;
    t.id = "custom";
    t.kind = TenderKind::Discount;
    t.percentBp = percent ? typed * 100 : 0;
    t.name = ss(percent ? tr("%1% off").arg(typed) : tr("%1 off").arg(format(Money::fromCents(typed))));
    Payment &p = c->addPayment(t, Money());
    if (!percent)
        p.amount = Money::fromCents(typed);
    entry_.clear();
    emit entryChanged();
    const Totals after = c->totals(s_->settings.tax);
    noteEvent(*c, tr("Discount: %1").arg(qs(t.name)), "discount", after.discounts - before.discounts, takeReason());
    emit notice(tr("%1 applied").arg(qs(t.name)));
    changed(*c);
    return true;
}

bool PosService::removePayment()
{
    Check *c = current();
    if (!c || c->payments.empty())
        return fail(tr("No payment to remove."));
    if (!require(perm::Settle, tr("Removing payments")))
        return false;
    // The selected payment, else the most recent one.
    const auto chosen = std::ranges::find_if(c->payments, [&](const Payment &p) { return p.id == selectedPayment_; });
    const Payment removed = chosen != c->payments.end() ? *chosen : c->payments.back();
    // A card taken on a Stripe reader: money back to the guest, a manager's
    // call; it comes off when Stripe says so.
    if (removed.processor == "stripe" && !c->training && !require(perm::Manager, tr("Refunding a card")))
        return false;
    switch (refundCardPayment(*c, removed)) {
    case RefundStart::Started: return true;
    case RefundStart::CantNow: return false;
    case RefundStart::NotNeeded: break;
    }
    const Money discountsBefore = c->totals(s_->settings.tax).discounts;
    c->removePayment(removed.id);
    if (removed.reference.starts_with("promo:"))   // a coupon: off the check (a promotion by itself comes back)
        std::erase(c->coupons, removed.reference.substr(6));
    returnPayment(*c, removed);
    if (removed.kind != TenderKind::Discount)
        noteEvent(*c, tr("Payment taken back: %1 %2").arg(qs(removed.tenderName), format(removed.amount)), "unpay",
                  removed.amount);
    else
        noteEvent(*c, tr("Discount taken off: %1").arg(qs(removed.tenderName)), "undiscount",
                  discountsBefore - c->totals(s_->settings.tax).discounts);
    selectedPayment_ = 0;
    emit notice(tr("Payment removed"));
    changed(*c);
    return true;
}

bool PosService::closeCheck()
{
    if (!require(perm::Settle, tr("Closing checks")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const Totals t = c->totals(s_->settings.tax);
    if (t.balance.cents() > 0)
        return fail(tr("%1 is still due.").arg(format(t.balance)));
    if (c->training)
        return closePractice(*c);
    // (A cash tip handed over for the server is cash here too.)
    const bool cash = t.cashPaid.cents() > 0 || c->cashTip.cents() > 0;
    // With server banks the cash stays with whoever closes the check (their
    // bank starts with the first cash sale); otherwise it goes in this
    // terminal's drawer, which must be open.
    DrawerSession *drawer = !cash ? nullptr : serverBank() ? ensureMyBank() : myDrawer();
    if (cash && !drawer)
        return fail(noDrawerMessage());

    if (waitingForLater(*c) && c->unsentCount() > 0)
        return fail(tr("This order is for %1: close it when it's picked up.").arg(dueText(c->dueAt)));
    if (c->unsentCount() > 0) {
        // Closing sends whatever is left, held courses too (a phone order
        // still needs its name first).
        if (const QString who = missingWho(*c); !who.isEmpty())
            return fail(who);
        const std::vector<OrderLine> fresh = c->sendable(true);
        if (const QString missing = missingChoice(fresh); !missing.isEmpty())
            return fail(missing);
        c->sendAll(now(), true);
        if (!c->training)
        takeStock(fresh, 1);
        if (s_->printer && !fresh.empty() && !c->training)
            s_->printer->printKitchen(s_->settings, *c, fresh, false);
    }
    if (c->type == CheckType::Takeout || c->type == CheckType::Delivery)
        rememberCustomer(*c);   // on file for next time
    c->status = CheckStatus::Closed;
    if (c->outAt && !c->deliveredAt)
        c->deliveredAt = now();   // paid: the driver is back
    c->closedAt = now();
    c->businessDay = s_->day.id;
    applyCloseEffects(*c);
    if (cash)
        c->drawerSession = drawer->id;
    if (s_->sink)
        s_->sink->saveCheck(*c);
    if (cash && s_->printer && !serverBank() && terminalHasDrawer())
        s_->printer->openDrawer(s_->settings, receiptPrinter());
    s_->closedToday.push_back(*c);
    lastClosedId_ = c->id;
    receiptAfterClosing(s_->closedToday.back());   // printed, or offered (this terminal's choice)
    const qint64 id = c->id;
    const bool table = c->type == CheckType::DineIn && !c->training;
    const std::string label = c->label;
    unlockCheck(id);
    s_->open.erase(id);
    if (table)
        tableEmptied(label);   // the host stand sees it needs bussing
    currentId_ = 0;
    selectedLine_ = 0;
    selectedPayment_ = 0;
    emit notice(t.change.cents() > 0 ? tr("Check closed. Change: %1").arg(format(t.change)) : tr("Check closed"));
    emit checkChanged();
    emit s_->checksChanged();
    emit checkClosed(id);
    emit s_->dayChanged();
    if (cash)
        emit s_->drawerChanged();
    return true;
}

// --- QML-facing state ------------------------------------------------------------------

bool PosService::closePractice(Check &c)
{
    // Kept for its number, out of every sale, drawer and report.
    letHoldGo(c, tr("Held card let go"));
    c.sendAll(now(), true);
    c.status = CheckStatus::Discarded;
    c.closedAt = now();
    if (s_->sink)
        s_->sink->saveCheck(c);
    const qint64 id = c.id;
    unlockCheck(id);
    s_->open.erase(id);
    currentId_ = 0;
    selectedLine_ = 0;
    selectedPayment_ = 0;
    emit notice(tr("Practice check closed: not a sale"));
    emit checkChanged();
    emit s_->checksChanged();
    return true;
}

QString PosService::userName() const { return user() ? qs(user()->name) : QString(); }
QString PosService::userRole() const { return user() ? qs(user()->role) : QString(); }
QString PosService::storeName() const { return qs(s_->settings.storeName); }

bool PosService::clockedIn() const
{
    return user() && std::ranges::any_of(s_->punches, [&](const TimePunch &p) { return p.employeeId == user()->id && p.open(); });
}

bool PosService::toggleBreak()
{
    if (!user())
        return fail(tr("Log in first."));
    return toggleBreakFor(*user());
}

bool PosService::toggleBreakFor(const Employee &employee)
{
    const Employee *e = &employee;
    TimePunch *p = openPunch(e->id);
    if (!p)
        return fail(tr("Clock in first."));
    if (p->onBreak()) {
        p->breaks.back().end = now();
        emit notice(tr("Welcome back, %1").arg(qs(e->name)));
    } else {
        p->breaks.push_back({now(), 0});
        emit notice(tr("%1 is on break").arg(qs(e->name)));
    }
    if (s_->sink)
        s_->sink->savePunch(*p);
    emit sessionChanged();
    emit s_->dayChanged();
    return true;
}

QString PosService::onBreakSince() const
{
    if (!user())
        return {};
    for (const TimePunch &p : s_->punches) {
        if (p.employeeId == user()->id && p.open() && p.onBreak())
            return timeOfDay(p.breaks.back().start);
    }
    return {};
}

QString PosService::clockedInSince() const
{
    if (!user())
        return {};
    for (const TimePunch &p : s_->punches) {
        if (p.employeeId == user()->id && p.open())
            return timeOfDay(p.clockIn);
    }
    return {};
}

QString PosService::entryAmount() const
{
    return format(Money::fromCents(entry_.toLongLong()));
}

int PosService::entryGuests() const
{
    return std::clamp(entry_.isEmpty() ? 1 : entry_.toInt(), 1, kMaxGuests);
}

QString PosService::pendingQualifier() const
{
    return qs(toString(qualifier_));
}

QVariantMap PosService::checkInfo() const
{
    const Check *c = currentCheck();
    if (!c)
        return {};
    return {
        {u"id"_s, qint64(c->id)}, {u"label"_s, qs(c->label)}, {u"guests"_s, c->guests},
        {u"server"_s, qs(c->serverName)}, {u"type"_s, qs(toString(c->type))},
        {u"seat"_s, seat_}, {u"course"_s, course_}, {u"firedCourse"_s, c->firedCourse},
        {u"heldCount"_s, c->heldCount()}, {u"roundSize"_s, int(lastRound(*c).size())},
        {u"firesAt"_s, c->fireAt ? timeOfDay(c->fireAt) : QString()}, {u"rush"_s, c->rush}, {u"vip"_s, c->vip},
        {u"allergies"_s, [c] { QStringList a; for (const std::string &x : c->allergies) a << qs(x); return a; }()},
        {u"coupons"_s, checkCoupons()}, {u"hold"_s, holdInfo(*c)}, {u"tab"_s, c->type == CheckType::Tab},
        {u"tabHold"_s, s_->settings.tabHold > 0 ? format(Money::fromCents(s_->settings.tabHold)) : QString()},
        {u"opened"_s, timeOfDay(c->openedAt)},
        {u"dueAt"_s, qint64(c->dueAt)}, {u"due"_s, c->dueAt ? dueText(c->dueAt) : QString()},
        {u"customer"_s, QVariantMap{{u"name"_s, qs(c->customer.name)}, {u"phone"_s, qs(c->customer.phone)},
                                    {u"address"_s, qs(c->customer.address)}, {u"note"_s, qs(c->customer.note)}}},
        // Phone orders: their last order, the ready time (quoted, or what was promised), the driver.
        {u"lastOrder"_s, lastOrderText(*c)},
        {u"quote"_s, (c->type == CheckType::Takeout || c->type == CheckType::Delivery) && !c->dueAt && !c->promisedAt
                         ? readyQuote(*c) : 0},
        {u"promised"_s, c->promisedAt ? timeOfDay(c->promisedAt) : QString()},
        {u"needsWho"_s, !missingWho(*c).isEmpty()},
        {u"driver"_s, qs(c->driverName)},
    };
}

QVariantList PosService::lines() const
{
    QVariantList out;
    const Check *c = currentCheck();
    if (!c)
        return out;
    for (const OrderLine &l : c->lines) {
        QVariantList mods;
        for (const Modifier &m : l.modifiers) {
            mods.append(QVariantMap{{u"name"_s, qs(m.displayName())},
                                    {u"price"_s, m.price().cents() ? format(m.price()) : QString()}});
        }
        out.append(QVariantMap{
            {u"id"_s, qint64(l.id)}, {u"name"_s, qs(l.displayName())}, {u"quantity"_s, l.quantity},
            {u"price"_s, l.isComment() ? QString() : format(l.total())}, {u"comment"_s, l.isComment()},
            {u"sent"_s, l.sent}, {u"voided"_s, l.voided}, {u"modifiers"_s, mods},
            // − / + / Again apply (not a comment, gift card or weighed item).
            {u"countable"_s, !l.isComment() && !l.isGiftCard() && !l.isFee() && !l.voided && l.weight == 0 && !l.isShare()},
            {u"fee"_s, l.isFee()},
            {u"shared"_s, l.isShare()},
            {u"selected"_s, qint64(l.id) == selectedLine_},
            {u"seat"_s, l.seat}, {u"course"_s, l.course}, {u"held"_s, c->held(l)},
            // Its modifier groups can still be changed / a required one is missing.
            {u"choices"_s, !l.sent && !l.isComment() && [&] {
                 const MenuItem *m = findItem(qs(l.itemId));
                 return m && !m->modifierGroups.empty(); }()},
            {u"needsChoice"_s, !l.sent && !missingChoice({l}).isEmpty()},
        });
    }
    return out;
}

QVariantList PosService::tableChecks() const
{
    const Check *cur = currentCheck();
    if (!cur || cur->type != CheckType::DineIn)
        return {};
    QVariantList out;
    for (const auto &[id, c] : s_->open) {   // by id: the order they were opened
        if (c.type != CheckType::DineIn || c.label != cur->label)
            continue;
        out.append(QVariantMap{{u"id"_s, qint64(id)}, {u"number"_s, int(out.size()) + 1},
                               {u"total"_s, format(c.totals(s_->settings.tax).total)},
                               {u"lines"_s, int(c.lines.size())}, {u"current"_s, id == cur->id},
                               {u"busyOn"_s, lockHolder(id)}});
    }
    return out;
}

bool PosService::newTableCheck()
{
    const Check *cur = currentCheck();
    if (!cur || cur->type != CheckType::DineIn)
        return fail(tr("Open a table's check first."));
    const std::string label = cur->label;
    const std::int64_t before = currentId_;
    pendingTable_ = qs(label);
    entry_ = u"1"_s;   // one guest on it
    if (!startCheck(CheckType::DineIn))
        return false;
    // The same table name as the others (a practice check keeps its own).
    if (Check *c = current(); c && currentId_ != before) {
        c->label = label;
        if (s_->sink)
            s_->sink->saveCheck(*c);
    }
    emit checkChanged();
    emit s_->checksChanged();
    emit notice(tr("Check %1 at %2").arg(tableChecks().size()).arg(qs(label)));
    return true;
}

QVariantMap PosService::totals() const
{
    const Check *c = currentCheck();
    if (!c)
        return {};
    const Totals t = c->totals(s_->settings.tax);
    QVariantList taxLines;
    for (const auto &[cls, amount] : t.taxByClass) {
        QString name;
        switch (cls) {
        case TaxClass::Food: name = tr("Food tax"); break;
        case TaxClass::Alcohol: name = tr("Alcohol tax"); break;
        case TaxClass::Merchandise: name = tr("Merchandise tax"); break;
        case TaxClass::Room: name = tr("Room tax"); break;
        case TaxClass::None: name = tr("Tax"); break;
        }
        taxLines.append(QVariantMap{{u"name"_s, name}, {u"amount"_s, format(amount)}});
    }
    return {
        {u"items"_s, format(t.items)}, {u"discounts"_s, format(-t.discounts)},
        {u"hasDiscount"_s, t.discounts.cents() > 0},
        {u"subtotal"_s, format(t.subtotal)}, {u"tax"_s, format(t.tax)}, {u"taxLines"_s, taxLines},
        {u"total"_s, format(t.total)}, {u"paid"_s, format(t.paid)},
        {u"balance"_s, format(t.balance.cents() > 0 ? t.balance : Money())},
        {u"balanceCents"_s, qint64(t.balance.cents())},
        {u"change"_s, format(t.change)}, {u"hasChange"_s, t.change.cents() > 0},
        {u"rounding"_s, format(t.rounding)}, {u"hasRounding"_s, t.rounding.cents() != 0},
        {u"gratuity"_s, format(t.gratuity)}, {u"hasGratuity"_s, t.gratuity.cents() > 0},
        {u"cashTip"_s, format(c->cashTip)}, {u"hasCashTip"_s, c->cashTip.cents() > 0},
        {u"server"_s, qs(c->serverName)}, {u"mine"_s, user() && c->serverId == user()->id},
        {u"gratuityPercent"_s, double(c->gratuityBp) / 100.0}, {u"autoGratuity"_s, c->autoGratuity},
        // What the Add gratuity key offers: the store's party rate, else 18%.
        {u"storeGratuityPercent"_s, double(s_->settings.gratuityBp > 0 ? s_->settings.gratuityBp : 1800) / 100.0},
        {u"tips"_s, format(t.tips)}, {u"hasTips"_s, t.tips.cents() > 0},
        {u"hasCard"_s, std::ranges::any_of(c->payments, [](const Payment &p) { return p.kind == TenderKind::Card; })},
    };
}

QVariantList PosService::payments() const
{
    QVariantList out;
    const Check *c = currentCheck();
    if (!c)
        return out;
    const Money items = c->totals(s_->settings.tax).items;
    for (const Payment &p : c->payments) {
        const QString amount = p.kind != TenderKind::Discount ? format(p.amount)
            : p.percentBp == 0 ? format(-p.amount)   // a fixed amount off (reward, promotion)
            : u"%1 (%2%)"_s.arg(format(-items.percent(p.percentBp))).arg(double(p.percentBp) / 100.0);
        // A card from a reader: which one ("Credit Card · Visa •••• 4242").
        QString name = qs(p.tenderName);
        if (!p.last4.empty()) {
            QString brand = qs(p.cardBrand);
            if (!brand.isEmpty())
                brand[0] = brand[0].toUpper();
            name += u"  ·  "_s + (brand.isEmpty() ? QString() : brand + u' ') + u"•••• "_s + qs(p.last4);
        }
        if (refunding_.contains(p.id))
            name += u"  ·  "_s + tr("refunding…");
        out.append(QVariantMap{{u"id"_s, qint64(p.id)}, {u"name"_s, name}, {u"amount"_s, amount},
                               {u"tip"_s, p.tip.cents() ? format(p.tip) : QString()},
                               {u"card"_s, p.kind == TenderKind::Card},
                               {u"selected"_s, qint64(p.id) == selectedPayment_}});
    }
    return out;
}

// Theirs; a delivery they're driving (they collect for it); a kiosk order
// (the counter's) for anyone who takes payments; or anyone's with "Open
// other people's checks".
bool PosService::mayOpen(const Check &c) const
{
    const Employee *e = user();
    if (!e)
        return false;
    return c.serverId == e->id || (!c.driverId.empty() && c.driverId == e->id) || permissionsOf(*e).contains(perm::OthersChecks)
           || (c.kiosk && permissionsOf(*e).contains(perm::Settle));
}

QVariantList PosService::openChecks() const
{
    QVariantList out;
    const std::int64_t t = now();
    for (const auto &[id, c] : s_->open) {
        const Money total = c.totals(s_->settings.tax).total;
        out.append(QVariantMap{
            {u"id"_s, qint64(id)}, {u"label"_s, qs(c.label)}, {u"server"_s, qs(c.serverName)},
            {u"guests"_s, c.guests}, {u"total"_s, format(total)}, {u"totalCents"_s, qint64(total.cents())},
            {u"minutes"_s, qint64((t - c.openedAt) / 60000)}, {u"type"_s, qs(toString(c.type))},
            {u"openedAt"_s, qint64(c.openedAt)}, {u"longAfter"_s, s_->settings.tableLongMinutes},
            {u"mine"_s, user() && c.serverId == user()->id}, {u"current"_s, id == currentId_},
            // In "My checks": theirs, and the counter's kiosk orders for whoever takes payments.
            {u"forMe"_s, user() && (c.serverId == user()->id || (!c.driverId.empty() && c.driverId == user()->id)
                                    || (c.kiosk && permissionsOf(*user()).contains(perm::Settle)))},
            {u"mayOpen"_s, mayOpen(c)}, {u"kiosk"_s, c.kiosk},
            {u"lineCount"_s, int(c.lines.size())}, {u"busyOn"_s, lockHolder(id)},
            {u"customer"_s, qs(c.customer.name)}, {u"due"_s, c.dueAt ? dueText(c.dueAt) : QString()},
        });
    }
    return out;
}

std::set<std::string> PosService::permissionsOf(const Employee &e) const
{
    std::set<std::string> out = e.permissions();
    // The store's: hosts take payments (unless that person may not).
    if (s_->settings.hostsTakePayments && e.role == "host")
        for (const char *p : {perm::Settle, perm::OthersChecks})
            if (!e.deny.contains(p))
                out.insert(p);
    return out;
}

QStringList PosService::permissions() const
{
    QStringList out;
    if (const Employee *e = user()) {
        for (const std::string &p : permissionsOf(*e))
            out << qs(p);
    }
    return out;
}

QString PosService::currencySymbol() const
{
    return qs(s_->settings.currencySymbol);
}

QStringList PosService::customColors() const
{
    QStringList out;
    for (const std::string &c : s_->settings.customColors)
        out << qs(c);
    return out;
}

// --- customers ---------------------------------------------------------------------------

bool PosService::setCustomer(const QVariantMap &customer)
{
    if (!require(perm::Order, tr("Changing customer details")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    c->customer = {ss(customer.value(u"name"_s).toString().trimmed()), ss(customer.value(u"phone"_s).toString().trimmed()),
                   ss(customer.value(u"address"_s).toString().trimmed()), ss(customer.value(u"note"_s).toString().trimmed())};
    emit notice(tr("Customer saved"));
    changed(*c);
    return true;
}

} // namespace vt::app
