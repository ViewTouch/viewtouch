#pragma once

#include "app/legacy/legacy_files.hh"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>

#include <functional>

// Legacy pages as ViewTouch Modern pages (page JSON, as seed/legacy has
// them): each zone where legacy had it, looking as legacy drew it (frames,
// textures, colors, fonts, shapes, its drawing order), doing what Modern
// does for it; a button for something Modern doesn't do says so when
// touched ("notInModern"). The same conversion makes the Legacy style's
// built-in pages (`vtmodern --legacy-pages`) and a store's own when it is
// brought over from legacy.
namespace vt::legacy {

struct NotInModern {
    QString page;
    QString label;
    QString what;
};

struct PageSet {
    QList<QJsonObject> pages;   // in legacy id order
    QList<NotInModern> missing;
    int zones = 0;
};

struct PageOptions {
    // The store's page defaults (zone_db.dat's): the look a zone with
    // "default" frame, texture, color or font takes.
    PageDefaults defaults;
    // A legacy item's name -> what an item button adds in Modern (the
    // item's id). Empty: the legacy name as it is.
    std::function<QString(const QString &)> item;
    // Legacy pages there besides these (the Legacy style's system pages, by
    // legacy id): a page framed by one, or a button going to one, keeps it.
    QSet<int> knownPages;
};

// Legacy pages standing in for Modern's, by legacy id: the Legacy style
// shows them in that page's place (-1 login, -20 settle...).
const QHash<int, QString> &standsIn();

// Every page of `files` (later ones of the same id kept only when they're
// the 1920x1080 one), converted.
PageSet convertPages(const QList<const PageFile *> &files, const PageOptions &options);

// NOT-IN-MODERN.md: legacy's buttons with nothing behind them in Modern yet.
QString notInModernReport(const QList<NotInModern> &missing);

} // namespace vt::legacy
