#pragma once

#include "core/menu.hh"
#include "core/money.hh"
#include "core/tax.hh"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace vt::core {

struct Modifier {
    std::string itemId;
    std::string name;
    Money unitPrice;
    Qualifier qualifier = Qualifier::None;
    std::string group;   // the ModifierGroup it was chosen from, if any
    std::string kitchenName;   // what the kitchen sees instead of the name
    bool kitchenHide = false;  // not shown in the kitchen at all
    // A part made at another station (a combo's fries at the fryer): that
    // station's screen shows it, and bumps it, on its own.
    std::string station;
    bool made = false;
    std::int64_t madeAt = 0;

    Money price() const { return qualifiedPrice(unitPrice, qualifier); }
    std::string displayName() const { return qualifierPrefix(qualifier) + name; }
    std::string kitchenText() const { return qualifierPrefix(qualifier) + (kitchenName.empty() ? name : kitchenName); }
    bool operator==(const Modifier &) const = default;
};

// One line on a check: an item (with its modifiers) or a free-text comment.
struct OrderLine {
    std::int64_t id = 0;
    std::string itemId;          // empty for comments
    std::string name;
    Money unitPrice;
    int quantity = 1;
    TaxClass taxClass = TaxClass::Food;
    Qualifier qualifier = Qualifier::None;
    std::vector<Modifier> modifiers;
    std::string printer;
    std::string station;         // the station that makes it (empty: its printer's screen)
    bool sent = false;           // sent to the kitchen; can no longer just be deleted
    bool voided = false;         // voided after sending (kept for the record)
    std::int64_t sentAt = 0;
    bool made = false;           // bumped on the kitchen display (legacy ORDER_MADE)
    std::int64_t madeAt = 0;
    int seat = 0;                // 0: not for a seat in particular
    int course = 1;              // later courses wait until they are fired
    // How the kitchen sees it (copied from the menu item when ordered).
    std::string kitchenName;     // instead of the name ("BCN BGR")
    std::string kitchenColor;    // highlight on the kitchen screen: red, orange...
    std::vector<std::string> allergens;   // its item's, when ordered
    bool kitchenHide = false;    // nothing for the kitchen (water, merchandise)
    bool noDiscount = false;      // from the menu item: discounts leave it out
    bool noStaffDiscount = false; // ...and staff meals
    // Sold by weight: the weight in thousandths of weightUnit (1250 = 1.25 lb);
    // unitPrice is per unit. 0 = not by weight.
    std::int64_t weight = 0;
    std::string weightUnit;
    bool served = false;         // the expediter sent it out (after it was made)
    std::int64_t servedAt = 0;
    // Shared between checks (a bottle of wine for three): piece `share` of
    // `shareOf`, each an even part of the whole; the first pieces take any
    // leftover cents. shareGroup ties the pieces together (to put them back).
    int share = 0;
    int shareOf = 0;
    std::int64_t shareGroup = 0;

    bool isComment() const { return itemId.empty(); }
    std::string printerOf() const { return printer.empty() ? std::string("kitchen") : printer; }
    std::string kitchenText() const
    {
        return qualifierPrefix(qualifier) + (kitchenName.empty() ? name : kitchenName) + weightText();
    }
    // Something the kitchen sees (not a gift card or a hidden item).
    // (The first piece of a shared item stands for it.)
    bool forKitchen() const { return !kitchenHide && !itemId.starts_with("giftcard:") && share <= 1; }
    bool isShare() const { return shareOf > 1; }
    void unshare() { share = shareOf = 0; shareGroup = 0; }
    // How many to count in item counts and stock: the first piece of a
    // shared item counts the whole, the others none.
    int counted() const { return share > 1 ? 0 : quantity; }
    // Selling or reloading gift card <number>: no kitchen, no tax.
    bool isGiftCard() const { return itemId.starts_with("giftcard:"); }
    // A charge, not food: the delivery fee ("fee:delivery").
    bool isFee() const { return itemId.starts_with("fee:"); }
    std::string giftCardNumber() const { return isGiftCard() ? itemId.substr(9) : std::string(); }
    std::string displayName() const { return qualifierPrefix(qualifier) + name + weightText() + shareText(); }
    // " 1.25 lb" for an item sold by weight, else empty.
    std::string weightText() const;
    // " (1/3)" for a piece of a shared item, else empty.
    std::string shareText() const { return isShare() ? " (" + std::to_string(share) + "/" + std::to_string(shareOf) + ")" : std::string(); }
    // (item + modifiers) x quantity; zero once voided. By weight: the
    // price per unit times the weight, rounded to the cent. A piece of a
    // shared item: its part of that.
    Money total() const;
    // The whole item, before it was shared.
    Money wholeTotal() const;
    bool operator==(const OrderLine &) const = default;
};

