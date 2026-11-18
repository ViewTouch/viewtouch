// PosService: returns and exchanges (a shop's, mostly). Items come back
// from a sale (its receipt scanned, or found with Find a Check) or without
// a receipt (a manager's): each a line below zero on the check being rung,
// at what was paid for it (its share of the sale's discounts off), its tax
// back too. New items on the same check make it an exchange. What's owed
// back goes as cash, back to the card the sale was paid with, or as store
// credit (a gift card). Closed, the items are back in stock (unless damaged)
// and the sale says how many came back and what went back on each payment.
//
// Money safety: the sale is always read afresh (today's, or the database's),
// never a screen's own copy; what can come back is checked again when the
// return closes; one refund at a time per check; a sale refunded by other
// means, a typed sale number and a refund in cash for a gift-card sale are a
// manager's; no-receipt prices are today's unless a manager types one.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// How many of a sale's line are already coming back on checks still open.
int returningNow(const PosShared &s, std::int64_t checkId, std::int64_t lineId)
{
    int n = 0;
    for (const auto &[id, c] : s.open)
        for (const OrderLine &l : c.lines)
            if (l.isReturn() && !l.voided && l.returnOf == checkId && l.returnLine == lineId)
                n -= l.quantity;
    return n;
}

// A sale given money back outside returns (Find a Check -> Refund...).
bool refundedOutside(const PosShared &s, const Check &sale)
{
    if (!sale.refunds.empty())
        return true;
    return std::ranges::any_of(s.refundsToday, [&](const Refund &r) { return r.checkId == sale.id; });
}

QString underwayKey(std::int64_t checkId)
{
    return u"return/%1"_s.arg(checkId);
}

} // namespace

std::optional<Check> PosShared::saleById(std::int64_t id) const
{
    // Today's, else the database's (any day), as it is now.
    for (const Check &c : closedToday)
        if (c.id == id)
            return c;
    if (findChecks) {
        CheckFind f;
        f.words << u"\"id\":%1,"_s.arg(id);
        f.limit = 5;
        for (Check &c : findChecks(f))
            if (c.id == id)
                return std::move(c);
    } else if (history) {
        for (Check &c : history(0, now() + 86'400'000))
            if (c.id == id)
                return std::move(c);
    }
    return std::nullopt;
}

bool PosService::refundUnderway(const Check &c) const
{
    return s_->refundsUnderway.contains(underwayKey(c.id));
}

const Check *PosService::returnSaleCheck() const
{
    return returnSale_ ? &*returnSale_ : nullptr;
}

int PosService::returnableLeft(const Check &sale, const OrderLine &l) const
{
    if (l.voided || l.isComment() || l.isReturn() || l.isGiftCard() || l.isFee() || l.share > 1)
        return 0;
    return std::max(0, l.quantity - l.returned - returningNow(*s_, sale.id, l.id));
}

int PosService::saleDaysAgo(const Check &sale) const
{
    const std::int64_t at = sale.closedAt ? sale.closedAt : sale.openedAt;
    return int(QDateTime::fromMSecsSinceEpoch(at).date().daysTo(QDateTime::fromMSecsSinceEpoch(now()).date()));
}

bool PosService::mayTakeReturn(const Check *sale, const QString &what)
{
    // A cashier: the receipt itself scanned, in time (Store Settings), a sale
    // not refunded another way. Anything else: a manager.
    const bool inTime = sale && (s_->settings.returnDays == 0 || saleDaysAgo(*sale) <= s_->settings.returnDays);
    const bool refunded = sale && refundedOutside(*s_, *sale);
    if (sale && inTime && !refunded && returnVerified_ && s_->settings.cashierReturns)
        return require(perm::Settle, tr("Returns"));
    return require(perm::Manager, !what.isEmpty()            ? what
                                  : refunded                 ? tr("A return from a sale refunded before")
                                  : sale && !inTime          ? tr("A return after %1 days").arg(s_->settings.returnDays)
                                  : sale && !returnVerified_ ? tr("A return without the receipt's barcode")
                                  : sale                     ? tr("Returns")
                                                             : tr("A return without a receipt"));
}

