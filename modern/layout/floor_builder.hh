#pragma once

#include "layout/layout.hh"
#include "layout/zone.hh"

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace vt::layout::floorplan {

// A floor plan from a few answers (the Tables walk-through): which rooms,
// how many tables of each size in each, and how they're named. The tables
// stand in rows in each room; a bar's seats along its counter.
struct Room {
    QString id;        // dining | bar | patio
    int two = 0;       // tables for 2
    int four = 0;      // tables for 4
    int six = 0;       // tables for 6
    int seats = 0;     // seats at the counter (a bar)
};
struct Answers {
    QList<Room> rooms;   // in the order they're drawn: dining, bar, patio
    bool byRoom = false; // D1, B1, P1; else T1, T2... across rooms
};

// {rooms: [{id, two, four, six, seats}], byRoom}; unknown rooms left out,
// counts kept to 0..60.
Answers fromMap(const QVariantMap &answers);
// Every table's name, in order ("T1", ..., "Bar 1", ...).
QStringList names(const Answers &a);
int tableCount(const Answers &a);

// The rooms' tables and floors (decor "area" zones), in floor units: one
// room (`only`: its id) or all of them together (dining and bar side by
// side, the patio below).
struct Plan {
    QList<Zone> zones;
    QSize size;
};
Plan build(const Answers &a, const QString &only = {});
// The plan scaled evenly into `box` and centered in it (never more than
// 1.5 times its size: a few tables stay table-sized).
QList<Zone> fit(const Plan &plan, const QRect &box);

// The floor pages with the new plan: Main (the "tables" role) has every
// room, floor-dining / floor-bar / floor-patio their own; a room not chosen
// has no tables left (its plan tab goes). Each page's floor is where its
// tables and walls were. Their buttons stay. Returns how many pages changed.
int apply(Layout &layout, const Answers &a);

} // namespace vt::layout::floorplan
