// PosService: a bar tab's card, held. When the tab opens the guest's card is
// read and the bank sets an amount aside on it (a Stripe payment made with
// manual capture); nothing is charged. When the tab closes the held card is
// charged for the tab and the tip: up to what's held, or more once the bank
// agrees to raise the hold. Paid another way, the hold is let go.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QString form(std::initializer_list<std::pair<QString, QString>> fields)
{
    QStringList out;
    for (const auto &[k, v] : fields)
        out << QString::fromLatin1(QUrl::toPercentEncoding(k, "[]")) + u'=' + QString::fromLatin1(QUrl::toPercentEncoding(v));
    return out.join(u'&');
}

QString timeOfDay(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

QString cardOf(const CardHold &h)
{
    QString brand = qs(h.cardBrand);
    if (!brand.isEmpty())
        brand[0] = brand[0].toUpper();
    if (h.last4.empty())
        return brand;
    return (brand.isEmpty() ? QString() : brand + u' ') + u"•••• "_s + qs(h.last4);
}

} // namespace

QVariantMap PosService::holdInfo(const Check &c) const
{
    if (c.hold.empty())
        return {};
    return {{u"card"_s, cardOf(c.hold)}, {u"amount"_s, format(c.hold.amount)},
            {u"amountCents"_s, qint64(c.hold.amount.cents())}, {u"since"_s, timeOfDay(c.hold.at)}};
}

QVariantMap PosService::holdCharge()
{
    const auto no = [&](const QString &why) {
        emit notice(why);
        return QVariantMap{{u"ok"_s, false}, {u"error"_s, why}};
    };
    if (!can(QString::fromLatin1(perm::Settle)))
        return no(tr("Holding a card needs permission to take payments."));
    Check *c = current();
    if (!c)
        return no(tr("No check is open."));
    if (!c->hold.empty())
        return no(tr("A card is already held on this check."));
    if (s_->settings.tabHold <= 0)
        return no(tr("Holding cards is off (Manager -> Store Settings)."));
    const Tender *card = nullptr;
    for (const Tender &t : s_->settings.tenders)
        if (t.kind == TenderKind::Card && !card)
            card = &t;
    if (!card)
        return no(tr("Set up a card payment type first (Manager -> Payment Types)."));
    const Money amount = Money::fromCents(s_->settings.tabHold);
    return {{u"ok"_s, true}, {u"hold"_s, true}, {u"tenderId"_s, qs(card->id)}, {u"checkId"_s, qint64(c->id)},
            {u"amountCents"_s, qint64(amount.cents())}, {u"tipCents"_s, 0},
            {u"currency"_s, qs(s_->settings.cardCurrency)},
            {u"description"_s, tr("Tab hold: %1 #%2").arg(qs(c->label)).arg(c->id)},
            {u"amount"_s, tr("Hold %1").arg(format(amount))},
            // A hold needs the bank's answer now: never taken offline.
            {u"offlineMax"_s, 0}, {u"offlineTotal"_s, 0}};
}

bool PosService::recordCardHold(const QVariantMap &r)
{
    if (!require(perm::Settle, tr("Holding a card")))
        return false;
    return addCardHold(r);
}

bool PosService::addCardHold(const QVariantMap &r)
{
    const QString reference = r.value(u"reference"_s).toString();
    if (r.value(u"offline"_s).toBool() || reference.isEmpty())
        return fail(tr("Holding a card needs the internet: try again, or take another payment when the tab closes."));
    const auto it = s_->open.find(r.value(u"checkId"_s).toLongLong());
    const bool onStripe = r.value(u"processor"_s).toString() == u"stripe";
    // Nowhere to keep it (the tab closed meanwhile, or it holds another
    // card): let it go, so nothing stays set aside on the guest's card.
    const auto letGo = [&](const QString &why) {
        if (onStripe)
            stripe(u"POST"_s, u"/v1/payment_intents/%1/cancel"_s.arg(reference), {}, [](const QJsonObject &, const QString &) {});
        return fail(why);
    };
    if (it == s_->open.end())
        return letGo(tr("The tab closed before the card was held: it was let go."));
    Check &c = it->second;
    if (qs(c.hold.reference) == reference)
        return true;   // the same answer again
    if (!c.hold.empty())
        return letGo(tr("%1 already holds a card: this one was let go.").arg(qs(c.label)));
    c.hold.reference = ss(reference);
    c.hold.processor = ss(r.value(u"processor"_s).toString());
    c.hold.cardBrand = ss(r.value(u"brand"_s).toString().toLower());
    c.hold.last4 = ss(r.value(u"last4"_s).toString().right(4));
    c.hold.amount = Money::fromCents(r.value(u"amountCents"_s).toLongLong());
    c.hold.at = now();
    const QString card = cardOf(c.hold);
    noteEvent(c, tr("Card held: %1 for %2").arg(card, format(c.hold.amount)), "hold");
    emit notice(tr("Card held: %1 for %2 (not charged)").arg(card, format(c.hold.amount)));
    changed(c);
    return true;
}

