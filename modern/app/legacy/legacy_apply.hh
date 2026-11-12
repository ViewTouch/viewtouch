#pragma once

#include "app/legacy/legacy_import.hh"
#include "app/pos_service.hh"
#include "layout/layout.hh"

#include <QStringList>
#include <QVariantMap>

#include <functional>

// Putting a legacy store, worked out (legacy_import.hh), into a ViewTouch
// Modern store: through the running service (so every screen and the
// standby get it) and into its pages.
namespace vt::legacy {

struct Applied {
    QString error;          // nothing was changed when set before anything began
    QStringList done;       // what was brought over, one line each
};

// The store's records, settings and past days, and (layout given) its pages
// and dining room, the store then in the Legacy style. Past days only into a
// store with no sales yet. The caller saves the layout.
Applied applyImport(app::PosService &pos, layout::Layout *layout, const Imported &in, const ImportParts &parts);

// Just the pages and dining room (a layout from a store's computer, say).
QStringList applyPages(layout::Layout &layout, const Imported &in, const ImportParts &parts);

// What the store's computer gives the import (PosShared::requestLegacyImport).
struct Request {
    std::function<layout::Layout()> pages;                          // the store's pages now
    std::function<QString(const layout::Layout &)> savePages;       // saved (to every screen): an error, or empty
    std::function<QString(QString *error)> backup;                 // a backup made: where, or empty and why
    std::function<void()> flush;                                    // what's written, on the disk
};
// PosShared::requestLegacyImport: `folder` read and worked out ({preview}),
// or (apply, a backup first) brought into the store ({done, error, backup}).
// `parts`: {pages, menu, tables, settings, staff, customers, history}, each
// true unless said.
QVariantMap request(app::PosShared &shared, const QString &folder, const QVariantMap &parts, bool apply,
                    const Request &how);

} // namespace vt::legacy
