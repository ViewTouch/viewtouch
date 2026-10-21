#pragma once

#include "core/check.hh"
#include "core/customer.hh"
#include "core/day.hh"
#include "core/employee.hh"
#include "core/inventory.hh"
#include "core/menu.hh"
#include "core/report.hh"
#include "core/settings.hh"
#include "core/waitlist.hh"
#include "app/pos_session.hh"
#include "app/tax_areas.hh"

#include <QJsonObject>
#include <QSet>
#include <QTimer>

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <set>
#include <unordered_set>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace vt::app {

// "11.50", "$11.50", "11,50" (with `symbol`, the currency's): a price in
// cents; -1 if it isn't one.
qint64 priceCents(QString text, const QString &symbol);

// Where the service hands off changes. Implementations must not block: the
// SQLite one queues writes to a worker thread.
class PosSink {
public:
    virtual ~PosSink() = default;
    virtual void saveCheck(const core::Check &check) = 0;
    virtual void savePunch(const core::TimePunch &punch) = 0;
    // `reports` holds the day's final reports (set when the day is closed).
    virtual void saveDay(const core::BusinessDay &, const QJsonObject &reports) { Q_UNUSED(reports) }
    virtual void saveDrawer(const core::DrawerSession &) {}
    virtual void saveSettings(const core::PosSettings &) {}
    virtual void saveMenuItem(const core::MenuItem &, int position) { Q_UNUSED(position) }
    virtual void deleteMenuItem(const std::string &id) { Q_UNUSED(id) }
    virtual void saveEmployee(const core::Employee &) {}
    virtual void saveCustomer(const core::CustomerRecord &) {}
    virtual void saveGiftCard(const core::GiftCard &) {}
    virtual void saveParty(const core::Party &) {}
    virtual void removeCustomer(const std::string &) {}
    // Saved, and no old copies left in the journal (privacy).
    virtual void purge() {}
    virtual void saveIngredient(const core::Ingredient &, int position) { Q_UNUSED(position) }
    virtual void deleteIngredient(const std::string &id) { Q_UNUSED(id) }
    virtual void saveShift(const core::Shift &) {}
    virtual void deletePunch(std::int64_t id) { Q_UNUSED(id) }
    virtual void deleteShift(std::int64_t id) { Q_UNUSED(id) }
    virtual void saveDelivery(const core::Delivery &) {}
    virtual void saveRefund(const core::Refund &) {}
    virtual void saveChange(const core::Change &) {}
    virtual void saveStockCount(const core::StockCount &) {}
    virtual void saveWaste(const core::Waste &) {}
    virtual void saveProblem(const core::Problem &) {}
    // The store's pictures (logo, buttons, backgrounds): name -> file bytes.
    virtual void saveImage(const std::string &name, const QByteArray &data) { Q_UNUSED(name) Q_UNUSED(data) }
    virtual void deleteImage(const std::string &name) { Q_UNUSED(name) }
};

// Where tickets go. Implementations must not block (see print::PrintSpooler).
class PosPrinter {
public:
    virtual ~PosPrinter() = default;
    // `voids`: the lines were cancelled after being sent.
    virtual void printKitchen(const core::PosSettings &settings, const core::Check &check,
                              const std::vector<core::OrderLine> &lines, bool voids) = 0;
    // `printerId`: the terminal's receipt printer (see PosSettings::receiptPrinterFor).
    virtual void printReceipt(const core::PosSettings &settings, const core::Check &check,
                              const std::string &printerId) = 0;
    virtual void printReport(const core::PosSettings &settings, const core::Report &report,
                             const std::string &printerId) = 0;
    virtual void openDrawer(const core::PosSettings &settings, const std::string &printerId) = 0;
    // A test page on printer `printerId` (its logo, text sizes, a cut), and
    // the drawer wired to it opened when `kickDrawer`. False: no such printer.
    virtual bool printTestPage(const core::PosSettings &, const std::string &, bool) { return false; }
    // Prep labels (title, then lines) on printer `printerId`: a label printer
    // in its own language, or a receipt printer as slips.
    virtual void printLabels(const core::PosSettings &, const std::vector<std::pair<std::string, std::vector<std::string>>> &,
                             const std::string &) {}
    // A self-order kiosk's slip for the guest: the order number to show at
    // the counter and what they ordered; `sent`: already being made.
    virtual void printOrderSlip(const core::PosSettings &settings, const core::Check &check,
                                const std::string &printerId, bool sent)
    {
        (void)sent;
        printReceipt(settings, check, printerId);
    }
};

// A closed business day and its final reports (report id -> report JSON).
struct PastDay {
    core::BusinessDay day;
    QJsonObject reports;
};

// The built-in user a self-order kiosk takes orders as (role "kiosk": it
// may only order). Not one of the staff: it can't log in with a PIN.
const core::Employee &kioskEmployee();
// The maintainer, logged in with the store's maintainer code: not on the
// staff list, so nobody in the store can remove or demote them.
const core::Employee &maintainerEmployee();

// Everything the running POS needs at startup.
struct PosData {
    core::PosSettings settings;
    std::vector<core::MenuItem> menu;
    std::vector<core::Employee> employees;
    std::vector<core::Check> openChecks;
    std::vector<core::TimePunch> punches;        // today's, plus any still open
    std::vector<core::TimePunch> earlierPunches; // finished, from the days before (a week or so)
    std::vector<core::CustomerRecord> customers;
    std::vector<core::GiftCard> giftCards;
    std::vector<core::Party> parties;
    std::int64_t lastPartyId = 0;
    std::vector<core::Ingredient> ingredients;
    std::vector<core::Shift> shifts;
    std::int64_t lastShiftId = 0;
    std::vector<core::Delivery> deliveries;       // the last 60 days
    std::int64_t lastDeliveryId = 0;
    std::map<std::string, QByteArray> images;     // the store's pictures, by name
    std::int64_t lastCheckId = 0;
    std::int64_t lastPunchId = 0;
    std::optional<core::BusinessDay> currentDay;  // none: the service opens one
    std::int64_t lastDayId = 0;
    std::vector<core::Check> closedToday;
    std::vector<core::DrawerSession> drawers;     // today's, plus any still open
    std::int64_t lastDrawerId = 0;
    std::vector<PastDay> pastDays;                // newest first
    std::vector<core::Refund> refundsToday;       // this business day's refunds (any check's)
    std::int64_t lastRefundId = 0;
    std::vector<core::Change> changesToday;       // this business day's changes (the Changes report)
    std::int64_t lastChangeId = 0;
    std::vector<core::StockCount> stockCounts;    // the latest ones, oldest first
    std::int64_t lastStockCountId = 0;
    std::vector<core::Waste> waste;               // the last 60 days, oldest first
    std::int64_t lastWasteId = 0;
    std::vector<core::Problem> problems;          // the last 7 days, oldest first
    std::int64_t lastProblemId = 0;
};

// State shared by every terminal of one store: open checks, today's closed
// checks, menu, staff, settings, the drawer and business day, and which
// terminal holds which check. Sessions change it and it tells all of them.
class PosShared : public QObject {
    Q_OBJECT

public:
    PosShared(PosData data, PosSink *sink, QObject *parent = nullptr);

    std::int64_t now() const { return now_(); }
    void setClock(std::function<std::int64_t()> now) { now_ = std::move(now); }

    // While the server makes every screen's update at once (nothing changes
    // meanwhile), what's the same for all of them (the kitchen's tickets, the
    // menu) is made once: `memo` keeps it until `endMemo`.
    void beginMemo() { memo_.emplace(); tipSharesMemo.reset(); }
    void endMemo() { memo_.reset(); tipSharesMemo.reset(); }
    bool memoOn() const { return memo_.has_value(); }
    // Closed checks with nothing left for the kitchen, or for the expediter:
    // skipped when making those screens (a busy night has a thousand). Done
    // stays done, except a recalled ticket, or a check reopened and closed
    // again (both forget it here).
    mutable std::unordered_set<qint64> kitchenDone, expoDone;
    void kitchenAgain(qint64 checkId) { kitchenDone.erase(checkId); expoDone.erase(checkId); }
    // Everyone's tip share, made once a round too (each server's screen shows theirs).
    mutable std::optional<std::map<std::string, core::TipShare>> tipSharesMemo;
    QVariant memo(const QString &key, const std::function<QVariant()> &make) const
    {
        if (!memo_)
            return make();
        if (const auto it = memo_->constFind(key); it != memo_->cend())
            return *it;
        return *memo_->insert(key, make());
    }

    const core::Employee *employee(const std::string &id) const;
    // The maintainer's code, on this store (empty: none). On the computer only.
    void setMaintainerPin(const QString &pin);
    bool isMaintainerPin(const QString &pin) const;

