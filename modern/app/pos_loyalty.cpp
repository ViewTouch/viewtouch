// PosService: loyalty (points for what customers spend, rewards that take
// money off) and promotions (happy hour, buy one get one...) applied by
// themselves while they run. Both show on the check as discounts.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// Within its first and last days (a coupon's dates).
bool inDates(const PosSettings::Promotion &p, std::int64_t ms)
{
    const std::string today = QDateTime::fromMSecsSinceEpoch(ms).date().toString(Qt::ISODate).toStdString();
    return (p.fromDay.empty() || today >= p.fromDay) && (p.untilDay.empty() || today <= p.untilDay);
}

bool runningAt(const PosSettings::Promotion &p, std::int64_t ms)
{
    if (!p.active || !inDates(p, ms))
        return false;
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms);
    if (!(p.days & (1 << (t.date().dayOfWeek() % 7))))
        return false;
    if (p.startMinute == p.endMinute)
        return true;
    const int m = t.time().hour() * 60 + t.time().minute();
    return p.startMinute < p.endMinute ? m >= p.startMinute && m < p.endMinute
                                       : m >= p.startMinute || m < p.endMinute;   // past midnight
}

bool usedUp(const PosSettings::Promotion &p)
{
    return p.maxUses > 0 && p.uses >= p.maxUses;
}

std::string normalCode(const QString &code)
{
    return code.trimmed().toUpper().toStdString();
}

// "2026-11-30" -> "Nov 30, 2026" in the screen's language.
QString dayText(const std::string &iso)
{
    return i18n::locale().toString(QDate::fromString(QString::fromStdString(iso), Qt::ISODate), QLocale::ShortFormat);
}

} // namespace

// --- promotions --------------------------------------------------------------------------

Money PosService::promotionAmount(const PosSettings::Promotion &p, const Check &c) const
{
    // What the matching lines are worth, one unit at a time.
    std::vector<Money> units;
    for (const OrderLine &l : c.lines) {
        if (l.voided || l.isComment() || l.isGiftCard())
            continue;
        const MenuItem *m = findItem(qs(l.itemId));
        const bool everything = p.families.empty() && p.items.empty();
        const bool family = m && std::ranges::find(p.families, m->family) != p.families.end();
        const bool item = std::ranges::find(p.items, l.itemId) != p.items.end();
        if (!everything && !family && !item)
            continue;
        const int qty = std::max(1, l.quantity);
        for (int i = 0; i < qty; ++i)
            units.push_back(Money::fromCents(l.total().cents() / qty));
    }
    Money off;
    if (p.minimum.cents() > 0 && c.totals(s_->settings.tax).items < p.minimum)
        return off;
    if (p.amount.cents() > 0 && p.buy <= 0) {   // a fixed amount, up to what it covers
        Money worth;
        for (Money u : units)
            worth += u;
        return std::min(p.amount, worth);
    }
    if (p.buy <= 0) {
        for (Money u : units)
            off += u.percent(p.percentBp);
        return off;
    }
    // Buy N get M: in every N + M, the cheapest M at percent off.
    std::ranges::sort(units, std::greater<>());
    const int group = p.buy + std::max(1, p.get);
    for (int start = 0; start + group <= int(units.size()); start += group) {
        for (int k = start + p.buy; k < start + group; ++k)
            off += units[k].percent(p.percentBp);
    }
    return off;
}

void PosService::applyPromotions(Check &c)
{
    if (c.status != CheckStatus::Open)
        return;
    std::vector<std::pair<const PosSettings::Promotion *, Money>> wanted;
    for (const PosSettings::Promotion &p : s_->settings.promotions) {
        if (!runningAt(p, now()))
            continue;
        // A coupon: only where it was entered (and kept once it was, even if
        // it's used up meanwhile on another check).
        if (!p.code.empty() && std::ranges::find(c.coupons, p.id) == c.coupons.end())
            continue;
        const Money off = promotionAmount(p, c);
        if (off.cents() > 0)
            wanted.emplace_back(&p, off);
    }
    // Already as wanted: leave the payments (and their ids) alone.
    std::vector<std::pair<std::string, Money>> have, want;
    for (const Payment &pay : c.payments)
        if (pay.reference.starts_with("promo:"))
            have.emplace_back(pay.reference, pay.amount);
    for (const auto &[p, off] : wanted)
        want.emplace_back("promo:" + p->id, off);
    if (have == want)
        return;
    std::erase_if(c.payments, [](const Payment &pay) { return pay.reference.starts_with("promo:"); });
    for (const auto &[p, off] : wanted) {
        Payment pay;
        pay.id = c.nextPaymentId++;
        pay.tenderId = "promo";
        pay.tenderName = p->name;
        pay.kind = TenderKind::Discount;
        pay.amount = off;
        pay.reference = "promo:" + p->id;
        c.payments.push_back(pay);
    }
}