// GiftCard: from a gift card's balance; HouseAccount: charged to a
// customer's account (both name which one in Payment::reference).
enum class TenderKind { Cash, Card, Discount, GiftCard, HouseAccount };

// A way to pay or reduce a check, configured in settings.
struct Tender {
    std::string id;
    std::string name;
    TenderKind kind = TenderKind::Cash;
    std::int64_t percentBp = 0;   // Discount: share of the items total (10000 = comp)
    bool staffMeal = false;       // Discount: a staff meal (records who ate; items marked "no staff discount" pay full)

    bool operator==(const Tender &) const = default;
};

struct Payment {
    std::int64_t id = 0;
    std::string tenderId;
    std::string tenderName;
    TenderKind kind = TenderKind::Cash;
    Money amount;                 // Cash/Card: what it pays toward the check; Discount: a fixed amount off
    std::int64_t percentBp = 0;   // Discount, applied to the current items total
    Money tip;                    // Card: tip on top of the amount (owed to the server)
    std::string reference;        // GiftCard: the card number; HouseAccount: the customer id; staff meal: who ate;
                                  // a card through a reader: the processor's payment id (Stripe's pi_...)
    bool staffMeal = false;
    // A card taken on a reader: whose ("stripe", "simulated"), and which card.
    std::string processor;
    std::string cardBrand;        // "visa", "mastercard"...
    std::string last4;
    // Taken while the internet was down (on a Stripe reader): "waiting" to
    // reach Stripe (reference: "offline:<id>"), "declined" by the bank once
    // it did, "seen" when a manager has dealt with that. Empty: online.
    std::string offline;

    bool operator==(const Payment &) const = default;
};

// Money given back on a paid check (after it closed): through Stripe for a
// card from a reader, out of the drawer for cash, or recorded only (a card
// refunded on another terminal).
struct Refund {
    std::int64_t id = 0;
    std::int64_t at = 0;
    std::int64_t paymentId = 0;   // which payment it gives back
    Money amount;
    std::string reason;
    std::string by;               // the manager who did it
    std::string reference;        // Stripe's refund id (re_...)
    std::string tenderName;
    std::string method;           // "stripe", "cash" (out of a drawer), "recorded" (given back elsewhere)
    std::int64_t day = 0;         // the business day it happened on (its reports)
    std::int64_t checkId = 0;     // the check it was on, and its name then
    std::string checkLabel;

    bool operator==(const Refund &) const = default;
};

// Tab: a bar tab, opened under the guest's name and kept open all night.
enum class CheckType { DineIn, Takeout, Quick, Delivery, Tab };

// Who a takeout / delivery order is for.
struct Customer {
    std::string name;
    std::string phone;
    std::string address;
    std::string note;

    bool empty() const { return name.empty() && phone.empty() && address.empty() && note.empty(); }
    bool operator==(const Customer &) const = default;
};
// Discarded: put away with nothing on it; Merged: moved into another check.
// Both are kept for the serial numbers and left out of sales.
enum class CheckStatus { Open, Closed, Discarded, Merged };

// Something done to a check, for its history and the audit report:
// transferred, moved, merged, reopened, voided, discounted...
struct CheckEvent {
    std::int64_t at = 0;
    std::string who;
    std::string what;
    std::string kind;   // void | discount | reopen | transfer | move | merge | unpay | undiscount
    Money amount;       // what it was worth: the item voided, the discount, the payment taken back
    std::string reason; // why (a void's or comp's reason, from the store's list)

    bool operator==(const CheckEvent &) const = default;
};

