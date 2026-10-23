// PosService: the manager screens' field lists (what each record has, how it is
// edited), in the schema format of the page editor's inspector.

#include "app/pos_admin_shared.hh"

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

using namespace admin;

// Settings that are the owner's or the maintainer's: shown to managers, not
// changed by them (null: a manager's).
const char *PosService::fieldPermission(const QString &panel, const QString &path) const
{
    static const QSet<QString> ownerStore = {
        // Cards: the money's way in.
        u"stripeSecretKey"_s, u"cardCurrency"_s, u"cardTipOn"_s, u"surchargePercent"_s, u"cashDiscountPercent"_s,
        u"tabHold"_s, u"offlineCards"_s, u"offlineCardMax"_s, u"offlineCardTotal"_s,
        // Labor law and pay.
        u"laborState"_s, u"paidBreaks"_s, u"overtimeDailyHours"_s, u"doubleTimeDailyHours"_s, u"seventhDayRule"_s,
        u"overtimeWeeklyHours"_s, u"mealBreakAfterHours"_s, u"restBreakEveryHours"_s, u"scheduleNoticeDays"_s,
        u"restBetweenShiftsHours"_s, u"giftCardCashBelow"_s, u"tipAllocation"_s, u"tipOuts"_s, u"tipPool"_s,
        // Franchise and accounting.
        u"royaltyPercent"_s, u"adFundPercent"_s, u"chartOfAccounts"_s};
    static const QSet<QString> maintainStore = {u"backupCopyDir"_s, u"encryptBackups"_s, u"backupPassword"_s};
    if (panel == u"taxes" || panel == u"cardReaders")
        return perm::Owner;
    if (panel == u"store")
        return ownerStore.contains(path) ? perm::Owner : maintainStore.contains(path) ? perm::Maintain : nullptr;
    return nullptr;
}

QVariantList PosService::adminFields(const QString &panel)
{
    QVariantList out = adminFieldsAll(panel);
    for (QVariant &f : out) {
        QVariantMap m = f.toMap();
        if (const char *p = fieldPermission(panel, m.value(u"path"_s).toString()); p && !can(QString::fromLatin1(p))) {
            m.insert(u"readOnly"_s, true);
            const QString whose = qstrcmp(p, perm::Maintain) == 0 ? tr("The maintainer's.") : tr("The owner's.");
            m.insert(u"hint"_s, m.value(u"hint"_s).toString().isEmpty() ? whose : whose + u' ' + m.value(u"hint"_s).toString());
            f = m;
        }
    }
    return out;
}

