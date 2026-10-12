#include "app/layout_editor.hh"

#include "layout/json_path.hh"
#include "layout/schema.hh"

#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <optional>
#include <limits>

using namespace Qt::StringLiterals;
using vt::layout::Layout;
using vt::layout::Page;
using vt::layout::Zone;

namespace vt::app {

// ---------------------------------------------------------------------------
// Undo command: the layout before and after one edit. Layout copies share
// unchanged pages and zones (Qt implicit sharing), so snapshots stay cheap.
// ---------------------------------------------------------------------------
class LayoutEditor::SnapshotCommand : public QUndoCommand {
public:
    SnapshotCommand(LayoutEditor *editor, const QString &text, Layout before, Layout after,
                    QString mergeKey)
        : QUndoCommand(text)
        , editor_(editor)
        , before_(std::move(before))
        , after_(std::move(after))
        , mergeKey_(std::move(mergeKey))
    {
    }

    void undo() override { editor_->setLayout(before_); }
    void redo() override { editor_->setLayout(after_); }
    int id() const override { return mergeKey_.isEmpty() ? -1 : 1; }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *o = static_cast<const SnapshotCommand *>(other);
        if (o->mergeKey_ != mergeKey_)
            return false;
        after_ = o->after_;
        return true;
    }

private:
    LayoutEditor *editor_;
    Layout before_;
    Layout after_;
    QString mergeKey_;
};

namespace {

constexpr int kMinZoneSize = 16;

Zone *findZone(Page &page, const QString &zoneId)
{
    for (Zone &z : page.zones) {
        if (z.id == zoneId)
            return &z;
    }
    return nullptr;
}

QRect clampToCanvas(QRect r, QSize canvas)
{
    const int w = std::clamp(r.width(), kMinZoneSize, std::max(kMinZoneSize, canvas.width()));
    const int h = std::clamp(r.height(), kMinZoneSize, std::max(kMinZoneSize, canvas.height()));
    const int x = std::clamp(r.x(), 0, std::max(0, canvas.width() - w));
    const int y = std::clamp(r.y(), 0, std::max(0, canvas.height() - h));
    return {x, y, w, h};
}

QString uniqueId(const QString &wanted, const std::function<bool(const QString &)> &taken)
{
    const QString base = wanted.isEmpty() ? u"item"_s : wanted;
    if (!taken(base))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = u"%1-%2"_s.arg(base).arg(n);
        if (!taken(candidate))
            return candidate;
    }
}

QString uniqueZoneId(const Page &page, const QString &wanted)
{
    return uniqueId(LayoutEditor::slugify(wanted),
                    [&](const QString &id) { return page.zone(id) != nullptr; });
}

QString uniquePageId(const Layout &layout, const QString &wanted)
{
    return uniqueId(LayoutEditor::slugify(wanted),
                    [&](const QString &id) { return layout.page(id) != nullptr; });
}

// Floor plan pieces ("decor" zones, props.decor): their usual size.
QSize decorSize(const QString &decor)
{
    static const QHash<QString, QSize> sizes = {
        {u"wall"_s, {480, 24}}, {u"window"_s, {240, 24}}, {u"door"_s, {120, 120}}, {u"counter"_s, {640, 96}},
        {u"booth"_s, {240, 96}}, {u"plant"_s, {96, 96}}, {u"pillar"_s, {64, 64}}, {u"fence"_s, {480, 24}},
        {u"entrance"_s, {200, 96}}, {u"area"_s, {480, 320}},
    };
    return sizes.value(decor, {480, 24});
}

QSize defaultZoneSize(const QString &kind)
{
    if (kind == u"label") return {480, 80};
    if (kind == u"image") return {240, 240};
    if (kind == u"table") return {200, 200};
    if (kind == u"comment") return {360, 120};
    if (layout::schema::isWidgetKind(kind)) return {480, 360};
    return {240, 120};
}

QString defaultZoneLabel(const QString &kind)
{
    if (kind == u"button") return u"New Button"_s;
    if (kind == u"label") return u"Label"_s;
    if (kind == u"image") return u"Image"_s;
    if (kind == u"comment") return u"Note"_s;
    return {};
}

