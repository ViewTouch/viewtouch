// Implementation of the pure-C++ POS domain (tax, menu, employees, checks).

#include "core/settings.hh"
#include "core/check.hh"
#include "core/waitlist.hh"
#include "core/day.hh"
#include "core/employee.hh"
#include "core/menu.hh"
#include "core/tax.hh"

#include <algorithm>
#include <cstdlib>

namespace vt::core {

// --- tax ---------------------------------------------------------------------

std::string toString(TaxClass c)
{
    switch (c) {
    case TaxClass::None: return "none";
    case TaxClass::Food: return "food";
    case TaxClass::Alcohol: return "alcohol";
    case TaxClass::Merchandise: return "merchandise";
    case TaxClass::Room: return "room";
    }
    return "food";
}

TaxClass taxClassFromString(const std::string &s)
{
    if (s == "none") return TaxClass::None;
    if (s == "alcohol") return TaxClass::Alcohol;
    if (s == "merchandise") return TaxClass::Merchandise;
    if (s == "room") return TaxClass::Room;
    return TaxClass::Food;
}

// --- menu --------------------------------------------------------------------

std::string toString(Qualifier q)
{
    switch (q) {
    case Qualifier::None: return "";
    case Qualifier::No: return "no";
    case Qualifier::Lite: return "lite";
    case Qualifier::Extra: return "extra";
    case Qualifier::Side: return "side";
    case Qualifier::Only: return "only";
    case Qualifier::Double: return "double";
    case Qualifier::Sub: return "sub";
    }
    return "";
}

Qualifier qualifierFromString(const std::string &s)
{
    if (s == "no") return Qualifier::No;
    if (s == "lite") return Qualifier::Lite;
    if (s == "extra") return Qualifier::Extra;
    if (s == "side") return Qualifier::Side;
    if (s == "only") return Qualifier::Only;
    if (s == "double") return Qualifier::Double;
    if (s == "sub") return Qualifier::Sub;
    return Qualifier::None;
}

std::string qualifierPrefix(Qualifier q)
{
    switch (q) {
    case Qualifier::None: return "";
    case Qualifier::No: return "No ";
    case Qualifier::Lite: return "Lite ";
    case Qualifier::Extra: return "Extra ";
    case Qualifier::Side: return "Side of ";
    case Qualifier::Only: return "Only ";
    case Qualifier::Double: return "Double ";
    case Qualifier::Sub: return "SUB ";
    }
    return "";
}

Money qualifiedPrice(Money unit, Qualifier q)
{
    switch (q) {
    case Qualifier::No:
    case Qualifier::Lite:
        return Money();
    case Qualifier::Double:
        return unit * 2;
    default:
        return unit;
    }
}

// --- employees -----------------------------------------------------------------

std::set<std::string> permissionsForRole(const std::string &role)
{
    if (role == "admin" || role == "manager")
        return {perm::Order, perm::Settle, perm::OthersChecks, perm::Discount, perm::Void, perm::Manager,
                perm::EditLayout};
    // Discounts, comps and staff meals need a manager (or their PIN on the spot).
    // The counter takes everyone's payments; a server works their own checks.
    if (role == "cashier")
        return {perm::Order, perm::Settle, perm::OthersChecks};
    if (role == "server" || role == "bartender" || role == "driver")
        return {perm::Order, perm::Settle};
    if (role == "host" || role == "kiosk")
        return {perm::Order};   // the waitlist and seating; guests ordering on their own
    // A busser clocks in and out (and gets a share of the tips).
    return {};
}

// --- day ---------------------------------------------------------------------------

std::string toString(CashMovement::Kind k)
{
    switch (k) {
    case CashMovement::Kind::Payout: return "payout";
    case CashMovement::Kind::PaidIn: return "paidIn";
    case CashMovement::Kind::TipPayout: return "tipPayout";
    case CashMovement::Kind::NoSale: return "noSale";
    }
    return "payout";
}

CashMovement::Kind cashMovementKindFromString(const std::string &s)
{
    if (s == "paidIn") return CashMovement::Kind::PaidIn;
    if (s == "tipPayout") return CashMovement::Kind::TipPayout;
    if (s == "noSale") return CashMovement::Kind::NoSale;
    return CashMovement::Kind::Payout;
}

// --- enums -------------------------------------------------------------------------

std::string toString(CheckType t)
{
    switch (t) {
    case CheckType::DineIn: return "dineIn";
    case CheckType::Takeout: return "takeout";
    case CheckType::Quick: return "quick";
    case CheckType::Delivery: return "delivery";
    case CheckType::Tab: return "tab";
    }
    return "dineIn";
}

CheckType checkTypeFromString(const std::string &s)
{
    if (s == "takeout") return CheckType::Takeout;
    if (s == "quick") return CheckType::Quick;
    if (s == "delivery") return CheckType::Delivery;
    if (s == "tab") return CheckType::Tab;
    return CheckType::DineIn;
}

std::string toString(CheckStatus s)
{
    switch (s) {
    case CheckStatus::Open: return "open";
    case CheckStatus::Closed: return "closed";
    case CheckStatus::Discarded: return "discarded";
    case CheckStatus::Merged: return "merged";
    }
    return "open";
}

CheckStatus checkStatusFromString(const std::string &s)
{
    if (s == "closed") return CheckStatus::Closed;
    if (s == "discarded") return CheckStatus::Discarded;
    if (s == "merged") return CheckStatus::Merged;
    return CheckStatus::Open;
}

std::string toString(TenderKind k)
{
    switch (k) {
    case TenderKind::Cash: return "cash";
    case TenderKind::Card: return "card";
    case TenderKind::Discount: return "discount";
    case TenderKind::GiftCard: return "giftcard";
    case TenderKind::HouseAccount: return "house";
    }
    return "cash";
}

std::string toString(Party::Status s)
{
    switch (s) {
    case Party::Status::Booked: return "booked";
    case Party::Status::Waiting: return "waiting";
    case Party::Status::Notified: return "notified";
    case Party::Status::Seated: return "seated";
    case Party::Status::Left: return "left";
    case Party::Status::NoShow: return "noShow";
    }
    return "waiting";
}

Party::Status partyStatusFromString(const std::string &s)
{
    if (s == "booked") return Party::Status::Booked;
    if (s == "notified") return Party::Status::Notified;
    if (s == "seated") return Party::Status::Seated;
    if (s == "left") return Party::Status::Left;
    if (s == "noShow") return Party::Status::NoShow;
    return Party::Status::Waiting;
}

TenderKind tenderKindFromString(const std::string &s)
{
    if (s == "card") return TenderKind::Card;
    if (s == "discount") return TenderKind::Discount;
    if (s == "giftcard") return TenderKind::GiftCard;
    if (s == "house") return TenderKind::HouseAccount;
    return TenderKind::Cash;
}

// --- check ---------------------------------------------------------------------------

std::string OrderLine::weightText() const
{
    if (weight <= 0)
        return {};
    // Two decimals unless the scale gave three: "1.25 lb", "0.125 kg".
    std::string digits = std::to_string(weight / 1000) + "." + std::to_string(1000 + weight % 1000).substr(1);
    if (digits.back() == '0')
        digits.pop_back();
    return " " + digits + " " + (weightUnit.empty() ? std::string("lb") : weightUnit);
}

Money OrderLine::total() const
{
    const Money whole = wholeTotal();
    if (!isShare())
        return whole;
    // Even parts; the first |leftover| pieces take a cent more (or less).
    const std::int64_t cents = whole.cents();
    const std::int64_t base = cents / shareOf;
    const std::int64_t left = cents - base * shareOf;
    const std::int64_t extra = share <= std::abs(left) ? (left > 0 ? 1 : -1) : 0;
    return Money::fromCents(base + extra);
}

Money OrderLine::wholeTotal() const
{
    if (voided)
        return Money();
    Money each = qualifiedPrice(unitPrice, qualifier);
    if (weight > 0)   // per pound (or kg...): rounded half up to the cent
        each = Money::fromCents((each.cents() * weight + (each.cents() >= 0 ? 500 : -500)) / 1000);
    for (const Modifier &m : modifiers)
        each += m.price();
    return each * quantity;
}

OrderLine *Check::line(std::int64_t lineId)
{
    auto it = std::ranges::find(lines, lineId, &OrderLine::id);
    return it == lines.end() ? nullptr : &*it;
}

const OrderLine *Check::line(std::int64_t lineId) const
{
    auto it = std::ranges::find(lines, lineId, &OrderLine::id);
    return it == lines.end() ? nullptr : &*it;
}

OrderLine *Check::lastItemLine()
{
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (!it->isComment() && !it->sent && !it->voided)
            return &*it;
    }
    return nullptr;
}

