// PosService: refunds on closed checks, today's or from any day before
// (found with Find a Check). A manager's. A card from a Stripe reader goes
// back through Stripe; cash comes out of the drawer (or the person's bank)
// as a refund pay-out; anything else is recorded (refund it where it was
// taken). Each refund goes on the check (its history) and on today's
// reports, the day the money went back.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QPointer>

#include <set>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

Check *PosService::closedCheckFor(qint64 checkId)
{
    for (Check &c : s_->closedToday)
        if (c.id == checkId)
            return &c;
    for (Check &c : searchHits_)
        if (c.id == checkId)
            return &c;
    return nullptr;
}

Money PosShared::refundedSoFar(const Check &check, std::int64_t paymentId) const
{
    Money sum;
    std::set<std::int64_t> seen;
    for (const Refund &r : check.refunds)
        if (r.paymentId == paymentId && seen.insert(r.id).second)
            sum += r.amount;
    for (const Refund &r : refundsToday)   // made today, maybe on another terminal
        if (r.checkId == check.id && r.paymentId == paymentId && seen.insert(r.id).second)
            sum += r.amount;
    return sum;
}

void PosShared::recordRefund(Refund r, const Check &found, const QString &what)
{
    r.id = ++lastRefundId;
    Check *today = nullptr;
    for (Check &c : closedToday)
        if (c.id == r.checkId)
            today = &c;
    Check updated = today ? *today : found;
    updated.refunds.push_back(r);
    updated.note(r.at, r.by, ss(what), "refund", r.amount);
    if (today)
        *today = updated;
    refundsToday.push_back(r);
    if (sink) {
        sink->saveCheck(updated);
        sink->saveRefund(r);
    }
    emit refundRecorded(r, what);
    emit dayChanged();
}

bool PosService::refundPayment(qint64 checkId, qint64 paymentId, qint64 cents, const QString &reason)
{
    // The manager who approved it (or the one logged in) is who refunded it.
    const std::string approver = approved_ ? approved_->by : (user() ? user()->name : std::string());
    if (!require(perm::Manager, tr("Refunds")))
        return false;
    Check *c = closedCheckFor(checkId);
    if (!c)
        return fail(tr("Find the check first (Find a Check)."));
    if (c->status != CheckStatus::Closed)
        return fail(tr("Refunds are for closed checks; on an open one, use Undo Payment."));
    const auto p = std::ranges::find(c->payments, paymentId, &Payment::id);
    if (p == c->payments.end())
        return fail(tr("That payment isn't on the check."));
    if (p->kind == TenderKind::Discount || p->kind == TenderKind::GiftCard || p->kind == TenderKind::HouseAccount)
        return fail(tr("%1 can't be refunded here.").arg(qs(p->tenderName)));
    if (p->offline == "waiting")
        return fail(tr("That card was taken offline and hasn't reached Stripe yet: refund it once the internet is back."));
    if (p->offline == "declined" || p->offline == "seen")
        return fail(tr("That card was declined: there's nothing to refund."));
    // The bill it paid (tips aren't refunded), less what went back already
    // (today's from any terminal too).
    const Money left = c->refundable(*p, s_->settings.tax) - s_->refundedSoFar(*c, paymentId);
    const Money amount = cents > 0 ? Money::fromCents(cents) : left;
    if (left.cents() <= 0)
        return fail(tr("That payment was already refunded in full."));
    if (amount.cents() <= 0 || amount > left)
        return fail(tr("Refund up to %1.").arg(format(left)));
    const QString why = reason.trimmed();
    if (why.isEmpty())
        return fail(tr("Say why (the reason goes in the check's history)."));
    const QString key = u"%1/%2"_s.arg(checkId).arg(paymentId);
    if (s_->refundsUnderway.contains(key))
        return fail(tr("That refund is on its way."));

    const Payment paid = *p;
    Refund r;
    r.at = now();
    r.paymentId = paid.id;
    r.amount = amount;
    r.reason = ss(why);
    r.by = approver;
    r.tenderName = paid.tenderName;
    r.method = paid.processor == "stripe" && !paid.reference.empty() ? "stripe"
             : paid.kind == TenderKind::Cash                         ? "cash"
                                                                     : "recorded";
    r.day = s_->day.id;
    r.checkId = checkId;
    r.checkLabel = c->label;
    const Check found = *c;   // as it was read: the record is kept from this, whoever is still here
    PosShared *shared = s_;
    const auto what = [this, r](const QString &reference) {
        return tr("Refunded %1 (%2): %3").arg(format(r.amount), qs(r.tenderName), qs(r.reason))
               + (reference.isEmpty() ? QString() : u" · "_s + reference);
    };

    if (r.method == "stripe") {
        if (!s_->stripeRefund || s_->settings.stripeSecretKey.empty())
            return fail(tr("Refunding a card needs the store's Stripe key (Store Settings), or refund it in the Stripe Dashboard."));
        s_->refundsUnderway.insert(key);
        emit notice(tr("Refunding %1 through Stripe…").arg(format(amount)));
        const QString done = what(u"%1"_s);   // the reference goes in when it's known
        QPointer<PosService> self(this);
        s_->stripeRefund(qs(paid.reference), amount.cents(),
                         [shared, self, key, r, found, done](const QString &refundId, const QString &error) mutable {
            shared->refundsUnderway.remove(key);
            if (!error.isEmpty()) {
                if (self) {
                    self->fail(tr("The refund didn't go through: %1").arg(error));
                    emit self->sessionChanged();
                }
                return;
            }
            r.reference = ss(refundId);
            shared->recordRefund(r, found, QString(done).replace(u"%1"_s, refundId));
        });
        emit sessionChanged();
        return true;
    }
    if (r.method == "cash") {
        // Out of this person's bank or this terminal's drawer, as a refund.
        DrawerSession *d = serverBank() ? ensureMyBank() : myDrawer();
        if (!d)
            return fail(noDrawerMessage());
        CashMovement m;
        m.id = d->nextMovementId++;
        m.kind = CashMovement::Kind::Payout;
        m.amount = amount;
        m.reason = ss(tr("Refund, %1 #%2: %3").arg(qs(c->label)).arg(checkId).arg(why));
        m.by = approver;
        m.at = now();
        m.category = ss(tr("Refunds"));
        d->movements.push_back(m);
        if (s_->sink)
            s_->sink->saveDrawer(*d);
        if (s_->printer && !serverBank() && terminalHasDrawer())
            s_->printer->openDrawer(s_->settings, receiptPrinter());
        emit s_->drawerChanged();
    }
    s_->recordRefund(r, found, what({}));
    if (r.method == "recorded")
        emit notice(tr("Recorded. Refund %1 on the card machine it was taken on.").arg(format(amount)));
    return true;
}

} // namespace vt::app
