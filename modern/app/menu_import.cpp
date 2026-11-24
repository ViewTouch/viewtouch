#include "app/menu_import.hh"

#include "app/xlsx.hh"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {

QString tr(const char *text, int n = -1) { return QCoreApplication::translate("MenuImport", text, nullptr, n); }

// The cells of one line; quotes may hold the separator and doubled quotes.
QList<QStringList> rowsOf(const QString &text, QChar sep)
{
    QList<QStringList> rows;
    QStringList row;
    QString cell;
    bool quoted = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (quoted) {
            if (c == u'"' && i + 1 < text.size() && text[i + 1] == u'"') {
                cell += u'"';
                ++i;
            } else if (c == u'"') {
                quoted = false;
            } else {
                cell += c;
            }
        } else if (c == u'"') {
            quoted = true;
        } else if (c == sep) {
            row << cell.trimmed();
            cell.clear();
        } else if (c == u'\n' || c == u'\r') {
            if (c == u'\r' && i + 1 < text.size() && text[i + 1] == u'\n')
                ++i;
            row << cell.trimmed();
            rows << row;
            row.clear();
            cell.clear();
        } else {
            cell += c;
        }
    }
    if (!cell.isEmpty() || !row.isEmpty()) {
        row << cell.trimmed();
        rows << row;
    }
    return rows;
}

// A header as plain lower-case words: "Weight (lb)" -> "weight lb".
QString plain(const QString &header)
{
    return header.trimmed().toLower().normalized(QString::NormalizationForm_KD).remove(QRegularExpression(u"[^a-z ]"_s)).simplified();
}

QString what(const QString &header)
{
    const QString h = plain(header);
    static const QList<std::pair<QString, QStringList>> names = {
        {u"name"_s, {u"name"_s, u"item"_s, u"item name"_s, u"product"_s, u"dish"_s, u"menu item"_s, u"nombre"_s, u"producto"_s, u"platillo"_s}},
        {u"price"_s, {u"price"_s, u"base price"_s, u"default price"_s, u"menu price"_s, u"retail price"_s, u"selling price"_s,
                      u"sale price"_s, u"amount"_s, u"precio"_s, u"precio de venta"_s}},
        // What the store pays: the price only when the sheet has no price.
        {u"cost"_s, {u"cost"_s, u"unit cost"_s, u"cost price"_s, u"costo"_s}},
        {u"onHand"_s, {u"on hand"_s, u"stock"_s, u"in stock"_s, u"stock on hand"_s, u"qty"_s, u"quantity"_s, u"existencia"_s,
                       u"existencias"_s}},
        {u"lowAt"_s, {u"low at"_s, u"reorder point"_s, u"reorder at"_s, u"reorder level"_s, u"min stock"_s}},
        {u"par"_s, {u"order up to"_s, u"par"_s, u"par level"_s}},
        {u"sku"_s, {u"sku"_s}},
        {u"category"_s, {u"category"_s, u"categories"_s, u"family"_s, u"group"_s, u"section"_s, u"menu group"_s, u"type"_s,
                         u"categoria"_s, u"categorias"_s, u"seccion"_s, u"grupo"_s, u"tipo"_s}},
        {u"menu"_s, {u"menu"_s, u"reporting category"_s}},   // a category when there's no other
        {u"subgroup"_s, {u"subgroup"_s, u"menu subgroup"_s, u"subcategory"_s, u"subcategoria"_s}},
        {u"description"_s, {u"description"_s, u"desc"_s, u"details"_s, u"descripcion"_s}},
        {u"onIt"_s, {u"ingredients"_s, u"on it"_s, u"toppings"_s, u"whats on it"_s, u"comes with"_s, u"ingredientes"_s}},
        {u"photo"_s, {u"photo"_s, u"photos"_s, u"picture"_s, u"image"_s, u"photo file"_s, u"picture file"_s, u"image file"_s,
                      u"foto"_s, u"imagen"_s, u"fotografia"_s}},
        {u"barcode"_s, {u"barcode"_s, u"barcodes"_s, u"upc"_s, u"ean"_s, u"gtin"_s, u"product code"_s, u"codigo de barras"_s}},
        {u"number"_s, {u"plu"_s, u"plu code"_s, u"item number"_s}},
        {u"size"_s, {u"variation name"_s, u"variation"_s, u"size"_s, u"tamano"_s}},
        {u"groups"_s, {u"modifier groups"_s, u"modifiers"_s, u"modifier sets"_s}},
        {u"archived"_s, {u"archived"_s}},
        {u"hidden"_s, {u"hidden"_s}},
        {u"sellable"_s, {u"sellable"_s}},
        {u"alcohol"_s, {u"contains alcohol"_s, u"alcohol"_s, u"alcoholic"_s}},
        {u"priceType"_s, {u"price type"_s}},
        {u"isModifier"_s, {u"modifier"_s, u"is modifier"_s}},   // Toast's item list: Yes for a modifier option
    };
    for (const auto &[key, words] : names)
        if (words.contains(h))
            return key;
    return {};
}

// Above this, not a price (in cents near the limit of whole numbers, a
// few of it would overflow the totals).
constexpr double kMaxPrice = 10'000'000;

// From Toast's JSON: a price, else 0.
double jsonPrice(const QJsonValue &v)
{
    const double p = v.toDouble();
    return std::isfinite(p) && p > 0 && p < kMaxPrice ? p : 0;
}