OrderLine &Check::addItem(const MenuItem &item, Qualifier q)
{
    OrderLine l;
    l.id = nextLineId++;
    l.itemId = item.id;
    l.name = item.name;
    l.unitPrice = item.price;
    l.taxClass = item.taxClass;
    l.qualifier = q;
    l.printer = item.printer;
    l.station = item.station;
    l.kitchenName = item.kitchenName;
    l.kitchenColor = item.kitchenColor;
    l.allergens = item.allergens;
    l.kitchenHide = item.kitchenHide;
    l.noDiscount = item.noDiscount;
    l.noStaffDiscount = item.noStaffDiscount;
    if (item.byWeight)
        l.weightUnit = item.weightUnit;
    lines.push_back(l);
    return lines.back();
}

bool Check::addModifier(std::int64_t lineId, const MenuItem &modifier, Qualifier q)
{
    OrderLine *l = line(lineId);
    if (!l || l->sent || l->voided || l->isComment())
        return false;
    l->modifiers.push_back({modifier.id, modifier.name, modifier.price, q, {}, modifier.kitchenName, modifier.kitchenHide,
                            modifier.station});
    return true;
}

OrderLine &Check::addComment(const std::string &text)
{
    OrderLine l;
    l.id = nextLineId++;
    l.name = text;
    l.taxClass = TaxClass::None;
    lines.push_back(l);
    return lines.back();
}