std::string toString(CheckType t);
CheckType checkTypeFromString(const std::string &s);
std::string toString(CheckStatus s);
CheckStatus checkStatusFromString(const std::string &s);
std::string toString(TenderKind k);
TenderKind tenderKindFromString(const std::string &s);

struct Totals {
    Money items;         // all non-voided lines (gift cards sold too)
    Money giftCardsSold; // the part of `items` that is gift cards sold or reloaded: owed to the guest, not a sale
    Money discounts;     // positive amount taken off
    std::map<std::int64_t, Money> discountOf;   // each discount's own amount, by payment id
    Money subtotal;      // items - discounts
    Money tax;
    std::map<TaxClass, Money> taxByClass;
    std::map<TaxClass, Money> taxableByClass;   // what each class's tax is on (after its share of discounts)
    Money netSales() const { return subtotal - giftCardsSold; }   // sales: without gift cards sold
    Money gratuity;      // service charge on the subtotal (owed to the server)
    Money taxableGratuity;   // the gratuity, when it's taxed (TaxRates::taxGratuity; its tax is in `tax`)
    Money total;         // subtotal + tax + gratuity
    Money paid;          // cash + card toward the total (tips not included)
    Money tips;          // card tips, on top of the total
    Money cashPaid;      // cash tendered
    Money staffMeals;    // the part of `discounts` that is staff meals
    Money rounding;      // cash rounding (TaxRates::cashRoundingCents): -2 to +2 cents...
    Money balance;       // total + rounding - paid (negative = change owed)
    Money change;        // max(0, -balance), always given in cash
    Money cashNet() const { return cashPaid - change; }   // what stays in the drawer

    bool operator==(const Totals &) const = default;
};

// A bar tab's card, held when the tab opens: the bank sets the amount
// aside on it (an authorization), nothing is charged. When the tab closes
// it's charged for the tab and the tip, or let go if paid another way.
struct CardHold {
    std::string reference;   // the processor's payment (Stripe's pi_...); empty: none held
    std::string processor;   // "stripe", "simulated"
    std::string cardBrand;
    std::string last4;
    Money amount;            // set aside so far
    std::int64_t at = 0;

    bool empty() const { return reference.empty(); }
    bool operator==(const CardHold &) const = default;
};

struct Check {
    std::int64_t id = 0;              // serial number
    CheckType type = CheckType::DineIn;
    CheckStatus status = CheckStatus::Open;
    std::string label;                // table "T4", "Takeout 12", ...
    int guests = 1;
    std::string serverId;
    std::string serverName;
    std::int64_t openedAt = 0;        // epoch ms
    std::int64_t closedAt = 0;
    std::vector<OrderLine> lines;
    std::vector<Payment> payments;
    std::int64_t nextLineId = 1;
    std::int64_t nextPaymentId = 1;
    std::int64_t businessDay = 0;     // day the check was closed in
    std::int64_t drawerSession = 0;   // drawer that took its cash
    Customer customer;
    std::string customerId;           // a saved customer (their visits, house account)
    // An emailed receipt asked for: Stripe sends it once every card payment on
    // the check has reached Stripe (a card taken offline, the internet down).
    std::string receiptEmail;
    bool receiptEmailSent = false;
    // A cash tip handed to whoever took the payment (a host, a cashier) for the
    // check's server: in the taker's cash (drawerSession), owed to the server.
    Money cashTip;
    std::int64_t gratuityBp = 0;      // e.g. 1800 = 18% of the subtotal
    // Sold tax-free to a buyer with an exemption certificate (a nonprofit,
    // a school, the government): its number and who, kept for the state.
    std::string taxExempt;
    bool autoGratuity = false;        // added for a large party (not by hand)
    std::vector<CheckEvent> events;   // oldest first
    int firedCourse = 1;              // courses up to this one go out on Send
    int pointsEarned = 0;             // loyalty points it gave its customer (taken back on reopen)
    bool training = false;            // a practice check: not a sale, never to the kitchen
    bool rush = false;                // the kitchen does it first
    bool vip = false;                 // the kitchen takes extra care
    std::vector<std::string> allergies;   // the guest's (allergen ids): the kitchen sees them
    std::vector<std::string> coupons;     // promotions whose code was entered on it (their ids)
    CardHold hold;                        // a tab's card, held
    bool kiosk = false;               // a guest ordered it on the self-order kiosk
    // An order for later: ready at this time (epoch ms; 0 = now). It goes to
    // the kitchen by itself shortly before (PosSettings::laterLeadMinutes).
    std::int64_t dueAt = 0;
    // Course pacing: the next held course fires by itself at this time (0: when someone fires it).
    std::int64_t fireAt = 0;
    // Money given back after it was paid (newest last).
    std::vector<Refund> refunds;
    Money refunded(std::int64_t paymentId) const
    {
        Money sum;
        for (const Refund &r : refunds)
            if (r.paymentId == paymentId)
                sum += r.amount;
        return sum;
    }
    // Phone orders: the time the guest was told at the first Send (0: none).
    std::int64_t promisedAt = 0;
    // Deliveries: who took it out, when, and when they were back.
    std::string driverId;
    std::string driverName;
    std::int64_t outAt = 0;
    std::int64_t deliveredAt = 0;