bool price(QString text, double *out)
{
    text = text.trimmed();
    text.remove(QRegularExpression(u"[^0-9.,]"_s));
    if (text.isEmpty())
        return false;
    // "3,50": a comma for the cents; "1,250.00": a thousands comma.
    if (!text.contains(u'.') && text.count(u',') == 1 && text.section(u',', 1).size() <= 2)
        text.replace(u',', u'.');
    else
        text.remove(u',');
    bool ok = false;
    *out = text.toDouble(&ok);
    return ok && *out >= 0 && *out < kMaxPrice;   // 99999999999999 isn't a price
}

bool yes(const QString &cell)
{
    static const QStringList words{u"y"_s, u"yes"_s, u"true"_s, u"1"_s, u"x"_s, u"si"_s};
    return words.contains(plain(cell)) || cell.trimmed() == u"1"_s || cell.trimmed() == u"✓"_s;
}

bool no(const QString &cell)
{
    static const QStringList words{u"n"_s, u"no"_s, u"false"_s, u"0"_s};
    return words.contains(plain(cell)) || cell.trimmed() == u"0"_s;
}

QStringList listOf(const QString &cell)
{
    QStringList out;
    for (const QString &s : cell.split(QRegularExpression(u"[,;\n]"_s)))
        if (const QString t = s.simplified(); !t.isEmpty() && !out.contains(t, Qt::CaseInsensitive))
            out << t;
    return out;
}

bool sameName(const QString &a, const QString &b) { return QString::compare(a, b, Qt::CaseInsensitive) == 0; }

bool isSizeGroup(const ImportedGroup &g)
{
    static const QStringList names{u"size"_s, u"sizes"_s, u"tamano"_s, u"tamanos"_s, u"portion"_s};
    return names.contains(plain(g.name)) && g.min == 1 && g.max == 1 && g.options.size() >= 2;
}

// What's left to do for any of them: sizes from a Size group, choice groups
// that ViewTouch can save (options once each, no more required than there are).
void finish(MenuImport &out)
{
    for (ImportedGroup &g : out.groups) {
        QList<ImportedPrice> options;
        for (const ImportedPrice &o : g.options)
            if (!o.name.isEmpty() && std::ranges::none_of(options, [&](const ImportedPrice &x) { return sameName(x.name, o.name); }))
                options << o;
        g.options = options;
        g.min = std::clamp(g.min, 0, int(g.options.size()));
        if (g.max > 0 && g.max < g.min)
            g.max = g.min;
    }
    const auto group = [&](const QString &name) -> const ImportedGroup * {
        const auto it = std::ranges::find_if(out.groups, [&](const ImportedGroup &g) { return sameName(g.name, name); });
        return it == out.groups.end() ? nullptr : &*it;
    };
    for (ImportedItem &i : out.items) {
        for (const QString &name : QStringList(i.groups)) {
            const ImportedGroup *g = group(name);
            if (!g || !i.sizes.isEmpty() || !isSizeGroup(*g))
                continue;
            for (const ImportedPrice &o : g->options)
                i.sizes << ImportedPrice{o.name, i.price + o.price};
            i.groups.removeAll(name);
        }
        if (i.sizes.size() == 1) {
            i.price = i.sizes[0].price;
            i.sizes.clear();
        } else if (!i.sizes.isEmpty()) {
            i.price = std::ranges::min(i.sizes, {}, &ImportedPrice::price).price;
        }
    }
    // Groups no item asks for any more (a Size group turned into sizes).
    out.groups.removeIf([&](const ImportedGroup &g) {
        return g.options.isEmpty() || std::ranges::none_of(out.items, [&](const ImportedItem &i) { return i.groups.contains(g.name, Qt::CaseInsensitive); });
    });
}