bool PosService::startReturn(qint64 checkId, const QString &code)
{
    if (!user())
        return fail(tr("Log in first."));
    if (!require(perm::Settle, tr("Returns")))
        return false;
    if (s_->open.contains(checkId))
        return fail(tr("%1 isn't paid and closed: items come back from a sale.").arg(qs(s_->open.at(checkId).label)));
    // As it is now (today's, or from the database): never a copy kept here.
    std::optional<Check> sale = s_->saleById(checkId);
    if (!sale)
        return fail(tr("Sale #%1 wasn't found.").arg(checkId));
    if (sale->status != CheckStatus::Closed)
        return fail(tr("%1 isn't paid and closed: items come back from a sale.").arg(qs(sale->label)));
    returnSale_ = std::move(sale);
    // The receipt's own barcode (its code matches): a cashier may take it back.
    const std::string expected = receiptCode(s_->settings.receiptKey, checkId);
    returnVerified_ = !code.isEmpty() && !expected.empty() && ss(code.toUpper()) == expected;
    returnWanted_ = 0;
    emit sessionChanged();
    return true;
}

void PosService::returnArrived()
{
    if (!returnWanted_)
        return;
    const std::int64_t id = returnWanted_;
    returnWanted_ = 0;
    for (const Check &c : searchHits_)
        if (c.id == id && c.status == CheckStatus::Closed) {
            returnSale_ = c;
            emit sessionChanged();
            return;
        }
    fail(tr("Sale #%1 wasn't found.").arg(id));
    emit sessionChanged();
}

void PosService::cancelReturn()
{
    returnSale_.reset();
    returnWanted_ = 0;
    emit sessionChanged();
}

QVariantMap PosService::returnSale() const
{
    if (returnWanted_)
        return {{u"loading"_s, true}, {u"id"_s, qint64(returnWanted_)}};
    const Check *sale = returnSaleCheck();
    if (!sale)
        return {};
    const Totals t = sale->totals(s_->settings.tax);
    QVariantList lines;
    for (const OrderLine &l : sale->lines) {
        if (l.voided || l.isComment() || l.isReturn() || l.isGiftCard() || l.isFee() || l.share > 1)
            continue;
        lines.append(QVariantMap{{u"id"_s, qint64(l.id)}, {u"name"_s, qs(l.displayName())},
                                 {u"sold"_s, l.quantity}, {u"left"_s, returnableLeft(*sale, l)},
                                 {u"price"_s, format(returnPrice(*sale, l))}});
    }
    const int days = saleDaysAgo(*sale);
    const std::int64_t at = sale->closedAt ? sale->closedAt : sale->openedAt;
    return {{u"id"_s, qint64(sale->id)}, {u"label"_s, qs(sale->label)},
            {u"when"_s, i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(at), QLocale::ShortFormat)},
            {u"days"_s, days}, {u"total"_s, format(t.total)},
            {u"tooOld"_s, s_->settings.returnDays > 0 && days > s_->settings.returnDays},
            {u"lines"_s, lines}};
}

Money PosService::returnPrice(const Check &sale, const OrderLine &l) const
{
    // What one was paid: its share of the line, less its share of the sale's
    // discounts (half off then: half back now).
    if (l.quantity <= 0)
        return {};
    const Totals t = sale.totals(s_->settings.tax);
    const Money goods = t.items - t.giftCardsSold;
    Money each = Money::fromCents(l.total().cents() / l.quantity);
    if (t.discounts.cents() > 0 && goods.cents() > 0 && !l.noDiscount)
        each = each.scaled((goods - t.discounts).cents(), goods.cents());
    return each;
}

