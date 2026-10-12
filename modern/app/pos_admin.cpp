// PosService: manager screens for the menu, staff, payment types, printers,
// taxes and store settings. Records travel to QML as maps; the field lists
// use the same schema format as the page editor's inspector.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include <QCoreApplication>
#include "app/pos_service.hh"

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QVariantMap field(const QString &path, const QString &label, const QString &type, const QString &hint = {})
{
    QVariantMap f{{u"path"_s, path}, {u"label"_s, label}, {u"type"_s, type}, {u"group"_s, QString()}};
    if (!hint.isEmpty())
        f.insert(u"hint"_s, hint);
    return f;
}

QVariantMap with(QVariantMap f, const QString &key, const QVariant &value)
{
    f.insert(key, value);
    return f;
}

QVariantList options(std::initializer_list<std::pair<const char *, const char *>> values)
{
    QVariantList out;
    for (const auto &[value, text] : values)   // the texts are phrases too (tools/i18n.py finds them)
        out.append(QVariantMap{{u"value"_s, QString::fromUtf8(value)},
                               {u"text"_s, QCoreApplication::translate("PosService", text)}});
    return out;
}

// "Store setting" first for a person; each language named in itself.
QVariantList languageOptions(bool person)
{
    QVariantList out;
    if (person)
        out.append(QVariantMap{{u"value"_s, QString()}, {u"text"_s, QCoreApplication::translate("PosService", "Store setting")}});
    for (const i18n::Language &l : i18n::languages())
        out.append(QVariantMap{{u"value"_s, l.code}, {u"text"_s, l.name}});
    return out;
}

QString slug(const QString &text)
{
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QString s = text.toLower().normalized(QString::NormalizationForm_KD);
    s.replace(nonWord, u"-"_s);
    while (s.startsWith(u'-')) s.remove(0, 1);
    while (s.endsWith(u'-')) s.chop(1);
    return s.isEmpty() ? u"item"_s : s;
}

template <typename Container, typename IdOf>
QString uniqueId(const QString &wanted, const Container &items, IdOf idOf, int skipIndex)
{
    auto taken = [&](const QString &id) {
        for (int i = 0; i < int(items.size()); ++i) {
            if (i != skipIndex && qs(idOf(items[i])) == id)
                return true;
        }
        return false;
    };
    QString id = slug(wanted);
    for (int n = 2; taken(id); ++n)
        id = slug(wanted) + u'-' + QString::number(n);
    return id;
}

// "As the role" / "Yes" / "No" for one permission of one person.
QVariantMap permField(const char *permission, const QString &label)
{
    return with(with(field(u"perm:"_s + QString::fromLatin1(permission), label, u"enum"_s, u"Permissions"_s),
                     u"options"_s, options({{"", "As the role"}, {"allow", "Yes"}, {"deny", "No"}})),
                u"hint"_s, QString());
}

double number(const QVariantMap &r, const char16_t *key)
{
    return r.value(QString::fromUtf16(key)).toString().toDouble();
}

// The jobs that can be in the tip pool, in their usual order.
constexpr std::string_view kPoolJobs[] = {"server", "bartender", "busser", "host", "cashier", "driver"};

// A list kept as text, edited as rows (FieldEditor "rows"): each line read
// with `pattern` (a group a column) and written with `format`.
QVariantMap column(const QString &label, const char *type, int width = 0, const QVariantList &opts = {},
                   const QString &placeholder = {})
{
    QVariantMap c{{u"label"_s, label}, {u"type"_s, QString::fromLatin1(type)}};
    if (width > 0)
        c.insert(u"width"_s, width);
    if (!opts.isEmpty())
        c.insert(u"options"_s, opts);
    if (!placeholder.isEmpty())
        c.insert(u"placeholder"_s, placeholder);
    return c;
}

QVariantMap asRows(QVariantMap f, const QString &addText, const QVariantList &columns = {},
                   const QString &pattern = {}, const QString &format = {})
{
    f.insert(u"type"_s, u"rows"_s);
    f.insert(u"addText"_s, addText);
    if (!columns.isEmpty())
        f.insert(u"columns"_s, columns);
    if (!pattern.isEmpty())
        f.insert(u"pattern"_s, pattern);
    if (!format.isEmpty())
        f.insert(u"format"_s, format);
    return f;
}

// Store Settings in sections, in this order; within one, in the order given
// below. "when": shown only while that other setting is on (or not 0).
QVariantList storeSections(const QVariantList &fields)
{
    struct Section {
        const char *key;
        QString title;
        std::initializer_list<const char *> paths;
    };
    const Section sections[] = {
        {"store", PosService::tr("The store"), {"storeName", "language", "currencySymbol", "displayLogo"}},
        {"receipts", PosService::tr("Receipts"), {"receiptHeader", "receiptFooter", "receiptLogo", "receiptFreeChoices"}},
        {"startPages", PosService::tr("Start pages"),
         {"startPage.server", "startPage.bartender", "startPage.cashier", "startPage.host", "startPage.manager"}},
        {"cash", PosService::tr("Cash"), {"cashMode", "terminalsHaveDrawer", "checkoutNeedsClosedChecks", "expenseCategories"}},
        {"hours", PosService::tr("Staff hours"),
         {"paidBreaks", "overtimeDailyHours", "overtimeWeeklyHours", "weekStartsOn", "scheduleRequired", "clockInEarlyMinutes"}},
        {"tips", PosService::tr("Tips"),
         {"tipPercents", "gratuityPercent", "gratuityMinGuests", "declareCashTips", "tipOuts", "tipPool"}},
        {"menu", PosService::tr("Menu and prices"), {"extraPercent", "extraCharge", "soldOutBackNextDay"}},
        {"kitchen", PosService::tr("Kitchen"), {"kitchenStations", "kitchenWarnMinutes", "kitchenLateMinutes", "laterLeadMinutes"}},
        {"togo", PosService::tr("Takeout and delivery"),
         {"requireOrderName", "takeoutMinutes", "deliveryMinutes", "minutesPerOrderWaiting", "deliveryFee"}},
        {"cards", PosService::tr("Cards"),
         {"stripeSecretKey", "cardCurrency", "cardTipOn", "tabHold", "offlineCards", "offlineCardMax", "offlineCardTotal"}},
        {"screens", PosService::tr("Screens"), {"screenSaverMinutes", "autoLogoutMinutes", "tableLongMinutes"}},
        {"display", PosService::tr("Customer display"), {"displayAccent", "displaySlides"}},
        {"loyalty", PosService::tr("Loyalty"), {"loyaltyEnabled", "pointsPerDollar", "rewards"}},
        {"waitlist", PosService::tr("Waitlist and texts"), {"waitMinutesPerParty", "tableReadyText", "textWebhook"}},
        {"kiosk", PosService::tr("Self-order kiosk"),
         {"kioskSendNow", "kioskSlip", "kioskIdleSeconds", "kioskWelcome", "kioskAskWhere", "kioskAskName", "kioskEasyReach",
          "kioskSizePercent", "kioskBackground", "kioskCard", "kioskGo", "kioskText", "kioskFont"}},
        {"checklists", PosService::tr("Checklists"), {"openingChecklist", "closingChecklist"}},
        {"backups", PosService::tr("Backups"), {"backupCopyDir", "encryptBackups", "backupPassword"}},
        {"accounting", PosService::tr("Franchise and accounting"), {"royaltyPercent", "adFundPercent", "chartOfAccounts"}},
    };
    static const std::pair<const char *, const char *> when[] = {
        {"clockInEarlyMinutes", "scheduleRequired"}, {"gratuityMinGuests", "gratuityPercent"},
        {"offlineCardMax", "offlineCards"}, {"offlineCardTotal", "offlineCards"},
        {"pointsPerDollar", "loyaltyEnabled"}, {"rewards", "loyaltyEnabled"}, {"backupPassword", "encryptBackups"},
    };
    // Numbers: what 0 means, and a bigger step where 1 is too fine.
    struct Number {
        const char *path;
        QString zero;
        int step;
    };
    const QString never = PosService::tr("Never"), noRule = PosService::tr("No rule"), none = PosService::tr("None");
    const Number numbers[] = {
        {"overtimeDailyHours", noRule, 1}, {"overtimeWeeklyHours", noRule, 1},
        {"clockInEarlyMinutes", PosService::tr("On time"), 5},
        {"extraPercent", none, 5}, {"laterLeadMinutes", PosService::tr("At the time"), 5},
        {"takeoutMinutes", none, 5}, {"deliveryMinutes", none, 5}, {"minutesPerOrderWaiting", none, 1},
        {"screenSaverMinutes", never, 1}, {"autoLogoutMinutes", never, 1}, {"tableLongMinutes", {}, 5},
        {"kioskIdleSeconds", {}, 10}, {"kioskSizePercent", {}, 5}, {"waitMinutesPerParty", {}, 1},
    };
    QHash<QString, QVariantMap> byPath;
    for (const QVariant &f : fields)
        byPath.insert(f.toMap().value(u"path"_s).toString(), f.toMap());
    QVariantList out;
    const auto place = [&](QVariantMap f, const char *key, const QString &title) {
        f.insert(u"group"_s, title);
        f.insert(u"groupKey"_s, QString::fromLatin1(key));
        const QString p = f.value(u"path"_s).toString();
        for (const auto &[path, on] : when)
            if (p == QLatin1StringView(path))
                f.insert(u"when"_s, QString::fromLatin1(on));
        for (const Number &n : numbers) {
            if (p != QLatin1StringView(n.path))
                continue;
            if (!n.zero.isEmpty())
                f.insert(u"zeroText"_s, n.zero);
            f.insert(u"step"_s, n.step);
        }
        out.append(f);
    };
    for (const Section &sec : sections)
        for (const char *path : sec.paths)
            if (const auto it = byPath.constFind(QString::fromLatin1(path)); it != byPath.cend()) {
                place(*it, sec.key, sec.title);
                byPath.erase(it);
            }
    for (const QVariant &f : fields)   // any not given a section: last, under More
        if (byPath.contains(f.toMap().value(u"path"_s).toString()))
            place(f.toMap(), "more", PosService::tr("More"));
    return out;
}

} // namespace

QVariantList PosService::adminFields(const QString &panel)
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
                  tr("Like 2026-10-09 19:00. On the ticket and the receipt; it can't be sold after.")),
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
            field(u"start"_s, tr("From"), u"string"_s, tr("24-hour time, e.g. 15:00. From = Until: all day.")),
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
            field(u"cost"_s, tr("Cost per unit"), u"money"_s, tr("For the Food Cost report.")),
        };
    }
    if (panel == u"employees") {
        return {
            field(u"name"_s, tr("Name"), u"string"_s), readonlyId,
            with(field(u"role"_s, tr("Role"), u"enum"_s), u"options"_s,
                 options({{"server", "Server"}, {"bartender", "Bartender"}, {"cashier", "Cashier"}, {"host", "Host"},
                          {"busser", "Busser"}, {"driver", "Delivery driver"}, {"manager", "Manager"}, {"admin", "Admin"}})),
            field(u"pin"_s, tr("New PIN"), u"pin"_s, tr("4 to 8 digits. Leave empty to keep the current PIN.")),
            field(u"payRate"_s, tr("Pay rate ($ an hour)"), u"money"_s, tr("For their role, before tips. For the Labor report.")),
            field(u"otherJobs"_s, tr("Other jobs"), u"text"_s,
                  tr("Other jobs they work, one per line with its pay, e.g. \"bartender 9.00\". "
                     "They choose the job when they clock in. Jobs: server, bartender, cashier, host, busser, driver, manager.")),
            with(field(u"language"_s, tr("Language"), u"enum"_s, tr("The screens switch to it when this person logs in.")),
                 u"options"_s, languageOptions(true)),
            with(field(u"textSize"_s, tr("Text size"), u"enum"_s, tr("Bigger button and check text, for them only.")),
                 u"options"_s, options({{"100", "Normal"}, {"115", "Bigger"}, {"130", "Biggest"}})),
            field(u"leftHanded"_s, tr("Left-handed"), u"bool"_s,
                  tr("On their order and Pay screens the check is on the right and the menu on the left.")),
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
                 options({{"", "Automatic"}, {"escpos", "ESC/POS (thermal)"}, {"text", "Plain text"}})),
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
            field(u"food"_s, tr("Food tax %"), u"percent"_s),
            field(u"alcohol"_s, tr("Alcohol tax %"), u"percent"_s),
            field(u"merchandise"_s, tr("Merchandise tax %"), u"percent"_s),
            field(u"room"_s, tr("Room tax %"), u"percent"_s),
            field(u"taxTakeoutFood"_s, tr("Tax takeout food"), u"bool"_s),
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
            with(with(field(u"laterLeadMinutes"_s, tr("Orders for later go to the kitchen (minutes before)"), u"int"_s,
                            tr("An order for 6:30 with 20 here reaches the kitchen at 6:10.")), u"min"_s, 0), u"max"_s, 240),
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
            field(u"gratuityPercent"_s, tr("Party gratuity %"), u"percent"_s, tr("Added to large tables. 0 = off.")),
            with(with(field(u"gratuityMinGuests"_s, tr("…for tables of at least"), u"int"_s), u"min"_s, 1), u"max"_s, 99),
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
                  tr("24-hour time, e.g. 11:00. Runs until the next period starts.")),
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

