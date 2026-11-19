#include "app/pos_starter.hh"

#include "app/pos_json.hh"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {

// seed/pos (a restaurant), seed/pos/truck, seed/pos/retail.
QString folderOf(const QString &kind)
{
    return kind == u"foodTruck" ? u":/seed/pos/truck/"_s : kind == u"retail" ? u":/seed/pos/retail/"_s : u":/seed/pos/"_s;
}

QJsonDocument readSeed(const QString &folder, const QString &name)
{
    QFile f(folder + name);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()) : QJsonDocument();
}

} // namespace

QString defaultPack(const QString &kind)
{
    return kind == u"foodTruck" ? u"taqueria"_s : kind == u"retail" ? u"apparel"_s : QString();
}

std::optional<StarterSet> parseStarterSet(const QString &text, QString *error)
{
    StarterSet set;
    const QString kind = text.section(u':', 0, 0).trimmed();
    set.pack = text.section(u':', 1).trimmed();
    if (!kind.isEmpty())
        set.kind = kind;
    if (set.kind == u"truck")
        set.kind = u"foodTruck"_s;
    if (set.kind != u"restaurant" && set.kind != u"foodTruck" && set.kind != u"retail") {
        if (error)
            *error = u"A starter set is restaurant, foodTruck or retail (with a menu after a colon: restaurant:sushi)."_s;
        return std::nullopt;
    }
    if (!set.pack.isEmpty()) {
        const StarterPack *p = starterPack(set.pack);
        if (!p || (p->kind == u"retail") != (set.kind == u"retail")) {
            QStringList fits;
            for (const StarterPack &x : starterPacks())
                if ((x.kind == u"retail") == (set.kind == u"retail"))
                    fits << x.id;
            if (error)
                *error = (!p ? u"There's no starter menu '%1'."_s.arg(set.pack)
                             : u"'%1' is for %2."_s.arg(set.pack, p->kind == u"retail" ? u"a shop"_s : u"a restaurant or food truck"_s))
                         + u" For %1: %2."_s.arg(set.kind, fits.join(u", "_s));
            return std::nullopt;
        }
    }
    return set;
}

std::optional<PosData> starterData(const StarterSet &set, QString *error)
{
    const QString folder = folderOf(set.kind);
    PosData d;
    d.settings = settingsFromJson(readSeed(folder, u"settings.json"_s).object());
    d.employees = employeesFromJson(readSeed(folder, u"employees.json"_s).array());
    d.menu = menuFromJson(readSeed(folder, u"menu.json"_s).array());
    d.ingredients = ingredientsFromJson(readSeed(folder, u"ingredients.json"_s).array());
    d.settings.serviceMode = set.kind == u"restaurant" ? std::string() : set.kind.toStdString();
    const QString pack = set.pack.isEmpty() ? defaultPack(set.kind) : set.pack;
    if (pack.isEmpty())
        return d;   // the restaurant's own menu (seed/pos/menu.json)

    // Another menu instead: made the way Menu Builder's Start from a Template
    // makes it, by the sample manager, on the kind's settings and staff.
    d.menu.clear();
    d.ingredients.clear();
    d.settings.menuCategories.clear();
    d.settings.modifierGroups.clear();
    d.settings.promotions.clear();   // (the café's: Burger Tuesday...)
    d.settings.priceChanges.clear();
    PosShared shared(std::move(d), nullptr);
    {
        PosService maker(&shared, u"Starter"_s);
        QString said;   // what went wrong, as the screen would say it
        QObject::connect(&maker, &PosService::notice, &maker, [&said](const QString &text) { said = text; });
        if (!maker.loginWithPin(u"1234"_s) || !maker.applyMenuTemplate(pack)) {
            if (error)
                *error = u"The starter menu '%1' didn't load: %2"_s.arg(pack, said);
            return std::nullopt;
        }
        maker.logout();
    }
    PosData out;
    out.settings = shared.settings;
    out.menu = shared.menu;
    out.employees = shared.employees;
    out.ingredients = shared.ingredients;
    out.settings.setupDone = false;
    return out;
}

} // namespace vt::app
