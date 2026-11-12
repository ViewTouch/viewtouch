#pragma once

#include "app/legacy/legacy_import.hh"
#include "app/pos_service.hh"
#include "layout/layout.hh"

#include <QStringList>
#include <QVariantMap>

#include <functional>

// Putting a legacy store's menu and sales, worked out (legacy_import.hh),
// into a ViewTouch Modern store: through the running service (so every
// screen and the standby get them).
namespace vt::legacy {

struct Applied {
    QString error;          // nothing was changed when set before anything began
    QStringList done;       // what was brought over, one line each
};

// The menu (in place of the store's, on Modern's own menu screens: the
// layout's hand-built menu pages switched to self-filling ones) and the past
// days (only into a store with no sales yet). The caller saves the layout.
Applied applyImport(app::PosService &pos, layout::Layout *layout, const Imported &in, const ImportParts &parts);

// What the store's computer gives the import (PosShared::requestLegacyImport).
struct Request {
    std::function<layout::Layout()> pages;                          // the store's pages now
    std::function<QString(const layout::Layout &)> savePages;       // saved (to every screen): an error, or empty
    std::function<QString(QString *error)> backup;                 // a backup made: where, or empty and why
    std::function<void()> flush;                                    // what's written, on the disk
};
// PosShared::requestLegacyImport: `folder` read and worked out ({preview}),
// or (apply, a backup first) brought into the store ({done, error, backup}).
// `parts`: {menu, history}, each true unless said.
QVariantMap request(app::PosShared &shared, const QString &folder, const QVariantMap &parts, bool apply,
                    const Request &how);

} // namespace vt::legacy
