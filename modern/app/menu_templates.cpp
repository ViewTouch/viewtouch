// PosService: starter menus (Menu Builder -> Start from a Template): a kind
// of place's usual categories, choice groups and items, with typical prices,
// added alongside what's there (nothing on the menu already is changed).

#include "app/pos_json.hh"
#include "app/pos_service.hh"

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

struct Template {
    const char *id;
    const char *name;
    const char *description;
    // One per line:
    //   category Name | #color | meal periods (empty: all day) | kitchen ticket | station
    //   group Name | min max [ask] | option, option + price, *comes on it
    //   item Category | Name | price | groups | what's on it
    const char *menu;
};

const Template kTemplates[] = {
    {"taqueria", QT_TR_NOOP("Taqueria"),
     QT_TR_NOOP("Tacos, burritos, tortas and aguas frescas; salsas, tortillas, what's on them."),
     R"(category Tacos | #b83232 | | kitchen | grill
category Burritos & Tortas | #a86a12 | | kitchen | grill
category Sides | #8a5a2b | | kitchen |
category Drinks | #1f6f73 | | bar |
group Salsa | 1 1 ask | Roja, Verde, Pico de gallo, Habanero + 0.50
group Tortilla | 1 1 | Corn, Flour
group Meat | 1 1 | Carne Asada, Al Pastor, Pollo, Carnitas, Barbacoa + 1.00
group Size | 1 1 | Small, Medium + 0.75, Large + 1.50
item Tacos | Carne Asada Taco | 3.50 | Salsa, Tortilla | onion, cilantro
item Tacos | Al Pastor Taco | 3.25 | Salsa, Tortilla | onion, cilantro, pineapple
item Tacos | Pollo Taco | 3.00 | Salsa, Tortilla | onion, cilantro
item Tacos | Carnitas Taco | 3.25 | Salsa, Tortilla | onion, cilantro
item Burritos & Tortas | Burrito | 11.00 | Meat, Salsa | rice, beans, cheese, sour cream
item Burritos & Tortas | Torta | 10.00 | Meat | beans, lettuce, tomato, avocado, mayo
item Burritos & Tortas | Quesadilla | 9.00 | Meat, Tortilla | cheese
item Sides | Chips & Salsa | 3.50 | Salsa |
item Sides | Guacamole | 5.00 | |
item Sides | Rice & Beans | 4.00 | |
item Drinks | Horchata | 3.00 | Size |
item Drinks | Agua de Jamaica | 3.00 | Size |
item Drinks | Mexican Coke | 3.50 | |)"},
    {"cafe", QT_TR_NOOP("Café"),
     QT_TR_NOOP("Espresso drinks, tea, pastries and breakfast; sizes, milks, extra shots."),
     R"(category Coffee | #8a5a2b | | bar |
category Tea | #1f8a4c | | bar |
category Pastries | #a86a12 | | kitchen |
category Breakfast | #2b62b0 | breakfast, lunch | kitchen |
group Size | 1 1 | Small, Medium + 0.50, Large + 1.00
group Milk | 0 1 ask | Whole, 2%, Oat + 0.75, Almond + 0.75, Soy + 0.50
group Extras | 0 0 ask | Extra Shot + 1.00, Vanilla + 0.75, Caramel + 0.75, Whipped Cream + 0.50
group Hot or Iced | 1 1 | Hot, Iced
item Coffee | Drip Coffee | 2.75 | Size |
item Coffee | Espresso | 3.00 | Extras |
item Coffee | Latte | 4.75 | Size, Hot or Iced, Milk, Extras |
item Coffee | Cappuccino | 4.50 | Size, Milk, Extras |
item Coffee | Americano | 3.50 | Size, Hot or Iced |
item Coffee | Mocha | 5.25 | Size, Hot or Iced, Milk, Extras |
item Tea | Chai Latte | 4.75 | Size, Hot or Iced, Milk |
item Tea | Hot Tea | 3.00 | Size |
item Pastries | Croissant | 3.75 | |
item Pastries | Blueberry Muffin | 3.50 | |
item Pastries | Cinnamon Roll | 4.25 | |
item Breakfast | Breakfast Sandwich | 7.50 | | egg, cheese, bacon
item Breakfast | Avocado Toast | 8.50 | | avocado, chili flakes)"},
    {"burgers", QT_TR_NOOP("Burgers & Bar"),
     QT_TR_NOOP("Burgers, wings, fries and beer; how it's cooked, sides, sauces."),
     R"(category Burgers | #a86a12 | | kitchen | grill
category Wings & Starters | #b83232 | | kitchen | fryer
category Sides | #8a5a2b | | kitchen | fryer
category Beer | #1f6f73 | | bar |
group How it's cooked | 1 1 | Rare, Medium Rare, Medium, Medium Well, Well Done
group Side | 1 1 | Fries, Tots, Onion Rings + 1.50, Side Salad
group Wing Sauce | 1 1 ask | Buffalo, BBQ, Honey Garlic, Lemon Pepper, Nashville Hot
group Add | 0 0 ask | Bacon + 2.00, Cheese + 1.00, Fried Egg + 1.50, Avocado + 1.50
item Burgers | Classic Burger | 12.00 | How it's cooked, Side, Add | lettuce, tomato, onion, pickles, mayo
item Burgers | Cheeseburger | 13.00 | How it's cooked, Side, Add | cheese, lettuce, tomato, onion, pickles
item Burgers | Bacon BBQ Burger | 14.50 | How it's cooked, Side, Add | bacon, cheddar, onion rings, BBQ sauce
item Burgers | Veggie Burger | 12.00 | Side, Add | lettuce, tomato, onion
item Wings & Starters | Wings (8) | 13.00 | Wing Sauce | celery, ranch
item Wings & Starters | Loaded Nachos | 11.00 | | cheese, jalapeños, sour cream, salsa
item Sides | Fries | 4.00 | |
item Sides | Onion Rings | 5.50 | |
item Beer | Draft Pint | 6.00 | |
item Beer | Bottled Beer | 5.00 | |)"},
    {"pizza", QT_TR_NOOP("Pizza"),
     QT_TR_NOOP("Pizzas by size, toppings, salads and drinks."),
     R"(category Pizza | #b83232 | | kitchen |
category Salads | #1f8a4c | | kitchen |
category Drinks | #1f6f73 | | bar |
group Size | 1 1 | 10" Small, 12" Medium + 3.00, 16" Large + 6.00
group Crust | 1 1 | Hand-Tossed, Thin, Deep Dish + 2.00
group Toppings | 0 0 ask | Pepperoni + 1.50, Sausage + 1.50, Mushrooms + 1.00, Onions + 1.00, Peppers + 1.00, Olives + 1.00, Extra Cheese + 1.50
group Dressing | 1 1 ask | Ranch, Italian, Caesar, Balsamic
item Pizza | Cheese Pizza | 11.00 | Size, Crust, Toppings | sauce, mozzarella
item Pizza | Pepperoni Pizza | 13.00 | Size, Crust, Toppings | sauce, mozzarella, pepperoni
item Pizza | Margherita | 13.50 | Size, Crust | sauce, fresh mozzarella, basil
item Pizza | Supreme | 16.00 | Size, Crust, Toppings | sauce, mozzarella, pepperoni, sausage, peppers, onions, olives
item Salads | Garden Salad | 7.50 | Dressing | lettuce, tomato, cucumber, onion
item Salads | Caesar Salad | 8.50 | Dressing | romaine, parmesan, croutons
item Drinks | Fountain Drink | 2.75 | |
item Drinks | Pitcher of Soda | 6.50 | |)"},
};