// One sheet of items: a header (else name, price, category), then a row an
// item, or (Square) a row a size of it.
MenuImport readTable(const QList<QStringList> &rows)
{
    MenuImport out;
    QHash<QString, int> at{{u"name"_s, 0}, {u"price"_s, 1}, {u"category"_s, 2}};
    QList<std::pair<int, QString>> modifierSets;   // Square: "Modifier Set - Toppings" (Y / N)
    QList<int> optionValues;                       // Square: "Option Value 1"...
    int firstRow = 0;
    if (!rows.isEmpty()) {
        QHash<QString, int> found;
        static const QRegularExpression set(u"^\\s*modifier set\\s*[-–:]\\s*(.+?)\\s*$"_s, QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression option(u"^\\s*option value\\s*\\d*\\s*$"_s, QRegularExpression::CaseInsensitiveOption);
        for (int i = 0; i < rows[0].size(); ++i) {
            const QString &h = rows[0][i];
            if (const auto m = set.match(h); m.hasMatch())
                modifierSets.append({i, m.captured(1)});
            else if (option.match(h).hasMatch())
                optionValues << i;
            else if (const QString w = what(h); !w.isEmpty() && !found.contains(w))
                found.insert(w, i);
        }
        // A sheet with a price and a cost: the price is what it sells for.
        // Only a cost (some menus call the price that): that is the price.
        if (!found.contains(u"price"_s) && found.contains(u"cost"_s))
            found.insert(u"price"_s, found.value(u"cost"_s));
        if (found.contains(u"name"_s)) {
            at = found;
            firstRow = 1;
            const QStringList headers = [&] {
                QStringList p;
                for (const QString &h : rows[0])
                    p << plain(h);
                return p;
            }();
            if (headers.contains(u"token"_s) && at.contains(u"size"_s))
                out.from = u"Square"_s;
            else if (headers.contains(u"clover id"_s))
                out.from = u"Clover"_s;
            else if (headers.contains(u"guid"_s) && at.contains(u"isModifier"_s)) {
                out.from = u"Toast"_s;
                // Toast's other lists (menus, menu groups, modifier options) look
                // like its item list but hold no items, and nothing says what
                // goes where. Only the item list has SKU and PLU.
                if (!headers.contains(u"sku"_s) && !headers.contains(u"plu"_s)) {
                    out.problems << (headers.contains(u"parent name"_s)
                                         ? tr("This is Toast's list of menu groups: it has no items in it. "
                                              "Import MenuItem_Export.csv, and make the categories here.")
                                         : tr("This is Toast's list of menus or of modifier options: it has no items in it. "
                                              "Import MenuItem_Export.csv, and make the choice groups here."));
                    return out;
                }
            }
        }
    }
    if (!at.contains(u"category"_s) && at.contains(u"menu"_s))
        at.insert(u"category"_s, at.value(u"menu"_s));
    for (const QString &key : {u"name"_s, u"price"_s, u"category"_s, u"subgroup"_s, u"description"_s, u"onIt"_s, u"photo"_s,
                               u"barcode"_s, u"number"_s, u"size"_s, u"groups"_s, u"alcohol"_s, u"sku"_s, u"onHand"_s, u"lowAt"_s, u"par"_s})
        if (at.contains(key))
            out.columns << key;
    if (at.contains(u"cost"_s) && at.value(u"cost"_s) != at.value(u"price"_s))
        out.columns << u"cost"_s;
    if (!modifierSets.isEmpty() && !out.columns.contains(u"groups"_s))
        out.columns << u"groups"_s;
    const auto cell = [&](const QStringList &r, const QString &key) {
        const int i = at.value(key, -1);
        return i >= 0 && i < r.size() ? r[i].trimmed() : QString();
    };
    const bool sized = at.contains(u"size"_s) || !optionValues.isEmpty();
    QHash<QString, int> byName;   // sized: the item each row is a size of
    QSet<QString> seen;           // not sized: the names read, once each
    QStringList twice;
    int options = 0;              // Toast's modifier options, left out
    for (int r = firstRow; r < rows.size(); ++r) {
        const QStringList &row = rows[r];
        if (std::all_of(row.begin(), row.end(), [](const QString &c) { return c.trimmed().isEmpty(); }))
            continue;
        const int line = r + 1;
        QString name = cell(row, u"name"_s);
        QString size = cell(row, u"size"_s);
        if (size.isEmpty()) {
            QStringList values;
            for (const int i : optionValues)
                if (i < row.size() && !row[i].trimmed().isEmpty())
                    values << row[i].trimmed();
            size = values.join(u' ');
        }
        if (name.isEmpty() && sized && !size.isEmpty() && !out.items.isEmpty())
            name = out.items.last().name;   // a size under the item's own row
        if (name.isEmpty()) {
            out.problems << tr("Row %1: no name").arg(line);
            continue;
        }
        // A modifier option (Toast's item list has them among the items, but
        // not which items they go with).
        if (yes(cell(row, u"isModifier"_s))) {
            ++options;
            continue;
        }
        if (yes(cell(row, u"archived"_s))) {
            out.problems << tr("Row %1: %2 is archived: left out").arg(line).arg(name);
            continue;
        }
        if (yes(cell(row, u"hidden"_s))) {
            out.problems << tr("Row %1: %2 is hidden there: left out").arg(line).arg(name);
            continue;
        }
        if (no(cell(row, u"sellable"_s))) {
            out.problems << tr("Row %1: %2 isn't for sale there: left out").arg(line).arg(name);
            continue;
        }
        if (plain(cell(row, u"priceType"_s)) == u"variable"_s) {
            out.problems << tr("Row %1: %2 is priced as it's sold: add it by hand").arg(line).arg(name);
            continue;
        }
        double p = 0;
        if (!price(cell(row, u"price"_s), &p)) {
            out.problems << (sized && !size.isEmpty() ? tr("Row %1: no price for %2 (%3)").arg(line).arg(name, size)
                                                      : tr("Row %1: no price for %2").arg(line).arg(name));
            continue;
        }
        // Another size of one already read.
        if (sized && byName.contains(name.toLower())) {
            ImportedItem &item = out.items[byName.value(name.toLower())];
            if (item.sizes.isEmpty())   // its first row had no size's name
                item.sizes << ImportedPrice{tr("Regular"), item.price};
            if (std::ranges::none_of(item.sizes, [&](const ImportedPrice &s) { return sameName(s.name, size); }))
                item.sizes << ImportedPrice{size.isEmpty() ? tr("Regular") : size, p};
            continue;
        }
        // The same item twice (Toast lists one once for each menu it's on): the first.
        if (!sized && seen.contains(name.toLower())) {
            if (!twice.contains(name, Qt::CaseInsensitive))
                twice << name;
            continue;
        }
        seen.insert(name.toLower());
        ImportedItem item;
        item.row = line;
        item.name = name;
        item.price = p;
        item.category = cell(row, u"category"_s);
        if (item.category.isEmpty())
            item.category = cell(row, u"menu"_s);
        if (out.from == u"Square"_s)   // "Burgers, Lunch": its first
            item.category = listOf(item.category).value(0);
        item.section = cell(row, u"subgroup"_s);
        item.description = cell(row, u"description"_s);
        item.onIt = cell(row, u"onIt"_s);
        item.photo = cell(row, u"photo"_s);
        item.barcode = cell(row, u"barcode"_s);
        item.number = cell(row, u"number"_s);
        item.sku = cell(row, u"sku"_s);
        // The cost only from a column of its own (not the one taken as the price).
        if (at.contains(u"cost"_s) && at.value(u"cost"_s) != at.value(u"price"_s))
            item.cost = cell(row, u"cost"_s).remove(QRegularExpression(u"[^0-9.,]"_s));
        item.onHand = cell(row, u"onHand"_s).trimmed();
        item.lowAt = cell(row, u"lowAt"_s).trimmed();
        item.par = cell(row, u"par"_s).trimmed();
        item.alcohol = yes(cell(row, u"alcohol"_s));
        item.groups = listOf(cell(row, u"groups"_s));
        for (const auto &[i, group] : modifierSets)
            if (i < row.size() && yes(row[i]) && !item.groups.contains(group, Qt::CaseInsensitive))
                item.groups << group;
        if (sized && !size.isEmpty())
            item.sizes << ImportedPrice{size, p};
        if (sized)
            byName.insert(name.toLower(), int(out.items.size()));
        out.items << item;
    }
    // Said first: they're most of what's left out.
    if (!twice.isEmpty())
        out.problems.prepend(tr("In the file twice (the first is kept): %1").arg(twice.join(u", "_s)));
    if (options > 0)
        out.problems.prepend(tr("%n modifier option(s) left out: the file doesn't say which items they go with. "
                           "Make them in Choice Groups.", options));
    return out;
}

// Clover's Modifier Groups sheet: a row an option (the group's name on its
// first row, or each), or a row a group with its options in one cell.
QList<ImportedGroup> readGroupSheet(const QList<QStringList> &rows)
{
    QList<ImportedGroup> groups;
    if (rows.isEmpty())
        return groups;
    int group = -1, option = -1, price = -1, min = -1, max = -1;
    for (int i = 0; i < rows[0].size(); ++i) {
        const QString h = plain(rows[0][i]);
        if (group < 0 && QStringList{u"modifier group name"_s, u"modifier group"_s, u"group name"_s, u"modifier set"_s, u"modifier set name"_s}.contains(h))
            group = i;
        else if (option < 0 && QStringList{u"modifier"_s, u"modifier name"_s, u"modifiers"_s, u"option"_s, u"option name"_s, u"options"_s}.contains(h))
            option = i;
        else if (price < 0 && QStringList{u"price"_s, u"modifier price"_s, u"option price"_s}.contains(h))
            price = i;
        else if (min < 0 && QStringList{u"min required"_s, u"minimum"_s, u"min"_s, u"min selections"_s, u"minimum required"_s}.contains(h))
            min = i;
        else if (max < 0 && QStringList{u"max allowed"_s, u"maximum"_s, u"max"_s, u"max selections"_s, u"maximum allowed"_s}.contains(h))
            max = i;
    }
    if (group < 0 || option < 0)
        return groups;
    const auto at = [](const QStringList &r, int i) { return i >= 0 && i < r.size() ? r[i].trimmed() : QString(); };
    for (int r = 1; r < rows.size(); ++r) {
        const QStringList &row = rows[r];
        if (const QString name = at(row, group); !name.isEmpty()) {
            const auto it = std::ranges::find_if(groups, [&](const ImportedGroup &g) { return sameName(g.name, name); });
            if (it == groups.end()) {
                ImportedGroup g;
                g.name = name;
                g.max = 0;   // any number, unless it says
                groups << g;
            }
        }
        if (groups.isEmpty())
            continue;
        ImportedGroup &g = groups.last();
        bool ok = false;
        if (const int n = at(row, min).toInt(&ok); ok)
            g.min = n;
        if (const int n = at(row, max).toInt(&ok); ok)
            g.max = n;
        const QString text = at(row, option);
        double p = 0;
        if (price >= 0 && !text.isEmpty()) {
            app::price(at(row, price), &p);   // empty: free
            g.options << ImportedPrice{text, p};
        } else {
            // "Bacon ($1.50), Cheddar, Avocado 2.00"
            static const QRegularExpression priced(u"^(.*?)\\s*\\(?\\s*[+$€£]?\\s*(\\d+(?:[.,]\\d{1,2})?)\\s*\\)?$"_s);
            for (const QString &o : listOf(text)) {
                const auto m = priced.match(o);
                if (m.hasMatch() && !m.captured(1).isEmpty() && app::price(m.captured(2), &p))
                    g.options << ImportedPrice{m.captured(1), p};
                else
                    g.options << ImportedPrice{o, 0};
            }
        }
    }
    return groups;
}

} // namespace

