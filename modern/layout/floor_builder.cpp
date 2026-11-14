#include "layout/floor_builder.hh"

#include <QCoreApplication>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace vt::layout::floorplan {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("vt::layout::floorplan", s); }

// Floor units: a table stands in a cell, the room's name above its rows.
constexpr int CellW = 230, CellH = 190, Pad = 24, Head = 64, Gap = 32;
constexpr int StoolW = 110, Counter = 70;

const QStringList order{u"dining"_s, u"bar"_s, u"patio"_s};

QString roomName(const QString &id)
{
    if (id == u"bar") return tr("Bar");
    if (id == u"patio") return tr("Patio");
    return tr("Dining Room");
}
QString roomColor(const QString &id)
{
    if (id == u"bar") return u"#5b3a1e"_s;
    if (id == u"patio") return u"#2f5d3a"_s;
    return {};
}
QString prefix(const QString &id) { return id == u"bar" ? u"B"_s : id == u"patio" ? u"P"_s : u"D"_s; }

Zone table(const QString &label, const QRect &r, const QString &shape, int seats, bool chairs = true)
{
    Zone z;
    z.id = u"table-"_s + label.toLower().replace(u' ', u'-');
    z.kind = u"table"_s;
    z.rect = r;
    z.shape = shape;
    z.behavior = u"none"_s;
    z.label = label;
    z.props.insert(u"seats"_s, seats);
    if (chairs)
        z.props.insert(u"chairs"_s, true);
    return z;
}

Zone area(const QString &room, const QRect &r)
{
    Zone z;
    z.id = u"area-"_s + room;
    z.kind = u"decor"_s;
    z.rect = r;
    z.behavior = u"none"_s;
    z.label = roomName(room);
    z.props.insert(u"decor"_s, u"area"_s);
    if (const QString c = roomColor(room); !c.isEmpty())
        z.props.insert(u"color"_s, c);
    return z;
}

Zone counter(const QRect &r)
{
    Zone z;
    z.id = u"counter-1"_s;
    z.kind = u"decor"_s;
    z.rect = r;
    z.behavior = u"none"_s;
    z.props.insert(u"decor"_s, u"counter"_s);
    return z;
}

// One room, at (0, 0): its tables (named from `names`, taken in order) and
// its size. Its floor (the area) is added by whoever places it.
struct Built {
    QList<Zone> tables;
    QSize size;
};

Built room(const Room &r, QStringList &tableNames, QStringList &seatNames)
{
    Built b;
    int y = Head;
    int width = 0;
    // A bar: the counter along the top, a stool for each seat in front of it.
    if (r.seats > 0) {
        b.tables.append(counter(QRect(Pad, y, r.seats * StoolW, Counter)));
        for (int i = 0; i < r.seats; ++i)
            b.tables.append(table(seatNames.takeFirst(), QRect(Pad + i * StoolW + 15, y + Counter + 16, 80, 80),
                                  u"circle"_s, 1, false));
        y += Counter + 16 + 80 + Gap;
        width = r.seats * StoolW;
    }
    // The tables in rows: twos, then fours, then sixes.
    QList<int> sizes;
    sizes.append(QList<int>(r.two, 2));
    sizes.append(QList<int>(r.four, 4));
    sizes.append(QList<int>(r.six, 6));
    const int n = int(sizes.size());
    if (n > 0) {
        const int cols = std::max(1, int(std::ceil(std::sqrt(n * 1.6))));
        const int rows = (n + cols - 1) / cols;
        for (int i = 0; i < n; ++i) {
            const int seats = sizes[i];
            const QSize s = seats == 2 ? QSize(120, 120) : seats == 4 ? QSize(170, 120) : QSize(210, 130);
            const QPoint cell(Pad + (i % cols) * CellW, y + (i / cols) * CellH);
            const QRect at(cell + QPoint((CellW - s.width()) / 2, (CellH - s.height()) / 2), s);
            b.tables.append(table(tableNames.takeFirst(), at, seats == 2 ? u"circle"_s : u"rect"_s, seats));
        }
        y += rows * CellH;
        width = std::max(width, cols * CellW);
    }
    b.size = QSize(std::max(width, 2 * CellW) + 2 * Pad, y + Pad);
    return b;
}