QVariantList PosService::promotionsNow() const
{
    QVariantList out;
    for (const PosSettings::Promotion &p : s_->settings.promotions)
        if (p.code.empty() && runningAt(p, now()))
            out.append(qs(p.name));
    return out;
}

// --- coupons -----------------------------------------------------------------------------

bool PosService::applyCoupon(const QString &code)
{
    if (!require(perm::Settle, tr("Coupons")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const std::string wanted = normalCode(code);
    if (wanted.empty())
        return fail(tr("Type or scan the coupon's code."));
    const auto it = std::ranges::find_if(s_->settings.promotions, [&](const PosSettings::Promotion &p) {
        return !p.code.empty() && normalCode(qs(p.code)) == wanted;
    });
    if (it == s_->settings.promotions.end())
        return fail(tr("There is no coupon %1.").arg(qs(wanted)));
    const PosSettings::Promotion &p = *it;
    if (std::ranges::find(c->coupons, p.id) != c->coupons.end())
        return fail(tr("%1 is already on this check.").arg(qs(p.name)));
    const std::string today = QDateTime::fromMSecsSinceEpoch(now()).date().toString(Qt::ISODate).toStdString();
    if (!p.active)
        return fail(tr("%1 is turned off.").arg(qs(p.name)));
    if (!p.untilDay.empty() && today > p.untilDay)
        return fail(tr("%1 ended on %2.").arg(qs(p.name), dayText(p.untilDay)));
    if (!p.fromDay.empty() && today < p.fromDay)
        return fail(tr("%1 starts on %2.").arg(qs(p.name), dayText(p.fromDay)));
    if (!runningAt(p, now()))
        return fail(tr("%1 isn't good at this hour or on this day.").arg(qs(p.name)));
    if (usedUp(p))
        return fail(tr("%1 has been used as many times as it may be (%2).").arg(qs(p.name)).arg(p.maxUses));
    if (p.minimum.cents() > 0 && c->totals(s_->settings.tax).items < p.minimum)
        return fail(tr("%1 is for orders of %2 or more.").arg(qs(p.name), format(p.minimum)));
    if (promotionAmount(p, *c).cents() <= 0)
        return fail(tr("Nothing on this check is covered by %1.").arg(qs(p.name)));
    c->coupons.push_back(p.id);
    const Money before = c->totals(s_->settings.tax).discounts;
    applyPromotions(*c);
    const Money off = c->totals(s_->settings.tax).discounts - before;
    noteEvent(*c, tr("Coupon: %1").arg(qs(p.name)), "discount", off);
    emit notice(tr("%1: %2 off").arg(qs(p.name), format(off)));
    changed(*c);
    return true;
}

bool PosService::removeCoupon(const QString &id)
{
    if (!require(perm::Settle, tr("Coupons")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (std::erase(c->coupons, ss(id)) == 0)
        return fail(tr("That coupon isn't on this check."));
    noteEvent(*c, tr("Coupon taken off"), "undiscount");
    changed(*c);
    return true;
}

QVariantList PosService::checkCoupons() const
{
    QVariantList out;
    const Check *c = currentCheck();
    if (!c)
        return out;
    for (const std::string &id : c->coupons) {
        const auto it = std::ranges::find_if(s_->settings.promotions, [&](const PosSettings::Promotion &p) { return p.id == id; });
        const bool known = it != s_->settings.promotions.end();
        Money off;
        for (const Payment &pay : c->payments)
            if (pay.reference == "promo:" + id)
                off += pay.amount;
        out.append(QVariantMap{{u"id"_s, qs(id)}, {u"name"_s, known ? qs(it->name) : qs(id)},
                               {u"code"_s, known ? qs(it->code) : QString()}, {u"off"_s, format(off)},
                               {u"working"_s, off.cents() > 0}});
    }
    return out;
}

QString PosService::scanCode(const QString &code)
{
    const QString c = code.trimmed();
    if (c.isEmpty())
        return {};
    const std::string wanted = normalCode(c);
    const bool coupon = std::ranges::any_of(s_->settings.promotions, [&](const PosSettings::Promotion &p) {
        return !p.code.empty() && normalCode(qs(p.code)) == wanted;
    });
    if (coupon)
        return applyCoupon(c) ? u"coupon"_s : QString();
    if (s_->giftCard(ss(c)))
        return lookupGiftCard(c) ? u"giftCard"_s : QString();
    fail(tr("Nothing has the code %1.").arg(c));
    return {};
}

void PosService::countCouponUses(const Check &c, int by)
{
    if (c.training)
        return;
    bool any = false;
    for (const std::string &id : c.coupons) {
        // Only those that took something off.
        if (!std::ranges::any_of(c.payments, [&](const Payment &p) { return p.reference == "promo:" + id; }))
            continue;
        for (PosSettings::Promotion &p : s_->settings.promotions) {
            if (p.id == id) {
                p.uses = std::max(0, p.uses + by);
                any = true;
            }
        }
    }
    if (any)
        settingsChanged();
}

// --- loyalty -----------------------------------------------------------------------------

int PosService::pointsFor(const Check &c) const
{
    if (!s_->settings.loyaltyEnabled)
        return 0;
    // On what they paid for, after discounts, not gift cards.
    Money giftCards;
    for (const OrderLine &l : c.lines)
        if (l.isGiftCard() && !l.voided)
            giftCards += l.total();
    const Money spent = c.totals(s_->settings.tax).subtotal - giftCards;
    return spent.cents() > 0 ? int(spent.cents() / 100 * s_->settings.pointsPerDollar) : 0;
}

void PosService::earnPoints(Check &c)
{
    CustomerRecord *r = s_->customer(c.customerId);
    const int points = r ? pointsFor(c) : 0;
    c.pointsEarned = points;
    if (points > 0) {
        r->points += points;
        r->lifetimePoints += points;
        saveCustomerRecord(*r);
    }
}

void PosService::takeBackPoints(Check &c)
{
    if (CustomerRecord *r = s_->customer(c.customerId); r && c.pointsEarned > 0) {
        r->points = std::max(0, r->points - c.pointsEarned);
        r->lifetimePoints = std::max(0, r->lifetimePoints - c.pointsEarned);
        saveCustomerRecord(*r);
    }
    c.pointsEarned = 0;
}

bool PosService::redeemReward(int index)
{
    if (!require(perm::Settle, tr("Rewards")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (!s_->settings.loyaltyEnabled || index < 0 || index >= int(s_->settings.rewards.size()))
        return fail(tr("There is no such reward."));
    if (c->training)
        return fail(tr("Practice checks can't spend real points."));
    CustomerRecord *r = s_->customer(c->customerId);
    if (!r)
        return fail(tr("Put the customer on the check first."));
    const PosSettings::Reward &reward = s_->settings.rewards[index];
    if (r->points < reward.points)
        return fail(tr("%1 has %2 points; this takes %3.").arg(qs(r->name)).arg(r->points).arg(reward.points));
    const Totals t = c->totals(s_->settings.tax);
    const Money off = std::min(reward.value, t.subtotal);
    if (off.cents() <= 0)
        return fail(tr("Nothing to take it off."));
    Payment pay;
    pay.id = c->nextPaymentId++;
    pay.tenderId = "reward";
    pay.tenderName = ss(tr("Reward: %1 off").arg(format(reward.value)));
    pay.kind = TenderKind::Discount;
    pay.amount = off;
    pay.reference = "reward:" + std::to_string(reward.points);
    c->payments.push_back(pay);
    r->points -= reward.points;
    saveCustomerRecord(*r);
    noteEvent(*c, tr("Reward %1 (%2 points)").arg(format(off)).arg(reward.points), "discount", off);
    emit notice(tr("%1 off for %2 points; %3 left").arg(format(off)).arg(reward.points).arg(r->points));
    changed(*c);
    return true;
}

bool PosService::customerJoin(const QString &phone)
{
    if (!require(perm::Order, tr("Rewards sign-up")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const std::string digits = CustomerRecord::digits(ss(phone));
    if (digits.size() < 7)
        return fail(tr("Enter a phone number."));
    const auto it = std::ranges::find_if(s_->customers, [&](const CustomerRecord &r) {
        return CustomerRecord::digits(r.phone) == digits;
    });
    if (it != s_->customers.end())
        return useCustomer(qs(it->id));
    return saveCustomer({{u"phone"_s, phone.trimmed()}, {u"name"_s, QString()}});
}

bool PosService::sendReceipt(const QString &how, const QString &to)
{
    const Check *last = nullptr;
    for (const Check &c : s_->closedToday)
        if (c.id == lastClosedId_)
            last = &c;
    if (!last)
        return fail(tr("No check to send."));
    if (how == u"print")
        return printReceipt();
    if (how != u"text")
        return true;   // no receipt
    if (CustomerRecord::digits(ss(to)).size() < 7)
        return fail(tr("Enter a phone number."));
    if (!s_->sendText)
        return fail(tr("Texting isn't set up (Store Settings)."));
    const Totals t = last->totals(s_->settings.tax);
    QStringList items;
    for (const OrderLine &l : last->lines)
        if (!l.voided && !l.isComment())
            items << (l.quantity > 1 ? u"%1 x %2"_s.arg(l.quantity).arg(qs(l.name)) : qs(l.name));
    const QString text = tr("%1 - check #%2\n%3\nTotal %4%5\nThank you!")
                             .arg(qs(s_->settings.storeName)).arg(last->id).arg(items.join(u", "_s), format(t.total),
                                  t.tips.cents() ? tr(" + tip %1").arg(format(t.tips)) : QString());
    s_->sendText(to.trimmed(), text);
    emit notice(tr("Receipt texted"));
    return true;
}

} // namespace vt::app
