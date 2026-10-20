#pragma once

#include "core/money.hh"
#include "core/tax.hh"

#include <map>
#include <string>
#include <vector>

namespace vt::core {

// What one of an item uses up (see Ingredient).
struct RecipeLine {
    std::string ingredientId;
    double quantity = 1;
    bool operator==(const RecipeLine &) const = default;
};

// Something that can be sold. Modifiers ("Medium Rare", "Onion Rings") are
// menu items too; they attach to the order line they modify.
// The major food allergens (US FALCPA + sesame), by id.
inline const std::vector<std::string> &allergenIds()
{
    static const std::vector<std::string> ids{"gluten", "dairy", "egg", "peanut", "treenut", "soy", "fish", "shellfish", "sesame"};
    return ids;
}

struct MenuItem {
    std::string id;
    std::string name;
    std::string family;      // reporting group: burgers, drinks, sides...
    Money price;
    TaxClass taxClass = TaxClass::Food;
    bool isModifier = false;
    std::string printer;     // kitchen routing target (used by printing, M4)
    // The kitchen station that makes it (a PosSettings::stations id: grill,
    // fryer...); empty = its printer's screen.
    std::string station;
    bool available = true;   // false = "86'd", cannot be ordered
    // Choices asked for when it is ordered (ModifierGroup ids, in order).
    std::vector<std::string> modifierGroups;
    // A different price during a meal period (meal period id -> price),
    // e.g. dinner portions, or a Happy Hour period.
    std::map<std::string, Money> periodPrices;
    // What one uses up; when the stock runs short it is sold out by itself
    // (autoSoldOut) until restocked.
    std::vector<RecipeLine> recipe;
    bool autoSoldOut = false;
    // 86'd during service (the order screen, the sold-out list): for today;
    // End of Day brings it back (PosSettings::soldOutBackNextDay). Marked
    // sold out in the menu editors: until changed there.
    bool soldOutToday = false;
    // On the kitchen screen and kitchen tickets: a shorter name, a highlight
    // color, or nothing at all.
    std::string kitchenName;
    std::string kitchenColor;
    bool kitchenHide = false;
    // On the self-order kiosk: a line about it and a photo (an image file on
    // the computer showing the kiosk); or not shown there at all.
    std::string description;
    std::string image;
    // Its number for ringing it in by number (a PLU: "104"); empty: none.
    std::string number;
    // The barcodes on its package (UPC, EAN...): scanning one rings it in.
    std::vector<std::string> barcodes;
    // How long it should take the kitchen (minutes); 0: what it usually
    // takes (learned, PosSettings::prepSeconds).
    int prepMinutes = 0;
    // Its button color in the self-filling menu ("#a86a12"); empty: the panel's.
    std::string buttonColor;
    // Its name's size there, in percent of the usual (0: its category's).
    int textSize = 0;
    // On the order screen: under a heading in its category ("Tacos"), and
    // what comes before it: nothing, a space ("space"), a new row ("row").
    std::string section;
    std::string breakBefore;
    bool kioskHide = false;
    // What's in it that guests may be allergic to (allergenIds()).
    std::vector<std::string> allergens;
    // Always in the menu screen's Favorites (with today's best sellers).
    bool favorite = false;
    // Prices by order type (0: the regular price), and who may not discount it.
    Money takeoutPrice;
    Money deliveryPrice;
    bool noDiscount = false;        // no discounts or comps
    bool noStaffDiscount = false;   // no staff meal discount (alcohol, say)
    // Priced by weight (the original's "Priced By Weight"): `price` is per
    // weightUnit, and ordering it asks for the weight.
    bool byWeight = false;
    std::string weightUnit = "lb";
    // The original's "Menu Item + Substitute": ordered on its own at `price`,
    // or with Sub in place of part of another item at substitutePrice
    // (a house salad instead of fries, + $3.00).
    bool substitute = false;
    Money substitutePrice;
    // The original's "Event Admission": tickets to an event with this many
    // seats (0 = not an event), on this date (epoch ms, 0 = none given).
    // ticketsSoldBefore: sold on earlier business days (End of Day adds them).
    int ticketCapacity = 0;
    std::int64_t eventAt = 0;
    int ticketsSoldBefore = 0;