bool PosService::startCounterHold()
{
    const QString reader = counterReaderId();
    if (reader.isEmpty())
        return fail(tr("This screen has no card reader beside it (Manager -> Terminals)."));
    if (s_->settings.stripeSecretKey.empty())
        return fail(tr("The store's Stripe key isn't set (Manager -> Store Settings)."));
    if (!counter_.isEmpty())
        return fail(tr("The card reader is already taking a card."));
    const QVariantMap charge = holdCharge();
    if (!charge.value(u"ok"_s).toBool())
        return false;
    return startCounter(charge);
}

bool PosService::chargeHold(std::int64_t percentBp, std::int64_t tipCents)
{
    if (!require(perm::Settle, tr("Charging a held card")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (c->hold.empty())
        return fail(tr("No card is held on this check."));
    if (holdCharging_.contains(c->id))
        return fail(tr("The held card is being charged…"));
    const Totals t = c->totals(s_->settings.tax);
    if (t.balance.cents() <= 0)
        return fail(tr("Nothing is owed on this check."));
    const Money tip = percentBp > 0 ? (t.netSales() + t.tax).percent(percentBp) : Money::fromCents(std::max<std::int64_t>(0, tipCents));
    const Money balance = t.balance;
    const Money want = balance + tip;
    const CardHold hold = c->hold;
    const qint64 checkId = c->id;
    // What was charged pays the check first, then the tip.
    const auto charged = [this, checkId, hold, balance](Money captured) {
        holdCharging_.remove(checkId);
        const auto it = s_->open.find(checkId);
        if (it == s_->open.end()) {
            emit notice(tr("The held card was charged %1 (%2) but the tab is closed: refund it in Stripe.")
                            .arg(format(captured), qs(hold.reference)));
            return;
        }
        Check &tab = it->second;
        const Money paid = std::min(captured, balance);
        QVariantMap r{{u"reference"_s, qs(hold.reference)}, {u"processor"_s, qs(hold.processor)},
                      {u"brand"_s, qs(hold.cardBrand)}, {u"last4"_s, qs(hold.last4)},
                      {u"amountCents"_s, qint64(captured.cents())}, {u"tipCents"_s, qint64((captured - paid).cents())},
                      {u"checkId"_s, checkId}};
        tab.hold = {};
        noteEvent(tab, tr("Held card charged: %1").arg(format(captured)), "hold");
        if (!addCardPayment(r))
            emit notice(tr("The held card was charged %1 (%2) but isn't on the check: add it by hand or refund it in Stripe.")
                            .arg(format(captured), qs(hold.reference)));
    };
    if (hold.processor != "stripe") {   // the simulated reader: the bank always agrees
        charged(want);
        return true;
    }
    holdCharging_.insert(checkId);
    emit notice(tr("Charging %1 to the held card…").arg(format(want)));
    const auto capture = [this, hold, checkId, charged](Money amount) {
        stripe(u"POST"_s, u"/v1/payment_intents/%1/capture"_s.arg(qs(hold.reference)),
               form({{u"amount_to_capture"_s, QString::number(amount.cents())}}),
               [this, checkId, amount, charged](const QJsonObject &, const QString &error) {
            if (!error.isEmpty()) {
                holdCharging_.remove(checkId);
                emit notice(tr("The held card couldn't be charged: %1. Take another payment.").arg(error));
                return;
            }
            charged(amount);
        });
    };
    if (want <= hold.amount) {
        capture(want);
        return true;
    }
    // More than is held: the bank is asked to raise the hold first; if it
    // won't, what's held is charged and the rest is paid another way.
    stripe(u"POST"_s, u"/v1/payment_intents/%1/increment_authorization"_s.arg(qs(hold.reference)),
           form({{u"amount"_s, QString::number(want.cents())}}),
           [this, hold, want, capture](const QJsonObject &, const QString &error) {
        if (error.isEmpty())
            return capture(want);
        emit notice(tr("The bank wouldn't hold %1 on the card (%2): charging the %3 held; take the rest another way.")
                        .arg(format(want), error, format(hold.amount)));
        capture(hold.amount);
    });
    return true;
}

bool PosService::releaseHold()
{
    if (!require(perm::Settle, tr("Letting a held card go")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (c->hold.empty())
        return fail(tr("No card is held on this check."));
    if (holdCharging_.contains(c->id))
        return fail(tr("The held card is being charged…"));
    letHoldGo(*c, tr("Held card let go"));
    emit notice(tr("The card is let go: nothing was charged."));
    changed(*c);
    return true;
}

void PosService::letHoldGo(Check &c, const QString &why)
{
    if (c.hold.empty())
        return;
    if (c.hold.processor == "stripe") {
        const QString label = qs(c.label);
        stripe(u"POST"_s, u"/v1/payment_intents/%1/cancel"_s.arg(qs(c.hold.reference)), {},
               [this, label](const QJsonObject &, const QString &error) {
            if (!error.isEmpty())
                emit notice(tr("The card held for %1 couldn't be let go (%2): the bank lets it go in a few days.").arg(label, error));
        });
    }
    noteEvent(c, tr("%1: %2").arg(why, cardOf(c.hold)), "hold");
    c.hold = {};
}

} // namespace vt::app
