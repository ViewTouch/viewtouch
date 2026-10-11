// PosService: customers (looked up by phone or name, their visits), gift
// cards (sold on a check, live once it is paid, spent as a payment) and
// house accounts (charged to the store, paid later).

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QString whenText(std::int64_t ms)
{
    return ms ? i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms), u"MMM d, h:mm AP"_s) : QString();
}

// Card numbers: digits only (a swiped or typed "6012 3456-7890" is one card).
QString cleanNumber(const QString &n)
{
    QString out;
    for (QChar ch : n) {
        if (ch.isDigit())
            out += ch;
    }
    return out;
}

QString lastFour(const std::string &number)
{
    return u"••"_s + qs(number.size() > 4 ? number.substr(number.size() - 4) : number);
}

constexpr std::int64_t kMaxGiftCardCents = 200'000;   // $2,000 on one card

} // namespace

CustomerRecord *PosShared::customer(const std::string &id)
{
    auto it = std::ranges::find_if(customers, [&](const CustomerRecord &c) { return c.id == id; });
    return it == customers.end() ? nullptr : &*it;
}

GiftCard *PosShared::giftCard(const std::string &number)
{
    auto it = std::ranges::find_if(giftCards, [&](const GiftCard &g) { return g.number == number; });
    return it == giftCards.end() ? nullptr : &*it;
}

void PosService::saveCustomerRecord(const CustomerRecord &c)
{
    if (s_->sink)
        s_->sink->saveCustomer(c);
    emit s_->customersChanged();
}

void PosService::saveGiftCardRecord(const GiftCard &g)
{
    if (s_->sink)
        s_->sink->saveGiftCard(g);
    emit s_->customersChanged();
}

// --- customers ---------------------------------------------------------------------------

QVariantMap PosService::customerSummary(const CustomerRecord &c) const
{
    return {
        {u"id"_s, qs(c.id)}, {u"name"_s, qs(c.name)}, {u"phone"_s, qs(c.phone)}, {u"email"_s, qs(c.email)},
        {u"address"_s, qs(c.address)}, {u"note"_s, qs(c.note)}, {u"visits"_s, c.visits},
        {u"spent"_s, format(c.spent)}, {u"lastVisit"_s, whenText(c.lastVisit)},
        {u"houseAccount"_s, c.houseAccount}, {u"accountLimit"_s, format(c.accountLimit)},
        {u"accountLimitCents"_s, qint64(c.accountLimit.cents())},
        {u"balance"_s, format(c.accountBalance)}, {u"balanceCents"_s, qint64(c.accountBalance.cents())},
        {u"points"_s, c.points}, {u"lifetimePoints"_s, c.lifetimePoints},
    };
}

QVariantList PosService::customerResults() const
{
    if (!user())
        return {};
    const QString query = customerQuery_.trimmed();
    const std::string digits = CustomerRecord::digits(ss(query));
    std::vector<const CustomerRecord *> found;
    for (const CustomerRecord &c : s_->customers) {
        const bool match = query.isEmpty()
            || (digits.size() >= 3 && CustomerRecord::digits(c.phone).find(digits) != std::string::npos)
            || qs(c.name).contains(query, Qt::CaseInsensitive);
        if (match)
            found.push_back(&c);
    }
    // Regulars first: the most recent visit, then the newest.
    std::ranges::sort(found, [](const CustomerRecord *a, const CustomerRecord *b) {
        return std::max(a->lastVisit, a->createdAt) > std::max(b->lastVisit, b->createdAt);
    });
    QVariantList out;
    for (const CustomerRecord *c : found) {
        if (out.size() >= 40)
            break;
        out.append(customerSummary(*c));
    }
    return out;
}

QVariantMap PosService::customerInfo() const
{
    const CustomerRecord *c = user() ? s_->customer(selectedCustomer_) : nullptr;
    if (!c)
        return {};
    QVariantMap m = customerSummary(*c);
    QVariantList account;
    for (auto it = c->account.rbegin(); it != c->account.rend() && account.size() < 20; ++it)
        account.append(QVariantMap{{u"at"_s, whenText(it->at)}, {u"amount"_s, format(it->amount)},
                                   {u"what"_s, qs(it->what)}});
    m.insert(u"account"_s, account);
    const Check *open = currentCheck();
    m.insert(u"onCheck"_s, open && open->customerId == c->id);
    return m;
}