// Zone JSON with default-valued keys present, so the inspector shows real
// values instead of "undefined". Style keys stay sparse: missing = inherit.
QJsonObject fullZoneJson(const Zone &z)
{
    QJsonObject o = z.toJson();
    const std::pair<const char16_t *, QJsonValue> defaults[] = {
        {u"label", u""_s}, {u"name", u""_s}, {u"hotkey", u""_s}, {u"group", u""_s},
        {u"imagePath", u""_s}, {u"enabled", true}, {u"z", 0}, {u"actions", QJsonArray()},
    };
    for (const auto &[key, value] : defaults) {
        if (!o.contains(QStringView(key)))
            o.insert(QStringView(key), value);
    }
    return o;
}

QJsonObject fullPageJson(const Page &p)
{
    QJsonObject o = p.toJson();
    for (const char16_t *key : {u"role", u"templateId", u"mealPeriod", u"permission"}) {
        if (!o.contains(QStringView(key)))
            o.insert(QStringView(key), u""_s);
    }
    return o;
}

QRect boundingBox(const QList<Zone *> &zones)
{
    QRect box;
    for (const Zone *z : zones)
        box = box.isNull() ? z->rect : box.united(z->rect);
    return box;
}

// Selected zones of a page, in the order given; unknown ids are skipped.
QList<Zone *> zonesOf(Page &page, const QStringList &ids)
{
    QList<Zone *> out;
    for (const QString &id : ids) {
        if (Zone *z = findZone(page, id))
            out.append(z);
    }
    return out;
}

bool templateLoops(const Layout &layout, const QString &pageId)
{
    const auto chain = layout.templateChain(pageId);
    return !chain.isEmpty() && !chain.last()->templateId.isEmpty() && layout.page(chain.last()->templateId);
}

} // namespace

LayoutEditor::LayoutEditor(Layout base)
    : layout_(std::move(base))
{
    undo_.setUndoLimit(200);
}

LayoutEditor::~LayoutEditor()
{
    // Commands point back at us; clear them before members go away.
    undo_.clear();
}

QString LayoutEditor::slugify(const QString &text)
{
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QString s = text.toLower().normalized(QString::NormalizationForm_KD);
    s.replace(nonWord, u"-"_s);
    while (s.startsWith(u'-')) s.remove(0, 1);
    while (s.endsWith(u'-')) s.chop(1);
    return s;
}

bool LayoutEditor::apply(const QString &text, const std::function<bool(Layout &)> &mutate,
                         const QString &mergeKey)
{
    Layout after = layout_;
    if (!mutate(after) || after == layout_)
        return false;
    undo_.push(new SnapshotCommand(this, text, layout_, std::move(after), mergeKey));
    return true;
}

void LayoutEditor::setLayout(const Layout &layout)
{
    layout_ = layout;
    if (changed_)
        changed_();
}

// --- zones -------------------------------------------------------------------

