#pragma once

#include "core/check.hh"
#include "core/inventory.hh"
#include "core/tax.hh"

#include <string>
#include <vector>

namespace vt::core {

// A ticket printer. Menu items name the printer their kitchen tickets go to
// ("kitchen", "bar"); receipts go to "receipt".
struct PrinterConfig {
    std::string id;
    std::string name;
    std::string type = "none";   // network | file | cups | none
    std::string host;            // network
    int port = 9100;             // network (raw / JetDirect)
    std::string path;            // file path, or CUPS queue name
    std::string format;          // escpos | text; empty = escpos for network, else text
    int width = 42;              // characters per line
    bool cutter = true;
    bool drawerKick = false;     // cash drawer is wired to this printer
    bool receipts = false;       // offered when a screen asks where a receipt goes
    // ESC/POS letters: empty = PC858 (accents, ñ, £ €; Epson and most), "ascii" = plain letters.
    std::string charset;
    // Network ESC/POS: every screen says when it's out of paper, its cover
    // is open, or it stops answering.
    bool watch = true;
    // It has answered a status question (learned; kept across restarts):
    // when it then goes silent, something's wrong with it. Some printers
    // never answer; they're not warned about.
    bool reportsStatus = false;
    // Label printers (format zpl or tspl): the label's size.
    int labelWidthMm = 51;
    int labelHeightMm = 25;

    std::string effectiveFormat() const { return !format.empty() ? format : type == "network" ? "escpos" : "text"; }
    bool operator==(const PrinterConfig &) const = default;
};

// Per-terminal setup: which printer takes its receipts (and opens its
// drawer). Terminals not listed use the "receipt" printer.
// A place in the kitchen with its own screen: Grill, Fryer, Cold Line.
struct Station {
    std::string id;
    std::string name;

    bool operator==(const Station &) const = default;
};

struct TerminalConfig {
    std::string name;
    std::string receiptPrinter;
    std::string drawer;   // "yes" / "no": has a cash drawer; empty = the store's setting
    // A paired device (tablet, remote terminal): the id it connects as and
    // its secret key (base64). Empty for terminals set up by name only.
    std::string id;
    std::string key;
    std::int64_t pairedAt = 0;
    // Pages to show: "phone" (phone versions), "standard", "selfOrder" (a
    // self-order kiosk for guests), or empty to decide from the screen size.
    std::string screen;
    // Its kitchen screen shows this station (a Station id or a printer id; empty = the page's).
    std::string station;
    // Its own look (a Look id: the bar dark, the patio light); empty = the store's.
    std::string look;
    // The on-screen keyboard for typing: "on", "off", or empty = on (but the
    // phone's or tablet's own keyboard on Android).
    std::string keyboard;
    // Phone orders need a name before Send here: "yes", "no", or empty = the person's / the store's.
    std::string requireName;
    // Its prep labels print here (a printer id); empty = the store's label printer.
    std::string labelPrinter;
    // Button and check text this much bigger here (percent; 0 = normal). A
    // person's own text size wins while they're logged in.
    int textSize = 0;
    // After a check is paid here: "print" a receipt, "ask" (print, email or
    // none), or nothing (empty). receiptPrinter "ask": choose the printer
    // each time (a handheld).
    std::string afterPaying;
    // Orders opened here start on this menu category (the bar: Drinks); empty: the meal's page.
    std::string startCategory;
    // Its card reader: "stripe" (a Stripe smart reader this app runs on),
    // "counter:tmr_..." (a Stripe reader beside it, run from the store's
    // computer), "simulated" (approves after a moment: for practice), or none.
    std::string cardReader;

    bool operator==(const TerminalConfig &) const = default;
};

// A part of the day. Index pages whose mealPeriod is this id are the ones the
// "index" jump opens while it lasts. A period runs from its start until the
// next one starts; before the first start of the day the last one continues
// (a dinner that runs past midnight).
struct MealPeriod {
    std::string id;
    std::string name;
    int start = 0;   // minutes after midnight

