// PosService::invoke: every operation by name, for the screens' QML and remote
// terminals (the same names go over the network).

#include "app/pos_service.hh"

#include "app/i18n.hh"
#include "app/pos_json.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

// --- invoke: operations by name (widgets, remote terminals) ----------------------------------

void PosService::invoke(const QString &method, const QVariantList &args, Reply reply)
{
    using Fn = std::function<QVariant(PosService &, const QVariantList &)>;
    static const QHash<QString, Fn> table = {
        {u"pinKey"_s, [](PosService &p, const QVariantList &a) { p.pinKey(a.value(0).toString()); return QVariant(true); }},
        {u"entryKey"_s, [](PosService &p, const QVariantList &a) { p.entryKey(a.value(0).toString()); return QVariant(true); }},
        {u"adjustGuests"_s, [](PosService &p, const QVariantList &a) { p.adjustGuests(a.value(0).toInt()); return QVariant(true); }},
        {u"textKey"_s, [](PosService &p, const QVariantList &a) { p.textKey(a.value(0).toString()); return QVariant(true); }},
        {u"clearEntry"_s, [](PosService &p, const QVariantList &) { p.clearEntry(); return QVariant(true); }},
        {u"login"_s, [](PosService &p, const QVariantList &) { return QVariant(p.login()); }},
        {u"loginWithPin"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.loginWithPin(a.value(0).toString())); }},
        {u"logout"_s, [](PosService &p, const QVariantList &) { p.logout(); return QVariant(true); }},
        {u"clockIn"_s, [](PosService &p, const QVariantList &) { return QVariant(p.clockIn()); }},
        {u"searchChecks"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.searchChecks(a.value(0).toString(), a.value(1, 365).toInt(), a.value(2, 0).toInt())); }},
        {u"selectFoundCheck"_s, [](PosService &p, const QVariantList &a) { p.selectFoundCheck(a.value(0).toLongLong()); return QVariant(true); }},
        {u"reprintCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.reprintCheck(a.value(0).toLongLong())); }},
        {u"receiveDelivery"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.receiveDelivery(a.value(0).toMap())); }},
        {u"printPrepLabels"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.printPrepLabels(a.value(0).toMap())); }},
        {u"problemsSeen"_s, [](PosService &p, const QVariantList &) { return QVariant(p.problemsSeen()); }},
        {u"logWaste"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.logWaste(a.value(0).toMap())); }},
        {u"saveStockCount"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.saveStockCount(a.value(0).toMap())); }},
        {u"setExpenseCategory"_s, [](PosService &p, const QVariantList &a) { p.setExpenseCategory(a.value(0).toString()); return QVariant(true); }},
        {u"clockInAs"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.clockInAs(a.value(0).toString())); }},
        {u"cancelClockIn"_s, [](PosService &p, const QVariantList &) { p.cancelClockIn(); return QVariant(true); }},
        {u"declareCashTips"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.declareCashTips(a.value(0).toString())); }},
        {u"clockOut"_s, [](PosService &p, const QVariantList &) { return QVariant(p.clockOut()); }},
        {u"selectTable"_s, [](PosService &p, const QVariantList &a) { return QVariant(int(p.selectTable(a.value(0).toString()))); }},
        {u"startCheck"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.startCheck(checkTypeFromString(ss(a.value(0).toString())))); }},
        {u"openCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.openCheck(a.value(0).toLongLong())); }},
        {u"openTab"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.openTab(a.value(0).toString())); }},
        {u"releaseCheck"_s, [](PosService &p, const QVariantList &) { p.releaseCheck(); return QVariant(true); }},
        {u"newTableCheck"_s, [](PosService &p, const QVariantList &) { return QVariant(p.newTableCheck()); }},
        {u"splitBySeat"_s, [](PosService &p, const QVariantList &) { return QVariant(p.splitBySeat()); }},
        {u"testPrinter"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.testPrinter(a.value(0).toString(), a.value(1).toBool())); }},
        {u"printTableChecks"_s, [](PosService &p, const QVariantList &) { return QVariant(p.printTableChecks()); }},
        {u"combineTableChecks"_s, [](PosService &p, const QVariantList &) { return QVariant(p.combineTableChecks()); }},
        {u"addItem"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addItem(a.value(0).toString())); }},
        {u"setSelfOrder"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setSelfOrder(a.value(0, true).toBool())); }},
        {u"leaveSelfOrder"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.leaveSelfOrder(a.value(0).toString())); }},
        {u"kioskStart"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.kioskStart(a.value(0).toBool())); }},
        {u"kioskAdd"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.kioskAdd(a.value(0).toString())); }},
        {u"kioskRemove"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.kioskRemove(a.value(0).toLongLong())); }},
        {u"kioskFinish"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.kioskFinish(a.value(0).toMap())); }},
        {u"kioskCancel"_s, [](PosService &p, const QVariantList &) { p.kioskCancel(); return QVariant(true); }},
        {u"storeImage"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.storeImage(a.value(0).toString())); }},
        {u"setQualifier"_s, [](PosService &p, const QVariantList &a) { p.setQualifier(a.value(0).toString()); return QVariant(true); }},
        {u"selectLine"_s, [](PosService &p, const QVariantList &a) { p.selectLine(a.value(0).toLongLong()); return QVariant(true); }},
        {u"selectPayment"_s, [](PosService &p, const QVariantList &a) { p.selectPayment(a.value(0).toLongLong()); return QVariant(true); }},
        {u"setCheckFilter"_s, [](PosService &p, const QVariantList &a) { p.setCheckFilter(a.value(0).toString()); return QVariant(true); }},
        {u"voidItem"_s, [](PosService &p, const QVariantList &) { return QVariant(p.voidItem()); }},
        {u"setLineQuantity"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setLineQuantity(a.value(0).toLongLong(), a.value(1).toInt())); }},
        {u"lineMore"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.changeLineQuantity(a.value(0).toLongLong(), 1)); }},
        {u"lineLess"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.changeLineQuantity(a.value(0).toLongLong(), -1)); }},
        {u"anotherRound"_s, [](PosService &p, const QVariantList &) { return QVariant(p.anotherRound()); }},
        {u"customDiscount"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.customDiscount(a.value(0).toBool())); }},
        {u"timeClockStart"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockStart(a.value(0).toString())); }},
        {u"timeClockAct"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockAct(a.value(0).toString())); }},
        {u"timeClockRequestOff"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockRequestOff(a.value(0).toString(), a.value(1).toString())); }},
        {u"timeClockGiveAway"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockGiveAway(a.value(0).toLongLong())); }},
        {u"timeClockTake"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockTake(a.value(0).toLongLong())); }},
        {u"timeClockCancelRequest"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.timeClockCancelRequest(a.value(0).toLongLong())); }},
        {u"tickChecklist"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.tickChecklist(a.value(0).toString(), a.value(1).toInt())); }},
        {u"moveMenuItem"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.moveMenuItem(a.value(0).toString(), a.value(1).toInt())); }},
        {u"addCustomColor"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addCustomColor(a.value(0).toString())); }},
        {u"setMenuItemColor"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setMenuItemColor(a.value(0).toString(), a.value(1).toString())); }},
        {u"refreshDay"_s, [](PosService &p, const QVariantList &) { emit p.shared()->dayChanged(); return QVariant(true); }},
        {u"cardCharge"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.cardCharge(a.value(0).toString())); }},
        {u"recordCardPayment"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.recordCardPayment(a.value(0).toMap())); }},
        {u"printReceiptOn"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.printReceiptOn(a.value(0).toLongLong(), a.value(1).toString())); }},
        {u"emailReceipt"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.emailReceipt(a.value(0).toLongLong(), a.value(1).toString())); }},
        {u"noReceipt"_s, [](PosService &p, const QVariantList &) { p.noReceipt(); return QVariant(true); }},
        {u"refundPayment"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.refundPayment(a.value(0).toLongLong(), a.value(1).toLongLong(), a.value(2).toLongLong(), a.value(3).toString())); }},
        {u"startCounterCharge"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.startCounterCharge(a.value(0).toString())); }},
        {u"cancelCounterCharge"_s, [](PosService &p, const QVariantList &) { return QVariant(p.cancelCounterCharge()); }},
        {u"presentTestCard"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.presentTestCard(a.value(0).toBool())); }},
        {u"requestReaderToken"_s, [](PosService &p, const QVariantList &) { p.requestReaderToken(); return QVariant(true); }},
        {u"sameAsLastTime"_s, [](PosService &p, const QVariantList &) { return QVariant(p.sameAsLastTime()); }},
        {u"sendOut"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.sendOut(a.value(0).toList(), a.value(1).toString())); }},
        {u"deliveryBack"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.deliveryBack(a.value(0).toLongLong())); }},
        {u"seatPartyAt"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.seatPartyAt(a.value(0).toLongLong(), a.value(1).toStringList(), a.value(2).toString())); }},
        {u"seatWalkIn"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.seatWalkIn(a.value(0).toInt(), a.value(1).toStringList(), a.value(2).toString())); }},
        {u"reserveTables"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.reserveTables(a.value(0).toLongLong(), a.value(1).toStringList())); }},
        {u"setTableState"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setTableState(a.value(0).toString(), a.value(1).toString())); }},
        {u"clockOutPunch"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.clockOutPunch(a.value(0).toLongLong())); }},
        {u"timeClockDone"_s, [](PosService &p, const QVariantList &) { p.timeClockDone(); return QVariant(true); }},
        {u"undoLast"_s, [](PosService &p, const QVariantList &) { return QVariant(p.undoLast()); }},
        {u"repeatLine"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.repeatLine(a.value(0).toLongLong())); }},
        {u"sendOrder"_s, [](PosService &p, const QVariantList &) { return QVariant(p.sendOrder()); }},
        {u"setKitchenStation"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setKitchenStation(a.value(0).toString())); }},
        {u"addWeighed"_s, [](PosService &p, const QVariantList &) { return QVariant(p.addWeighed()); }},
        {u"cancelWeighing"_s, [](PosService &p, const QVariantList &) { return QVariant(p.cancelWeighing()); }},
        {u"addStoreImage"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.addStoreImage(a.value(0).toString(), a.value(1).toString())); }},
        {u"removeStoreImage"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.removeStoreImage(a.value(0).toString())); }},
        {u"setupStore"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setupStore(a.value(0).toString(), a.value(1).toString())); }},
        {u"setupLogo"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setupLogo(a.value(0).toString(), a.value(1).toBool())); }},
        {u"setupTaxes"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setupTaxes(a.value(0).toDouble(), a.value(1).toDouble())); }},
        {u"setupAddItem"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setupAddItem(a.value(0).toString(), a.value(1).toDouble(), a.value(2).toString())); }},
        {u"setupAddEmployee"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setupAddEmployee(a.value(0).toString(), a.value(1).toString(), a.value(2).toString())); }},
        {u"setupRetireSamples"_s, [](PosService &p, const QVariantList &) { return QVariant(p.setupRetireSamples()); }},
        {u"setupFinish"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setupFinish(a.value(0).toBool())); }},
        {u"setDueAt"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setDueAt(a.value(0).toLongLong())); }},
        {u"addComment"_s, [](PosService &p, const QVariantList &) { return QVariant(p.addComment()); }},
        {u"tender"_s, [](PosService &p, const QVariantList &a) {
             const QVariant amount = a.value(1);
             return QVariant(p.tender(a.value(0).toString(), amount.isValid() && !amount.isNull()
                                          ? std::optional<std::int64_t>(amount.toLongLong()) : std::nullopt)); }},
        {u"removePayment"_s, [](PosService &p, const QVariantList &) { return QVariant(p.removePayment()); }},
        {u"closeCheck"_s, [](PosService &p, const QVariantList &) { return QVariant(p.closeCheck()); }},
        {u"printReceipt"_s, [](PosService &p, const QVariantList &) { return QVariant(p.printReceipt()); }},
        {u"noSale"_s, [](PosService &p, const QVariantList &) { return QVariant(p.noSale()); }},
        {u"setCustomer"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setCustomer(a.value(0).toMap())); }},
        {u"splitLine"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.splitLine(a.value(0).toLongLong(), a.value(1).toBool())); }},
        {u"shareLine"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.shareLine(a.value(0).toInt())); }},
        {u"splitEvenly"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.splitEvenly(a.value(0).toInt())); }},
        {u"unshareLine"_s, [](PosService &p, const QVariantList &) { return QVariant(p.unshareLine()); }},
        {u"bumpTicket"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.bumpTicket(a.value(0).toLongLong(), a.value(1).toLongLong(), a.value(2).toString())); }},
        {u"recallTicket"_s, [](PosService &p, const QVariantList &) { return QVariant(p.recallTicket()); }},
        {u"expoBump"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.expoBump(a.value(0).toLongLong(), a.value(1).toLongLong())); }},
        {u"expoRecall"_s, [](PosService &p, const QVariantList &) { return QVariant(p.expoRecall()); }},
        {u"addTip"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addTip(a.value(0).toLongLong())); }},
        {u"setCashTip"_s, [](PosService &p, const QVariantList &) { return QVariant(p.setCashTip()); }},
        {u"leaveHandoffNote"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.leaveHandoffNote(a.value(0).toString())); }},
        {u"readHandoffNotes"_s, [](PosService &p, const QVariantList &) { return QVariant(p.readHandoffNotes()); }},
        {u"setGratuity"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setGratuity(a.value(0).toLongLong())); }},
        {u"payout"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.payout(cashMovementKindFromString(ss(a.value(0).toString())))); }},
        {u"cashOutTips"_s, [](PosService &p, const QVariantList &) { return QVariant(p.cashOutTips()); }},
        {u"toggleBreak"_s, [](PosService &p, const QVariantList &) { return QVariant(p.toggleBreak()); }},
        {u"askForTip"_s, [](PosService &p, const QVariantList &) { return QVariant(p.askForTip()); }},
        {u"approve"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.approve(a.value(0).toString())); }},
        {u"sendMessage"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.sendMessage(a.value(0).toString(), a.value(1).toString(), a.value(2).toLongLong())); }},
        {u"removeMessage"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.removeMessage(a.value(0).toString())); }},
        {u"cancelApproval"_s, [](PosService &p, const QVariantList &) { return QVariant(p.cancelApproval()); }},
        {u"setTraining"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setTraining(a.value(0).toBool())); }},
        {u"toggleTraining"_s, [](PosService &p, const QVariantList &) { return QVariant(p.setTraining(!p.training())); }},
        {u"redeemReward"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.redeemReward(a.value(0).toInt())); }},
        {u"holdCharge"_s, [](PosService &p, const QVariantList &) { return QVariant(p.holdCharge()); }},
        {u"recordCardHold"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.recordCardHold(a.value(0).toMap())); }},
        {u"startCounterHold"_s, [](PosService &p, const QVariantList &) { return QVariant(p.startCounterHold()); }},
        {u"chargeHold"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.chargeHold(a.value(0).toLongLong(), a.value(1).toLongLong())); }},
        {u"releaseHold"_s, [](PosService &p, const QVariantList &) { return QVariant(p.releaseHold()); }},
        {u"scanCode"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.scanCode(a.value(0).toString())); }},
        {u"applyCoupon"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.applyCoupon(a.value(0).toString())); }},
        {u"removeCoupon"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.removeCoupon(a.value(0).toString())); }},
        {u"customerJoin"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.customerJoin(a.value(0).toString())); }},
        {u"sendReceipt"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.sendReceipt(a.value(0).toString(), a.value(1).toString())); }},
        {u"addShift"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addShift(a.value(0).toMap())); }},
        {u"requestForecast"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.requestForecast(a.value(0).toInt())); }},
        {u"requestRangeReport"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.requestRangeReport(a.value(0).toString(), a.value(1).toString(), a.value(2).toString(),
                                                  a.value(3).toString(), a.value(4).toBool())); }},
        {u"removeShift"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.removeShift(a.value(0).toLongLong())); }},
        {u"clockInEmployee"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.clockInEmployee(a.value(0).toString())); }},
        {u"setScheduleWeek"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setScheduleWeek(a.value(0).toInt())); }},
        {u"toggleFlag"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.toggleFlag(a.value(0).toString())); }},
        {u"customerTip"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.customerTip(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"backupNow"_s, [](PosService &p, const QVariantList &) { return QVariant(p.backupNow()); }},
        {u"factoryReset"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.factoryReset(a.value(0).toString())); }},
        {u"findCustomers"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.findCustomers(a.value(0).toString())); }},
        {u"selectCustomer"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.selectCustomer(a.value(0).toString())); }},
        {u"useCustomer"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.useCustomer(a.value(0).toString())); }},
        {u"saveCustomer"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.saveCustomer(a.value(0).toMap())); }},
        {u"sellGiftCard"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.sellGiftCard(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"lookupGiftCard"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.lookupGiftCard(a.value(0).toString())); }},
        {u"payWithGiftCard"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.payWithGiftCard(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"addToWaitlist"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addToWaitlist(a.value(0).toMap())); }},
        {u"addReservation"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.addReservation(a.value(0).toMap())); }},
        {u"updateParty"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.updateParty(a.value(0).toLongLong(), a.value(1).toMap())); }},
        {u"checkInParty"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.checkInParty(a.value(0).toLongLong())); }},
        {u"notifyParty"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.notifyParty(a.value(0).toLongLong())); }},
        {u"seatParty"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.seatParty(a.value(0).toLongLong(), a.value(1).toString(), a.value(2).toString())); }},
        {u"partyGone"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.partyGone(a.value(0).toLongLong(), a.value(1).toBool())); }},
        {u"payOnAccount"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.payOnAccount(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"setSeat"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setSeat(a.value(0).toInt())); }},
        {u"saveCategory"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.saveCategory(a.value(0).toMap())); }},
        {u"moveCategory"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.moveCategory(a.value(0).toString(), a.value(1).toInt())); }},
        {u"moveCategoryTo"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.moveCategoryTo(a.value(0).toString(), a.value(1).toInt())); }},
        {u"moveMenuItemTo"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.moveMenuItemTo(a.value(0).toString(), a.value(1).toInt())); }},
        {u"deleteCategory"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.deleteCategory(a.value(0).toString())); }},
        {u"saveMenuItemCard"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.saveMenuItemCard(a.value(0).toMap())); }},
        {u"deleteMenuItemCard"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.deleteMenuItemCard(a.value(0).toString())); }},
        {u"saveChoiceGroup"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.saveChoiceGroup(a.value(0).toMap())); }},
        {u"setItemSizes"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setItemSizes(a.value(0).toString(), a.value(1).toList())); }},
        {u"setCategoryColors"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setCategoryColors(a.value(0).toMap())); }},
        {u"changeMenuItems"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.changeMenuItems(a.value(0).toStringList(), a.value(1).toMap())); }},
        {u"removeMenuItems"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.removeMenuItems(a.value(0).toStringList())); }},
        {u"schedulePrices"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.schedulePrices(a.value(0).toList(), a.value(1).toLongLong(), a.value(2).toString())); }},
        {u"cancelPriceChange"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.cancelPriceChange(a.value(0).toLongLong())); }},
        {u"setMenuPrices"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setMenuPrices(a.value(0).toList())); }},
        {u"duplicateMenuItem"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.duplicateMenuItem(a.value(0).toString())); }},
        {u"applyMenuTemplate"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.applyMenuTemplate(a.value(0).toString())); }},
        {u"setAllergies"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setAllergies(a.value(0).toStringList())); }},
        {u"cardForwarded"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.cardForwarded(a.value(0).toMap())); }},
        {u"seeCardAlert"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.seeCardAlert(a.value(0).toString())); }},
        {u"undoMenuChange"_s, [](PosService &p, const QVariantList &) { return QVariant(p.undoMenuChange()); }},
        {u"menuExport"_s, [](PosService &p, const QVariantList &) { return QVariant(p.menuExport()); }},
        {u"importMenuFile"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.importMenuFile(a.value(0).toMap())); }},
        {u"importMenuRows"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.importMenuRows(a.value(0).toList(), a.value(1).toString(), a.value(2).toBool()) > 0); }},
        {u"addMenuItemsFromText"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.addMenuItemsFromText(a.value(0).toString(), a.value(1).toString()) > 0); }},
        {u"deleteChoiceGroup"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.deleteChoiceGroup(a.value(0).toString())); }},
        {u"setChoice"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setChoice(a.value(0).toString(), a.value(1).toInt(), a.value(2).toString())); }},
        {u"chooseOptionAs"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.chooseOptionAs(a.value(0).toString(), a.value(1).toInt(), a.value(2).toString())); }},
        {u"chooseOption"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.chooseOption(a.value(0).toString(), a.value(1).toInt())); }},
        {u"finishChoosing"_s, [](PosService &p, const QVariantList &) { return QVariant(p.finishChoosing()); }},
        {u"cancelChoosing"_s, [](PosService &p, const QVariantList &) { return QVariant(p.cancelChoosing()); }},
        {u"chooseLine"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.chooseLine(a.value(0).toLongLong())); }},
        {u"setAvailable"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.setAvailable(a.value(0).toString(), a.value(1).toBool())); }},
        {u"setCourse"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.setCourse(a.value(0).toInt())); }},
        {u"fireCourse"_s, [](PosService &p, const QVariantList &) { return QVariant(p.fireCourse()); }},
        {u"fireCourseIn"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.fireCourseIn(a.value(0).toInt())); }},
        {u"startPairing"_s, [](PosService &p, const QVariantList &) { return QVariant(p.startPairing()); }},
        {u"transferCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.transferCheck(a.value(0).toString())); }},
        {u"moveCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.moveCheck(a.value(0).toString())); }},
        {u"mergeCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.mergeCheck(a.value(0).toLongLong())); }},
        {u"reopenCheck"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.reopenCheck(a.value(0).toLongLong())); }},
        {u"stopPairing"_s, [](PosService &p, const QVariantList &) { return QVariant(p.stopPairing()); }},
        {u"openDrawerSession"_s, [](PosService &p, const QVariantList &) { return QVariant(p.openDrawerSession()); }},
        {u"countDrawer"_s, [](PosService &p, const QVariantList &) { return QVariant(p.countDrawer()); }},
        {u"countDrawerById"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.countDrawerById(a.value(0).toLongLong())); }},
        {u"logPageEdits"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.logPageEdits(a.value(0).toStringList(), a.value(1).toStringList(), a.value(2).toStringList())); }},
        {u"giveReason"_s, [](PosService &p, const QVariantList &a) { return QVariant(p.giveReason(a.value(0).toString())); }},
        {u"countDrawerPieces"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.countDrawerPieces(a.value(0).toLongLong(), a.value(1).toList())); }},
        {u"endOfDay"_s, [](PosService &p, const QVariantList &) { return QVariant(p.endOfDay()); }},
        {u"printReport"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.printReport(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"adminSave"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.adminSave(a.value(0).toString(), a.value(1).toInt(), a.value(2).toMap())); }},
        {u"adminDelete"_s, [](PosService &p, const QVariantList &a) {
             return QVariant(p.adminDelete(a.value(0).toString(), a.value(1).toInt())); }},
        // Queries (remote terminals fetch these).
        // Sales, settings, pay rates, customers: managers only, whatever a
        // screen sends (the pages showing them are managers' too).
        {u"report"_s, [](PosService &p, const QVariantList &a) {
             if (!p.can(QString::fromLatin1(perm::Manager)))
                 return QVariant(QVariantMap{{u"title"_s, PosService::tr("Reports are for managers.")}, {u"rows"_s, QVariantList()}});
             return QVariant(p.report(a.value(0).toString(), a.value(1).toLongLong())); }},
        {u"adminFields"_s, [](PosService &p, const QVariantList &a) {
             return p.can(QString::fromLatin1(perm::Manager)) ? QVariant(p.adminFields(a.value(0).toString())) : QVariant(QVariantList()); }},
        {u"adminRecords"_s, [](PosService &p, const QVariantList &a) {
             return p.can(QString::fromLatin1(perm::Manager)) ? QVariant(p.adminRecords(a.value(0).toString())) : QVariant(QVariantList()); }},
        {u"adminNewRecord"_s, [](PosService &p, const QVariantList &a) {
             return p.can(QString::fromLatin1(perm::Manager)) ? QVariant(p.adminNewRecord(a.value(0).toString())) : QVariant(QVariantMap()); }},
    };
    const auto it = table.constFind(method);
    if (it == table.cend()) {
        emit notice(tr("Unknown operation '%1'").arg(method));
        if (reply)
            reply(QVariant(false));
        return;
    }
    // In this terminal's language (on a server, each its own).
    const i18n::Scope language([this] { return this->language(); });
    // Remembered, so a manager's approval can run it again.
    const std::optional<Running> outer = running_;
    if (method != u"approve" && method != u"cancelApproval")
        running_ = Running{method, args};
    const QVariant result = (*it)(*this, args);
    running_ = outer;
    if (reply)
        reply(result);
}

} // namespace vt::app
