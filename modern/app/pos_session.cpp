#include "app/pos_session.hh"

#include "app/i18n.hh"

#include <QHash>

using namespace Qt::StringLiterals;

namespace vt::app {

QString PosSession::formatCents(qint64 cents) const
{
    const qint64 a = cents < 0 ? -cents : cents;
    const QString s = u"%1%2.%3"_s.arg(currencySymbol()).arg(a / 100).arg(a % 100, 2, 10, QChar(u'0'));
    return cents < 0 ? u"-"_s + s : s;
}

const QStringList &PosSession::stateKeys()
{
    static const QStringList keys = {
        u"terminalName"_s, u"loggedIn"_s, u"userName"_s, u"userRole"_s, u"permissions"_s, u"clockedIn"_s,
        u"clockedInSince"_s, u"storeName"_s, u"currencySymbol"_s, u"customColors"_s, u"priceChanges"_s, u"pinLength"_s, u"entry"_s, u"entryAmount"_s,
        u"entryGuests"_s, u"textEntry"_s, u"pendingQualifier"_s, u"pendingTable"_s, u"hasCheck"_s, u"check"_s, u"tableChecks"_s, u"undoText"_s,
        u"lines"_s, u"totals"_s, u"payments"_s, u"selectedLine"_s, u"selectedPayment"_s, u"openChecks"_s, u"floor"_s, u"deliveries"_s, u"drivers"_s, u"popularItems"_s, u"stockLeft"_s, u"dashboard"_s, u"checklists"_s,
        u"checkFilter"_s, u"kitchenTickets"_s, u"drawer"_s, u"day"_s, u"days"_s, u"adminRevision"_s,
        u"tipsOwed"_s, u"mealPeriods"_s, u"pairing"_s, u"screenMode"_s, u"terminalLook"_s, u"terminalKeyboard"_s, u"terminalStartCategory"_s, u"cardReader"_s, u"readerToken"_s, u"counterCharge"_s, u"receiptOffer"_s,
        u"closedChecks"_s, u"staff"_s, u"checkHistory"_s, u"choosing"_s, u"weighing"_s, u"setup"_s, u"soldOut"_s, u"menuItems"_s, u"menuCategories"_s, u"choiceGroups"_s, u"menuTemplates"_s, u"menuProblems"_s, u"menuUndoText"_s, u"cardAlerts"_s, u"onBreakSince"_s,
        u"customers"_s, u"customer"_s, u"giftCard"_s, u"waitlist"_s, u"customerPrompt"_s, u"schedule"_s, u"nextShift"_s, u"rangeReport"_s, u"expoTickets"_s, u"approval"_s, u"training"_s, u"autoLogoutMinutes"_s, u"phoneLockSeconds"_s, u"screenSaverMinutes"_s, u"storeImages"_s, u"storeLogo"_s, u"kitchenStations"_s, u"kitchenStation"_s, u"messages"_s, u"network"_s, u"printerAlerts"_s, u"language"_s, u"userPrefs"_s, u"storeLanguage"_s, u"selfOrder"_s, u"kioskMenu"_s, u"clockInJobs"_s, u"cashTipsAsk"_s, u"reasonAsk"_s, u"timeClock"_s, u"receiving"_s, u"stockCounting"_s, u"wasteLog"_s, u"storeCheck"_s, u"prepLabels"_s, u"checkSearch"_s,
    };
    return keys;
}

QString PosSession::roleName(const QString &role) const
{
    static const QHash<QString, const char *> names = {
        {u"server"_s, QT_TR_NOOP("Server")}, {u"bartender"_s, QT_TR_NOOP("Bartender")},
        {u"cashier"_s, QT_TR_NOOP("Cashier")}, {u"host"_s, QT_TR_NOOP("Host")}, {u"busser"_s, QT_TR_NOOP("Busser")}, {u"driver"_s, QT_TR_NOOP("Driver")},
        {u"manager"_s, QT_TR_NOOP("Manager")}, {u"admin"_s, QT_TR_NOOP("Admin")},
    };
    const auto it = names.constFind(role);
    return it == names.cend() ? role : tr(*it);
}

QVariantMap PosSession::snapshot() const
{
    const i18n::Scope scope([this] { return language(); });
    return {
        {u"terminalName"_s, terminalName()}, {u"loggedIn"_s, loggedIn()}, {u"userName"_s, userName()},
        {u"userRole"_s, userRole()}, {u"permissions"_s, permissions()}, {u"clockedIn"_s, clockedIn()},
        {u"clockedInSince"_s, clockedInSince()}, {u"storeName"_s, storeName()},
        {u"currencySymbol"_s, currencySymbol()}, {u"customColors"_s, customColors()}, {u"priceChanges"_s, priceChanges()}, {u"pinLength"_s, pinLength()}, {u"entry"_s, entry()},
        {u"entryAmount"_s, entryAmount()}, {u"entryGuests"_s, entryGuests()}, {u"textEntry"_s, textEntry()},
        {u"pendingQualifier"_s, pendingQualifier()}, {u"pendingTable"_s, pendingTable()},
        {u"hasCheck"_s, hasCheck()}, {u"check"_s, checkInfo()}, {u"tableChecks"_s, tableChecks()}, {u"undoText"_s, undoText()}, {u"lines"_s, lines()}, {u"totals"_s, totals()},
        {u"payments"_s, payments()}, {u"selectedLine"_s, selectedLine()}, {u"selectedPayment"_s, selectedPayment()},
        {u"openChecks"_s, openChecks()}, {u"floor"_s, floor()}, {u"deliveries"_s, deliveries()}, {u"drivers"_s, drivers()}, {u"popularItems"_s, popularItems()}, {u"stockLeft"_s, stockLeft()}, {u"dashboard"_s, dashboard()}, {u"checklists"_s, checklists()}, {u"checkFilter"_s, checkFilter()}, {u"kitchenTickets"_s, kitchenTickets()},
        {u"drawer"_s, drawerInfo()}, {u"day"_s, dayInfo()}, {u"days"_s, days()},
        {u"adminRevision"_s, adminRevision()}, {u"tipsOwed"_s, tipsOwed()},
        {u"mealPeriods"_s, mealPeriods()}, {u"pairing"_s, pairingInfo()}, {u"screenMode"_s, screenMode()}, {u"terminalLook"_s, terminalLook()}, {u"terminalKeyboard"_s, terminalKeyboard()}, {u"terminalStartCategory"_s, terminalStartCategory()},
        {u"cardReader"_s, terminalCardReader()}, {u"readerToken"_s, readerToken()}, {u"counterCharge"_s, counterCharge()}, {u"receiptOffer"_s, receiptOffer()},
        {u"closedChecks"_s, closedChecks()}, {u"staff"_s, staff()}, {u"checkHistory"_s, checkHistory()},
        {u"choosing"_s, choosingInfo()}, {u"weighing"_s, weighingInfo()}, {u"setup"_s, setupInfo()}, {u"soldOut"_s, soldOut()}, {u"menuItems"_s, menuItems()}, {u"menuCategories"_s, menuCategories()}, {u"choiceGroups"_s, choiceGroups()}, {u"menuTemplates"_s, menuTemplates()}, {u"menuProblems"_s, menuProblems()}, {u"menuUndoText"_s, menuUndoText()}, {u"cardAlerts"_s, cardAlerts()},
        {u"onBreakSince"_s, onBreakSince()},
        {u"customers"_s, customerResults()}, {u"customer"_s, customerInfo()}, {u"giftCard"_s, giftCardInfo()},
        {u"waitlist"_s, waitlistInfo()}, {u"customerPrompt"_s, customerPrompt()},
        {u"schedule"_s, scheduleInfo()}, {u"nextShift"_s, nextShift()},
        {u"rangeReport"_s, rangeReport()}, {u"expoTickets"_s, expoTickets()},
        {u"approval"_s, approvalInfo()}, {u"training"_s, training()}, {u"autoLogoutMinutes"_s, autoLogoutMinutes()}, {u"phoneLockSeconds"_s, phoneLockSeconds()},
        {u"screenSaverMinutes"_s, screenSaverMinutes()},
        {u"storeImages"_s, storeImages()}, {u"storeLogo"_s, storeLogo()},
        {u"kitchenStations"_s, kitchenStations()}, {u"kitchenStation"_s, kitchenStation()},
        {u"messages"_s, messages()},
        {u"network"_s, networkInfo()}, {u"printerAlerts"_s, printerAlerts()},
        {u"language"_s, language()}, {u"userPrefs"_s, userPrefs()},
        {u"storeLanguage"_s, storeLanguage()},
        {u"selfOrder"_s, selfOrderInfo()},
        {u"kioskMenu"_s, kioskMenu()},
        {u"clockInJobs"_s, clockInJobs()}, {u"cashTipsAsk"_s, cashTipsAsk()}, {u"reasonAsk"_s, reasonAsk()}, {u"timeClock"_s, timeClock()},
        {u"receiving"_s, receiving()}, {u"stockCounting"_s, stockCounting()}, {u"wasteLog"_s, wasteLog()}, {u"storeCheck"_s, storeCheck()}, {u"prepLabels"_s, prepLabels()},
        {u"checkSearch"_s, checkSearch()},
    };
}

QVariantMap PosSession::tableStatus(const QString &label) const
{
    QVariantMap status{{u"open"_s, false}};
    int count = 0;
    qint64 total = 0;
    bool current = false;
    QString busy;
    for (const QVariant &v : openChecks()) {
        const QVariantMap c = v.toMap();
        if (c.value(u"type"_s).toString() != u"dineIn" || c.value(u"label"_s).toString() != label)
            continue;
        if (count++ == 0) {
            status = {{u"open"_s, true}, {u"checkId"_s, c.value(u"id"_s)}, {u"server"_s, c.value(u"server"_s)},
                      {u"guests"_s, c.value(u"guests"_s)}, {u"mine"_s, c.value(u"mine"_s)},
                      {u"since"_s, c.value(u"openedAt"_s)}, {u"longAfter"_s, c.value(u"longAfter"_s)}};
        }
        total += c.value(u"totalCents"_s).toLongLong();
        current = current || c.value(u"current"_s).toBool();
        if (busy.isEmpty())
            busy = c.value(u"busyOn"_s).toString();
    }
    if (count > 0) {
        status.insert(u"checks"_s, count);
        status.insert(u"total"_s, formatCents(total));
        status.insert(u"current"_s, current);
        status.insert(u"busyOn"_s, busy);   // open on another terminal
    }
    // The host stand: needs bussing, held for a party, or pushed together with another table.
    const QVariantMap f = floor().value(label).toMap();
    const QString state = f.value(u"state"_s).toString();
    if (count == 0 && (state == u"dirty" || state == u"reserved")) {
        status.insert(u"floor"_s, state);
        status.insert(u"party"_s, f.value(u"party"_s));
        status.insert(u"partyId"_s, f.value(u"partyId"_s));
        status.insert(u"time"_s, f.value(u"time"_s));
    } else if (count == 0 && f.contains(u"with"_s)) {
        status.insert(u"floor"_s, u"joined"_s);
        status.insert(u"with"_s, f.value(u"with"_s));
        status.insert(u"server"_s, f.value(u"server"_s));
    }
    return status;
}

QVariantList PosSession::splitTargets() const
{
    QVariantList out;
    const QVariantMap check = checkInfo();
    if (check.isEmpty())
        return out;
    const qint64 id = check.value(u"id"_s).toLongLong();
    for (const QVariant &v : openChecks()) {
        const QVariantMap c = v.toMap();
        if (c.value(u"id"_s).toLongLong() == id || c.value(u"label"_s) != check.value(u"label"_s)
            || c.value(u"type"_s) != check.value(u"type"_s))
            continue;
        out.append(QVariantMap{{u"id"_s, c.value(u"id"_s)}, {u"label"_s, tr("Check #%1").arg(c.value(u"id"_s).toLongLong())},
                               {u"total"_s, c.value(u"total"_s)}, {u"count"_s, c.value(u"lineCount"_s)}});
    }
    out.append(QVariantMap{{u"id"_s, qint64(0)}, {u"label"_s, tr("New check")}, {u"total"_s, QString()}, {u"count"_s, 0}});
    return out;
}

} // namespace vt::app
