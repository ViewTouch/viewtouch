// PosService: phone orders and deliveries. A regular's last order again,
// the name (and address) a phone order needs before Send, the ready time
// quoted from how busy the kitchen is, the delivery fee, and drivers: who
// took which orders out, and when they were back.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kMinute = 60'000;

QString clock(std::int64_t ms)
{
    return i18n::timeText(QDateTime::fromMSecsSinceEpoch(ms).time());
}

bool phoneOrder(const Check &c) { return c.type == CheckType::Takeout || c.type == CheckType::Delivery; }

// Something ordered (not the fee, a note or a void).
bool ordered(const OrderLine &l) { return !l.isFee() && !l.isComment() && !l.voided; }

} // namespace

// --- a regular's last order --------------------------------------------------------

void PosService::rememberOrder(CustomerRecord &r, const Check &c)
{
    std::vector<OrderLine> lines;
    for (const OrderLine &l : c.lines)
        if (!l.voided && !l.isFee() && !l.isGiftCard())
            lines.push_back(l);
    if (std::ranges::none_of(lines, ordered))
        return;   // nothing worth ordering again
    r.lastOrder = std::move(lines);
    r.lastOrderAt = c.closedAt ? c.closedAt : now();
}

QString PosService::regularText(const std::string &customerId) const
{
    const CustomerRecord *r = customerId.empty() ? nullptr : s_->customer(customerId);
    if (!r)
        return {};
    QStringList out;
    if (r->visits >= 2)
        out << tr("Regular · %n visit(s)", "", r->visits);
    if (!r->note.empty())
        out << qs(r->note);
    return out.join(u"  ·  "_s);
}

QString PosService::lastOrderText(const Check &c) const
{
    const CustomerRecord *r = c.customerId.empty() ? nullptr : s_->customer(c.customerId);
    if (!r || r->lastOrder.empty())
        return {};
    // "2 × Coffee, Water": the same item on several lines counted together.
    QStringList names;
    QList<int> counts;
    for (const OrderLine &l : r->lastOrder) {
        if (!ordered(l))
            continue;
        const qsizetype i = names.indexOf(qs(l.name));
        if (i >= 0) {
            counts[i] += l.quantity;
        } else {
            names << qs(l.name);
            counts << l.quantity;
        }
    }
    QStringList out;
    for (qsizetype i = 0; i < names.size(); ++i)
        out << (counts[i] > 1 ? u"%1 × %2"_s.arg(counts[i]).arg(names[i]) : names[i]);
    return out.join(u", "_s);
}

bool PosService::sameAsLastTime()
{
    if (!require(perm::Order, tr("Ordering")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    const CustomerRecord *r = c->customerId.empty() ? nullptr : s_->customer(c->customerId);
    if (!r)
        return fail(tr("Choose the customer first (their phone number or name)."));
    if (r->lastOrder.empty())
        return fail(tr("%1 has no order on file yet.").arg(qs(r->name)));
    QStringList skipped;
    int added = 0;
    for (OrderLine copy : r->lastOrder) {
        copy.unshare();
        if (!copy.isComment()) {
            const MenuItem *m = findItem(qs(copy.itemId));
            if (!m || !m->available) {
                skipped << qs(copy.name);
                continue;
            }
            if (copy.qualifier == Qualifier::None)   // today's price
                copy.unitPrice = m->priceFor(currentMealPeriod(), c->type == CheckType::Takeout,
                                             c->type == CheckType::Delivery);
        }
        copy.id = c->nextLineId++;
        copy.seat = seat_;        // this order's seat and course, not last time's
        copy.course = course_;
        copy.sent = copy.voided = copy.made = copy.served = false;
        copy.sentAt = copy.madeAt = copy.servedAt = 0;
        for (Modifier &mod : copy.modifiers) {
            mod.made = false;
            mod.madeAt = 0;
        }
        c->lines.push_back(copy);
        selectedLine_ = copy.id;
        added += copy.isComment() ? 0 : copy.quantity;
    }
    if (added == 0)
        return fail(tr("Sold out: %1").arg(skipped.join(u", "_s)));
    emit notice(skipped.isEmpty() ? tr("Same as last time: %n item(s)", nullptr, added)
                                  : tr("Same as last time: %n item(s) (sold out: %1)", nullptr, added)
                                        .arg(skipped.join(u", "_s)));
    changed(*c);
    return true;
}

// --- a name before Send --------------------------------------------------------------

bool PosService::nameRequired() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_ && !t.requireName.empty())
            return t.requireName == "yes";
    if (const Employee *e = user(); e && !e->requireName.empty())
        return e->requireName == "yes";
    return s_->settings.requireOrderName;
}