void shift(QList<Zone> &zones, const QPoint &by)
{
    for (Zone &z : zones)
        z.rect.translate(by);
}

} // namespace

Answers fromMap(const QVariantMap &answers)
{
    Answers a;
    a.byRoom = answers.value(u"byRoom"_s).toBool();
    const auto count = [](const QVariantMap &m, const QString &k) { return std::clamp(m.value(k).toInt(), 0, 60); };
    QList<Room> given;
    for (const QVariant &v : answers.value(u"rooms"_s).toList()) {
        const QVariantMap m = v.toMap();
        Room r;
        r.id = m.value(u"id"_s).toString();
        if (!order.contains(r.id))
            continue;
        r.two = count(m, u"two"_s);
        r.four = count(m, u"four"_s);
        r.six = count(m, u"six"_s);
        r.seats = r.id == u"bar" ? count(m, u"seats"_s) : 0;
        if (r.two + r.four + r.six + r.seats > 0 && std::ranges::none_of(given, [&](const Room &x) { return x.id == r.id; }))
            given.append(r);
    }
    for (const QString &id : order)
        for (const Room &r : given)
            if (r.id == id)
                a.rooms.append(r);
    return a;
}

QStringList names(const Answers &a)
{
    QStringList out, seats;
    int n = 0;
    for (const Room &r : a.rooms) {
        int k = 0;
        for (int i = 0; i < r.two + r.four + r.six; ++i)
            out << (a.byRoom ? prefix(r.id) + QString::number(++k) : u"T"_s + QString::number(++n));
        for (int i = 0; i < r.seats; ++i)
            seats << tr("Bar %1").arg(i + 1);
    }
    return out + seats;
}

int tableCount(const Answers &a) { return int(names(a).size()); }

Plan build(const Answers &a, const QString &only)
{
    // Names first, across every room, so a table is called the same on Main
    // and on its room's own plan.
    QStringList all = names(a);
    QList<QStringList> tableNames, seatNames;
    for (const Room &r : a.rooms) {
        tableNames.append(all.mid(0, r.two + r.four + r.six));
        all = all.mid(r.two + r.four + r.six);
    }
    for (const Room &r : a.rooms) {
        seatNames.append(all.mid(0, r.seats));
        all = all.mid(r.seats);
    }
    QList<Built> built;
    QStringList ids;
    for (int i = 0; i < a.rooms.size(); ++i) {
        built.append(room(a.rooms[i], tableNames[i], seatNames[i]));
        ids << a.rooms[i].id;
    }
    Plan plan;
    if (!only.isEmpty()) {
        const qsizetype i = ids.indexOf(only);
        if (i < 0)
            return plan;
        plan.size = built[i].size;
        plan.zones.append(area(only, QRect(QPoint(0, 0), plan.size)));
        plan.zones.append(built[i].tables);
        return plan;
    }
    // Inside (the dining room and the bar) side by side; the patio below.
    int x = 0, insideH = 0;
    for (int i = 0; i < built.size(); ++i)
        if (ids[i] != u"patio")
            insideH = std::max(insideH, built[i].size.height());
    QList<Zone> floors, tables;
    for (int i = 0; i < built.size(); ++i) {
        if (ids[i] == u"patio")
            continue;
        floors.append(area(ids[i], QRect(x, 0, built[i].size.width(), insideH)));
        QList<Zone> t = built[i].tables;
        shift(t, QPoint(x, 0));
        tables.append(t);
        x += built[i].size.width() + Gap;
    }
    int width = std::max(0, x - Gap), height = insideH;
    if (const qsizetype p = ids.indexOf(u"patio"_s); p >= 0) {
        const int top = insideH > 0 ? insideH + Gap : 0;
        width = std::max(width, built[p].size.width());
        floors.append(area(u"patio"_s, QRect(0, top, width, built[p].size.height())));
        QList<Zone> t = built[p].tables;
        shift(t, QPoint(0, top));
        tables.append(t);
        height = top + built[p].size.height();
    }
    // A room narrower than the plan: its floor as wide as the others'.
    if (!floors.isEmpty() && floors.last().id == u"area-patio")
        floors.last().rect.setWidth(width);
    plan.size = QSize(width, height);
    plan.zones = floors + tables;
    return plan;
}