MenuImport readMenuCsv(const QString &text)
{
    const QString first = text.section(u'\n', 0, 0);
    const QChar sep = first.count(u'\t') > 0 ? u'\t'
                      : first.count(u';') > first.count(u',') ? u';' : u',';
    MenuImport out = readTable(rowsOf(text, sep));
    finish(out);
    return out;
}

MenuImport readMenuSheets(const QList<Sheet> &sheets)
{
    // The items: the sheet called Items, else the one with the most.
    MenuImport out;
    int itemsSheet = -1;
    for (int i = 0; i < sheets.size(); ++i) {
        if (QStringList{u"items"_s, u"item"_s, u"menu"_s, u"articulos"_s, u"productos"_s}.contains(plain(sheets[i].name))) {
            out = readTable(sheets[i].rows);
            itemsSheet = i;
            break;
        }
    }
    if (itemsSheet < 0)
        for (int i = 0; i < sheets.size(); ++i)
            if (MenuImport m = readTable(sheets[i].rows); itemsSheet < 0 || m.items.size() > out.items.size()) {
                out = m;
                itemsSheet = i;
            }
    // The other sheets: categories (by item), choice groups.
    for (int i = 0; i < sheets.size(); ++i) {
        if (i == itemsSheet || sheets[i].rows.isEmpty())
            continue;
        const QList<QStringList> &rows = sheets[i].rows;
        if (const QList<ImportedGroup> groups = readGroupSheet(rows); !groups.isEmpty()) {
            for (const ImportedGroup &g : groups)
                if (std::ranges::none_of(out.groups, [&](const ImportedGroup &x) { return sameName(x.name, g.name); }))
                    out.groups << g;
            continue;
        }
        int category = -1, items = -1;
        for (int c = 0; c < rows[0].size(); ++c) {
            const QString h = plain(rows[0][c]);
            if (category < 0 && QStringList{u"category name"_s, u"category"_s, u"name"_s, u"categoria"_s}.contains(h))
                category = c;
            else if (items < 0 && QStringList{u"items in category"_s, u"items"_s, u"item name"_s, u"item"_s}.contains(h))
                items = c;
        }
        if (category < 0 || items < 0)
            continue;
        QHash<QString, QString> categoryOf;
        for (int r = 1; r < rows.size(); ++r) {
            const QString name = rows[r].value(category).trimmed();
            for (const QString &item : listOf(rows[r].value(items)))
                if (!name.isEmpty() && !categoryOf.contains(item.toLower()))
                    categoryOf.insert(item.toLower(), name);
        }
        for (ImportedItem &item : out.items)
            if (item.category.isEmpty())
                item.category = categoryOf.value(item.name.toLower());
    }
    finish(out);
    return out;
}

