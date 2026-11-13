#include "app/legacy/legacy_apply.hh"

#include "layout/menu_screens.hh"

#include <QCoreApplication>

#include <algorithm>
#include <memory>
#include <set>

using namespace Qt::StringLiterals;

namespace vt::legacy {

namespace {

QString tr(const char *s, const char *c = nullptr, int n = -1) { return QCoreApplication::translate("vt::legacy", s, c, n); }

} // namespace

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
        out.error = tr("This store already has sales: its past days can't go in before them. Bring over only the "
                       "menu, or the sales after a factory reset (it takes a backup).");
        return out;
    }
    // All its days or none: what importPastDay would refuse, found first.
    if (parts.history) {
        std::int64_t before = 0;
        for (const ImportedDay &d : in.days) {
            if (d.closedAt <= 0 || (d.openedAt > 0 && d.closedAt < d.openedAt) || d.closedAt < before) {
                out.error = tr("Day %1 doesn't follow the day before it (its times are wrong in legacy's files): "
                               "nothing was brought over.").arg(d.legacyId);
                return out;
            }
            before = d.closedAt;
        }
    }
    // --- the menu, in place of the store's ---
    if (parts.menu && !in.menu.empty()) {
        for (const core::MenuItem &m : s->menu)
            if (s->sink)
                s->sink->deleteMenuItem(m.id);
        s->menu = in.menu;
        for (int i = 0; i < int(s->menu.size()); ++i)
            if (s->sink)
                s->sink->saveMenuItem(s->menu[std::size_t(i)], i);
        s->settings.menuCategories = in.categories;
        s->settings.modifierGroups = in.modifierGroups;
        if (s->sink)
            s->sink->saveSettings(s->settings);
        ++s->adminRevision;
        emit s->adminChanged();
        emit s->menuChanged();
        out.done.append(tr("%n menu item(s)", nullptr, int(in.menu.size())) + u", "_s
                        + tr("%n famil(ies)", nullptr, int(in.categories.size())) + u", "_s
                        + tr("%n choice group(s)", nullptr, int(in.modifierGroups.size())));
        // On Modern's own menu screens: pages that fill themselves from the menu.
        if (layout && layout::hasHandBuiltMenu(*layout))
            *layout = layout::withSelfFillingMenu(*layout);
        // Buttons for the old menu's items go; an item page left with none
        // (the sample café's Burgers, Drinks...) goes, and the ways to it.
        if (layout) {
            std::set<std::string> items;
            for (const core::MenuItem &m : s->menu)
                items.insert(m.id);
            const auto stale = [&](const layout::Zone &z) {
                return std::ranges::any_of(z.actions, [&](const layout::Action &a) {
                    return a.type() == u"addItem" && !items.contains(a.str(u"item"_s).toStdString());
                });
            };
            QSet<QString> gone;
            for (layout::Page &p : layout->pages) {
                const bool hadItems = std::ranges::any_of(p.zones, [](const layout::Zone &z) {
                    return std::ranges::any_of(z.actions, [](const layout::Action &a) { return a.type() == u"addItem"; });
                });
                p.zones.removeIf(stale);
                const bool hasItems = std::ranges::any_of(p.zones, [](const layout::Zone &z) {
                    return std::ranges::any_of(z.actions, [](const layout::Action &a) { return a.type() == u"addItem"; });
                });
                if (hadItems && !hasItems && p.kind == u"items")
                    gone.insert(p.id);
            }
            layout->pages.removeIf([&](const layout::Page &p) { return gone.contains(p.id) || gone.contains(p.variantOf); });
            for (layout::Page &p : layout->pages) {
                p.zones.removeIf([&](const layout::Zone &z) {
                    return std::ranges::any_of(z.actions, [&](const layout::Action &a) {
                        return a.type() == u"jump" && gone.contains(a.str(u"page"_s));
                    });
                });
            }
        }
    }
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
    return out;
}

namespace {

// A folder read and worked out, between the two halves.
struct Work {
    QString error;
    Imported in;
    ImportParts parts;
    QVariantMap preview;
};

} // namespace

std::any read(const QString &folder, const QVariantMap &parts, const core::PosSettings &settings)
{
    auto work = std::make_shared<Work>();
    if (!isDatFolder(folder)) {
        work->error = tr("%1 isn't legacy ViewTouch's dat folder (no menu.dat, settings.dat or zone_db.dat).").arg(folder);
        return work;
    }
    const Store legacy = readStore(folder);
    ImportContext cx;
    cx.current = &settings;
    if (parts.contains(u"menu"_s))
        cx.parts.menu = parts.value(u"menu"_s).toBool();
    if (parts.contains(u"history"_s))
        cx.parts.history = parts.value(u"history"_s).toBool();
    work->parts = cx.parts;
    work->in = convert(legacy, cx);
    work->preview = preview(legacy, work->in);
    QStringList differ;
    for (const Mismatch &m : work->in.mismatches)
        differ << tr("Day %1, check %2 (%3): legacy $%4, here $%5")
                      .arg(m.day)
                      .arg(m.serial)
                      .arg(m.label)
                      .arg(m.legacy.cents() / 100.0, 0, 'f', 2)
                      .arg(m.modern.cents() / 100.0, 0, 'f', 2);
    work->preview.insert(u"differ"_s, differ);
    return work;
}

QVariantMap finish(app::PosShared &shared, const std::any &read, bool apply, const Request &how)
{
    const auto *held = std::any_cast<std::shared_ptr<Work>>(&read);
    if (!held || !*held)
        return {{u"error"_s, tr("Nothing was read.")}};
    const Work &work = **held;
    if (!work.error.isEmpty())
        return {{u"error"_s, work.error}};
    if (!apply) {
        QVariantMap p = work.preview;
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
    const Applied applied = applyImport(pos, pages ? &*pages : nullptr, work.in, work.parts);
    if (how.flush)
        how.flush();
    if (applied.error.isEmpty() && pages && how.savePages)
        error = how.savePages(*pages);
    return {{u"done"_s, applied.done}, {u"error"_s, applied.error.isEmpty() ? error : applied.error}, {u"backup"_s, backup}};
}

void install(app::PosShared &shared, Request how)
{
    shared.legacyRead = [](const QString &folder, const QVariantMap &parts, const core::PosSettings &settings) {
        return read(folder, parts, settings);
    };
    shared.legacyFinish = [&shared, how = std::move(how)](const std::any &work, bool apply) {
        return finish(shared, work, apply, how);
    };
}

} // namespace vt::legacy