    core::PosSettings settings;
    std::vector<core::MenuItem> menu;
    // The Menu Builder's changes, to undo (last first): the menu, its
    // categories and choice groups before, and after (undone only if the
    // menu is still as that change left it).
    struct MenuState {
        std::vector<core::MenuItem> menu;
        std::vector<core::MenuCategory> categories;
        std::vector<core::ModifierGroup> groups;
        bool operator==(const MenuState &) const = default;
    };
    struct MenuUndo {
        QString label;
        MenuState before, after;
    };
    std::vector<MenuUndo> menuUndo;
    int menuUndoDepth = 0;   // inside a change (an import's many saves): one step
    std::vector<core::Employee> employees;
    std::map<std::int64_t, core::Check> open;
    std::vector<core::TimePunch> punches;   // today's, plus any still open
    std::vector<core::TimePunch> earlierPunches;   // finished, from the last days (weekly overtime)
    std::vector<core::CustomerRecord> customers;
    std::vector<core::GiftCard> giftCards;
    std::vector<core::Party> parties;   // waiting, booked, and today's
    std::int64_t lastPartyId = 0;
    std::vector<core::Ingredient> ingredients;
    core::Ingredient *ingredient(const std::string &id);
    // Closed checks in [from, to) from the database (set up by main; may run
    // on a worker thread). Unset: reports over a range use today's only.
    std::function<std::vector<core::Check>(std::int64_t from, std::int64_t to)> history;
    // Every check and party in the database that may be a customer's (forgetting them).
    std::function<std::pair<std::vector<core::Check>, std::vector<core::Party>>(const std::string &id,
                                                                                 const std::string &digits)> customerTrail;
    // Items ordered together over the past four weeks (from history, loaded
    // once in the background): checks with each item, and with each pair.
    std::map<std::string, int> pastChecksWith;
    std::map<std::string, std::map<std::string, int>> pastTogether;
    // ...and how many of each item each server ordered (employee id -> item).
    std::map<std::string, std::map<std::string, int>> pastByServer;
    // ...and for Rush Radar: items sent by weekday and 10 minutes of the day
    // (by item: its station is looked up when used), how many days of each
    // weekday that was, items per guest at tables, and how long tables take
    // to send their first order.
    struct RushPast {
        std::map<std::string, std::vector<int>> itemSlots;   // item -> 7 * 144 counts
        std::set<std::int64_t> days;                          // the dates seen (Julian day numbers)
        std::map<std::string, int> tableItems;
        std::int64_t tableGuests = 0;
        std::int64_t firstOrderMs = 0, firstOrders = 0;
        std::vector<int> checkHours = std::vector<int>(7 * 24, 0);   // checks opened, weekday x hour
        void count(const core::Check &c);
    };
    RushPast rushPast;
    bool pastTogetherAsked = false;
    // ...and today's, counted as checks close (not again for every screen).
    std::map<std::string, int> todayChecksWith;
    std::map<std::string, std::map<std::string, int>> todayTogether;
    size_t todayCounted = 0;
    std::int64_t todayLastId = 0;
    // Older time punches from the store (Time Punches reaches a month back).
    std::function<std::vector<core::TimePunch>(std::int64_t from, std::int64_t to)> punchHistory;
    std::vector<core::TimePunch> olderPunches;   // loaded from it for editing
    std::vector<core::Shift> shifts;   // from two weeks back on
    std::int64_t lastShiftId = 0;
    std::vector<core::Delivery> deliveries;
    std::int64_t lastDeliveryId = 0;
    // The store's pictures (Manager -> Pictures, or any picture field): kept in
    // the database, so backups, the standby and every screen have them.
    std::map<std::string, QByteArray> images;
    // The check each person had open when they logged out (switching to the
    // next person): open again when they log back in.
    std::map<std::string, std::int64_t> resumeChecks;
    // Where pictures are written out for this computer's screens to load
    // (main sets it under the app's data; else a temporary folder).
    QString imageCacheDir;
    // A picture as a file here: its name, or a path ("logo:" is the store's logo).
    QString imageFile(const QString &ref);
    // Texts a guest (set up by main when a texting service is configured).
    std::function<void(const QString &phone, const QString &message)> sendText;
    // Stripe, on the store's computer (net/stripe_api.cpp, set by main): a
    // card reader's connection token, and refunds. Each answers once,
    // with an error text when it didn't work.
    std::function<void(std::function<void(const QString &token, const QString &error)>)> stripeConnectionToken;
    // Emailed receipts waiting for Stripe (a card taken offline, the internet
    // down): one terminal's service sends them (PosService::sendWaitingReceipts).
    QObject *receiptSender = nullptr;
    std::map<std::int64_t, std::int64_t> receiptTried;   // check id -> when last tried (ms)
    std::function<void(const QString &paymentId, std::int64_t cents,
                       std::function<void(const QString &refundId, const QString &error)>)> stripeRefund;
    // Any Stripe call (countertop readers, pairing): "GET" / "POST", a path
    // ("/v1/terminal/readers/tmr_1"), a form ("a=1&b[c]=2").
    std::function<void(const QString &method, const QString &path, const QString &form,
                       std::function<void(const QJsonObject &reply, const QString &error)>)> stripeCall;
    core::CustomerRecord *customer(const std::string &id);
    core::GiftCard *giftCard(const std::string &number);
    std::int64_t lastCheckId = 0;
    std::int64_t lastPunchId = 0;
    PosSink *sink = nullptr;
    PosPrinter *printer = nullptr;

    core::BusinessDay day;
    std::int64_t lastDayId = 0;
    std::vector<core::Check> closedToday;
    std::vector<core::DrawerSession> drawers;   // drawers and server banks; today's plus open ones
    std::int64_t lastDrawerId = 0;
    std::vector<PastDay> pastDays;
    std::vector<core::Refund> refundsToday;
    std::int64_t lastRefundId = 0;
    std::vector<core::Change> changesToday;
    std::int64_t lastChangeId = 0;
    std::vector<core::StockCount> stockCounts;   // the latest ones, oldest first
    std::int64_t lastStockCountId = 0;
    std::vector<core::Waste> waste;              // the last 60 days, oldest first
    std::int64_t lastWasteId = 0;
    // What went wrong lately (Store Check), and how many of them the
    // managers have seen (problem ids up to settings.problemsSeen).
    std::vector<core::Problem> problems;
    std::int64_t lastProblemId = 0;
    // Kept, unless the same thing was noted in the last 10 minutes.
    void noteProblem(const std::string &kind, const std::string &subject, const std::string &detail = {});
    // Guessing PINs: wrong ones in a row on each screen, and the store's
    // lately. After 5 in a row a screen waits (30 seconds, doubling to 15
    // minutes); 30 in the store within 10 minutes and every screen waits a minute.
    struct PinGuard {
        int wrong = 0;
        std::int64_t waitUntil = 0;
    };
    QHash<QString, PinGuard> pinGuards;
    std::vector<std::int64_t> wrongPinsLately;
    // A newer ViewTouch on the release page ("0.9.1360"; empty: none known).
    QString newVersion;
    std::int64_t storeWaitUntil = 0;
    // Refunds sent to Stripe and not answered yet ("check/payment"), from any terminal.
    QSet<QString> refundsUnderway;
    // How much of a payment has gone back: on the check, and today's from any terminal.
    vt::Money refundedSoFar(const core::Check &check, std::int64_t paymentId) const;
    // A refund done: on the check (today's here, or `found`: one from an
    // earlier day, as it was read), saved, in today's refunds; every
    // terminal is told (refundRecorded). Doesn't need the terminal that
    // started it to still be there.
    void recordRefund(core::Refund refund, const core::Check &found, const QString &what);
    // Closed checks back to the first one (Find a Check): the newest first,
    // in [from, to) (to = 0: up to now), those whose saved record contains
    // any of `words` (none: all), `limit` from `offset`. Set by main (storage).
    struct CheckFind {
        std::int64_t from = 0, to = 0;
        QStringList words;
        int limit = 50, offset = 0;
    };
    std::function<std::vector<core::Check>(const CheckFind &)> findChecks;
    // Refunds made in [from, to), whatever day their check was (range reports).
    std::function<std::vector<core::Refund>(std::int64_t from, std::int64_t to)> refundHistory;

    // The terminal's open drawer / its most recent one today.
    core::DrawerSession *openDrawerFor(const std::string &terminal);
    const core::DrawerSession *latestDrawerFor(const std::string &terminal) const;
    // An employee's open server bank / their most recent one today.
    core::DrawerSession *openBankFor(const std::string &employeeId);
    const core::DrawerSession *latestBankFor(const std::string &employeeId) const;
    int adminRevision = 0;

    // Check locks: a check open on one terminal cannot be opened on another.
    std::map<std::int64_t, const QObject *> lockedBy;
    // Bumped kitchen tickets, newest last, for Recall.
    struct Bump { std::int64_t checkId; std::int64_t sentAt; std::string station; };
    std::vector<Bump> bumped;
    std::vector<Bump> served;   // expediter bumps, newest last
    // Messages between screens ("86 salmon", "need a runner"), newest last.
    struct Message { std::int64_t id; std::int64_t at; std::string from; std::string to; std::string text; };
    std::vector<Message> messages;
    std::int64_t lastMessageId = 0;

    // A device pairing a manager has started: the code the device must
    // type, until it is used or expires. One at a time.
    // standby: this code may pair the store's standby server (which gets a
    // full copy of the data); otherwise only screens.
    struct Pairing { QString code; std::int64_t expires = 0; bool standby = false; };
    std::optional<Pairing> pairing;
    const Pairing *activePairing() const
    {
        return pairing && pairing->expires > now() ? &*pairing : nullptr;
    }
    // Save the settings and tell every terminal (paired devices changed...).
    void saveSettings();
    // The kitchen's learned prep times: saved quietly, at most once a minute
    // (no screen needs telling; the tickets show them anyway). Any other
    // save of the settings saves them too.
    void savePrepTimes();
    std::int64_t prepSavedAt = 0;
    // Open a pairing (replacing any open one) and return its code.
    QString startPairing(bool standby = false);