// --- Toast ---------------------------------------------------------------------------

namespace {

QJsonArray toastMenus(const QJsonDocument &doc)
{
    return doc.isArray() ? doc.array() : doc.object().value(u"menus"_s).toArray();
}

} // namespace

bool isToastMenu(const QJsonDocument &doc)
{
    const QJsonArray menus = toastMenus(doc);
    return !menus.isEmpty() && menus.first().toObject().contains(u"menuGroups"_s);
}

MenuImport readToastMenu(const QJsonDocument &doc)
{
    MenuImport out;
    out.from = u"Toast"_s;
    const QJsonObject groupRefs = doc.object().value(u"modifierGroupReferences"_s).toObject();
    const QJsonObject optionRefs = doc.object().value(u"modifierOptionReferences"_s).toObject();
    const auto ref = [](const QJsonObject &refs, const QJsonValue &v) {
        if (v.isObject())
            return v.toObject();
        return refs.value(v.isString() ? v.toString() : QString::number(v.toInteger())).toObject();
    };
    QHash<QString, QString> nameOfRef;   // a group read once, by its reference
    // A group's name here: two of one name (a drink's Size, a pizza's) kept apart.
    const auto groupName = [&](const QJsonValue &v) -> QString {
        const QString key = v.isObject() ? QString() : v.toVariant().toString();
        if (!key.isEmpty() && nameOfRef.contains(key))
            return nameOfRef.value(key);
        const QJsonObject o = ref(groupRefs, v);
        ImportedGroup g;
        g.name = o.value(u"name"_s).toString().simplified();
        g.min = std::max(0, o.value(u"minSelections"_s).toInt());
        g.max = o.value(u"maxSelections"_s).isDouble() ? std::max(0, o.value(u"maxSelections"_s).toInt()) : 0;
        QJsonArray options = o.value(u"modifierOptionReferences"_s).toArray();
        for (const char *k : {"modifierOptions", "options", "items"})
            for (const QJsonValue &x : o.value(QLatin1String(k)).toArray())
                options.append(x);
        for (const QJsonValue &x : options) {
            const QJsonObject opt = ref(optionRefs, x);
            if (const QString name = opt.value(u"name"_s).toString().simplified(); !name.isEmpty())
                g.options << ImportedPrice{name, jsonPrice(opt.value(u"price"_s))};
        }
        if (g.name.isEmpty() || g.options.isEmpty())
            return {};
        const auto same = [&](const ImportedGroup &x) {
            return x.min == g.min && x.max == g.max && x.options.size() == g.options.size()
                   && std::ranges::equal(x.options, g.options, [](const ImportedPrice &a, const ImportedPrice &b) {
                          return a.name == b.name && a.price == b.price;
                      });
        };
        QString name = g.name;
        for (int n = 2;; ++n) {
            const auto it = std::ranges::find_if(out.groups, [&](const ImportedGroup &x) { return sameName(x.name, name); });
            if (it == out.groups.end()) {
                g.name = name;
                out.groups << g;
                break;
            }
            if (same(*it))
                break;
            name = u"%1 %2"_s.arg(g.name).arg(n);
        }
        if (!key.isEmpty())
            nameOfRef.insert(key, name);
        return name;
    };
    QHash<QString, bool> seen;
    const std::function<void(const QJsonObject &, const QString &, const QString &)> walk =
        [&](const QJsonObject &group, const QString &category, const QString &section) {
            for (const QJsonValue &v : group.value(u"menuItems"_s).toArray()) {
                const QJsonObject o = v.toObject();
                ImportedItem item;
                item.name = o.value(u"name"_s).toString().simplified();
                if (item.name.isEmpty() || seen.contains(item.name.toLower()))
                    continue;   // on two menus (Lunch, Dinner): once
                item.category = category;
                item.section = section;
                item.description = o.value(u"description"_s).toString().simplified();
                item.number = o.value(u"plu"_s).toVariant().toString().trimmed();
                QJsonArray groups = o.value(u"modifierGroupReferences"_s).toArray();
                for (const char *k : {"modifierGroups", "optionGroups"})
                    for (const QJsonValue &x : o.value(QLatin1String(k)).toArray())
                        groups.append(x);
                for (const QJsonValue &g : groups)
                    if (const QString name = groupName(g); !name.isEmpty() && !item.groups.contains(name))
                        item.groups << name;
                if (o.value(u"price"_s).isDouble()) {
                    item.price = jsonPrice(o.value(u"price"_s));
                } else {
                    // Priced by its choices (a size), else not at all.
                    const bool byChoice = std::ranges::any_of(item.groups, [&](const QString &n) {
                        const auto g = std::ranges::find_if(out.groups, [&](const ImportedGroup &x) { return x.name == n; });
                        return g != out.groups.end() && g->min >= 1
                               && std::ranges::any_of(g->options, [](const ImportedPrice &p) { return p.price > 0; });
                    });
                    if (!byChoice) {
                        out.problems << tr("No price for %1").arg(item.name);
                        continue;
                    }
                }
                seen.insert(item.name.toLower(), true);
                out.items << item;
            }
            for (const QJsonValue &v : group.value(u"menuGroups"_s).toArray()) {
                const QJsonObject sub = v.toObject();
                walk(sub, category, sub.value(u"name"_s).toString().simplified());
            }
        };
    for (const QJsonValue &m : toastMenus(doc))
        for (const QJsonValue &g : m.toObject().value(u"menuGroups"_s).toArray())
            walk(g.toObject(), g.toObject().value(u"name"_s).toString().simplified(), {});
    out.columns = QStringList{u"name"_s, u"price"_s, u"category"_s, u"description"_s, u"groups"_s};
    finish(out);
    return out;
}