bool Check::removeLine(std::int64_t lineId)
{
    const OrderLine *l = line(lineId);
    if (!l || l->sent)
        return false;
    std::erase_if(lines, [&](const OrderLine &x) { return x.id == lineId; });
    return true;
}

bool Check::voidLine(std::int64_t lineId)
{
    OrderLine *l = line(lineId);
    if (!l || !l->sent || l->voided)
        return false;
    l->voided = true;
    return true;
}

std::vector<OrderLine> Check::sendable(bool everything) const
{
    std::vector<OrderLine> out;
    for (const OrderLine &l : lines) {
        if (!l.sent && (everything || !held(l)))
            out.push_back(l);
    }
    return out;
}

int Check::heldCount() const
{
    return int(std::ranges::count_if(lines, [this](const OrderLine &l) { return held(l); }));
}

int Check::fireNextCourse()
{
    int next = 0;
    for (const OrderLine &l : lines) {
        if (held(l) && (next == 0 || l.course < next))
            next = l.course;
    }
    if (next > 0)
        firedCourse = next;
    return next;
}

int Check::sendAll(std::int64_t now, bool everything)
{
    if (everything) {
        for (const OrderLine &l : lines)
            firedCourse = std::max(firedCourse, l.course);
    }
    // Each send is one kitchen ticket, told apart by its time: keep them
    // distinct even for two sends within the same millisecond.
    for (const OrderLine &l : lines) {
        if (l.sent && l.sentAt >= now)
            now = l.sentAt + 1;
    }
    int count = 0;
    for (OrderLine &l : lines) {
        if (!l.sent && !held(l)) {
            l.sent = true;
            l.sentAt = now;
            ++count;
        }
    }
    return count;
}

int Check::unsentCount() const
{
    return int(std::ranges::count_if(lines, [](const OrderLine &l) { return !l.sent; }));
}

std::optional<OrderLine> Check::takeLine(std::int64_t lineId)
{
    auto it = std::ranges::find(lines, lineId, &OrderLine::id);
    if (it == lines.end())
        return std::nullopt;
    OrderLine l = std::move(*it);
    lines.erase(it);
    return l;
}

OrderLine &Check::adoptLine(OrderLine line)
{
    line.id = nextLineId++;
    lines.push_back(std::move(line));
    return lines.back();
}

std::optional<OrderLine> Check::takeOne(std::int64_t lineId)
{
    OrderLine *l = line(lineId);
    if (!l)
        return std::nullopt;
    if (l->quantity <= 1 || l->isShare())
        return takeLine(lineId);
    OrderLine one = *l;
    one.quantity = 1;
    --l->quantity;
    return one;
}