    void startDay();

    // Backups (set up by main on the store server): take one now, and how
    // the last one went - {at, ok, error, copy, copyOk}.
    std::function<bool()> requestBackup;
    // A backup key for a password: {key, salt}, both empty when this
    // computer can't encrypt (the app sets it; see storage/sealed.hh).
    std::function<std::pair<QByteArray, QByteArray>(const QString &password)> backupKeyFor;
    // Factory reset from the Manager page: main backs up, closes, deletes the
    // database and starts again. False: not possible here.
    std::function<bool()> requestFactoryReset;
    // The store's computers and printers, for Manager -> Network (set by
    // the app): {role, term, terminals, standby, printers}.
    std::function<QVariantMap()> network;
    // The menu's categories: those set up, in order, then any family an item
    // names that isn't (older menus), named after it.
    std::vector<core::MenuCategory> categories() const;
    // What each printer says is wrong (print::PrinterMonitor): printer id ->
    // "paperOut", "coverOpen", "paperLow", "error", "offline"; none: fine.
    std::map<std::string, std::string> printerProblems;
    void setPrinterProblem(const std::string &printerId, const std::string &problem);
    QVariantMap backup;
    void setBackupStatus(QVariantMap status)
    {
        backup = std::move(status);
        emit dayChanged();
    }

signals:
    void checksChanged();    // any check: open, closed, made
    void refundRecorded(const core::Refund &refund, const QString &what);
    void dayChanged();
    void drawerChanged();
    void adminChanged();     // menu, settings, tenders, printers, taxes
    void staffChanged();
    void customersChanged();   // customers, gift cards, house accounts
    void networkChanged();     // a screen or the standby came or went; a printer worked or failed

private:
    std::function<std::int64_t()> now_;

private:
    mutable std::optional<QHash<QString, QVariant>> memo_;
};

// One terminal's POS session: who is logged in, the check being worked on,
// the keypad entry, and every order/payment operation, over the shared
// store. Pages reach it through zone actions and widgets (PosSession API);
// C++ callers and tests may use the typed methods directly.
class PosService : public PosSession {
    Q_OBJECT

public:
    // Single-terminal convenience: owns its own shared state.
    PosService(PosData data, PosSink *sink, QObject *parent = nullptr);
    // One of several terminals on a shared store.
    PosService(PosShared *shared, QString terminalName, QObject *parent = nullptr);
    ~PosService() override;

    PosShared *shared() const { return s_; }
    void setPrinter(PosPrinter *printer) { s_->printer = printer; }
    // Test hook: replace the wall clock (epoch ms) for the shared store.
    void setClock(std::function<std::int64_t()> now) { s_->setClock(std::move(now)); }

    void invoke(const QString &method, const QVariantList &args = {}, Reply reply = {}) override;

    const core::PosSettings &settings() const { return s_->settings; }
    const core::Check *currentCheck() const;
    const core::Employee *user() const;
    // Their name as shown (the maintainer's in this screen's language).
    QString nameOf(const core::Employee &e) const;
    const core::MenuItem *findItem(const QString &idOrName) const;
    // Barcodes typed in a field ("012345678905, 4006381333931"), and why
    // they can't be item `itemId`'s (empty: they can).
    static QStringList barcodeList(const QString &text);
    QString barcodeProblem(const QStringList &codes, const std::string &itemId) const;
    QString format(Money amount) const;

    // --- session -----------------------------------------------------------
    void pinKey(const QString &key);   // "0".."9", "clear", "back"
    bool login();                      // with the PIN entered
    bool loginWithPin(const QString &pin);
    void logout();
    bool clockIn();                    // logged-in user, else PIN entered
    // Someone with more than one job is asked which one first (clockInJobs),
    // then clocks in as it.
    bool clockInAs(const QString &role);
    // What the next pay out is for (Store Settings: expense categories);
    // touching the chosen one again clears it.
    void setExpenseCategory(const QString &category);
    // A delivery from a vendor (managers): {vendor, invoice, lines: [{ingredient,
    // qty, cost}]}. Adds to stock and sets each ingredient's cost per unit.
    bool receiveDelivery(const QVariantMap &delivery);
    // Find checks, open or closed, from the last `days`: "#123", "17.62",
    // or text (table, customer, phone, server, item, gift card). Results
    // arrive in checkSearch (loading until then).
    // Closed checks back to the first: `query` (empty: all of them) in the
    // last `days` (0: ever), a page from `offset` (checkSearch: nextOffset).
    bool searchChecks(const QString &query, int days = 365, int offset = 0);
    void selectFoundCheck(qint64 id);
    bool reprintCheck(qint64 id);   // a copy of a found check's receipt
    QVariantMap checkSearch() const override;
    QVariantMap receiving() const override;
    QVariantMap stockCounting() const override;
    // A stock count: each item counted becomes what's on hand, and the count
    // (against what should be there) is kept for the Stock Count report.
    bool saveStockCount(const QVariantMap &count);
    core::Report stockCountReport(const core::ReportContext &ctx) const;
    void stockExtras(core::Report &r, const core::ReportContext &ctx) const;
    QVariantMap wasteLog() const override;
    // Store Check: {checks: [{id, ok, info, title, detail}], problems: [{when, kind, text, new}], unseen, ready}.
    QVariantMap storeCheck() const override;
    bool problemsSeen();   // the problems listed now have been seen
    static QString printerProblemText(const QString &name, const std::string &what);
    QString problemText(const core::Problem &p) const;
    // Thrown away: a menu item (its recipe) or an ingredient, how much, why.
    bool logWaste(const QVariantMap &waste);
    // Prep labels: {ingredients: [{id, name, hours}], printer (its name; empty: none)}.
    QVariantMap prepLabels() const override;
    // {ingredient | name, hours (use by; 0: none), count, note}.
    bool printPrepLabels(const QVariantMap &labels);
    std::string labelPrinterId() const;
    const std::vector<core::Waste> &wasteEntries() const { return s_->waste; }
    const std::vector<core::StockCount> &stockCounts() const { return s_->stockCounts; }
    void cancelClockIn();
    QVariantMap clockInJobs() const override;
    QVariantMap cashTipsAsk() const override;
    QVariantMap reasonAsk() const override { return reasonAsk_; }
    // Answers reasonAsk: the void or comp goes ahead with it ("": it doesn't).
    bool giveReason(const QString &reason);
    // The cash tips kept on the shift just clocked out ("12.50"; "" or "0": none).
    bool declareCashTips(const QString &amount);
    QVariantMap timeClock() const override;
    bool timeClockStart(const QString &pin);
    bool timeClockAct(const QString &action);   // "in", "out", "break"
    // How long an item should take the kitchen (minutes): set, else learned; 0: unknown.
    int prepMinutesFor(const std::string &itemId) const;
    // Time off and shift swaps (pos_requests.cpp).
    bool timeClockRequestOff(const QString &date, const QString &reason);
    bool timeClockGiveAway(qint64 shiftId);
    bool timeClockTake(qint64 requestId);
    bool timeClockCancelRequest(qint64 requestId);
    void timeClockDone();
    bool clockOut();

    // --- keypads -------------------------------------------------------------
    void entryKey(const QString &key);   // digits, "00", "clear", "back"
    void adjustGuests(int delta);
    void textKey(const QString &key);    // characters, "space", "back", "clear"
    void clearEntry();

    // --- checks ---------------------------------------------------------------
    TableResult selectTable(const QString &label);
    bool startCheck(core::CheckType type);   // dine-in uses the pending table + guest entry
    bool openCheck(std::int64_t checkId);
    bool newTableCheck();
    // A bar tab under this name (empty: the name typed on the keyboard).
    bool openTab(const QString &name = {});
    void releaseCheck();

    bool addItem(const QString &idOrName);
    void setQualifier(const QString &qualifier);   // same one again clears it
    void selectLine(qint64 lineId) override;
    void selectPayment(qint64 paymentId) override;
    bool voidItem();
    // How many of an unsent line (1-99); 0: the selected line. A sent line
    // can't change: more of it is a new line (repeatLine).
    bool setLineQuantity(qint64 lineId, int quantity);
    bool changeLineQuantity(qint64 lineId, int by);
    // One more the same way (its choices too), as a new unsent line.
    bool repeatLine(qint64 lineId);
    // Puts back the item last taken off (or made fewer), for a little while.
    bool undoLast();
    // The drinks sent last (one Send), ordered again as new lines.
    bool anotherRound();
    std::vector<const core::OrderLine *> lastRound(const core::Check &c) const;
    bool sendOrder();
    // Orders for later: ready at `at` (epoch ms; 0 = as soon as possible).
    bool setDueAt(qint64 at);
    // Sends the orders for later whose time has come (the server calls it
    // every half minute); returns how many went.
    int fireDueOrders();
    // Kitchen stations: the store's, and the one this screen shows.
    QVariantList kitchenStations() const override;
    QString kitchenStation() const override;
    bool setKitchenStation(const QString &station);
    QString stationName(const std::string &id) const;
    // Event tickets: sold so far (all days), and left (-1: not an event).
    int ticketsSold(const core::MenuItem &item) const;
    int ticketsLeft(const core::MenuItem &item) const;
    bool knownStation(const std::string &id) const;
    std::string stationOf(const core::OrderLine &l) const;
    // Made where the expediter doesn't look (Store Settings -> Kitchen -> Expo doesn't show).
    bool expoSkips(const core::OrderLine &l) const;
    bool isPart(const core::OrderLine &l, const core::Modifier &m) const;
    bool allMade(const core::OrderLine &l) const;
    bool atStation(const core::OrderLine &l, const std::string &station) const;
    bool partAt(const core::OrderLine &l, const core::Modifier &m, const std::string &station) const;
    bool addComment();
    // Amount: explicit cents, else the keypad entry, else the balance due.
    bool tender(const QString &tenderId, std::optional<std::int64_t> amountCents = std::nullopt);
    bool removePayment();
    // A discount of the amount typed: dollars (cents typed) or a percent.
    bool customDiscount(bool percent);
    bool closeCheck();
    // The current check, else the last one closed on this terminal.
    bool printReceipt();
    bool noSale();   // open the cash drawer without a sale
    bool setCustomer(const QVariantMap &customer);