OrderLine PosService::returnLineFrom(const Check &sale, const OrderLine &l, int qty, const QString &reason) const
{
    OrderLine r = l;
    r.id = 0;
    r.quantity = -qty;
    r.qualifier = Qualifier::None;
    const Money each = returnPrice(sale, l);
    // The price on the receipt (after discounts), its choices kept for stock.
    for (Modifier &m : r.modifiers)
        m.unitPrice = Money();
    r.unitPrice = l.weight > 0 ? Money::fromCents(std::llround(double(each.cents()) * 1000.0 / double(l.weight))) : each;
    r.sent = r.voided = r.made = r.served = false;
    r.sentAt = r.madeAt = r.servedAt = 0;
    r.queueAtSend = -1;
    r.kitchenHide = true;   // nothing for the kitchen
    r.noDiscount = r.noStaffDiscount = true;
    r.unshare();
    r.returnOf = sale.id;
    r.returnLine = l.id;
    r.returned = 0;
    r.restock = true;
    r.returnReason = ss(reason.trimmed());
    r.note.clear();
    return r;
}

bool PosService::returnLines(const QVariantList &picks, const QString &reason)
{
    if (!returnSaleCheck())
        return fail(tr("Scan the receipt, or find the sale, first."));
    // Read again: another screen may have taken some back since it was shown.
    if (std::optional<Check> fresh = s_->saleById(returnSale_->id))
        returnSale_ = std::move(fresh);
    const Check *sale = returnSaleCheck();
    if (reason.trimmed().isEmpty())
        return fail(tr("Say why it's coming back."));
    if (!mayTakeReturn(sale))
        return false;
    std::vector<OrderLine> back;
    for (const QVariant &v : picks) {
        const QVariantMap p = v.toMap();
        const int qty = p.value(u"qty"_s).toInt();
        if (qty <= 0)
            continue;
        const OrderLine *l = sale->line(p.value(u"line"_s).toLongLong());
        if (!l)
            return fail(tr("That item isn't on the sale."));
        if (qty > returnableLeft(*sale, *l))
            return fail(tr("Only %1 of %2 can come back.").arg(returnableLeft(*sale, *l)).arg(qs(l->displayName())));
        back.push_back(returnLineFrom(*sale, *l, qty, reason));
        back.back().restock = p.value(u"restock"_s, true).toBool();
    }
    if (back.empty())
        return fail(tr("Choose what's coming back."));
    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check *c = current();
    if (c->training != sale->training)
        return fail(tr("A practice sale's items come back on a practice check."));
    if (refundUnderway(*c))
        return fail(tr("A refund is on its way on this check."));
    Money value;
    for (OrderLine &r : back) {
        r.id = c->nextLineId++;
        c->lines.push_back(r);
        selectedLine_ = r.id;
        value -= r.total();
    }
    noteEvent(*c, tr("Return from %1 #%2: %3").arg(qs(sale->label)).arg(sale->id).arg(reason.trimmed()), "return",
              value, ss(reason.trimmed()));
    returnSale_.reset();
    emit notice(tr("Coming back: %n item(s)", nullptr, int(back.size())));
    changed(*c);
    return true;
}

