#pragma once

#include "layout/layout.hh"

#include <QHash>
#include <QList>
#include <QRect>
#include <QSize>

namespace vt::layout {

// Lays a page's zones out again for a narrow (phone) screen, inside `area`,
// in reading order (top to bottom, left to right): labels and panels across
// the full width, buttons in a grid of large touch targets. The grid uses
// 2 columns, or 3 when that is what it takes to fit; rows shrink down to a
// minimum before anything is allowed to run past the bottom.
// Returns one rect per zone, in the order given. Notes (comments) get an
// empty rect: they are not shown.
QList<QRect> reflowZones(const QList<const Zone *> &zones, const QRect &area, int gap = 16);

// A page as a phone shows it: its phone version when it has one (`useVersion`);
// else, under a template with a phone version that frames pages (a content
// area), that frame with the page's own zones laid out in the area; else the
// page's zones laid out on a phone held upright. `moved`: where a zone laid
// out again goes (zones not shown there are left out of `zones`).
struct PhoneView {
    QString pageId;   // the page drawn: the version, the frame, or the page
    QSize canvas;
    QList<Layout::PlacedZone> zones;
    QHash<const Zone *, QRect> moved;
};
PhoneView phoneView(const Layout &layout, const QString &pageId, bool useVersion = true);

} // namespace vt::layout
