// PosService: how a screen serves (restaurant, food truck, retail), and the
// food truck's counter - no login, straight to ordering and paying.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

const Employee &counterEmployee()
{
    // Nothing translated in here (see maintainerEmployee); shown translated by nameOf.
    static const Employee e = [] {
        Employee k;
        k.id = "counter";
        k.name = "Counter";
        k.role = "cashier";   // orders and payments, everyone's checks; never a manager's
        return k;
    }();
    return e;
}

QString PosService::serviceMode() const
{
    std::string mode = s_->settings.serviceMode;
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_ && !t.service.empty())
            mode = t.service;
    if (mode == "foodTruck" || mode == "retail")
        return qs(mode);
    return u"restaurant"_s;
}

bool PosService::numbersOrders() const
{
    return counterMode() || (s_->settings.orderNumbers && serviceMode() == u"restaurant" && !selfOrder_);
}

QVariantMap PosService::serviceInfo() const
{
    const bool truck = counterMode();
    const Employee *person = truck ? s_->employee(counterUser_) : nullptr;
    return {{u"mode"_s, serviceMode()},
            {u"counter"_s, truck && !userId_.empty() && userId_ == counterUser_},
            {u"steppedIn"_s, truck && !counterUser_.empty() && !userId_.empty() && userId_ != counterUser_},
            {u"askStaff"_s, s_->settings.truckAskStaff},
            {u"person"_s, person && person->id != counterEmployee().id ? nameOf(*person) : QString()},
            {u"needsName"_s, s_->settings.truckNeedsName},
            // Orders called by number: the Orders button, Now Serving.
            {u"numbers"_s, truck || (s_->settings.orderNumbers && serviceMode() == u"restaurant")}};
}

void PosService::applyServiceMode()
{
    // Called with every settings change: says so only when something did.
    const auto shown = [this] {
        if (const QVariantMap now = serviceInfo(); now != serviceShown_) {
            serviceShown_ = now;
            emit sessionChanged();
        }
    };
    if (!counterMode()) {
        // Not a food truck (any more): the Counter isn't anyone to stay
        // logged in as (a person who said they're working stays).
        counterUser_.clear();
        if (userId_ == counterEmployee().id)
            logout();
        shown();
        return;
    }
    // Someone is here already (a manager, who switched it on), or the
    // screen asks who's working: the login page does.
    if (!userId_.empty() || s_->settings.truckAskStaff) {
        shown();
        return;
    }
    counterUser_ = counterEmployee().id;
    userId_ = counterUser_;
    serviceShown_ = serviceInfo();
    emit sessionChanged();
    emit loggedInChanged(true);
}

bool PosService::switchCounterPerson()
{
    if (!counterMode())
        return fail(tr("This screen isn't a food truck's."));
    // Who was working logs out (the order in progress waits for them);
    // the next PIN is whoever works now.
    if (!counterUser_.empty() && userId_ != counterUser_)
        logout();   // back to the counter first
    counterUser_.clear();
    logout();
    if (s_->settings.truckAskStaff)
        emit notice(tr("Who's working? Enter your PIN."));
    return true;
}

int PosService::nextOrderNumber() const
{
    // From 1 each business day: the day's checks (open ones too) say how far it is.
    int last = 0;
    for (const auto &[id, c] : s_->open)
        last = std::max(last, c.orderNumber);
    for (const Check &c : s_->closedToday)
        last = std::max(last, c.orderNumber);
    return std::max(last + 1, s_->firstOrderNumber);
}

QString PosService::orderLabel(const Check &c) const
{
    const QString name = qs(c.customer.name).left(20);
    return name.isEmpty() ? u"#%1"_s.arg(c.orderNumber) : u"#%1 %2"_s.arg(c.orderNumber).arg(name);
}

// --- the window: pagers, Ready, picked up ------------------------------------------

namespace {

Check *truckOrder(PosShared *s, std::int64_t id)
{
    if (auto it = s->open.find(id); it != s->open.end())
        return it->second.orderNumber > 0 ? &it->second : nullptr;
    for (Check &c : s->closedToday)
        if (c.id == id)
            return c.orderNumber > 0 ? &c : nullptr;
    return nullptr;
}

// Waiting at the window: sent to the kitchen (or paid), not picked up yet.
bool waiting(const Check &c)
{
    return c.orderNumber > 0 && !c.training && !c.pickedUpAt
           && (c.status == CheckStatus::Closed || c.status == CheckStatus::Open)
           && (c.status == CheckStatus::Closed || std::ranges::any_of(c.lines, [](const OrderLine &l) { return l.sent && !l.voided; }));
}

} // namespace

