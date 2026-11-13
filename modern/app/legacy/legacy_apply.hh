#pragma once

#include "app/legacy/legacy_import.hh"
#include "app/pos_service.hh"
#include "layout/layout.hh"

#include <QStringList>
#include <QVariantMap>

#include <any>
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

// What the store's computer gives the import.
struct Request {
    std::function<layout::Layout()> pages;                          // the store's pages now
    std::function<QString(const layout::Layout &)> savePages;       // saved (to every screen): an error, or empty
    std::function<QString(QString *error)> backup;                 // a backup made: where, or empty and why
    std::function<void()> flush;                                    // what's written, on the disk
};
// A dat folder read and worked out: on any thread (it touches nothing of the
// store; `settings` is a copy). `parts`: {menu, history}, each true unless said.
std::any read(const QString &folder, const QVariantMap &parts, const core::PosSettings &settings);
// On the store's thread: what `read` found ({preview}), or (apply, a backup
// first) bringing it into the store ({done, error, backup}).
QVariantMap finish(app::PosShared &shared, const std::any &read, bool apply, const Request &how);
// Both, as the store's way of bringing a legacy store over
// (PosShared::legacyRead, legacyFinish).
void install(app::PosShared &shared, Request how);

} // namespace vt::legacy