std::vector<std::int64_t> Check::shareLine(std::int64_t lineId, int parts, std::int64_t group)
{
    OrderLine *l = line(lineId);
    if (!l || l->isComment() || l->voided || l->isShare() || l->quantity != 1 || parts < 2)
        return {};
    l->share = 1;
    l->shareOf = parts;
    l->shareGroup = group;
    std::vector<std::int64_t> ids{l->id};
    const OrderLine first = *l;
    auto at = std::ranges::find(lines, lineId, &OrderLine::id) + 1;
    for (int i = 2; i <= parts; ++i) {
        OrderLine piece = first;
        piece.id = nextLineId++;
        piece.share = i;
        ids.push_back(piece.id);
        at = lines.insert(at, std::move(piece)) + 1;
    }
    return ids;
}

void Check::absorb(Check &other)
{
    for (OrderLine &l : other.lines)
        adoptLine(std::move(l));
    for (Payment &p : other.payments) {
        p.id = nextPaymentId++;
        payments.push_back(std::move(p));
    }
    other.lines.clear();
    other.payments.clear();
    guests += other.guests;
    rush = rush || other.rush;
    vip = vip || other.vip;
    if (customer.empty())
        customer = other.customer;
}

Payment &Check::addPayment(const Tender &tender, Money amount)
{
    Payment p;
    p.id = nextPaymentId++;
    p.tenderId = tender.id;
    p.tenderName = tender.name;
    p.kind = tender.kind;
    if (tender.kind == TenderKind::Discount) {
        p.percentBp = tender.percentBp;
        p.staffMeal = tender.staffMeal;
    }
    else
        p.amount = amount;
    payments.push_back(p);
    return payments.back();
}

bool Check::removePayment(std::int64_t paymentId)
{
    return std::erase_if(payments, [&](const Payment &p) { return p.id == paymentId; }) > 0;
}

Money Check::changeFrom(const Payment &p, const TaxRates &rates) const
{
    if (p.kind != TenderKind::Cash)
        return {};
    Money change = totals(rates).change;
    for (auto it = payments.rbegin(); it != payments.rend() && change.cents() > 0; ++it) {
        if (it->kind != TenderKind::Cash)
            continue;
        const Money share = it->amount < change ? it->amount : change;
        if (it->id == p.id)
            return share;
        change -= share;
    }
    return {};
}

Money Check::kept(const Payment &p, const TaxRates &rates) const
{
    return p.amount + p.tip - changeFrom(p, rates);
}

