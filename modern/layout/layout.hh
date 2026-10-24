#pragma once

#include "layout/page.hh"

#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace vt::layout {

// The complete customizable UI: a theme plus every page.
//
// On disk a layout is either a directory (theme.json + pages/<id>.json, used
// for the shipped seed) or a single .vtlayout.json document.
class Layout {
public:
    static constexpr int SchemaVersion = 1;
    static constexpr int MaxTemplateDepth = 8;

    Theme theme;
    QList<Page> pages;

    const Page *page(const QString &id) const;
    Page *page(const QString &id);
    const Page *pageByRole(const QString &role) const;
    const Page *firstPageOfKind(const QString &kind) const;

    // Page id named by an action's "page" or "role" key; empty if unresolved.
    QString resolveTarget(const QJsonObject &params) const;

    // [page, its template, that template's template, ...]. Stops at a missing
    // template, a cycle, or MaxTemplateDepth.
    QList<const Page *> templateChain(const QString &pageId) const;

    struct PlacedZone {
        const Zone *zone = nullptr;
        const Page *owner = nullptr;
        bool inherited = false;
    };
    // Zones visible on a page, bottom to top: deepest template first, the
    // page's own zones last. Within a page, ordered by z then file order.
    QList<PlacedZone> effectiveZones(const QString &pageId) const;

    // Fully resolved style for a zone in a state, as seen on `viewingPageId`:
    // zone -> page chain -> theme kind -> theme -> (non-normal) resolved normal.
    QJsonObject resolveStyle(const Zone &zone, const QString &viewingPageId, ZoneState state) const;
    QJsonObject resolveBackground(const QString &pageId) const;

    // The store's style (Store Settings -> Screens -> Style): "classic", or
    // "modern". The theme in use says which it is ("styleId"); the other
    // ones wait in it ("otherStyles"), with any changes the store made to
    // them, for when the store switches back. `builtIn`: the style's
    // starter theme, for a style this store has never used. A style's
    // "enforce" (normal, selected, disabled, background) is its look over
    // the pages' own (frames, shadows, fonts, textures; not colors).
    static QString styleOf(const Theme &theme);
    bool useStyle(const QString &id, const Theme &builtIn);

    // Human-readable problems: dangling references, duplicate ids, missing
    // required roles, zones off the canvas. Empty means the layout is sound.
    QStringList validate() const;

    // A store started empty (Factory Reset -> Start empty): one floor plan,
    // the tables page, with no tables or decor on it; the other floor plans
    // (and their phone versions) and the buttons that went to them, gone.
    // Returns the pages taken out.
    QStringList clearFloors();

    // Bringing a saved layout up to date with a newer set of starter pages:
    // - starter pages it doesn't have and never had (`seen`) are added;
    // - starter pages it still has exactly as installed (their fingerprint
    //   is in `installed`) are replaced by the newer version;
    // - anything else (edited, or from before fingerprints) is left alone.
    // Returns what changed and the fingerprints to keep for next time.
    struct StarterUpdate {
        QStringList added;
        QStringList updated;
        QStringList buttonsAdded;            // "page id#zone id": new starter buttons put on pages edited here
        QStringList buttonsNoRoom;           // ... and those with no free place for them
        QHash<QString, QString> installed;   // page id -> fingerprint of the starter version installed
    };
    // Starter buttons new since buttons were tracked ("page id#zone id" in
    // `seen`): before that, these are known to be new; any other missing one
    // was taken off on purpose.
    static const QStringList &newStarterButtons();
    StarterUpdate updateFromStarter(const Layout &starter, const QStringList &seen,
                                    const QHash<QString, QString> &installed);
    static QString fingerprint(const Page &page);

private:
    void enforce(QJsonObject &style, const QString &part) const;   // the style's own look (useStyle)

public:
    // The version of `pageId` made for `formFactor` screens ("phone"), if any.
    const Page *variantFor(const QString &pageId, const QString &formFactor) const;

    // A version's shared zones (Zone::shared) brought in step with the page's
    // zones they show: everything but where they sit and their own fields. A
    // shared zone whose zone is gone goes too. Done on loading and after
    // every edit, so everything else sees whole zones.
    void syncVariants();
    // Where a version's zone comes from: the zone of its page (or of that
    // page's templates) with that id, and the page holding it.
    PlacedZone sourceOf(const Page &variant, const QString &zoneId) const;
    // The page's zones a version doesn't show, in the page's order, with
    // whether that's on purpose (Page::hiddenZones) or they are new there.
    // Notes and floor plan pieces (walls, doors) don't count: phones never
    // show them. Templates' zones count only when the version has no
    // template of its own (one framing it).
    struct NotShown {
        const Zone *zone = nullptr;
        bool hidden = false;
    };
    QList<NotShown> notOnVariant(const QString &variantId) const;

    // Every table name in the layout (its table zones).
    QStringList tableLabels() const;
    // A table name not used yet, following `like`: "T7" -> "T8" (the next
    // free number after the highest with that prefix), "Patio" -> "Patio 2".
    QString nextTableLabel(const QString &like = QStringLiteral("T1")) const;

    // False (with an error) when `o` carries a schemaVersion newer than ours.
    static bool checkSchema(const QJsonObject &o, const QString &what, QStringList *errors = nullptr);

    static std::optional<Layout> fromJson(const QJsonObject &o, QStringList *errors = nullptr);
    QJsonObject toJson() const;

    static std::optional<Layout> loadDirectory(const QString &dir, QStringList *errors = nullptr);
    bool saveDirectory(const QString &dir, QStringList *errors = nullptr) const;

    bool operator==(const Layout &) const = default;
};

// The parts of a layout not every page editor may change: the templates every
// page builds on (the owner's: the ready-made layouts change them too), and
// the store's own pages, gone (login, tables, pay... a page with a role: the
// maintainer's). What `after` changes of them, said for a person ("" if
// nothing); *system: it's a store's own page.
QString protectedEdit(const Layout &before, const Layout &after, bool *system = nullptr);

} // namespace vt::layout