QString LayoutEditor::addZone(const QString &pageId, const QString &asked, QRect rect)
{
    const Page *page = layout_.page(pageId);
    if (!page || asked.isEmpty())
        return {};
    // "decor:wall": a floor plan piece of that sort.
    const bool isDecor = asked.startsWith(u"decor");
    const QString kind = isDecor ? u"decor"_s : asked;
    const QString decor = isDecor ? (asked.section(u':', 1).isEmpty() ? u"wall"_s : asked.section(u':', 1)) : QString();

    const int grid = std::max(1, page->grid);
    if (rect.isEmpty()) {
        const QSize size = isDecor ? decorSize(decor) : defaultZoneSize(kind);
        QPoint pos((page->canvas.width() - size.width()) / 2, (page->canvas.height() - size.height()) / 2);
        pos = {pos.x() / grid * grid, pos.y() / grid * grid};
        rect = QRect(pos, size);
        // In the free space nearest the middle (not on top of the tables);
        // zones from the template count too.
        std::vector<QRect> taken;
        for (const Layout::PlacedZone &pz : layout_.effectiveZones(pageId))
            if (pz.zone->props.value(u"decor").toString() != u"area")   // a room's floor is free to put things on
                taken.push_back(pz.zone->rect);
        const int step = std::max(grid, 8) * 2;
        std::optional<QRect> best;
        qint64 bestDistance = 0;
        for (int y = 0; y + size.height() <= page->canvas.height(); y += step) {
            for (int x = 0; x + size.width() <= page->canvas.width(); x += step) {
                const QRect candidate(QPoint(x, y), size);
                if (std::ranges::any_of(taken, [&](const QRect &r) { return r.intersects(candidate); }))
                    continue;
                const qint64 dx = candidate.center().x() - page->canvas.width() / 2;
                const qint64 dy = candidate.center().y() - page->canvas.height() / 2;
                if (!best || dx * dx + dy * dy < bestDistance) {
                    best = candidate;
                    bestDistance = dx * dx + dy * dy;
                }
            }
        }
        if (best)
            rect = *best;
        // No room: don't drop it exactly on top of an existing one.
        for (int tries = 0; tries < 32; ++tries) {
            const bool occupied = std::ranges::any_of(page->zones, [&](const Zone &z) {
                return z.rect.topLeft() == rect.topLeft();
            });
            if (!occupied)
                break;
            rect.translate(grid * 4, grid * 4);
        }
    }

    Zone z;
    z.id = uniqueZoneId(*page, kind);
    z.kind = kind;
    z.label = kind == u"table" ? layout_.nextTableLabel() : defaultZoneLabel(kind);
    if (kind == u"table") {
        z.shape = u"rounded"_s;
        z.props.insert(u"seats"_s, 4);
    }
    if (isDecor) {
        z.id = uniqueZoneId(*page, decor);
        z.props.insert(u"decor"_s, decor);
        z.label = decor == u"entrance" ? u"Entrance"_s : decor == u"area" ? u"Area"_s : QString();
    }
    z.behavior = (kind == u"button" || kind == u"image") ? u"blink"_s : u"none"_s;
    z.rect = clampToCanvas(rect, page->canvas);

    const QString id = z.id;
    const bool ok = apply(u"Add %1"_s.arg(isDecor ? decor : kind), [&](Layout &l) {
        // Walls and the rest go over the other pieces, under the tables; an
        // area (a room's floor) under everything.
        QList<Zone> &zones = l.page(pageId)->zones;
        if (!isDecor) {
            zones.append(z);
        } else if (decor == u"area") {
            zones.prepend(z);
        } else {
            const auto first = std::ranges::find_if(zones, [](const Zone &o) { return o.kind != u"decor"; });
            zones.insert(first - zones.begin(), z);
        }
        return true;
    });
    return ok ? id : QString();
}

QStringList LayoutEditor::duplicateZones(const QString &pageId, const QStringList &zoneIds)
{
    QStringList created;
    apply(u"Duplicate"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        const int step = std::max(1, page->grid) * 2;
        QList<Zone> sources;   // copy first: appending may reallocate page->zones
        for (const Zone *src : zonesOf(*page, zoneIds))
            sources.append(*src);
        for (Zone copy : sources) {
            copy.id = uniqueZoneId(*page, copy.id);
            copy.rect = clampToCanvas(copy.rect.translated(step, step), page->canvas);
            if (copy.kind == u"table")   // T7 -> T8: two zones can't be one table
                copy.label = l.nextTableLabel(copy.label);
            created.append(copy.id);
            page->zones.append(copy);
        }
        return !created.isEmpty();
    });
    return created;
}

bool LayoutEditor::deleteZones(const QString &pageId, const QStringList &zoneIds)
{
    return apply(zoneIds.size() == 1 ? u"Delete zone"_s : u"Delete %1 zones"_s.arg(zoneIds.size()),
                 [&](Layout &l) {
        Page *page = l.page(pageId);
        return page && page->zones.removeIf([&](const Zone &z) { return zoneIds.contains(z.id); }) > 0;
    });
}