QVariantList PosService::mealPeriods() const
{
    QVariantList out;
    for (const MealPeriod &m : s_->settings.mealPeriods)
        out.append(QVariantMap{{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"start"_s, m.start}});
    return out;
}

QVariantList PosService::adminRecords(const QString &panel)
{
    QVariantList out;
    auto add = [&](QVariantMap r, const QString &title, const QString &detail) {
        r.insert(u"_title"_s, title);
        r.insert(u"_detail"_s, detail);
        out.append(r);
    };
    if (panel == u"menu") {
        for (const MenuItem &m : s_->menu)
            add(menuRecord(m), qs(m.name),
                format(m.price) + (m.isModifier ? tr(" · modifier") : QString())
                    + (m.available ? QString() : tr(" · SOLD OUT")));
    } else if (panel == u"employees") {
        for (const Employee &e : s_->employees) {
            QVariantMap r{{u"id"_s, qs(e.id)}, {u"name"_s, qs(e.name)}, {u"role"_s, qs(e.role)},
                          {u"active"_s, e.active}, {u"training"_s, e.training}, {u"pin"_s, QString()}, {u"cashMode"_s, qs(e.cashMode)}, {u"requireName"_s, qs(e.requireName)}, {u"language"_s, qs(e.language)},
                          {u"textSize"_s, QString::number(e.textSize)}, {u"leftHanded"_s, e.leftHanded}, {u"startPage"_s, qs(e.startPage)},
                          {u"payRate"_s, e.payRate.cents() / 100.0}, {u"otherJobs"_s, [&] {
                               QStringList lines;
                               for (const Job &j : e.otherJobs)
                                   lines << u"%1 %2"_s.arg(qs(j.role), QString::number(j.rate.cents() / 100.0, 'f', 2));
                               return lines.join(u'\n');
                           }()},
                          {u"checkout"_s, qs(e.checkout)}, {u"tipPool"_s, qs(e.tipPool)}};
            for (const char *p : AllPermissions)
                r.insert(u"perm:"_s + QString::fromLatin1(p),
                         e.allow.contains(p) ? u"allow"_s : e.deny.contains(p) ? u"deny"_s : QString());
            const QString cash = e.cashMode == "serverBank" ? tr(" · own bank")
                                 : e.cashMode == "drawer"   ? tr(" · drawer") : QString();
            add(r, qs(e.name), roleName(qs(e.role)) + cash + (e.active ? QString() : tr(" · inactive")));
        }
    } else if (panel == u"tenders") {
        for (const Tender &t : s_->settings.tenders) {
            QVariantMap r{{u"id"_s, qs(t.id)}, {u"name"_s, qs(t.name)}, {u"kind"_s, qs(toString(t.kind))},
                          {u"percent"_s, double(t.percentBp) / 100.0}, {u"staffMeal"_s, t.staffMeal}};
            const QString kind = t.kind == TenderKind::Cash ? tr("Cash") : t.kind == TenderKind::Card ? tr("Card")
                               : t.kind == TenderKind::GiftCard ? tr("Gift card") : tr("House account");
            add(r, qs(t.name), t.kind == TenderKind::Discount ? u"%1%"_s.arg(double(t.percentBp) / 100.0) : kind);
        }
    } else if (panel == u"printers") {
        for (const PrinterConfig &p : s_->settings.printers) {
            QVariantMap r = toJson(p).toVariantMap();
            for (const char16_t *k : {u"host", u"path", u"format", u"charset"}) {
                if (!r.contains(QString::fromUtf16(k)))
                    r.insert(QString::fromUtf16(k), QString());
            }
            r.insert(u"port"_s, p.port);
            const QString how = p.type == "network" ? tr("Network") : p.type == "cups" ? tr("CUPS queue")
                              : p.type == "file" ? tr("Text file") : p.type == "none" ? tr("Off") : qs(p.type);
            add(r, qs(p.name), how + (p.host.empty() ? QString() : u" · "_s + qs(p.host)));
        }
    } else if (panel == u"taxes") {
        const TaxRates &t = s_->settings.tax;
        add({{u"food"_s, percentFromPpm(t.foodPpm)}, {u"alcohol"_s, percentFromPpm(t.alcoholPpm)},
             {u"merchandise"_s, percentFromPpm(t.merchandisePpm)}, {u"room"_s, percentFromPpm(t.roomPpm)},
             {u"taxTakeoutFood"_s, t.taxTakeoutFood}, {u"cashRounding"_s, QString::number(t.cashRoundingCents)}},
            tr("Tax rates"), QString());
    } else if (panel == u"store") {
        // The tip pool, one job a line ("busser 50"), in the jobs' usual order.
        const QString tipPool = [&] {
            QStringList l;
            for (std::string_view role : kPoolJobs)
                if (const auto it = s_->settings.tipPool.find(std::string(role)); it != s_->settings.tipPool.end())
                    l << u"%1 %2"_s.arg(QLatin1StringView(role)).arg(it->second);
            return l.join(u'\n');
        }();
        add({{u"storeName"_s, qs(s_->settings.storeName)}, {u"language"_s, qs(s_->settings.language)}, {u"currencySymbol"_s, qs(s_->settings.currencySymbol)},
             {u"receiptHeader"_s, qs(s_->settings.receiptHeader)}, {u"receiptFooter"_s, qs(s_->settings.receiptFooter)},
             {u"receiptFreeChoices"_s, s_->settings.receiptFreeChoices},
             {u"soldOutBackNextDay"_s, s_->settings.soldOutBackNextDay},
             {u"gratuityPercent"_s, double(s_->settings.gratuityBp) / 100.0},
             {u"gratuityMinGuests"_s, s_->settings.gratuityMinGuests},
             {u"cashMode"_s, qs(toString(s_->settings.cashMode))},
             {u"terminalsHaveDrawer"_s, s_->settings.terminalsHaveDrawer},
             {u"checkoutNeedsClosedChecks"_s, s_->settings.checkoutNeedsClosedChecks},
             {u"backupCopyDir"_s, qs(s_->settings.backupCopyDir)},
             {u"openingChecklist"_s, [&] { QStringList l; for (const std::string &t : s_->settings.openingChecklist) l << qs(t); return l.join(u'\n'); }()},
             {u"closingChecklist"_s, [&] { QStringList l; for (const std::string &t : s_->settings.closingChecklist) l << qs(t); return l.join(u'\n'); }()},
             {u"kitchenStations"_s, [&] {
                  QStringList lines;
                  for (const Station &x : s_->settings.stations)
                      lines << qs(x.name);
                  return lines.join(u'\n');
              }()},
             {u"expenseCategories"_s, [&] {
                  QStringList lines;
                  for (const std::string &c : s_->settings.expenseCategories)
                      lines << qs(c);
                  return lines.join(u'\n');
              }()},
             {u"extraPercent"_s, s_->settings.extraPercent},
             {u"royaltyPercent"_s, s_->settings.royaltyBp ? QString::number(s_->settings.royaltyBp / 100.0) : QString()},
             {u"adFundPercent"_s, s_->settings.adFundBp ? QString::number(s_->settings.adFundBp / 100.0) : QString()},
             {u"chartOfAccounts"_s, [&] {
                  QStringList lines;
                  for (const auto &[k, v] : s_->settings.accounts)
                      lines << u"%1 = %2"_s.arg(qs(k), qs(v));
                  return lines.join(u'\n');
              }()},
             {u"extraCharge"_s, s_->settings.extraCharge.cents() / 100.0},
             {u"kioskSendNow"_s, s_->settings.kioskSendNow}, {u"kioskIdleSeconds"_s, s_->settings.kioskIdleSeconds},
             {u"kioskSlip"_s, s_->settings.kioskSlip},
             {u"kioskWelcome"_s, qs(s_->settings.kioskLook.welcome)}, {u"kioskAskWhere"_s, s_->settings.kioskLook.askWhere},
             {u"kioskAskName"_s, s_->settings.kioskLook.askName}, {u"kioskEasyReach"_s, s_->settings.kioskLook.easyReach},
             {u"kioskSizePercent"_s, s_->settings.kioskLook.sizePercent},
             {u"kioskBackground"_s, qs(s_->settings.kioskLook.background)}, {u"kioskCard"_s, qs(s_->settings.kioskLook.card)},
             {u"kioskGo"_s, qs(s_->settings.kioskLook.go)}, {u"kioskText"_s, qs(s_->settings.kioskLook.text)},
             {u"kioskFont"_s, qs(s_->settings.kioskLook.font)},
             {u"encryptBackups"_s, !s_->settings.backupKey.empty()}, {u"backupPassword"_s, QString()},
             {u"waitMinutesPerParty"_s, s_->settings.waitMinutesPerParty},
             {u"autoLogoutMinutes"_s, s_->settings.autoLogoutMinutes},
             {u"screenSaverMinutes"_s, s_->settings.screenSaverMinutes},
             {u"tableLongMinutes"_s, s_->settings.tableLongMinutes},
             {u"loyaltyEnabled"_s, s_->settings.loyaltyEnabled}, {u"pointsPerDollar"_s, s_->settings.pointsPerDollar},
             {u"rewards"_s, [&] {
                  QStringList l;
                  for (const PosSettings::Reward &w : s_->settings.rewards)
                      l << u"%1 = %2"_s.arg(w.points).arg(qs(w.value.toString()));
                  return l.join(u'\n');
              }()},
             {u"displayLogo"_s, qs(s_->settings.displayLogo)}, {u"receiptLogo"_s, s_->settings.receiptLogo}, {u"displayAccent"_s, qs(s_->settings.displayAccent)},
             {u"displaySlides"_s, [&] {
                  QStringList l;
                  for (const std::string &sl : s_->settings.displaySlides)
                      l << qs(sl);
                  return l.join(u'\n');
              }()},
             {u"scheduleRequired"_s, s_->settings.scheduleRequired},
             {u"clockInEarlyMinutes"_s, s_->settings.clockInEarlyMinutes},
             {u"tipPool"_s, tipPool},
             {u"declareCashTips"_s, s_->settings.declareCashTips},
             {u"tipOuts"_s, [&] {
                  QStringList l;
                  for (const PosSettings::TipOut &t : s_->settings.tipOuts)
                      l << u"%1 %2 %3"_s.arg(qs(t.role), QString::number(double(t.percentBp) / 100.0), qs(t.basis));
                  return l.join(u'\n');
              }()},
             {u"kitchenWarnMinutes"_s, s_->settings.kitchenWarnMinutes},
             {u"kitchenLateMinutes"_s, s_->settings.kitchenLateMinutes},
             {u"laterLeadMinutes"_s, s_->settings.laterLeadMinutes},
             {u"requireOrderName"_s, s_->settings.requireOrderName}, {u"takeoutMinutes"_s, s_->settings.takeoutMinutes},
             {u"deliveryMinutes"_s, s_->settings.deliveryMinutes},
             {u"minutesPerOrderWaiting"_s, s_->settings.minutesPerOrderWaiting},
             {u"deliveryFee"_s, double(s_->settings.deliveryFee.cents()) / 100.0},
             {u"stripeSecretKey"_s, QString()}, {u"cardCurrency"_s, qs(s_->settings.cardCurrency)},
             {u"offlineCards"_s, s_->settings.offlineCards},
             {u"offlineCardMax"_s, double(s_->settings.offlineCardMax) / 100.0},
             {u"tabHold"_s, double(s_->settings.tabHold) / 100.0},
             {u"offlineCardTotal"_s, double(s_->settings.offlineCardTotal) / 100.0},
             {u"cardTipOn"_s, qs(s_->settings.cardTipOn)},
             {u"tipPercents"_s, [&] { QStringList l; for (int p : s_->settings.tipPercents) l << QString::number(p); return l.join(u", "_s); }()},
             {u"tableReadyText"_s, qs(s_->settings.tableReadyText)}, {u"textWebhook"_s, qs(s_->settings.textWebhook)},
             {u"paidBreaks"_s, s_->settings.paidBreaks}, {u"overtimeDailyHours"_s, s_->settings.overtimeDailyHours},
             {u"overtimeWeeklyHours"_s, s_->settings.overtimeWeeklyHours},
             {u"weekStartsOn"_s, QString::number(s_->settings.weekStartsOn)}},
            tr("Store"), QString());
        // The page each job starts on.
        QVariantMap store = out.last().toMap();
        for (const char *job : {"server", "bartender", "cashier", "host", "manager"})
            store.insert(u"startPage."_s + QLatin1StringView(job),
                         s_->settings.startPages.contains(job) ? qs(s_->settings.startPages.at(job)) : QString());
        out.last() = store;
    } else if (panel == u"promotions") {
        for (const PosSettings::Promotion &p : s_->settings.promotions)
            add(promotionRecord(p), qs(p.name),
                (p.code.empty() ? QString() : tr("code %1").arg(qs(p.code)) + u" · "_s)
                    + (p.buy > 0 ? tr("buy %1 get %2 at %3% off").arg(p.buy).arg(p.get).arg(double(p.percentBp) / 100.0)
                       : p.amount.cents() > 0 ? tr("%1 off").arg(format(p.amount))
                                              : tr("%1% off").arg(double(p.percentBp) / 100.0))
                    + (p.maxUses > 0 ? u" · "_s + tr("used %1 of %2").arg(p.uses).arg(p.maxUses) : QString())
                    + (p.startMinute != p.endMinute ? u" · "_s + clockText(p.startMinute) + u"-"_s + clockText(p.endMinute) : QString())
                    + (p.active ? QString() : tr(" · off")));
    } else if (panel == u"punches") {
        return punchRecords();
    } else if (panel == u"requests") {
        return requestRecords();
    } else if (panel == u"cardReaders") {
        for (const PosSettings::StripeReader &r : s_->settings.stripeReaders) {
            QStringList screens;
            for (const TerminalConfig &t : s_->settings.terminals)
                if (t.cardReader == "counter:" + r.id)
                    screens << qs(t.name);
            add({{u"id"_s, qs(r.id)}, {u"label"_s, qs(r.label)}, {u"deviceType"_s, qs(r.deviceType)}, {u"code"_s, QString()}},
                qs(r.label), screens.isEmpty() ? tr("%1 · no screen uses it yet").arg(qs(r.deviceType))
                                               : tr("%1 · for %2").arg(qs(r.deviceType), screens.join(u", "_s)));
        }
    } else if (panel == u"vendors") {
        for (const Vendor &v : s_->settings.vendors)
            add({{u"id"_s, qs(v.id)}, {u"name"_s, qs(v.name)}, {u"phone"_s, qs(v.phone)}, {u"account"_s, qs(v.account)},
                 {u"note"_s, qs(v.note)}},
                qs(v.name), qs(v.phone));
    } else if (panel == u"inventory") {
        for (const Ingredient &g : s_->ingredients)
            add(toJson(g).toVariantMap(), qs(g.name),
                QString::number(g.onHand, 'g', 8) + u' ' + qs(g.unit)
                    + (g.onHand <= 0 ? tr(" · OUT") : g.low() ? tr(" · LOW") : QString()));
    } else if (panel == u"modifierGroups") {
        for (const ModifierGroup &g : s_->settings.modifierGroups) {
            QStringList lines;
            for (const ModifierOption &o : g.options)
                lines << (o.included ? u"*"_s : QString())
                             + (o.price.cents() ? u"%1 + %2"_s.arg(qs(o.name), qs(o.price.toString())) : qs(o.name))
                             + (o.kitchenHide ? u" | -"_s : o.kitchenName.empty() ? QString() : u" | "_s + qs(o.kitchenName));
            add({{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"min"_s, g.min}, {u"max"_s, g.max},
                 {u"options"_s, lines.join(u'\n')}, {u"menuItems"_s, g.menuItems}, {u"askHow"_s, g.askHow}},
                qs(g.name), tr("%1 options").arg(g.options.size()) + (g.min > 0 ? tr(" · required") : QString())
                                + (g.menuItems ? tr(" · menu items") : QString()));
        }
    } else if (panel == u"mealPeriods") {
        for (const MealPeriod &m : s_->settings.mealPeriods)
            add({{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"start"_s, clockText(m.start)}}, qs(m.name),
                tr("from %1").arg(clockText(m.start)));
    } else if (panel == u"terminals") {
        // The screen you're on is always here, ready to set up.
        auto &list = s_->settings.terminals;
        if (!terminal_.isEmpty()
            && std::ranges::none_of(list, [&](const TerminalConfig &t) { return qs(t.name) == terminal_; })) {
            TerminalConfig self;
            self.name = ss(terminal_);
            list.insert(list.begin(), self);
            settingsChanged();
        }
        for (const TerminalConfig &t : list) {
            const PrinterConfig *p = s_->settings.printer(t.receiptPrinter);
            add({{u"name"_s, qs(t.name)}, {u"receiptPrinter"_s, qs(t.receiptPrinter)}, {u"drawer"_s, qs(t.drawer)},
                 {u"screen"_s, qs(t.screen)}, {u"look"_s, qs(t.look)}, {u"keyboard"_s, qs(t.keyboard)},
                 {u"requireName"_s, qs(t.requireName)}, {u"cardReader"_s, qs(t.cardReader)},
                 {u"afterPaying"_s, qs(t.afterPaying)}, {u"startCategory"_s, qs(t.startCategory)}},
                qs(t.name), (p ? qs(p->name) : tr("Receipt (default)"))
                                + (s_->settings.hasDrawer(t.name) ? QString() : tr(" · no drawer"))
                                + (t.key.empty() ? QString() : tr(" · paired device"))
                                + (qs(t.name) == terminal_ ? tr(" · this screen") : QString()));
        }
    }
    return out;
}

