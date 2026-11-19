#pragma once

// The starter sets a store begins with: a kind of store (a restaurant, a
// food truck, a shop: its settings, staff, printers and checklists) and a
// menu for it (seed/menus: a sushi bar's, a pizzeria's, a hardware store's,
// a bookstore's...). Food and a shop's goods never mix: a restaurant or a
// food truck starts with a food menu, a shop with a shop's.

#include "app/pos_service.hh"

#include <QList>
#include <QString>

#include <optional>

namespace vt::app {

// A menu in seed/menus (menu_templates.cpp reads it).
struct StarterPack {
    QString id;            // the file's name: "sushi", "hardware"
    QString name;
    QString kind;          // "food" or "retail"
    QString description;
    QString text;          // its lines
    int number = 0;        // a shop's: in its barcodes
    int categories = 0;
    int items = 0;
};
const QList<StarterPack> &starterPacks();
const StarterPack *starterPack(const QString &id);

// "restaurant", "foodTruck" or "retail", with a menu after a colon
// ("restaurant:sushi", "retail:hardware"); the kind alone: its own menu (a
// restaurant's café, a food truck's taqueria, a shop's apparel).
struct StarterSet {
    QString kind = QStringLiteral("restaurant");
    QString pack;   // "": the restaurant's own menu
};
std::optional<StarterSet> parseStarterSet(const QString &text, QString *error = nullptr);
QString defaultPack(const QString &kind);

// The settings, menu, staff and stock a store of that kind starts with.
std::optional<PosData> starterData(const StarterSet &set, QString *error = nullptr);

} // namespace vt::app