Totals Check::totals(const TaxRates &rates) const
{
    Totals t;

    std::map<TaxClass, Money> byClass;
    Money discountable, staffDiscountable;   // what percent discounts and staff meals apply to
    for (const OrderLine &l : lines) {
        const Money lt = l.total();
        t.items += lt;
        if (l.isGiftCard())
            t.giftCardsSold += lt;
        if (!l.isGiftCard() && !l.noDiscount)
            discountable += lt;
        if (!l.isGiftCard() && !l.noDiscount && !l.noStaffDiscount)
            staffDiscountable += lt;
        if (l.taxClass != TaxClass::None)
            byClass[l.taxClass] += lt;
    }

    // Discounts: each is a share of the items total; together they never
    // exceed it.
    for (const Payment &p : payments) {
        if (p.kind != TenderKind::Discount)
            continue;
        // Percent off what may be discounted, or a fixed amount (rewards, promotions).
        const Money off = (p.staffMeal ? staffDiscountable : discountable).percent(p.percentBp) + p.amount;
        t.discounts += off;
        t.discountOf[p.id] = off;
        if (p.staffMeal)
            t.staffMeals += off;
    }
    if (t.discounts > t.items - t.giftCardsSold)   // nothing comes off a gift card bought
        t.discounts = t.items - t.giftCardsSold;
    t.subtotal = t.items - t.discounts;

    // Spread the discount over tax classes in proportion to their sales,
    // giving rounding leftovers to the largest class, then tax each class
    // on its net total.
    Money taxableTotal;
    for (const auto &[cls, amount] : byClass)
        taxableTotal += amount;
    // The part of the discount that falls on taxable sales (comments and
    // untaxed lines take the rest; gift cards sold take none: never discounted).
    const Money discounted = t.items - t.giftCardsSold;
    const Money taxableDiscount = taxableTotal.cents() > 0 && discounted.cents() > 0
        ? Money::fromCents(t.discounts.cents() * taxableTotal.cents() / discounted.cents())
        : Money();
    Money allocated;
    TaxClass largest = TaxClass::None;
    Money largestAmount;
    std::map<TaxClass, Money> discountShare;
    for (const auto &[cls, amount] : byClass) {
        if (largest == TaxClass::None || amount > largestAmount) {
            largest = cls;
            largestAmount = amount;
        }
        if (taxableTotal.cents() > 0) {
            const Money share = Money::fromCents(taxableDiscount.cents() * amount.cents() / taxableTotal.cents());
            discountShare[cls] = share;
            allocated += share;
        }
    }
    if (largest != TaxClass::None)
        discountShare[largest] += taxableDiscount - allocated;

    for (const auto &[cls, amount] : byClass) {
        if (!taxExempt.empty())
            break;   // an exemption certificate: no tax
        if (cls == TaxClass::Food && type == CheckType::Takeout && !rates.taxTakeoutFood)
            continue;
        const Money net = amount - discountShare[cls];
        const Money tax = net.cents() > 0 ? net.ppm(rates.ratePpm(cls)) : Money();
        if (net.cents() > 0 && rates.ratePpm(cls) > 0)
            t.taxableByClass[cls] = net;
        if (tax.cents() != 0)
            t.taxByClass[cls] = tax;
        t.tax += tax;
    }

    // Gratuity: on the (discounted) subtotal, not taxed, added after tax.
    t.gratuity = t.netSales().percent(gratuityBp);   // not on gift cards bought
    // Taxed too where the state says so: at the food rate.
    if (rates.taxGratuity && taxExempt.empty() && t.gratuity.cents() > 0 && rates.ratePpm(TaxClass::Food) > 0) {
        const Money tax = t.gratuity.ppm(rates.ratePpm(TaxClass::Food));
        t.taxableGratuity = t.gratuity;
        t.taxByClass[TaxClass::Food] += tax;
        t.tax += tax;
    }
    t.total = t.subtotal + t.tax + t.gratuity;
    for (const Payment &p : payments) {
        t.tips += p.tip;
        if (p.kind != TenderKind::Discount)
            t.paid += p.amount;
        if (p.kind == TenderKind::Cash)
            t.cashPaid += p.amount;
    }
    // Paid in cash where pennies are gone: what was owed when the cash came
    // is rounded to the nearest 5 (10) cents.
    if (rates.cashRoundingCents > 1 && t.cashPaid.cents() > 0) {
        const std::int64_t step = rates.cashRoundingCents;
        const std::int64_t due = (t.total - (t.paid - t.cashPaid)).cents();
        if (due > 0)
            t.rounding = Money::fromCents((due + step / 2) / step * step - due);
    }
    t.balance = t.total + t.rounding - t.paid;
    t.change = t.balance.cents() < 0 ? -t.balance : Money();
    return t;
}