QVariantMap PosService::adminNewRecord(const QString &panel)
{
    if (panel == u"menu")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"price"_s, 0.0}, {u"family"_s, QString()},
                {u"taxClass"_s, u"food"_s}, {u"printer"_s, u"kitchen"_s}, {u"station"_s, QString()}, {u"byWeight"_s, false}, {u"weightUnit"_s, u"lb"_s}, {u"substitute"_s, false}, {u"substitutePrice"_s, 0.0}, {u"ticketCapacity"_s, 0}, {u"eventAt"_s, QString()}, {u"modifier"_s, false}, {u"available"_s, true},
                {u"modifierGroups"_s, QString()}, {u"periodPrices"_s, QString()}, {u"recipe"_s, QString()},
                {u"kitchenName"_s, QString()}, {u"kitchenColor"_s, QString()}, {u"kitchenHide"_s, false},
                {u"kioskHide"_s, false}, {u"description"_s, QString()}, {u"image"_s, QString()},
                {u"takeoutPrice"_s, 0.0}, {u"deliveryPrice"_s, 0.0}, {u"noDiscount"_s, false}, {u"noStaffDiscount"_s, false}};
    if (panel == u"employees")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"role"_s, u"server"_s}, {u"pin"_s, QString()},
                {u"active"_s, true}, {u"training"_s, false}, {u"cashMode"_s, QString()}, {u"requireName"_s, QString()}, {u"checkout"_s, QString()}, {u"tipPool"_s, QString()},
                {u"payRate"_s, 0.0}, {u"otherJobs"_s, QString()}, {u"language"_s, QString()},
                {u"textSize"_s, u"100"_s}, {u"leftHanded"_s, false}, {u"startPage"_s, QString()},
                {u"perm:order"_s, QString()}, {u"perm:check.settle"_s, QString()}, {u"perm:check.others"_s, QString()}, {u"perm:check.discount"_s, QString()},
                {u"perm:order.void"_s, QString()}, {u"perm:manager"_s, QString()}, {u"perm:layout.edit"_s, QString()}};
    if (panel == u"tenders")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"kind"_s, u"card"_s}, {u"percent"_s, 0.0},
                {u"staffMeal"_s, false}};
    if (panel == u"terminals")
        return {{u"name"_s, terminal_}, {u"receiptPrinter"_s, QString()}, {u"drawer"_s, QString()},
                {u"screen"_s, QString()}, {u"look"_s, QString()}, {u"keyboard"_s, QString()}, {u"requireName"_s, QString()},
                {u"cardReader"_s, QString()}, {u"afterPaying"_s, QString()}, {u"startCategory"_s, QString()}};
    if (panel == u"mealPeriods")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"start"_s, u"17:00"_s}};
    if (panel == u"modifierGroups")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"min"_s, 1}, {u"max"_s, 1}, {u"options"_s, QString()}, {u"askHow"_s, false},
                {u"menuItems"_s, false}};
    if (panel == u"promotions")
        return promotionRecord({});
    if (panel == u"inventory")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"unit"_s, u"each"_s}, {u"onHand"_s, 0.0},
                {u"lowAt"_s, 0.0}, {u"cost"_s, 0.0}, {u"vendor"_s, QString()}};
    if (panel == u"punches")
        return punchNewRecord();
    if (panel == u"cardReaders")
        return {{u"id"_s, QString()}, {u"label"_s, QString()}, {u"deviceType"_s, QString()}, {u"code"_s, QString()},
                {u"line1"_s, QString()}, {u"city"_s, QString()}, {u"state"_s, QString()}, {u"postalCode"_s, QString()},
                {u"country"_s, u"US"_s}};
    if (panel == u"vendors")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"phone"_s, QString()}, {u"account"_s, QString()},
                {u"note"_s, QString()}};
    if (panel == u"printers")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"type"_s, u"network"_s}, {u"host"_s, QString()},
                {u"port"_s, 9100}, {u"path"_s, QString()}, {u"format"_s, QString()}, {u"width"_s, 42},
                {u"cutter"_s, true}, {u"drawerKick"_s, false}, {u"receipts"_s, true}, {u"charset"_s, QString()}, {u"watch"_s, true}};
    return {};
}