const Template *templateById(const QString &id)
{
    for (const Template &t : kTemplates)
        if (QString::fromLatin1(t.id) == id)
            return &t;
    return nullptr;
}

QStringList cells(const QString &line)
{
    QStringList out;
    for (const QString &c : line.section(u' ', 1).split(u'|'))
        out << c.trimmed();
    return out;
}

QStringList list(const QString &text)
{
    QStringList out;
    for (const QString &x : text.split(u',', Qt::SkipEmptyParts))
        if (!x.trimmed().isEmpty())
            out << x.trimmed();
    return out;
}

} // namespace

QVariantList PosService::menuTemplates() const
{
    QVariantList out;
    for (const Template &t : kTemplates) {
        int categories = 0, items = 0;
        for (const QString &line : QString::fromUtf8(t.menu).split(u'\n')) {
            categories += line.startsWith(u"category ");
            items += line.startsWith(u"item ");
        }
        out.append(QVariantMap{{u"id"_s, QString::fromLatin1(t.id)},
                               {u"name"_s, tr(t.name)},
                               {u"description"_s, tr(t.description)},
                               {u"categories"_s, categories}, {u"items"_s, items}});
    }
    return out;
}

bool PosService::applyMenuTemplate(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const MenuStep step(this, tr("Starter menu"));
    const Template *t = templateById(id);
    if (!t)
        return fail(tr("There's no starter menu '%1'.").arg(id));
    const QStringList lines = QString::fromUtf8(t->menu).split(u'\n');
    const auto categoryId = [&](const QString &name) {
        for (const MenuCategory &c : s_->categories())
            if (QString::compare(qs(c.name), name, Qt::CaseInsensitive) == 0)
                return qs(c.id);
        return QString();
    };
    const auto groupId = [&](const QString &name) {
        for (const ModifierGroup &g : s_->settings.modifierGroups)
            if (QString::compare(qs(g.name), name, Qt::CaseInsensitive) == 0)
                return qs(g.id);
        return QString();
    };
    int added = 0;
    for (const QString &line : lines) {
        const QStringList c = cells(line);
        if (line.startsWith(u"category ") && categoryId(c.value(0)).isEmpty()) {
            if (!saveCategory({{u"name"_s, c.value(0)}, {u"color"_s, c.value(1)}, {u"periods"_s, list(c.value(2))},
                               {u"printer"_s, c.value(3).isEmpty() ? u"kitchen"_s : c.value(3)}, {u"station"_s, c.value(4)}}))
                return false;
        } else if (line.startsWith(u"group ") && groupId(c.value(0)).isEmpty()) {
            const QStringList rule = c.value(1).split(u' ', Qt::SkipEmptyParts);
            QVariantList options;
            for (const QString &o : list(c.value(2))) {
                const bool included = o.startsWith(u'*');
                const QString text = included ? o.mid(1).trimmed() : o;
                const qsizetype plus = text.lastIndexOf(u'+');
                options.append(QVariantMap{{u"name"_s, (plus > 0 ? text.left(plus) : text).trimmed()},
                                           {u"price"_s, plus > 0 ? text.mid(plus + 1).trimmed() : QString()},
                                           {u"included"_s, included}});
            }
            if (!saveChoiceGroup({{u"name"_s, c.value(0)}, {u"min"_s, rule.value(0).toInt()}, {u"max"_s, rule.value(1).toInt()},
                                  {u"askHow"_s, rule.contains(u"ask"_s)}, {u"options"_s, options}}))
                return false;
        } else if (line.startsWith(u"item ")) {
            const QString name = c.value(1);
            if (std::ranges::any_of(s_->menu, [&](const MenuItem &m) { return QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0; }))
                continue;
            QStringList groups;
            for (const QString &g : list(c.value(3)))
                if (const QString gid = groupId(g); !gid.isEmpty())
                    groups << gid;
            QVariantMap card{{u"name"_s, name}, {u"price"_s, c.value(2)}, {u"family"_s, categoryId(c.value(0))},
                             {u"groups"_s, groups}};
            if (!c.value(4).isEmpty())
                card.insert(u"onIt"_s, c.value(4));
            if (!saveMenuItemCard(card))
                return false;
            ++added;
        }
    }
    emit notice(tr("%1: added %n item(s). Change any of it as you like.", nullptr, added)
                    .arg(tr(t->name)));
    return true;
}

} // namespace vt::app
