// PosService: saving and removing the manager screens' records, checked first.

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

bool PosService::adminSave(const QString &panel, int index, const QVariantMap &record)
{
    // A new one: added, by its name.
    if (index < 0) {
        if (!saveAdminRecord(panel, index, record))
            return false;
        QString name;
        for (const char *key : {"name", "label", "title", "id"})
            if (name.isEmpty())
                name = record.value(QString::fromLatin1(key)).toString().trimmed();
        if (!name.isEmpty())
            logChange(panelName(panel), name, QString(), QString(), tr("Added"));
        return true;
    }
    // (Listing the records first: for terminals, that adds this screen when
    // it's missing, so it's only done for a record already there.)
    const QVariantList before = adminRecords(panel);
    if (!saveAdminRecord(panel, index, record))
        return false;
    // A PIN or a secret typed: the records never carry it back.
    QVariantList was = before;
    if (index >= 0 && index < was.size()) {
        QVariantMap r = was[index].toMap();
        for (const char *secret : {"pin", "stripeSecretKey", "backupPassword"})
            if (!record.value(QString::fromLatin1(secret)).toString().isEmpty())
                r.insert(QString::fromLatin1(secret), u"-"_s);
        was[index] = r;
    }
    logRecordChanges(panel, index, was, record);
    return true;
}

bool PosService::adminDelete(const QString &panel, int index)
{
    const QVariantList before = adminRecords(panel);
    if (!deleteAdminRecord(panel, index))
        return false;
    if (index >= 0 && index < before.size())
        logChange(panelName(panel), before[index].toMap().value(u"_title"_s).toString(), QString(),
                  QString(), panel == u"employees" ? tr("Made inactive") : tr("Removed"));
    return true;
}