QString PosService::missingWho(const Check &c) const
{
    if (!phoneOrder(c) || c.training || !nameRequired())
        return {};
    if (c.customer.name.empty())
        return tr("Who is it for? Add their name (+ Name on the check) before sending.");
    if (c.type == CheckType::Delivery && c.customer.address.empty())
        return tr("Where does it go? Add the address (+ Address on the check) before sending.");
    return {};
}

// --- the ready time ------------------------------------------------------------------

int PosService::readyQuote(const Check &c) const
{
    const PosSettings &st = s_->settings;
    const bool delivery = c.type == CheckType::Delivery;
    int minutes = delivery ? st.deliveryMinutes : st.takeoutMinutes;
    // How long the kitchen has been taking today, when it's slower than usual.
    std::int64_t madeMs = 0;
    int made = 0;
    std::set<std::int64_t> cooking;   // other orders the kitchen is still on
    const auto look = [&](const Check &k) {
        for (const OrderLine &l : k.lines) {
            if (!l.sent || l.voided || !l.forKitchen() || l.sentAt <= 0)
                continue;
            if (l.made && l.madeAt > l.sentAt) {
                madeMs += l.madeAt - l.sentAt;
                ++made;
            } else if (!l.made && !k.held(l) && k.id != c.id) {
                cooking.insert(k.id);
            }
        }
    };
    for (const Check &k : s_->closedToday)
        if (!k.training)
            look(k);
    for (const auto &[id, k] : s_->open)
        if (!k.training)
            look(k);
    const int travel = delivery ? std::max(0, st.deliveryMinutes - st.takeoutMinutes) : 0;
    // Rush Radar: what's ahead at each station this order uses, at their pace.
    // Once every one has shown its pace, that is the kitchen's time (the
    // orders ahead are in it); before, a few minutes for each order cooking.
    bool known = false;
    const int kitchen = kitchenMinutesFor(c, &known);
    if (known && kitchen > 0)
        return (std::max(minutes, kitchen + travel) + 4) / 5 * 5;
    if (made >= 3) {
        const int cook = int(madeMs / made / kMinute);
        minutes = std::max(minutes, cook + travel);
    }
    minutes = std::max(minutes, kitchen + travel);
    minutes += int(cooking.size()) * st.minutesPerOrderWaiting;
    return (minutes + 4) / 5 * 5;   // to the next 5 minutes
}

// --- the delivery fee ----------------------------------------------------------------

void PosService::applyDeliveryFee(Check &c)
{
    if (c.type != CheckType::Delivery)
        return;
    const bool food = std::ranges::any_of(c.lines, ordered);
    const bool has = std::ranges::any_of(c.lines, [](const OrderLine &l) { return l.isFee(); });
    if (!food) {
        // Nothing ordered (any more): no fee either, unless it was paid toward.
        if (has && c.payments.empty())
            std::erase_if(c.lines, [](const OrderLine &l) { return l.isFee(); });
        return;
    }
    if (has || s_->settings.deliveryFee.cents() <= 0)
        return;
    OrderLine fee;
    fee.id = c.nextLineId++;
    fee.itemId = "fee:delivery";
    fee.name = ss(tr("Delivery fee"));
    fee.unitPrice = s_->settings.deliveryFee;
    fee.taxClass = TaxClass::None;
    fee.kitchenHide = true;
    fee.noDiscount = fee.noStaffDiscount = true;
    // Already "sent": nothing for the kitchen, and taking it off is a void.
    fee.sent = true;
    fee.sentAt = now();
    c.lines.push_back(fee);
}

// --- drivers ------------------------------------------------------------------------

QVariantList PosService::drivers() const
{
    QVariantList out;
    if (!user())
        return out;
    for (const Employee &e : s_->employees) {
        if (!e.active)
            continue;
        const bool driver = e.role == "driver"
                            || std::ranges::any_of(e.otherJobs, [](const Job &j) { return j.role == "driver"; });
        if (!driver)
            continue;
        const bool onClock = std::ranges::any_of(s_->punches, [&](const TimePunch &p) {
            return p.employeeId == e.id && p.open();
        });
        int out_ = 0;
        for (const auto &[id, c] : s_->open)
            if (c.driverId == e.id && c.outAt && !c.deliveredAt)
                ++out_;
        out.append(QVariantMap{{u"id"_s, qs(e.id)}, {u"name"_s, qs(e.name)}, {u"clockedIn"_s, onClock}, {u"out"_s, out_}});
    }
    std::ranges::stable_sort(out, [](const QVariant &a, const QVariant &b) {
        return a.toMap()[u"clockedIn"_s].toBool() && !b.toMap()[u"clockedIn"_s].toBool();
    });
    return out;
}

