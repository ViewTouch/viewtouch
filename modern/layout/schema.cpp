#include "layout/schema.hh"

#include <QHash>
#include <QJsonObject>
#include <QList>

using namespace Qt::StringLiterals;

namespace vt::layout::schema {

namespace {

QJsonArray options(std::initializer_list<std::pair<const char *, const char *>> values)
{
    QJsonArray arr;
    for (const auto &[value, text] : values)
        arr.append(QJsonObject{{u"value"_s, QString::fromUtf8(value)}, {u"text"_s, QString::fromUtf8(text)}});
    return arr;
}

QJsonArray options(const QStringList &values)
{
    QJsonArray arr;
    for (const QString &v : values)
        arr.append(QJsonObject{{u"value"_s, v}, {u"text"_s, v}});
    return arr;
}

QJsonObject field(const QString &path, const QString &label, const QString &type, const QString &group)
{
    return {{u"path"_s, path}, {u"label"_s, label}, {u"type"_s, type}, {u"group"_s, group}};
}

QJsonObject with(QJsonObject f, const QString &key, const QJsonValue &v)
{
    f.insert(key, v);
    return f;
}

QJsonObject intField(const QString &path, const QString &label, const QString &group, int min, int max)
{
    QJsonObject f = field(path, label, u"int"_s, group);
    f.insert(u"min"_s, min);
    f.insert(u"max"_s, max);
    return f;
}

const QJsonArray kShapes = options({{"rect", "Rectangle"}, {"rounded", "Rounded"}, {"circle", "Circle"},
                                    {"diamond", "Diamond"}, {"hexagon", "Hexagon"}, {"octagon", "Octagon"}});
const QJsonArray kBehaviors = options({{"blink", "Blink on touch"}, {"toggle", "Toggle on/off"},
                                       {"select", "Stay lit (one per page)"}, {"double", "Double touch"},
                                       {"passthrough", "Ignore touches"}, {"none", "No feedback"}});
const QJsonArray kFrames = options({{"raised", "Raised"}, {"inset", "Inset"}, {"border", "Border"},
                                    {"flat", "Flat"}, {"none", "None"}});
const QJsonArray kTextStyles = options({{"none", "Plain"}, {"embossed", "Embossed"}, {"outline", "Outline"}});
const QJsonArray kBold = QJsonArray{QJsonObject{{u"value"_s, true}, {u"text"_s, u"Bold"_s}},
                                    QJsonObject{{u"value"_s, false}, {u"text"_s, u"Regular"_s}}};
const QJsonArray kMealPeriods = options({{"", "(none)"}, {"breakfast", "Breakfast"}, {"lunch", "Lunch"},
                                         {"dinner", "Dinner"}, {"all", "All day"}});
const QJsonArray kJumpModes = options({{"push", "Go (remember this page)"}, {"replace", "Go (don't remember)"},
                                       {"back", "Back"}, {"home", "Home"}, {"index", "Menu index"},
                                       {"sequence", "Continue modifier sequence"}});
const QJsonArray kQualifiers = options({{"no", "No"}, {"extra", "Extra"}, {"lite", "Lite"}, {"side", "Side"},
                                        {"only", "Only"}, {"double", "Double"}});

// Style fields for one state. Normal gets the full set; selected and
// disabled only the keys that usually differ.
void appendStyleFields(QJsonArray &out, const QString &prefix, const QString &state, const QString &group)
{
    const QString p = prefix + state + u'.';
    auto inh = [](QJsonObject f) { return with(std::move(f), u"inheritable"_s, true); };

    out.append(inh(field(p + u"fill"_s, u"Color"_s, u"color"_s, group)));
    out.append(inh(field(p + u"texture"_s, u"Texture"_s, u"texture"_s, group)));
    out.append(inh(field(p + u"textColor"_s, u"Text color"_s, u"color"_s, group)));
    if (state == u"normal") {
        out.append(inh(field(p + u"font"_s, u"Font"_s, u"font"_s, group)));
        out.append(inh(intField(p + u"fontSize"_s, u"Text size"_s, group, 8, 200)));
        out.append(inh(with(field(p + u"bold"_s, u"Weight"_s, u"enum"_s, group), u"options"_s, kBold)));
        out.append(inh(with(field(p + u"textStyle"_s, u"Text effect"_s, u"enum"_s, group), u"options"_s, kTextStyles)));
    }
    out.append(inh(with(field(p + u"frame"_s, u"Frame"_s, u"enum"_s, group), u"options"_s, kFrames)));
    if (state == u"normal") {
        out.append(inh(intField(p + u"frameWidth"_s, u"Frame width"_s, group, 0, 32)));
        out.append(inh(field(p + u"borderColor"_s, u"Border color"_s, u"color"_s, group)));
        out.append(inh(intField(p + u"radius"_s, u"Corner radius"_s, group, 0, 200)));
    }
    out.append(inh(intField(p + u"shadow"_s, u"Shadow"_s, group, 0, 40)));
    out.append(inh(with(with(intField(p + u"opacity"_s, u"Opacity %"_s, group, 0, 100), u"scale"_s, 100),
                        u"default"_s, 1)));
}

void appendStyleGroups(QJsonArray &out, const QString &prefix, bool allStates)
{
    appendStyleFields(out, prefix, u"normal"_s, u"Look"_s);
    if (allStates) {
        appendStyleFields(out, prefix, u"selected"_s, u"Look when lit"_s);
        appendStyleFields(out, prefix, u"disabled"_s, u"Look when disabled"_s);
    }
}

// Settings of each widget (its props), in the inspector's "Settings" section.
QJsonArray widgetSettings(const QString &kind)
{
    const QString g = u"Settings"_s;
    QJsonArray out;
    auto hint = [](QJsonObject f, const QString &h) { return with(std::move(f), u"hint"_s, h); };
    if (kind == u"kitchenDisplay") {
        out.append(hint(with(field(u"props.mode"_s, u"Shows"_s, u"enum"_s, g), u"options"_s,
                             options({{"", "A station's orders"}, {"expo", "The expediter (every station)"}})),
                        u"The expediter sees what each station has made and sends orders out."_s));
        out.append(hint(field(u"props.station"_s, u"Station"_s, u"string"_s, g),
                        u"A printer (kitchen, bar) or a kitchen station id (grill, fryer). Empty: everything. "
                        u"The screen's Station button can pick another."_s));
        out.append(hint(field(u"props.lockStation"_s, u"Keep this station"_s, u"bool"_s, g),
                        u"For panels side by side: the screen's Station button doesn't change this one."_s));
    } else if (kind == u"menuCategories") {
        out.append(hint(field(u"props.period"_s, u"Meal period"_s, u"string"_s, g),
                        u"Its categories (breakfast, lunch...: Menu Builder). Empty: what's on now."_s));
        out.append(intField(u"props.columns"_s, u"Columns"_s, g, 1, 8));
    } else if (kind == u"menuGrid") {
        out.append(hint(field(u"props.family"_s, u"Family"_s, u"string"_s, g),
                        u"The menu family it shows (burgers, drinks...). Empty: every family, with a button for each."_s));
        out.append(intField(u"props.columns"_s, u"Columns"_s, g, 1, 10));
        out.append(hint(field(u"props.photos"_s, u"Show photos"_s, u"bool"_s, g), u"The items' photos (Manager -> Menu)"_s));
        out.append(hint(field(u"props.popular"_s, u"Today's best sellers"_s, u"bool"_s, g),
                        u"The items sold most today, most first (the Popular page)"_s));
        out.append(hint(field(u"props.search"_s, u"Find by name"_s, u"bool"_s, g),
                        u"Shows the items whose name has what's typed on a keyboard panel (the Find page)"_s));
    } else if (kind == u"checkList") {
        out.append(with(field(u"props.mode"_s, u"Lists"_s, u"enum"_s, g), u"options"_s,
                        options({{"", "Open checks (touch to open)"}, {"tabs", "Bar tabs"},
                                 {"merge", "Checks to merge into this one"}, {"closed", "Today's closed checks (reopen)"}})));
    } else if (kind == u"tableGrid") {
        out.append(intField(u"props.columns"_s, u"Columns"_s, g, 1, 12));
        out.append(with(field(u"props.action"_s, u"Touching a table"_s, u"enum"_s, g), u"options"_s,
                        options({{"", "Opens it"}, {"move", "Moves this check there"}})));
    } else if (kind == u"floorPlan") {
        out.append(with(field(u"props.plan"_s, u"Starts on"_s, u"page"_s, g), u"hint"_s,
                        u"The floor plan it shows first. Empty: the one last picked on this screen, else the floor's own."_s));
        out.append(with(with(field(u"props.tabs"_s, u"Tabs for the other plans"_s, u"bool"_s, g), u"hint"_s,
                             u"Main, Dining Area, Bar, Patio… across the top. Off: only this plan."_s),
                        u"default"_s, true));
        out.append(with(field(u"props.action"_s, u"Touching a table"_s, u"enum"_s, g), u"options"_s,
                        options({{"", "Opens it"}, {"move", "Moves this check there"}})));
    } else if (kind == u"numPad") {
        out.append(with(field(u"props.mode"_s, u"Enters"_s, u"enum"_s, g), u"options"_s,
                        options({{"number", "A number"}, {"amount", "Money (with a 00 key)"},
                                 {"weight", "A weight (125 = 1.25 lb)"}})));
    } else if (kind == u"keyboard") {
        out.append(hint(field(u"props.placeholder"_s, u"Hint text"_s, u"string"_s, g), u"Shown while nothing is typed"_s));
    } else if (kind == u"reportView") {
        out.append(with(field(u"props.report"_s, u"Opens on"_s, u"enum"_s, g), u"options"_s,
                        options({{"sales", "Sales"}, {"items", "Items"}, {"categories", "Categories"},
                                 {"hourly", "By hour"}, {"servers", "Servers"}, {"tips", "Tips"}, {"labor", "Labor"},
                                 {"drawer", "Drawer"}, {"expenses", "Expenses"}, {"purchases", "Purchases"}, {"stock", "Stock count"},
                                 {"audit", "Audit"}, {"exceptions", "Exceptions"}, {"changes", "Changes"}, {"deposit", "Deposit"}, {"customers", "Customers"}, {"royalty", "Royalty"}, {"accounting", "Accounting"}, {"accounts", "Gift cards"}, {"kitchen", "Kitchen"},
                                 {"foodcost", "Food cost"}, {"engineering", "Menu mix"}, {"forecast", "Forecast"}})));
    } else if (kind == u"adminPanel") {
        out.append(with(field(u"props.panel"_s, u"Edits"_s, u"enum"_s, g), u"options"_s,
                        options({{"menu", "Menu"}, {"employees", "Employees"}, {"tenders", "Payment types"},
                                 {"taxes", "Taxes"}, {"printers", "Printers"}, {"terminals", "Terminals"},
                                 {"store", "Store settings"}, {"mealPeriods", "Meal periods"},
                                 {"modifierGroups", "Modifier groups"}, {"inventory", "Inventory"},
                                 {"promotions", "Promotions"}, {"vendors", "Vendors"}})));
    }
    return out;
}

// The look of a widget's own buttons: inherited like any style (zone ->
// page -> theme), under keys of their own in the normal state.
void appendKeyStyleFields(QJsonArray &out)
{
    const QString g = u"Built-in button look"_s;
    const QString p = u"style.normal."_s;
    auto inh = [](QJsonObject f) { return with(std::move(f), u"inheritable"_s, true); };
    out.append(inh(field(p + u"keyFill"_s, u"Button color"_s, u"color"_s, g)));
    out.append(inh(field(p + u"keyTextColor"_s, u"Button text color"_s, u"color"_s, g)));
    out.append(inh(field(p + u"keyLitFill"_s, u"Pressed / chosen color"_s, u"color"_s, g)));
    out.append(inh(field(p + u"keyFont"_s, u"Button font"_s, u"font"_s, g)));
    out.append(inh(intField(p + u"keyRadius"_s, u"Button corner radius"_s, g, 0, 60)));
}

} // namespace

QList<BuiltIn> builtInButtons(const QString &kind)
{
    static const QHash<QString, QList<BuiltIn>> buttons = {
        {u"kitchenDisplay"_s, {{u"station"_s, u"Station…"_s, u"kitchenStation"_s, true},
                               {u"message"_s, u"Message…"_s, QString(), true},
                               {u"waste"_s, u"Waste…"_s, QString(), true},
                               {u"labels"_s, u"Labels…"_s, QString(), true},
                               {u"allDay"_s, u"All Day"_s, u"kitchenAllDay"_s, true},
                               {u"recall"_s, u"Recall"_s, u"recallTicket"_s, true}}},
        {u"orderList"_s, {{u"tableChecks"_s, u"The table's checks (1 2 3 +)"_s, u"newTableCheck"_s},
                          {u"quantity"_s, u"On the touched line: − 2 + and Again"_s, u"repeatLine"_s},
                          {u"undo"_s, u"Removed …  Undo"_s, u"undoLast"_s},
                          {u"round"_s, u"Another Round"_s, u"anotherRound"_s},
                          {u"seat"_s, u"Seat − / +"_s, u"seatNext"_s},
                          {u"course"_s, u"Course 1 2 3"_s, u"courseNext"_s},
                          {u"fire"_s, u"Fire Course"_s, u"fireCourse"_s}}},
        {u"paymentPanel"_s, {{u"tips"_s, u"Tip buttons"_s, u"addTip"_s},
                             {u"gratuity"_s, u"Gratuity"_s, u"gratuity"_s},
                             {u"cashTip"_s, u"Cash tip for the server"_s, u"setCashTip"_s}}},
        {u"modifierPicker"_s, {{u"cancel"_s, u"Cancel Item"_s, u"cancelChoosing"_s, true},
                               {u"done"_s, u"Done"_s, u"finishChoosing"_s, true}}},
        {u"guestCount"_s, {{u"fewer"_s, u"−"_s, u"guestsFewer"_s}, {u"more"_s, u"+"_s, u"guestsMore"_s}}},
        {u"drawerPanel"_s, {{u"drawer"_s, u"Start / Count Drawer"_s, u"countDrawer"_s, true},
                            {u"noSale"_s, u"No Sale"_s, u"noSale"_s, true},
                            {u"payOut"_s, u"Pay Out"_s, u"payout"_s, true},
                            {u"paidIn"_s, u"Paid In"_s, u"paidIn"_s, true}}},
        {u"endOfDay"_s, {{u"backup"_s, u"Back Up Now"_s, u"backupNow"_s}}},
    };
    return buttons.value(kind);
}

QStringList basicKinds()
{
    return {u"button"_s, u"label"_s, u"image"_s, u"comment"_s};
}

QStringList widgetKinds()
{
    return {u"table"_s, u"tableGrid"_s, u"staffPicker"_s, u"checkHistory"_s, u"modifierPicker"_s,
            u"soldOutList"_s, u"orderList"_s, u"loginPad"_s, u"guestCount"_s, u"checkList"_s,
            u"paymentPanel"_s, u"numPad"_s, u"keyboard"_s, u"splitCheck"_s, u"drawerPanel"_s,
            u"reportView"_s, u"endOfDay"_s, u"logoutPanel"_s, u"clock"_s, u"statusBar"_s,
            u"adminPanel"_s, u"kitchenDisplay"_s, u"customerInfo"_s, u"customerLookup"_s, u"giftCard"_s, u"waitlist"_s, u"schedule"_s, u"factoryReset"_s, u"messageComposer"_s, u"network"_s, u"receiveDelivery"_s, u"stockCount"_s, u"wasteLog"_s, u"storeCheck"_s, u"prepLabels"_s, u"checkSearch"_s, u"orderLater"_s, u"menuGrid"_s, u"menuCategories"_s, u"menuBuilder"_s, u"timeClock"_s, u"dashboard"_s, u"checklist"_s, u"hostStand"_s, u"deliveryBoard"_s, u"decor"_s, u"floorPlan"_s};
}

QStringList allKinds()
{
    return basicKinds() + widgetKinds();
}

bool isWidgetKind(const QString &kind)
{
    return widgetKinds().contains(kind);
}

QStringList pageKinds()
{
    return {u"custom"_s, u"login"_s, u"tables"_s, u"guestCount"_s, u"index"_s, u"items"_s,
            u"modifier"_s, u"settle"_s, u"logout"_s, u"manager"_s, u"kitchen"_s, u"template"_s,
            u"library"_s};
}

QStringList pageRoles()
{
    return {u"login"_s, u"tables"_s, u"guestCount"_s, u"checkList"_s, u"settle"_s, u"logout"_s,
            u"manager"_s, u"bar"_s, u"kitchen"_s, u"weigh"_s, u"timeClock"_s, u"menu"_s};
}

// What page kinds and roles are called on screen.
QJsonArray pageKindOptions()
{
    return options({{"custom", "Custom"}, {"login", "Login"}, {"tables", "Tables"}, {"guestCount", "Guest count"},
                    {"index", "Menu index"}, {"items", "Menu items"}, {"modifier", "Choices"}, {"settle", "Payment"},
                    {"logout", "Log out"}, {"manager", "Manager"}, {"kitchen", "Kitchen"}, {"template", "Template"},
                    {"library", "Button library"}});
}

QJsonArray pageRoleOptions()
{
    return options({{"login", "Login"}, {"tables", "Tables"}, {"guestCount", "Guest count"}, {"checkList", "Open checks"},
                    {"settle", "Payment"}, {"logout", "Log out"}, {"manager", "Manager"}, {"bar", "Bar"},
                    {"kitchen", "Kitchen"}, {"weigh", "Weighing"}, {"timeClock", "Time clock"}, {"menu", "Menu"}});
}

QJsonArray zoneFields(const QString &kind)
{
    const bool touchable = kind == u"button" || kind == u"image";
    const QString general = u"General"_s;
    QJsonArray out;

    if (kind == u"table") {
        out.append(with(field(u"label"_s, u"Table name"_s, u"string"_s, general), u"hint"_s,
                        u"Shown on the table, checks and kitchen tickets. The same name on another floor plan is "
                        u"the same table (Main and Dining Area both show T1); on one page each table needs its own."_s));
        out.append(with(intField(u"props.seats"_s, u"Seats"_s, general, 0, 99), u"hint"_s,
                        u"Shown while the table is free. 0 hides it."_s));
        out.append(with(field(u"props.chairs"_s, u"Show chairs"_s, u"bool"_s, general), u"hint"_s,
                        u"A chair around the table for each seat, like a floor plan."_s));
    } else if (kind == u"decor") {
        out.append(with(with(field(u"props.decor"_s, u"Piece"_s, u"enum"_s, general), u"options"_s,
                             options({{"wall", "Wall"}, {"window", "Window"}, {"door", "Door"}, {"counter", "Bar counter"},
                                      {"booth", "Booth bench"}, {"plant", "Plant"}, {"pillar", "Pillar"},
                                      {"fence", "Fence or rail"}, {"entrance", "Entrance"}, {"area", "Area (a room's floor)"}})),
                        u"hint"_s, u"Drawn on the floor plan; touching it does nothing."_s));
        out.append(with(field(u"label"_s, u"Words on it"_s, u"string"_s, general), u"hint"_s,
                        u"An area's name (Patio), Entrance, Restrooms… Empty: none."_s));
        out.append(with(field(u"props.color"_s, u"Color"_s, u"color"_s, general), u"hint"_s,
                        u"Empty: the usual for the piece (an area: faint)."_s));
    } else {
        out.append(with(field(u"label"_s, kind == u"comment" ? u"Note"_s : u"Text"_s, u"text"_s, general),
                        u"hint"_s, isWidgetKind(kind) ? u"Optional caption"_s
                                   : kind == u"comment" ? QString()
                                   : u"Live text: {check.total} {check.balance} {check.label} {check.guests} "
                                     u"{check.items} {check.due} {user.name} {store.name} {time} {date}…"_s));
    }
    out.append(with(field(u"name"_s, u"Name"_s, u"string"_s, general), u"hint"_s, u"For your reference only"_s));
    if (touchable) {
        out.append(with(field(u"shape"_s, u"Shape"_s, u"enum"_s, general), u"options"_s, kShapes));
        out.append(with(field(u"behavior"_s, u"Touch behavior"_s, u"enum"_s, general), u"options"_s, kBehaviors));
        out.append(with(field(u"group"_s, u"Group"_s, u"string"_s, general), u"hint"_s,
                        u"Zones sharing a group act together"_s));
        out.append(with(field(u"hotkey"_s, u"Keyboard key"_s, u"string"_s, general), u"hint"_s,
                        u"Single key that presses this button"_s));
        out.append(with(field(u"imagePath"_s, u"Picture"_s, u"image"_s, general), u"hint"_s,
                        u"One of the store's pictures, the store logo, or Add Picture… from this computer"_s));
        out.append(field(u"enabled"_s, u"Enabled"_s, u"bool"_s, general));
    } else if (kind != u"comment" && kind != u"label") {
        out.append(with(field(u"shape"_s, u"Shape"_s, u"enum"_s, general), u"options"_s, kShapes));
    }

    // Show/hide rules: the zone is on the page only while all of these hold.
    const QString when = u"Show only when"_s;
    out.append(with(with(field(u"showWhen.login"_s, u"Who"_s, u"enum"_s, when), u"options"_s,
                         options({{"", "Always"}, {"loggedIn", "Someone is logged in"},
                                  {"loggedOut", "Nobody is logged in"}, {"manager", "A manager is logged in"}})),
                    u"hint"_s, u"In edit mode every zone shows (dimmed when its rules hide it now)."_s));
    out.append(with(field(u"showWhen.check"_s, u"Check"_s, u"enum"_s, when), u"options"_s,
                    options({{"", "Either way"}, {"open", "A check is open"}, {"none", "No check is open"}})));
    out.append(with(field(u"showWhen.checkType"_s, u"Kind of check"_s, u"enum"_s, when), u"options"_s,
                    options({{"", "Any"}, {"dineIn", "Dine in"}, {"takeout", "Takeout"}, {"delivery", "Delivery"},
                             {"tab", "Bar tab"}, {"quick", "Quick order"}})));
    out.append(with(field(u"showWhen.mealPeriod"_s, u"Meal period"_s, u"enum"_s, when), u"options"_s,
                    options({{"", "Any time"}, {"breakfast", "Breakfast"}, {"lunch", "Lunch"}, {"dinner", "Dinner"}})));
    out.append(with(field(u"showWhen.screen"_s, u"Screen"_s, u"enum"_s, when), u"options"_s,
                    options({{"", "Any"}, {"standard", "Standard screens"}, {"phone", "Phones"}})));

    // How phones lay it out when its page has no phone version of its own.
    const QString phones = u"On phones"_s;
    out.append(with(field(u"phone.wide"_s, u"A row of its own"_s, u"bool"_s, phones), u"hint"_s,
                    u"Phones lay pages without a phone version out by themselves, in rows of big buttons: this one takes the whole width."_s));
    out.append(with(field(u"phone.place"_s, u"Where"_s, u"enum"_s, phones), u"options"_s,
                    options({{"", "Where it is on the page"}, {"first", "First"}, {"last", "Last"}})));

    const QString geo = u"Position and size"_s;
    out.append(intField(u"rect.x"_s, u"X"_s, geo, 0, 7680));
    out.append(intField(u"rect.y"_s, u"Y"_s, geo, 0, 4320));
    out.append(intField(u"rect.w"_s, u"Width"_s, geo, 16, 7680));
    out.append(intField(u"rect.h"_s, u"Height"_s, geo, 16, 4320));
    out.append(with(intField(u"z"_s, u"Layer"_s, geo, -999, 999), u"hint"_s, u"Higher is drawn on top"_s));
    out.append(with(with(intField(u"rotation"_s, u"Turned (degrees)"_s, geo, 0, 359), u"hint"_s,
                         u"Clockwise about its middle. The round handle above it turns it too (15° at a time)."_s),
                    u"step"_s, 15));

    if (touchable)
        out.append(field(u"actions"_s, u"When touched"_s, u"actions"_s, u"Actions"_s));

    if (isWidgetKind(kind)) {
        for (const QJsonValue &f : widgetSettings(kind))
            out.append(f);
        const QList<BuiltIn> keys = builtInButtons(kind);
        if (!keys.isEmpty()) {
            const QString g = u"Built-in buttons"_s;
            out.append(with(field(u"props.hideButtons"_s, u"Hide all its buttons"_s, u"bool"_s, g), u"hint"_s,
                            u"Put your own buttons anywhere instead: each has a command that does the same."_s));
            for (const BuiltIn &b : keys) {
                // Composed labels: their parts, to be translated apart.
                const auto parts = [&](QJsonObject f, const QString &tmpl) {
                    f.insert(u"labelTemplate"_s, tmpl);
                    f.insert(u"labelArg"_s, b.label);
                    return f;
                };
                QJsonObject hide = parts(field(u"props.buttons."_s + b.id + u".hide"_s, u"Hide “%1”"_s.arg(b.label), u"bool"_s, g),
                                         u"Hide “%1”"_s);
                if (!b.command.isEmpty()) {
                    hide.insert(u"hint"_s, u"Command: %1"_s.arg(b.command));
                    hide.insert(u"hintTemplate"_s, u"Command: %1"_s);
                    hide.insert(u"hintArg"_s, b.command);
                }
                out.append(hide);
                out.append(with(parts(field(u"props.buttons."_s + b.id + u".label"_s, u"“%1” says"_s.arg(b.label),
                                            u"string"_s, g), u"“%1” says"_s), u"hint"_s, u"Empty: the usual words"_s));
                if (b.orderable)
                    out.append(with(parts(intField(u"props.buttons."_s + b.id + u".order"_s, u"“%1” position"_s.arg(b.label), g, 0, 9),
                                          u"“%1” position"_s),
                                    u"hint"_s, u"1 = first in its row. 0 or empty: the usual place."_s));
            }
        }
        appendKeyStyleFields(out);
    }

    appendStyleGroups(out, u"style."_s, touchable);
    return out;
}

QJsonArray pageFields()
{
    const QString general = u"Page"_s;
    QJsonArray out;
    out.append(field(u"name"_s, u"Name"_s, u"string"_s, general));
    out.append(with(field(u"id"_s, u"Page ID"_s, u"string"_s, general), u"hint"_s,
                    u"Renaming updates every button that jumps here"_s));
    out.append(with(field(u"kind"_s, u"Page type"_s, u"enum"_s, general), u"options"_s, pageKindOptions()));
    QJsonArray roles = options({{"", "(none)"}});
    for (const QJsonValue &v : pageRoleOptions())
        roles.append(v);
    out.append(with(with(field(u"role"_s, u"System role"_s, u"enum"_s, general), u"options"_s, roles),
                    u"hint"_s, u"The app opens this page for that job"_s));
    out.append(with(field(u"templateId"_s, u"Template page"_s, u"page"_s, general), u"hint"_s,
                    u"Template zones show behind this page's zones"_s));
    out.append(with(field(u"mealPeriod"_s, u"Meal period"_s, u"enum"_s, general), u"options"_s, kMealPeriods));
    out.append(with(with(field(u"permission"_s, u"Who may open it"_s, u"enum"_s, general), u"options"_s,
                         options({{"", "Anyone logged in"}, {"public", "Anyone, even logged out (kitchen screens)"},
                                  {"check.settle", "Staff who take payments"},
                                  {"order.void", "Staff who void items"}, {"manager", "Managers"},
                                  {"layout.edit", "Page editors"}})),
                    u"hint"_s, u"Others get a message instead of the page"_s));
    out.append(intField(u"grid"_s, u"Snap grid"_s, general, 1, 128));
    out.append(intField(u"canvas.w"_s, u"Canvas width"_s, general, 320, 7680));
    out.append(intField(u"canvas.h"_s, u"Canvas height"_s, general, 240, 4320));

    const QString screens = u"Phones"_s;
    out.append(with(field(u"variantOf"_s, u"Phone version of"_s, u"page"_s, screens), u"hint"_s,
                    u"Phones show this page instead of that one (with For screens: Phones)"_s));
    out.append(with(with(field(u"formFactor"_s, u"For screens"_s, u"enum"_s, screens), u"options"_s,
                         options({{"", "All"}, {"phone", "Phones"}})),
                    u"hint"_s, u"Use a portrait canvas, e.g. 1080 × 2280"_s));
    out.append(with(intField(u"contentArea.x"_s, u"Content area X"_s, screens, 0, 7680), u"hint"_s,
                    u"Phone templates: where pages without a phone version get their buttons"_s));
    out.append(intField(u"contentArea.y"_s, u"Content area Y"_s, screens, 0, 7680));
    out.append(intField(u"contentArea.w"_s, u"Content area width"_s, screens, 0, 7680));
    out.append(intField(u"contentArea.h"_s, u"Content area height"_s, screens, 0, 7680));

    const QString bg = u"Background"_s;
    out.append(with(field(u"background.fill"_s, u"Color"_s, u"color"_s, bg), u"inheritable"_s, true));
    out.append(with(field(u"background.texture"_s, u"Texture"_s, u"texture"_s, bg), u"inheritable"_s, true));
    out.append(with(field(u"background.image"_s, u"Picture"_s, u"image"_s, bg), u"inheritable"_s, true));
    out.append(with(with(field(u"background.imageFit"_s, u"Picture fits"_s, u"enum"_s, bg), u"options"_s,
                         options({{"cover", "Fill the page (cropped)"}, {"fit", "Whole picture"},
                                  {"stretch", "Stretched"}, {"tile", "Tiled"}, {"center", "Centered, as is"}})),
                    u"inheritable"_s, true));

    QJsonArray defaults;
    appendStyleFields(defaults, u"style."_s, u"normal"_s, u"Default button look"_s);
    for (const QJsonValue &v : defaults)
        out.append(v);
    return out;
}

QJsonArray themeFields()
{
    QJsonArray out;
    out.append(field(u"name"_s, u"Theme name"_s, u"string"_s, u"Theme"_s));
    out.append(with(field(u"background.fill"_s, u"Background color"_s, u"color"_s, u"Theme"_s), u"inheritable"_s, true));
    out.append(with(field(u"background.texture"_s, u"Background texture"_s, u"texture"_s, u"Theme"_s), u"inheritable"_s, true));
    out.append(with(field(u"background.image"_s, u"Background picture"_s, u"image"_s, u"Theme"_s), u"inheritable"_s, true));
    out.append(with(with(field(u"background.imageFit"_s, u"Picture fits"_s, u"enum"_s, u"Theme"_s), u"options"_s,
                         options({{"cover", "Fill the page (cropped)"}, {"fit", "Whole picture"},
                                  {"stretch", "Stretched"}, {"tile", "Tiled"}, {"center", "Centered, as is"}})),
                    u"inheritable"_s, true));
    appendStyleGroups(out, u"style."_s, true);
    // Colors that mean something: table states, kitchen ticket ages, sold out.
    const QString g = u"Status colors"_s;
    const auto color = [&](const char *key, const char *label, const char *usual) {
        QJsonObject f = with(with(field(u"status."_s + QLatin1String(key), QString::fromLatin1(label), u"color"_s, g),
                                  u"inheritable"_s, true), u"hint"_s, u"Usually %1"_s.arg(QLatin1String(usual)));
        f.insert(u"hintTemplate"_s, u"Usually %1"_s);
        f.insert(u"hintArg"_s, QLatin1String(usual));
        out.append(f);
    };
    color("tableOpen", "Table with a check", "#a86a12");
    color("tableMine", "Table with my check", "#1f8a4c");
    color("tableCurrent", "Table being worked on", "#2f6fd6");
    color("tableLong", "Table seated a long time (border)", "#ff4d4d");
    color("kitchenNew", "Kitchen ticket: new", "#1f8a4c");
    color("kitchenWarn", "Kitchen ticket: getting old", "#b7791f");
    color("kitchenLate", "Kitchen ticket: late", "#c53030");
    color("kitchenReady", "Expediter: order ready", "#1f6fd6");
    color("soldOut", "Sold out badge", "#b83232");
    return out;
}

QJsonArray actionTypes()
{
    auto type = [](const QString &t, const QString &label, QJsonArray fields) {
        return QJsonObject{{u"type"_s, t}, {u"label"_s, label}, {u"fields"_s, fields}};
    };
    const QString g;   // no groups inside an action card
    QJsonArray roles = options({{"", "(use page)"}});
    for (const QJsonValue &v : pageRoleOptions())
        roles.append(v);

    return {
        type(u"jump"_s, u"Go to page"_s,
             {with(field(u"mode"_s, u"How"_s, u"enum"_s, g), u"options"_s, kJumpModes),
              field(u"page"_s, u"Page"_s, u"page"_s, g),
              with(with(field(u"role"_s, u"Or system page"_s, u"enum"_s, g), u"options"_s, roles),
                   u"hint"_s, u"Used when no page is chosen"_s)}),
        type(u"addItem"_s, u"Add menu item"_s,
             {field(u"item"_s, u"Item"_s, u"string"_s, g),
              with(field(u"modifierSequence"_s, u"Then show"_s, u"pageList"_s, g), u"hint"_s,
                   u"Modifier pages, in order"_s)}),
        type(u"qualifier"_s, u"Qualifier"_s,
             {with(field(u"qualifier"_s, u"Qualifier"_s, u"enum"_s, g), u"options"_s, kQualifiers)}),
        type(u"tender"_s, u"Payment"_s,
             {field(u"tender"_s, u"Tender"_s, u"string"_s, g),
              with(field(u"amount"_s, u"Amount (cents)"_s, u"int"_s, g), u"hint"_s, u"Empty = balance due"_s)}),
        type(u"command"_s, u"Command"_s,
             {with(field(u"name"_s, u"Command"_s, u"enum"_s, g), u"options"_s,
                   options({{"sendOrder", "Send order"}, {"voidItem", "Void item"}, {"lineMore", "One more of the item (+)"},
                            {"lineLess", "One less of the item (−)"}, {"repeatLine", "Again (one more the same way)"},
                            {"undoLast", "Undo (put back the item just removed)"},
                            {"anotherRound", "Another round (the drinks sent last)"},
                            {"amountOff", "Discount: the dollars typed (manager)"}, {"percentOff", "Discount: the percent typed (manager)"}, {"coupon", "Coupon: type or scan its code"}, {"holdCard", "Hold the guest's card (bar tab)"}, {"cardOnFile", "Card on file (bar tab): charge or let go"}, {"printReceipt", "Print receipt"},
                            {"closeCheck", "Close check"}, {"removePayment", "Undo payment"},
                            {"noSale", "Open drawer (no sale)"}, {"openDrawerSession", "Start drawer (bank)"},
                            {"countDrawer", "Count drawer"}, {"endOfDay", "End of day"},
                            {"login", "Log in"}, {"logout", "Log out"}, {"clockIn", "Clock in"},
                            {"clockOut", "Clock out"}, {"startCheck", "Start table check"},
                            {"startQuick", "Start quick check"}, {"startTakeout", "Start takeout"},
                            {"releaseCheck", "Put check away"}, {"newTableCheck", "Another check at this table"},
                            {"splitBySeat", "Split the table by seat"}, {"printTableChecks", "Print every check at the table"},
                            {"combineTableChecks", "Put the table's checks back together"}, {"addComment", "Add note"},
                            {"openAdmin", "Admin screen"}, {"editMode", "Edit pages"},
                            {"addTip", "Add tip (args.percent)"}, {"gratuity", "Gratuity (args.percent)"},
                            {"setCashTip", "Cash tip for the server (the keypad amount)"},
                            {"payout", "Pay out of drawer"}, {"paidIn", "Pay into drawer"},
                            {"cashOutTips", "Cash out my tips"}, {"closeApp", "Close ViewTouch (manager; leaves a kiosk)"}, {"toggleBreak", "Start / end a break"}, {"backupNow", "Back up the database now (manager)"},
        {"askForTip", "Ask the guest for a tip (customer display)"},
        {"toggleTraining", "Practice mode on / off for this screen (manager)"},
        {"selfOrder", "Make this screen a self-order kiosk for guests (manager)"},
        {"rush", "Rush this check (kitchen does it first)"}, {"allergies", "Guest allergies…"}, {"vip", "Mark this check VIP"}, {"clearText", "Clear typed text"}, {"recallTicket", "Recall kitchen ticket"},
        {"openTab", "Open a bar tab (the name typed)"},
        {"kitchenStation", "Kitchen screen: next station"}, {"kitchenAllDay", "Kitchen screen: show / hide All Day"},
        {"expoRecall", "Expediter: bring back the last order sent out"},
        {"seatNext", "Next seat"}, {"seatPrev", "Previous seat"}, {"courseNext", "Next course"},
        {"fireCourse", "Fire the next course"}, {"cancelChoosing", "Cancel the item being chosen"},
        {"finishChoosing", "Done choosing"}, {"setupGuide", "Open the setup guide (managers)"}, {"addWeighed", "Add the item being weighed"},
        {"cancelWeighing", "Cancel weighing"}, {"guestsMore", "One more guest"}, {"guestsFewer", "One fewer guest"},
                            {"startDelivery", "Start delivery"}}))}),
    };
}

} // namespace vt::layout::schema