    // --- the menu while ordering (app/pos_menu.cpp) -------------------------------------
    // An item with modifier groups opens choosing: choose options, then
    // finish (required groups must be satisfied) or cancel (the item comes off).
    bool chooseOption(const QString &groupId, int index);
    // The same, as "no", "lite", "extra" or "side" (held down on the Choose page).
    bool chooseOptionAs(const QString &groupId, int index, const QString &qualifier);
    // Exactly how a choice is had: "off" (as it comes / not added), "" (an
    // add-on, plain), "no" (what comes on it), "lite", "extra", "side".
    bool setChoice(const QString &groupId, int index, const QString &how);
    bool finishChoosing();
    bool cancelChoosing();
    // Choose again for an unsent item already on the check.
    bool chooseLine(qint64 lineId);
    // "House Salad needs a Dressing..." for the first of `lines` missing a
    // required choice; empty when they are complete.
    QString missingChoice(const std::vector<core::OrderLine> &lines) const;
    // 86 / un-86 an item (anyone taking orders).
    bool setAvailable(const QString &itemId, bool available);
    std::string currentMealPeriod() const;

    // Start a break, or end the one going on (clocked-in staff).
    bool toggleBreak();

    // Rush ("rush") or VIP ("vip") on the current check, on or off.
    bool toggleFlag(const QString &flag);
    // A buyer's exemption certificate (number and who): the check is tax-free
    // ("" taxes it again). Managers.
    bool setTaxExempt(const QString &certificate);

    // --- seats and courses --------------------------------------------------------
    // The seat / course new items go to; also changes the selected line.
    bool setSeat(int seat);
    bool setCourse(int course);
    // Send the next course that is on hold.
    bool fireCourse();
    // Course pacing: fire the next held course in `minutes` (0: now; -1: never mind).
    bool fireCourseIn(int minutes);
    bool fireCourseOn(core::Check &c, bool paced);

    // --- managing checks (app/pos_checks.cpp) ----------------------------------------
    // On the current check. Servers may do this to their own checks,
    // managers to anyone's; each is kept in the check's history.
    bool transferCheck(const QString &employeeId);
    bool moveCheck(const QString &table);
    bool mergeCheck(qint64 otherId);   // the other open check comes onto this one
    bool reopenCheck(qint64 checkId);  // manager: a check closed today, made current

    // --- tips, gratuity, cash in and out ------------------------------------------
    // Tip on the selected card payment (else the last one): a percentage of
    // the check (bp, e.g. 1800) or, with percentBp 0, the keypad amount.
    bool addTip(std::int64_t percentBp);
    // The keypad amount as a cash tip for the check's server, handed to whoever
    // takes the payment (nothing typed: none).
    bool setCashTip();
    // An open check printed for the guests: noted once (the host stand's "nearly done").
    void notePrinted(std::int64_t checkId);
    // The customer display: ask the guest for a tip; their choice (a
    // percent, an amount in cents, or none) goes on the card payment, now
    // or when it is added.
    bool askForTip();
    bool customerTip(const QString &kind, std::int64_t value);   // "percent" | "amount" | "none"
    QVariantMap customerPrompt() const override;

    // --- loyalty and promotions (pos_loyalty.cpp) ---------------------------------
    // Spend reward `index` (PosSettings::rewards) of the check's customer.
    bool redeemReward(int index);
    // The guest types their phone on the customer display: found or signed up.
    bool customerJoin(const QString &phone);
    // After paying: "print", "text" (to a phone) or "none".
    bool sendReceipt(const QString &how, const QString &to = {});
    // A code a scanner read (it types it, then Enter), on whatever screen:
    // a coupon goes on the open check, a gift card is looked up ("giftCard";
    // its page shows it), else says nothing has it. Items: the screen rings
    // them in itself (findItem finds them by barcode).
    QString scanCode(const QString &code);
    // Names of the promotions running now (not coupons).
    QVariantList promotionsNow() const;
    // A coupon (a promotion with a code) typed or scanned onto the current
    // check: says why not when it's unknown, over, used up or covers nothing.
    bool applyCoupon(const QString &code);
    // --- a bar tab's card, held (pos_holds.cpp) ---------------------------------
    // The hold for the current check, for this screen's card reader (like
    // cardCharge, with hold: true): PosSettings::tabHold, set aside, not charged.
    QVariantMap holdCharge();
    // The reader's answer: the card held ({reference, processor, brand, last4, amountCents, checkId}).
    bool recordCardHold(const QVariantMap &r);
    // A reader beside the screen holds it (the store's computer runs it).
    bool startCounterHold();
    // Charges the held card for what's owed and a tip (a percent of the
    // check, else `tipCents`); more than is held: the bank is asked first.
    bool chargeHold(std::int64_t percentBp, std::int64_t tipCents);
    // Nothing charged: the card's hold is let go.
    bool releaseHold();
    QVariantMap holdInfo(const core::Check &c) const;
    // Takes coupon `id` (its promotion's) off the current check.
    bool removeCoupon(const QString &id);
    // The coupons on the current check: id, name, code, off, working.
    QVariantList checkCoupons() const;
    // Gratuity on the current check (bp; 0 removes). Removing an automatic
    // one needs a manager.
    bool setGratuity(std::int64_t percentBp);
    // Pay out of / into this terminal's drawer: keypad amount, typed reason.
    bool payout(core::CashMovement::Kind kind);
    // Pay the logged-in employee the tips they are owed, from this drawer.
    bool cashOutTips();

    // --- split check -------------------------------------------------------------
    // Move the selected line to another check (0 = a new one at the table).
    // Moves one of the chosen line's quantity to that check (0: a new one); all: the whole line.
    bool splitLine(qint64 targetCheckId, bool all = false);
    // The chosen item in even pieces on this check, to move to other checks.
    bool shareLine(int parts);
    // This check into `ways` checks of the same amount.
    bool splitEvenly(int ways);
    // The chosen shared item whole again, here (its pieces off the other checks).
    bool unshareLine();
    // A table's checks: one per seat (lines with no seat stay); every one's
    // receipt; all of them back into this one.
    bool splitBySeat();
    bool printTableChecks();
    // Manager -> Printers: a test page (and the drawer wired to it, opened).
    bool testPrinter(const QString &printerId, bool kickDrawer);
    bool combineTableChecks();
    QString checkFilter() const override { return checkFilter_; }
    void setCheckFilter(const QString &label) override;

    // --- kitchen display -----------------------------------------------------------
    // Mark the lines sent at `sentAt` on the check as made; with a station,
    // only the lines printed there (a bar screen leaves kitchen lines alone).
    bool bumpTicket(qint64 checkId, qint64 sentAt, const QString &station = {});
    // The expediter: every ticket across the stations until it's run out.
    QVariantList expoTickets() const override;
    bool expoBump(qint64 checkId, qint64 sentAt);   // out to the table (made, if a station forgot)
    bool expoRecall();
    bool recallTicket();   // undo the latest bump

    // --- drawer and business day ---------------------------------------------------
    // "My drawer" is this terminal's drawer, or with server banks the
    // logged-in employee's own bank.
    bool openDrawerSession();   // starting cash from the keypad entry
    bool countDrawer();         // counted cash from the keypad entry (bank: check out)
    // Manager: count someone else's drawer or bank (a server who left).
    bool countDrawerById(qint64 drawerId);
    // Counted bill by bill: how many of each of the store's bills and coins
    // (PosSettings::cashDenominations, in that order); drawerId 0: mine.
    bool countDrawerPieces(qint64 drawerId, const QVariantList &counts);
    // This person counts drawers blind (not seeing what should be there).
    bool blindCount() const;
    bool endOfDay();
    const core::BusinessDay &currentDay() const { return s_->day; }
    const std::vector<core::Check> &closedToday() const { return s_->closedToday; }

    // --- reports -------------------------------------------------------------------
    // id: sales | items | servers | labor | drawer. dayId 0 = today (live).
    QVariantMap report(const QString &id, qint64 dayId = 0) override;
    bool printReport(const QString &id, qint64 dayId = 0);
    core::Report buildReport(const QString &id) const;

