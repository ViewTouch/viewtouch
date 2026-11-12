#include "app/legacy/legacy_apply.hh"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace vt::legacy {

namespace {

QString tr(const char *s, const char *c = nullptr, int n = -1) { return QCoreApplication::translate("vt::legacy", s, c, n); }

layout::Theme legacyTheme()
{
    QFile f(u":/seed/themes/legacy.json"_s);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return layout::Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object());
}

// Where a floor's tables and drawings are on a page (their bounds), or none.
std::optional<QRect> floorArea(const layout::Page &p)
{
    QRect r;
    for (const layout::Zone &z : p.zones)
        if (z.kind == u"table" || z.kind == u"decor")
            r = r.isNull() ? z.rect : r.united(z.rect);
    return r.isNull() ? std::nullopt : std::optional<QRect>(r);
}

// The dining room's tables, from legacy's page to `area`, as they were
// placed (scaled alike both ways, centered).
QList<layout::Zone> placeTables(const QJsonArray &floor, const QRect &area)
{
    QRect from;
    for (const QJsonValue &v : floor) {
        const QJsonObject r = v.toObject().value(u"rect"_s).toObject();
        const QRect t(r.value(u"x"_s).toInt(), r.value(u"y"_s).toInt(), r.value(u"w"_s).toInt(), r.value(u"h"_s).toInt());
        from = from.isNull() ? t : from.united(t);
    }
    QList<layout::Zone> out;
    if (from.isEmpty())
        return out;
    const double scale = std::min(double(area.width()) / from.width(), double(area.height()) / from.height());
    const int dx = area.x() + int((area.width() - from.width() * scale) / 2);
    const int dy = area.y() + int((area.height() - from.height() * scale) / 2);
    for (const QJsonValue &v : floor) {
        const QJsonObject o = v.toObject();
        const QJsonObject r = o.value(u"rect"_s).toObject();
        layout::Zone z;
        z.id = o.value(u"id"_s).toString();
        z.kind = u"table"_s;
        z.label = o.value(u"label"_s).toString();
        z.shape = o.value(u"shape"_s).toString();
        z.rect = QRect(dx + int((r.value(u"x"_s).toInt() - from.x()) * scale),
                       dy + int((r.value(u"y"_s).toInt() - from.y()) * scale),
                       std::max(48, int(r.value(u"w"_s).toInt() * scale)),
                       std::max(48, int(r.value(u"h"_s).toInt() * scale)));
        z.props.insert(u"seats"_s, 4);
        out.append(z);
    }
    return out;
}

} // namespace

QStringList applyPages(layout::Layout &layout, const Imported &in, const ImportParts &parts)
{
    QStringList done;
    if (parts.pages && !in.pages.isEmpty()) {
        // Legacy's own pages, the style the store knows.
        layout.useStyle(u"legacy"_s, legacyTheme(), layout::Layout::stylePages(u"legacy"_s));
        // The store's own pages in place of legacy's starter menu pages
        // (the system pages stay; legacy's sample dining room, 50, too).
        QSet<QString> theirs;
        for (const QJsonObject &o : in.pages)
            theirs.insert(o.value(u"id"_s).toString());
        layout.pages.removeIf([&](const layout::Page &p) {
            const int lid = p.extra.value(u"legacyId"_s).toInt();
            return p.inStyle == u"legacy" && (theirs.contains(p.id) || (lid > 0 && lid != 50));
        });
        for (QJsonObject o : in.pages) {
            o.insert(u"storeLegacy"_s, true);
            if (o.value(u"kind"_s).toString() == u"index") {
                o.insert(u"storeMenu"_s, true);
                o.insert(u"mealPeriod"_s, u"all"_s);
            }
            layout.pages.append(layout::Page::fromJson(o));
        }
        done.append(tr("%n page(s) of the store's own, in the Legacy style", nullptr, int(in.pages.size())));
    }
    if (parts.tables && in.floor) {
        // The dining room: the floor pages' tables and drawings give way to
        // the store's tables, on the main floor (and its phone version);
        // the area tabs go (legacy had one room here).
        int placed = 0;
        for (layout::Page &p : layout.pages) {
            if (p.kind != u"tables" || !p.inStyle.isEmpty() || !p.service.isEmpty())
                continue;
            const std::optional<QRect> area = floorArea(p);
            if (!area) {
                // A version with no floor of its own (a phone's list): not on it.
                if (p.variantOf == u"tables")
                    for (const QJsonValue &v : *in.floor)
                        if (const QString id = v.toObject().value(u"id"_s).toString(); !p.hiddenZones.contains(id))
                            p.hiddenZones.append(id);
                continue;
            }
            p.zones.removeIf([](const layout::Zone &z) { return z.kind == u"table" || z.kind == u"decor"; });
            const bool main = p.id == u"tables" || p.variantOf == u"tables";
            if (main) {
                const QList<layout::Zone> tables = placeTables(*in.floor, *area);
                p.zones.append(tables);
                placed = std::max(placed, int(tables.size()));
            }
            p.zones.removeIf([](const layout::Zone &z) { return z.id.startsWith(u"plan-"_s); });
        }
        if (placed)
            done.append(tr("%n table(s) of its dining room", nullptr, placed));
    }
    return done;
}

