#pragma once

#include "core/money.hh"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace vt::core {

// Something the kitchen uses up: buns, patties, oz of coffee... Stock goes
// down as recipes are sent to the kitchen and back up on a void; a manager
// counts or receives it in Manager -> Inventory.
struct Ingredient {
    std::string id;
    std::string name;
    std::string unit = "each";   // each, oz, lb, slice...
    double onHand = 0;
    double lowAt = 0;            // low-stock warning at or below this
    Money cost;                  // per unit, for food cost
    std::string vendor;          // who it usually comes from (a Vendor id)
    // How long it keeps once prepped (hours), for its prep label's use-by;
    // 0: asked each time.
    int shelfLifeHours = 0;
    // Kept on hand: below it, the Stock report's order list asks for more
    // (up to this). 0: not ordered by par.
    double par = 0;
    // Since it was last counted (a stock count): what there was then, and
    // what came in, went out in what was sold or thrown away (the waste
    // log), or was changed by hand. What should be there is
    // onHand = start + received - sold - wasted + adjusted.
    std::int64_t countedAt = 0;  // 0: never counted
    double start = 0;
    double received = 0;
    double sold = 0;
    double wasted = 0;
    double adjusted = 0;

    bool low() const { return onHand <= lowAt; }
    // Amounts to the thousandth after each change: 0.1 oz taken off a
    // thousand times is 100 oz, not 99.99999999 (a count of exactly the
    // low mark not read as above it). Kept as numbers, as saved.
    static double thousandths(double v) { return std::round(v * 1000.0) / 1000.0; }
    void settle()
    {
        for (double *v : {&onHand, &start, &received, &sold, &wasted, &adjusted})
            *v = thousandths(*v);
    }
    bool operator==(const Ingredient &) const = default;
};

// Who the store buys from (Manager -> Vendors).
struct Vendor {
    std::string id;
    std::string name;
    std::string phone;
    std::string account;   // the store's account number with them
    std::string note;      // delivery days, the rep...
    bool operator==(const Vendor &) const = default;
};

// A delivery received (Manager -> Inventory -> Receive a Delivery): what came,
// at what cost. Receiving adds it to stock and updates each ingredient's cost.
struct Delivery {
    std::int64_t id = 0;
    std::int64_t at = 0;
    std::string vendorId;
    std::string vendorName;
    std::string invoice;   // their invoice number
    std::string by;
    struct Line {
        std::string ingredientId;
        std::string name;
        std::string unit;
        double qty = 0;
        Money unitCost;
        Money total() const { return Money::fromCents(std::llround(double(unitCost.cents()) * qty)); }
        bool operator==(const Line &) const = default;
    };
    std::vector<Line> lines;

    Money total() const
    {
        Money t;
        for (const Line &l : lines)
            t += l.total();
        return t;
    }
    bool operator==(const Delivery &) const = default;
};

// Counting what's on the shelves (Manager -> Inventory -> Count Stock): each
// item counted, against what should be there by the sales and deliveries
// since its last count. The difference is waste, portions, theft or mistakes.
struct StockCount {
    std::int64_t id = 0;
    std::int64_t at = 0;
    std::string by;
    struct Line {
        std::string ingredientId;
        std::string name;
        std::string unit;
        std::int64_t since = 0;   // its last count (0: never)
        double start = 0;
        double received = 0;
        double sold = 0;
        double adjusted = 0;
        double expected = 0;      // start + received - sold - wasted + adjusted
        double counted = 0;
        Money unitCost;
        double wasted = 0;        // thrown away, and logged (already out of `expected`)
        double off() const { return counted - expected; }   // < 0: missing, not explained
        Money value() const { return Money::fromCents(std::llround(double(unitCost.cents()) * off())); }
        Money wasteValue() const { return Money::fromCents(std::llround(double(unitCost.cents()) * wasted)); }
        bool operator==(const Line &) const = default;
    };
    std::vector<Line> lines;

    Money wasteValue() const
    {
        Money t;
        for (const Line &l : lines)
            t += l.wasteValue();
        return t;
    }
    Money value() const
    {
        Money t;
        for (const Line &l : lines)
            t += l.value();
        return t;
    }
    bool operator==(const StockCount &) const = default;
};

// Thrown away, and said so (Log Waste): a dropped burger (its recipe), two
// pounds of lettuce gone bad. It comes off what's on hand, and the Stock
// report tells it apart from what went missing without a word.
struct Waste {
    std::int64_t id = 0;
    std::int64_t at = 0;
    std::string by;
    std::string what;     // the item or ingredient, as named then
    double qty = 0;
    std::string unit;     // an ingredient's; empty for a menu item
    std::string reason;
    struct Line {
        std::string ingredientId;
        std::string name;
        std::string unit;
        double qty = 0;
        Money unitCost;
        Money value() const { return Money::fromCents(std::llround(double(unitCost.cents()) * qty)); }
        bool operator==(const Line &) const = default;
    };
    std::vector<Line> lines;

    Money value() const
    {
        Money t;
        for (const Line &l : lines)
            t += l.value();
        return t;
    }
    bool operator==(const Waste &) const = default;
};

} // namespace vt::core
