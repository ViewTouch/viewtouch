#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QList>
#include <QString>
#include <QStringList>

namespace vt::app {

struct Sheet;

// A size of an item, or an option of a choice group: "Large", 4.50.
struct ImportedPrice {
    QString name;
    double price = 0;
};

// One menu item read from a spreadsheet (CSV, a tab- or semicolon-separated
// file, or an Excel workbook) or another POS's menu export.
struct ImportedItem {
    int row = 0;               // its line in the file (1 = the first); 0: not from a sheet
    QString name;
    double price = 0;          // with sizes: the smallest
    QString category;          // empty: the one chosen
    QString section;           // a heading in its category ("Tacos")
    QString description;
    QString onIt;              // what's on it, "lettuce, tomato"
    QString photo;             // a picture's file name, next to the spreadsheet
    QString barcode;           // the package's barcode(s), "012345678905"
    QString number;            // its PLU, "104"
    QString sku;               // the store's own code for it (matches it again)
    // A shop's stock: what one costs, how many there are, when to warn
    // (as text, as typed; empty: not in the file).
    QString cost, onHand, lowAt, par;
    QList<ImportedPrice> sizes;   // two or more, each its own price
    QStringList groups;        // the choice groups it asks for, by name
    bool alcohol = false;
};

// A choice group from the file: "Temperature", one required.
struct ImportedGroup {
    QString name;
    int min = 0;
    int max = 1;               // 0: any number
    QList<ImportedPrice> options;
};

struct MenuImport {
    QList<ImportedItem> items;
    QList<ImportedGroup> groups;
    QStringList problems;      // "Row 7: no price for Nachos"
    QStringList columns;       // what each column was taken to be
    QString from;              // "Square", "Clover", "Toast"; empty: a spreadsheet
};

// Columns by their header (Name / Item, Price, Category / Section / Group,
// Description, Ingredients / On it, Photo, Barcode, PLU; English or Spanish),
// else name, price, category in that order. Prices: "$3.50", "3,50", "3.50".
// Square's item library (a row a variation: its sizes; Modifier Set columns)
// and Clover's items are read by their own headers.
MenuImport readMenuCsv(const QString &text);

// An Excel workbook's sheets: the one with the items (Clover's "Items"), its
// categories (Clover's "Categories") and choice groups ("Modifier Groups").
MenuImport readMenuSheets(const QList<Sheet> &sheets);

// Toast's menu export (its Menus API JSON, MenuExport.json): menus, their
// groups (categories; a group inside one is a section) and items, with their
// modifier groups.
bool isToastMenu(const QJsonDocument &doc);
MenuImport readToastMenu(const QJsonDocument &doc);

// VT Ai's answers (seed/vtai/menu.json's shape: {items: [{name, price,
// category, description, sizes, groups}], groups: [{name, options}]}), one
// a part of the menu, as an import: checked (names and prices that make
// sense, no item twice, groups that are there), cleaned (lengths, spacing),
// and what it couldn't be sure of in problems ("No price for French Toast").
MenuImport readVtAiMenu(const QList<QJsonObject> &answers);

// A menu's text with the lines that read like orders to an AI taken out
// ("Ignore all previous instructions", "SYSTEM:", "you are now..."): what
// VT Ai is given. Each line left out goes in `removed`.
QString withoutInstructions(const QString &text, QStringList *removed = nullptr);

// VT Ai's menu held to the menu's own words (`source`): an item, choice or
// category it names that isn't there is left out, a price that isn't there
// is none, a size whose price isn't there is dropped; each said in problems.
// (What a small AI makes up, or what text in the menu told it to.)
void keepToSource(MenuImport &read, const QString &source);

// VT Ai's answer in its line format (seed/vtai/menu-lines.json: "C Tacos",
// "I Carne Asada Taco | 3.50 | sizes | groups", "G Salsa | Roja=0; Verde=0",
// "END"), as the shape above. Lines it can't read are left out.
QJsonObject parseVtAiLines(const QString &text);

// An import as Menu Builder's preview takes it ({items, groups, problems,
// from}, each item {row, name, price, category, description, sizes, groups}).
QVariantMap importRowsOf(const MenuImport &read);

} // namespace vt::app