Applied applyImport(app::PosService &pos, layout::Layout *layout, const Imported &in, const ImportParts &parts)
{
    Applied out;
    app::PosShared *s = pos.shared();
    if (!in.errors.isEmpty()) {
        out.error = in.errors.join(u'\n');
        return out;
    }
    if (parts.history && !in.days.empty()
        && (!s->pastDays.empty() || !s->closedToday.empty() || !s->open.empty())) {
        out.error = tr("This store already has sales: its past days can't go in before them. Bring it over "
                       "without the sales history, or after a factory reset (it takes a backup).");
        return out;
    }
    core::PosSettings &settings = s->settings;
    // --- settings ---
    if (parts.settings) {
        if (in.storeName)
            settings.storeName = in.storeName->toStdString();
        if (in.receiptHeader)
            settings.receiptHeader = in.receiptHeader->toStdString();
        if (in.receiptFooter)
            settings.receiptFooter = in.receiptFooter->toStdString();
        if (in.tax)
            settings.tax = *in.tax;
        settings.setupDone = true;   // set up already, in legacy: no first-day guide
        for (const core::Tender &t : in.tenders)
            if (std::ranges::none_of(settings.tenders, [&](const core::Tender &x) { return x.id == t.id; }))
                settings.tenders.push_back(t);
        for (const core::PosSettings::Promotion &p : in.promotions) {
            std::erase_if(settings.promotions, [&](const core::PosSettings::Promotion &x) { return x.id == p.id; });
            settings.promotions.push_back(p);
        }
        for (const core::PrinterConfig &p : in.printers) {
            const auto it = std::ranges::find(settings.printers, p.id, &core::PrinterConfig::id);
            if (it != settings.printers.end())
                *it = p;
            else
                settings.printers.push_back(p);
        }
        out.done.append(tr("Store settings: name, receipt lines, taxes, %n discount(s) and comp(s), printers", nullptr,
                           int(in.tenders.size())));
    }
    if (parts.pages && !in.pages.isEmpty())
        settings.style = "legacy";
    // --- menu ---
    if (parts.menu && !in.menu.empty()) {
        for (const core::MenuItem &m : s->menu)
            if (s->sink)
                s->sink->deleteMenuItem(m.id);
        s->menu = in.menu;
        for (int i = 0; i < int(s->menu.size()); ++i)
            if (s->sink)
                s->sink->saveMenuItem(s->menu[std::size_t(i)], i);
        settings.menuCategories = in.categories;
        out.done.append(tr("%n menu item(s)", nullptr, int(in.menu.size())));
    }
    if (s->sink && (parts.settings || parts.menu || parts.pages))
        s->sink->saveSettings(settings);
    // --- staff ---
    if (parts.staff && !in.employees.empty()) {
        // The starter's sample staff stop (their PINs are public).
        for (core::Employee &e : s->employees)
            if (e.sample && e.active) {
                e.active = false;
                if (s->sink)
                    s->sink->saveEmployee(e);
            }
        for (const core::Employee &e : in.employees) {
            const auto it = std::ranges::find(s->employees, e.id, &core::Employee::id);
            if (it != s->employees.end())
                *it = e;
            else
                s->employees.push_back(e);
            if (s->sink)
                s->sink->saveEmployee(e);
        }
        out.done.append(tr("%n employee(s)", nullptr, int(in.employees.size())));
    }
    // --- customers ---
    if (parts.customers && !in.customers.empty()) {
        for (const core::CustomerRecord &c : in.customers) {
            const auto it = std::ranges::find(s->customers, c.id, &core::CustomerRecord::id);
            if (it != s->customers.end())
                *it = c;
            else
                s->customers.push_back(c);
            if (s->sink)
                s->sink->saveCustomer(c);
        }
        out.done.append(tr("%n customer(s)", nullptr, int(in.customers.size())));
    }
    ++s->adminRevision;
    emit s->adminChanged();
    emit s->menuChanged();
    emit s->staffChanged();
    emit s->customersChanged();
    // --- past days, oldest first ---
    if (parts.history && !in.days.empty()) {
        int checks = 0;
        for (const ImportedDay &d : in.days) {
            QString error;
            if (!pos.importPastDay(d.openedAt, d.closedAt, d.checks, d.punches, d.drawers, d.rates, &error)) {
                out.error = tr("Day %1: %2").arg(d.legacyId).arg(error);
                break;
            }
            checks += int(d.checks.size());
        }
        out.done.append(tr("%n business day(s) of sales", nullptr, int(in.days.size())) + u", "_s
                        + tr("%n check(s)", nullptr, checks));
        emit s->dayChanged();
        emit s->checksChanged();
    }
    if (layout)
        out.done.append(applyPages(*layout, in, parts));
    return out;
}