    Money priceDuring(const std::string &mealPeriod) const
    {
        const auto it = periodPrices.find(mealPeriod);
        return it == periodPrices.end() ? price : it->second;
    }
    // Takeout and delivery have their own price when set (delivery falls
    // back on the takeout price); otherwise the meal period's.
    Money priceFor(const std::string &mealPeriod, bool takeout, bool delivery) const
    {
        if (delivery && deliveryPrice.cents() > 0)
            return deliveryPrice;
        if ((takeout || delivery) && takeoutPrice.cents() > 0)
            return takeoutPrice;
        return priceDuring(mealPeriod);
    }
    bool operator==(const MenuItem &) const = default;
};

// One choice in a modifier group: "Medium Rare", "Onion Rings +1.50".
struct ModifierOption {
    std::string name;
    Money price;
    std::string kitchenName;   // what the kitchen sees instead
    bool kitchenHide = false;  // "No dressing": nothing for the kitchen
    // The menu item it is (ModifierGroup::menuItems): it uses up the item's
    // stock and can't be chosen while it's sold out.
    std::string itemId;
    // It comes on the item (a burger's lettuce, onion, mayo): nothing to
    // choose, but it can be had No, Lite, Extra or on the Side.
    bool included = false;
    // Its price on an item of this size (by the size's name, lower case:
    // "large" -> 1.50); a size not here: `price`.
    std::map<std::string, Money> sizePrices;

    bool operator==(const ModifierOption &) const = default;
};

// A category of the menu (Burgers, Drinks): its button on the menu screens,
// in this order, and what new items in it start with. Items name it by id
// (MenuItem::family).
struct MenuCategory {
    std::string id;
    std::string name;
    std::string color;                  // "#rrggbb": its button and its items'; empty: the theme's
    std::vector<std::string> periods;   // meal period ids it's on; empty: all day
    // New items in it start with these.
    std::string printer;                // their kitchen ticket ("kitchen", "bar")
    std::string station;                // made at (kitchen screen)
    std::string taxClass;               // "food", "alcohol"...; empty: food
    // Its items' buttons on the order screen: their size ("small", "medium",
    // "large"; empty: as many as fit, as big as they can be), with photos,
    // without prices.
    std::string buttonSize;
    bool photos = false;
    bool hidePrice = false;
    // Its items' names' size, in percent of the usual (0: the usual).
    int textSize = 0;
    // Its items without their own color: shades of its color, each a little
    // different.
    bool shades = false;

    bool operator==(const MenuCategory &) const = default;
};

// Choices asked for when an item is ordered: "Temperature" (exactly one),
// "Toppings" (up to three)... `min` choices are required; `max` is the most
// allowed (0 = any number; 1 = choosing another replaces the choice).
struct ModifierGroup {
    std::string id;
    std::string name;
    int min = 0;
    int max = 1;
    std::vector<ModifierOption> options;
    // Each option is the menu item of that name. That's how a combo works:
    // "Burger Combo" asks for a side and a drink from the menu.
    bool menuItems = false;
    // Guests (the kiosk) can have a choice Lite, Extra or on the Side
    // (dressing on the side, extra bacon); always for what comes on it.
    bool askHow = false;

    bool operator==(const ModifierGroup &) const = default;
};

// No / Lite / Extra ... applied to the next item or modifier ordered.
// Sub: the next item replaces part of the one before it, at its
// substitute price (MenuItem::substitute).
enum class Qualifier { None, No, Lite, Extra, Side, Only, Double, Sub };

std::string toString(Qualifier q);
Qualifier qualifierFromString(const std::string &s);   // unknown -> None
// Display prefix, e.g. "No " for No. Empty for None.
std::string qualifierPrefix(Qualifier q);
// Price after the qualifier: No/Lite are free, Double charges twice.
Money qualifiedPrice(Money unit, Qualifier q);

} // namespace vt::core