bool PosService::returnItem(const QString &itemId, qint64 cents, const QString &reason, bool restock)
{
    // Without a receipt: the item at today's price (or the one typed), a manager's.
    if (reason.trimmed().isEmpty())
        return fail(tr("Say why it's coming back."));
    const MenuItem *m = findItem(itemId);
    if (!m || m->isModifier || m->byWeight)
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    // Today's price, unless a manager (logged in themselves) types one: at
    // most three times the price and never over $1,000.
    const Money today = m->priceDuring(currentMealPeriod());
    const bool manager = user() && permissionsOf(*user()).contains(perm::Manager);
    Money price = today;
    if (cents > 0 && manager) {
        const std::int64_t most = std::min<std::int64_t>(100'000, std::max<std::int64_t>(today.cents() * 3, 100));
        if (cents > most)
            return fail(tr("That's more than %1 for %2.").arg(format(Money::fromCents(most)), qs(m->name)));
        price = Money::fromCents(cents);
    }
    if (!mayTakeReturn(nullptr, tr("A return without a receipt: %1 %2").arg(qs(m->name), format(price))))
        return false;
    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check *c = current();
    if (refundUnderway(*c))
        return fail(tr("A refund is on its way on this check."));
    OrderLine &l = c->addItem(*m);
    l.unitPrice = price;
    l.modifiers.clear();
    l.quantity = -1;
    l.kitchenHide = true;
    l.noDiscount = l.noStaffDiscount = true;
    l.returnReason = ss(reason.trimmed());
    // An item with sizes and colors: which one isn't known, so not back on a shelf.
    l.restock = restock && m->variants.empty();
    selectedLine_ = l.id;
    noteEvent(*c, tr("Return without a receipt: %1 (%2)").arg(qs(m->name), reason.trimmed()), "returnNoReceipt",
              l.unitPrice, ss(reason.trimmed()));
    emit notice(tr("Coming back: %1").arg(qs(m->name)));
    changed(*c);
    return true;
}

bool PosService::setRestock(qint64 lineId, bool restock)
{
    Check *c = current();
    OrderLine *l = c ? c->line(lineId) : nullptr;
    if (!l || !l->isReturn())
        return fail(tr("Choose an item coming back."));
    if (l->sent)
        return fail(tr("It's already back in stock."));
    l->restock = restock;
    changed(*c);
    return true;
}

Money PosService::refundDueOn(const Check &c) const
{
    // Owed back on a return: more came back than was bought (and paid).
    if (!c.hasReturns())
        return {};
    const Totals t = c.totals(s_->settings.tax);
    return t.total.cents() < 0 && t.balance.cents() < 0 ? -t.balance : Money();
}

std::optional<Payment> PosService::returnCard(const Check &c, std::optional<Check> *sale) const
{
    // The card one sale was paid with: every item coming back from it (no
    // receipt, or several sales: cash or store credit).
    std::int64_t saleId = 0;
    for (const OrderLine &l : c.lines) {
        if (!l.isReturn() || l.voided)
            continue;
        if (!l.returnOf || (saleId && l.returnOf != saleId))
            return std::nullopt;
        saleId = l.returnOf;
    }
    if (!saleId)
        return std::nullopt;
    std::optional<Check> found = s_->saleById(saleId);
    if (!found)
        return std::nullopt;
    for (const Payment &p : found->payments)
        if (p.kind == TenderKind::Card && p.amount.cents() > 0) {
            const Payment card = p;
            if (sale)
                *sale = std::move(found);
            return card;
        }
    return std::nullopt;
}

bool PosService::paidOtherwise(const Check &c) const
{
    // Any item coming back from a sale paid with a gift card or on account.
    std::set<std::int64_t> sales;
    for (const OrderLine &l : c.lines)
        if (l.isReturn() && !l.voided && l.returnOf)
            sales.insert(l.returnOf);
    for (const std::int64_t id : sales)
        if (const std::optional<Check> sale = s_->saleById(id))
            if (std::ranges::any_of(sale->payments, [](const Payment &p) {
                    return p.kind == TenderKind::GiftCard || p.kind == TenderKind::HouseAccount;
                }))
                return true;
    return false;
}

QString PosService::returnProblem(const Check &c) const
{
    // Closing a return: what comes back still can (another screen may have
    // taken some back meanwhile), and the sales are still there.
    std::map<std::int64_t, std::map<std::int64_t, int>> bySale;
    for (const OrderLine &l : c.lines)
        if (l.isReturn() && !l.voided && l.returnOf)
            bySale[l.returnOf][l.returnLine] -= l.quantity;
    for (const auto &[saleId, lines] : bySale) {
        const std::optional<Check> sale = s_->saleById(saleId);
        if (!sale)
            return tr("Sale #%1 wasn't found: void the items from it.").arg(saleId);
        for (const auto &[lineId, n] : lines) {
            const OrderLine *l = sale->line(lineId);
            if (!l || l->returned + n > l->quantity)
                return tr("Some of %1 came back already: void it and choose again.").arg(l ? qs(l->displayName()) : qs(sale->label));
        }
    }
    return {};
}

bool PosService::refundReturn(const QString &how)
{
    if (!require(perm::Settle, tr("Refunds")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const Money due = refundDueOn(*c);
    if (due.cents() <= 0)
        return fail(tr("Nothing is owed back on this check."));
    if (refundUnderway(*c))
        return fail(tr("A refund is on its way on this check."));
    if (const QString problem = returnProblem(*c); !problem.isEmpty())
        return fail(problem);

    if (how == u"credit") {
        // Store credit: a new gift card with what's owed back on it.
        const std::int64_t id = c->id;
        if (!sellGiftCard(QString(), due.cents()))
            return false;
        Check *now = current();
        if (!now || now->id != id)
            return false;
        noteEvent(*now, tr("Store credit: %1").arg(format(due)), "returnRefund", due);
        emit notice(tr("Store credit of %1 on a new gift card").arg(format(due)));
        return closeCheck();
    }

    if (how == u"cash") {
        // Paid with a gift card or on account: cash for it is a manager's.
        if (paidOtherwise(*c) && !require(perm::Manager, tr("Cash back for a sale paid by gift card or account")))
            return false;
        const Tender &cash = tenderOfKind(TenderKind::Cash, "cash", tr("Cash"));
        if (!serverBank() && !myDrawer())
            return fail(noDrawerMessage());
        // Where pennies are gone: what's handed back is rounded too.
        Money back = due;
        if (const std::int64_t step = s_->settings.tax.cashRoundingCents; step > 1)
            back = Money::fromCents((due.cents() + step / 2) / step * step);
        Payment &p = c->addPayment(cash, -back);
        p.reference = "refund";
        noteEvent(*c, tr("Refunded %1 in cash").arg(format(back)), "returnRefund", back);
        changed(*c);
        return closeCheck();
    }

    if (how != u"card")
        return fail(tr("Give it back as cash, to the card or as store credit."));
    std::optional<Check> sale;
    const std::optional<Payment> card = returnCard(*c, &sale);
    if (!card || !sale)
        return fail(tr("The items aren't all from one sale paid by card: give it back as cash or store credit."));
    const Money cap = sale->refundable(*card, s_->settings.tax) - s_->refundedSoFar(*sale, card->id);
    if (due > cap)
        return fail(tr("Up to %1 can go back to that card: the rest as cash or store credit.").arg(format(cap)));
    Tender t;
    t.id = card->tenderId;
    t.name = card->tenderName;
    t.kind = TenderKind::Card;
    if (card->processor == "stripe" && !card->reference.empty() && !c->training) {
        if (!s_->stripeRefund || s_->settings.stripeSecretKey.empty())
            return fail(tr("Refunding a card needs the store's Stripe key (Store Settings): give it back as cash or store credit."));
        const QString key = underwayKey(c->id);
        s_->refundsUnderway.insert(key);
        emit notice(tr("Refunding %1 through Stripe…").arg(format(due)));
        QPointer<PosService> self(this);
        PosShared *shared = s_;
        const std::int64_t checkId = c->id;
        s_->stripeRefund(qs(card->reference), due.cents(), [shared, self, key, checkId, t, due, paid = *card](const QString &refundId, const QString &error) {
            shared->refundsUnderway.remove(key);
            if (!self)
                return;
            if (!error.isEmpty()) {
                self->fail(tr("The refund didn't go through: %1").arg(error));
                emit self->sessionChanged();
                return;
            }
            const auto it = shared->open.find(checkId);
            if (it == shared->open.end()) {
                // (Nothing closes a check while its refund is on its way: this is a bug if it happens.)
                qWarning("A return's card refund came back for check %lld, no longer open", static_cast<long long>(checkId));
                return;
            }
            Payment &p = it->second.addPayment(t, -due);
            p.processor = "stripe";
            p.reference = ss(refundId);
            p.cardBrand = paid.cardBrand;
            p.last4 = paid.last4;
            self->noteEvent(it->second, tr("Refunded %1 to %2").arg(self->format(due), qs(paid.tenderName)), "returnRefund", due);
            self->changed(it->second);
            const Check *now = self->currentCheck();
            if (now && now->id == checkId)
                self->closeCheck();
        });
        emit sessionChanged();
        return true;
    }
    // A card machine of its own (or a practice check): recorded here, given back there.
    Payment &p = c->addPayment(t, -due);
    p.reference = "refund";
    p.cardBrand = card->cardBrand;
    p.last4 = card->last4;
    noteEvent(*c, tr("Refunded %1 to %2").arg(format(due), qs(card->tenderName)), "returnRefund", due);
    changed(*c);
    if (!c->training)
        emit notice(tr("Recorded. Refund %1 on the card machine it was taken on.").arg(format(due)));
    return closeCheck();
}

void PosService::markReturned(const Check &c)
{
    // Closed: each sale (as it is now) says how many came back, and what went
    // back on its payments, so neither a return nor a refund gives it twice.
    std::map<std::int64_t, std::map<std::int64_t, int>> bySale;
    std::map<std::int64_t, Check> valueOf;   // each sale's lines coming back, for their value with tax
    for (const OrderLine &l : c.lines)
        if (l.isReturn() && !l.voided && l.returnOf) {
            bySale[l.returnOf][l.returnLine] -= l.quantity;
            Check &v = valueOf[l.returnOf];
            v.type = c.type;
            v.taxExempt = c.taxExempt;
            v.closedRates = c.closedRates;
            v.lines.push_back(l);
        }
    // Given back by card (to which sale's card), else cash or store credit.
    std::string cardRef;
    for (const Payment &p : c.payments)
        if (p.kind == TenderKind::Card && p.amount.cents() < 0)
            cardRef = p.last4 + "|" + p.tenderId;
    // What went back in all (an exchange: only what the new items didn't take).
    Money left = std::max(Money(), -c.totals(s_->settings.tax).total);
    for (const auto &[saleId, lines] : bySale) {
        std::optional<Check> sale = s_->saleById(saleId);
        if (!sale)
            continue;   // (returnProblem refused this before the check closed)
        for (const auto &[lineId, n] : lines)
            if (OrderLine *l = sale->line(lineId))
                l->returned = std::min(l->quantity, l->returned + n);
        // What went back for these items, on the sale's payments: the card it
        // went back to first, then the others, each up to what it took.
        Money back = std::min(left, std::max(Money(), -valueOf[saleId].totals(s_->settings.tax).total));
        left -= back;
        std::vector<Payment *> order;
        for (Payment &p : sale->payments)
            if (p.kind != TenderKind::Discount && p.amount.cents() > 0
                && p.kind == TenderKind::Card && (p.last4 + "|" + p.tenderId) == cardRef)
                order.push_back(&p);
        for (Payment &p : sale->payments)
            if (p.kind != TenderKind::Discount && p.amount.cents() > 0 && std::ranges::find(order, &p) == order.end())
                order.push_back(&p);
        for (Payment *p : order) {
            if (back.cents() <= 0)
                break;
            const Money room = sale->refundable(*p, s_->settings.tax) - s_->refundedSoFar(*sale, p->id);
            const Money take = std::min(back, std::max(Money(), room));
            p->returnedBack += take;
            back -= take;
        }
        sale->note(now(), user() ? user()->name : std::string(), ss(tr("Items returned on #%1").arg(c.id)), "return", {});
        for (Check &x : s_->closedToday)
            if (x.id == saleId)
                x = *sale;
        for (Check &x : searchHits_)
            if (x.id == saleId)
                x = *sale;
        if (s_->sink)
            s_->sink->saveCheck(*sale);
    }
}

} // namespace vt::app