bool LayoutEditor::setZoneRects(const QString &pageId, const QHash<QString, QRect> &rects)
{
    return apply(u"Move / resize"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        for (auto it = rects.begin(); it != rects.end(); ++it) {
            if (Zone *z = findZone(*page, it.key()))
                z->rect = clampToCanvas(it.value(), page->canvas);
        }
        return true;
    });
}

bool LayoutEditor::nudgeZones(const QString &pageId, const QStringList &zoneIds, int dx, int dy)
{
    return apply(u"Nudge"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        for (Zone *z : zonesOf(*page, zoneIds))
            z->rect = clampToCanvas(z->rect.translated(dx, dy), page->canvas);
        return true;
    }, u"nudge:%1:%2"_s.arg(pageId, zoneIds.join(u',')));
}

bool LayoutEditor::rotateZones(const QString &pageId, const QStringList &zoneIds, int degrees, bool by)
{
    return apply(u"Rotate"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        bool any = false;
        for (Zone *z : zonesOf(*page, zoneIds)) {
            const int r = (((by ? layout::rotationOf(*z) + degrees : degrees) % 360) + 360) % 360;
            if (r == layout::rotationOf(*z))
                continue;
            if (r == 0)
                z->extra.remove(u"rotation"_s);
            else
                z->extra.insert(u"rotation"_s, r);
            any = true;
        }
        return any;
    });
}

bool LayoutEditor::bringToFront(const QString &pageId, const QStringList &zoneIds)
{
    return apply(u"Bring to front"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page || page->zones.isEmpty())
            return false;
        int top = std::numeric_limits<int>::min();
        for (const Zone &z : page->zones)
            top = std::max(top, z.z);
        QList<Zone> moved;
        page->zones.removeIf([&](const Zone &z) {
            if (!zoneIds.contains(z.id))
                return false;
            moved.append(z);
            return true;
        });
        for (Zone &z : moved) {
            z.z = top;
            page->zones.append(z);
        }
        return !moved.isEmpty();
    });
}

bool LayoutEditor::sendToBack(const QString &pageId, const QStringList &zoneIds)
{
    return apply(u"Send to back"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page || page->zones.isEmpty())
            return false;
        int bottom = std::numeric_limits<int>::max();
        for (const Zone &z : page->zones)
            bottom = std::min(bottom, z.z);
        QList<Zone> moved;
        page->zones.removeIf([&](const Zone &z) {
            if (!zoneIds.contains(z.id))
                return false;
            moved.append(z);
            return true;
        });
        for (qsizetype i = moved.size() - 1; i >= 0; --i) {
            moved[i].z = bottom;
            page->zones.prepend(moved[i]);
        }
        return !moved.isEmpty();
    });
}

bool LayoutEditor::align(const QString &pageId, const QStringList &zoneIds, Align how)
{
    return apply(u"Align"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        const QList<Zone *> zones = zonesOf(*page, zoneIds);
        if (zones.size() < 2)
            return false;
        const QRect box = boundingBox(zones);
        for (Zone *z : zones) {
            QRect &r = z->rect;
            switch (how) {
            case Align::Left: r.moveLeft(box.left()); break;
            case Align::Right: r.moveLeft(box.left() + box.width() - r.width()); break;
            case Align::HCenter: r.moveLeft(box.left() + (box.width() - r.width()) / 2); break;
            case Align::Top: r.moveTop(box.top()); break;
            case Align::Bottom: r.moveTop(box.top() + box.height() - r.height()); break;
            case Align::VCenter: r.moveTop(box.top() + (box.height() - r.height()) / 2); break;
            }
        }
        return true;
    });
}