QVariantList PosService::adminFieldsAll(const QString &panel)
{
    const QVariantMap readonlyId = with(field(u"id"_s, tr("ID"), u"string"_s,
                                              tr("How buttons refer to it. Fixed once saved.")),
                                        u"readonlyExisting"_s, true);
    if (panel == u"menu") {
        QVariantList printers = options({{"", "(no ticket)"}});
        for (const PrinterConfig &p : s_->settings.printers)
            printers.append(QVariantMap{{u"value"_s, qs(p.id)}, {u"text"_s, qs(p.name)}});
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(with(field(u"prepMinutes"_s, tr("Kitchen time (minutes)"), u"int"_s,
                            tr("How long it should take. 0: what it usually takes (learned from the kitchen screen).")),
                      u"min"_s, 0), u"max"_s, 240),
            field(u"number"_s, tr("Number"), u"string"_s,
                  tr("To ring it in by number: type it on Find, or on a keyboard then Enter. Digits, each item its own.")),
            field(u"barcodes"_s, tr("Barcodes"), u"string"_s,
                  tr("Scan the package here (several: commas between). Scanning it on the order screen rings it in.")),
            field(u"price"_s, tr("Price"), u"money"_s),
            field(u"family"_s, tr("Family"), u"string"_s, tr("Groups items on the sales report")),
            with(field(u"taxClass"_s, tr("Tax"), u"enum"_s), u"options"_s,
                 options({{"food", "Food"}, {"alcohol", "Alcohol"}, {"merchandise", "Merchandise"},
                          {"room", "Room"}, {"none", "No tax"}})),
            with(field(u"printer"_s, tr("Kitchen ticket goes to"), u"enum"_s), u"options"_s, printers),
            with(field(u"station"_s, tr("Made at (kitchen screen)"), u"enum"_s,
                       tr("The station whose screen shows it (Store Settings -> Kitchen stations). "
                          "As a combo's part, it shows there on its own.")),
                 u"options"_s, [this] {
                     QVariantList st = options({{"", "Its ticket's screen"}});
                     for (const Station &x : s_->settings.stations)
                         st.append(QVariantMap{{u"value"_s, qs(x.id)}, {u"text"_s, qs(x.name)}});
                     return st;
                 }()),
            field(u"modifier"_s, tr("Is a modifier (attaches to the item before it)"), u"bool"_s),
            field(u"available"_s, tr("Available (untick when sold out / 86'd)"), u"bool"_s),
            field(u"modifierGroups"_s, tr("Modifier groups"), u"string"_s,
                  tr("Choices asked for when it's ordered, by group ID, comma separated: %1")
                      .arg(groupIds().join(u", "))),
            asRows(field(u"periodPrices"_s, tr("Prices by meal period"), u"text"_s,
                         tr("A different price during a meal period (Manager -> Meal Periods).")),
                   tr("+ Add a meal period price"),
                   {column(tr("Meal period"), "enum", 0, [this] {
                        QVariantList o;
                        for (const MealPeriod &m : s_->settings.mealPeriods)
                            o.append(QVariantMap{{u"value"_s, qs(m.id)}, {u"text"_s, qs(m.name)}});
                        return o;
                    }()),
                    column(tr("Price"), "money", 140)},
                   uR"(^\s*([^=]+?)\s*=\s*\D?\s*([\d.]+)\s*$)"_s, u"%1 = %2"_s),
            field(u"kitchenName"_s, tr("Kitchen name"), u"string"_s,
                  tr("What the kitchen screen and tickets show instead, e.g. \"BCN BGR\". Empty: the name.")),
            with(field(u"kitchenColor"_s, tr("Kitchen highlight"), u"enum"_s), u"options"_s,
                 options({{"", "None"}, {"red", "Red"}, {"orange", "Orange"}, {"yellow", "Yellow"}, {"green", "Green"},
                          {"blue", "Blue"}, {"purple", "Purple"}})),
            field(u"kitchenHide"_s, tr("Don't show in the kitchen"), u"bool"_s,
                  tr("Nothing to make (water, a gift card...). As a modifier: left off the kitchen ticket.")),
            field(u"takeoutPrice"_s, tr("Takeout price"), u"money"_s, tr("0 = the regular price.")),
            field(u"deliveryPrice"_s, tr("Delivery price"), u"money"_s, tr("0 = the takeout price (or the regular one).")),
            field(u"noDiscount"_s, tr("No discounts"), u"bool"_s, tr("Discounts and comps leave it out.")),
            field(u"noStaffDiscount"_s, tr("No staff discount"), u"bool"_s, tr("Staff pay full price for it (alcohol, say).")),
            with(with(field(u"ticketCapacity"_s, tr("Event tickets: seats"), u"int"_s,
                            tr("Tickets to an event: it sells out when this many are sold. 0 = not an event.")),
                      u"min"_s, 0), u"max"_s, 100000),
            field(u"eventAt"_s, tr("Event date and time"), u"string"_s,
                  tr("Like 2026-10-09 7:00 PM. On the ticket and the receipt; it can't be sold after.")),
            field(u"substitute"_s, tr("Can be a substitute"), u"bool"_s,
                  tr("With Sub, it goes in place of part of the item before it (a house salad instead of fries).")),
            field(u"substitutePrice"_s, tr("Substitute price"), u"money"_s, tr("What it adds as a substitute, e.g. 3.00.")),
            field(u"byWeight"_s, tr("Sold by weight"), u"bool"_s,
                  tr("The price is per pound (or the unit below); ordering it asks for the weight.")),
            with(field(u"weightUnit"_s, tr("Weight unit"), u"enum"_s), u"options"_s,
                 options({{"lb", "Pound (lb)"}, {"oz", "Ounce (oz)"}, {"kg", "Kilogram (kg)"}})),
            field(u"kioskHide"_s, tr("Not on the self-order kiosk"), u"bool"_s,
                  tr("Guests can't order it on their own. Alcohol never shows there.")),
            field(u"description"_s, tr("Description (self-order kiosk)"), u"text"_s,
                  tr("A line guests see under the name, e.g. \"Two patties, cheddar, house sauce\".")),
            field(u"image"_s, tr("Photo (self-order kiosk)"), u"image"_s,
                  tr("Pick one of the store's pictures, or Add Picture… from this computer.")),
            field(u"recipe"_s, tr("Recipe (what one uses up)"), u"text"_s,
                  tr("One ingredient per line with the amount, e.g. \"bun 1\" or \"lettuce 0.5\" (Manager -> Inventory)."
                     " Sold out by itself when one runs short.")),
        };
    }
    if (panel == u"promotions") {
        QStringList families;
        for (const MenuItem &m : s_->menu)
            if (!m.family.empty() && !families.contains(qs(m.family)))
                families << qs(m.family);
        return {
            field(u"name"_s, tr("Name"), u"string"_s, tr("On the check and receipt, e.g. Happy Hour")), readonlyId,
            field(u"active"_s, tr("On"), u"bool"_s),
            field(u"families"_s, tr("Categories"), u"string"_s,
                  tr("Comma separated: %1").arg(families.join(u", "_s))),
            field(u"items"_s, tr("Items"), u"string"_s, tr("Menu item IDs, comma separated (e.g. draft-beer, house-wine)")),
            field(u"percent"_s, tr("Percent off"), u"percent"_s, tr("50 = half price, 100 = free")),
            with(with(field(u"buy"_s, tr("Buy"), u"int"_s,
                            tr("0 = every matching item is off. Otherwise buy this many...")), u"min"_s, 0), u"max"_s, 20),
            with(with(field(u"get"_s, tr("…and get this many (the cheapest) at the percent off"), u"int"_s), u"min"_s, 0),
                 u"max"_s, 20),
            field(u"start"_s, tr("From"), u"string"_s, tr("Like 3:00 PM. From = Until: all day.")),
            field(u"end"_s, tr("Until"), u"string"_s),
            field(u"days"_s, tr("Days"), u"string"_s, tr("e.g. Mon-Fri, or Tue, or Sat, Sun. Empty: every day.")),
            field(u"code"_s, tr("Coupon code"), u"string"_s,
                  tr("e.g. SAVE5: then it only comes off checks the code is typed or scanned on (Pay -> Coupon…). Empty: by itself.")),
            field(u"amount"_s, tr("Amount off"), u"money"_s, tr("A fixed amount instead of a percent, e.g. 5.00.")),
            field(u"minimum"_s, tr("Orders of at least"), u"money"_s, tr("0 = any order.")),
            field(u"fromDay"_s, tr("First day"), u"string"_s, tr("e.g. 2026-11-01. Empty: from now.")),
            field(u"untilDay"_s, tr("Last day"), u"string"_s, tr("e.g. 2026-12-31. Empty: no end.")),
            with(field(u"maxUses"_s, tr("Times it may be used"), u"int"_s, tr("Checks in all. 0 = no limit.")), u"min"_s, 0),
            with(field(u"uses"_s, tr("Used so far"), u"int"_s, tr("Closed checks it came off. 0 starts over.")), u"min"_s, 0),
        };
    }
    if (panel == u"punches")
        return punchFields();
    if (panel == u"requests")
        return requestFields();
    if (panel == u"cardReaders") {
        return {
            field(u"label"_s, tr("Name"), u"string"_s, tr("Which one it is: \"Front counter\", \"Bar\".")),
            with(field(u"deviceType"_s, tr("Model"), u"string"_s, tr("Filled in by Stripe.")), u"readonlyExisting"_s, true),
            with(field(u"id"_s, tr("Stripe reader id"), u"string"_s, tr("Filled in by Stripe.")), u"readonlyExisting"_s, true),
            field(u"code"_s, tr("Pairing code"), u"string"_s,
                  tr("New reader: on the reader, swipe from the left edge -> Settings -> Generate pairing code, "
                     "and type it here. In Stripe test mode, simulated-wpe pairs a simulated reader.")),
            field(u"line1"_s, tr("Store street address"), u"string"_s,
                  tr("Only the first time, if Stripe has no location for the store yet.")),
            field(u"city"_s, tr("City"), u"string"_s),
            field(u"state"_s, tr("State"), u"string"_s),
            field(u"postalCode"_s, tr("ZIP / postal code"), u"string"_s),
            field(u"country"_s, tr("Country (two letters)"), u"string"_s),
        };
    }
    if (panel == u"vendors") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            field(u"phone"_s, tr("Phone"), u"string"_s),
            field(u"account"_s, tr("Our account number"), u"string"_s),
            field(u"note"_s, tr("Note"), u"text"_s, tr("Delivery days, the rep, order cut-off…")),
        };
    }
    if (panel == u"inventory") {
        QVariantList vendorOptions{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(none)")}}};
        for (const Vendor &v : s_->settings.vendors)
            vendorOptions.append(QVariantMap{{u"value"_s, qs(v.id)}, {u"text"_s, qs(v.name)}});
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(field(u"vendor"_s, tr("Vendor"), u"enum"_s, tr("Who it usually comes from (Manager -> Vendors).")),
                 u"options"_s, vendorOptions),
            field(u"unit"_s, tr("Unit"), u"string"_s, tr("each, oz, lb, slice…")),
            field(u"onHand"_s, tr("On hand"), u"number"_s, tr("Count it, or add what was delivered.")),
            field(u"lowAt"_s, tr("Low at"), u"number"_s, tr("Warn when it gets down to this.")),
            with(with(field(u"shelfLifeHours"_s, tr("Keeps for (hours)"), u"int"_s,
                            tr("Once prepped: its prep label's use-by. 0: chosen when printing.")), u"min"_s, 0), u"max"_s, 8760),
            field(u"par"_s, tr("Keep on hand (par)"), u"number"_s,
                  tr("Below this, the Stock report's order list asks for enough to get back up to it. 0: not ordered by par.")),
            field(u"cost"_s, tr("Cost per unit"), u"money"_s, tr("For the Food Cost report.")),
        };
    }
    if (panel == u"employees") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(field(u"role"_s, tr("Role"), u"enum"_s), u"options"_s,
                 options({{"server", "Server"}, {"bartender", "Bartender"}, {"cashier", "Cashier"}, {"host", "Host"},
                          {"busser", "Busser"}, {"driver", "Delivery driver"}, {"manager", "Manager"}, {"owner", "Owner"}})),
            field(u"pin"_s, tr("New PIN"), u"pin"_s, tr("4 to 8 digits. Leave empty to keep the current PIN.")),
            field(u"payRate"_s, tr("Pay rate ($ an hour)"), u"money"_s, tr("For their role, before tips. For the Labor report.")),
            field(u"otherJobs"_s, tr("Other jobs"), u"text"_s,
                  tr("Other jobs they work, one per line with its pay, e.g. \"bartender 9.00\". "
                     "They choose the job when they clock in. Jobs: server, bartender, cashier, host, busser, driver, manager.")),
            with(field(u"language"_s, tr("Language"), u"enum"_s, tr("The screens switch to it when this person logs in.")),
                 u"options"_s, languageOptions(true)),
            field(u"look"_s, tr("Look"), u"look"_s,
                  tr("Their colors on any screen while they're logged in (High Contrast, say); empty: the screen's.")),
            with(field(u"textSize"_s, tr("Text size"), u"enum"_s, tr("Bigger button and check text, for them only.")),
                 u"options"_s, options({{"100", "Normal"}, {"115", "Bigger"}, {"130", "Biggest"}, {"150", "Largest"}})),
            field(u"colorSafe"_s, tr("Colors for color blindness"), u"bool"_s,
                  tr("Table and kitchen ticket colors that color-blind eyes tell apart (blue, yellow, orange), on any screen while they're logged in.")),
            field(u"leftHanded"_s, tr("Left-handed"), u"bool"_s,
                  tr("On their order and Pay screens the check is on the right and the menu on the left.")),
            field(u"buzz"_s, tr("Buzz for each item"), u"bool"_s,
                  tr("A handheld gives a short buzz each time they add an item, so they know it went on without looking.")),
            with(with(field(u"startPage"_s, tr("Start screen"), u"page"_s,
                       tr("The floor plan they see first when they log in. Empty: their job's (Store settings).")),
                 u"emptyText"_s, tr("(their job's)")), u"pages"_s, u"floor"_s),
            field(u"active"_s, tr("Active (can log in)"), u"bool"_s),
            field(u"training"_s, tr("In training (practice only)"), u"bool"_s,
                  tr("Their checks never go to the kitchen and don't count as sales, stock or cash.")),
            with(field(u"cashMode"_s, tr("Cash handling"), u"enum"_s,
                       tr("Where the cash this person takes goes. Store setting: %1.")
                           .arg(s_->settings.cashMode == CashMode::ServerBank ? tr("server bank") : tr("terminal drawer"))),
                 u"options"_s, options({{"", "Store setting"}, {"serverBank", "Own bank (carries their cash)"},
                                        {"drawer", "Terminal's cash drawer"}})),
            permField(perm::Order, tr("Take orders")),
            permField(perm::Settle, tr("Take payments and close checks")),
            permField(perm::OthersChecks, tr("Open other people's checks")),
            permField(perm::Discount, tr("Give discounts and comps")),
            permField(perm::Void, tr("Void items already sent")),
            permField(perm::Manager, tr("Manager screens (reports, settings, staff…)")),
            permField(perm::EditLayout, tr("Edit pages")),
            with(field(u"requireName"_s, tr("Phone orders need a name before Send"), u"enum"_s,
                       tr("Store setting: %1.").arg(s_->settings.requireOrderName ? tr("yes") : tr("no"))),
                 u"options"_s, options({{"", "Store setting"}, {"yes", "Yes"}, {"no", "No"}})),
            with(field(u"reasons"_s, tr("Asked why on voids and comps"), u"enum"_s,
                       tr("Store setting: %1.").arg(s_->settings.askReasons ? tr("yes") : tr("no"))),
                 u"options"_s, options({{"", "Store setting"}, {"ask", "Yes"}, {"skip", "No"}})),
            with(field(u"blindCount"_s, tr("Counting a drawer"), u"enum"_s,
                       tr("Store setting: %1.").arg(s_->settings.blindDrawerCount ? tr("blind (managers see it)")
                                                                                   : tr("sees what should be there"))),
                 u"options"_s, options({{"", "Store setting"}, {"blind", "Blind: doesn't see what should be there"},
                                        {"sees", "Sees what should be there"}})),
            with(field(u"checkout"_s, tr("Checking out with open checks"), u"enum"_s,
                       tr("Store setting: %1.").arg(s_->settings.checkoutNeedsClosedChecks
                                                         ? tr("close all checks first") : tr("allowed"))),
                 u"options"_s, options({{"", "Store setting"}, {"closeChecks", "Must close all checks first"},
                                        {"anyTime", "May leave checks open"}})),
            with(field(u"tipPool"_s, tr("Tip pool"), u"enum"_s,
                       tr("By their job: the store's tip pool shares (Store Settings).")),
                 u"options"_s, options({{"", "By their job"}, {"in", "In the pool (a full share)"},
                                        {"out", "Keeps their own tips"}})),
        };
    }
    if (panel == u"tenders") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(field(u"kind"_s, tr("Kind"), u"enum"_s), u"options"_s,
                 options({{"cash", "Cash (gives change, goes in the drawer)"},
                          {"card", "Card (capped at the balance)"},
                          {"discount", "Discount / comp"}})),
            field(u"percent"_s, tr("Discount %"), u"percent"_s, tr("Discounts only. 100 = comp.")),
            field(u"staffMeal"_s, tr("Staff meal"), u"bool"_s,
                  tr("Discounts only: records who ate it, leaves out items marked \"No staff discount\", "
                     "and shows on the Sales report.")),
        };
    }
    if (panel == u"printers") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(field(u"type"_s, tr("Connection"), u"enum"_s), u"options"_s,
                 options({{"network", "Network (raw TCP, port 9100)"}, {"cups", "CUPS queue"},
                          {"file", "Text file"}, {"none", "Off"}})),
            field(u"host"_s, tr("Address"), u"string"_s, tr("Network printers: IP address or host name")),
            with(field(u"port"_s, tr("Port"), u"int"_s), u"min"_s, 1),
            field(u"path"_s, tr("File or queue"), u"string"_s, tr("File path, or the CUPS queue name")),
            with(field(u"format"_s, tr("Format"), u"enum"_s), u"options"_s,
                 options({{"", "Automatic"}, {"escpos", "ESC/POS (thermal)"}, {"text", "Plain text"},
                          {"zpl", "Label printer: ZPL (Zebra and compatible)"},
                          {"tspl", "Label printer: TSPL (TSC, Xprinter, Munbyn, Rollo…)"}})),
            with(with(field(u"labelWidthMm"_s, tr("Label width (mm)"), u"int"_s,
                            tr("Label printers: the labels' size. 2 x 1 inch is 51 x 25 mm.")), u"min"_s, 10), u"max"_s, 200),
            with(with(field(u"labelHeightMm"_s, tr("Label height (mm)"), u"int"_s), u"min"_s, 10), u"max"_s, 300),
            with(with(field(u"width"_s, tr("Characters per line"), u"int"_s), u"min"_s, 16), u"max"_s, 80),
            field(u"cutter"_s, tr("Cut paper after each ticket"), u"bool"_s),
            field(u"drawerKick"_s, tr("Cash drawer is connected to this printer"), u"bool"_s),
            with(field(u"charset"_s, tr("Letters"), u"enum"_s,
                       tr("Accented letters (ñ, é), ¿ ¡, £ and € on Epson and most receipt printers; plain letters for a printer that shows them wrong.")),
                 u"options"_s, options({{"", "Accented letters (PC858)"}, {"ascii", "Plain letters only"}})),
            field(u"watch"_s, tr("Warn when it's out of paper"), u"bool"_s,
                  tr("Network ESC/POS printers: every screen says when it's out of paper or low, its cover is "
                     "open, or it stops answering. Turn off for a printer that doesn't report it.")),
            field(u"receipts"_s, tr("Prints receipts"), u"bool"_s,
                  tr("Offered when a screen asks where to print a receipt (handhelds: Terminals -> Ask each time).")),
        };
    }
    if (panel == u"taxes") {
        return {
            // Where the store is: the rate there, from each state's published rates.
            with(field(u"state"_s, tr("The store's state"), u"enum"_s,
                       tr("Also sets its overtime, meal break and gift card rules (Staff hours).")),
                 u"options"_s, stateOptions()),
            field(u"zip"_s, tr("ZIP code"), u"string"_s, tr("Where the store is. Save, and the rate there shows below.")),
            with(field(u"taxWhere"_s, tr("Where in the ZIP code"), u"enum"_s,
                       tr("Some ZIP codes have more than one rate: inside a city's limits or outside it. Choose the store's.")),
                 u"options"_s, taxWhereOptions()),
            field(u"suggested"_s, tr("The rate here"), u"info"_s),
            field(u"useSuggested"_s, tr("Set the rates below to these"), u"bool"_s,
                  tr("Food, alcohol and merchandise, then Save. Rates change: Store Check says when the table has a new one.")),
            field(u"food"_s, tr("Food tax %"), u"percent"_s),
            field(u"alcohol"_s, tr("Alcohol tax %"), u"percent"_s),
            field(u"merchandise"_s, tr("Merchandise tax %"), u"percent"_s),
            field(u"room"_s, tr("Room tax %"), u"percent"_s),
            field(u"taxTakeoutFood"_s, tr("Tax takeout food"), u"bool"_s),
            field(u"taxGratuity"_s, tr("Tax the automatic gratuity"), u"bool"_s,
                  tr("A charge the guest can't decline is taxable in California and Florida (on when the state is "
                     "chosen) and in most states under conditions; tips the guest chooses never are. Ask your accountant.")),
            with(field(u"cashRounding"_s, tr("Cash rounding"), u"enum"_s,
                       tr("Where pennies are gone: checks paid in cash are rounded to the nearest 5 or 10 cents. Cards are exact.")),
                 u"options"_s, options({{"0", "None (to the cent)"}, {"5", "Nearest 5 cents"}, {"10", "Nearest 10 cents"}})),
        };
    }
    if (panel == u"store") {
        return storeSections({
            field(u"storeName"_s, tr("Store name"), u"string"_s),
            with(field(u"language"_s, tr("Language"), u"enum"_s,
                       tr("For screens nobody is logged in to, the customer display, receipts and tickets. "
                          "Each person can have their own (Employees).")),
                 u"options"_s, languageOptions(false)),
            with(with(field(u"startPage.server"_s, tr("Servers start on"), u"page"_s,
                       tr("The floor plan each job sees first after login (a room drawn with a table map in the editor counts too). "
                          "Each person can have their own (Employees).")),
                 u"emptyText"_s, tr("(the floor plan)")), u"pages"_s, u"floor"_s),
            with(with(field(u"startPage.bartender"_s, tr("Bartenders start on"), u"page"_s), u"emptyText"_s, tr("(the floor plan)")), u"pages"_s, u"floor"_s),
            with(with(field(u"startPage.cashier"_s, tr("Cashiers start on"), u"page"_s), u"emptyText"_s, tr("(the floor plan)")), u"pages"_s, u"floor"_s),
            with(with(field(u"startPage.host"_s, tr("Hosts start on"), u"page"_s), u"emptyText"_s, tr("(the seating screen)")), u"pages"_s, u"floor"_s),
            with(with(field(u"startPage.manager"_s, tr("Managers start on"), u"page"_s), u"emptyText"_s, tr("(the floor plan)")), u"pages"_s, u"floor"_s),
            field(u"currencySymbol"_s, tr("Currency symbol"), u"string"_s),
            field(u"receiptHeader"_s, tr("Receipt header"), u"text"_s, tr("Address, phone… one per line")),
            field(u"receiptFooter"_s, tr("Receipt footer"), u"text"_s),
            field(u"receiptFreeChoices"_s, tr("Free choices on receipts"), u"bool"_s,
                  tr("Off: the guest's receipt shows only choices that change the price (Extra bacon), not No onion or Medium rare. Kitchen tickets show them all.")),
            field(u"soldOutBackNextDay"_s, tr("86'd items come back the next day"), u"bool"_s,
                  tr("On: items marked sold out during service are back after End of Day. Off: until someone turns them back on. Marked sold out in the Menu Builder: always until changed there.")),
            with(field(u"cashMode"_s, tr("Cash handling"), u"enum"_s,
                       tr("Server bank: whoever takes cash keeps it and turns it in at check out, so anyone can "
                          "use any terminal. Drawer: each terminal has a cash drawer.")),
                 u"options"_s, options({{"serverBank", "Server bank (each person carries their own cash)"},
                                        {"drawer", "Cash drawer on each terminal"}})),
            field(u"terminalsHaveDrawer"_s, tr("Terminals have a cash drawer"), u"bool"_s,
                  tr("Unless set for a terminal in Terminals.")),
            field(u"paidBreaks"_s, tr("Breaks are paid"), u"bool"_s, tr("Otherwise break time isn't counted as worked.")),
            with(with(field(u"checksPerServerHour"_s, tr("Checks a server takes care of in an hour"), u"int"_s,
                            tr("About how many. Reports -> Forecast then says how many servers each hour needs.")),
                      u"min"_s, 1), u"max"_s, 50),
            with(field(u"laborState"_s, tr("The store's state"), u"enum"_s,
                       tr("Sets its overtime, double time, meal break and gift card rules below (federal overtime "
                          "past 40 hours a week applies everywhere). They're the usual rules for restaurant staff: "
                          "check special cases with your accountant.")),
                 u"options"_s, [this] {
                     QVariantList o = stateOptions();
                     o[0] = QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(not chosen: federal rules only)")}};
                     return o;
                 }()),
            with(with(field(u"doubleTimeDailyHours"_s, tr("Double time after hours in a day"), u"int"_s,
                            tr("0 = none (12 in California)")), u"min"_s, 0), u"max"_s, 24),
            field(u"seventhDayRule"_s, tr("Seventh day in a row: time and a half, double past 8 hours"), u"bool"_s,
                  tr("California: the seventh day worked in a row in the pay week.")),
            with(with(field(u"mealBreakAfterHours"_s, tr("A meal break is due by (hours on the clock)"), u"int"_s,
                            tr("0 = none. The person is reminded half an hour before, and Store Check shows who's late (5 in California).")),
                      u"min"_s, 0), u"max"_s, 12),
            with(with(field(u"restBreakEveryHours"_s, tr("A paid 10-minute rest break every (hours worked)"), u"int"_s,
                            tr("0 = none (4 in California, Colorado, Kentucky, Minnesota, Nevada, Oregon and Washington). "
                               "The person is reminded at login. Breaks under 20 minutes are always paid.")),
                      u"min"_s, 0), u"max"_s, 8),
            with(with(field(u"scheduleNoticeDays"_s, tr("Post the schedule this many days ahead (fair workweek)"), u"int"_s,
                            tr("0 = no such law here. 14 in New York City, Chicago, Seattle, Philadelphia, San Francisco, "
                               "Los Angeles and Oregon (for the larger employers each names). Shifts changed inside it are "
                               "logged in Reports -> Changes: predictability pay may be owed.")),
                      u"min"_s, 0), u"max"_s, 31),
            with(with(field(u"restBetweenShiftsHours"_s, tr("Hours off between shifts, at least"), u"int"_s,
                            tr("0 = none. 10 under most fair workweek laws, 11 in New York City: a closing shift and the next "
                               "morning's closer than that are warned about and marked.")),
                      u"min"_s, 0), u"max"_s, 16),
            with(field(u"giftCardCashBelow"_s, tr("Cash out gift card balances under ($)"), u"money"_s,
                       tr("When the guest asks (California: under $10). 0 = none.")), u"min"_s, 0),
            with(with(field(u"tipAllocation"_s, tr("Tip allocation rate, IRS Form 8027 (%)"), u"percent"_s,
                            tr("8, unless the IRS has approved a lower rate for the store (2 at least). Reports -> IRS 8027.")),
                      u"min"_s, 2), u"max"_s, 8),
            with(with(field(u"overtimeDailyHours"_s, tr("Overtime after hours in a day"), u"int"_s,
                            tr("0 = no daily rule (e.g. 8 in California)")), u"min"_s, 0), u"max"_s, 24),
            with(with(field(u"overtimeWeeklyHours"_s, tr("Overtime after hours in a week"), u"int"_s,
                            tr("0 = no weekly rule (40 under US federal law)")), u"min"_s, 0), u"max"_s, 168),
            with(field(u"weekStartsOn"_s, tr("Pay week starts on"), u"enum"_s), u"options"_s,
                 options({{"0", "Sunday"}, {"1", "Monday"}, {"2", "Tuesday"}, {"3", "Wednesday"},
                          {"4", "Thursday"}, {"5", "Friday"}, {"6", "Saturday"}})),
            field(u"scheduleRequired"_s, tr("Staff clock in only on their schedule"), u"bool"_s,
                  tr("From a little before a scheduled shift until it ends. Managers can always clock in, and can clock "
                     "someone in from Manager → Schedule.")),
            with(with(field(u"clockInEarlyMinutes"_s, tr("…clock in up to (minutes) early"), u"int"_s), u"min"_s, 0),
                 u"max"_s, 240),
            asRows(field(u"tipOuts"_s, tr("Tip-outs"), u"text"_s,
                         tr("A share of each server's tips (or sales), split by hours among everyone in that job who "
                            "worked today.")),
                   tr("+ Add a tip-out"),
                   {column(tr("Goes to"), "enum", 0,
                           options({{"busser", "Bussers"}, {"bartender", "Bartenders"}, {"host", "Hosts"},
                                    {"server", "Servers"}, {"cashier", "Cashiers"}})),
                    column(tr("Percent"), "number", 100, {}, u"15"_s),
                    column(tr("Of"), "enum", 150, options({{"tips", "of tips"}, {"sales", "of sales"}}))},
                   uR"(^\s*(\S+)\s+([\d.]+)%?\s*(\S*)\s*$)"_s, u"%1 %2 %3"_s),
            // The tip pool: each job's share for an hour worked.
            with(asRows(field(u"tipPool"_s, tr("Tip pool"), u"text"_s,
                              tr("The jobs in the pool. Everyone in it puts in their tips (after tip-outs) and takes out by "
                                 "hours worked times their job's share: 100 a full share, 50 half. A job not here isn't in "
                                 "the pool. Someone's own setting (Staff) can put them in or keep them out.")),
                        tr("+ Add a job"),
                        {with(column(tr("Job"), "enum", 0,
                                     options({{"server", "Servers"}, {"bartender", "Bartenders"}, {"busser", "Bussers"},
                                              {"host", "Hosts"}, {"cashier", "Cashiers"}, {"driver", "Drivers"}})),
                              u"unique"_s, true),
                         column(tr("Share an hour (%)"), "int", 180, {}, u"100"_s)},
                        uR"(^\s*(\S+)\s+(\d+)\s*$)"_s, u"%1 %2"_s),
                 u"max"_s, 6),
            field(u"declareCashTips"_s, tr("Ask for cash tips at clock out"), u"bool"_s,
                  tr("They count with card tips in the Tips report and the tip pool.")),
            with(with(field(u"kitchenWarnMinutes"_s, tr("Kitchen: ticket turns yellow after (minutes)"), u"int"_s),
                      u"min"_s, 1), u"max"_s, 120),
            with(with(field(u"kitchenLateMinutes"_s, tr("Kitchen: ticket is late (red) after (minutes)"), u"int"_s,
                            tr("Late tickets are counted in the Kitchen report.")), u"min"_s, 1), u"max"_s, 240),
            asRows(field(u"expoSkips"_s, tr("Expo doesn't show"), u"text"_s,
                         tr("Stations or printers (Bar, Fryer...) whose items the expediter doesn't run: drinks go out "
                            "from the bar. Running a ticket leaves them on their own screen.")),
                   tr("+ Add a station or printer")),
            with(with(field(u"laterLeadMinutes"_s, tr("Orders for later go to the kitchen (minutes before)"), u"int"_s,
                            tr("An order for 6:30 with 20 here reaches the kitchen at 6:10.")), u"min"_s, 0), u"max"_s, 240),
            field(u"sendToTables"_s, tr("After Send, back to the tables"), u"bool"_s,
                  tr("Send on a table's check goes back to the floor plan, ready for the next table (bar tabs and "
                     "takeout stay on the order).")),
            field(u"requireOrderName"_s, tr("Phone orders need a name before Send"), u"bool"_s,
                  tr("Takeout and delivery: a name (and an address for deliveries) before the kitchen gets it. "
                     "Each person and each screen can say otherwise (Employees, Terminals).")),
            with(with(field(u"takeoutMinutes"_s, tr("Takeout is usually ready in (minutes)"), u"int"_s,
                            tr("The quote on a takeout check, when the kitchen isn't busy.")), u"min"_s, 0), u"max"_s, 240),
            with(with(field(u"deliveryMinutes"_s, tr("Deliveries usually arrive in (minutes)"), u"int"_s),
                      u"min"_s, 0), u"max"_s, 240),
            with(with(field(u"minutesPerOrderWaiting"_s, tr("Add to the quote for each order cooking (minutes)"), u"int"_s,
                            tr("With 15 and 2 here, 5 orders in the kitchen make it 25.")), u"min"_s, 0), u"max"_s, 60),
            field(u"deliveryFee"_s, tr("Delivery fee"), u"money"_s,
                  tr("Added to every delivery (0 = none). Taking it off is a void (a manager).")),
            field(u"deliveryFeeToDriver"_s, tr("The delivery fee goes to the driver"), u"bool"_s,
                  tr("Receipts say whether it does: a fee may not look like the driver's tip (California and others).")),
            // Never shown again once saved: only its last four.
            field(u"stripeSecretKey"_s, tr("Stripe secret key (card readers)"), u"password"_s,
                  s_->settings.stripeSecretKey.empty()
                      ? tr("From the Stripe Dashboard (Developers -> API keys): sk_live_... or a restricted rk_... key. "
                           "It stays on this computer; readers get short-lived tokens from it.")
                      : tr("Set (ends in %1). Type a new key to change it, or \"none\" to remove it.")
                            .arg(qs(s_->settings.stripeSecretKey.substr(s_->settings.stripeSecretKey.size() - 4)))),
            with(field(u"cardCurrency"_s, tr("Card currency"), u"enum"_s, tr("What cards are charged in.")),
                 u"options"_s, [this] {
                     QVariantList o = options({{"usd", "US dollar (usd)"}, {"cad", "Canadian dollar (cad)"},
                                               {"mxn", "Mexican peso (mxn)"}, {"eur", "Euro (eur)"},
                                               {"gbp", "British pound (gbp)"}, {"aud", "Australian dollar (aud)"},
                                               {"nzd", "New Zealand dollar (nzd)"}});
                     const QString now = qs(s_->settings.cardCurrency);
                     if (!now.isEmpty() && std::ranges::none_of(o, [&](const QVariant &x) { return x.toMap()[u"value"_s] == now; }))
                         o.append(QVariantMap{{u"value"_s, now}, {u"text"_s, now}});
                     return o;
                 }()),
            field(u"offlineCards"_s, tr("Take cards when the internet is down"), u"bool"_s,
                  tr("On Stripe readers running ViewTouch: the card is kept and sent to Stripe when the internet is back. "
                     "The bank only sees it then: if it declines, the store doesn't get that money (a manager is told). "
                     "Turning this on also turns on Stripe's offline mode for the store's readers.")),
            field(u"offlineCardMax"_s, tr("Offline: up to this much a card"), u"money"_s,
                  tr("A bigger payment needs the internet.")),
            field(u"offlineCardTotal"_s, tr("Offline: up to this much waiting in all"), u"money"_s,
                  tr("Past that, cards wait for the internet.")),
            field(u"surchargePercent"_s, tr("Credit card surcharge %"), u"percent"_s,
                  tr("Added on top when a card reader says the card is a credit card; never on debit or prepaid cards "
                     "(the law). Up to 3% (2% in Colorado); not allowed in Connecticut, Massachusetts or Maine. Post a "
                     "sign at the door and the register, and tell your card processor first. 0 = none.")),
            field(u"cashDiscountPercent"_s, tr("Cash discount %"), u"percent"_s,
                  tr("Dual pricing: the menu's prices are the card prices, and a check paid all in cash takes this much off. "
                     "The Pay screen and receipts show both prices. 0 = none.")),
            field(u"tabHold"_s, tr("Bar tabs: hold the guest's card for"), u"money"_s,
                  tr("Set aside on the card when a tab opens (nothing is charged); charged for the tab when it closes. 0 = tabs don't hold cards.")),
            with(field(u"cardTipOn"_s, tr("Card tips are asked on"), u"enum"_s,
                       tr("The card reader's screen needs tipping turned on in Stripe (Dashboard -> Terminal -> Configurations).")),
                 u"options"_s, options({{"", "The customer display (Ask Guest for Tip)"}, {"reader", "The card reader beside the register"}})),
            with(with(field(u"screenSaverMinutes"_s, tr("Dim the screen after (minutes)"), u"int"_s,
                            tr("Untouched screens dim; a touch wakes them (and does nothing else). 0 = never. "
                               "Kitchen, bar and expo screens stay on.")), u"min"_s, 0), u"max"_s, 240),
            with(with(field(u"autoLogoutMinutes"_s, tr("Log out idle screens after (minutes)"), u"int"_s,
                            tr("0 = never. Kitchen, bar and expo screens don't need a login.")), u"min"_s, 0), u"max"_s, 120),
            with(with(field(u"phoneLockSeconds"_s, tr("Handhelds: ask for the PIN after the screen was off (seconds)"), u"int"_s,
                            tr("Phones and handhelds let their screen sleep; when it was off this long, the next touch asks "
                               "for a PIN (their check comes back). 0 = every time, -1 = never.")), u"min"_s, -1), u"max"_s, 3600),
            field(u"screensUpdate"_s, tr("Phones and tablets get updates by themselves"), u"bool"_s,
                  tr("While nobody is logged in on one, it gets the newer app from this computer; "
                     "the next person to use it touches Install. Off: the maintainer updates each one.")),
            with(with(field(u"tableLongMinutes"_s, tr("Mark tables seated longer than (minutes)"), u"int"_s),
                      u"min"_s, 10), u"max"_s, 600),
            field(u"loyaltyEnabled"_s, tr("Loyalty points"), u"bool"_s,
                  tr("Customers on a check earn points on what they spend (after discounts).")),
            with(with(field(u"pointsPerDollar"_s, tr("…points per dollar"), u"int"_s), u"min"_s, 1), u"max"_s, 100),
            asRows(field(u"rewards"_s, tr("Rewards"), u"text"_s,
                         tr("With that many points, a customer can take that much off a check.")),
                   tr("+ Add a reward"),
                   {column(tr("Points"), "int", 140, {}, u"50"_s), column(tr("Amount off"), "money", 160, {}, u"5.00"_s)},
                   uR"(^\s*(\d+)\s*=\s*\D?\s*([\d.]+)\s*$)"_s, u"%1 = %2"_s),
            field(u"displayLogo"_s, tr("Store logo"), u"image"_s,
                  tr("On the login page, the screen saver, receipts (below), the customer display and the "
                     "kiosk. Empty: the store's name.")),
            field(u"receiptLogo"_s, tr("Print the logo on receipts"), u"bool"_s,
                  tr("At the top of every receipt, on thermal (ESC/POS) printers.")),
            field(u"displayAccent"_s, tr("Customer display: color"), u"color"_s),
            asRows(field(u"displaySlides"_s, tr("Customer display: between guests"), u"text"_s,
                         tr("Shown in turn; \"image:/path/to/photo.jpg\" for a picture. Running promotions are added "
                            "by themselves.")),
                   tr("+ Add a message")),
            with(with(with(asRows(field(u"tipPercents"_s, tr("Tip choices for guests (%)"), u"text"_s,
                                        tr("Shown on the customer display, up to 6.")),
                                  tr("+ Add a tip choice"), {column(tr("Percent"), "int", 120, {}, u"18"_s)},
                                  uR"(^\s*(\d+))"_s, u"%1"_s),
                           u"split"_s, u"\\s*,\\s*"_s), u"join"_s, u", "_s), u"max"_s, 6),
            field(u"hostsTakePayments"_s, tr("Hosts take payments at the host stand"), u"bool"_s,
                  tr("When a server is busy, the host touches the table and Pay This Table. Hosts can then open "
                     "any check to take its payment. Turn it off for one host, or on for one, in Employees.")),
            with(with(field(u"waitMinutesPerParty"_s, tr("Waitlist: minutes per party ahead"), u"int"_s,
                            tr("Wait quotes: (parties ahead + 1) x this, rounded up to 5 minutes.")), u"min"_s, 1), u"max"_s, 120),
            field(u"tableReadyText"_s, tr("Waitlist: table-ready text"), u"text"_s,
                  tr("Sent when the host touches Notify. {name} and {store} are filled in.")),
            field(u"textWebhook"_s, tr("Texting service URL"), u"text"_s,
                  tr("Texts are POSTed here as JSON {\"to\", \"message\"} (your SMS provider or a relay). Empty: no texts; the host tells the guest.")),
            field(u"backupCopyDir"_s, tr("Also copy backups to"), u"text"_s,
                  tr("A USB drive or network folder on the server, e.g. /media/usb/viewtouch. Empty = no second copy.")),
            field(u"royaltyPercent"_s, tr("Royalty (% of net sales)"), u"string"_s,
                  tr("A franchise's fee, e.g. 5 or 4.5. Reports -> Royalty works it out. Empty or 0: none.")),
            field(u"adFundPercent"_s, tr("Advertising fund (% of net sales)"), u"string"_s, tr("e.g. 2. Empty or 0: none.")),
            asRows(field(u"chartOfAccounts"_s, tr("Chart of accounts"), u"text"_s,
                         tr("For Reports -> Accounting (its CSV goes to QuickBooks or your accountant): the account "
                            "each kind of money goes to.")),
                   tr("+ Add an account"),
                   {column(tr("Money"), "enum", 260, accountKeys()), column(tr("Account"), "text", 0, {}, u"4010 Food sales"_s)},
                   uR"(^\s*([^=]+?)\s*=\s*(.*)$)"_s, u"%1 = %2"_s),
            with(with(field(u"extraPercent"_s, tr("Extra: percent added"), u"int"_s,
                            tr("What \"Extra\" adds to an item's or a choice's price: 50 = half again. 0 = nothing.")),
                      u"min"_s, 0), u"max"_s, 500),
            field(u"extraCharge"_s, tr("Extra: amount added"), u"money"_s,
                  tr("And/or a fixed amount for Extra, e.g. 0.75. Charged even on choices that are free.")),
            with(with(field(u"targetFoodCostPercent"_s, tr("Food cost to stay under (% of the price)"), u"int"_s,
                            tr("A delivery at higher prices says which items went over; Reports -> Food Cost suggests a price.")),
                      u"min"_s, 5), u"max"_s, 90),
            field(u"stockCountShowsExpected"_s, tr("Show what should be there while counting stock"), u"bool"_s,
                  tr("Off: whoever counts sees it only in the Stock Count report, after, so it can't sway the count.")),
            with(field(u"labelPrinter"_s, tr("Prep labels print on"), u"enum"_s,
                       tr("A label printer (Manager → Printers: format ZPL or TSPL), or a receipt printer for slips. "
                          "Each screen can say otherwise (Terminals).")),
                 u"options"_s, ([this](const QString &none) {
                     QVariantList o{QVariantMap{{u"value"_s, QString()}, {u"text"_s, none}}};
                     for (const PrinterConfig &pc : s_->settings.printers)
                         o.append(QVariantMap{{u"value"_s, qs(pc.id)}, {u"text"_s, qs(pc.name)}});
                     return o;
                 })(tr("The first label printer"))),
            asRows(field(u"wasteReasons"_s, tr("Reasons for waste"), u"text"_s, tr("Log Waste asks which one.")),
                   tr("+ Add a reason")),
            field(u"askReasons"_s, tr("Ask why on voids and comps"), u"bool"_s,
                  tr("Voiding an item already sent, or a discount or comp, asks for a reason from the lists below. "
                     "The Exceptions report adds them up. Can be set per employee.")),
            asRows(field(u"voidReasons"_s, tr("Reasons for voids"), u"text"_s), tr("+ Add a reason")),
            asRows(field(u"compReasons"_s, tr("Reasons for discounts and comps"), u"text"_s), tr("+ Add a reason")),
            asRows(field(u"openingChecklist"_s, tr("Opening checklist"), u"text"_s,
                         tr("Staff tick them off on the Checklists page.")),
                   tr("+ Add a task")),
            asRows(field(u"closingChecklist"_s, tr("Closing checklist"), u"text"_s, tr("End of Day shows what's left.")),
                   tr("+ Add a task")),
            asRows(field(u"kitchenStations"_s, tr("Kitchen stations"), u"text"_s,
                         tr("Grill, Fryer, Cold Line… Each kitchen screen picks one with its Station button; menu items "
                            "say where they're made.")),
                   tr("+ Add a station")),
            asRows(field(u"expenseCategories"_s, tr("Expense categories"), u"text"_s,
                         tr("What cash paid out of a drawer can be for (Produce, Ice, Repairs...). Each pay out picks "
                            "one; Reports -> Expenses adds them up.")),
                   tr("+ Add a category")),
            field(u"kioskSendNow"_s, tr("Send orders to the kitchen at once"), u"bool"_s,
                  tr("Otherwise a kiosk order goes to the kitchen when it is paid for at the counter.")),
            field(u"kioskSlip"_s, tr("Print an order slip"), u"bool"_s,
                  tr("On the kiosk's receipt printer (Terminals) when the order is placed: the order number "
                     "to show at the counter, the name, what was ordered and the total.")),
            with(with(field(u"kioskIdleSeconds"_s, tr("Clear an untouched order after (seconds)"),
                            u"int"_s), u"min"_s, 30), u"max"_s, 600),
            field(u"kioskWelcome"_s, tr("Welcome button"), u"string"_s,
                  tr("The words on the pictures screen. Empty: \"Touch to Order\".")),
            field(u"kioskAskWhere"_s, tr("Ask For Here or To Go"), u"bool"_s,
                  tr("Off: every kiosk order is for here.")),
            field(u"kioskAskName"_s, tr("Ask for a name"), u"bool"_s,
                  tr("Off: orders are called by number only.")),
            field(u"kioskEasyReach"_s, tr("Easy Reach button"), u"bool"_s,
                  tr("Brings everything down within reach of a seated guest.")),
            with(with(field(u"kioskSizePercent"_s, tr("Size of buttons and text (%)"), u"int"_s,
                            tr("100 = as designed for the screen; bigger for a far-away or tall screen.")),
                      u"min"_s, 60), u"max"_s, 200),
            with(field(u"kioskBackground"_s, tr("Background color"), u"color"_s,
                  tr("Empty: the usual. The accent color and logo are the customer display's.")), u"emptyText"_s, tr("The usual")),
            with(field(u"kioskCard"_s, tr("Button and card color"), u"color"_s, tr("Empty: the usual.")), u"emptyText"_s, tr("The usual")),
            with(field(u"kioskGo"_s, tr("Add / Place Order color"), u"color"_s, tr("Empty: the usual.")), u"emptyText"_s, tr("The usual")),
            with(field(u"kioskText"_s, tr("Text color"), u"color"_s, tr("Empty: the usual.")), u"emptyText"_s, tr("The usual")),
            field(u"kioskFont"_s, tr("Font"), u"string"_s, tr("A font installed on the kiosk, e.g. DejaVu Sans.")),
            field(u"encryptBackups"_s, tr("Encrypt backups"), u"bool"_s,
                  tr("Backups (and the second copy) can only be opened with the backup password. "
                     "For a lost or stolen USB drive.")),
            field(u"backupPassword"_s, tr("Backup password"), u"password"_s,
                  tr("At least 8 characters. Write it down somewhere safe: without it, an encrypted backup can't be "
                     "restored on another computer. Changing it keeps older backups on the old one. "
                     "Leave empty to keep the current password.")),
            field(u"checkoutNeedsClosedChecks"_s, tr("Close all checks before checking out"), u"bool"_s,
                  tr("Servers must close or hand over their checks before they check out their bank. "
                     "Can be set per employee.")),
            field(u"blindDrawerCount"_s, tr("Count drawers blind"), u"bool"_s,
                  tr("Whoever counts a drawer or bank doesn't see what should be in it until it's counted, nor "
                     "whether it came out over or short. Managers still see it. Can be set per employee.")),
            with(with(asRows(field(u"cashDenominations"_s, tr("Bills and coins"), u"text"_s,
                                   tr("Counting a drawer: how many of each. Largest first.")),
                             tr("+ Add a bill or coin"), {column(tr("Worth"), "money", 140)},
                             uR"(^\s*\D?\s*([\d.]+))"_s, u"%1"_s),
                      u"split"_s, u"\\s*,\\s*"_s), u"join"_s, u", "_s),
            field(u"gratuityPercent"_s, tr("Party gratuity %"), u"percent"_s, tr("Added to large tables. 0 = off.")),
            with(with(field(u"gratuityMinGuests"_s, tr("…for tables of at least"), u"int"_s), u"min"_s, 1), u"max"_s, 99),
            // About: read only. "screen.*" is filled in by each screen (its own build).
            field(u"about.version"_s, tr("ViewTouch version"), u"info"_s, tr("The program running the store: its version, build number and commit.")),
            field(u"screen.version"_s, tr("This screen's version"), u"info"_s,
                  tr("A tablet or another screen can run an older build until it updates.")),
            field(u"screen.display"_s, tr("This screen"), u"info"_s),
            field(u"about.machine"_s, tr("Store computer"), u"info"_s),
            field(u"about.started"_s, tr("Running since"), u"info"_s),
            field(u"about.database"_s, tr("Data kept in"), u"info"_s),
            field(u"about.backup"_s, tr("Last backup"), u"info"_s),
            field(u"about.system"_s, tr("System"), u"info"_s),
            field(u"about.updates"_s, tr("Updates on this store"), u"info"_s,
                  tr("Each version this store has run, and when it started here.")),
            field(u"about.history"_s, tr("Recent changes"), u"info"_s,
                  tr("What the latest builds changed, newest first. The version's last number is the build: it goes up by itself.")),
            field(u"about.license"_s, tr("License"), u"info"_s),
            field(u"about.source"_s, tr("Source code and manual"), u"info"_s),
            field(u"about.credits"_s, tr("ViewTouch"), u"info"_s),
        });
    }
    if (panel == u"modifierGroups") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s, tr("What the server sees, e.g. Temperature")), readonlyId,
            with(with(field(u"min"_s, tr("Choices required"), u"int"_s, tr("0 = optional")), u"min"_s, 0), u"max"_s, 20),
            with(with(field(u"max"_s, tr("Most choices allowed"), u"int"_s,
                            tr("1 = pick one (a new choice replaces it), 0 = any number")), u"min"_s, 0), u"max"_s, 20),
            field(u"options"_s, tr("Options"), u"text"_s,
                  tr("One per line; a price after +, e.g. \"Onion Rings + 1.50\". After |, what the kitchen sees "
                     "(\"Ranch | RNCH\"), or - to leave it off the kitchen ticket (\"No dressing | -\"). "
                     "Start with * what comes on the item (\"*Onion\"): guests can have it No, Light, Extra or "
                     "on the side.")),
            field(u"askHow"_s, tr("Guests can ask for Light, Extra or On the side"), u"bool"_s,
                  tr("On the kiosk, a choice can be had Light, Extra or on the side (dressing on the side, "
                     "extra bacon). Off for temperatures and sizes.")),
            field(u"menuItems"_s, tr("Options are menu items"), u"bool"_s,
                  tr("For combos: each option is the menu item of that name (\"Soda\", \"Fries\"). It uses up "
                     "that item's stock, can't be chosen while it's sold out, and shows on the Items report.")),
        };
    }
    if (panel == u"mealPeriods") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            field(u"start"_s, tr("Starts at"), u"string"_s,
                  tr("Like 11:00 AM. Runs until the next period starts.")),
        };
    }
    if (panel == u"terminals") {
        QVariantList printers = options({{"", "Receipt (default)"}, {"ask", "Ask each time (handhelds)"}});
        for (const PrinterConfig &p : s_->settings.printers)
            printers.append(QVariantMap{{u"value"_s, qs(p.id)}, {u"text"_s, qs(p.name)}});
        return {
            field(u"name"_s, tr("Terminal name"), u"string"_s, tr("As given with --terminal (this one: %1)").arg(terminal_)),
            with(field(u"receiptPrinter"_s, tr("Receipts and cash drawer on"), u"enum"_s), u"options"_s, printers),
            with(field(u"drawer"_s, tr("Cash drawer"), u"enum"_s,
                       tr("Store setting: %1.").arg(s_->settings.terminalsHaveDrawer ? tr("has a drawer") : tr("no drawer"))),
                 u"options"_s, options({{"", "Store setting"}, {"yes", "Has a cash drawer"}, {"no", "No cash drawer"}})),
            with(field(u"screen"_s, tr("Screen layout"), u"enum"_s,
                       tr("Phone pages: big buttons in portrait, for phones and small handhelds.")),
                 u"options"_s, options({{"", "Automatic (phone pages on phones)"}, {"standard", "Standard pages"},
                                        {"phone", "Phone pages"}, {"selfOrder", "Self-order kiosk (guests order on their own)"},
                                        {"timeClock", "Time Clock (clock in / out and schedules only)"}})),
            // Choices filled in by the screen (its looks).
            field(u"look"_s, tr("Look"), u"look"_s, tr("This screen's colors; the store's look elsewhere.")),
            with(field(u"textSize"_s, tr("Text size"), u"enum"_s,
                       tr("Bigger button and check text on this screen (one read from afar, a bright kitchen). "
                          "A person's own text size wins while they're logged in.")),
                 u"options"_s, options({{"", "Normal"}, {"115", "Bigger"}, {"130", "Biggest"}, {"150", "Largest"}})),
            field(u"colorSafe"_s, tr("Colors for color blindness"), u"bool"_s,
                  tr("Table and kitchen ticket colors that color-blind eyes tell apart (blue, yellow, orange) on this screen, "
                     "whoever is logged in. A kitchen screen nobody logs into, say.")),
            with(field(u"labelPrinter"_s, tr("Prep labels print on"), u"enum"_s,
                       tr("The printer by this screen for prep labels. Empty: the store's (Store Settings → Inventory).")),
                 u"options"_s, ([this](const QString &none) {
                     QVariantList o{QVariantMap{{u"value"_s, QString()}, {u"text"_s, none}}};
                     for (const PrinterConfig &pc : s_->settings.printers)
                         o.append(QVariantMap{{u"value"_s, qs(pc.id)}, {u"text"_s, qs(pc.name)}});
                     return o;
                 })(tr("The store's label printer"))),
            with(with(field(u"startPage"_s, tr("Start screen"), u"page"_s,
                       tr("What this device shows after a login: the patio handheld the Patio floor plan, the takeout "
                          "tablet Open Checks. Empty: each person's or their job's.")),
                 u"emptyText"_s, tr("(each person's)")), u"pages"_s, u"start"_s),
            with(field(u"startCategory"_s, tr("Start orders on"), u"enum"_s,
                       tr("A menu category orders opened here start on (the bar: Drinks). The meal's categories: the usual.")),
                 u"options"_s, [this] {
                     QVariantList o{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("The meal's categories")}}};
                     for (const MenuCategory &c : s_->categories())
                         o.append(QVariantMap{{u"value"_s, qs(c.id)}, {u"text"_s, qs(c.name)}});
                     return o;
                 }()),
            with(field(u"keyboard"_s, tr("On-screen keyboard"), u"enum"_s,
                       tr("For typing names, notes and numbers on a touch screen. Phones and tablets use their own.")),
                 u"options"_s, options({{"", "Automatic (on; phones and tablets use their own)"}, {"on", "On"},
                                        {"off", "Off (this screen has a keyboard)"}})),
            with(field(u"requireName"_s, tr("Phone orders need a name before Send"), u"enum"_s,
                       tr("At this screen, whoever is taking the order. Empty: the person's setting, else the store's.")),
                 u"options"_s, options({{"", "The person's / store setting"}, {"yes", "Yes"}, {"no", "No"}})),
            with(field(u"afterPaying"_s, tr("After a check is paid"), u"enum"_s,
                       tr("Ask: print (choosing the printer), email (cards from a Stripe reader) or no receipt.")),
                 u"options"_s, options({{"", "Nothing (Print Receipt when asked)"}, {"print", "Print a receipt"},
                                        {"ask", "Ask: print, email or no receipt"}})),
            with(field(u"cardReader"_s, tr("Card reader"), u"enum"_s,
                       tr("Stripe: this app runs on a Stripe Reader S700 (Apps on Devices) and takes cards on it; "
                          "the store needs its Stripe secret key (Store Settings). Simulated: approves after a moment, "
                          "for practice (cents ending in 05 are declined).")),
                 u"options"_s, [&] {
                     QVariantList o = options({{"", "None (card payments are typed in)"},
                                               {"stripe", "Stripe smart reader (this device)"},
                                               {"simulated", "Simulated (practice)"}});
                     for (const PosSettings::StripeReader &r : s_->settings.stripeReaders)
                         o.append(QVariantMap{{u"value"_s, u"counter:"_s + qs(r.id)},
                                              {u"text"_s, tr("Reader beside it: %1").arg(qs(r.label))}});
                     return o;
                 }()),
        };
    }
    return {};
}

} // namespace vt::app