QVariantMap request(app::PosShared &shared, const QString &folder, const QVariantMap &parts, bool apply,
                    const Request &how)
{
    if (!isDatFolder(folder))
        return {{u"error"_s, tr("%1 isn't legacy ViewTouch's dat folder (no menu.dat, settings.dat or zone_db.dat).").arg(folder)}};
    const Store legacy = readStore(folder);
    ImportContext cx;
    cx.current = &shared.settings;
    for (const auto &[key, on] : std::initializer_list<std::pair<QString, bool *>>{
             {u"pages"_s, &cx.parts.pages}, {u"menu"_s, &cx.parts.menu}, {u"tables"_s, &cx.parts.tables},
             {u"settings"_s, &cx.parts.settings}, {u"staff"_s, &cx.parts.staff},
             {u"customers"_s, &cx.parts.customers}, {u"history"_s, &cx.parts.history}})
        if (parts.contains(key))
            *on = parts.value(key).toBool();
    const Imported in = convert(legacy, cx);
    if (!apply) {
        QVariantMap p = preview(legacy, in);
        QStringList differ;
        for (const Mismatch &m : in.mismatches)
            differ << tr("Day %1, check %2 (%3): legacy $%4, here $%5")
                          .arg(m.day)
                          .arg(m.serial)
                          .arg(m.label)
                          .arg(m.legacy.cents() / 100.0, 0, 'f', 2)
                          .arg(m.modern.cents() / 100.0, 0, 'f', 2);
        p.insert(u"differ"_s, differ);
        p.insert(u"hasSales"_s, !shared.pastDays.empty() || !shared.closedToday.empty() || !shared.open.empty());
        return {{u"preview"_s, p}};
    }
    QString error;
    const QString backup = how.backup ? how.backup(&error) : QString();
    if (how.backup && backup.isEmpty())
        return {{u"error"_s, tr("The backup failed, so nothing was brought over: %1").arg(error)}};
    app::PosService pos(&shared, u"legacy import"_s);
    std::optional<layout::Layout> pages;
    if (how.pages)
        pages = how.pages();
    const Applied applied = applyImport(pos, pages ? &*pages : nullptr, in, cx.parts);
    if (how.flush)
        how.flush();
    if (applied.error.isEmpty() && pages && how.savePages)
        error = how.savePages(*pages);
    return {{u"done"_s, applied.done}, {u"error"_s, applied.error.isEmpty() ? error : applied.error}, {u"backup"_s, backup}};
}

} // namespace vt::legacy