bool LayoutEditor::distribute(const QString &pageId, const QStringList &zoneIds, Qt::Orientation orientation)
{
    const bool horizontal = orientation == Qt::Horizontal;
    return apply(u"Distribute"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        QList<Zone *> zones = zonesOf(*page, zoneIds);
        if (zones.size() < 3)
            return false;
        std::ranges::sort(zones, {}, [&](const Zone *z) { return horizontal ? z->rect.x() : z->rect.y(); });
        const QRect box = boundingBox(zones);
        int used = 0;
        for (const Zone *z : zones)
            used += horizontal ? z->rect.width() : z->rect.height();
        const int span = horizontal ? box.width() : box.height();
        const double gap = double(span - used) / double(zones.size() - 1);
        double pos = horizontal ? box.left() : box.top();
        for (Zone *z : zones) {
            if (horizontal) {
                z->rect.moveLeft(int(std::lround(pos)));
                pos += z->rect.width() + gap;
            } else {
                z->rect.moveTop(int(std::lround(pos)));
                pos += z->rect.height() + gap;
            }
        }
        return true;
    });
}

bool LayoutEditor::matchSize(const QString &pageId, const QStringList &zoneIds, bool width, bool height)
{
    return apply(u"Match size"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        const QList<Zone *> zones = zonesOf(*page, zoneIds);
        if (zones.size() < 2)
            return false;
        const QSize ref = zones.first()->rect.size();
        for (Zone *z : zones) {
            QRect r = z->rect;
            if (width) r.setWidth(ref.width());
            if (height) r.setHeight(ref.height());
            z->rect = clampToCanvas(r, page->canvas);
        }
        return true;
    });
}

bool LayoutEditor::setZoneField(const QString &pageId, const QStringList &zoneIds, const QString &path,
                                const QJsonValue &value)
{
    if (path.isEmpty() || path == u"id")
        return false;
    return apply(u"Change %1"_s.arg(path), [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page)
            return false;
        for (Zone *z : zonesOf(*page, zoneIds)) {
            QJsonObject json = z->toJson();
            layout::jsonSet(json, path, value);
            Zone updated = Zone::fromJson(json);
            updated.id = z->id;
            updated.rect = clampToCanvas(updated.rect, page->canvas);
            *z = updated;
        }
        return true;
    });
}

QJsonValue LayoutEditor::zoneField(const QString &pageId, const QString &zoneId, const QString &path) const
{
    const Page *page = layout_.page(pageId);
    const Zone *z = page ? page->zone(zoneId) : nullptr;
    if (!z)
        return QJsonValue(QJsonValue::Undefined);
    return layout::jsonGet(fullZoneJson(*z), path);
}

// --- clipboard ---------------------------------------------------------------

void LayoutEditor::copyZones(const QString &pageId, const QStringList &zoneIds)
{
    const Page *page = layout_.page(pageId);
    if (!page)
        return;
    QList<Zone> copied;
    for (const QString &id : zoneIds) {
        if (const Zone *z = page->zone(id))
            copied.append(*z);
    }
    if (!copied.isEmpty()) {
        clipboard_ = copied;
        clipboardPage_ = pageId;
    }
}

QStringList LayoutEditor::paste(const QString &pageId)
{
    QStringList created;
    apply(u"Paste"_s, [&](Layout &l) {
        Page *page = l.page(pageId);
        if (!page || clipboard_.isEmpty())
            return false;
        const int step = std::max(1, page->grid) * 2;
        for (Zone z : clipboard_) {
            // Same page: offset so the copy is visible. Other page: same spot,
            // like the legacy "copy selected zones onto this page".
            const bool clash = std::ranges::any_of(page->zones, [&](const Zone &o) { return o.rect == z.rect; });
            if (clash)
                z.rect.translate(step, step);
            z.rect = clampToCanvas(z.rect, page->canvas);
            z.id = uniqueZoneId(*page, z.id);
            // A table pasted onto another floor plan stays the same table
            // (Main and Dining Area both show T1); a second one on the same
            // page is a new table.
            if (z.kind == u"table" && std::ranges::any_of(page->zones, [&](const Zone &o) {
                    return o.kind == u"table" && o.label.trimmed().compare(z.label.trimmed(), Qt::CaseInsensitive) == 0;
                }))
                z.label = l.nextTableLabel(z.label);
            created.append(z.id);
            page->zones.append(z);
        }
        return true;
    });
    return created;
}

// --- pages -------------------------------------------------------------------