// --- VT Ai ------------------------------------------------------------------------------

namespace {

// Spacing tidied, and no longer than `most` (a model can ramble).
QString tidy(const QString &text, int most)
{
    QString t = text.simplified();
    if (t.size() > most)
        t = t.left(most).trimmed();
    return t;
}

// A price it can be: a number, 0 to 99,999.99, to the cent; -1: none.
double priceOf(const QJsonValue &v)
{
    if (!v.isDouble())
        return -1;
    const double p = v.toDouble();
    if (!std::isfinite(p) || p < 0 || p >= 100000)
        return -1;
    return std::round(p * 100) / 100;
}

// One required (a temperature, a sauce, a side, a size, a milk, a dressing),
// else any number (toppings, extras, add-ons).
bool pickOne(const QString &group)
{
    static const QRegularExpression one(
        uR"(\b(temp|temperature|cooked|doneness|side|sauce|size|milk|dressing|bread|tortilla|crust|broth|protein|meat|choice|style|base|rice|flavor|spice)s?\b)"_s,
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression many(uR"(\b(add|extra|topping|option|modifier)s?\b)"_s, QRegularExpression::CaseInsensitiveOption);
    return one.match(group).hasMatch() && !many.match(group).hasMatch();
}

} // namespace

MenuImport readVtAiMenu(const QList<QJsonObject> &answers)
{
    const auto tr = [](const char *text) { return QCoreApplication::translate("MenuImport", text); };
    MenuImport out;
    out.from = u"VT Ai"_s;
    QHash<QString, int> itemAt;    // lower-case name -> its index
    QHash<QString, int> groupAt;
    int row = 0;
    for (const QJsonObject &answer : answers) {
        // The groups first: the items say which they ask for.
        for (const QJsonValue &gv : answer.value(u"groups").toArray()) {
            const QJsonObject g = gv.toObject();
            ImportedGroup group;
            group.name = tidy(g.value(u"name").toString(), 40);
            if (group.name.isEmpty())
                continue;
            QSet<QString> seen;
            for (const QJsonValue &ov : g.value(u"options").toArray()) {
                const QJsonObject o = ov.toObject();
                const QString name = tidy(o.value(u"name").toString(), 40);
                const double price = priceOf(o.value(u"price"));
                if (name.isEmpty() || seen.contains(name.toLower()) || group.options.size() >= 30)
                    continue;
                seen.insert(name.toLower());
                group.options.append({name, std::max(0.0, price)});
            }
            if (group.options.isEmpty())
                continue;
            group.min = pickOne(group.name) ? 1 : 0;
            group.max = pickOne(group.name) ? 1 : 0;
            if (const auto it = groupAt.constFind(group.name.toLower()); it != groupAt.cend()) {
                // The same group from another part of the menu: its options added.
                ImportedGroup &was = out.groups[*it];
                for (const ImportedPrice &o : std::as_const(group.options))
                    if (std::ranges::none_of(was.options, [&](const ImportedPrice &x) { return x.name.compare(o.name, Qt::CaseInsensitive) == 0; }))
                        was.options.append(o);
                continue;
            }
            groupAt.insert(group.name.toLower(), int(out.groups.size()));
            out.groups.append(group);
        }
        for (const QJsonValue &iv : answer.value(u"items").toArray()) {
            const QJsonObject i = iv.toObject();
            ImportedItem item;
            item.row = ++row;
            item.name = tidy(i.value(u"name").toString(), 60);
            if (item.name.isEmpty())
                continue;
            item.category = tidy(i.value(u"category").toString(), 40);
            item.description = tidy(i.value(u"description").toString(), 300);
            for (const QJsonValue &sv : i.value(u"sizes").toArray()) {
                const QJsonObject sz = sv.toObject();
                const QString name = tidy(sz.value(u"name").toString(), 30);
                const double price = priceOf(sz.value(u"price"));
                if (!name.isEmpty() && price >= 0 && item.sizes.size() < 12
                    && std::ranges::none_of(item.sizes, [&](const ImportedPrice &x) { return x.name.compare(name, Qt::CaseInsensitive) == 0; }))
                    item.sizes.append({name, price});
            }
            if (item.sizes.size() < 2)   // one size is the item's price
                item.sizes.clear();
            double price = priceOf(i.value(u"price"));
            if (!item.sizes.isEmpty())
                price = std::ranges::min(item.sizes, {}, &ImportedPrice::price).price;
            if (price < 0) {
                price = 0;
                out.problems << tr("No price for %1: type it in the preview, or after.").arg(item.name);
            }
            item.price = price;
            for (const QJsonValue &gv : i.value(u"groups").toArray()) {
                const auto it = groupAt.constFind(tidy(gv.toString(), 40).toLower());
                if (it != groupAt.cend() && !item.groups.contains(out.groups[*it].name))
                    item.groups << out.groups[*it].name;   // (one it named but didn't give: left out)
            }
            if (const auto it = itemAt.constFind(item.name.toLower()); it != itemAt.cend()) {
                out.problems << tr("%1 was there twice: once kept.").arg(item.name);
                continue;
            }
            itemAt.insert(item.name.toLower(), int(out.items.size()));
            out.items.append(item);
        }
    }
    if (out.items.isEmpty())
        out.problems << tr("VT Ai found no items in it.");
    return out;
}

QString withoutInstructions(const QString &text, QStringList *removed)
{
    static const QRegularExpression orders(
        u"\\b(ignore|disregard|forget)\\b.{0,40}\\b(instruction|rule|prompt|above|previous)|"
        "\\b(system|assistant|developer)\\s*:|\\byou are (now )?(an? |in )|\\badmin mode\\b|\\bjailbreak|"
        "\\bnew instructions?\\b|\\bprompt\\b|\\b(ignora|olvida)\\b.{0,40}\\b(instruccion|regla)"_s,
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    QStringList kept;
    for (const QString &line : text.split(u'\n')) {
        if (orders.match(line).hasMatch()) {
            if (removed)
                *removed << line.simplified().left(120);
            continue;
        }
        kept << line;
    }
    return kept.join(u'\n');
}

namespace {

// Words for comparing: lower case, no accents, letters and digits only.
QStringList plainWords(const QString &text)
{
    QString t = text.normalized(QString::NormalizationForm_D).toLower();
    QString out;
    out.reserve(t.size());
    for (const QChar c : t) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        out += c.isLetterOrNumber() ? c : u' ';
    }
    return out.split(u' ', Qt::SkipEmptyParts);
}

} // namespace

void keepToSource(MenuImport &read, const QString &source)
{
    const QStringList words = plainWords(source);
    const QSet<QString> have(words.begin(), words.end());
    // A word of the name is there as it is, or as the menu's singular/plural,
    // or (4 letters or more) as the start of one of its words ("Burgers").
    const auto there = [&](const QString &w) {
        if (have.contains(w) || have.contains(w + u's') || (w.endsWith(u's') && have.contains(w.chopped(1))))
            return true;
        if (w.size() >= 4)
            for (const QString &h : words)
                if (h.size() >= 4 && (h.startsWith(w) || w.startsWith(h)))
                    return true;
        return false;
    };
    // Most of a name's words (all of a short one's) in the menu.
    const auto named = [&](const QString &name) {
        int all = 0, found = 0;
        for (const QString &w : plainWords(name)) {
            if (w.size() < 3 && !w.front().isDigit())
                continue;   // (of, a, la)
            ++all;
            found += there(w) ? 1 : 0;
        }
        return all == 0 || (all <= 2 ? found == all : found * 3 >= all * 2);
    };
    // A price as the menu could write it: 3.50, 3.5, 3,50, 3 (3.00), .75.
    const QString flat = QString(source).replace(u',', u'.');
    const auto priced = [&](double p) {
        if (p <= 0)
            return true;   // none: nothing to check
        QStringList ways{QString::number(p, 'f', 2), QString::number(p, 'f', 1)};
        if (std::abs(p - std::round(p)) < 0.001)
            ways << QString::number(qRound64(p));
        if (p < 1)
            ways << QString::number(p, 'f', 2).mid(1);   // ".75"
        for (const QString &w : ways) {
            const QRegularExpression at(u"(?<![0-9.])"_s + QRegularExpression::escape(w) + u"(?![0-9])"_s);
            if (at.match(flat).hasMatch())
                return true;
        }
        return false;
    };
    QList<ImportedItem> kept;
    for (ImportedItem it : read.items) {
        if (!named(it.name)) {
            // (What was said about it before goes with it.)
            read.problems.removeAll(tr("No price for %1: type it in the preview, or after.").arg(it.name));
            read.problems.removeAll(tr("%1 was there twice: once kept.").arg(it.name));
            read.problems << tr("VT Ai named %1, which isn't in the menu's words: left out.").arg(it.name);
            continue;
        }
        if (!it.category.isEmpty() && !named(it.category))
            it.category.clear();   // (into the category chosen in the preview)
        QList<ImportedPrice> sizes;
        for (const ImportedPrice &z : it.sizes)
            if (priced(z.price))
                sizes << z;
        if (sizes.size() != it.sizes.size())
            read.problems << tr("Some of %1's sizes had prices that aren't on the menu: left out.").arg(it.name);
        it.sizes = sizes.size() >= 2 ? sizes : QList<ImportedPrice>();
        if (!it.sizes.isEmpty()) {
            it.price = it.sizes.front().price;
            for (const ImportedPrice &z : it.sizes)
                it.price = std::min(it.price, z.price);
        } else if (!priced(it.price)) {
            read.problems << tr("%1's price (%2) isn't on the menu: type it in the preview.").arg(it.name).arg(it.price, 0, 'f', 2);
            it.price = 0;
        }
        kept << it;
    }
    read.items = kept;
    for (ImportedGroup &g : read.groups) {
        QList<ImportedPrice> options;
        for (ImportedPrice o : g.options) {
            if (!named(o.name))
                continue;
            if (!priced(o.price))
                o.price = 0;
            options << o;
        }
        g.options = options;
    }
    read.groups.erase(std::remove_if(read.groups.begin(), read.groups.end(), [](const ImportedGroup &g) { return g.options.isEmpty(); }),
                      read.groups.end());
    // Items asking for a group that's gone: not any more.
    for (ImportedItem &it : read.items)
        it.groups.erase(std::remove_if(it.groups.begin(), it.groups.end(), [&](const QString &name) {
                            return std::none_of(read.groups.begin(), read.groups.end(), [&](const ImportedGroup &g) { return g.name == name; });
                        }),
                        it.groups.end());
}

QJsonObject parseVtAiLines(const QString &text)
{
    // "3.50", "$3.50", "12" -> a number; "?" or nonsense -> null.
    const auto money = [](QString t) -> QJsonValue {
        t = t.trimmed();
        if (t.startsWith(u'$'))
            t.remove(0, 1);
        bool ok = false;
        const double v = t.toDouble(&ok);
        return ok ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
    };
    // "10\"=11; 14\"=15" -> [{name, price}]
    const auto pairs = [&](const QString &cell) {
        QJsonArray out;
        for (const QString &part : cell.split(u';', Qt::SkipEmptyParts)) {
            const qsizetype eq = part.lastIndexOf(u'=');
            if (eq <= 0)
                continue;
            const QJsonValue price = money(part.mid(eq + 1));
            const QString name = part.left(eq).trimmed();
            if (!name.isEmpty() && price.isDouble())
                out.append(QJsonObject{{u"name"_s, name}, {u"price"_s, price}});
        }
        return out;
    };
    QJsonArray items, groups;
    QString category;
    for (const QString &raw : text.split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.size() < 3 || line[1] != u' ')
            continue;
        const QChar kind = line[0];
        const QString rest = line.mid(2).trimmed();
        const QStringList cells = rest.split(u'|');
        const auto cell = [&](qsizetype i) { return i < cells.size() ? cells[i].trimmed() : QString(); };
        if (kind == u'C') {
            category = rest;
        } else if (kind == u'I' && !cell(0).isEmpty()) {
            QJsonArray names;
            for (const QString &g : cell(3).split(u';', Qt::SkipEmptyParts))
                if (!g.trimmed().isEmpty())
                    names.append(g.trimmed());
            items.append(QJsonObject{{u"name"_s, cell(0)}, {u"price"_s, money(cell(1))}, {u"category"_s, category},
                                     {u"sizes"_s, pairs(cell(2))}, {u"groups"_s, names}});
        } else if (kind == u'G' && !cell(0).isEmpty()) {
            QJsonArray options;
            for (const QString &part : cell(1).split(u';', Qt::SkipEmptyParts)) {
                const qsizetype eq = part.lastIndexOf(u'=');
                const QString name = (eq > 0 ? part.left(eq) : part).trimmed();
                const QJsonValue price = eq > 0 ? money(part.mid(eq + 1)) : QJsonValue(0);
                if (!name.isEmpty())
                    options.append(QJsonObject{{u"name"_s, name}, {u"price"_s, price.isDouble() ? price : QJsonValue(0)}});
            }
            groups.append(QJsonObject{{u"name"_s, cell(0)}, {u"options"_s, options}});
        }
    }
    return {{u"items"_s, items}, {u"groups"_s, groups}};
}