bool PosService::findCustomers(const QString &query)
{
    if (!require(perm::Order, tr("Looking up customers")))
        return false;
    customerQuery_ = query;
    emit checkChanged();
    return true;
}

bool PosService::selectCustomer(const QString &id)
{
    if (!require(perm::Order, tr("Looking up customers")))
        return false;
    if (!id.isEmpty() && !s_->customer(ss(id)))
        return fail(tr("That customer is not on file."));
    selectedCustomer_ = ss(id);
    emit checkChanged();
    return true;
}

bool PosService::useCustomer(const QString &id)
{
    if (!require(perm::Order, tr("Changing customer details")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("Open a check first."));
    const CustomerRecord *r = s_->customer(id.isEmpty() ? selectedCustomer_ : ss(id));
    if (!r)
        return fail(tr("Choose a customer first."));
    c->customerId = r->id;
    c->customer = {r->name, r->phone, r->address, r->note};
    selectedCustomer_ = r->id;
    emit notice(tr("%1 is on this check").arg(qs(r->name.empty() ? r->phone : r->name)));
    changed(*c);
    return true;
}

bool PosService::saveCustomer(const QVariantMap &record)
{
    if (!require(perm::Order, tr("Saving customers")))
        return false;
    const auto text = [&](const char16_t *key) { return ss(record.value(QString::fromUtf16(key)).toString().trimmed()); };
    const std::string name = text(u"name"), phone = text(u"phone");
    if (name.empty() && phone.empty())
        return fail(tr("A customer needs a name or a phone number."));

    // The same person: by id, else by phone number.
    CustomerRecord *c = s_->customer(text(u"id"));
    const std::string digits = CustomerRecord::digits(phone);
    if (!c && digits.size() >= 7) {
        auto it = std::ranges::find_if(s_->customers, [&](const CustomerRecord &r) {
            return CustomerRecord::digits(r.phone) == digits;
        });
        c = it == s_->customers.end() ? nullptr : &*it;
    }
    if (!c) {
        CustomerRecord fresh;
        fresh.id = "c" + std::to_string(now()) + "-" + std::to_string(QRandomGenerator::global()->bounded(1000, 9999));
        fresh.createdAt = now();
        s_->customers.push_back(fresh);
        c = &s_->customers.back();
    }
    c->name = name;
    c->phone = phone;
    c->email = text(u"email");
    c->address = text(u"address");
    c->note = text(u"note");
    // House accounts are a manager's decision.
    if (record.contains(u"houseAccount"_s) || record.contains(u"accountLimit"_s)) {
        const bool wants = record.value(u"houseAccount"_s, c->houseAccount).toBool();
        const Money limit = record.contains(u"accountLimit"_s)
            ? Money::fromCents(record.value(u"accountLimit"_s).toLongLong()) : c->accountLimit;
        if ((wants != c->houseAccount || limit != c->accountLimit)) {
            if (!require(perm::Manager, tr("House accounts")))
                return false;
            if (!wants && c->accountBalance.cents() != 0)
                return fail(tr("%1 still owes %2 on their account.").arg(qs(c->name), format(c->accountBalance)));
            c->houseAccount = wants;
            c->accountLimit = limit.cents() > 0 ? limit : Money();
        }
    }
    selectedCustomer_ = c->id;
    saveCustomerRecord(*c);
    // On the open check unless it is someone else's.
    if (Check *open = current(); open && (open->customerId.empty() || open->customerId == c->id)) {
        open->customerId = c->id;
        open->customer = {c->name, c->phone, c->address, c->note};
        changed(*open);
    }
    emit notice(tr("Saved %1").arg(qs(name.empty() ? phone : name)));
    emit checkChanged();
    return true;
}

// The customer typed on a takeout / delivery check goes on file too.
void PosService::rememberCustomer(Check &c)
{
    if (c.customer.empty() || (c.customer.name.empty() && c.customer.phone.empty()))
        return;
    if (!c.customerId.empty()) {
        if (CustomerRecord *r = s_->customer(c.customerId)) {
            r->name = c.customer.name;
            r->phone = c.customer.phone;
            r->address = c.customer.address;
            r->note = c.customer.note;
            saveCustomerRecord(*r);
            return;
        }
    }
    const QVariantMap record{{u"name"_s, qs(c.customer.name)}, {u"phone"_s, qs(c.customer.phone)},
                             {u"address"_s, qs(c.customer.address)}, {u"note"_s, qs(c.customer.note)}};
    saveCustomer(record);
}

// --- gift cards ---------------------------------------------------------------------------

QVariantMap PosService::giftCardInfo() const
{
    if (!user() || giftCardNumber_.isEmpty())
        return {};
    const GiftCard *g = s_->giftCard(ss(giftCardNumber_));
    QVariantMap m{{u"number"_s, giftCardNumber_}, {u"found"_s, g != nullptr},
                  {u"display"_s, lastFour(ss(giftCardNumber_))}};
    if (!g)
        return m;
    m.insert(u"balance"_s, format(g->balance));
    m.insert(u"balanceCents"_s, qint64(g->balance.cents()));
    m.insert(u"issued"_s, whenText(g->issuedAt));
    QVariantList history;
    for (auto it = g->history.rbegin(); it != g->history.rend() && history.size() < 12; ++it)
        history.append(QVariantMap{{u"at"_s, whenText(it->at)}, {u"amount"_s, format(it->amount)},
                                   {u"what"_s, qs(it->what)}});
    m.insert(u"history"_s, history);
    return m;
}

bool PosService::lookupGiftCard(const QString &number)
{
    if (!require(perm::Order, tr("Gift cards")))
        return false;
    const QString n = cleanNumber(number);
    if (n.size() < 4)
        return fail(tr("Enter or swipe the card number."));
    giftCardNumber_ = n;
    const GiftCard *g = s_->giftCard(ss(n));
    emit notice(g ? tr("Gift card %1: %2").arg(lastFour(ss(n)), format(g->balance))
                  : tr("Gift card %1 is new").arg(lastFour(ss(n))));
    emit checkChanged();
    return true;
}

bool PosService::sellGiftCard(const QString &number, qint64 amountCents)
{
    if (!require(perm::Order, tr("Selling gift cards")))
        return false;
    QString n = cleanNumber(number);
    if (n.isEmpty()) {   // no card in hand: make up a number (printed on the receipt)
        do {
            n = u"6"_s;
            for (int i = 0; i < 15; ++i)
                n += QChar(u'0' + QRandomGenerator::global()->bounded(10));
        } while (s_->giftCard(ss(n)));
    }
    if (n.size() < 4)
        return fail(tr("The card number is too short."));
    const Money amount = Money::fromCents(amountCents > 0 ? amountCents : entry_.toLongLong());
    if (amount.cents() <= 0)
        return fail(tr("Enter the amount to put on the card."));
    const GiftCard *existing = s_->giftCard(ss(n));
    if ((existing ? existing->balance : Money()) + amount > Money::fromCents(kMaxGiftCardCents))
        return fail(tr("A gift card can hold up to %1.").arg(format(Money::fromCents(kMaxGiftCardCents))));
    if (!current() && !startCheck(CheckType::Quick))
        return false;
    Check *c = current();
    for (const OrderLine &l : c->lines) {
        if (l.isGiftCard() && !l.voided && l.giftCardNumber() == ss(n))
            return fail(tr("Card %1 is already on this check.").arg(lastFour(ss(n))));
    }
    MenuItem card;
    card.id = "giftcard:" + ss(n);
    card.name = ss((existing ? tr("Gift Card reload %1") : tr("Gift Card %1")).arg(lastFour(ss(n))));
    card.price = amount;
    card.taxClass = TaxClass::None;
    card.family = "giftcards";
    OrderLine &l = c->addItem(card);
    selectedLine_ = l.id;
    giftCardNumber_ = n;
    entry_.clear();
    emit entryChanged();
    emit notice(tr("%1 on %2; it works once this check is paid").arg(format(amount), lastFour(ss(n))));
    changed(*c);
    return true;
}

const Tender &PosService::tenderOfKind(TenderKind kind, const char *id, const QString &name) const
{
    for (const Tender &t : s_->settings.tenders) {
        if (t.kind == kind)
            return t;
    }
    static std::map<TenderKind, Tender> fallback;
    Tender &t = fallback[kind];
    t = {id, ss(name), kind, 0};
    return t;
}

bool PosService::payWithGiftCard(const QString &number, qint64 amountCents)
{
    if (!require(perm::Settle, tr("Taking payments")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (c->training)
        return fail(tr("Practice checks can't use a real gift card."));
    const QString n = cleanNumber(number.isEmpty() ? giftCardNumber_ : number);
    GiftCard *g = s_->giftCard(ss(n));
    if (!g)
        return fail(tr("Gift card %1 isn't active. Cards work once the check that sold them is paid.")
                        .arg(lastFour(ss(n))));
    if (g->balance.cents() <= 0)
        return fail(tr("Gift card %1 is empty.").arg(lastFour(ss(n))));
    const Money due = c->totals(s_->settings.tax).balance;
    if (due.cents() <= 0)
        return fail(tr("Nothing is owed on this check."));
    Money amount = amountCents > 0 ? Money::fromCents(amountCents)
                 : !entry_.isEmpty() ? Money::fromCents(entry_.toLongLong()) : due;
    amount = std::min({amount, due, g->balance});
    if (amount.cents() <= 0)
        return fail(tr("Enter an amount."));
    Payment &p = c->addPayment(tenderOfKind(TenderKind::GiftCard, "gift", tr("Gift Card")), amount);
    p.reference = g->number;
    g->post(now(), -amount, ss(tr("Paid check #%1").arg(c->id)), c->id, "spend");
    saveGiftCardRecord(*g);
    giftCardNumber_ = n;
    entry_.clear();
    emit entryChanged();
    emit notice(tr("%1 from gift card %2; %3 left on it").arg(format(amount), lastFour(g->number), format(g->balance)));
    changed(*c);
    return true;
}

// --- house accounts -------------------------------------------------------------------------

bool PosService::chargeHouseAccount(Check &c, const Tender &t, Money amount)
{
    if (c.training)
        return fail(tr("Practice checks can't charge a real account."));
    CustomerRecord *r = s_->customer(c.customerId);
    if (!r)
        return fail(tr("Put the customer on the check first (Customers)."));
    if (!r->houseAccount)
        return fail(tr("%1 has no house account.").arg(qs(r->name)));
    if (amount > r->accountRoom())
        return fail(tr("That's over %1's limit: %2 left on the account.").arg(qs(r->name), format(r->accountRoom())));
    Payment &p = c.addPayment(t, amount);
    p.reference = r->id;
    r->post(now(), amount, ss(tr("Check #%1").arg(c.id)), c.id, "charge");
    saveCustomerRecord(*r);
    emit notice(tr("%1 charged to %2's account (owes %3)").arg(format(amount), qs(r->name), format(r->accountBalance)));
    changed(c);
    return true;
}

bool PosService::payOnAccount(const QString &method, qint64 amountCents)
{
    if (!require(perm::Settle, tr("Account payments")))
        return false;
    CustomerRecord *r = s_->customer(selectedCustomer_);
    if (!r || !r->houseAccount)
        return fail(tr("Choose a customer with a house account."));
    if (r->accountBalance.cents() <= 0)
        return fail(tr("%1 owes nothing.").arg(qs(r->name)));
    Money amount = amountCents > 0 ? Money::fromCents(amountCents)
                 : entry_.isEmpty() ? r->accountBalance : Money::fromCents(entry_.toLongLong());
    amount = std::min(amount, r->accountBalance);
    if (amount.cents() <= 0)
        return fail(tr("Enter an amount."));
    const bool cash = method == u"cash";
    if (cash) {   // the cash goes in the drawer (or the bank) like a paid in
        DrawerSession *d = serverBank() ? ensureMyBank() : myDrawer();
        if (!d)
            return fail(noDrawerMessage());
        CashMovement m;
        m.id = d->nextMovementId++;
        m.kind = CashMovement::Kind::PaidIn;
        m.amount = amount;
        m.reason = ss(tr("Account payment: %1").arg(qs(r->name)));
        m.by = user()->name;
        m.at = now();
        d->movements.push_back(m);
        if (s_->sink)
            s_->sink->saveDrawer(*d);
        if (s_->printer && !serverBank() && terminalHasDrawer())
            s_->printer->openDrawer(s_->settings, receiptPrinter());
        emit s_->drawerChanged();
    }
    r->post(now(), -amount, ss(cash ? tr("Payment (cash), %1").arg(qs(user()->name))
                                   : tr("Payment (card), %1").arg(qs(user()->name))), 0, "payment");
    saveCustomerRecord(*r);
    entry_.clear();
    emit entryChanged();
    emit notice(tr("%1 paid on %2's account; %3 still owed").arg(format(amount), qs(r->name), format(r->accountBalance)));
    emit s_->dayChanged();
    return true;
}

// --- when checks close, reopen, or lose a payment -------------------------------------------

void PosService::returnPayment(const Check &c, const Payment &p)
{
    if (p.kind == TenderKind::GiftCard) {
        if (GiftCard *g = s_->giftCard(p.reference)) {
            g->post(now(), p.amount, ss(tr("Back from check #%1").arg(c.id)), c.id, "refund");
            saveGiftCardRecord(*g);
        }
    } else if (p.reference.starts_with("reward:")) {   // the points go back
        if (CustomerRecord *r = s_->customer(c.customerId)) {
            r->points += std::stoi(p.reference.substr(7));
            saveCustomerRecord(*r);
        }
    } else if (p.kind == TenderKind::HouseAccount) {
        if (CustomerRecord *r = s_->customer(p.reference)) {
            r->post(now(), -p.amount, ss(tr("Charge removed, check #%1").arg(c.id)), c.id, "uncharge");
            saveCustomerRecord(*r);
        }
    }
}

void PosService::applyCloseEffects(Check &c)
{
    for (const OrderLine &l : c.lines) {
        if (!l.isGiftCard() || l.voided)
            continue;
        GiftCard *g = s_->giftCard(l.giftCardNumber());
        const bool fresh = !g;
        if (fresh) {
            s_->giftCards.push_back({l.giftCardNumber(), Money(), now(), {}});
            g = &s_->giftCards.back();
        }
        g->post(now(), l.total(), ss((fresh ? tr("Sold on check #%1") : tr("Reloaded on check #%1")).arg(c.id)), c.id, "sale");
        saveGiftCardRecord(*g);
    }
    earnPoints(c);
    countCouponUses(c, 1);
    letHoldGo(c, tr("Paid another way: held card let go"));
    if (CustomerRecord *r = s_->customer(c.customerId)) {
        ++r->visits;
        r->spent += c.totals(s_->settings.tax).total;
        r->lastVisit = c.closedAt;
        rememberOrder(*r, c);
        saveCustomerRecord(*r);
    }
}

QString PosService::reopenBlocked(const Check &c) const
{
    for (const OrderLine &l : c.lines) {
        if (!l.isGiftCard() || l.voided)
            continue;
        const GiftCard *g = s_->giftCard(l.giftCardNumber());
        if (g && g->balance < l.total())
            return tr("Gift card %1 from this check has been spent; it can't be taken back.")
                .arg(lastFour(l.giftCardNumber()));
    }
    return {};
}

void PosService::undoCloseEffects(Check &c)
{
    for (const OrderLine &l : c.lines) {
        if (!l.isGiftCard() || l.voided)
            continue;
        if (GiftCard *g = s_->giftCard(l.giftCardNumber())) {
            g->post(now(), -l.total(), ss(tr("Check #%1 reopened").arg(c.id)), c.id, "reopen");
            saveGiftCardRecord(*g);
        }
    }
    takeBackPoints(c);
    countCouponUses(c, -1);
    if (CustomerRecord *r = s_->customer(c.customerId)) {
        r->visits = std::max(0, r->visits - 1);
        r->spent -= c.totals(s_->settings.tax).total;
        saveCustomerRecord(*r);
    }
}

} // namespace vt::app
