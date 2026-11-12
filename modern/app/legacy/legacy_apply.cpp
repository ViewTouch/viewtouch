#include "app/legacy/legacy_apply.hh"

#include "layout/menu_screens.hh"

#include <QCoreApplication>

#include <algorithm>

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

QVariantMap request(app::PosShared &shared, const QString &folder, const QVariantMap &parts, bool apply,
                    const Request &how)
{
    if (!isDatFolder(folder))
        return {{u"error"_s, tr("%1 isn't legacy ViewTouch's dat folder (no menu.dat, settings.dat or zone_db.dat).").arg(folder)}};
    const Store legacy = readStore(folder);
    ImportContext cx;
    cx.current = &shared.settings;
    if (parts.contains(u"menu"_s))
        cx.parts.menu = parts.value(u"menu"_s).toBool();
    if (parts.contains(u"history"_s))
        cx.parts.history = parts.value(u"history"_s).toBool();
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