    bool operator==(const MealPeriod &) const = default;
};

inline std::vector<MealPeriod> defaultMealPeriods()
{
    return {{"breakfast", "Breakfast", 4 * 60}, {"lunch", "Lunch", 11 * 60}, {"dinner", "Dinner", 16 * 60}};
}

// The id of the period running at `minute` (0-1439); empty if there are none.
inline std::string mealPeriodAt(const std::vector<MealPeriod> &periods, int minute)
{
    const MealPeriod *current = nullptr;
    const MealPeriod *latest = nullptr;
    for (const MealPeriod &p : periods) {
        if (!latest || p.start > latest->start)
            latest = &p;
        if (p.start <= minute && (!current || p.start > current->start))
            current = &p;
    }
    return current ? current->id : latest ? latest->id : std::string();
}

// Where cash goes. With a drawer per terminal, cash sales go in the drawer
// of the terminal that closes the check. With server banks, whoever takes
// the cash keeps it in their own bank (no drawer) and turns it in when they
// check out, so any terminal can be used by anyone.
enum class CashMode { TerminalDrawer, ServerBank };

inline std::string toString(CashMode m) { return m == CashMode::ServerBank ? "serverBank" : "drawer"; }
inline CashMode cashModeFromString(const std::string &s)
{
    return s == "serverBank" ? CashMode::ServerBank : CashMode::TerminalDrawer;
}

// Store-wide POS settings, edited on the manager's admin screens.
struct PosSettings {
    std::string storeName = "ViewTouch";
    bool setupDone = false;   // the setup guide was finished (it opens for managers until then)
    std::string currencySymbol = "$";
    // Colors the store mixed itself (custom colors), the newest first.
    std::vector<std::string> customColors;
    // Price changes set for later (Prices… → Later): at `at` (epoch ms) the
    // prices (as Prices… shows them: a JSON list of {id, price, …}) are put
    // on the menu, and the change is gone from here.
    struct PriceChange {
        std::int64_t id = 0;
        std::int64_t at = 0;
        std::string label;     // "Burgers +10%"
        std::string prices;    // JSON
        bool operator==(const PriceChange &) const = default;
    };
    std::vector<PriceChange> priceChanges;
    TaxRates tax;
    std::vector<Tender> tenders;
    std::vector<PrinterConfig> printers;
    std::string receiptHeader;   // lines under the store name
    // Choices that cost nothing (No onion, Medium rare) on the guest's
    // receipt; off: only those that change the price (the kitchen gets all).
    bool receiptFreeChoices = false;
    // Items 86'd during service are back on the menu after End of Day.
    bool soldOutBackNextDay = true;
    std::string receiptFooter;
    // Auto-gratuity: added to dine-in checks with at least this many guests.
    std::int64_t gratuityBp = 0;   // 0 = off; 1800 = 18%
    int gratuityMinGuests = 6;
    std::vector<TerminalConfig> terminals;
    // This store's server, as terminals know it (found again after an
    // address change). Made once, on first start.
    std::string serverId;
    std::vector<MealPeriod> mealPeriods = defaultMealPeriods();
    // The menu's categories, in order (an item's family is one's id).
    std::vector<MenuCategory> menuCategories;
    std::vector<ModifierGroup> modifierGroups;

