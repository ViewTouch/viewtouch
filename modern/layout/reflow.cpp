#include "layout/reflow.hh"

#include "layout/schema.hh"

#include <algorithm>
#include <numeric>

using namespace Qt::StringLiterals;

namespace vt::layout {

namespace {

enum class Kind { Hidden, Label, Panel, Button };

Kind kindOf(const Zone &z)
{
    // Notes, and a floor plan's walls, doors and the rest (on a phone its
    // tables are laid out in rows instead).
    if (z.kind == u"comment" || z.kind == u"decor")
        return Kind::Hidden;
    if (z.kind == u"label")
        return Kind::Label;
    if (z.kind == u"button" || z.kind == u"image" || z.kind == u"table")
        return Kind::Button;
    return schema::isWidgetKind(z.kind) ? Kind::Panel : Kind::Button;
}

// Panels keep their shape scaled to the width, within sensible bounds
// (times `squeeze` when several together would run past the bottom).
int panelHeight(const Zone &z, const QRect &area, double squeeze = 1.0)
{
    const double scaled = z.rect.width() > 0 ? double(z.rect.height()) * area.width() / z.rect.width() : 400.0;
    return std::max(240, int(std::clamp(int(scaled), 240, std::max(240, area.height() * 2 / 3)) * squeeze));
}

struct Plan {
    QList<QRect> rects;
    int height = 0;
};

Plan layOut(const QList<const Zone *> &zones, const QList<int> &order, const QRect &area, int gap, int columns,
            int rowHeight, int labelHeight, double squeeze = 1.0)
{
    Plan plan;
    plan.rects.resize(zones.size());
    const int cellW = (area.width() - gap * (columns - 1)) / columns;
    int y = area.y();
    int col = 0;   // next free column of the current button row
    auto endRow = [&] {
        if (col > 0) {
            y += rowHeight + gap;
            col = 0;
        }
    };
    for (int i : order) {
        const Zone &z = *zones[i];
        switch (kindOf(z)) {
        case Kind::Hidden:
            break;
        case Kind::Label:
            endRow();
            plan.rects[i] = QRect(area.x(), y, area.width(), labelHeight);
            y += labelHeight + gap;
            break;
        case Kind::Panel: {
            endRow();
            const int h = panelHeight(z, area, squeeze);
            plan.rects[i] = QRect(area.x(), y, area.width(), h);
            y += h + gap;
            break;
        }
        case Kind::Button:
            plan.rects[i] = QRect(area.x() + col * (cellW + gap), y, cellW, rowHeight);
            if (++col == columns)
                endRow();
            break;
        }
    }
    if (col > 0)
        y += rowHeight;
    else if (y > area.y())
        y -= gap;
    plan.height = y - area.y();
    return plan;
}

} // namespace

QList<QRect> reflowZones(const QList<const Zone *> &zones, const QRect &area, int gap)
{
    // Reading order: rows first (zones whose tops are close count as one row).
    QList<int> order(zones.size());
    std::iota(order.begin(), order.end(), 0);
    std::ranges::stable_sort(order, [&](int a, int b) {
        const QRect &ra = zones[a]->rect;
        const QRect &rb = zones[b]->rect;
        if (std::abs(ra.y() - rb.y()) > 24)
            return ra.y() < rb.y();
        return ra.x() < rb.x();
    });

    // Room left over goes to the biggest panel (a list, a report, the
    // kitchen's orders): everything below it moves down.
    const auto growMain = [&](Plan &p) {
        const int spare = area.height() - p.height;
        int main = -1;
        for (int i = 0; i < zones.size(); ++i)
            if (kindOf(*zones[i]) == Kind::Panel && !p.rects[i].isEmpty()
                && (main < 0 || zones[i]->rect.width() * zones[i]->rect.height()
                                    > zones[main]->rect.width() * zones[main]->rect.height()))
                main = i;
        if (spare <= 0 || main < 0)
            return;
        const int below = p.rects[main].bottom();
        p.rects[main].setHeight(p.rects[main].height() + spare);
        for (int i = 0; i < zones.size(); ++i)
            if (i != main && !p.rects[i].isEmpty() && p.rects[i].top() > below)
                p.rects[i].translate(0, spare);
        p.height = area.height();
    };

    Plan best;
    for (int columns : {2, 3}) {
        for (int rowHeight : {220, 190, 160, 130, 110}) {
            Plan p = layOut(zones, order, area, gap, columns, rowHeight, rowHeight >= 160 ? 96 : 72);
            if (p.height <= area.height()) {
                growMain(p);
                return p.rects;
            }
            if (best.rects.isEmpty() || p.height < best.height)
                best = std::move(p);
        }
    }
    // Still too tall: the panels give up height together, as far as they can.
    for (double squeeze : {0.85, 0.7, 0.55, 0.4}) {
        Plan p = layOut(zones, order, area, gap, 3, 110, 72, squeeze);
        if (p.height <= area.height()) {
            growMain(p);
            return p.rects;
        }
        if (p.height < best.height)
            best = std::move(p);
    }
    return best.rects;   // doesn't fit: the most compact one (runs past the bottom)
}

PhoneView phoneView(const Layout &l, const QString &id, bool useVersion)
{
    const Page *page = l.page(id);
    PhoneView s{id, page ? page->canvas : QSize(1920, 1080), {}, {}};
    if (!page)
        return s;
    if (useVersion) {
        if (const Page *v = l.variantFor(id, u"phone"_s)) {
            s.pageId = v->id;
            s.canvas = v->canvas;
            s.zones = l.effectiveZones(v->id);
            return s;
        }
    }
    // A template with a phone version: it frames this page's own buttons.
    const QList<const Page *> chain = l.templateChain(id);
    for (qsizetype i = 1; i < chain.size(); ++i) {
        const Page *frame = l.variantFor(chain[i]->id, u"phone"_s);
        if (!frame || frame->contentArea.isEmpty())
            continue;
        s.pageId = frame->id;
        s.canvas = frame->canvas;
        s.zones = l.effectiveZones(frame->id);
        QList<Layout::PlacedZone> own;
        for (const Layout::PlacedZone &pz : l.effectiveZones(id)) {
            if (chain.indexOf(pz.owner) < i)   // this page, or templates below the framed one
                own.append(pz);
        }
        QList<const Zone *> zones;
        for (const Layout::PlacedZone &pz : own)
            zones.append(pz.zone);
        const QList<QRect> rects = reflowZones(zones, frame->contentArea);
        for (qsizetype k = 0; k < own.size(); ++k) {
            if (rects[k].isEmpty())
                continue;
            s.zones.append(own[k]);
            s.moved.insert(own[k].zone, rects[k]);
        }
        return s;
    }
    // No phone version and no phone frame: the page's own zones laid out
    // again on a phone held upright.
    static const QSize phone(1080, 2280);
    s.canvas = phone;
    const QList<Layout::PlacedZone> all = l.effectiveZones(id);
    QList<const Zone *> zones;
    for (const Layout::PlacedZone &pz : all)
        zones.append(pz.zone);
    const QList<QRect> rects = reflowZones(zones, QRect(24, 24, phone.width() - 48, phone.height() - 48));
    for (qsizetype k = 0; k < all.size(); ++k) {
        if (rects[k].isEmpty())
            continue;
        s.zones.append(all[k]);
        s.moved.insert(all[k].zone, rects[k]);
    }
    return s;
}

} // namespace vt::layout
