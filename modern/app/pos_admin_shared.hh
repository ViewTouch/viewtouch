#pragma once

// Shared by the manager screens' sources (pos_admin*.cpp, pos_changes.cpp): the
// schema helpers for field lists, record ids, and how values are shown.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>

namespace vt::app::admin {

using namespace Qt::StringLiterals;
using namespace vt::core;


inline QVariantMap field(const QString &path, const QString &label, const QString &type, const QString &hint = {})
{
    QVariantMap f{{u"path"_s, path}, {u"label"_s, label}, {u"type"_s, type}, {u"group"_s, QString()}};
    if (!hint.isEmpty())
        f.insert(u"hint"_s, hint);
    return f;
}

inline QVariantMap with(QVariantMap f, const QString &key, const QVariant &value)
{
    f.insert(key, value);
    return f;
}

inline QVariantList options(std::initializer_list<std::pair<const char *, const char *>> values)
{
    QVariantList out;
    for (const auto &[value, text] : values)   // the texts are phrases too (tools/i18n.py finds them)
        out.append(QVariantMap{{u"value"_s, QString::fromUtf8(value)},
                               {u"text"_s, QCoreApplication::translate("PosService", text)}});
    return out;
}

// "Store setting" first for a person; each language named in itself.
inline QVariantList languageOptions(bool person)
{
    QVariantList out;
    if (person)
        out.append(QVariantMap{{u"value"_s, QString()}, {u"text"_s, QCoreApplication::translate("PosService", "Store setting")}});
    for (const i18n::Language &l : i18n::languages())
        out.append(QVariantMap{{u"value"_s, l.code}, {u"text"_s, l.name}});
    return out;
}

inline QString slug(const QString &text)
{
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QString s = text.toLower().normalized(QString::NormalizationForm_KD);
    s.replace(nonWord, u"-"_s);
    while (s.startsWith(u'-')) s.remove(0, 1);
    while (s.endsWith(u'-')) s.chop(1);
    return s.isEmpty() ? u"item"_s : s;
}

template <typename Container, typename IdOf>
inline QString uniqueId(const QString &wanted, const Container &items, IdOf idOf, int skipIndex)
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
inline QVariantMap permField(const char *permission, const QString &label)
{
    return with(with(field(u"perm:"_s + QString::fromLatin1(permission), label, u"enum"_s, u"Permissions"_s),
                     u"options"_s, options({{"", "As the role"}, {"allow", "Yes"}, {"deny", "No"}})),
                u"hint"_s, QString());
}

inline double number(const QVariantMap &r, const char16_t *key)
{
    return r.value(QString::fromUtf16(key)).toString().toDouble();
}

// The jobs that can be in the tip pool, in their usual order.
inline constexpr std::string_view kPoolJobs[] = {"server", "bartender", "busser", "host", "cashier", "driver"};

// A list kept as text, edited as rows (FieldEditor "rows"): each line read
// with `pattern` (a group a column) and written with `format`.
inline QVariantMap column(const QString &label, const char *type, int width = 0, const QVariantList &opts = {},
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

inline QVariantMap asRows(QVariantMap f, const QString &addText, const QVariantList &columns = {},
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
inline QVariantList storeSections(const QVariantList &fields)
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
        {"cash", PosService::tr("Cash"), {"cashMode", "terminalsHaveDrawer", "checkoutNeedsClosedChecks", "blindDrawerCount", "cashDenominations",
          "expenseCategories"}},
        {"reasons", PosService::tr("Voids and comps"), {"askReasons", "voidReasons", "compReasons"}},
        {"hours", PosService::tr("Staff hours"),
         {"laborState", "paidBreaks", "overtimeDailyHours", "doubleTimeDailyHours", "seventhDayRule", "overtimeWeeklyHours", "mealBreakAfterHours", "restBreakEveryHours", "scheduleNoticeDays", "restBetweenShiftsHours", "giftCardCashBelow", "tipAllocation", "weekStartsOn", "scheduleRequired", "clockInEarlyMinutes", "checksPerServerHour"}},
        {"tips", PosService::tr("Tips"),
         {"tipPercents", "gratuityPercent", "gratuityMinGuests", "declareCashTips", "tipOuts", "tipPool"}},
        {"menu", PosService::tr("Menu and prices"), {"extraPercent", "extraCharge", "soldOutBackNextDay"}},
        {"stock", PosService::tr("Inventory"), {"targetFoodCostPercent", "stockCountShowsExpected", "wasteReasons", "labelPrinter"}},
        {"kitchen", PosService::tr("Kitchen"), {"kitchenStations", "kitchenWarnMinutes", "kitchenLateMinutes", "expoSkips", "laterLeadMinutes"}},
        {"togo", PosService::tr("Takeout and delivery"),
         {"requireOrderName", "takeoutMinutes", "deliveryMinutes", "minutesPerOrderWaiting", "deliveryFee", "deliveryFeeToDriver"}},
        {"cards", PosService::tr("Cards"),
         {"stripeSecretKey", "cardCurrency", "cardTipOn", "tabHold", "offlineCards", "offlineCardMax", "offlineCardTotal"}},
        {"screens", PosService::tr("Screens"), {"screenSaverMinutes", "autoLogoutMinutes", "phoneLockSeconds", "tableLongMinutes"}},
        {"display", PosService::tr("Customer display"), {"displayAccent", "displaySlides"}},
        {"loyalty", PosService::tr("Loyalty"), {"loyaltyEnabled", "pointsPerDollar", "rewards"}},
        {"waitlist", PosService::tr("Waitlist and texts"), {"hostsTakePayments", "waitMinutesPerParty", "tableReadyText", "textWebhook"}},
        {"kiosk", PosService::tr("Self-order kiosk"),
         {"kioskSendNow", "kioskSlip", "kioskIdleSeconds", "kioskWelcome", "kioskAskWhere", "kioskAskName", "kioskEasyReach",
          "kioskSizePercent", "kioskBackground", "kioskCard", "kioskGo", "kioskText", "kioskFont"}},
        {"checklists", PosService::tr("Checklists"), {"openingChecklist", "closingChecklist"}},
        {"backups", PosService::tr("Backups"), {"backupCopyDir", "encryptBackups", "backupPassword"}},
        {"accounting", PosService::tr("Franchise and accounting"), {"royaltyPercent", "adFundPercent", "chartOfAccounts"}},
        {"about", PosService::tr("About"),
         {"about.version", "screen.version", "screen.display", "about.machine", "about.started", "about.database",
          "about.backup", "about.system", "about.updates", "about.history", "about.license", "about.source",
          "about.credits"}},
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
        {"screenSaverMinutes", never, 1}, {"autoLogoutMinutes", never, 1}, {"phoneLockSeconds", {}, 1}, {"tableLongMinutes", {}, 5},
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


const char *const kDayNames[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

inline QString daysText(int mask)
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
inline int daysFrom(const QString &text)
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

inline int minuteFrom(const QString &text)
{
    return clockMinutes(text);
}


// A manager screen, as the Changes report names it.
inline QString panelName(const QString &panel)
{
    static const QHash<QString, const char *> names = {
        {u"menu"_s, QT_TR_NOOP("Menu items")}, {u"employees"_s, QT_TR_NOOP("Employees")},
        {u"tenders"_s, QT_TR_NOOP("Payment types")}, {u"printers"_s, QT_TR_NOOP("Printers")},
        {u"mealPeriods"_s, QT_TR_NOOP("Meal periods")}, {u"modifierGroups"_s, QT_TR_NOOP("Modifier groups")},
        {u"promotions"_s, QT_TR_NOOP("Promotions")}, {u"punches"_s, QT_TR_NOOP("Time punches")},
        {u"requests"_s, QT_TR_NOOP("Time off and swaps")}, {u"cardReaders"_s, QT_TR_NOOP("Card readers")},
        {u"taxes"_s, QT_TR_NOOP("Taxes")}, {u"store"_s, QT_TR_NOOP("Store Settings")},
        {u"terminals"_s, QT_TR_NOOP("Terminals")}, {u"vendors"_s, QT_TR_NOOP("Vendors")},
        {u"inventory"_s, QT_TR_NOOP("Inventory")}, {u"customers"_s, QT_TR_NOOP("Customers")},
        {u"schedule"_s, QT_TR_NOOP("Schedule")},
    };
    const auto it = names.constFind(panel);
    return it == names.cend() ? panel : QCoreApplication::translate("PosService", *it);
}

// A field's value as the Changes report shows it.
inline QString shown(const QVariant &v, const QVariantMap &field)
{
    const QString type = field.value(u"type"_s).toString();
    if (type == u"password")
        return v.toString().isEmpty() ? QString() : u"••••"_s;
    if (v.typeId() == QMetaType::Bool)
        return v.toBool() ? PosService::tr("yes") : PosService::tr("no");
    for (const QVariant &o : field.value(u"options"_s).toList()) {
        const QVariantMap m = o.toMap();
        if (m.value(u"value"_s).toString() == v.toString())
            return m.value(u"text"_s).toString();
    }
    if (v.typeId() == QMetaType::QVariantList || v.typeId() == QMetaType::QStringList)
        return v.toStringList().join(u", "_s);
    return v.toString().simplified();
}

inline QString recordKey(const QVariantMap &r)
{
    return r.value(u"id"_s).toString();
}


} // namespace vt::app::admin