    // --- admin (manager) -------------------------------------------------------------
    // panel: menu | employees | tenders | printers | taxes | store
    QVariantList adminFields(const QString &panel) override;
    QVariantList adminRecords(const QString &panel) override;
    QVariantMap adminNewRecord(const QString &panel) override;
    // index -1 adds a record. Returns false (with a notice) when invalid.
    bool adminSave(const QString &panel, int index, const QVariantMap &record);
    bool adminDelete(const QString &panel, int index);
    // Kept for the Changes report: who changed what, from what to what.
    void logChange(const QString &area, const QString &item, const QString &field, const QString &before,
                   const QString &after);
    const std::vector<core::Change> &changesToday() const { return s_->changesToday; }
    bool logPageEdits(const QStringList &added, const QStringList &removed, const QStringList &changed);
    const std::vector<core::MenuItem> &menu() const { return s_->menu; }
    const std::vector<core::Employee> &employees() const { return s_->employees; }

    // --- PosSession state ------------------------------------------------------------
    QString terminalName() const override { return terminal_; }
    bool loggedIn() const override { return user() != nullptr; }
    QString userName() const override;
    QString userRole() const override;
    QStringList permissions() const override;
    // What someone may do here: their job's permissions, their own changes, and
    // what the store lets their job do (hosts taking payments).
    std::set<std::string> permissionsOf(const core::Employee &e) const;
    bool clockedIn() const override;
    QString clockedInSince() const override;
    QString storeName() const override;
    QString currencySymbol() const override;
    QStringList customColors() const override;
    QVariantList priceChanges() const override;
    int pinLength() const override { return int(pin_.size()); }
    QString entry() const override { return entry_; }
    QString entryAmount() const override;
    int entryGuests() const override;
    QString textEntry() const override { return text_; }
    QString pendingQualifier() const override;
    QString pendingTable() const override { return pendingTable_; }
    bool hasCheck() const override { return currentCheck() != nullptr; }
    QVariantMap checkInfo() const override;
    QVariantList tableChecks() const override;
    QString undoText() const override;
    QVariantList lines() const override;
    QVariantMap totals() const override;
    QVariantList payments() const override;
    qint64 selectedLine() const override { return selectedLine_; }
    qint64 selectedPayment() const override { return selectedPayment_; }
    QVariantList openChecks() const override;
    QVariantList openChecksShared() const override;
    QVariantMap openChecksViewer() const override;
    QVariantList openChecksSharedNow() const;
    QVariantList customerResultsNow() const;
    QVariantMap floor() const override;
    QVariantList deliveries() const override;
    QVariantList drivers() const override;
    QStringList popularItems() const override;
    QStringList myUsuals() const override;
    QVariantMap rushRadar() const override;
    // Often ordered with what's on this check (a hint on the order screen): {id, name}.
    QVariantList goesWith(const core::Check &c) const;
    // Rush Radar: the items not made yet at each station (sent, not voided,
    // not on hold), and each newly sent line's line ahead of it.
    std::map<std::string, int> stationQueues(std::int64_t sentBefore = 0) const;
    void noteQueuesAtSend(core::Check &check);
    QVariantMap rushRadarNow() const;
    double typicalMinutes(const std::string &station) const;
    // Rush Radar's labor guidance (Dashboard): the next hour's checks at
    // today's pace, the servers that takes, against those on the clock now.
    QVariantMap staffingNow() const;
    // Minutes until this order's kitchen items would be made, from what's ahead
    // at each station it uses, at their pace (0: nothing for the kitchen).
    // paceKnown: every station has shown its pace (enough tickets).
    int kitchenMinutesFor(const core::Check &c, bool *paceKnown = nullptr) const;
    // The credit card surcharge in force (basis points; capped by the state), and what the guest is told.
    std::int64_t surchargeBp() const;
    QString surchargeNotice() const;
    // Dual pricing (PosSettings::cashDiscountBp): the check at its cash price
    // (none: off, already on, or something's paid); put on it when the cash
    // tendered pays all of it.
    std::optional<core::Check> atCashPrice(const core::Check &c) const;
    void offerCashDiscount(core::Check &c, Money tendered);
    static constexpr const char *kCashDiscountRef = "cashDiscount";
    // The past four weeks' checks, counted once in the background (goesWith, myUsuals).
    void askPastOnce() const;
    // Arranging the self-filling menu by touch (managers): one place earlier
    // or later within its family, and its button color.
    bool moveMenuItem(const QString &id, int by);
    bool setMenuItemColor(const QString &id, const QString &color);
    bool addCustomColor(const QString &color);   // kept for the store's color pickers
    QVariantMap stockLeft() const override;
    QVariantMap dashboard() const override;
    QVariantMap checklists() const override;
    bool tickChecklist(const QString &list, int index);
    core::Report checklistReport(const core::ReportContext &ctx) const;
    QVariantList closedChecks() const override;
    QVariantList staff() const override;
    QVariantList checkHistory() const override;
    QVariantMap choosingInfo() const override;
    // Items sold by weight: the one waiting for its weight (the Weigh page).
    QVariantMap weighingInfo() const override;
    // The setup guide (managers): what's set so far, and each step.
    QVariantMap setupInfo() const override;
    bool setupStore(const QString &name, const QString &receiptLines);
    bool setupLogo(const QString &ref, bool onReceipts);
    bool setupTaxes(double foodPercent, double alcoholPercent);
    // The store's ZIP code: its state and the rates there (the next step can change them).
    bool setupTaxArea(const QString &zip);
    bool setupAddItem(const QString &name, double price, const QString &family);
    bool setupAddEmployee(const QString &name, const QString &role, const QString &pin);
    bool setupRetireSamples();
    bool setupFinish(bool done = true);
    bool addWeighed();
    bool cancelWeighing();
    QString onBreakSince() const override;
    QStringList soldOut() const override;
    QVariantList menuItems() const override;
    QVariantList menuCategories() const override;
    QVariantList choiceGroups() const override;
    // The Menu Builder (pos_menu_builder.cpp).
    bool saveCategory(const QVariantMap &record);   // {id ("" new), name, color, periods, printer, station, taxClass}
    bool moveCategory(const QString &id, int by);
    bool moveCategoryTo(const QString &id, int position);   // dragged
    bool moveMenuItemTo(const QString &id, int position);   // among its category's items
    bool deleteCategory(const QString &id);          // only an empty one
    // {id ("" new), name, price, family, image, description, available, kioskHide,
    //  taxClass, printer, station, groups: [ids], onIt: [ingredients]}
    bool saveMenuItemCard(const QVariantMap &card);
    bool deleteMenuItemCard(const QString &id);
    bool setMenuPrices(const QVariantList &prices);   // [{id, price, ...}]: one Undo step
    // The same, at `at` (epoch ms, later than now); applied by itself then.
    bool schedulePrices(const QVariantList &prices, qint64 at, const QString &label);
    bool cancelPriceChange(qint64 id);
    // Those due: put on the menu (every so often, and at start).
    void applyDuePriceChanges();
    // Emailed receipts whose cards have all reached Stripe now: Stripe sends them.
    void sendWaitingReceipts();
    // Ask Stripe to email check `c`'s receipt (each card payment's); marks it sent when Stripe took it.
    void emailReceiptNow(std::int64_t checkId);
    bool setItemSizes(const QString &itemId, const QVariantList &sizes);   // [{name, price}]
    bool setCategoryColors(const QVariantMap &colors);   // {id: color}: one Undo step
    bool changeMenuItems(const QStringList &ids, const QVariantMap &changes);   // one Undo step
    bool removeMenuItems(const QStringList &ids);   // [{id, price}]: one Undo step
    bool duplicateMenuItem(const QString &id);   // "Fish Tacos 2", right after it
    // "Tacos: Carne Asada 3.50, Al Pastor 3.25" / one per line; returns how many were added.
    int addMenuItemsFromText(const QString &categoryId, const QString &text);
    // Rows read from a spreadsheet: [{name, price, category, description, onIt}].
    int importMenuRows(const QVariantList &rows, const QString &categoryId, bool updatePrices);
    // Starter menus (menu_templates.cpp): [{id, name, description, categories, items}].
    QVariantList menuTemplates() const override;
    // What would trip up service: [{text, item, group, category, serious}].
    QVariantList menuProblems() const override;
    QVariantMap menuExport();
    // The whole table's (seat 0) or one guest's.
    bool setAllergies(const QStringList &ids, int seat = 0);
    bool cardForwarded(const QVariantMap &r);
    QVariantList cardAlerts() const override;
    bool seeCardAlert(const QString &ref);
    void setStripeOffline(bool on);
    std::vector<core::Check *> checksToday();
    void saveAnyCheck(core::Check &c);
    // A line's allergens the guest is allergic to.
    static std::vector<std::string> allergyHits(const core::Check &c, const core::OrderLine &l);
    QString menuUndoText() const override;
    bool undoMenuChange();
    QString putMenuPicture(const QString &fileName, const QByteArray &data);
    int importMenuFile(const QVariantMap &file);
    bool applyMenuTemplate(const QString &id);
    // {id ("" new), name, min, max, askHow, options: [{name, price, included, kitchenName}]}
    bool saveChoiceGroup(const QVariantMap &record);
    bool deleteChoiceGroup(const QString &id);   // items stop asking for it
    QVariantList kitchenTickets() const override;
    // The same for every screen: made once per round of updates (PosShared::memo).
    QVariantList kitchenTicketsNow() const;
    QVariantList expoTicketsNow() const;
    QVariantMap dashboardNow() const;
    QVariantList menuItemsNow() const;
    QVariantList menuCategoriesNow() const;
    QVariantList choiceGroupsNow() const;
    QVariantList closedChecksNow() const;
    QVariantMap waitlistInfoNow() const;
    QVariantMap floorNow() const;
    QVariantMap stockLeftNow() const;
    QVariantMap dayInfoNow() const;
    QVariantList daysNow() const;
    QVariantMap checklistsNow() const;
    QStringList popularItemsNow() const;
    QVariantMap drawerInfo() const override;
    QVariantMap dayInfo() const override;
    QVariantList days() const override;
    int adminRevision() const override { return s_->adminRevision; }
    QString tipsOwed() const override;
    QVariantList mealPeriods() const override;
    QVariantMap pairingInfo() const override;
    QString screenMode() const override;
    QString terminalLook() const override;
    QString terminalKeyboard() const override;
    QString terminalStartCategory() const override;