bool PosService::adminSave(const QString &panel, int index, const QVariantMap &record)
{
    if (!require(perm::Manager, tr("Changing settings")))
        return false;
    bool ok = false;
    if (panel == u"menu") {
        ok = saveMenuRecord(index, record);
        if (ok)
            refreshSoldOut();
    } else if (panel == u"employees") {
        ok = saveEmployeeRecord(index, record);
    } else if (panel == u"tenders") {
        ok = saveTenderRecord(index, record);
    } else if (panel == u"printers") {
        ok = savePrinterRecord(index, record);
    } else if (panel == u"mealPeriods") {
        ok = saveMealPeriodRecord(index, record);
    } else if (panel == u"modifierGroups") {
        ok = saveModifierGroupRecord(index, record);
    } else if (panel == u"promotions") {
        ok = savePromotionRecord(index, record);
    } else if (panel == u"punches") {
        ok = savePunchRecord(index, record);
    } else if (panel == u"requests") {
        ok = saveRequestRecord(index, record);
    } else if (panel == u"cardReaders") {
        auto &list = s_->settings.stripeReaders;
        if (index >= 0 && index < int(list.size())) {   // a new name
            const QString label = record.value(u"label"_s).toString().trimmed();
            if (label.isEmpty())
                return fail(tr("Give the reader a name."));
            list[index].label = ss(label);
        } else {
            // A new reader: pairing with Stripe takes a moment; it shows up when it's done.
            if (s_->settings.stripeSecretKey.empty())
                return fail(tr("Set the store's Stripe secret key first (Store Settings)."));
            if (record.value(u"code"_s).toString().trimmed().isEmpty())
                return fail(tr("Type the pairing code the reader shows."));
            emit notice(tr("Pairing with Stripe…"));
            pairCounterReader(record);
            return true;
        }
    } else if (panel == u"vendors") {
        auto &list = s_->settings.vendors;
        const QString name = record.value(u"name"_s).toString().trimmed();
        if (name.isEmpty())
            return fail(tr("The vendor needs a name."));
        if (index >= int(list.size()))
            return false;
        Vendor v{std::string(), ss(name), ss(record.value(u"phone"_s).toString().trimmed()),
                 ss(record.value(u"account"_s).toString().trimmed()), ss(record.value(u"note"_s).toString().trimmed())};
        if (index >= 0) {
            v.id = list[index].id;
            list[index] = v;
        } else {
            const QString wanted = record.value(u"id"_s).toString().trimmed();
            v.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, list, [](const Vendor &x) { return x.id; }, -1));
            list.push_back(v);
        }
        settingsChanged();
        ok = true;
    } else if (panel == u"inventory") {
        const QString name = record.value(u"name"_s).toString().trimmed();
        if (name.isEmpty())
            return fail(tr("The ingredient needs a name."));
        if (index >= int(s_->ingredients.size()))
            return false;
        Ingredient g = ingredientFromJson(QJsonObject::fromVariantMap(record));
        g.name = ss(name);
        if (g.unit.empty())
            g.unit = "each";
        if (g.lowAt < 0)
            return fail(tr("Low at can't be negative."));
        if (index >= 0) {
            g.id = s_->ingredients[index].id;
            s_->ingredients[index] = g;
        } else {
            const QString wanted = record.value(u"id"_s).toString().trimmed();
            g.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, s_->ingredients,
                               [](const Ingredient &i) { return i.id; }, -1));
            s_->ingredients.push_back(g);
            index = int(s_->ingredients.size()) - 1;
        }
        if (s_->sink)
            s_->sink->saveIngredient(s_->ingredients[index], index);
        refreshSoldOut();   // restocked items come back, short ones go
        ok = true;
    } else if (panel == u"terminals") {
        const QString name = record.value(u"name"_s).toString().trimmed();
        if (name.isEmpty())
            return fail(tr("The terminal needs a name."));
        auto &list = s_->settings.terminals;
        for (int i = 0; i < int(list.size()); ++i) {
            if (i != index && qs(list[i].name) == name)
                return fail(tr("%1 is already set up.").arg(name));
        }
        const QString drawer = record.value(u"drawer"_s).toString();
        if (!QStringList{QString(), u"yes"_s, u"no"_s}.contains(drawer))
            return fail(tr("Choose whether the terminal has a cash drawer."));
        // Editing keeps a paired device's id and key.
        TerminalConfig t = index >= 0 && index < int(list.size()) ? list[index] : TerminalConfig{};
        t.name = ss(name);
        t.receiptPrinter = ss(record.value(u"receiptPrinter"_s).toString());
        t.drawer = ss(drawer);
        const QString screen = record.value(u"screen"_s).toString();
        if (!QStringList{QString(), u"standard"_s, u"phone"_s, u"selfOrder"_s, u"timeClock"_s}.contains(screen))
            return fail(tr("Choose the terminal's screen layout."));
        t.screen = ss(screen);
        t.look = ss(record.value(u"look"_s).toString().left(40));
        const QString keyboard = record.value(u"keyboard"_s).toString();
        if (!QStringList{QString(), u"on"_s, u"off"_s}.contains(keyboard))
            return fail(tr("Choose whether the screen shows a keyboard."));
        t.keyboard = ss(keyboard);
        const QString requireName = record.value(u"requireName"_s).toString();
        if (!QStringList{QString(), u"yes"_s, u"no"_s}.contains(requireName))
            return fail(tr("Choose whether phone orders need a name at this screen."));
        t.requireName = ss(requireName);
        const QString reader = record.value(u"cardReader"_s).toString();
        const bool paired = reader.startsWith(u"counter:"_s)
                            && std::ranges::any_of(s_->settings.stripeReaders,
                                                   [&](const PosSettings::StripeReader &r) { return qs(r.id) == reader.mid(8); });
        if (!paired && !QStringList{QString(), u"stripe"_s, u"simulated"_s}.contains(reader))
            return fail(tr("Choose the screen's card reader."));
        t.cardReader = ss(reader);
        const QString after = record.value(u"afterPaying"_s).toString();
        if (!QStringList{QString(), u"print"_s, u"ask"_s}.contains(after))
            return fail(tr("Choose what happens after a check is paid."));
        t.afterPaying = ss(after);
        t.startCategory = ss(record.value(u"startCategory"_s).toString());
        if (index >= 0 && index < int(list.size()))
            list[index] = t;
        else
            list.push_back(t);
        settingsChanged();
        ok = true;
    } else if (panel == u"taxes") {
        const double rates[] = {number(record, u"food"), number(record, u"alcohol"),
                                number(record, u"merchandise"), number(record, u"room")};
        for (double r : rates) {
            if (r < 0 || r > 100)
                return fail(tr("Tax rates must be between 0 and 100%."));
        }
        s_->settings.tax.foodPpm = ppmFromPercent(rates[0]);
        s_->settings.tax.alcoholPpm = ppmFromPercent(rates[1]);
        s_->settings.tax.merchandisePpm = ppmFromPercent(rates[2]);
        s_->settings.tax.roomPpm = ppmFromPercent(rates[3]);
        s_->settings.tax.taxTakeoutFood = record.value(u"taxTakeoutFood"_s).toBool();
        if (record.contains(u"cashRounding"_s)) {
            const int step = record.value(u"cashRounding"_s).toInt();
            if (step != 0 && step != 5 && step != 10)
                return fail(tr("Cash rounding is to 5 or 10 cents, or none."));
            s_->settings.tax.cashRoundingCents = step;
        }
        settingsChanged();
        ok = true;
    } else if (panel == u"store") {
        const QString name = record.value(u"storeName"_s).toString().trimmed();
        if (name.isEmpty())
            return fail(tr("The store needs a name."));
        s_->settings.storeName = ss(name);
        s_->settings.currencySymbol = ss(record.value(u"currencySymbol"_s).toString());
        s_->settings.receiptHeader = ss(record.value(u"receiptHeader"_s).toString());
        s_->settings.receiptFooter = ss(record.value(u"receiptFooter"_s).toString());
        if (record.contains(u"receiptFreeChoices"_s))
            s_->settings.receiptFreeChoices = record.value(u"receiptFreeChoices"_s).toBool();
        if (record.contains(u"soldOutBackNextDay"_s))
            s_->settings.soldOutBackNextDay = record.value(u"soldOutBackNextDay"_s).toBool();
        const double gratuity = number(record, u"gratuityPercent");
        if (gratuity < 0 || gratuity > 100)
            return fail(tr("Gratuity is between 0 and 100%."));
        s_->settings.gratuityBp = std::llround(gratuity * 100.0);
        s_->settings.gratuityMinGuests = std::max(1, record.value(u"gratuityMinGuests"_s, 6).toInt());
        if (record.contains(u"cashMode"_s))
            s_->settings.cashMode = cashModeFromString(ss(record.value(u"cashMode"_s).toString()));
        if (record.contains(u"language"_s)) {
            const QString lang = record.value(u"language"_s).toString();
            if (std::ranges::none_of(i18n::languages(), [&](const i18n::Language &l) { return l.code == lang; }))
                return fail(tr("Choose a language."));
            s_->settings.language = ss(lang);
        }
        for (const char *job : {"server", "bartender", "cashier", "host", "manager"}) {
            const QString key = u"startPage."_s + QLatin1StringView(job);
            if (!record.contains(key))
                continue;
            if (const QString page = record.value(key).toString(); page.isEmpty())
                s_->settings.startPages.erase(job);
            else
                s_->settings.startPages[job] = ss(page);
        }
        if (record.contains(u"terminalsHaveDrawer"_s))
            s_->settings.terminalsHaveDrawer = record.value(u"terminalsHaveDrawer"_s).toBool();
        if (record.contains(u"paidBreaks"_s))
            s_->settings.paidBreaks = record.value(u"paidBreaks"_s).toBool();
        if (record.contains(u"overtimeDailyHours"_s))
            s_->settings.overtimeDailyHours = std::clamp(record.value(u"overtimeDailyHours"_s).toInt(), 0, 24);
        if (record.contains(u"overtimeWeeklyHours"_s))
            s_->settings.overtimeWeeklyHours = std::clamp(record.value(u"overtimeWeeklyHours"_s).toInt(), 0, 168);
        if (record.contains(u"weekStartsOn"_s))
            s_->settings.weekStartsOn = std::clamp(record.value(u"weekStartsOn"_s).toString().toInt(), 0, 6);
        if (record.contains(u"checkoutNeedsClosedChecks"_s))
            s_->settings.checkoutNeedsClosedChecks = record.value(u"checkoutNeedsClosedChecks"_s).toBool();
        if (record.contains(u"tipPercents"_s)) {
            std::vector<int> tips;
            for (const QString &part : record.value(u"tipPercents"_s).toString().split(u',', Qt::SkipEmptyParts)) {
                const int v = part.trimmed().remove(u'%').toInt();
                if (v > 0 && v <= 100 && tips.size() < 6)
                    tips.push_back(v);
            }
            if (!tips.empty())
                s_->settings.tipPercents = tips;
        }
        if (record.contains(u"screenSaverMinutes"_s))
            s_->settings.screenSaverMinutes = std::clamp(record.value(u"screenSaverMinutes"_s).toInt(), 0, 240);
        if (record.contains(u"autoLogoutMinutes"_s))
            s_->settings.autoLogoutMinutes = std::clamp(record.value(u"autoLogoutMinutes"_s).toInt(), 0, 120);
        if (record.contains(u"tableLongMinutes"_s))
            s_->settings.tableLongMinutes = std::clamp(record.value(u"tableLongMinutes"_s).toInt(), 10, 600);
        if (record.contains(u"loyaltyEnabled"_s))
            s_->settings.loyaltyEnabled = record.value(u"loyaltyEnabled"_s).toBool();
        if (record.contains(u"pointsPerDollar"_s))
            s_->settings.pointsPerDollar = std::clamp(record.value(u"pointsPerDollar"_s).toInt(), 1, 100);
        if (record.contains(u"rewards"_s)) {
            std::vector<PosSettings::Reward> rewards;
            for (const QString &line : record.value(u"rewards"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
                const QStringList kv = line.split(u'=');
                bool okPoints = false, okValue = false;
                const int points = kv.value(0).trimmed().toInt(&okPoints);
                const double value = kv.value(1).trimmed().remove(qs(s_->settings.currencySymbol)).toDouble(&okValue);
                if (line.trimmed().isEmpty())
                    continue;
                if (kv.size() != 2 || !okPoints || !okValue || points <= 0 || value <= 0)
                    return fail(tr("Write rewards like \"50 = 5.00\" (points = amount off)."));
                rewards.push_back({points, Money::fromCents(std::llround(value * 100.0))});
            }
            std::ranges::sort(rewards, {}, &PosSettings::Reward::points);
            s_->settings.rewards = rewards;
        }
        if (record.contains(u"displayLogo"_s))
            s_->settings.displayLogo = ss(record.value(u"displayLogo"_s).toString().trimmed());
        if (record.contains(u"receiptLogo"_s))
            s_->settings.receiptLogo = record.value(u"receiptLogo"_s).toBool();
        if (record.contains(u"displayAccent"_s) && !record.value(u"displayAccent"_s).toString().isEmpty())
            s_->settings.displayAccent = ss(record.value(u"displayAccent"_s).toString());
        if (record.contains(u"displaySlides"_s)) {
            s_->settings.displaySlides.clear();
            for (const QString &line : record.value(u"displaySlides"_s).toString().split(u'\n', Qt::SkipEmptyParts))
                if (!line.trimmed().isEmpty())
                    s_->settings.displaySlides.push_back(ss(line.trimmed()));
        }
        if (record.contains(u"scheduleRequired"_s))
            s_->settings.scheduleRequired = record.value(u"scheduleRequired"_s).toBool();
        if (record.contains(u"clockInEarlyMinutes"_s))
            s_->settings.clockInEarlyMinutes = std::clamp(record.value(u"clockInEarlyMinutes"_s).toInt(), 0, 240);
        if (record.contains(u"tipPool"_s)) {
            std::map<std::string, int> pool;
            for (const QString &line : record.value(u"tipPool"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
                const QStringList parts = line.simplified().remove(u'%').split(u' ', Qt::SkipEmptyParts);
                if (parts.isEmpty())
                    continue;
                const std::string role = ss(parts.value(0).toLower());
                bool ok = false;
                const int share = parts.value(1).toInt(&ok);
                if (std::ranges::find(kPoolJobs, std::string_view(role)) == std::end(kPoolJobs))
                    return fail(tr("Tip pool: \"%1\" is not a job (server, bartender, busser, host, cashier, driver).").arg(qs(role)));
                if (!ok || share < 1 || share > 200)
                    return fail(tr("Tip pool: a job's share is 1 to 200 (100 a full share)."));
                if (pool.contains(role))
                    return fail(tr("Tip pool: each job once."));
                pool[role] = share;
            }
            s_->settings.tipPool = pool;
        }
        if (record.contains(u"declareCashTips"_s))
            s_->settings.declareCashTips = record.value(u"declareCashTips"_s).toBool();
        if (record.contains(u"tipOuts"_s)) {
            std::vector<PosSettings::TipOut> outs;
            for (const QString &line : record.value(u"tipOuts"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
                const QStringList parts = line.simplified().remove(u'%').split(u' ', Qt::SkipEmptyParts);
                if (parts.isEmpty())
                    continue;
                bool ok = false;
                const double percent = parts.value(1).toDouble(&ok);
                const QString role = parts.value(0).toLower();
                const QString basis = parts.value(2, u"tips"_s).toLower();
                if (!QStringList{u"busser"_s, u"bartender"_s, u"host"_s, u"server"_s, u"cashier"_s}.contains(role))
                    return fail(tr("Tip-outs: \"%1\" is not a role (busser, bartender, host, server, cashier).").arg(role));
                if (!ok || percent <= 0 || percent > 100 || (basis != u"tips" && basis != u"sales"))
                    return fail(tr("Write tip-outs like \"busser 15 tips\" or \"bartender 2 sales\"."));
                outs.push_back({ss(role), std::llround(percent * 100.0), ss(basis)});
            }
            s_->settings.tipOuts = outs;
        }
        if (record.contains(u"kitchenWarnMinutes"_s))
            s_->settings.kitchenWarnMinutes = std::clamp(record.value(u"kitchenWarnMinutes"_s).toInt(), 1, 120);
        if (record.contains(u"laterLeadMinutes"_s))
            s_->settings.laterLeadMinutes = std::clamp(record.value(u"laterLeadMinutes"_s).toInt(), 0, 240);
        if (record.contains(u"requireOrderName"_s))
            s_->settings.requireOrderName = record.value(u"requireOrderName"_s).toBool();
        if (record.contains(u"takeoutMinutes"_s))
            s_->settings.takeoutMinutes = std::clamp(record.value(u"takeoutMinutes"_s).toInt(), 0, 240);
        if (record.contains(u"deliveryMinutes"_s))
            s_->settings.deliveryMinutes = std::clamp(record.value(u"deliveryMinutes"_s).toInt(), 0, 240);
        if (record.contains(u"minutesPerOrderWaiting"_s))
            s_->settings.minutesPerOrderWaiting = std::clamp(record.value(u"minutesPerOrderWaiting"_s).toInt(), 0, 60);
        if (const QString key = record.value(u"stripeSecretKey"_s).toString().trimmed(); !key.isEmpty()) {
            if (key.compare(u"none"_s, Qt::CaseInsensitive) == 0)
                s_->settings.stripeSecretKey.clear();
            else if (!key.startsWith(u"sk_"_s) && !key.startsWith(u"rk_"_s))
                return fail(tr("A Stripe secret key starts with sk_ (or rk_ for a restricted key)."));
            else
                s_->settings.stripeSecretKey = ss(key);
        }
        if (record.contains(u"tabHold"_s))
            s_->settings.tabHold = std::max<std::int64_t>(0, std::llround(record.value(u"tabHold"_s).toDouble() * 100.0));
        if (record.contains(u"offlineCardMax"_s))
            s_->settings.offlineCardMax = std::max<std::int64_t>(0, std::llround(record.value(u"offlineCardMax"_s).toDouble() * 100.0));
        if (record.contains(u"offlineCardTotal"_s))
            s_->settings.offlineCardTotal = std::max<std::int64_t>(0, std::llround(record.value(u"offlineCardTotal"_s).toDouble() * 100.0));
        if (record.contains(u"offlineCards"_s) && record.value(u"offlineCards"_s).toBool() != s_->settings.offlineCards) {
            s_->settings.offlineCards = record.value(u"offlineCards"_s).toBool();
            setStripeOffline(s_->settings.offlineCards);   // the readers follow
        }
        if (record.contains(u"cardTipOn"_s)) {
            const QString on = record.value(u"cardTipOn"_s).toString();
            if (on != u"reader" && !on.isEmpty())
                return fail(tr("Choose where card tips are asked."));
            s_->settings.cardTipOn = ss(on);
        }
        if (record.contains(u"cardCurrency"_s)) {
            const QString currency = record.value(u"cardCurrency"_s).toString().trimmed().toLower();
            if (currency.size() != 3 || !std::ranges::all_of(currency, [](QChar ch) { return ch.isLetter(); }))
                return fail(tr("Write the currency as three letters, like usd."));
            s_->settings.cardCurrency = ss(currency);
        }
        if (record.contains(u"deliveryFee"_s)) {
            const double fee = record.value(u"deliveryFee"_s).toDouble();
            if (fee < 0 || fee > 1000)
                return fail(tr("Write the delivery fee in dollars."));
            s_->settings.deliveryFee = Money::fromCents(std::llround(fee * 100));
        }
        if (record.contains(u"kitchenLateMinutes"_s))
            s_->settings.kitchenLateMinutes =
                std::clamp(record.value(u"kitchenLateMinutes"_s).toInt(), s_->settings.kitchenWarnMinutes, 240);
        if (record.contains(u"waitMinutesPerParty"_s))
            s_->settings.waitMinutesPerParty = std::clamp(record.value(u"waitMinutesPerParty"_s).toInt(), 1, 120);
        if (record.contains(u"tableReadyText"_s) && !record.value(u"tableReadyText"_s).toString().trimmed().isEmpty())
            s_->settings.tableReadyText = ss(record.value(u"tableReadyText"_s).toString().trimmed());
        if (record.contains(u"textWebhook"_s))
            s_->settings.textWebhook = ss(record.value(u"textWebhook"_s).toString().trimmed());
        if (record.contains(u"backupCopyDir"_s))
            s_->settings.backupCopyDir = ss(record.value(u"backupCopyDir"_s).toString().trimmed());
        for (const auto &[key, list] : {std::pair{u"openingChecklist"_s, &s_->settings.openingChecklist},
                                        std::pair{u"closingChecklist"_s, &s_->settings.closingChecklist}}) {
            if (!record.contains(key))
                continue;
            list->clear();
            for (const QString &line : record.value(key).toString().split(u'\n', Qt::SkipEmptyParts))
                if (!line.trimmed().isEmpty() && list->size() < 40)
                    list->push_back(ss(line.trimmed().left(120)));
        }
        if (record.contains(u"kitchenStations"_s)) {
            // Names in; ids stay the same for names already there.
            std::vector<Station> stations;
            for (const QString &line : record.value(u"kitchenStations"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
                const QString name = line.trimmed();
                if (name.isEmpty())
                    continue;
                const auto old = std::ranges::find_if(s_->settings.stations, [&](const Station &x) {
                    return QString::compare(qs(x.name), name, Qt::CaseInsensitive) == 0;
                });
                const std::string id = old != s_->settings.stations.end() ? old->id : ss(slug(name));
                if (std::ranges::none_of(stations, [&](const Station &x) { return x.id == id; }))
                    stations.push_back({id, ss(name)});
            }
            s_->settings.stations = stations;
        }
        if (record.contains(u"expenseCategories"_s)) {
            std::vector<std::string> categories;
            for (const QString &line : record.value(u"expenseCategories"_s).toString().split(u'\n', Qt::SkipEmptyParts))
                if (!line.trimmed().isEmpty())
                    categories.push_back(ss(line.trimmed()));
            s_->settings.expenseCategories = categories;
        }
        for (const auto &[key, slot] : {std::pair{u"royaltyPercent"_s, &s_->settings.royaltyBp},
                                        {u"adFundPercent"_s, &s_->settings.adFundBp}}) {
            if (!record.contains(key))
                continue;
            const QString v = record.value(key).toString().trimmed().remove(u'%');
            bool ok = true;
            const double pct = v.isEmpty() ? 0 : v.toDouble(&ok);
            if (!ok || pct < 0 || pct > 100)
                return fail(tr("Write percents like 5 or 4.5."));
            *slot = std::llround(pct * 100);
        }
        if (record.contains(u"chartOfAccounts"_s)) {
            std::map<std::string, std::string> accounts;
            for (const QString &line : record.value(u"chartOfAccounts"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
                const qsizetype eq = line.indexOf(u'=');
                if (eq <= 0)
                    return fail(tr("Write each account like \"sales:burgers = 4010 Burger sales\"."));
                accounts[ss(line.left(eq).trimmed().toLower())] = ss(line.mid(eq + 1).trimmed());
            }
            s_->settings.accounts = accounts;
        }
        if (record.contains(u"extraPercent"_s))
            s_->settings.extraPercent = std::clamp(record.value(u"extraPercent"_s).toInt(), 0, 500);
        if (record.contains(u"extraCharge"_s)) {
            const double extra = record.value(u"extraCharge"_s).toDouble();
            if (extra < 0)
                return fail(tr("The Extra charge can't be negative."));
            s_->settings.extraCharge = Money::fromCents(std::llround(extra * 100));
        }
        if (record.contains(u"kioskSendNow"_s))
            s_->settings.kioskSendNow = record.value(u"kioskSendNow"_s).toBool();
        if (record.contains(u"kioskSlip"_s))
            s_->settings.kioskSlip = record.value(u"kioskSlip"_s).toBool();
        if (record.contains(u"kioskIdleSeconds"_s))
            s_->settings.kioskIdleSeconds = std::clamp(record.value(u"kioskIdleSeconds"_s).toInt(), 30, 600);
        {
            auto &l = s_->settings.kioskLook;
            static const QRegularExpression hex(u"^(#[0-9a-fA-F]{6})?$"_s);
            for (const auto &[key, slot] : {std::pair{u"kioskBackground"_s, &l.background}, {u"kioskCard"_s, &l.card},
                                            {u"kioskGo"_s, &l.go}, {u"kioskText"_s, &l.text}}) {
                if (!record.contains(key))
                    continue;
                const QString v = record.value(key).toString().trimmed();
                if (!hex.match(v).hasMatch())
                    return fail(tr("Write kiosk colors like #1f8a4c, or leave them empty."));
                *slot = ss(v);
            }
            if (record.contains(u"kioskFont"_s))
                l.font = ss(record.value(u"kioskFont"_s).toString().trimmed());
            if (record.contains(u"kioskWelcome"_s))
                l.welcome = ss(record.value(u"kioskWelcome"_s).toString().trimmed().left(40));
            if (record.contains(u"kioskSizePercent"_s))
                l.sizePercent = std::clamp(record.value(u"kioskSizePercent"_s).toInt(), 60, 200);
            if (record.contains(u"kioskAskWhere"_s))
                l.askWhere = record.value(u"kioskAskWhere"_s).toBool();
            if (record.contains(u"kioskAskName"_s))
                l.askName = record.value(u"kioskAskName"_s).toBool();
            if (record.contains(u"kioskEasyReach"_s))
                l.easyReach = record.value(u"kioskEasyReach"_s).toBool();
        }
        if (record.contains(u"encryptBackups"_s)) {
            const QString password = record.value(u"backupPassword"_s).toString();
            if (!record.value(u"encryptBackups"_s).toBool()) {
                s_->settings.backupKey.clear();
                s_->settings.backupSalt.clear();
            } else if (!password.isEmpty() || s_->settings.backupKey.empty()) {
                if (password.size() < 8)
                    return fail(tr("Choose a backup password of at least 8 characters."));
                const auto [key, salt] = s_->backupKeyFor ? s_->backupKeyFor(password) : std::pair<QByteArray, QByteArray>{};
                if (key.isEmpty())
                    return fail(tr("This computer can't encrypt backups."));
                s_->settings.backupKey = key.toBase64().toStdString();
                s_->settings.backupSalt = salt.toBase64().toStdString();
            }
        }
        settingsChanged();
        ok = true;
    }
    if (ok) {
        ++s_->adminRevision;
        emit s_->adminChanged();
        emit notice(tr("Saved"));
    }
    return ok;
}

bool PosService::saveMenuRecord(int index, const QVariantMap &record)
{
    if (index >= int(s_->menu.size()))
        return false;
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The item needs a name."));
    if (number(record, u"price") < 0)
        return fail(tr("Prices cannot be negative."));
    const QString plu = record.value(u"number"_s).toString().trimmed();
    if (!plu.isEmpty()) {
        if (plu.size() > 6 || !std::ranges::all_of(plu, [](QChar c) { return c.isDigit(); }))
            return fail(tr("The number is digits only (up to 6), like 104."));
        for (int i = 0; i < int(s_->menu.size()); ++i)
            if (i != index && qs(s_->menu[i].number) == plu)
                return fail(tr("%1 already has number %2.").arg(qs(s_->menu[i].name), plu));
    }
    const QStringList codes = barcodeList(record.value(u"barcodes"_s).toString());
    if (const QString problem = barcodeProblem(codes, index >= 0 ? s_->menu[index].id : std::string()); !problem.isEmpty())
        return fail(problem);
    // The form's text fields, as menu data.
    QVariantMap data = record;
    data.insert(u"barcodes"_s, codes);
    QVariantList groups;
    for (QString g : record.value(u"modifierGroups"_s).toString().split(u',', Qt::SkipEmptyParts)) {
        g = g.trimmed();
        if (g.isEmpty())
            continue;
        if (!s_->settings.modifierGroup(ss(g)))
            return fail(tr("There is no modifier group '%1' (see Manager → Modifier Groups).").arg(g));
        groups << g;
    }
    data.insert(u"modifierGroups"_s, groups);
    QVariantMap prices;
    for (const QString &line : record.value(u"periodPrices"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
        const QStringList kv = line.split(u'=');
        bool ok = false;
        const double price = kv.value(1).trimmed().remove(qs(s_->settings.currencySymbol)).toDouble(&ok);
        const QString period = kv.value(0).trimmed();
        if (period.isEmpty() && line.trimmed().isEmpty())
            continue;
        if (kv.size() != 2 || !ok || price < 0 || period.isEmpty())
            return fail(tr("Write meal period prices like \"dinner = 14.50\"."));
        if (!periodIds().contains(period))
            return fail(tr("There is no meal period '%1'.").arg(period));
        prices.insert(period, price);
    }
    data.insert(u"periodPrices"_s, prices);
    // "bun 1" / "Burger Buns 1" / "lettuce 0.5": an ingredient (id or name), then the amount.
    QVariantList recipe;
    for (const QString &line : record.value(u"recipe"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
        QString text = line.trimmed();
        if (text.isEmpty())
            continue;
        bool ok = false;
        const qsizetype space = text.lastIndexOf(u' ');
        double qty = space > 0 ? text.mid(space + 1).toDouble(&ok) : 0;
        QString what = ok ? text.left(space).trimmed() : text;
        if (!ok)
            qty = 1;
        if (qty <= 0)
            return fail(tr("Recipe amounts must be more than zero: \"%1\".").arg(text));
        const Ingredient *g = s_->ingredient(ss(what));
        if (!g) {
            for (const Ingredient &i : s_->ingredients)
                if (QString::compare(qs(i.name), what, Qt::CaseInsensitive) == 0)
                    g = &i;
        }
        if (!g)
            return fail(tr("There is no ingredient '%1' (see Manager → Inventory).").arg(what));
        recipe.append(QVariantMap{{u"ingredient"_s, qs(g->id)}, {u"qty"_s, qty}});
    }
    data.insert(u"recipe"_s, recipe);
    data.remove(u"autoSoldOut"_s);
    // The event's date, typed: 2026-10-09 19:00.
    if (const QString when = data.value(u"eventAt"_s).toString().trimmed(); when.isEmpty()) {
        data.insert(u"eventAt"_s, 0);
    } else {
        const QDateTime at = QDateTime::fromString(when, u"yyyy-MM-dd HH:mm"_s);
        if (!at.isValid())
            return fail(tr("Write the event's date like 2026-10-09 19:00."));
        data.insert(u"eventAt"_s, at.toMSecsSinceEpoch());
    }
    data.remove(u"ticketsSoldBefore"_s);
    data.remove(u"soldOutToday"_s);
    MenuItem item = menuItemFromJson(QJsonObject::fromVariantMap(data));
    item.name = ss(name);
    if (index >= 0 && index < int(s_->menu.size())) {
        item.ticketsSoldBefore = s_->menu[index].ticketsSoldBefore;   // sales already made stay
        // 86'd today and left so: still today's.
        item.soldOutToday = s_->menu[index].soldOutToday && item.available == s_->menu[index].available;
    }
    if (index >= 0) {
        item.id = s_->menu[index].id;
        s_->menu[index] = item;
    } else {
        const QString wanted = record.value(u"id"_s).toString().trimmed();
        item.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, s_->menu, [](const MenuItem &m) { return m.id; }, -1));
        s_->menu.push_back(item);
        index = int(s_->menu.size()) - 1;
    }
    if (s_->sink)
        s_->sink->saveMenuItem(s_->menu[index], index);
    return true;
}

bool PosService::saveEmployeeRecord(int index, const QVariantMap &record)
{
    if (index >= int(s_->employees.size()))
        return false;
    const QString name = record.value(u"name"_s).toString().trimmed();
    const QString pin = record.value(u"pin"_s).toString();
    const QString role = record.value(u"role"_s).toString();
    const bool active = record.value(u"active"_s, true).toBool();
    if (name.isEmpty())
        return fail(tr("The employee needs a name."));
    if (!QStringList{u"server"_s, u"bartender"_s, u"cashier"_s, u"host"_s, u"busser"_s, u"driver"_s, u"manager"_s, u"admin"_s}
             .contains(role))
        return fail(tr("Choose a role."));
    if (index < 0 && pin.isEmpty())
        return fail(tr("New employees need a PIN."));
    static const QRegularExpression digits(u"^[0-9]{4,8}$"_s);
    if (!pin.isEmpty() && !digits.match(pin).hasMatch())
        return fail(tr("PINs are 4 to 8 digits."));
    if (!pin.isEmpty()) {
        for (int i = 0; i < int(s_->employees.size()); ++i) {
            if (i != index && s_->employees[i].pinHash == hashPin(pin, s_->employees[i].pinSalt))
                return fail(tr("Someone else already uses that PIN."));
        }
    }
    std::set<std::string> allow, deny;
    for (const char *p : AllPermissions) {
        const QString v = record.value(u"perm:"_s + QString::fromLatin1(p)).toString();
        if (v == u"allow")
            allow.insert(p);
        else if (v == u"deny")
            deny.insert(p);
    }
    Employee check;
    check.role = ss(role);
    check.allow = allow;
    check.deny = deny;
    const bool isSelf = index >= 0 && user() && s_->employees[index].id == user()->id;
    if (isSelf && (!active || !check.can(perm::Manager)))
        return fail(tr("You cannot lock yourself out. Ask another manager."));

    const QString cashMode = record.value(u"cashMode"_s).toString();
    if (!QStringList{QString(), u"serverBank"_s, u"drawer"_s}.contains(cashMode))
        return fail(tr("Choose how this person handles cash."));
    const QString requireName = record.value(u"requireName"_s).toString();
    if (!QStringList{QString(), u"yes"_s, u"no"_s}.contains(requireName))
        return fail(tr("Choose whether this person's phone orders need a name."));
    const QString checkout = record.value(u"checkout"_s).toString();
    if (!QStringList{QString(), u"closeChecks"_s, u"anyTime"_s}.contains(checkout))
        return fail(tr("Choose whether this person may check out with open checks."));
    const QString tipPool = record.value(u"tipPool"_s).toString();
    if (!QStringList{QString(), u"in"_s, u"out"_s}.contains(tipPool))
        return fail(tr("Choose whether this person is in the tip pool."));
    const QString language = record.value(u"language"_s).toString();
    if (!language.isEmpty()
        && std::ranges::none_of(i18n::languages(), [&](const i18n::Language &l) { return l.code == language; }))
        return fail(tr("Choose a language."));
    // Pay: their rate, and other jobs as "bartender 9.00", one per line.
    const double rate = record.value(u"payRate"_s, 0.0).toDouble();
    if (rate < 0 || rate > 1000)
        return fail(tr("Write the pay rate in dollars an hour."));
    std::vector<Job> otherJobs;
    static const QStringList jobRoles{u"server"_s, u"bartender"_s, u"cashier"_s, u"host"_s, u"busser"_s, u"driver"_s, u"manager"_s, u"admin"_s};
    for (const QString &line : record.value(u"otherJobs"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
        const QStringList parts = line.simplified().split(u' ');
        bool ok = parts.size() == 2;
        const double jobRate = ok ? parts[1].toDouble(&ok) : 0;
        const QString jobRole = parts.value(0).toLower();
        if (!ok || jobRate < 0 || !jobRoles.contains(jobRole))
            return fail(tr("Write other jobs like \"bartender 9.00\": %1").arg(line.trimmed()));
        otherJobs.push_back({ss(jobRole), Money::fromCents(std::llround(jobRate * 100))});
    }
    Employee e = index >= 0 ? s_->employees[index] : Employee{};
    e.payRate = Money::fromCents(std::llround(rate * 100));
    e.otherJobs = otherJobs;
    e.language = ss(language);
    e.textSize = std::clamp(record.value(u"textSize"_s, e.textSize).toInt(), 80, 160);
    e.leftHanded = record.value(u"leftHanded"_s, e.leftHanded).toBool();
    e.startPage = ss(record.value(u"startPage"_s, qs(e.startPage)).toString());
    e.cashMode = ss(cashMode);
    e.requireName = ss(requireName);
    e.checkout = ss(checkout);
    e.tipPool = ss(tipPool);
    e.allow = allow;
    e.deny = deny;
    e.name = ss(name);
    e.role = ss(role);
    e.active = active;
    e.training = record.value(u"training"_s, e.training).toBool();
    if (index < 0)
        e.id = ss(uniqueId(record.value(u"id"_s).toString().trimmed().isEmpty() ? name : record.value(u"id"_s).toString(),
                           s_->employees, [](const Employee &x) { return x.id; }, -1));
    if (!pin.isEmpty()) {
        e.pinSalt = newSalt();
        e.pinHash = hashPin(pin, e.pinSalt);
    }
    if (index >= 0) {
        s_->employees[index] = e;
    } else {
        s_->employees.push_back(e);
        index = int(s_->employees.size()) - 1;
    }
    if (s_->sink)
        s_->sink->saveEmployee(s_->employees[index]);
    emit s_->staffChanged();
    return true;
}

QVariantMap PosService::menuRecord(const MenuItem &m) const
{
    QVariantMap r = toJson(m).toVariantMap();
    QStringList groups;
    for (const std::string &g : m.modifierGroups)
        groups << qs(g);
    r.insert(u"modifierGroups"_s, groups.join(u", "));
    QStringList codes;
    for (const std::string &b : m.barcodes)
        codes << qs(b);
    r.insert(u"barcodes"_s, codes.join(u", "));
    QStringList prices;
    for (const auto &[period, price] : m.periodPrices)
        prices << u"%1 = %2"_s.arg(qs(period), qs(price.toString()));
    r.insert(u"periodPrices"_s, prices.join(u'\n'));
    QStringList recipe;
    for (const RecipeLine &line : m.recipe)
        recipe << u"%1 %2"_s.arg(qs(line.ingredientId), QString::number(line.quantity, 'g', 8));
    r.insert(u"recipe"_s, recipe.join(u'\n'));
    r.remove(u"autoSoldOut"_s);
    r.insert(u"kitchenName"_s, qs(m.kitchenName));
    r.insert(u"kitchenColor"_s, qs(m.kitchenColor));
    r.insert(u"kitchenHide"_s, m.kitchenHide);
    r.insert(u"kioskHide"_s, m.kioskHide);
    r.insert(u"takeoutPrice"_s, m.takeoutPrice.cents() / 100.0);
    r.insert(u"deliveryPrice"_s, m.deliveryPrice.cents() / 100.0);
    r.insert(u"noDiscount"_s, m.noDiscount);
    r.insert(u"noStaffDiscount"_s, m.noStaffDiscount);
    r.insert(u"description"_s, qs(m.description));
    r.insert(u"image"_s, qs(m.image));
    r.insert(u"ticketCapacity"_s, m.ticketCapacity);
    r.insert(u"eventAt"_s, m.eventAt > 0 ? QDateTime::fromMSecsSinceEpoch(m.eventAt).toString(u"yyyy-MM-dd HH:mm"_s)
                                         : QString());
    for (const char16_t *k : {u"family", u"printer"}) {
        if (!r.contains(QString::fromUtf16(k)))
            r.insert(QString::fromUtf16(k), QString());
    }
    r.insert(u"modifier"_s, m.isModifier);
    r.insert(u"available"_s, m.available);
    return r;
}

namespace {
const char *const kDayNames[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

QString daysText(int mask)
{
    if ((mask & 0x7F) == 0x7F)
        return {};
    QStringList out;
    for (int d = 0; d < 7; ++d)
        if (mask & (1 << d))
            out << QString::fromLatin1(kDayNames[d]).replace(0, 1, QString::fromLatin1(kDayNames[d]).left(1).toUpper());
    return out.join(u", "_s);
}

// "Mon-Fri", "Tue", "Sat, Sun"; empty: every day; -1 if not understood.
int daysFrom(const QString &text)
{
    if (text.trimmed().isEmpty())
        return 0x7F;
    const auto dayOf = [](QString t) {
        t = t.trimmed().left(3).toLower();
        for (int d = 0; d < 7; ++d)
            if (t == QString::fromLatin1(kDayNames[d]))
                return d;
        return -1;
    };
    int mask = 0;
    for (const QString &part : text.split(u',', Qt::SkipEmptyParts)) {
        const QStringList range = part.split(u'-');
        const int a = dayOf(range.value(0)), b = range.size() > 1 ? dayOf(range.value(1)) : a;
        if (a < 0 || b < 0)
            return -1;
        for (int d = a;; d = (d + 1) % 7) {
            mask |= 1 << d;
            if (d == b)
                break;
        }
    }
    return mask;
}

int minuteFrom(const QString &text)
{
    const QTime t = QTime::fromString(text.trimmed(), u"H:mm"_s);
    return t.isValid() ? t.hour() * 60 + t.minute() : -1;
}
} // namespace

QVariantMap PosService::promotionRecord(const PosSettings::Promotion &p) const
{
    QStringList families, items;
    for (const std::string &f : p.families) families << qs(f);
    for (const std::string &i : p.items) items << qs(i);
    return {{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)}, {u"active"_s, p.active}, {u"families"_s, families.join(u", "_s)},
            {u"items"_s, items.join(u", "_s)}, {u"percent"_s, double(p.percentBp) / 100.0}, {u"buy"_s, p.buy},
            {u"get"_s, p.get}, {u"start"_s, clockText(p.startMinute)}, {u"end"_s, clockText(p.endMinute)},
            {u"days"_s, daysText(p.days)}, {u"code"_s, qs(p.code)}, {u"amount"_s, double(p.amount.cents()) / 100.0},
            {u"minimum"_s, double(p.minimum.cents()) / 100.0}, {u"fromDay"_s, qs(p.fromDay)}, {u"untilDay"_s, qs(p.untilDay)},
            {u"maxUses"_s, p.maxUses}, {u"uses"_s, p.uses}};
}

bool PosService::savePromotionRecord(int index, const QVariantMap &r)
{
    auto &list = s_->settings.promotions;
    if (index >= int(list.size()))
        return false;
    PosSettings::Promotion p;
    const QString name = r.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The promotion needs a name."));
    p.name = ss(name);
    p.active = r.value(u"active"_s, true).toBool();
    for (QString f : r.value(u"families"_s).toString().split(u',', Qt::SkipEmptyParts))
        if (!(f = f.trimmed()).isEmpty()) p.families.push_back(ss(f));
    for (QString i : r.value(u"items"_s).toString().split(u',', Qt::SkipEmptyParts)) {
        if ((i = i.trimmed()).isEmpty())
            continue;
        if (!findItem(i))
            return fail(tr("There is no menu item '%1'.").arg(i));
        p.items.push_back(ss(i));
    }
    p.code = ss(r.value(u"code"_s).toString().trimmed().toUpper());
    if (p.code.find(' ') != std::string::npos)
        return fail(tr("A coupon code is one word, e.g. SAVE5."));
    for (int k = 0; k < int(list.size()); ++k)
        if (k != index && !p.code.empty() && list[k].code == p.code)
            return fail(tr("%1 already uses the code %2.").arg(qs(list[k].name), qs(p.code)));
    // No categories or items: the whole order (a coupon; a by-itself one only when it says so).
    if (p.families.empty() && p.items.empty() && p.code.empty())
        return fail(tr("Say which categories or items it covers."));
    p.percentBp = std::clamp<std::int64_t>(std::llround(r.value(u"percent"_s).toDouble() * 100.0), 0, 10000);
    p.amount = Money::fromCents(std::max<std::int64_t>(0, std::llround(r.value(u"amount"_s).toDouble() * 100.0)));
    p.minimum = Money::fromCents(std::max<std::int64_t>(0, std::llround(r.value(u"minimum"_s).toDouble() * 100.0)));
    if (p.percentBp <= 0 && p.amount.cents() <= 0)
        return fail(tr("How much off? (50 = half price, or an amount)"));
    if (p.amount.cents() > 0 && p.buy > 0)
        return fail(tr("Buy-and-get takes a percent off, not an amount."));
    for (const auto &[key, day] : {std::pair{u"fromDay", &p.fromDay}, std::pair{u"untilDay", &p.untilDay}}) {
        const QString text = r.value(QString::fromUtf16(key)).toString().trimmed();
        const QDate d = QDate::fromString(text, Qt::ISODate);
        if (!text.isEmpty() && !d.isValid())
            return fail(tr("Write days like 2026-12-31."));
        *day = text.isEmpty() ? std::string() : d.toString(Qt::ISODate).toStdString();
    }
    if (!p.fromDay.empty() && !p.untilDay.empty() && p.untilDay < p.fromDay)
        return fail(tr("The last day is before the first."));
    p.maxUses = std::max(0, r.value(u"maxUses"_s).toInt());
    p.uses = std::max(0, r.value(u"uses"_s, index >= 0 ? list[index].uses : 0).toInt());
    p.buy = std::max(0, r.value(u"buy"_s).toInt());
    p.get = std::max(0, r.value(u"get"_s).toInt());
    if (p.buy > 0 && p.get <= 0)
        return fail(tr("Buy %1 and get how many?").arg(p.buy));
    p.startMinute = minuteFrom(r.value(u"start"_s).toString().isEmpty() ? u"0:00"_s : r.value(u"start"_s).toString());
    p.endMinute = minuteFrom(r.value(u"end"_s).toString().isEmpty() ? u"0:00"_s : r.value(u"end"_s).toString());
    if (p.startMinute < 0 || p.endMinute < 0)
        return fail(tr("Write times like 15:00."));
    p.days = daysFrom(r.value(u"days"_s).toString());
    if (p.days <= 0)
        return fail(tr("Write days like Mon-Fri or Tue, Thu."));
    if (index >= 0) {
        p.id = list[index].id;
        list[index] = p;
    } else {
        const QString wanted = r.value(u"id"_s).toString().trimmed();
        p.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, list, [](const PosSettings::Promotion &x) { return x.id; }, -1));
        list.push_back(p);
    }
    settingsChanged();
    return true;
}

QStringList PosService::groupIds() const
{
    QStringList out;
    for (const ModifierGroup &g : s_->settings.modifierGroups)
        out << qs(g.id);
    return out;
}

QVariantList PosService::accountKeys() const
{
    QVariantList out;
    const auto add = [&](const QString &key, const QString &text) {
        if (std::ranges::none_of(out, [&](const QVariant &x) { return x.toMap()[u"value"_s] == key; }))
            out.append(QVariantMap{{u"value"_s, key}, {u"text"_s, text}});
    };
    add(u"sales"_s, tr("Sales"));
    std::vector<std::string> families;
    for (const MenuItem &m : s_->menu)
        if (!m.family.empty() && std::ranges::find(families, m.family) == families.end())
            families.push_back(m.family);
    std::ranges::sort(families);
    for (const std::string &f : families)
        add(u"sales:"_s + qs(f).toLower(), tr("Sales: %1").arg(qs(f)));
    add(u"tax"_s, tr("Tax"));
    for (const auto &[key, text] : {std::pair{"food", "Food"}, {"alcohol", "Alcohol"}, {"merchandise", "Merchandise"}, {"room", "Room"}})
        add(u"tax:"_s + QLatin1StringView(key), tr("Tax: %1").arg(QCoreApplication::translate("PosService", text)));
    add(u"gratuity"_s, tr("Gratuity"));
    add(u"tips"_s, tr("Tips"));
    add(u"discounts"_s, tr("Discounts"));
    add(u"staffMeals"_s, tr("Staff meals"));
    add(u"rounding"_s, tr("Cash rounding"));
    add(u"giftCardsSold"_s, tr("Gift cards sold"));
    for (const Tender &t : s_->settings.tenders)
        add(u"tender:"_s + qs(t.id), tr("Paid by %1").arg(qs(t.name)));
    for (const auto &[key, account] : s_->settings.accounts)   // any other already set
        add(qs(key), qs(key));
    return out;
}

QStringList PosService::periodIds() const
{
    QStringList out;
    for (const MealPeriod &m : s_->settings.mealPeriods)
        out << qs(m.id);
    return out;
}

bool PosService::saveModifierGroupRecord(int index, const QVariantMap &record)
{
    auto &list = s_->settings.modifierGroups;
    if (index >= int(list.size()))
        return false;
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The group needs a name."));
    ModifierGroup g;
    g.name = ss(name);
    g.min = std::max(0, record.value(u"min"_s).toInt());
    g.max = std::max(0, record.value(u"max"_s).toInt());
    g.menuItems = record.value(u"menuItems"_s).toBool();
    g.askHow = record.value(u"askHow"_s).toBool();
    if (g.max > 0 && g.min > g.max)
        return fail(tr("It can't require more choices than it allows."));
    for (QString line : record.value(u"options"_s).toString().split(u'\n', Qt::SkipEmptyParts)) {
        // "Ranch + 0.50 | RNCH": the kitchen's name after |, or - for none.
        QString kitchen;
        if (const qsizetype bar = line.indexOf(u'|'); bar >= 0) {
            kitchen = line.mid(bar + 1).trimmed();
            line = line.left(bar);
        }
        // "*Onion": it comes on the item.
        const bool included = line.trimmed().startsWith(u'*');
        if (included)
            line = line.trimmed().mid(1);
        const qsizetype plus = line.lastIndexOf(u'+');
        QString optName = (plus > 0 ? line.left(plus) : line).trimmed();
        Money price;
        if (plus > 0) {
            bool ok = false;
            const double p = line.mid(plus + 1).trimmed().remove(qs(s_->settings.currencySymbol)).toDouble(&ok);
            if (!ok || p < 0)
                return fail(tr("Write option prices like \"Onion Rings + 1.50\"."));
            price = Money::fromCents(std::llround(p * 100.0));
        }
        // A combo's choices are menu items, by name.
        std::string itemId;
        if (g.menuItems && !optName.isEmpty()) {
            for (const MenuItem &m : s_->menu) {
                if (QString::compare(qs(m.name), optName, Qt::CaseInsensitive) == 0) {
                    itemId = m.id;
                    break;
                }
            }
            if (itemId.empty())
                return fail(tr("\"%1\" isn't on the menu. Use a menu item's name, or turn off "
                               "\"Options are menu items\".").arg(optName));
        }
        if (!optName.isEmpty())
            g.options.push_back({ss(optName), price, kitchen == u"-" ? std::string() : ss(kitchen), kitchen == u"-",
                                 itemId, included});
    }
    if (g.options.empty())
        return fail(tr("Add the options, one per line."));
    if (g.min > int(g.options.size()))
        return fail(tr("It requires more choices than it has options."));
    if (index >= 0) {
        g.id = list[index].id;
        list[index] = g;
    } else {
        const QString wanted = record.value(u"id"_s).toString().trimmed();
        g.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, list, [](const ModifierGroup &x) { return x.id; }, -1));
        list.push_back(g);
    }
    settingsChanged();
    return true;
}

bool PosService::saveMealPeriodRecord(int index, const QVariantMap &record)
{
    auto &list = s_->settings.mealPeriods;
    if (index >= int(list.size()))
        return false;
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The meal period needs a name."));
    const int start = clockMinutes(record.value(u"start"_s).toString());
    if (start < 0)
        return fail(tr("Type the start as a 24-hour time, like 16:30."));
    for (int i = 0; i < int(list.size()); ++i) {
        if (i != index && list[i].start == start)
            return fail(tr("%1 already starts at %2.").arg(qs(list[i].name), clockText(start)));
    }
    MealPeriod m{{}, ss(name), start};
    if (index >= 0) {
        m.id = list[index].id;
        list[index] = m;
    } else {
        const QString wanted = record.value(u"id"_s).toString().trimmed();
        m.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, list, [](const MealPeriod &x) { return x.id; }, -1));
        list.push_back(m);
    }
    std::ranges::sort(list, {}, &MealPeriod::start);
    settingsChanged();
    return true;
}

