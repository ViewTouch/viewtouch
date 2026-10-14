#include "app/menu_import.hh"

#include <QCoreApplication>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {

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

QString what(const QString &header)
{
    const QString h = header.trimmed().toLower().normalized(QString::NormalizationForm_KD).remove(QRegularExpression(u"[^a-z ]"_s));
    static const QList<std::pair<QString, QStringList>> names = {
        {u"name"_s, {u"name"_s, u"item"_s, u"item name"_s, u"product"_s, u"dish"_s, u"menu item"_s, u"nombre"_s, u"producto"_s, u"platillo"_s}},
        {u"price"_s, {u"price"_s, u"cost"_s, u"amount"_s, u"precio"_s, u"costo"_s}},
        {u"category"_s, {u"category"_s, u"categories"_s, u"family"_s, u"group"_s, u"section"_s, u"menu"_s, u"type"_s,
                         u"categoria"_s, u"seccion"_s, u"grupo"_s, u"tipo"_s}},
        {u"description"_s, {u"description"_s, u"desc"_s, u"details"_s, u"descripcion"_s}},
        {u"onIt"_s, {u"ingredients"_s, u"on it"_s, u"toppings"_s, u"whats on it"_s, u"comes with"_s, u"ingredientes"_s}},
        {u"photo"_s, {u"photo"_s, u"photos"_s, u"picture"_s, u"image"_s, u"photo file"_s, u"picture file"_s, u"image file"_s,
                      u"foto"_s, u"imagen"_s, u"fotografia"_s}},
        {u"barcode"_s, {u"barcode"_s, u"barcodes"_s, u"upc"_s, u"ean"_s, u"gtin"_s, u"codigo de barras"_s}},
    };
    for (const auto &[key, words] : names)
        if (words.contains(h))
            return key;
    return {};
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
    return ok && *out >= 0;
}

} // namespace

MenuImport readMenuCsv(const QString &text)
{
    MenuImport out;
    const QString first = text.section(u'\n', 0, 0);
    const QChar sep = first.count(u'\t') > 0 ? u'\t'
                      : first.count(u';') > first.count(u',') ? u';' : u',';
    QList<QStringList> rows = rowsOf(text, sep);
    // Which column is which: the header, else name, price, category.
    QHash<QString, int> at{{u"name"_s, 0}, {u"price"_s, 1}, {u"category"_s, 2}};
    int firstRow = 0;
    if (!rows.isEmpty()) {
        QHash<QString, int> found;
        for (int i = 0; i < rows[0].size(); ++i)
            if (const QString w = what(rows[0][i]); !w.isEmpty() && !found.contains(w))
                found.insert(w, i);
        if (found.contains(u"name"_s)) {
            at = found;
            firstRow = 1;
        }
    }
    for (const QString &key : {u"name"_s, u"price"_s, u"category"_s, u"description"_s, u"onIt"_s, u"photo"_s, u"barcode"_s})
        if (at.contains(key))
            out.columns << key;
    const auto cell = [&](const QStringList &r, const QString &key) {
        const int i = at.value(key, -1);
        return i >= 0 && i < r.size() ? r[i].trimmed() : QString();
    };
    for (int r = firstRow; r < rows.size(); ++r) {
        const QStringList &row = rows[r];
        if (std::all_of(row.begin(), row.end(), [](const QString &c) { return c.trimmed().isEmpty(); }))
            continue;
        ImportedItem item;
        item.row = r + 1;
        item.name = cell(row, u"name"_s);
        item.category = cell(row, u"category"_s);
        item.description = cell(row, u"description"_s);
        item.onIt = cell(row, u"onIt"_s);
        item.photo = cell(row, u"photo"_s);
        item.barcode = cell(row, u"barcode"_s);
        if (item.name.isEmpty()) {
            out.problems << QCoreApplication::translate("MenuImport", "Row %1: no name").arg(item.row);
            continue;
        }
        if (!price(cell(row, u"price"_s), &item.price)) {
            out.problems << QCoreApplication::translate("MenuImport", "Row %1: no price for %2").arg(item.row).arg(item.name);
            continue;
        }
        out.items << item;
    }
    return out;
}

} // namespace vt::app