    // --- pairing devices (manager) -------------------------------------------------
    // Start a pairing: a 10-character code, good for 10 minutes and one device.
    bool startPairing(bool standby = false);
    // Back the database up now (managers).
    bool backupNow();

    // --- messages between screens -------------------------------------------------
    // to: "all", "kitchen" (kitchen, bar and expo screens), "floor" (every
    // other screen) or a person's name. Needs no login (kitchen screens).
    // until > 0: posted, shown to everyone it's for until then (epoch ms).
    bool sendMessage(const QString &to, const QString &text, qint64 until = 0);
    bool removeMessage(const QString &id);
    QVariantList messages() const override;
    // Shift notes: the ones this manager hasn't read (newest first), leaving one, reading them.
    QVariantList handoff() const override;
    bool leaveHandoffNote(const QString &text);
    bool readHandoffNotes();
    QVariantMap networkInfo() const override;
    QVariantList printerAlerts() const override;
    QString language() const override;
    QVariantMap userPrefs() const override;
    QString storeLanguage() const override { return QString::fromStdString(s_->settings.language); }

    // --- manager approval ---------------------------------------------------------
    // A manager's PIN lets the waiting operation (a void, a discount...) through once.
    bool approve(const QString &pin);
    bool cancelApproval();
    QVariantMap approvalInfo() const override { return approval_; }
    int autoLogoutMinutes() const override { return s_->settings.autoLogoutMinutes; }
    int phoneLockSeconds() const override { return s_->settings.phoneLockSeconds; }
    int screenSaverMinutes() const override { return s_->settings.screenSaverMinutes; }

    // --- practice (training) --------------------------------------------------------
    // This screen's checks are practice: the person is in training, or a
    // manager switched it on here.
    bool training() const override;
    bool setTraining(bool on);

    // Self-order kiosk: this screen is for guests ordering on their own (no
    // staff logged in; the built-in "Self-order kiosk" user takes the
    // orders). A manager turns it on here (setSelfOrder), the terminal's
    // setting does at connect (enableSelfOrder), and a manager's PIN turns
    // it off (leaveSelfOrder).
    bool setSelfOrder(bool on);
    void enableSelfOrder();
    bool leaveSelfOrder(const QString &managerPin);
    bool selfOrder() const { return selfOrder_; }
    // A guest's order: start (for here / to go), add and remove items,
    // finish with the name to call it by (it waits for the counter, or goes
    // to the kitchen at once), or cancel.
    bool kioskStart(bool toGo);
    bool kioskAdd(const QString &itemId);
    bool kioskRemove(qint64 lineId);
    bool kioskFinish(const QVariantMap &guest);
    void kioskCancel();
    QVariantMap selfOrderInfo() const override;
    QVariantMap kioskMenu() const override;
    // A picture the store uses (a menu item's photo, the display logo or a
    // slide), base64, for screens on other computers; empty for anything else.
    QString storeImage(const QString &path) const;
    QString imageUrl(const QString &ref) const override;
    QVariantList storeImages() const override;
    QString storeLogo() const override { return QString::fromStdString(s_->settings.displayLogo); }
    bool addStoreImage(const QString &fileName, const QString &base64);
    bool removeStoreImage(const QString &ref);
    // Everything back to a fresh install, after a backup (managers; `confirm`
    // must be "RESET"). ViewTouch restarts with the starter set.
    bool factoryReset(const QString &confirm);

    // --- customers, gift cards, house accounts (pos_customers.cpp) ---------------
    // Search by phone digits or name ("" = the most recent).
    bool findCustomers(const QString &query);
    bool selectCustomer(const QString &id);
    // Put a customer (default: the selected one) on the open check.
    bool useCustomer(const QString &id = {});
    // Add or update (by id, else the same phone number); goes on the open check.
    bool saveCustomer(const QVariantMap &record);
    // A customer asked to be forgotten (privacy laws): their record goes, and
    // their name, phone, address, email and note come off every check and
    // waitlist entry (the sales stay, with no one's name on them).
    bool forgetCustomer(const QString &id);
    // Sell or reload a card on the check (a Quick check if none is open);
    // no number: a new one. Amount 0: the keypad. Live once the check is paid.
    bool sellGiftCard(const QString &number, qint64 amountCents = 0);
    bool lookupGiftCard(const QString &number);
    // Pay from a card (default: the looked-up one); 0 = keypad, else as much as it covers.
    bool payWithGiftCard(const QString &number = {}, qint64 amountCents = 0);
    // The looked-up card's small balance in cash, when the guest asks (state
    // law: California under $10); paid from the drawer (or the person's bank).
    bool cashOutGiftCard();
    // A payment on the selected customer's account: "cash" | "card" (keypad amount, else all).
    bool payOnAccount(const QString &method, qint64 amountCents = 0);
    QVariantList customerResults() const override;
    QVariantMap customerInfo() const override;
    QVariantMap giftCardInfo() const override;

    // --- the host stand (pos_waitlist.cpp) ----------------------------------------
    // {name, phone, size, quote?, note, customerId?} -> the party's id (0: refused).
    qint64 addToWaitlist(const QVariantMap &party) { return addParty(party, false); }
    // As above with at: epoch ms, or "yyyy-MM-dd HH:mm".
    qint64 addReservation(const QVariantMap &party) { return addParty(party, true); }
    bool updateParty(qint64 id, const QVariantMap &changes);
    bool checkInParty(qint64 id);      // a reservation has arrived: into the line
    bool notifyParty(qint64 id);       // "your table is ready" (texted when set up)
    // Seat them: opens their table's check for `serverId` (default: you).
    bool seatParty(qint64 id, const QString &table, const QString &serverId = {});
    // The guest's receipt (pos_receipts.cpp): printed where chosen, after
    // paying by itself or offered, emailed by Stripe for a Stripe card.
    QVariantMap receiptOffer() const override { return receiptOffer_; }
    bool printReceiptOn(qint64 checkId, const QString &printerId);
    bool emailReceipt(qint64 checkId, const QString &email);
    void noReceipt();
    QVariantList receiptPrinters() const;
    // Refunds on closed checks, today's or found (pos_refunds.cpp): a
    // manager's; `cents` 0: all that's left of that payment.
    bool refundPayment(qint64 checkId, qint64 paymentId, qint64 cents, const QString &reason);
    const std::vector<core::Refund> &refundsToday() const { return s_->refundsToday; }
    // Cards on a reader (pos_cards.cpp).
    QString terminalCardReader() const override;
    QVariantMap readerToken() const override { return readerToken_; }
    QVariantMap cardCharge(const QString &tenderId);   // {ok, amountCents, tipCents, currency...} or {ok: false}
    bool recordCardPayment(const QVariantMap &result);
    bool addCardPayment(const QVariantMap &result);   // recordCardPayment, without asking who
    void requestReaderToken();
    // A Stripe reader beside this screen, run from the store's computer.
    QVariantMap counterCharge() const override { return counter_; }
    bool startCounterCharge(const QString &tenderId);
    bool cancelCounterCharge();
    // Stripe test mode: a card "tapped" on the reader: "credit", "debit" or "declined".
    bool presentTestCard(const QString &card);
    bool presentTestCard(bool decline) { return presentTestCard(decline ? QStringLiteral("declined") : QStringLiteral("credit")); }
    void pairCounterReader(const QVariantMap &record);
    QString counterReaderId() const;
    QString counterReaderLabel(const QString &id) const;
    enum class RefundStart { NotNeeded, Started, CantNow };
    RefundStart refundCardPayment(const core::Check &c, const core::Payment &p);
    // Phone orders and deliveries (pos_phone_orders.cpp).
    bool sameAsLastTime();
    bool sendOut(const QVariantList &checkIds, const QString &driverId);
    bool deliveryBack(qint64 checkId);
    bool nameRequired() const;
    QString missingWho(const core::Check &c) const;   // empty: it may go to the kitchen
    int readyQuote(const core::Check &c) const;        // minutes, from how busy the kitchen is
    void applyDeliveryFee(core::Check &c);
    void rememberOrder(core::CustomerRecord &r, const core::Check &c);
    QString lastOrderText(const core::Check &c) const;
    // A regular on file: "Regular · 7 visits", and their note ("allergic to nuts").
    QString regularText(const std::string &customerId) const;
    // The host stand (pos_waitlist.cpp).
    bool seatPartyAt(qint64 partyId, const QStringList &tables, const QString &serverId);
    bool seatWalkIn(int size, const QStringList &tables, const QString &serverId);
    bool reserveTables(qint64 partyId, const QStringList &tables);
    bool setTableState(const QString &table, const QString &state);
    bool tableTaken(const QString &table) const;   // an open table check is on it
    void tableEmptied(const std::string &table);   // its last check closed: dirty
    // Left the line, or (a reservation) never came.
    bool partyGone(qint64 id, bool noShow = false);
    QVariantMap waitlistInfo() const override;

