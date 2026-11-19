// PosService: starter menus (Menu Builder -> Start from a Template, and the
// starter sets: a sushi bar's, a hardware store's): a kind of place's usual
// categories, choice groups and items, with typical prices, added alongside
// what's there (nothing on the menu already is changed).
//
// Each is a file, seed/menus/<id>.menu:
//   pack Name | food or retail | its number (shops: in its barcodes) | description
//   category Name | #color | meal periods (empty: all day) | ticket printer | station | tax class
//   group Name | min max [ask] | option, option + price, *comes on it
//   item Category | Name | price | groups | what's on it | key: value | ...
// An item's keys (a shop's mostly):
//   variants: Size=S,M,L,XL; Color=Black,White    (every combination, each its own barcode)
//   plus: XL 2.00, XXL 3.00                      (a size or color's price over the item's)
//   sku: TS-100        cost: 6.50        stock: 12 (each size and color, or the item)
//   tax: merchandise   weight: lb (sold by weight)   isbn (a book's barcode: 978...)
// A shop's items and their sizes and colors get a barcode each (EAN-13, the
// store's own 2... range; a book an ISBN's 978...), the same every time.

#include "app/pos_json.hh"
#include "app/pos_service.hh"
#include "app/pos_starter.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QStringList cells(const QString &line)
{
    QStringList out;
    for (const QString &c : line.section(u' ', 1).split(u'|'))
        out << c.trimmed();
    return out;
}

QStringList list(const QString &text, QChar sep = u',')
{
    QStringList out;
    for (const QString &x : text.split(sep, Qt::SkipEmptyParts))
        if (!x.trimmed().isEmpty())
            out << x.trimmed();
    return out;
}

// The check digit that makes 12 digits an EAN-13.
QString ean13(const QString &twelve)
{
    int sum = 0;
    for (int i = 0; i < 12; ++i)
        sum += twelve[i].digitValue() * (i % 2 ? 3 : 1);
    return twelve + QString::number((10 - sum % 10) % 10);
}

} // namespace

const QList<StarterPack> &starterPacks()
{
    static const QList<StarterPack> packs = [] {
        QList<StarterPack> out;
        const QDir dir(u":/seed/menus"_s);
        for (const QString &name : dir.entryList({u"*.menu"_s}, QDir::Files, QDir::Name)) {
            QFile f(dir.filePath(name));
            if (!f.open(QIODevice::ReadOnly))
                continue;
            StarterPack p;
            p.id = name.chopped(5);
            p.text = QString::fromUtf8(f.readAll());
            const QString head = p.text.section(u'\n', 0, 0);
            if (!head.startsWith(u"pack "))
                continue;
            const QStringList c = cells(head);
            p.name = c.value(0);
            p.kind = c.value(1) == u"retail" ? u"retail"_s : u"food"_s;
            p.number = c.value(2).toInt();
            p.description = c.value(3);
            for (const QString &line : p.text.split(u'\n')) {
                p.categories += line.startsWith(u"category ");
                p.items += line.startsWith(u"item ");
            }
            out.append(p);
        }
        return out;
    }();
    return packs;
}

const StarterPack *starterPack(const QString &id)
{
    for (const StarterPack &p : starterPacks())
        if (p.id == id)
            return &p;
    return nullptr;
}

QVariantList PosService::menuTemplates() const
{
    // A shop's for a shop; a restaurant's or a food truck's otherwise: the
    // two never mix on one menu.
    const QString kind = serviceMode() == u"retail" ? u"retail"_s : u"food"_s;
    QVariantList out;
    for (const StarterPack &t : starterPacks()) {
        if (t.kind != kind)
            continue;
        out.append(QVariantMap{{u"id"_s, t.id},
                               {u"name"_s, tr(t.name.toUtf8().constData())},
                               {u"description"_s, tr(t.description.toUtf8().constData())},
                               {u"kind"_s, t.kind},
                               {u"categories"_s, t.categories}, {u"items"_s, t.items}});
    }
    return out;
}