bool PosService::setPager(int pager)
{
    if (!require(perm::Order, tr("Handing out a pager")))
        return false;
    Check *c = current();
    if (!c || c->orderNumber == 0)
        return fail(tr("Pagers go with a food truck's orders."));
    if (pager < 0 || pager > 999)
        return fail(tr("Pagers are numbered 1 to 999."));
    if (pager > 0) {
        const auto out = [&](const Check &o) { return o.id != c->id && o.pager == pager && !o.pickedUpAt && !o.training; };
        for (const auto &[id, o] : s_->open)
            if (out(o))
                return fail(tr("Pager %1 is still out with %2.").arg(pager).arg(qs(o.label)));
        for (const Check &o : s_->closedToday)
            if (out(o))
                return fail(tr("Pager %1 is still out with %2.").arg(pager).arg(qs(o.label)));
    }
    c->pager = pager;
    changed(*c);
    return true;
}

void PosService::callOrder(Check &c)
{
    if (!c.readyAt) {
        c.readyAt = now();
        // Texted once, when first called (Call Again doesn't), if the store says what.
        if (!s_->settings.orderReadyText.empty() && !c.customer.phone.empty() && s_->sendText && !c.training) {
            QString message = qs(s_->settings.orderReadyText);
            message.replace(u"{name}"_s, qs(c.customer.name))
                .replace(u"{number}"_s, u"#"_s + QString::number(c.orderNumber))
                .replace(u"{store}"_s, qs(s_->settings.storeName));
            s_->sendText(qs(c.customer.phone), message.simplified());
        }
    }
    if (s_->sink)
        s_->sink->saveCheck(c);
    emit s_->checksChanged();
    const QString label = qs(c.label);
    QString said = tr("%1 is ready").arg(label);
    if (c.pager > 0)   // keyed in on the pager system's transmitter
        said = tr("%1 is ready: buzz pager %2").arg(label).arg(c.pager);
    emit s_->orderReady(said);
    if (!counterMode())
        emit notice(said);   // said here too (the kitchen's screen, a restaurant's)
}

bool PosService::orderReady(qint64 checkId)
{
    if (!require(perm::Order, tr("Calling an order")))
        return false;
    Check *c = truckOrder(s_, checkId);
    if (!c || c->pickedUpAt)
        return fail(tr("That order isn't waiting."));
    callOrder(*c);   // again: said again
    return true;
}

bool PosService::orderPickedUp(qint64 checkId)
{
    if (!require(perm::Order, tr("Handing out an order")))
        return false;
    Check *c = truckOrder(s_, checkId);
    if (!c || c->pickedUpAt)
        return fail(tr("That order isn't waiting."));
    c->pickedUpAt = now();
    if (!c->readyAt)
        c->readyAt = c->pickedUpAt;
    if (s_->sink)
        s_->sink->saveCheck(*c);
    emit s_->checksChanged();
    emit notice(c->pager > 0 ? tr("%1 picked up: pager %2 is back").arg(qs(c->label)).arg(c->pager)
                             : tr("%1 picked up").arg(qs(c->label)));
    return true;
}

QVariantList PosService::truckOrders() const
{
    std::vector<const Check *> list;
    for (const auto &[id, c] : s_->open)
        if (waiting(c))
            list.push_back(&c);
    for (const Check &c : s_->closedToday)
        if (waiting(c))
            list.push_back(&c);
    // Still being made first (oldest on top), then those called, waiting for their guest.
    const auto sentAt = [](const Check *c) {
        std::int64_t first = 0;
        for (const OrderLine &l : c->lines)
            if (l.sent && !l.voided && (!first || l.sentAt < first))
                first = l.sentAt;
        return first ? first : c->closedAt ? c->closedAt : c->openedAt;
    };
    std::ranges::sort(list, [&](const Check *a, const Check *b) {
        if ((a->readyAt != 0) != (b->readyAt != 0))
            return a->readyAt == 0;
        return sentAt(a) != sentAt(b) ? sentAt(a) < sentAt(b) : a->orderNumber < b->orderNumber;
    });
    QVariantList out;
    const std::int64_t t = now();
    for (const Check *c : list) {
        QStringList items;
        for (const OrderLine &l : c->lines)
            if (!l.voided && !l.isComment())
                items << (l.quantity > 1 ? u"%1 %2"_s.arg(l.quantity).arg(qs(l.name)) : qs(l.name));
        out.append(QVariantMap{{u"id"_s, qint64(c->id)}, {u"number"_s, c->orderNumber},
                               {u"name"_s, qs(c->customer.name)}, {u"label"_s, qs(c->label)}, {u"pager"_s, c->pager},
                               {u"items"_s, items.join(u", "_s)},
                               {u"minutes"_s, int((t - sentAt(c)) / 60'000)}, {u"ready"_s, c->readyAt != 0},
                               {u"readyMinutes"_s, c->readyAt ? int((t - c->readyAt) / 60'000) : 0},
                               {u"paid"_s, c->status == CheckStatus::Closed}});
    }
    return out;
}

} // namespace vt::app