bool PosService::saveTenderRecord(int index, const QVariantMap &record)
{
    if (index >= int(s_->settings.tenders.size()))
        return false;
    const QString name = record.value(u"name"_s).toString().trimmed();
    if (name.isEmpty())
        return fail(tr("The payment type needs a name."));
    const double percent = number(record, u"percent");
    const TenderKind kind = tenderKindFromString(ss(record.value(u"kind"_s).toString()));
    if (kind == TenderKind::Discount && (percent <= 0 || percent > 100))
        return fail(tr("Discounts are between 0 and 100%."));
    Tender t;
    t.name = ss(name);
    t.kind = kind;
    t.percentBp = kind == TenderKind::Discount ? std::llround(percent * 100.0) : 0;
    t.staffMeal = kind == TenderKind::Discount && record.value(u"staffMeal"_s).toBool();
    if (index >= 0) {
        t.id = s_->settings.tenders[index].id;
        s_->settings.tenders[index] = t;
    } else {
        const QString wanted = record.value(u"id"_s).toString().trimmed();
        t.id = ss(uniqueId(wanted.isEmpty() ? name : wanted, s_->settings.tenders, [](const Tender &x) { return x.id; }, -1));
        s_->settings.tenders.push_back(t);
    }
    settingsChanged();
    return true;
}