QString LayoutEditor::addPage(const QString &name, const QString &kind, const QString &templateId)
{
    Page p;
    p.name = name.isEmpty() ? u"New Page"_s : name;
    p.id = uniquePageId(layout_, p.name);
    p.kind = kind.isEmpty() ? u"custom"_s : kind;
    if (const Page *t = layout_.page(templateId)) {
        p.templateId = t->id;
        p.canvas = t->canvas;
        p.grid = t->grid;
    }
    const QString id = p.id;
    return apply(u"Add page"_s, [&](Layout &l) {
        l.pages.append(p);
        return true;
    }) ? id : QString();
}

QString LayoutEditor::duplicatePage(const QString &pageId)
{
    const Page *src = layout_.page(pageId);
    if (!src)
        return {};
    Page copy = *src;
    copy.id = uniquePageId(layout_, src->id + u"-copy"_s);
    copy.name = src->name + u" (copy)"_s;
    copy.role.clear();   // roles are unique
    const QString id = copy.id;
    return apply(u"Duplicate page"_s, [&](Layout &l) {
        const auto at = std::ranges::find(l.pages, pageId, &Page::id);
        l.pages.insert(at - l.pages.begin() + 1, copy);
        return true;
    }) ? id : QString();
}

bool LayoutEditor::deletePage(const QString &pageId, QString *why)
{
    const Page *page = layout_.page(pageId);
    if (!page)
        return false;
    for (const Page &p : layout_.pages) {
        if (p.templateId == pageId) {
            if (why)
                *why = u"'%1' is the template of '%2'."_s.arg(page->name, p.name);
            return false;
        }
    }
    if (page->role == u"login") {
        if (why)
            *why = u"The login page cannot be deleted."_s;
        return false;
    }
    return apply(u"Delete page"_s, [&](Layout &l) {
        return l.pages.removeIf([&](const Page &p) { return p.id == pageId; }) > 0;
    });
}

bool LayoutEditor::setPageField(const QString &pageId, const QString &path, const QJsonValue &value,
                                QString *why)
{
    auto fail = [&](const QString &msg) {
        if (why)
            *why = msg;
        return false;
    };
    const Page *page = layout_.page(pageId);
    if (!page)
        return fail(u"No such page."_s);

    if (path == u"id") {
        const QString newId = value.toString();
        static const QRegularExpression valid(u"^[A-Za-z0-9_-]+$"_s);
        if (newId == pageId)
            return false;
        if (!valid.match(newId).hasMatch())
            return fail(u"Page IDs may use letters, digits, '-' and '_'."_s);
        if (layout_.page(newId))
            return fail(u"Another page already uses ID '%1'."_s.arg(newId));
        return apply(u"Rename page ID"_s, [&](Layout &l) {
            for (Page &p : l.pages) {
                if (p.id == pageId) p.id = newId;
                if (p.templateId == pageId) p.templateId = newId;
                for (Zone &z : p.zones) {
                    for (layout::Action &a : z.actions) {
                        if (a.data.value(u"page").toString() == pageId)
                            a.data.insert(u"page", newId);
                        QJsonArray seq = a.data.value(u"modifierSequence").toArray();
                        bool touched = false;
                        for (qsizetype i = 0; i < seq.size(); ++i) {
                            if (seq[i].toString() == pageId) {
                                seq[i] = newId;
                                touched = true;
                            }
                        }
                        if (touched)
                            a.data.insert(u"modifierSequence", seq);
                    }
                }
            }
            return true;
        });
    }

    if (path == u"role" && !value.toString().isEmpty()) {
        const Page *holder = layout_.pageByRole(value.toString());
        if (holder && holder->id != pageId)
            return fail(u"'%1' already has that role."_s.arg(holder->name));
    }
    if (path == u"role" && page->role == u"login" && value.toString() != u"login")
        return fail(u"Give another page the login role first."_s);

    Layout after = layout_;
    Page *target = after.page(pageId);
    QJsonObject json = target->toJson();
    layout::jsonSet(json, path, value);
    Page updated = Page::fromJson(json);
    updated.id = pageId;
    *target = updated;
    if (path == u"templateId" && templateLoops(after, pageId))
        return fail(u"That template would make a loop."_s);

    return apply(u"Change page %1"_s.arg(path), [&](Layout &l) {
        l = after;
        return true;
    });
}

