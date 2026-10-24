// PosService: the manager screens' records, as maps for QML (and the blank
// record a new one starts from).

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
                          {u"textSize"_s, QString::number(e.textSize)}, {u"leftHanded"_s, e.leftHanded}, {u"colorSafe"_s, e.colorSafe}, {u"startPage"_s, qs(e.startPage)}, {u"buzz"_s, e.buzz},
                          {u"payRate"_s, e.payRate.cents() / 100.0}, {u"otherJobs"_s, [&] {
                               QStringList lines;
                               for (const Job &j : e.otherJobs)
                                   lines << u"%1 %2"_s.arg(qs(j.role), QString::number(j.rate.cents() / 100.0, 'f', 2));
                               return lines.join(u'\n');
                           }()},
                          {u"checkout"_s, qs(e.checkout)}, {u"blindCount"_s, qs(e.blindCount)}, {u"reasons"_s, qs(e.reasons)}, {u"look"_s, qs(e.look)}, {u"tipPool"_s, qs(e.tipPool)}};
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
            r.insert(u"labelWidthMm"_s, p.labelWidthMm);
            r.insert(u"labelHeightMm"_s, p.labelHeightMm);
            const QString how = p.type == "network" ? tr("Network") : p.type == "cups" ? tr("CUPS queue")
                              : p.type == "file" ? tr("Text file") : p.type == "none" ? tr("Off") : qs(p.type);
            add(r, qs(p.name), how + (p.host.empty() ? QString() : u" · "_s + qs(p.host)));
        }
    } else if (panel == u"taxes") {
        const TaxRates &t = s_->settings.tax;
        add({{u"state"_s, qs(s_->settings.laborState)}, {u"zip"_s, qs(s_->settings.storeZip)},
             {u"taxWhere"_s, qs(s_->settings.taxWhere)}, {u"suggested"_s, taxAreaText(taxHere())},
             {u"useSuggested"_s, false},
             {u"food"_s, percentFromPpm(t.foodPpm)}, {u"alcohol"_s, percentFromPpm(t.alcoholPpm)},
             {u"merchandise"_s, percentFromPpm(t.merchandisePpm)}, {u"room"_s, percentFromPpm(t.roomPpm)},
             {u"taxTakeoutFood"_s, t.taxTakeoutFood}, {u"cashRounding"_s, QString::number(t.cashRoundingCents)},
             {u"taxGratuity"_s, t.taxGratuity}},
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
             {u"blindDrawerCount"_s, s_->settings.blindDrawerCount},
             {u"cashDenominations"_s, [&] {
                  QStringList l;
                  for (std::int64_t c : s_->settings.cashDenominations)
                      l << QString::number(double(c) / 100.0, 'f', c % 100 ? 2 : 0);
                  return l.join(u", "_s);
              }()},
             {u"backupCopyDir"_s, qs(s_->settings.backupCopyDir)},
             {u"askReasons"_s, s_->settings.askReasons},
             {u"stockCountShowsExpected"_s, s_->settings.stockCountShowsExpected},
             {u"targetFoodCostPercent"_s, s_->settings.targetFoodCostPercent},
             {u"labelPrinter"_s, qs(s_->settings.labelPrinter)},
             {u"wasteReasons"_s, [&] { QStringList l; for (const std::string &t : s_->settings.wasteReasons) l << qs(t); return l.join(u'\n'); }()},
             {u"expoSkips"_s, [&] { QStringList l; for (const std::string &t : s_->settings.expoSkips) l << qs(t); return l.join(u'\n'); }()},
             {u"voidReasons"_s, [&] { QStringList l; for (const std::string &t : s_->settings.voidReasons) l << qs(t); return l.join(u'\n'); }()},
             {u"compReasons"_s, [&] { QStringList l; for (const std::string &t : s_->settings.compReasons) l << qs(t); return l.join(u'\n'); }()},
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
             {u"hostsTakePayments"_s, s_->settings.hostsTakePayments},
             {u"autoLogoutMinutes"_s, s_->settings.autoLogoutMinutes},
             {u"phoneLockSeconds"_s, s_->settings.phoneLockSeconds},
             {u"screensUpdate"_s, s_->settings.screensUpdate},
             {u"style"_s, qs(s_->settings.style)},
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
             {u"sendToTables"_s, s_->settings.sendToTables},
             {u"deliveryMinutes"_s, s_->settings.deliveryMinutes},
             {u"minutesPerOrderWaiting"_s, s_->settings.minutesPerOrderWaiting},
             {u"deliveryFee"_s, double(s_->settings.deliveryFee.cents()) / 100.0},
             {u"deliveryFeeToDriver"_s, s_->settings.deliveryFeeToDriver},
             {u"stripeSecretKey"_s, QString()}, {u"cardCurrency"_s, qs(s_->settings.cardCurrency)},
             {u"offlineCards"_s, s_->settings.offlineCards},
             {u"offlineCardMax"_s, double(s_->settings.offlineCardMax) / 100.0},
             {u"tabHold"_s, double(s_->settings.tabHold) / 100.0},
             {u"offlineCardTotal"_s, double(s_->settings.offlineCardTotal) / 100.0},
             {u"surchargePercent"_s, double(s_->settings.surchargeBp) / 100.0},
             {u"cashDiscountPercent"_s, double(s_->settings.cashDiscountBp) / 100.0},
             {u"cardTipOn"_s, qs(s_->settings.cardTipOn)},
             {u"tipPercents"_s, [&] { QStringList l; for (int p : s_->settings.tipPercents) l << QString::number(p); return l.join(u", "_s); }()},
             {u"tableReadyText"_s, qs(s_->settings.tableReadyText)}, {u"textWebhook"_s, qs(s_->settings.textWebhook)},
             {u"paidBreaks"_s, s_->settings.paidBreaks}, {u"overtimeDailyHours"_s, s_->settings.overtimeDailyHours},
             {u"laborState"_s, qs(s_->settings.laborState)}, {u"tipAllocation"_s, double(s_->settings.tipAllocationBp) / 100}, {u"doubleTimeDailyHours"_s, s_->settings.doubleTimeDailyHours},
             {u"seventhDayRule"_s, s_->settings.seventhDayRule}, {u"mealBreakAfterHours"_s, s_->settings.mealBreakAfterHours}, {u"restBreakEveryHours"_s, s_->settings.restBreakEveryHours}, {u"scheduleNoticeDays"_s, s_->settings.scheduleNoticeDays}, {u"restBetweenShiftsHours"_s, s_->settings.restBetweenShiftsHours},
             {u"giftCardCashBelow"_s, double(s_->settings.giftCardCashBelowCents) / 100.0},
             {u"checksPerServerHour"_s, s_->settings.checksPerServerHour},
             {u"overtimeWeeklyHours"_s, s_->settings.overtimeWeeklyHours},
             {u"weekStartsOn"_s, QString::number(s_->settings.weekStartsOn)}},
            tr("Store"), QString());
        // The page each job starts on.
        QVariantMap store = out.last().toMap();
        for (const char *job : {"server", "bartender", "cashier", "host", "manager"})
            store.insert(u"startPage."_s + QLatin1StringView(job),
                         s_->settings.startPages.contains(job) ? qs(s_->settings.startPages.at(job)) : QString());
        store.insert(aboutValues());
        out.last() = store;
    } else if (panel == u"promotions") {
        for (const PosSettings::Promotion &p : s_->settings.promotions)
            add(promotionRecord(p), qs(p.name),
                (p.code.empty() ? QString() : tr("code %1").arg(qs(p.code)) + u" · "_s)
                    + (p.buy > 0 ? tr("buy %1 get %2 at %3% off").arg(p.buy).arg(p.get).arg(double(p.percentBp) / 100.0)
                       : p.amount.cents() > 0 ? tr("%1 off").arg(format(p.amount))
                                              : tr("%1% off").arg(double(p.percentBp) / 100.0))
                    + (p.maxUses > 0 ? u" · "_s + tr("used %1 of %2").arg(p.uses).arg(p.maxUses) : QString())
                    + (p.startMinute != p.endMinute ? u" · "_s + clockText12(p.startMinute) + u"-"_s + clockText12(p.endMinute) : QString())
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
            add([&] { QVariantMap r = toJson(g).toVariantMap(); r.remove(u"sinceCount"_s); return r; }(), qs(g.name),
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
            add({{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"start"_s, clockText12(m.start)}}, qs(m.name),
                tr("from %1").arg(clockText12(m.start)));
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
                 {u"afterPaying"_s, qs(t.afterPaying)}, {u"startCategory"_s, qs(t.startCategory)},
                 {u"startPage"_s, qs(t.startPage)},
                 {u"labelPrinter"_s, qs(t.labelPrinter)}, {u"textSize"_s, t.textSize ? QString::number(t.textSize) : QString()},
                 {u"colorSafe"_s, t.colorSafe}},
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
                {u"active"_s, true}, {u"training"_s, false}, {u"cashMode"_s, QString()}, {u"requireName"_s, QString()}, {u"checkout"_s, QString()}, {u"blindCount"_s, QString()}, {u"reasons"_s, QString()}, {u"look"_s, QString()}, {u"tipPool"_s, QString()},
                {u"payRate"_s, 0.0}, {u"otherJobs"_s, QString()}, {u"language"_s, QString()},
                {u"textSize"_s, u"100"_s}, {u"leftHanded"_s, false}, {u"colorSafe"_s, false}, {u"startPage"_s, QString()}, {u"buzz"_s, true},
                {u"perm:order"_s, QString()}, {u"perm:check.settle"_s, QString()}, {u"perm:check.others"_s, QString()}, {u"perm:check.discount"_s, QString()},
                {u"perm:order.void"_s, QString()}, {u"perm:manager"_s, QString()}, {u"perm:layout.edit"_s, QString()}};
    if (panel == u"tenders")
        return {{u"id"_s, QString()}, {u"name"_s, QString()}, {u"kind"_s, u"card"_s}, {u"percent"_s, 0.0},
                {u"staffMeal"_s, false}};
    if (panel == u"terminals")
        return {{u"name"_s, terminal_}, {u"receiptPrinter"_s, QString()}, {u"drawer"_s, QString()},
                {u"screen"_s, QString()}, {u"look"_s, QString()}, {u"keyboard"_s, QString()}, {u"requireName"_s, QString()},
                {u"cardReader"_s, QString()}, {u"afterPaying"_s, QString()}, {u"startCategory"_s, QString()}, {u"startPage"_s, QString()},
                {u"labelPrinter"_s, QString()}, {u"textSize"_s, QString()}, {u"colorSafe"_s, false}};
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
                {u"labelWidthMm"_s, 51}, {u"labelHeightMm"_s, 25},
                {u"cutter"_s, true}, {u"drawerKick"_s, false}, {u"receipts"_s, true}, {u"charset"_s, QString()}, {u"watch"_s, true}};
    return {};
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
    r.insert(u"eventAt"_s, m.eventAt > 0 ? dateTimeField(m.eventAt)
                                         : QString());
    for (const char16_t *k : {u"family", u"printer"}) {
        if (!r.contains(QString::fromUtf16(k)))
            r.insert(QString::fromUtf16(k), QString());
    }
    r.insert(u"modifier"_s, m.isModifier);
    r.insert(u"available"_s, m.available);
    return r;
}



