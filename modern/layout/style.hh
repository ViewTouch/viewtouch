#pragma once

#include <QJsonObject>

namespace vt::layout {

enum class ZoneState { Normal, Selected, Disabled };

// Per-state appearance. Each state is a sparse JSON object; a missing key is
// inherited from the next level (zone -> page -> templates -> theme kind ->
// theme -> resolved normal state). Keys the renderer understands:
//
//   fill        color        face color
//   texture     string       texture name (qrc:/textures/<name>.xpm)
//   textColor   color
//   font        string       font family
//   fontSize    number       pixels on the logical canvas
//   bold        bool
//   textStyle   string       "none" | "embossed" | "outline"
//   frame       string       "raised" | "inset" | "border" | "flat" | "none"
//   frameWidth  number
//   borderColor color        used by frame "border"
//   radius      number       corner radius for shape "rect"
//   shadow      number       drop shadow offset
//   opacity     number       0..1
//
// Unknown keys are preserved so newer files survive a round trip.
struct Style {
    QJsonObject normal;
    QJsonObject selected;
    QJsonObject disabled;
    QJsonObject extra;

    const QJsonObject &state(ZoneState s) const;
    QJsonObject &state(ZoneState s);
    bool isEmpty() const;

    static Style fromJson(const QJsonObject &o);
    QJsonObject toJson() const;

    // Copy every key of `fallback` that `into` does not already define.
    static void mergeMissing(QJsonObject &into, const QJsonObject &fallback);

    bool operator==(const Style &) const = default;
};

} // namespace vt::layout