QJsonValue LayoutEditor::pageField(const QString &pageId, const QString &path) const
{
    const Page *page = layout_.page(pageId);
    return page ? layout::jsonGet(fullPageJson(*page), path) : QJsonValue(QJsonValue::Undefined);
}

QStringList LayoutEditor::referencesTo(const QString &pageId) const
{
    QStringList refs;
    for (const Page &p : layout_.pages) {
        if (p.templateId == pageId)
            refs << u"Template of '%1'"_s.arg(p.name);
        for (const Zone &z : p.zones) {
            const QString who = z.label.isEmpty() ? z.id : z.label.simplified();
            for (const layout::Action &a : z.actions) {
                if (a.data.value(u"page").toString() == pageId)
                    refs << u"'%1' › %2"_s.arg(p.name, who);
                if (a.data.value(u"modifierSequence").toArray().contains(QJsonValue(pageId)))
                    refs << u"'%1' › %2 (modifiers)"_s.arg(p.name, who);
            }
        }
    }
    refs.removeDuplicates();
    return refs;
}

// --- theme -------------------------------------------------------------------

bool LayoutEditor::setTheme(const layout::Theme &theme, const QString &text)
{
    return apply(text, [&](Layout &l) {
        l.theme = theme;
        return true;
    });
}

bool LayoutEditor::setThemeField(const QString &path, const QJsonValue &value)
{
    return apply(u"Change theme %1"_s.arg(path), [&](Layout &l) {
        QJsonObject json = l.theme.toJson();
        layout::jsonSet(json, path, value);
        l.theme = layout::Theme::fromJson(json);
        return true;
    });
}

QJsonValue LayoutEditor::themeField(const QString &path) const
{
    return layout::jsonGet(layout_.theme.toJson(), path);
}

// --- import / export -----------------------------------------------------------

QJsonObject LayoutEditor::exportPage(const QString &pageId) const
{
    const Page *page = layout_.page(pageId);
    if (!page)
        return {};
    QJsonObject o = page->toJson();
    o.insert(u"schemaVersion", Layout::SchemaVersion);
    return o;
}

QString LayoutEditor::importPage(const QJsonObject &json, QString *why)
{
    QStringList errors;
    if (!Layout::checkSchema(json, u"page"_s, &errors)) {
        if (why)
            *why = errors.join(u'\n');
        return {};
    }
    Page p = Page::fromJson(json);
    if (p.zones.isEmpty() && p.name.isEmpty() && p.id.isEmpty()) {
        if (why)
            *why = u"The file does not contain a page."_s;
        return {};
    }
    p.id = uniquePageId(layout_, p.id.isEmpty() ? p.name : p.id);
    if (!p.role.isEmpty() && layout_.pageByRole(p.role))
        p.role.clear();
    const QString id = p.id;
    return apply(u"Import page"_s, [&](Layout &l) {
        l.pages.append(p);
        return true;
    }) ? id : QString();
}

bool LayoutEditor::arrangePage(const QString &pageId, const QJsonArray &zones, const QJsonObject &background,
                               const QString &description)
{
    const Page *current = layout_.page(pageId);
    if (!current)
        return false;
    // Already arranged so: nothing to do (and nothing to undo).
    QList<layout::Zone> wanted;
    for (const QJsonValue &v : zones)
        wanted.append(layout::Zone::fromJson(v.toObject()));
    if (wanted == current->zones && background == current->background)
        return true;
    return apply(description, [&](Layout &l) {
        Page *p = l.page(pageId);
        if (!p)
            return false;
        p->zones.clear();
        for (const QJsonValue &v : zones)
            p->zones.append(layout::Zone::fromJson(v.toObject()));
        p->background = background;
        return true;
    });
}

bool LayoutEditor::replaceLayout(const Layout &layout, const QString &description)
{
    return apply(description, [&](Layout &l) {
        l = layout;
        return true;
    });
}

} // namespace vt::app