QVariantList PosService::deliveries() const
{
    QVariantList out;
    if (!user())
        return out;
    const std::int64_t t = now();
    std::vector<const Check *> list;
    for (const auto &[id, c] : s_->open)
        if (c.type == CheckType::Delivery && !c.training && std::ranges::any_of(c.lines, ordered))
            list.push_back(&c);
    // The oldest promise first.
    std::ranges::sort(list, {}, [](const Check *c) { return c->promisedAt ? c->promisedAt : c->openedAt; });
    for (const Check *c : list) {
        bool unsent = false, cooking = false;
        for (const OrderLine &l : c->lines) {
            if (!ordered(l))
                continue;
            if (!l.sent)
                unsent = true;
            else if (l.forKitchen() && !l.made && !c->held(l))
                cooking = true;
        }
        const QString state = c->deliveredAt ? u"back"_s
                            : c->outAt       ? u"out"_s
                            : unsent         ? u"new"_s
                            : cooking        ? u"cooking"_s
                                             : u"ready"_s;
        const Totals totals = c->totals(s_->settings.tax);
        QVariantMap row{
            {u"id"_s, qint64(c->id)}, {u"label"_s, qs(c->label)}, {u"state"_s, state},
            {u"name"_s, qs(c->customer.name)}, {u"phone"_s, qs(c->customer.phone)},
            {u"address"_s, qs(c->customer.address)}, {u"note"_s, qs(c->customer.note)},
            {u"total"_s, format(totals.total)}, {u"balance"_s, format(totals.balance)},
            {u"paid"_s, totals.balance.cents() <= 0},
            {u"driver"_s, qs(c->driverName)}, {u"driverId"_s, qs(c->driverId)},
            {u"busyOn"_s, lockHolder(c->id)},
        };
        if (c->promisedAt) {
            row.insert(u"promised"_s, clock(c->promisedAt));
            row.insert(u"late"_s, !c->outAt && t > c->promisedAt);
        }
        if (c->outAt)
            row.insert(u"outMinutes"_s, int(((c->deliveredAt ? c->deliveredAt : t) - c->outAt) / kMinute));
        out.append(row);
    }
    return out;
}

bool PosService::sendOut(const QVariantList &checkIds, const QString &driverId)
{
    if (!require(perm::Order, tr("Deliveries")))
        return false;
    const Employee *driver = s_->employee(ss(driverId));
    if (!driver)
        return fail(tr("Choose the driver."));
    if (checkIds.isEmpty())
        return fail(tr("Choose the orders going out."));
    std::vector<Check *> going;
    for (const QVariant &v : checkIds) {
        const auto it = s_->open.find(v.toLongLong());
        if (it == s_->open.end() || it->second.type != CheckType::Delivery)
            return fail(tr("That delivery is closed."));
        Check &c = it->second;
        if (c.unsentCount() > 0)
            return fail(tr("%1 hasn't been sent to the kitchen yet.").arg(qs(c.label)));
        if (c.outAt && !c.deliveredAt)
            return fail(tr("%1 is already out with %2.").arg(qs(c.label), qs(c.driverName)));
        going.push_back(&c);
    }
    for (Check *c : going) {
        c->driverId = driver->id;
        c->driverName = driver->name;
        c->outAt = now();
        c->deliveredAt = 0;
        // Still the check of whoever took the order (its sale and tip); the
        // driver collects for it, into their own bank.
        noteEvent(*c, tr("Out for delivery with %1").arg(qs(driver->name)), "delivery");
        changed(*c);
    }
    emit notice(tr("%n order(s) out with %1", nullptr, int(going.size())).arg(qs(driver->name)));
    return true;
}

bool PosService::deliveryBack(qint64 checkId)
{
    if (!require(perm::Order, tr("Deliveries")))
        return false;
    const auto it = s_->open.find(checkId);
    if (it == s_->open.end() || !it->second.outAt)
        return fail(tr("That order isn't out for delivery."));
    Check &c = it->second;
    if (!c.deliveredAt) {
        c.deliveredAt = now();
        noteEvent(c, tr("Delivered"), "delivery");
        changed(c);
    }
    emit notice(tr("%1 delivered (%2 min)").arg(qs(c.label)).arg((c.deliveredAt - c.outAt) / kMinute));
    return true;
}

} // namespace vt::app
