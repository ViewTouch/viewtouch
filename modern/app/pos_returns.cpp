// PosService: returns and exchanges (a shop's, mostly). Items come back
// from a sale (its receipt scanned, or found with Find a Check) or without
// a receipt (a manager's): each a line below zero on the check being rung,
// at what was paid for it (its share of the sale's discounts off), its tax
// back too. New items on the same check make it an exchange. What's owed
// back goes as cash, back to the card the sale was paid with, or as store
// credit (a gift card). Closed, the items are back in stock (unless damaged)
// and the sale says how many came back.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QPointer>

#include <algorithm>
#include <cmath>

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

} // namespace

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

bool PosService::mayTakeReturn(const Check *sale)
{
    // A cashier, with the receipt and in time (Store Settings); else a manager.
    const bool inTime = sale && (s_->settings.returnDays == 0 || saleDaysAgo(*sale) <= s_->settings.returnDays);
    if (sale && inTime && s_->settings.cashierReturns)
        return require(perm::Settle, tr("Returns"));
    return require(perm::Manager, sale && !inTime ? tr("A return after %1 days").arg(s_->settings.returnDays)
                                                  : sale ? tr("Returns") : tr("A return without a receipt"));
}

bool PosService::startReturn(qint64 checkId)
{
    if (!user())
        return fail(tr("Log in first."));
    if (!require(perm::Settle, tr("Returns")))
        return false;
    const auto use = [this](const Check &c) {
        if (c.status != CheckStatus::Closed)
            return fail(tr("%1 isn't paid and closed: items come back from a sale.").arg(qs(c.label)));
        returnSale_ = c;
        returnWanted_ = 0;
        emit sessionChanged();
        return true;
    };
    for (const Check &c : s_->closedToday)
        if (c.id == checkId)
            return use(c);
    for (const Check &c : searchHits_)
        if (c.id == checkId)
            return use(c);
    if (s_->open.contains(checkId))
        return fail(tr("%1 isn't paid and closed: items come back from a sale.").arg(qs(s_->open.at(checkId).label)));
    // An earlier day's: looked for, then shown when found (returnArrived).
    returnSale_.reset();
    returnWanted_ = checkId;
    emit sessionChanged();
    return searchChecks(u"#%1"_s.arg(checkId), 0, 0);
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
    const Check *sale = returnSaleCheck();
    if (!sale)
        return fail(tr("Scan the receipt, or find the sale, first."));
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
    for (OrderLine &r : back) {
        r.id = c->nextLineId++;
        c->lines.push_back(r);
        selectedLine_ = r.id;
    }
    noteEvent(*c, tr("Return from %1 #%2: %3").arg(qs(sale->label)).arg(sale->id).arg(reason.trimmed()), "return",
              -c->totals(s_->settings.tax).items, ss(reason.trimmed()));
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
    if (!m || m->isModifier)
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    if (!mayTakeReturn(nullptr))
        return false;
    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check *c = current();
    OrderLine &l = c->addItem(*m);
    l.unitPrice = cents > 0 ? Money::fromCents(cents) : m->priceDuring(currentMealPeriod());
    l.quantity = -1;
    l.kitchenHide = true;
    l.noDiscount = l.noStaffDiscount = true;
    l.returnReason = ss(reason.trimmed());
    l.restock = restock;
    selectedLine_ = l.id;
    noteEvent(*c, tr("Return without a receipt: %1 (%2)").arg(qs(m->name), reason.trimmed()), "return",
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

const Payment *PosService::returnCard(const Check &c, const Check **sale) const
{
    // The card the sale was paid with (the first return line's sale).
    for (const OrderLine &l : c.lines) {
        if (!l.isReturn() || l.voided || !l.returnOf)
            continue;
        const Check *found = nullptr;
        for (const Check &x : s_->closedToday)
            if (x.id == l.returnOf)
                found = &x;
        for (const Check &x : searchHits_)
            if (!found && x.id == l.returnOf)
                found = &x;
        if (returnSale_ && !found && returnSale_->id == l.returnOf)
            found = &*returnSale_;
        if (!found)
            continue;
        for (const Payment &p : found->payments)
            if (p.kind == TenderKind::Card && p.amount.cents() > 0) {
                if (sale)
                    *sale = found;
                return &p;
            }
    }
    return nullptr;
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

    if (how == u"credit") {
        // Store credit: a new gift card with what's owed back on it.
        const std::int64_t id = c->id;
        if (!sellGiftCard(QString(), due.cents()))
            return false;
        Check *now = current();
        if (!now || now->id != id)
            return false;
        noteEvent(*now, tr("Store credit: %1").arg(format(due)), "return", due);
        emit notice(tr("Store credit of %1 on a new gift card").arg(format(due)));
        return closeCheck();
    }

    if (how == u"cash") {
        const Tender &cash = tenderOfKind(TenderKind::Cash, "cash", tr("Cash"));
        if (!serverBank() && !myDrawer())
            return fail(noDrawerMessage());
        Payment &p = c->addPayment(cash, -due);
        p.reference = "refund";
        noteEvent(*c, tr("Refunded %1 in cash").arg(format(due)), "return", due);
        changed(*c);
        return closeCheck();
    }

    if (how != u"card")
        return fail(tr("Give it back as cash, to the card or as store credit."));
    const Check *sale = nullptr;
    const Payment *card = returnCard(*c, &sale);
    if (!card || !sale)
        return fail(tr("The sale wasn't paid by card: give it back as cash or store credit."));
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
        const QString key = u"return/%1"_s.arg(c->id);
        if (s_->refundsUnderway.contains(key))
            return fail(tr("That refund is on its way."));
        s_->refundsUnderway.insert(key);
        emit notice(tr("Refunding %1 through Stripe…").arg(format(due)));
        QPointer<PosService> self(this);
        PosShared *shared = s_;
        const std::int64_t checkId = c->id;
        const Payment paid = *card;
        s_->stripeRefund(qs(card->reference), due.cents(), [shared, self, key, checkId, t, due, paid](const QString &refundId, const QString &error) {
            shared->refundsUnderway.remove(key);
            if (!self)
                return;
            if (!error.isEmpty()) {
                self->fail(tr("The refund didn't go through: %1").arg(error));
                emit self->sessionChanged();
                return;
            }
            const auto it = shared->open.find(checkId);
            if (it == shared->open.end())
                return;
            Payment &p = it->second.addPayment(t, -due);
            p.processor = "stripe";
            p.reference = ss(refundId);
            p.cardBrand = paid.cardBrand;
            p.last4 = paid.last4;
            self->noteEvent(it->second, tr("Refunded %1 to %2").arg(self->format(due), qs(paid.tenderName)), "return", due);
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
    noteEvent(*c, tr("Refunded %1 to %2").arg(format(due), qs(card->tenderName)), "return", due);
    changed(*c);
    if (!c->training)
        emit notice(tr("Recorded. Refund %1 on the card machine it was taken on.").arg(format(due)));
    return closeCheck();
}

void PosService::markReturned(const Check &c)
{
    // Closed: the sales say how many came back (today's here, earlier days' saved).
    std::map<std::int64_t, std::map<std::int64_t, int>> bySale;
    for (const OrderLine &l : c.lines)
        if (l.isReturn() && !l.voided && l.returnOf)
            bySale[l.returnOf][l.returnLine] -= l.quantity;
    for (const auto &[saleId, lines] : bySale) {
        Check *sale = nullptr;
        for (Check &x : s_->closedToday)
            if (x.id == saleId)
                sale = &x;
        for (Check &x : searchHits_)
            if (!sale && x.id == saleId)
                sale = &x;
        std::optional<Check> copy;
        if (!sale && returnSale_ && returnSale_->id == saleId)
            sale = &copy.emplace(*returnSale_);
        if (!sale)
            continue;
        for (const auto &[lineId, n] : lines)
            if (OrderLine *l = sale->line(lineId))
                l->returned = std::min(l->quantity, l->returned + n);
        sale->note(now(), user() ? user()->name : std::string(), ss(tr("Items returned on #%1").arg(c.id)), "return", {});
        if (s_->sink)
            s_->sink->saveCheck(*sale);
        // The same sale, kept elsewhere here: the same count.
        for (Check &x : searchHits_)
            if (x.id == saleId && &x != sale)
                x = *sale;
    }
}

} // namespace vt::app