QVariantMap importRowsOf(const MenuImport &read)
{
    QVariantList items;
    for (const ImportedItem &i : read.items) {
        QVariantMap row{{u"row"_s, i.row}, {u"name"_s, i.name}, {u"price"_s, i.price}, {u"category"_s, i.category},
                        {u"description"_s, i.description}, {u"onIt"_s, i.onIt}, {u"barcodes"_s, i.barcode}};
        if (!i.groups.isEmpty())
            row.insert(u"groups"_s, i.groups);
        if (!i.sizes.isEmpty()) {
            QVariantList sizes;
            for (const ImportedPrice &s : i.sizes)
                sizes.append(QVariantMap{{u"name"_s, s.name}, {u"price"_s, s.price}});
            row.insert(u"sizes"_s, sizes);
        }
        items.append(row);
    }
    QVariantList groups;
    for (const ImportedGroup &g : read.groups) {
        QVariantList options;
        for (const ImportedPrice &o : g.options)
            options.append(QVariantMap{{u"name"_s, o.name}, {u"price"_s, QString::number(o.price, 'f', 2)}});
        groups.append(QVariantMap{{u"name"_s, g.name}, {u"min"_s, g.min}, {u"max"_s, g.max}, {u"options"_s, options}});
    }
    return {{u"items"_s, items}, {u"groups"_s, groups}, {u"problems"_s, read.problems}, {u"columns"_s, QStringList()},
            {u"photos"_s, 0}, {u"from"_s, read.from}};
}

} // namespace vt::app

