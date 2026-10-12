#pragma once

#include "layout/style.hh"
#include "layout/zone.hh"

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QRect>
#include <QSize>
#include <QString>

namespace vt::layout {

// A screen. Pages are authored on a logical canvas that is scaled uniformly
// to the physical display, so one page serves every resolution.
struct Page {
    QString id;
    QString name;
    // login|tables|guestCount|index|items|modifier|settle|logout|manager|
    // kitchen|template|library|custom
    QString kind = QStringLiteral("custom");
    // Well-known entry point the app navigates to by name (replaces the
    // legacy negative page IDs such as -1 login, -20 settle).
    QString role;
    // Zones of the template page (and its templates) are drawn behind this
    // page's own zones, like the legacy parent pages -94..-99.
    QString templateId;
    QString mealPeriod;
    // Permission needed to open this page (e.g. "manager"); empty = anyone
    // logged in. Replaces the legacy password-jump.
    QString permission;
    // A version of another page for one kind of screen: `variantOf` is that
    // page's id, `formFactor` the screen ("phone"). Phones show it in place
    // of the page; everything else still refers to the page by its own id.
    QString variantOf;
    QString formFactor;
    // A version's: zones of that page left off this screen on purpose (the
    // rest not placed here are new there, and the page check says so).
    QStringList hiddenZones;
    // Phone templates: where a page without a phone version of its own gets
    // its buttons laid out (see layout/reflow.hh). Empty: none.
    QRect contentArea;
    QSize canvas{1920, 1080};
    int grid = 8;
    QJsonObject background;   // { fill, texture }; missing keys inherit
    Style style;              // page-level zone defaults
    QList<Zone> zones;        // bottom to top
    QJsonObject extra;

    const Zone *zone(const QString &zoneId) const;

    static Page fromJson(const QJsonObject &o);
    QJsonObject toJson() const;

    bool operator==(const Page &) const = default;
};

struct Theme {
    QString name;
    QJsonObject background;
    Style style;                  // defaults for every zone
    QMap<QString, Style> kinds;   // per-kind overrides, e.g. "label"
    QJsonObject extra;

    static Theme fromJson(const QJsonObject &o);
    QJsonObject toJson() const;

    bool operator==(const Theme &) const = default;
};

} // namespace vt::layout