bool PosService::savePrinterRecord(int index, const QVariantMap &record)
{
    if (index >= int(s_->settings.printers.size()))
        return false;
    PrinterConfig p = printerFromJson(QJsonObject::fromVariantMap(record));
    if (p.name.empty() || record.value(u"name"_s).toString().trimmed().isEmpty())
        return fail(tr("The printer needs a name."));
    if (!QStringList{u"network"_s, u"cups"_s, u"file"_s, u"none"_s}.contains(qs(p.type)))
        return fail(tr("Choose how the printer is connected."));
    if (p.type == "network" && p.host.empty())
        return fail(tr("Network printers need an address."));
    if (p.type == "file" && p.path.empty())
        return fail(tr("Choose a file to print into."));
    if (index >= 0) {
        const PrinterConfig &was = s_->settings.printers[index];
        p.id = was.id;
        // Learned about this printer; a new address may be another printer.
        p.reportsStatus = was.reportsStatus && was.type == p.type && was.host == p.host && was.port == p.port;
        s_->settings.printers[index] = p;
    } else {
        const QString wanted = record.value(u"id"_s).toString().trimmed();
        p.id = ss(uniqueId(wanted.isEmpty() ? qs(p.name) : wanted, s_->settings.printers,
                           [](const PrinterConfig &x) { return x.id; }, -1));
        s_->settings.printers.push_back(p);
    }
    settingsChanged();
    return true;
}