    const ModifierGroup *modifierGroup(const std::string &id) const
    {
        for (const ModifierGroup &g : modifierGroups) {
            if (g.id == id)
                return &g;
        }
        return nullptr;
    }
    CashMode cashMode = CashMode::TerminalDrawer;
    // Whether terminals have a cash drawer, unless set per terminal.
    bool terminalsHaveDrawer = true;
    // Time and overtime: paid breaks count as worked time; overtime after
    // this many hours in a day / in the pay week (0: no such rule); the pay
    // week starts on this weekday (0 Sunday ... 6 Saturday).
    bool paidBreaks = false;
    int overtimeDailyHours = 0;
    int overtimeWeeklyHours = 40;
    int weekStartsOn = 0;
    // Whether people must close (or hand over) their checks before they
    // check out their bank, unless set per employee.
    bool checkoutNeedsClosedChecks = true;
    // Counting a drawer or bank blind: what should be in it stays hidden
    // until it is counted (managers see it, unless set per employee). And
    // the bills and coins it is counted in, in cents, largest first.
    bool blindDrawerCount = false;
    // Voids of sent items and discounts / comps made on a screen ask why,
    // from these lists (unless set per employee); an empty list asks nothing.
    bool askReasons = true;
    // Counting stock: what should be there is shown while counting (else
    // only in the Stock Count report, after: a count not swayed by it).
    bool stockCountShowsExpected = false;
    // Store Check: problems up to this id have been seen by a manager.
    std::int64_t problemsSeen = 0;
    // Prep labels print here (a printer id; each screen can say otherwise).
    std::string labelPrinter;
    // Log Waste: why it was thrown away.
    std::vector<std::string> wasteReasons{"Dropped or spilled", "Expired or spoiled", "Made wrong",
                                          "Sent back by a guest", "Overcooked or burnt"};
    std::vector<std::string> voidReasons{"Wrong item rung in", "Guest changed their mind", "Took too long",
                                         "Guest didn't like it", "Kitchen mistake"};
    std::vector<std::string> compReasons{"Guest unhappy", "Took too long", "Manager's guest", "Kitchen mistake",
                                         "Regular / VIP"};
    std::vector<std::int64_t> cashDenominations{10000, 5000, 2000, 1000, 500, 100, 25, 10, 5, 1};
    // A second folder every backup is also copied to (a USB drive, a
    // network share), so one dead disk can't take the backups with it.
    std::string backupCopyDir;
    // Encrypted backups: the key made from the backup password, and its salt
    // (base64). Empty: backups are plain copies.
    std::string backupKey;
    std::string backupSalt;
    // Waitlist: quoted minutes for each party ahead, and the text sent when
    // a table is ready ({name}, {store}). textWebhook: where texts are sent
    // (a JSON POST of {to, message}; empty: no texting).
    // Between the store's servers (main and standby): the key the standby
    // replicates with (base64), and how many times a standby took over.
    std::string replicaKey;
    // The store's language: screens without anyone logged in (and people
    // without their own), the customer display, receipts and tickets.
    std::string language = "en";
    // The page each job starts on at login (job -> page id); none: the floor plan.
    std::map<std::string, std::string> startPages;
    // What "Extra" adds to an item's or a modifier's price: a percent of it
    // and/or an amount (extra cheese: +50%, or +$0.75). 0 and 0: no charge.
    int extraPercent = 0;
    Money extraCharge;
    Money withExtra(Money price) const { return price + price.percent(std::int64_t(extraPercent) * 100) + extraCharge; }
    std::vector<Vendor> vendors;
    // What cash paid out of a drawer is for, for the Expenses report.
    // Franchise fees on net sales (basis points: 500 = 5%), for the Royalty report.
    std::int64_t royaltyBp = 0;
    std::int64_t adFundBp = 0;
    // Chart of accounts for the Accounting report's journal: key -> "4010 Food sales".
    // Keys: sales, sales:<family>, tax, tax:<class>, gratuity, tips, discounts,
    // staffMeals, rounding, giftCardsSold, tender:<tender id>.
    std::map<std::string, std::string> accounts;
    std::vector<std::string> expenseCategories{"Food & supplies", "Produce", "Ice", "Cleaning", "Repairs", "Other"};
    // Self-order kiosk: send orders to the kitchen as soon as the guest
    // finishes (else when they are paid for at the counter), and how long an
    // untouched order waits before it is cleared.
    bool kioskSendNow = false;
    int kioskIdleSeconds = 90;
    // A slip from the kiosk's receipt printer when the guest places the
    // order: the number to show at the counter, and what they ordered.
    bool kioskSlip = true;
    // How the self-order kiosk looks and what it asks (its accent color,
    // logo and pictures are the customer display's). Colors "#rrggbb";
    // empty = the usual.
    struct KioskLook {
        std::string background, card, go, text, font, welcome;
        int sizePercent = 100;
        bool askWhere = true;    // For Here / To Go
        bool askName = true;     // the name for calling the order
        bool easyReach = true;   // the Easy Reach button
        bool operator==(const KioskLook &) const = default;
    } kioskLook;
    int serverTerm = 0;
    // The builds this store has run, oldest first (the last 50): each when it
    // first started here. Store Settings -> About lists them.
    struct BuildRun {
        std::string version;   // 0.9.1283
        int number = 0;
        std::string commit;
        std::int64_t at = 0;
        bool operator==(const BuildRun &) const = default;
    };
    std::vector<BuildRun> builds;
    // Log a screen out after this many idle minutes (0: never).
    int autoLogoutMinutes = 0;
    // Dim a screen nobody has touched for this long (0: never). Kitchen,
    // bar and expo screens and the self-order kiosk stay on.
    int screenSaverMinutes = 10;
    // A table seated longer than this is marked on the floor plan.
    int tableLongMinutes = 90;
    // Clock in only near a scheduled shift (managers excepted), from this
    // many minutes before it starts until it ends.
    bool scheduleRequired = false;
    int clockInEarlyMinutes = 15;
    // Tip-outs: a share of each person's tips ("tips") or sales ("sales")
    // goes to everyone of `role` who worked today, split by hours.
    struct TipOut {
        std::string role;
        std::int64_t percentBp = 0;
        std::string basis = "tips";
        bool operator==(const TipOut &) const = default;
    };
    std::vector<TipOut> tipOuts;
    // The tip pool: each job's share for an hour worked (100: a full share;
    // none: not in the pool). Everyone in it puts in their tips (after
    // tip-outs) and takes out by hours times share.
    std::map<std::string, int> tipPool;
    // Ask at clock out for the cash tips kept (they count toward tips, and
    // the pool).
    bool declareCashTips = false;
    // Loyalty: points per dollar spent (after discounts), and what they buy.
    struct Reward {
        int points = 0;
        Money value;
        bool operator==(const Reward &) const = default;
    };
    // The customer display: a logo (an image file), an accent color, and
    // what it shows between guests (one message per slide; "image:<file>"
    // for a picture).
    std::string displayLogo;     // the store's logo: a picture ref ("store:logo.png") or path
    bool receiptLogo = false;    // printed at the top of receipts (ESC/POS printers)
    std::string displayAccent = "#2f6fd6";
    std::vector<std::string> displaySlides;
    bool loyaltyEnabled = false;
    int pointsPerDollar = 1;
    std::vector<Reward> rewards;
    // Promotions, applied by themselves while they run: percent off the
    // matching items, or (buy > 0) "buy `buy`, get `get` at percent off".
    // No categories or items: the whole order. A coupon (a code) applies
    // only to checks it was typed or scanned on.
    struct Promotion {
        std::string id;
        std::string name;
        bool active = true;
        std::vector<std::string> families;   // menu families it covers
        std::vector<std::string> items;      // and/or menu item ids
        std::int64_t percentBp = 0;          // 5000 = half off, 10000 = free
        int buy = 0;
        int get = 0;
        int startMinute = 0;                 // start == end: all day
        int endMinute = 0;
        int days = 0x7F;                     // bit 0 Sunday ... bit 6 Saturday
        std::string code;                    // a coupon's code ("SAVE5"; any case); empty: by itself
        Money amount;                        // a fixed amount off instead of a percent
        Money minimum;                       // only on orders of at least this (items)
        std::string fromDay;                 // "2026-11-01": its first day (empty: now)
        std::string untilDay;                // ...and its last (empty: no end)
        int maxUses = 0;                     // checks it may be used on (0: any number)
        int uses = 0;                        // closed checks it was used on
        bool operator==(const Promotion &) const = default;
    };
    std::vector<Promotion> promotions;
    int waitMinutesPerParty = 10;
    // Kitchen display: a ticket turns yellow after warn minutes, red after late.
    // What each item usually takes, sent to made (seconds, item id -> a
    // running average): kitchen tickets are late past their slowest item's.
    std::map<std::string, int> prepSeconds;
    int kitchenWarnMinutes = 8;
    int kitchenLateMinutes = 15;
    // Orders for later go to the kitchen this long before they're due.
    int laterLeadMinutes = 20;
    // Phone orders: a name before Send (and an address for deliveries);
    // employees and terminals can say otherwise.
    bool requireOrderName = false;
    // The ready time quoted on takeouts and deliveries: the usual minutes, plus
    // this much for each order the kitchen is still working on.
    int takeoutMinutes = 15;
    int deliveryMinutes = 35;
    int minutesPerOrderWaiting = 2;
    Money deliveryFee;   // added to deliveries when sent (0: none)
    // Card readers: the store's Stripe secret key (stays on the store's
    // computer: it gets the readers' connection tokens and makes refunds),
    // and the currency cards are charged in.
    std::string stripeSecretKey;
    // Cards taken while the internet is down (Stripe readers running
    // ViewTouch): the bank only sees them later, so the store takes the risk
    // of a decline. Off unless turned on; then up to so much each, and so
    // much waiting in all (cents).
    bool offlineCards = false;
    std::int64_t offlineCardMax = 10000;
    std::int64_t offlineCardTotal = 100000;
    std::string cardCurrency = "usd";
    // Holding a bar tab's card: how much the bank sets aside (0: tabs don't hold cards).
    std::int64_t tabHold = 5000;
    // Stripe countertop readers paired with the store (Manager -> Card
    // Readers), the Stripe location they belong to, and where card tips are
    // asked: "" the customer display, "reader" on the reader's screen.
    struct StripeReader {
        std::string id;           // tmr_...
        std::string label;
        std::string deviceType;   // stripe_s700, bbpos_wisepos_e, simulated_wisepos_e...
        bool operator==(const StripeReader &) const = default;
    };
    std::vector<StripeReader> stripeReaders;
    std::string stripeLocation;
    std::string cardTipOn;
    // Messages posted until a time (the original's Expire Messages): every
    // screen shows them, to each person, until then.
    struct Notice {
        std::int64_t id = 0;
        std::int64_t at = 0;
        std::int64_t until = 0;
        std::string from, to, text;
        bool operator==(const Notice &) const = default;
    };
    std::vector<Notice> notices;
    // Changes managers made to time punches, with why (the last 500): the
    // Labor report lists them.
    struct PunchChange {
        std::int64_t at = 0;
        std::int64_t punchId = 0;
        std::string by, employee, what, why;
        bool operator==(const PunchChange &) const = default;
    };
    std::vector<PunchChange> punchChanges;
    // Asked for on the Time Clock, decided by a manager (Schedule -> Requests):
    // a day off, or giving a shift away (someone takes it, then it's approved).
    struct StaffRequest {
        std::int64_t id = 0;
        std::string kind;          // "timeOff" | "swap"
        std::string employeeId;
        std::int64_t at = 0;       // when asked
        std::int64_t day = 0;      // time off: that day's midnight
        std::int64_t shiftId = 0;  // swap: the shift given away
        std::string takerId;       // swap: who takes it (empty: up for grabs)
        std::string note;          // the reason
        std::string status = "pending";   // pending | approved | denied | cancelled
        std::string decidedBy;
        std::int64_t decidedAt = 0;
        bool operator==(const StaffRequest &) const = default;
    };
    std::vector<StaffRequest> staffRequests;
    // Opening and closing checklists (Store settings), and what's been done
    // on them this business day (checklistDayId): who ticked what, when.
    std::vector<std::string> openingChecklist;
    std::vector<std::string> closingChecklist;
    struct ChecklistTick {
        std::string list, task, by;
        std::int64_t at = 0;
        bool operator==(const ChecklistTick &) const = default;
    };
    std::int64_t checklistDayId = 0;
    // Tables the host stand marked: "dirty" (until bussed), "reserved" for a
    // party, or "joined" to another table for a big party (`with`).
    struct TableState {
        std::string table, state, with, by;
        std::int64_t partyId = 0;
        std::int64_t since = 0;
        bool operator==(const TableState &) const = default;
    };
    std::vector<TableState> tableStates;
    std::vector<ChecklistTick> checklistTicks;
    // Kitchen stations, each with its own screen (Manager -> Menu: where an item is made).
    std::vector<Station> stations;
    // Tip choices offered to the guest (percent of the check before gratuity).
    std::vector<int> tipPercents{15, 18, 20, 25};
    std::string tableReadyText = "Hi {name}, your table at {store} is ready! Please come to the host stand.";
    std::string textWebhook;

    bool hasDrawer(const std::string &terminal) const
    {
        for (const TerminalConfig &t : terminals) {
            if (t.name == terminal && !t.drawer.empty())
                return t.drawer == "yes";
        }
        return terminalsHaveDrawer;
    }

    std::string receiptPrinterFor(const std::string &terminal) const
    {
        for (const TerminalConfig &t : terminals) {
            if (t.name == terminal && !t.receiptPrinter.empty() && t.receiptPrinter != "ask")
                return t.receiptPrinter;
        }
        return "receipt";
    }

    const TerminalConfig *pairedTerminal(const std::string &id) const
    {
        for (const TerminalConfig &t : terminals) {
            if (!id.empty() && t.id == id && !t.key.empty())
                return &t;
        }
        return nullptr;
    }

    const PrinterConfig *printer(const std::string &id) const
    {
        for (const PrinterConfig &p : printers) {
            if (p.id == id)
                return &p;
        }
        return nullptr;
    }

    const Tender *tender(const std::string &id) const
    {
        for (const Tender &t : tenders) {
            if (t.id == id)
                return &t;
        }
        return nullptr;
    }

    bool operator==(const PosSettings &) const = default;
};

} // namespace vt::core