QList<Zone> fit(const Plan &plan, const QRect &box)
{
    QList<Zone> out;
    if (plan.size.isEmpty() || box.isEmpty())
        return out;
    const double s = std::min({double(box.width()) / plan.size.width(), double(box.height()) / plan.size.height(), 1.5});
    const double ox = box.x() + (box.width() - plan.size.width() * s) / 2;
    const double oy = box.y() + (box.height() - plan.size.height() * s) / 2;
    for (Zone z : plan.zones) {
        const QRect r = z.rect;
        z.rect = QRect(int(std::lround(ox + r.x() * s)), int(std::lround(oy + r.y() * s)),
                       std::max(8, int(std::lround(r.width() * s))), std::max(8, int(std::lround(r.height() * s))));
        out.append(z);
    }
    return out;
}

int apply(Layout &layout, const Answers &a)
{
    QStringList chosen;
    for (const Room &r : a.rooms)
        chosen << r.id;
    const Page *mainPage = layout.pageByRole(u"tables"_s);
    const QString mainId = mainPage ? mainPage->id : QString();
    struct Target {
        QString page;
        QString room;   // empty: every room (Main)
    };
    QList<Target> targets;
    if (!mainId.isEmpty())
        targets.append({mainId, {}});
    for (const QString &room : order)
        if (layout.page(u"floor-"_s + room))
            targets.append({u"floor-"_s + room, room});
    // Plan tabs (plan-main, plan-dining...): one for each room that has
    // tables, none when there's only one room.
    QStringList tabs{u"plan-main"_s};
    for (const QString &room : chosen)
        tabs << u"plan-"_s + room;
    if (chosen.size() < 2)
        tabs.clear();

    int changed = 0;
    for (const Target &t : targets) {
        Page *p = layout.page(t.page);
        if (!p)
            continue;
        QRect box;
        for (const Zone &z : p->zones)
            if (z.kind == u"table" || z.kind == u"decor")
                box = box.united(z.rect);
        if (box.isEmpty())
            box = QRect(16, 104, 1440, 864).intersected(QRect(QPoint(0, 0), p->canvas));
        p->zones.removeIf([](const Zone &z) { return z.kind == u"table" || z.kind == u"decor"; });
        if (t.room.isEmpty() || chosen.contains(t.room)) {
            const QList<Zone> floor = fit(build(a, t.room), box);
            for (qsizetype i = floor.size() - 1; i >= 0; --i)
                p->zones.prepend(floor[i]);   // under the buttons
        }
        // The tabs: those still wanted, spread over the row they had.
        QList<Zone *> row;
        int left = INT_MAX, right = INT_MIN;
        for (Zone &z : p->zones)
            if (z.id.startsWith(u"plan-")) {
                left = std::min(left, z.rect.left());
                right = std::max(right, z.rect.right());
            }
        p->zones.removeIf([&](const Zone &z) { return z.id.startsWith(u"plan-") && !tabs.contains(z.id); });
        for (const QString &id : tabs)
            for (Zone &z : p->zones)
                if (z.id == id)
                    row.append(&z);
        if (!row.isEmpty()) {
            const int gap = 8, n = int(row.size());
            const int w = (right - left + 1 - gap * (n - 1)) / n;
            for (int i = 0; i < n; ++i)
                row[i]->rect = QRect(left + i * (w + gap), row[i]->rect.y(), w, row[i]->rect.height());
        }
        ++changed;
    }
    return changed;
}

} // namespace vt::layout::floorplan