bool PosService::applyMenuTemplate(const QString &id)
{
    if (!require(perm::Manager, tr("Changing the menu")))
        return false;
    const StarterPack *t = starterPack(id);
    if (!t)
        return fail(tr("There's no starter menu '%1'.").arg(id));
    if ((t->kind == u"retail") != (serviceMode() == u"retail"))
        return fail(t->kind == u"retail" ? tr("That's a shop's: this store serves food (Store Settings -> How you serve).")
                                         : tr("That's a restaurant's: this store is a shop (Store Settings -> How you serve)."));
    const MenuStep step(this, tr("Starter menu"));
    const QStringList lines = t->text.split(u'\n');
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
    const bool shop = t->kind == u"retail";
    int added = 0, itemNumber = 0;
    for (const QString &line : lines) {
        const QStringList c = cells(line);
        if (line.startsWith(u"category ") && categoryId(c.value(0)).isEmpty()) {
            // Its kitchen station (the sushi bar, the wok): the store gets it if it hasn't.
            if (const std::string station = ss(c.value(4)); !station.empty()
                && std::ranges::none_of(s_->settings.stations, [&](const Station &x) { return x.id == station; })) {
                QString name = c.value(4);
                name[0] = name[0].toUpper();
                s_->settings.stations.push_back({station, ss(name)});
                if (s_->sink)
                    s_->sink->saveSettings(s_->settings);
            }
            QVariantMap record{{u"name"_s, c.value(0)}, {u"color"_s, c.value(1)}, {u"periods"_s, list(c.value(2))},
                               {u"printer"_s, c.value(3).isEmpty() ? (shop ? QString() : u"kitchen"_s) : c.value(3)},
                               {u"station"_s, c.value(4)}};
            if (!c.value(5).isEmpty())
                record.insert(u"taxClass"_s, c.value(5));
            if (!saveCategory(record))
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
            ++itemNumber;   // counted whether added or not: its barcode stays its own
            const QString name = c.value(1);
            if (std::ranges::any_of(s_->menu, [&](const MenuItem &m) { return QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0; }))
                continue;
            // key: value cells after what's on it
            QMap<QString, QString> keys;
            for (qsizetype k = 5; k < c.size(); ++k) {
                const qsizetype colon = c[k].indexOf(u':');
                if (colon > 0)
                    keys.insert(c[k].left(colon).trimmed(), c[k].mid(colon + 1).trimmed());
                else if (!c[k].isEmpty())
                    keys.insert(c[k], QString());
            }
            QStringList groups;
            for (const QString &g : list(c.value(3)))
                if (const QString gid = groupId(g); !gid.isEmpty())
                    groups << gid;
            QVariantMap card{{u"name"_s, name}, {u"price"_s, c.value(2)}, {u"family"_s, categoryId(c.value(0))},
                             {u"groups"_s, groups}};
            if (!c.value(4).isEmpty())
                card.insert(u"onIt"_s, c.value(4));
            if (keys.contains(u"tax"_s))
                card.insert(u"taxClass"_s, keys.value(u"tax"_s));
            const bool variants = keys.contains(u"variants"_s);
            // The item's barcode: the store's own range (2), the pack's number, the item's.
            const QString base = keys.contains(u"isbn"_s) ? u"9780%1%2"_s.arg(t->number, 2, 10, QChar(u'0')).arg(itemNumber, 3, 10, QChar(u'0'))
                                                          : u"2%1%2"_s.arg(t->number, 2, 10, QChar(u'0')).arg(itemNumber, 3, 10, QChar(u'0'));
            const auto code = [&](int variant) {
                return ean13((base + u"%1"_s.arg(variant, 3, 10, QChar(u'0'))).leftJustified(12, u'0').left(12));
            };
            if (shop && !variants)
                card.insert(u"barcodes"_s, code(0));
            if (keys.contains(u"stock"_s)) {
                // (With sizes and colors, each counts its own: the item's own count stays 0.)
                card.insert(u"stockTrack"_s, true);
                card.insert(u"stockOnHand"_s, variants ? u"0"_s : keys.value(u"stock"_s));
                card.insert(u"stockLowAt"_s, QString::number(std::max(1, keys.value(u"stock"_s).toInt() / 4)));
                card.insert(u"stockPar"_s, keys.value(u"stock"_s));
                if (keys.contains(u"cost"_s))
                    card.insert(u"cost"_s, keys.value(u"cost"_s));
            }
            if (!saveMenuItemCard(card))
                return false;
            const auto made = std::ranges::find_if(s_->menu, [&](const MenuItem &m) {
                return !m.isModifier && QString::compare(qs(m.name), name, Qt::CaseInsensitive) == 0;
            });
            if (made == s_->menu.end())
                return false;
            const QString itemId = qs(made->id);
            if (keys.contains(u"weight"_s)) {   // sold by the pound (or kilo): the scale asks
                made->byWeight = true;
                made->weightUnit = ss(keys.value(u"weight"_s).isEmpty() ? u"lb"_s : keys.value(u"weight"_s));
                if (s_->sink)
                    s_->sink->saveMenuItem(*made, int(made - s_->menu.begin()));
            }
            if (variants) {
                // Size=S,M,L; Color=Black,White: every combination, its barcode, SKU and price.
                QVariantList axes;
                QList<QStringList> values;
                for (const QString &axis : list(keys.value(u"variants"_s), u';')) {
                    const QStringList kv = axis.split(u'=');
                    values << list(kv.value(1));
                    axes.append(QVariantMap{{u"name"_s, kv.value(0).trimmed()}, {u"values"_s, values.back()}});
                }
                QMap<QString, qint64> plus;   // "XL 2.00": XL is 2.00 more
                for (const QString &p : list(keys.value(u"plus"_s))) {
                    const qsizetype space = p.lastIndexOf(u' ');
                    plus.insert(p.left(space).trimmed(), priceCents(p.mid(space + 1), qs(s_->settings.currencySymbol)));
                }
                QStringList combos{QString()};
                for (const QStringList &v : values) {
                    QStringList next;
                    for (const QString &k : combos)
                        for (const QString &x : v)
                            next << (k.isEmpty() ? x : k + u'|' + x);
                    combos = next;
                }
                const qint64 price = priceCents(c.value(2), qs(s_->settings.currencySymbol));
                QVariantList rows;
                for (int n = 0; n < combos.size(); ++n) {
                    qint64 more = 0;
                    for (const QString &part : combos[n].split(u'|'))
                        more += plus.value(part, 0);
                    QString sku = keys.value(u"sku"_s);
                    for (const QString &part : combos[n].split(u'|'))
                        sku += u'-' + part.left(3).toUpper().remove(u' ');
                    QVariantMap row{{u"key"_s, combos[n]}, {u"barcode"_s, code(n + 1)}, {u"sku"_s, sku}};
                    if (more > 0)
                        row.insert(u"price"_s, QString::number((price + more) / 100.0, 'f', 2));
                    rows.append(row);
                }
                if (!setItemVariants(itemId, axes, rows))
                    return false;
                if (keys.contains(u"stock"_s)) {
                    // A few of each, more of the middle sizes; one size running low.
                    const int each = std::max(1, keys.value(u"stock"_s).toInt());
                    for (int n = 0; n < combos.size(); ++n)
                        setVariantStock(itemId, combos[n], std::max(0, each - (n % 5 == 4 ? each - 1 : (n % 3))));
                }
            }
            ++added;
        }
    }
    emit notice(tr("%1: added %n item(s). Change any of it as you like.", nullptr, added)
                    .arg(tr(t->name.toUtf8().constData())));
    return true;
}

} // namespace vt::app