    // --- inventory (pos_inventory.cpp) ------------------------------------------
    // What a line uses up (ingredient id -> amount).
    std::map<std::string, double> stockUse(const core::OrderLine &line) const;
    // Ingredients at or below their low mark.
    QVariantList lowStock() const;
    core::Report foodCostReport(const std::vector<core::Check> &closed, const core::ReportContext &ctx) const;
    core::Report menuEngineeringReport(const std::vector<core::Check> &closed, const core::ReportContext &ctx) const;
    // What a line's recipe costs (its ingredients' cost; none without a recipe).
    Money lineCost(const core::OrderLine &l) const;
    // A menu item's recipe at today's costs, and the items over the store's
    // food cost target: {name, price, cost, percent, suggested} (most over first).
    Money recipeCost(const core::MenuItem &m) const;
    QVariantList overFoodCost() const;
    // Deliveries received today: by vendor, then each one.
    core::Report purchasesReport(const core::ReportContext &ctx) const;

    // --- the schedule (pos_schedule.cpp) ----------------------------------------
    // {employeeId, start, end (ms or "yyyy-MM-dd HH:mm"; an end before the
    // start runs past midnight), note}. Managers.
    bool addShift(const QVariantMap &shift);
    bool removeShift(qint64 id);
    // A manager clocks someone in (off the schedule, or forgot).
    bool clockInEmployee(const QString &employeeId);
    // The week the schedule shows: 0 this week, 1 next...
    bool setScheduleWeek(int offset);
    QVariantMap scheduleInfo() const override;
    QString nextShift() const override;

    // --- reports over a range of days (pos_day.cpp) -------------------------------
    // period: week | lastWeek | month | lastMonth | year | custom (from / to:
    // yyyy-MM-dd); compare: beside the same days a year before. The answer
    // comes in rangeReport ({loading, report}) when the checks are read.
    bool requestRangeReport(const QString &id, const QString &period, const QString &from = {},
                            const QString &to = {}, bool compare = false);
    QVariantMap rangeReport() const override { return rangeReport_; }
    // A coming day (0 today ... 6), from the same weekday in past weeks
    // (pos_forecast.cpp); the answer comes in rangeReport ({id: forecast, report}).
    bool requestForecast(int daysAhead);
    core::Report forecastReport(QDate target, const std::vector<core::Check> &checks, const core::ReportContext &ctx) const;
    QVariantMap reportVariant(const core::Report &r) const;   // as the screen shows it

    // Tips after tip-outs and pools (see core::tipShares), everyone / one person.
    std::map<std::string, core::TipShare> allTipShares() const;
    std::map<std::string, core::TipShare> allTipSharesNow() const;
    core::TipShare tipShareFor(const std::string &employeeId) const;
    bool stopPairing();

private:
    // Why the last PIN was refused when it's because of guessing (wait), else `usual`.
    QString pinRefused(const QString &usual) const { return pinWait_.isEmpty() ? usual : pinWait_; }
    QString pinWait_;

    // One Menu Builder change, as one step to undo (PosShared::menuUndo).
    class MenuStep {
    public:
        MenuStep(PosService *pos, QString label);
        ~MenuStep();
        MenuStep(const MenuStep &) = delete;
        MenuStep &operator=(const MenuStep &) = delete;
    private:
        PosService *pos_;
        QString label_;
        bool outer_;
        PosShared::MenuState before_;
    };
    PosShared::MenuState menuState() const;