const std::vector<StateRules> &stateRules()
{
    // code, name, daily OT, daily double, 7th day, meal break by (hours), gift card cash-out under (cents),
    // a paid rest break every (hours)
    static const std::vector<StateRules> states = {
        {"AL", "Alabama"}, {"AK", "Alaska", 8}, {"AZ", "Arizona"}, {"AR", "Arkansas"},
        {"CA", "California", 8, 12, true, 5, 1000, 4}, {"CO", "Colorado", 12, 0, false, 5, 500, 4},
        {"CT", "Connecticut", 0, 0, false, 7}, {"DE", "Delaware", 0, 0, false, 7}, {"DC", "District of Columbia"},
        {"FL", "Florida"}, {"GA", "Georgia"}, {"HI", "Hawaii"}, {"ID", "Idaho"},
        {"IL", "Illinois", 0, 0, false, 5}, {"IN", "Indiana"}, {"IA", "Iowa"}, {"KS", "Kansas"},
        {"KY", "Kentucky", 0, 0, false, 5, 0, 4}, {"LA", "Louisiana"}, {"ME", "Maine", 0, 0, false, 6, 500},
        {"MD", "Maryland"}, {"MA", "Massachusetts", 0, 0, false, 6, 500}, {"MI", "Michigan"},
        {"MN", "Minnesota", 0, 0, false, 8, 0, 4}, {"MS", "Mississippi"}, {"MO", "Missouri"},
        {"MT", "Montana", 0, 0, false, 0, 500}, {"NE", "Nebraska"}, {"NV", "Nevada", 8, 0, false, 8, 0, 4},
        {"NH", "New Hampshire", 0, 0, false, 5}, {"NJ", "New Jersey", 0, 0, false, 0, 500},
        {"NM", "New Mexico"}, {"NY", "New York", 0, 0, false, 6}, {"NC", "North Carolina"},
        {"ND", "North Dakota", 0, 0, false, 5}, {"OH", "Ohio"}, {"OK", "Oklahoma"},
        {"OR", "Oregon", 0, 0, false, 5, 500, 4}, {"PA", "Pennsylvania"}, {"RI", "Rhode Island", 0, 0, false, 6, 100},
        {"SC", "South Carolina"}, {"SD", "South Dakota"}, {"TN", "Tennessee", 0, 0, false, 6}, {"TX", "Texas"},
        {"UT", "Utah"}, {"VT", "Vermont", 0, 0, false, 0, 100}, {"VA", "Virginia"},
        {"WA", "Washington", 0, 0, false, 5, 500, 4}, {"WV", "West Virginia", 0, 0, false, 6}, {"WI", "Wisconsin"},
        {"WY", "Wyoming"},
    };
    return states;
}

const StateRules *stateRulesFor(const std::string &code)
{
    for (const StateRules &s : stateRules())
        if (code == s.code)
            return &s;
    return nullptr;
}

const MinimumWage &minimumWageFor(const std::string &state)
{
    static const MinimumWage federal{"US", 725, 213};
    static const std::vector<MinimumWage> wages = {
        {"AL", 725, 213}, {"AK", 1400, 1400}, {"AZ", 1515, 1215}, {"AR", 1100, 263}, {"CA", 1690, 1690},
        {"CO", 1516, 1214}, {"CT", 1694, 638}, {"DE", 1500, 223}, {"DC", 1840, 1030}, {"FL", 1500, 1198},
        {"GA", 725, 213}, {"HI", 1600, 1475}, {"ID", 725, 335}, {"IL", 1500, 900}, {"IN", 725, 213},
        {"IA", 725, 435}, {"KS", 725, 213}, {"KY", 725, 213}, {"LA", 725, 213}, {"ME", 1510, 755},
        {"MD", 1500, 363}, {"MA", 1500, 675}, {"MI", 1373, 549}, {"MN", 1141, 1141}, {"MS", 725, 213},
        {"MO", 1500, 750}, {"MT", 1085, 1085}, {"NE", 1500, 213}, {"NV", 1200, 1200}, {"NH", 725, 327},
        {"NJ", 1592, 605}, {"NM", 1200, 300}, {"NY", 1600, 1065}, {"NC", 725, 213}, {"ND", 725, 486},
        {"OH", 1100, 550}, {"OK", 725, 213}, {"OR", 1455, 1455}, {"PA", 725, 283}, {"RI", 1600, 389},
        {"SC", 725, 213}, {"SD", 1185, 593}, {"TN", 725, 213}, {"TX", 725, 213}, {"UT", 725, 213},
        {"VT", 1442, 721}, {"VA", 1277, 213}, {"WA", 1713, 1713}, {"WV", 875, 262}, {"WI", 725, 233},
        {"WY", 725, 213},
    };
    for (const MinimumWage &w : wages)
        if (state == w.code)
            return w;
    return federal;
}

void applyStateRules(PosSettings &s, const std::string &code)
{
    const StateRules *r = stateRulesFor(code);
    if (!r)
        return;
    s.laborState = r->code;
    s.overtimeWeeklyHours = 40;   // federal, everywhere
    s.overtimeDailyHours = r->dailyOvertime;
    s.doubleTimeDailyHours = r->dailyDouble;
    s.seventhDayRule = r->seventhDay;
    s.mealBreakAfterHours = r->mealBreakAfter;
    s.giftCardCashBelowCents = r->giftCardCashBelowCents;
    s.restBreakEveryHours = r->restBreakEvery;
}

} // namespace vt::core
