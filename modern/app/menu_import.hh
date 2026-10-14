#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace vt::app {

// One menu item read from a spreadsheet saved as CSV (or a tab- or
// semicolon-separated file).
struct ImportedItem {
    int row = 0;               // its line in the file (1 = the first)
    QString name;
    double price = 0;
    QString category;          // empty: the one chosen
    QString description;
    QString onIt;              // what's on it, "lettuce, tomato"
    QString photo;             // a picture's file name, next to the spreadsheet
    QString barcode;           // the package's barcode(s), "012345678905"
};

struct MenuImport {
    QList<ImportedItem> items;
    QStringList problems;      // "Row 7: no price for Nachos"
    QStringList columns;       // what each column was taken to be
};

// Columns by their header (Name / Item, Price, Category / Section / Group,
// Description, Ingredients / On it, Photo; English or Spanish), else name, price,
// category in that order. Prices: "$3.50", "3,50", "3.50".
MenuImport readMenuCsv(const QString &text);

} // namespace vt::app
