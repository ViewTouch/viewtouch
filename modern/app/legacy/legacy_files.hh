#pragma once

#include "app/legacy/legacy_tokens.hh"

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

#include <array>
#include <cstdint>
#include <optional>

// The original ViewTouch's files, read as it reads them (field by field, in
// its order, each version as legacy wrote it). Only what ViewTouch Modern
// uses is kept; the rest is read past. Sources (the legacy branch):
// zone/pos_zone.cc, zone/zone.cc (pages), main/business/sales.cc (menu),
// main/data/settings.cc, main/business/employee.cc, customer.cc,
// main/data/archive.cc with check.cc, drawer.cc, tips.cc, exception.cc,
// expense.cc and credit.cc (a closed business day), labor.cc (time clock).
namespace vt::legacy {

// --- pages: vt_data, zone_db.dat, tables.dat --------------------------------

struct Look {
    int frame = 0;
    int texture = 0;
    int color = 0;
};

struct Zone {
    int type = 0;
    QString kind;               // "ZONE_STANDARD"...
    QString name;
    int group = 0;
    int x = 0, y = 0, w = 0, h = 0;
    int behave = 0;
    int font = 0;
    std::array<int, 3> frame{}, texture{}, color{}, image{};   // normal, selected, alternate
    int shadow = 0;
    int shape = 0;
    QVariantMap fields;         // the type's own: "JumpType", "ItemName", "Message"...
    int key = 0;
};

struct Page {
    QString name;
    int id = 0;
    int image = 0;
    int titleColor = 0;
    int parent = 0;
    int type = 0;
    int index = 0;
    int size = 0;
    int font = 0;
    std::array<Look, 3> defaults{};
    int spacing = 0;
    int shadow = 0;
    QList<Zone> zones;
};

struct PageDefaults {           // a page file's store-wide defaults (version 28 on)
    int font = 0, shadow = 0, spacing = 0;
    std::array<Look, 3> looks{};
    int image = 0, titleColor = 0, size = 0;
};

struct PageFile {
    int version = 0;            // the zone version
    QList<Page> pages;
    std::optional<PageDefaults> defaults;
};

// vt_data: the system pages (and legacy's default accounts, read past).
std::optional<PageFile> readVtData(const QString &path, QString *error);
// zone_db.dat, tables.dat: a store's own pages, its dining rooms.
std::optional<PageFile> readPageFile(const QString &path, QString *error);

// --- menu.dat ----------------------------------------------------------------

struct MenuItem {
    int id = 0;
    QString name;
    QString buttonName;         // zone_name: on its button (empty: the name)
    QString printName;          // on kitchen tickets (empty: the name)
    int type = 0;               // 0 item, 1 modifier, 2 non-tracking modifier, 3 item + substitute, 6 by weight, 7 admission
    int cost = 0;               // cents
    int subCost = 0;            // as a substitute
    int employeeCost = 0;
    int takeoutCost = 0;
    int taxId = 0, takeoutTaxId = 0;
    int callOrder = 0;
    int printer = 0;            // 0 default, 1 Kitchen1, 2 Kitchen2, 3 Bar1, 4 Bar2, 5 expediter, 12/13 Kitchen3/4, 99 none
    int family = 0;             // 255 unknown
    int itemClass = 0;
    int salesType = 0;          // flags: 1 alcohol, 2 untaxed, 4 room, 8 merchandise, 16 no comp, 32 no staff meal, 64 no discount
    int period = 0;
    int stocked = 0;
    int prepareTime = 0;
    int allowIncrease = 1;
    QString callCenterName;
    int deliveryCost = 0;
    QString itemCode;
    bool outOfStock = false;
};

struct Menu {
    int version = 0;
    QList<MenuItem> items;
};
std::optional<Menu> readMenu(const QString &path, QString *error);

// --- settings.dat (and the settings snapshot in each archive) ----------------

struct Discount {               // DiscountInfo, CompInfo, CouponInfo, MealInfo, CreditCardInfo
    int id = 0;
    QString name;
    int flags = 0;              // 1 a percent, 1024 each item, 2048 a particular item...
    int amount = 0;             // cents, or a percent x 100 (flags & 1)
    int local = 0;
    bool active = true;
    // coupons only
    int family = -1;
    int itemId = -1;
    QString itemName;
    WallTime startTime, endTime;    // hours it's good for
    int days = 0, months = 0;
    int automatic = 0;
    WallTime startDate, endDate;
};

struct Media {                  // the five lists, in a settings file or an archive
    QList<Discount> discounts, coupons, creditCards, comps, meals;
};

struct Terminal {
    QString name;
    int hardware = 0;
    int type = 0;
    QString displayHost;
    QString printerHost;
    int printerModel = 0;
    int printerPort = 0;
    int drawers = 0;
    int kitchen = 0;
    bool isServer = false;
};

struct Printer {
    QString name;
    QString host;
    int port = 0;
    int model = 0;
    int type = 0;               // 1 kitchen1... (as menu items' printer), 6 receipt, 7 report
};

struct Tax {                    // TaxInfo
    int id = 0;
    QString name, shortName;
    int flags = 0;
    int amount = 0;
};

struct Settings {
    int version = 0;
    QString storeName, storeAddress, storeAddress2;
    // rates as fractions (0.085 is 8.5%)
    double taxFood = 0, taxAlcohol = 0, taxRoom = 0, taxMerchandise = 0;
    double taxGst = 0, taxPst = 0, taxHst = 0, taxQst = 0, royalty = 0, taxVat = 0, advertiseFund = 0;
    int priceRounding = 0;
    int useSeats = 0;
    int drawerMode = 0;
    double doubleMult = 2;
    int doubleAdd = 0;
    QString language;
    int overtimeShift = 0, overtimeWeek = 0, wageWeekStart = 0;
    int timeFormat = 0, dateFormat = 0;
    QHash<int, int> jobFlags, jobLevel;   // by job number
    QStringList receiptHeader, receiptFooter;   // 4 lines each (empty ones kept)
    QStringList displayHeader;                  // the customer display's 2 lines
    std::array<int, 12> shiftStart{};
    std::array<int, 12> mealActive{}, mealStart{};
    std::array<int, 64> familyGroup{}, familyPrinter{}, videoTarget{};
    QList<Terminal> terminals;
    QList<Printer> printers;
    Media media;
    QList<Tax> taxes;
    int drawerDayFloat = 0, drawerNightFloat = 0;   // cents
    QString moneySymbol;
    int fastTakeouts = 0;
    // conf/*.ini (they win over settings.dat)
    bool taxTakeoutFood = true;
    std::array<int, 4> taxInclusive{};    // food, room, alcohol, merchandise
};
// settings.dat, then conf/tax.ini, fees.ini, fastfood.ini beside it.
std::optional<Settings> readSettings(const QString &datFolder, QString *error);

// --- employee.dat ------------------------------------------------------------

struct Job {
    int job = 0;                // 1 dishwasher... 6 server, 7 server/cashier, 12 manager, 13 bartender, 14 chef
    int payRate = 1;            // 1 an hour, 2 a day, 3 a week, 7 a month
    int payAmount = 0;          // cents
    int startPage = 0;
    int dept = 0;
};

struct Employee {
    QString systemName, lastName, firstName;
    QString address, city, state, phone;   // (the SSN is read past, never kept)
    QString description;
    int id = 0;
    int key = 0;                // the login number
    int employeeNo = 0;
    bool training = false;
    QString password;
    bool active = true;
    QList<Job> jobs;
};
std::optional<QList<Employee>> readEmployees(const QString &path, QString *error);

// --- customers/customer_N ------------------------------------------------------

struct Customer {
    int id = 0;
    QString lastName, firstName, company, phone, extension;
    QString address, address2, crossStreet, city, state, postal;
    QString comment;            // (card numbers and licenses are read past, never kept)
};
std::optional<Customer> readCustomer(const QString &path, QString *error);

// --- a closed business day: archive/archive_N --------------------------------

struct Order {
    QString name;
    int type = 0;               // 0 item, 1 modifier, 2 method, 3 substitute...
    int cost = 0;               // cents, one
    int family = 0;
    int salesType = 0;
    int count = 1;
    int qualifier = 0;          // flags: 1 no, 2 side, 4 sub, 8 lite, 16 only, 32 extra, 64 double...
    int status = 0;             // flags: 1 final, 2 sent, 4 made, 8 served, 16 comped
    int userId = 0;
    int seat = 0;
    int employeeMeal = 0;
    int isReduced = 0;
    int reducedCost = 0;
    int autoCouponId = -1;
    bool isModifier() const { return type == 1 || type == 2 || (type == 3 && (qualifier & 4)); }
};

struct Payment {
    int tenderType = 0;         // 0 cash, 1 check, 2 charge card, 3 coupon, 4 gift, 5 comp, 6 account, 7 room,
                                // 8 discount, 10 staff meal, 11 credit, 12 debit, 13 charged tip, 18 change, 21 gratuity...
    int tenderId = 0;
    int amount = 0;             // cents, or a percent x 100 (flags & 1)
    int flags = 0;
    int drawerId = 0;
    int userId = 0;
    // a card (tenders 11, 12)
    int cardType = 0;
    int cardTip = 0;
};

struct SubCheck {
    int status = 0;             // 1 open, 2 closed, 3 voided
    int settleUser = 0;
    WallTime settleTime;
    int drawerId = 0;
    QList<Order> orders;
    QList<Payment> payments;
    QString taxExempt;
    int tabTotal = 0;
    int deliveryCharge = 0;
};

struct Check {
    int serial = 0;
    WallTime opened;
    int userOpen = 0, userOwner = 0;
    int flags = 0;              // 4 training
    int type = 1;               // 1 restaurant, 2 takeout, 3 bar, 5 delivery, 8 retail, 9 fast food, 11 dine in, 12 to go, 13 call in...
    int customerId = -1;
    int state = 0;
    int number = 0;             // checknum
    WallTime checkIn, checkOut, date;
    QString comment;
    int guests = 0;
    QString label;              // the table
    QList<SubCheck> subchecks;
    bool training() const { return flags & 4; }
};

struct DrawerBalance {
    int tenderType = 0, tenderId = 0, entered = 0;
};

struct Drawer {
    int serial = 0;
    QString host;
    int position = 0;
    WallTime start, pulled, balanced;
    int owner = 0, puller = 0;
    int number = 0;             // a server bank: minus the employee's id
    int mediaBalanced = 0;
    QList<DrawerBalance> balances;
    struct Move { int tenderType = 0, amount = 0, userId = 0, targetId = 0; WallTime time; };
    QList<Move> payments;       // payouts, paid tips...
};

struct Tip {
    int userId = 0, amount = 0, paid = 0;
};

struct Archive {
    int version = 0;
    int id = 0;
    WallTime start, end;        // start unset: the previous day's end
    QList<Drawer> drawers;
    QList<Check> checks;
    QList<Tip> tips;
    int voids = 0, comps = 0;   // legacy's item exceptions, counted
    int expenses = 0;
    std::optional<Media> media;           // version 10 on
    std::optional<double> taxFood, taxAlcohol, taxRoom, taxMerchandise, taxGst, taxPst, taxHst, taxQst, taxVat;
    int priceRounding = 0;
    int cardRecords = 0;        // card batches and the like, read past
};
std::optional<Archive> readArchive(const QString &path, QString *error);

// The open business day (current/): its checks and drawers, one per file.
struct OpenDay {
    QList<Check> checks;
    QList<Drawer> drawers;
};
std::optional<OpenDay> readCurrent(const QString &folder, QString *error);

// --- labor/labor_N -------------------------------------------------------------

struct WorkEntry {
    int userId = 0;
    int job = 0;
    int payRate = 1;
    int payAmount = 0;          // cents
    int tips = 0;
    WallTime start, end;        // end unset: still clocked in
    int endShift = 0;
    bool edited = false;        // a manager changed it (the original kept by legacy)
};

struct LaborPeriod {
    int serial = 0;
    WallTime end;
    QList<WorkEntry> entries;
};
std::optional<LaborPeriod> readLabor(const QString &path, QString *error);

// --- a whole dat folder ------------------------------------------------------

// What a dat folder has, read in full (nothing changed): what an import
// previews and then brings over. A file that can't be read is an error of
// its own (the others are still read).
struct Store {
    QString folder;
    std::optional<PageFile> vtData;       // only when the store kept its own
    std::optional<PageFile> pages;        // zone_db.dat
    std::optional<PageFile> tables;       // tables.dat
    std::optional<Menu> menu;
    std::optional<Settings> settings;
    QList<Employee> employees;
    QList<Customer> customers;
    QList<Archive> archives;              // oldest first
    std::optional<OpenDay> current;
    QList<LaborPeriod> labor;
    QStringList errors;
    bool any() const { return pages || menu || settings || !employees.isEmpty() || !archives.isEmpty(); }
};
// A folder is legacy's dat folder when it has at least menu.dat,
// settings.dat or zone_db.dat.
bool isDatFolder(const QString &folder);
Store readStore(const QString &folder);

} // namespace vt::legacy