    void note(std::int64_t at, const std::string &who, const std::string &what, const std::string &kind = {},
              Money amount = {}, const std::string &reason = {})
    {
        events.push_back({at, who, what, kind, amount, reason});
    }

    OrderLine *line(std::int64_t lineId);
    const OrderLine *line(std::int64_t lineId) const;
    // Last line that can take modifiers (an unsent, non-comment item).
    OrderLine *lastItemLine();

    OrderLine &addItem(const MenuItem &item, Qualifier q = Qualifier::None);
    // Attach a modifier to a line; false if the line is missing, sent, or a comment.
    bool addModifier(std::int64_t lineId, const MenuItem &modifier, Qualifier q = Qualifier::None);
    OrderLine &addComment(const std::string &text);
    // Unsent lines are removed; sent lines must be voided instead.
    bool removeLine(std::int64_t lineId);
    bool voidLine(std::int64_t lineId);
    // A line waiting for its course to be fired.
    bool held(const OrderLine &l) const { return !l.sent && !l.voided && l.course > firedCourse; }
    // Unsent lines that would go out now: those of fired courses, or all.
    std::vector<OrderLine> sendable(bool everything = false) const;
    // Marks those lines sent (one ticket: a sentAt later than any earlier
    // send on this check); returns how many. `everything` fires all courses.
    int sendAll(std::int64_t now, bool everything = false);
    int unsentCount() const;
    int heldCount() const;
    // Fire the next course that has lines waiting: returns it (0: none).
    int fireNextCourse();

    // Split checks: remove a line (with its modifiers) / add one under a new id.
    std::optional<OrderLine> takeLine(std::int64_t lineId);
    OrderLine &adoptLine(OrderLine line);
    // Take one of a line's quantity off as a line of its own (the whole line
    // when there's only one).
    std::optional<OrderLine> takeOne(std::int64_t lineId);
    // Cut a line (quantity 1, not already shared) into `parts` even pieces,
    // all on this check; returns their ids, or empty when it can't be.
    // `group` ties them together, `first` is the piece that keeps line's place.
    std::vector<std::int64_t> shareLine(std::int64_t lineId, int parts, std::int64_t group);

    // Merge: everything on `other` (items, payments, guests, customer)
    // comes onto this check; `other` is left empty.
    void absorb(Check &other);

    Payment &addPayment(const Tender &tender, Money amount);
    bool removePayment(std::int64_t paymentId);

    Totals totals(const TaxRates &rates) const;
    // What a payment actually left with the store, tip included: cash less
    // the change given back (from the last cash payment first).
    Money kept(const Payment &p, const TaxRates &rates) const;
    // Change given back on this cash payment.
    Money changeFrom(const Payment &p, const TaxRates &rates) const;
    // The most a refund can give back on a payment: what it paid on the
    // bill (cash less change). Tips aren't refunded: they're the staff's.
    Money refundable(const Payment &p, const TaxRates &rates) const { return p.amount - changeFrom(p, rates); }

    bool operator==(const Check &) const = default;
};

} // namespace vt::core