bool PosService::adminDelete(const QString &panel, int index)
{
    if (!require(perm::Manager, tr("Changing settings")))
        return false;
    if (panel == u"menu" && index >= 0 && index < int(s_->menu.size())) {
        const std::string id = s_->menu[index].id;
        s_->menu.erase(s_->menu.begin() + index);
        if (s_->sink) {
            s_->sink->deleteMenuItem(id);
            for (int i = index; i < int(s_->menu.size()); ++i)
                s_->sink->saveMenuItem(s_->menu[i], i);   // positions shifted
        }
    } else if (panel == u"employees" && index >= 0 && index < int(s_->employees.size())) {
        // Staff are deactivated, not erased: their sales and hours keep a name.
        if (user() && s_->employees[index].id == user()->id)
            return fail(tr("You cannot remove yourself."));
        s_->employees[index].active = false;
        if (s_->sink)
            s_->sink->saveEmployee(s_->employees[index]);
        emit s_->staffChanged();
    } else if (panel == u"tenders" && index >= 0 && index < int(s_->settings.tenders.size())) {
        s_->settings.tenders.erase(s_->settings.tenders.begin() + index);
        settingsChanged();
    } else if (panel == u"punches") {
        return deletePunchRecord(index, QString());
    } else if (panel == u"cardReaders" && index >= 0 && index < int(s_->settings.stripeReaders.size())) {
        // Forgotten here (it stays in Stripe); screens that used it have none.
        const std::string gone = "counter:" + s_->settings.stripeReaders[index].id;
        for (TerminalConfig &t : s_->settings.terminals)
            if (t.cardReader == gone)
                t.cardReader.clear();
        s_->settings.stripeReaders.erase(s_->settings.stripeReaders.begin() + index);
        settingsChanged();
    } else if (panel == u"vendors" && index >= 0 && index < int(s_->settings.vendors.size())) {
        s_->settings.vendors.erase(s_->settings.vendors.begin() + index);
        settingsChanged();
    } else if (panel == u"promotions" && index >= 0 && index < int(s_->settings.promotions.size())) {
        s_->settings.promotions.erase(s_->settings.promotions.begin() + index);
        settingsChanged();
    } else if (panel == u"inventory" && index >= 0 && index < int(s_->ingredients.size())) {
        const std::string id = s_->ingredients[index].id;
        s_->ingredients.erase(s_->ingredients.begin() + index);
        if (s_->sink) {
            s_->sink->deleteIngredient(id);
            for (int i = index; i < int(s_->ingredients.size()); ++i)
                s_->sink->saveIngredient(s_->ingredients[i], i);
        }
        refreshSoldOut();
    } else if (panel == u"modifierGroups" && index >= 0 && index < int(s_->settings.modifierGroups.size())) {
        s_->settings.modifierGroups.erase(s_->settings.modifierGroups.begin() + index);
        settingsChanged();
    } else if (panel == u"mealPeriods" && index >= 0 && index < int(s_->settings.mealPeriods.size())) {
        s_->settings.mealPeriods.erase(s_->settings.mealPeriods.begin() + index);
        settingsChanged();
    } else if (panel == u"terminals" && index >= 0 && index < int(s_->settings.terminals.size())) {
        s_->settings.terminals.erase(s_->settings.terminals.begin() + index);
        settingsChanged();
    } else if (panel == u"printers" && index >= 0 && index < int(s_->settings.printers.size())) {
        s_->settings.printers.erase(s_->settings.printers.begin() + index);
        settingsChanged();
    } else {
        return fail(tr("That cannot be removed."));
    }
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(panel == u"employees" ? tr("Deactivated") : tr("Removed"));
    return true;
}

