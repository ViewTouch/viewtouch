#pragma once

#include "layout/style.hh"

#include <QJsonObject>
#include <QList>
#include <QRect>
#include <QString>

namespace vt::layout {

// One step a zone performs when touched. Stored as its full JSON object so
// action types added later round-trip untouched. Known types:
//
//   jump      { page | role, mode: push|replace|back|home|index|sequence }
//   addItem   { item, modifierSequence: [pageId...] }
//   qualifier { qualifier: no|extra|lite|side }
//   tender    { tender, amount? }
//   command   { name, args? }
//   setting   { key }
//   cycle     { states: [...] }
struct Action {
    QJsonObject data;

    QString type() const { return data.value(u"type").toString(); }
    QString str(QStringView key) const { return data.value(key).toString(); }

    bool operator==(const Action &) const = default;
};

// A placed element on a page: a button, label, image, or a widget panel
// (orderList, loginPad, ...). Replaces the legacy Zone/PosZone type zoo.
struct Zone {
    QString id;
    QString kind = QStringLiteral("button");
    QString name;
    QString label;
    QRect rect;
    int z = 0;
    QString shape = QStringLiteral("rect");      // rect|rounded|circle|diamond|hexagon|octagon
    QString behavior = QStringLiteral("blink");  // blink|toggle|select|double|passthrough|none
    Style style;
    QString hotkey;
    QString group;
    QString imagePath;
    bool enabled = true;
    QList<Action> actions;
    QJsonObject props;   // widget-specific settings
    QJsonObject extra;   // unknown keys, preserved

    static Zone fromJson(const QJsonObject &o);
    QJsonObject toJson() const;

    bool operator==(const Zone &) const = default;
};

// How far a zone is turned, clockwise in degrees, 0 to 359 (its "rotation";
// kept with the unknown keys so older layouts load unchanged). It turns
// about its middle; `rect` is where it sits before turning.
int rotationOf(const Zone &z);

} // namespace vt::layout