QVariantMap PosService::promotionRecord(const PosSettings::Promotion &p) const
{
    QStringList families, items;
    for (const std::string &f : p.families) families << qs(f);
    for (const std::string &i : p.items) items << qs(i);
    return {{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)}, {u"active"_s, p.active}, {u"families"_s, families.join(u", "_s)},
            {u"items"_s, items.join(u", "_s)}, {u"percent"_s, double(p.percentBp) / 100.0}, {u"buy"_s, p.buy},
            {u"get"_s, p.get}, {u"start"_s, clockText12(p.startMinute)}, {u"end"_s, clockText12(p.endMinute)},
            {u"days"_s, daysText(p.days)}, {u"code"_s, qs(p.code)}, {u"amount"_s, double(p.amount.cents()) / 100.0},
            {u"minimum"_s, double(p.minimum.cents()) / 100.0}, {u"fromDay"_s, qs(p.fromDay)}, {u"untilDay"_s, qs(p.untilDay)},
            {u"maxUses"_s, p.maxUses}, {u"uses"_s, p.uses}};
}


QStringList PosService::groupIds() const
{
    QStringList out;
    for (const ModifierGroup &g : s_->settings.modifierGroups)
        out << qs(g.id);
    return out;
}

QVariantMap PosService::aboutValues() const
{
    const QVariantMap n = s_->network ? s_->network() : QVariantMap();
    const auto when = [](qint64 ms) {
        return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms), u"ddd MMM d, "_s)
             + i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
    };
    const QString unknown = tr("(not known here)");
    const QString version = n.value(u"build"_s).toString();
    const QString database = n.value(u"database"_s).toString();
    const qint64 bytes = n.value(u"databaseBytes"_s).toLongLong();
    const qint64 lastBackup = n.value(u"lastBackup"_s).toLongLong();
    const QString system = QStringList{n.value(u"platform"_s).toString(),
                                       n.value(u"qt"_s).toString().isEmpty() ? QString() : u"Qt "_s + n.value(u"qt"_s).toString()}
                               .filter(QRegularExpression(u"."_s)).join(u", "_s);
    QString machine = n.value(u"machine"_s).toString();
    if (!machine.isEmpty())
        machine += n.value(u"role"_s).toString() == u"main" ? tr(" (the store's server: other screens connect to it)")
                                                  : tr(" (this computer runs the store on its own)");
    return {
        {u"about.version"_s, version.isEmpty() ? unknown : version},
        {u"about.machine"_s, machine.isEmpty() ? unknown : machine},
        {u"about.started"_s, n.value(u"startedAt"_s).toLongLong() > 0 ? when(n.value(u"startedAt"_s).toLongLong()) : unknown},
        {u"about.database"_s, database.isEmpty() ? unknown
                                                 : tr("%1 (%2)").arg(database, i18n::locale().formattedDataSize(bytes))},
        {u"about.backup"_s, n.value(u"backups"_s).toString().isEmpty() ? unknown
                            : lastBackup > 0 ? tr("%1, in %2").arg(when(lastBackup), n.value(u"backups"_s).toString())
                                             : tr("None yet (they go in %1)").arg(n.value(u"backups"_s).toString())},
        {u"about.system"_s, system.isEmpty() ? unknown : system},
        // The builds this store has run, newest first: when each arrived.
        {u"about.updates"_s, [&] {
             QStringList l;
             const auto &runs = s_->settings.builds;
             for (auto it = runs.rbegin(); it != runs.rend() && l.size() < 10; ++it)
                 l << tr("%1 since %2").arg(qs(it->version), when(it->at));
             return l.isEmpty() ? tr("None recorded yet") : l.join(u'\n');
         }()},
        // What the recent builds changed (built into the program), newest first.
        {u"about.history"_s, [&] {
             QStringList l;
             for (const QVariant &v : n.value(u"history"_s).toList()) {
                 const QVariantMap b = v.toMap();
                 QString change = b.value(u"change"_s).toString();
                 change.remove(QRegularExpression(u"^[a-z]+(\\([^)]*\\))?:\\s*"_s));   // "feat(modern): "
                 l << u"%1 · %2 · %3"_s.arg(b.value(u"version"_s).toString(),
                                           i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(b.value(u"at"_s).toLongLong()).date(),
                                                                   u"MMM d"_s),
                                           change);
                 if (l.size() == 15)
                     break;
             }
             return l.isEmpty() ? unknown : l.join(u'\n');
         }()},
        {u"about.license"_s, tr("GNU General Public License, version 3: free to use, share and change; it comes with no warranty.")},
        {u"about.source"_s, u"https://github.com/ViewTouch/viewtouch"_s},
        {u"about.credits"_s, tr("ViewTouch was created by Gene Mosher (viewtouch.com). This is its modern edition.")},
    };
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
    add(u"refunds"_s, tr("Refunds"));
    add(u"paidIn"_s, tr("Paid in"));
    add(u"paidOut"_s, tr("Paid out"));
    for (const std::string &c : s_->settings.expenseCategories)
        add(u"paidout:"_s + qs(c).toLower(), tr("Paid out: %1").arg(qs(c)));
    add(u"overShort"_s, tr("Drawers over or short"));
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


} // namespace vt::app