bool PosService::saveAdminRecord(const QString &panel, int index, const QVariantMap &record)
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
            // Since its last count: kept; a different amount on hand is a change by hand.
            const Ingredient &was = s_->ingredients[index];
            g.id = was.id;
            g.countedAt = was.countedAt;
            g.start = was.start;
            g.received = was.received;
            g.sold = was.sold;
            g.wasted = was.wasted;
            g.adjusted = was.adjusted + (g.onHand - was.onHand);
            s_->ingredients[index] = g;
        } else {
            g.countedAt = 0;
            g.start = g.onHand;
            g.received = g.sold = g.wasted = g.adjusted = 0;
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
        t.startPage = ss(record.value(u"startPage"_s).toString());
        t.labelPrinter = ss(record.value(u"labelPrinter"_s).toString());
        t.textSize = record.value(u"textSize"_s).toString().isEmpty() ? 0
                                                                    : std::clamp(record.value(u"textSize"_s).toInt(), 80, 160);
        if (index >= 0 && index < int(list.size()))
            list[index] = t;
        else
            list.push_back(t);
        settingsChanged();
        ok = true;
    } else if (panel == u"taxes") {
        // Where the store is: its state (and that state's labor rules), its
        // ZIP code and which rate in it.
        if (record.contains(u"state"_s) && !setStoreState(ss(record.value(u"state"_s).toString())))
            return false;
        bool zipChanged = false;
        if (record.contains(u"zip"_s)) {
            const QString zip = record.value(u"zip"_s).toString().trimmed();
            static const QRegularExpression zipCode(u"^\\d{5}(-\\d{4})?$"_s);
            if (!zip.isEmpty() && !zipCode.match(zip).hasMatch())
                return fail(tr("A ZIP code is 5 digits."));
            if (ss(zip.left(5)) != s_->settings.storeZip) {
                s_->settings.taxWhere.clear();   // another ZIP code: its own choices
                zipChanged = true;
            }
            s_->settings.storeZip = ss(zip.left(5));
        }
        if (record.contains(u"taxWhere"_s) && !zipChanged)
            s_->settings.taxWhere = ss(record.value(u"taxWhere"_s).toString());
        const double rates[] = {number(record, u"food"), number(record, u"alcohol"),
                                number(record, u"merchandise"), number(record, u"room")};
        for (double r : rates) {
            if (r < 0 || r > 100)
                return fail(tr("Tax rates must be between 0 and 100%."));
        }
        if (record.value(u"useSuggested"_s).toBool()) {
            // The rates where the store is (the manager asked).
            const TaxSuggestion here = taxHere();
            if (!here.found)
                return fail(here.problem);
            s_->settings.tax.foodPpm = ppmFromPercent(here.food);
            s_->settings.tax.alcoholPpm = ppmFromPercent(here.alcohol);
            s_->settings.tax.merchandisePpm = ppmFromPercent(here.sales);
            s_->settings.taxAreaAsOf = ss(here.asOf);
        } else {
            s_->settings.tax.foodPpm = ppmFromPercent(rates[0]);
            s_->settings.tax.alcoholPpm = ppmFromPercent(rates[1]);
            s_->settings.tax.merchandisePpm = ppmFromPercent(rates[2]);
        }
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
        if (record.contains(u"checksPerServerHour"_s))
            s_->settings.checksPerServerHour = std::clamp(record.value(u"checksPerServerHour"_s).toInt(), 1, 50);
        // A state just picked: its rules (the fields below can still say otherwise).
        bool stateNow = false;
        if (record.contains(u"laborState"_s) && !setStoreState(ss(record.value(u"laborState"_s).toString()), &stateNow))
            return false;
        if (!stateNow) {
            if (record.contains(u"overtimeDailyHours"_s))
                s_->settings.overtimeDailyHours = std::clamp(record.value(u"overtimeDailyHours"_s).toInt(), 0, 24);
            if (record.contains(u"doubleTimeDailyHours"_s))
                s_->settings.doubleTimeDailyHours = std::clamp(record.value(u"doubleTimeDailyHours"_s).toInt(), 0, 24);
            if (record.contains(u"seventhDayRule"_s))
                s_->settings.seventhDayRule = record.value(u"seventhDayRule"_s).toBool();
            if (record.contains(u"mealBreakAfterHours"_s))
                s_->settings.mealBreakAfterHours = std::clamp(record.value(u"mealBreakAfterHours"_s).toInt(), 0, 12);
            if (record.contains(u"giftCardCashBelow"_s))
                s_->settings.giftCardCashBelowCents =
                    std::clamp<std::int64_t>(std::llround(record.value(u"giftCardCashBelow"_s).toDouble() * 100), 0, 100000);
        }
        if (record.contains(u"overtimeWeeklyHours"_s))
            s_->settings.overtimeWeeklyHours = std::clamp(record.value(u"overtimeWeeklyHours"_s).toInt(), 0, 168);
        if (record.contains(u"weekStartsOn"_s))
            s_->settings.weekStartsOn = std::clamp(record.value(u"weekStartsOn"_s).toString().toInt(), 0, 6);
        if (record.contains(u"checkoutNeedsClosedChecks"_s))
            s_->settings.checkoutNeedsClosedChecks = record.value(u"checkoutNeedsClosedChecks"_s).toBool();
        if (record.contains(u"blindDrawerCount"_s))
            s_->settings.blindDrawerCount = record.value(u"blindDrawerCount"_s).toBool();
        if (record.contains(u"cashDenominations"_s)) {
            std::vector<std::int64_t> pieces;
            for (const QString &part : record.value(u"cashDenominations"_s).toString().split(u',', Qt::SkipEmptyParts)) {
                QString t = part.trimmed();
                t.remove(QRegularExpression(u"[^\\d.]"_s));
                const std::int64_t cents = std::llround(t.toDouble() * 100);
                if (cents > 0 && pieces.size() < 16 && std::ranges::find(pieces, cents) == pieces.end())
                    pieces.push_back(cents);
            }
            std::ranges::sort(pieces, std::greater<>());
            if (!pieces.empty())
                s_->settings.cashDenominations = pieces;
        }
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
        if (record.contains(u"phoneLockSeconds"_s))
            s_->settings.phoneLockSeconds = std::clamp(record.value(u"phoneLockSeconds"_s).toInt(), -1, 3600);
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
        if (record.contains(u"deliveryFeeToDriver"_s))
            s_->settings.deliveryFeeToDriver = record.value(u"deliveryFeeToDriver"_s).toBool();
        if (record.contains(u"deliveryFee"_s)) {
            const double fee = record.value(u"deliveryFee"_s).toDouble();
            if (fee < 0 || fee > 1000)
                return fail(tr("Write the delivery fee in dollars."));
            s_->settings.deliveryFee = Money::fromCents(std::llround(fee * 100));
        }
        if (record.contains(u"kitchenLateMinutes"_s))
            s_->settings.kitchenLateMinutes =
                std::clamp(record.value(u"kitchenLateMinutes"_s).toInt(), s_->settings.kitchenWarnMinutes, 240);
        if (record.contains(u"hostsTakePayments"_s))
            s_->settings.hostsTakePayments = record.value(u"hostsTakePayments"_s).toBool();
        if (record.contains(u"waitMinutesPerParty"_s))
            s_->settings.waitMinutesPerParty = std::clamp(record.value(u"waitMinutesPerParty"_s).toInt(), 1, 120);
        if (record.contains(u"tableReadyText"_s) && !record.value(u"tableReadyText"_s).toString().trimmed().isEmpty())
            s_->settings.tableReadyText = ss(record.value(u"tableReadyText"_s).toString().trimmed());
        if (record.contains(u"textWebhook"_s)) {
            // Guests' phone numbers go there: only over an encrypted connection.
            const QString url = record.value(u"textWebhook"_s).toString().trimmed();
            if (!url.isEmpty() && !url.startsWith(u"https://"_s, Qt::CaseInsensitive))
                return fail(tr("The texting service's address must start with https:// (guests' phone numbers go there)."));
            s_->settings.textWebhook = ss(url);
        }
        if (record.contains(u"backupCopyDir"_s))
            s_->settings.backupCopyDir = ss(record.value(u"backupCopyDir"_s).toString().trimmed());
        if (record.contains(u"askReasons"_s))
            s_->settings.askReasons = record.value(u"askReasons"_s).toBool();
        if (record.contains(u"labelPrinter"_s))
            s_->settings.labelPrinter = ss(record.value(u"labelPrinter"_s).toString());
        if (record.contains(u"targetFoodCostPercent"_s))
            s_->settings.targetFoodCostPercent = std::clamp(record.value(u"targetFoodCostPercent"_s).toInt(), 5, 90);
        if (record.contains(u"stockCountShowsExpected"_s))
            s_->settings.stockCountShowsExpected = record.value(u"stockCountShowsExpected"_s).toBool();
        for (const auto &[key, list] : {std::pair{u"openingChecklist"_s, &s_->settings.openingChecklist},
                                        std::pair{u"closingChecklist"_s, &s_->settings.closingChecklist},
                                        std::pair{u"voidReasons"_s, &s_->settings.voidReasons},
                                        std::pair{u"compReasons"_s, &s_->settings.compReasons},
                                        std::pair{u"wasteReasons"_s, &s_->settings.wasteReasons},
                                        std::pair{u"expoSkips"_s, &s_->settings.expoSkips}}) {
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
    // The event's date, typed: 2026-10-09 7:00 PM (or 19:00).
    if (const QString when = data.value(u"eventAt"_s).toString().trimmed(); when.isEmpty()) {
        data.insert(u"eventAt"_s, 0);
    } else {
        const std::int64_t at = dateTimeMs(when);
        if (at <= 0)
            return fail(tr("Write the event's date like 2026-10-09 7:00 PM."));
        data.insert(u"eventAt"_s, qint64(at));
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
    // Managers can do the most: a PIN that's harder to guess.
    if (!pin.isEmpty() && (role == u"manager" || role == u"admin") && pin.size() < 6)
        return fail(tr("Managers' PINs are 6 to 8 digits."));
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
    const QString blindCount = record.value(u"blindCount"_s).toString();
    if (!QStringList{QString(), u"blind"_s, u"sees"_s}.contains(blindCount))
        return fail(tr("Choose how this person counts a drawer."));
    const QString reasons = record.value(u"reasons"_s).toString();
    if (!QStringList{QString(), u"ask"_s, u"skip"_s}.contains(reasons))
        return fail(tr("Choose whether this person is asked why on voids and comps."));
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
    e.blindCount = ss(blindCount);
    e.reasons = ss(reasons);
    e.look = ss(record.value(u"look"_s, qs(e.look)).toString());
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
        return fail(tr("Write times like 3:00 PM."));
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
        return fail(tr("Type the start like 4:30 PM."));
    for (int i = 0; i < int(list.size()); ++i) {
        if (i != index && list[i].start == start)
            return fail(tr("%1 already starts at %2.").arg(qs(list[i].name), clockText12(start)));
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
    if (!QStringList{QString(), u"escpos"_s, u"text"_s, u"zpl"_s, u"tspl"_s}.contains(qs(p.format)))
        return fail(tr("Choose what the printer understands."));
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

bool PosService::deleteAdminRecord(const QString &panel, int index)
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

void PosShared::savePrepTimes()
{
    if (!sink || now() - prepSavedAt < 60'000)
        return;
    prepSavedAt = now();
    sink->saveSettings(settings);
}

void PosShared::saveSettings()
{
    if (sink)
        sink->saveSettings(settings);
    ++adminRevision;
    emit adminChanged();
}


} // namespace vt::app