void PosShared::saveSettings()
{
    if (sink)
        sink->saveSettings(settings);
    ++adminRevision;
    emit adminChanged();
}

// --- pairing devices -------------------------------------------------------------

namespace {
// Crockford base32: no I, L, O or U to mix up. 10 characters = 50 bits,
// stretched by the key derivation on both sides (see net/pairing).
QString newPairingCode()
{
    static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    QString code;
    for (int i = 0; i < 10; ++i) {
        if (i == 5)
            code += u'-';
        code += QChar::fromLatin1(alphabet[QRandomGenerator::system()->bounded(32)]);
    }
    return code;
}
} // namespace

QString PosShared::startPairing()
{
    pairing = Pairing{newPairingCode(), now() + 10 * 60 * 1000};
    ++adminRevision;
    emit adminChanged();
    // Take the code off the manager screens when it runs out.
    QTimer::singleShot(10 * 60 * 1000 + 500, this, [this, code = pairing->code] {
        if (pairing && pairing->code == code && !activePairing()) {
            pairing.reset();
            ++adminRevision;
            emit adminChanged();
        }
    });
    return pairing->code;
}

bool PosService::factoryReset(const QString &confirm)
{
    if (!require(perm::Manager, tr("Factory reset")))
        return false;
    if (confirm.trimmed() != u"RESET")
        return fail(tr("Type RESET to confirm."));
    if (!s_->requestFactoryReset)
        return fail(tr("A factory reset is done on the computer that keeps the data."));
    if (!s_->requestFactoryReset())
        return fail(tr("The backup failed, so nothing was reset."));
    emit notice(tr("Backed up. Resetting and restarting..."));
    return true;
}

bool PosService::backupNow()
{
    if (!require(perm::Manager, tr("Backups")))
        return false;
    if (!s_->requestBackup || !s_->requestBackup())
        return fail(tr("Backups aren't set up on this store, or one is already running."));
    emit notice(tr("Backing up..."));
    return true;
}

bool PosService::startPairing()
{
    if (!require(perm::Manager, tr("Pairing a device")))
        return false;
    s_->startPairing();
    emit notice(tr("Type %1 on the new device. The code works once, for 10 minutes.").arg(s_->pairing->code));
    return true;
}

bool PosService::stopPairing()
{
    if (!require(perm::Manager, tr("Pairing a device")))
        return false;
    s_->pairing.reset();
    ++s_->adminRevision;
    emit s_->adminChanged();
    return true;
}

QString PosService::terminalLook() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.look);
    return {};
}

QString PosService::terminalStartCategory() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.startCategory);
    return {};
}

QString PosService::terminalKeyboard() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.keyboard);
    return {};
}

QString PosService::screenMode() const
{
    for (const TerminalConfig &t : s_->settings.terminals) {
        if (qs(t.name) == terminal_)
            return qs(t.screen);
    }
    return {};
}

QVariantMap PosService::pairingInfo() const
{
    const PosShared::Pairing *p = s_->activePairing();
    if (!p || !can(QString::fromLatin1(perm::Manager)))
        return {{u"active"_s, false}};
    const QString until = i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(p->expires).time(), QLocale::ShortFormat);
    return {{u"active"_s, true}, {u"code"_s, p->code}, {u"until"_s, until}};
}

void PosService::settingsChanged()
{
    if (s_->sink)
        s_->sink->saveSettings(s_->settings);
    // Tax changes re-total open checks.
    emit checkChanged();
    emit s_->checksChanged();
}

} // namespace vt::app