    // Stock out (sign 1) or back (-1) for these lines; then sold-out marks.
    void takeStock(const std::vector<core::OrderLine> &lines, int sign);
    // Sold out by itself when an ingredient runs short; back when restocked.
    void refreshSoldOut();
    // The shift `employeeId` may clock in for now (see clockInEarlyMinutes).
    const core::Shift *shiftNow(const std::string &employeeId) const;
    // Why `e` can't clock in now ("" = they can).
    QString scheduleCheck(const core::Employee &e) const;
    double shortRest(const core::Shift &s) const;
    QString scheduleChangeNote(const core::Shift &s, bool added);
    int scheduleWeek_ = 0;
    QVariantMap rangeReport_;
    int rangeRequest_ = 0;
    core::Report forecast_;   // the forecast last shown (Print)
    QVariantMap checkSearch_;
    std::vector<core::Check> searchHits_;
    qint64 selectedHit_ = 0;
    int searchRequest_ = 0;
    // A report that can cover several days, over `closed`.
    // `refunds`: made in the range (by date); none: those on these checks.
    // Today's house account payments (from the customers' accounts).
    // Where the store is (Store Settings -> Taxes; pos_tax_area.cpp).
    QVariantList stateOptions() const;
    TaxSuggestion taxHere() const;
    QVariantList taxWhereOptions() const;
    QString taxAreaText(const TaxSuggestion &s) const;
    bool setStoreState(const std::string &state, bool *applied = nullptr);
    std::vector<core::AccountPayment> accountPaymentsToday() const;
    core::Report rangeCapableReport(const QString &id, const std::vector<core::Check> &closed,
                                    const core::ReportContext &ctx, const std::vector<core::Refund> *refunds = nullptr,
                                    const core::DayMoney *day = nullptr,
                                    const std::vector<core::TimePunch> *punches = nullptr) const;
    qint64 addParty(const QVariantMap &r, bool reservation);
    core::Party *party(qint64 id);
    void saveParty(const core::Party &p);
    int quoteFor(int ahead) const;
    QVariantMap promotionRecord(const core::PosSettings::Promotion &p) const;
    bool savePromotionRecord(int index, const QVariantMap &record);
    QVariantMap customerSummary(const core::CustomerRecord &c) const;
    void saveCustomerRecord(const core::CustomerRecord &c);
    void saveGiftCardRecord(const core::GiftCard &g);
    void rememberCustomer(core::Check &c);
    const core::Tender &tenderOfKind(core::TenderKind kind, const char *id, const QString &name) const;
    bool chargeHouseAccount(core::Check &c, const core::Tender &t, Money amount);
    // A removed gift card / house account payment goes back where it came from.
    void returnPayment(const core::Check &c, const core::Payment &p);
    // Closing: gift cards sold go live, the customer's visit counts.
    void applyCloseEffects(core::Check &c);
    QString reopenBlocked(const core::Check &c) const;
    void undoCloseEffects(core::Check &c);
    Money promotionAmount(const core::PosSettings::Promotion &p, const core::Check &c) const;
    void applyPromotions(core::Check &c);
    // A closed check's coupons count as used (by 1), or not anymore (-1: reopened).
    void countCouponUses(const core::Check &c, int by);
    bool addCardHold(const QVariantMap &r);
    void letHoldGo(core::Check &c, const QString &why);
    bool startCounter(const QVariantMap &charge);
    QSet<qint64> holdCharging_;   // checks whose held card is being charged
    int pointsFor(const core::Check &c) const;
    void earnPoints(core::Check &c);
    void takeBackPoints(core::Check &c);
    // The tip the guest chose on the customer display, until a card takes it.
    struct TipChoice { bool asked = false; bool chosen = false; bool none = false; std::int64_t percentBp = 0;
                       Money amount; std::int64_t checkId = 0; };
    TipChoice tipChoice_;
    Money tipFor(const core::Check &c) const;
    QString customerQuery_;
    std::string selectedCustomer_;
    QString giftCardNumber_;
    std::string receiptPrinter() const { return s_->settings.receiptPrinterFor(terminal_.toStdString()); }
    // Cash handling for the logged-in person: their own choice, else the store's.
    bool serverBank() const;
    bool terminalHasDrawer() const { return s_->settings.hasDrawer(terminal_.toStdString()); }
    // Why cash can't go in this terminal's drawer (none, or not started).
    QString noDrawerMessage() const;
    // Whether the logged-in person must close their checks before checking out.
    bool mustCloseChecksToCheckOut() const;
    core::DrawerSession *myDrawer();
    const core::DrawerSession *latestMyDrawer() const;
    // Server banks: my open bank, started at `start` if I have none.
    core::DrawerSession *ensureMyBank(Money start = {});
    bool closeDrawer(core::DrawerSession &d);   // counted = keypad entry
    Money expectedNow(const core::DrawerSession &d) const;
    void connectShared();
    core::Check *current();
    // A new, empty check at the same table as check `fromId` (same server, type).
    // Sizes typed in ([{name, price}]): checked (empty rows dropped), or
    // nullopt having said what's wrong.
    std::optional<std::vector<std::pair<QString, qint64>>> sizeList(const QVariantList &sizes);
    core::Check &splitOff(qint64 fromId);
    // `noteOn`: the check the approval is noted on (else the open one).
    bool require(const char *permission, const QString &action, core::Check *noteOn = nullptr);
    // The operation running through invoke(), to try again once approved.
    struct Running { QString method; QVariantList args; };
    std::optional<Running> running_;
    // Waiting for a manager's PIN: {needed, action, permission}; and the
    // one-time approval that lets the operation through.
    QVariantMap approval_;
    // Why: a void or comp made on a screen waits for its reason (reasonAsk_),
    // then runs again with it (reason_, for that operation only).
    struct GivenReason { QString method; std::string reason; };
    std::optional<GivenReason> reason_;
    QVariantMap reasonAsk_;
    QString reasonMethod_;
    QVariantList reasonArgs_;
    // Whether this can go ahead: no reason needed, or given. Else asks (false).
    bool haveReason(const char *kind, const QString &what);
    std::string takeReason();
    QVariantList approvalArgs_;
    QString approvalMethod_;
    struct Approved { std::string permission; std::string by; };
    std::optional<Approved> approved_;
    bool trainingOn_ = false;
    bool selfOrder_ = false;
    bool kioskToGo_ = false;
    QVariantMap lastKioskOrder_;   // the confirmation the guest sees
    bool closePractice(core::Check &c);   // a manager switched this screen to practice
    bool fail(const QString &message);
    void changed(core::Check &check);   // persist + notify
    const core::Employee *employeeByPin(const QString &pin) const;
    QString ownerRule(const core::Employee *before, const core::Employee &after) const;
    QVariantList adminFieldsAll(const QString &panel);
    const char *fieldPermission(const QString &panel, const QString &path) const;
    core::TimePunch *openPunch(const std::string &employeeId);
    // Start a shift: the job and its pay recorded on the punch.
    bool punchIn(const core::Employee &e, const core::Job &job, const QString &by = {});
    std::string jobChoice_;   // waiting for this person to pick a job
    std::int64_t cashTipsPunch_ = 0;   // waiting for the cash tips kept on this shift
    std::string clockWho_;
    // The dashboard's "same day last week, by this time" (read from the store; kept a few minutes).
    mutable std::int64_t lastWeekAt_ = 0;
    mutable Money lastWeekNet_;
    mutable int lastWeekChecks_ = -1;    // the Time Clock screen: whose PIN was typed (not logged in)
    bool clockOutFor(const core::Employee &e);
    // Hours this pay week and today, and how long until overtime (daily or
    // weekly rule, whichever comes first): {weekHours, todayHours, leftMinutes, state ok|soon|over}.
    QVariantMap overtimeFor(const std::string &employeeId) const;
    // Time off and shift swaps (pos_requests.cpp).
    core::PosSettings::StaffRequest *staffRequest(std::int64_t id);
    const core::Shift *shiftById(std::int64_t id) const;
    QString requestText(const core::PosSettings::StaffRequest &r) const;
    QString requestStatusText(const core::PosSettings::StaffRequest &r) const;
    void addStaffRequest(core::PosSettings::StaffRequest r);
    QVariantMap requestsFor(const std::string &employeeId) const;
    std::vector<core::PosSettings::StaffRequest *> requestList();
    QVariantList requestFields();
    QVariantList requestRecords();
    bool saveRequestRecord(int index, const QVariantMap &record);
    int requestsWaiting() const;
    // Manager -> Time Punches: the last week's, newest first; change, add, remove.
    std::vector<core::TimePunch *> punchList();
    QVariantList punchFields();
    QVariantList punchRecords();
    QVariantMap punchNewRecord();
    bool savePunchRecord(int index, const QVariantMap &record);
    bool deletePunchRecord(int index, const QString &why);
    // End of Day: a manager clocks out someone still on the clock (logged as a change).
    bool clockOutPunch(qint64 punchId);
    void logPunchChange(const core::TimePunch &p, const QString &what, const QString &why);
    bool toggleBreakFor(const core::Employee &e);
    QString expenseCategory_;
    std::int64_t now() const { return s_->now(); }
    bool lockCheck(std::int64_t checkId);   // false: open on another terminal
    void unlockCheck(std::int64_t checkId);
    QString lockHolder(std::int64_t checkId) const;
    QString dueText(std::int64_t at) const;                 // "6:30 PM", "tomorrow 6:30 PM"
    bool waitingForLater(const core::Check &c) const;       // not the kitchen's yet
    bool forAnotherDay(const core::Check &c) const;
    bool saveAdminRecord(const QString &panel, int index, const QVariantMap &record);
    bool deleteAdminRecord(const QString &panel, int index);
    // A manager screen's record saved: each field changed (records before the save).
    void logRecordChanges(const QString &panel, int index, const QVariantList &before, const QVariantMap &saved);
    void logMenuChanges(const PosShared::MenuState &before, const PosShared::MenuState &after, const QString &step);
    void noteEvent(core::Check &c, const QString &what, const char *kind, Money amount = {}, const std::string &reason = {});
    bool mayManage(const core::Check &c, const QString &action);
    bool mayOpen(const core::Check &c) const;
    QStringList onItOf(const core::MenuItem &m) const;
    // Choices priced by size: the size on a line, an option's price on it,
    // and its choices' prices brought in line with it.
    std::string sizeOf(const core::OrderLine &l) const;
    Money optionPrice(const core::ModifierOption &o, const core::OrderLine &l) const;
    void repriceForSize(core::OrderLine &l) const;
    QVariantList sizesOf(const core::MenuItem &m) const;
    core::ReportContext reportContext(const QString &period) const;
    QString dayLabel(const core::BusinessDay &day) const;
    // A day's reports' period: its date and hours ("Wednesday, October 14,
    // 2026, 10:31 AM to 2:05 AM"; "since" while it's open).
    QString dayPeriod(const core::BusinessDay &day) const;
    // A closed day's saved report (one saved saying "Today" gets its date).
    core::Report pastReport(const core::BusinessDay &day, const QJsonObject &stored) const;
    bool saveMenuRecord(int index, const QVariantMap &record);
    bool saveEmployeeRecord(int index, const QVariantMap &record);
    bool saveTenderRecord(int index, const QVariantMap &record);
    bool saveMealPeriodRecord(int index, const QVariantMap &record);
    bool saveModifierGroupRecord(int index, const QVariantMap &record);
    QVariantMap menuRecord(const core::MenuItem &m) const;
    QStringList groupIds() const;
    QStringList periodIds() const;
    // The chart of accounts' keys, named: sales, sales:<family>, tender:<id>...
    QVariantList accountKeys() const;
    // Store Settings -> About: the store's program, where its data is, its
    // backups ("about.*", read only).
    QVariantMap aboutValues() const;
    bool savePrinterRecord(int index, const QVariantMap &record);
    void settingsChanged();
    void menuChanged();   // the menu changed: sold out again, every screen's menu

    PosShared *s_;
    std::unique_ptr<PosShared> owned_;
    QString terminal_;

    std::string userId_;
    std::int64_t currentId_ = 0;
    int seat_ = 0;      // seat for new items (0: none)
    int course_ = 1;    // course for new items
    bool lineTouched_ = false;   // the selected line was touched (not just added)
    std::int64_t choosingLine_ = 0;   // the line whose modifiers are being chosen
    // An item already on the check (Choose): as it was, for Cancel Changes;
    // empty: just added (Cancel Item takes it off).
    std::optional<core::OrderLine> choosingBefore_;
    QString weighing_;                // an item sold by weight, waiting for its weight
    bool retireMeAtFinish_ = false;   // setup guide: this sample manager goes off at Finish
    qint64 selectedLine_ = 0;
    QVariantMap readerToken_;          // the latest connection token for this terminal's reader
    int readerTokenSeq_ = 0;
    std::set<std::int64_t> refunding_; // card payments whose refund is on its way
    core::Check *closedCheckFor(qint64 checkId);
    // The card a countertop reader is taking: {status: starting | waiting,
    // reader, readerLabel, paymentIntent, amount, test...}; empty when none.
    QVariantMap counter_;
    QVariantMap receiptOffer_;   // {checkId, label, total, printers, choosePrinter, canEmail, email}; empty: none
    const core::TerminalConfig *terminalConfig() const;
    bool askReceiptPrinter() const;
    const core::Check *checkForReceipt(qint64 checkId) const;
    void offerReceipt(const core::Check &c, bool choosePrinter);
    void receiptAfterClosing(const core::Check &c);
    QTimer counterPoll_;
    QTimer priceClock_;   // price changes set for later
    bool applyPrices(const QVariantList &prices, const QString &label);
    int counterPolls_ = 0;
    bool counterAsking_ = false;
    void pollCounter();
    void confirmCounter(const QString &paymentIntent, const QJsonValue &paymentMethod);
    bool counterHeir_ = false;   // watching a card for a terminal that went away
    void adoptCounterCharge(const QVariantMap &state);
    void counterPaid(const QString &paymentIntent);
    void counterDone(const QString &message);
    void stripe(const QString &method, const QString &path, const QString &form,
                std::function<void(const QJsonObject &, const QString &)> done);
    // The last item taken off (or made fewer) on this terminal's check, for Undo.
    struct LastChange {
        std::int64_t checkId = 0;
        core::OrderLine before;   // the line as it was
        std::size_t index = 0;    // where it was on the check
        bool removed = false;
        std::int64_t at = 0;
        QString text;
    };
    std::optional<LastChange> lastChange_;
    void rememberChange(const core::Check &c, const core::OrderLine &before, bool removed, const QString &text);
    qint64 selectedPayment_ = 0;
    core::Qualifier qualifier_ = core::Qualifier::None;
    std::int64_t lastClosedId_ = 0;
    QString checkFilter_;
    QString pin_;
    QString entry_;
    std::vector<std::pair<std::int64_t, int>> countedPieces_;   // countDrawerPieces -> closeDrawer
    QString text_;
    QString pendingTable_;
};

} // namespace vt::app
