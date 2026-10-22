#include "layout/layout.hh"

#include <QObject>

#include "layout/json_path.hh"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace vt::layout {

namespace {

const QStringList kRequiredRoles = {u"login"_s};
const QStringList kTargetedJumpModes = {u"push"_s, u"replace"_s};
const QStringList kJumpModes = {u"push"_s, u"replace"_s, u"back"_s, u"home"_s, u"index"_s,
                                u"sequence"_s};

void addError(QStringList *errors, const QString &msg)
{
    if (errors)
        errors->append(msg);
}

std::optional<QJsonObject> readJsonObject(const QString &path, QStringList *errors)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        addError(errors, u"%1: cannot open (%2)"_s.arg(path, f.errorString()));
        return std::nullopt;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        addError(errors, u"%1: invalid JSON at offset %2: %3"_s.arg(path).arg(err.offset).arg(err.errorString()));
        return std::nullopt;
    }
    return doc.object();
}

bool writeJsonObject(const QString &path, const QJsonObject &o, QStringList *errors)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        addError(errors, u"%1: cannot write (%2)"_s.arg(path, f.errorString()));
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        addError(errors, u"%1: cannot commit (%2)"_s.arg(path, f.errorString()));
        return false;
    }
    return true;
}

} // namespace

bool Layout::checkSchema(const QJsonObject &o, const QString &what, QStringList *errors)
{
    const int v = o.value(u"schemaVersion").toInt(SchemaVersion);
    if (v > SchemaVersion) {
        addError(errors, u"%1: schemaVersion %2 is newer than supported (%3)"_s
                             .arg(what).arg(v).arg(SchemaVersion));
        return false;
    }
    return true;
}

const Page *Layout::page(const QString &id) const
{
    if (id.isEmpty())
        return nullptr;
    for (const Page &p : pages) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

Page *Layout::page(const QString &id)
{
    // Must iterate non-const so the list detaches: returning a pointer into
    // data still shared with another Layout copy would edit both copies.
    if (id.isEmpty())
        return nullptr;
    for (Page &p : pages) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

const Page *Layout::pageByRole(const QString &role) const
{
    if (role.isEmpty())
        return nullptr;
    for (const Page &p : pages) {
        if (p.role == role)
            return &p;
    }
    return nullptr;
}

const Page *Layout::firstPageOfKind(const QString &kind) const
{
    for (const Page &p : pages) {
        if (p.kind == kind)
            return &p;
    }
    return nullptr;
}

QString Layout::resolveTarget(const QJsonObject &params) const
{
    if (const Page *p = page(params.value(u"page").toString()))
        return p->id;
    if (const Page *p = pageByRole(params.value(u"role").toString()))
        return p->id;
    return {};
}

QList<const Page *> Layout::templateChain(const QString &pageId) const
{
    QList<const Page *> chain;
    QSet<QString> seen;
    const Page *p = page(pageId);
    while (p && !seen.contains(p->id) && chain.size() <= MaxTemplateDepth) {
        seen.insert(p->id);
        chain.append(p);
        p = page(p->templateId);
    }
    return chain;
}

QList<Layout::PlacedZone> Layout::effectiveZones(const QString &pageId) const
{
    QList<PlacedZone> out;
    const QList<const Page *> chain = templateChain(pageId);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        const Page *owner = *it;
        QList<const Zone *> zones;
        for (const Zone &z : owner->zones)
            zones.append(&z);
        std::ranges::stable_sort(zones, {}, [](const Zone *z) { return z->z; });
        for (const Zone *z : zones)
            out.append({z, owner, owner->id != pageId});
    }
    return out;
}

QJsonObject Layout::resolveStyle(const Zone &zone, const QString &viewingPageId, ZoneState state) const
{
    const QList<const Page *> chain = templateChain(viewingPageId);
    const auto kindIt = theme.kinds.constFind(zone.kind);
    const Style *kindStyle = kindIt != theme.kinds.cend() ? &*kindIt : nullptr;

    QJsonObject s = zone.style.state(state);
    for (const Page *p : chain)
        Style::mergeMissing(s, p->style.state(state));
    if (kindStyle)
        Style::mergeMissing(s, kindStyle->state(state));
    Style::mergeMissing(s, theme.style.state(state));

    if (state != ZoneState::Normal)
        Style::mergeMissing(s, resolveStyle(zone, viewingPageId, ZoneState::Normal));
    return s;
}

QJsonObject Layout::resolveBackground(const QString &pageId) const
{
    QJsonObject bg;
    for (const Page *p : templateChain(pageId))
        Style::mergeMissing(bg, p->background);
    Style::mergeMissing(bg, theme.background);
    return bg;
}

QString Layout::fingerprint(const Page &page)
{
    // QJsonObject keeps its keys sorted, so equal pages give equal text.
    return QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(page.toJson()).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256)
            .toHex());
}

const QStringList &Layout::newStarterButtons()
{
    static const QStringList buttons = {u"check-options#tax-exempt"_s};
    return buttons;
}

namespace {

// A new starter button onto a page edited here: where the starter has it if
// that's free; else into its column of buttons (the same x and width) after
// the one above it in the starter, the column's buttons made shorter to fit
// in the same space (no shorter than a finger: 64 units).
bool placeNewButton(Page &mine, const Page &fresh, Zone zone)
{
    const auto free = [&](const QRect &r) {
        return std::ranges::none_of(mine.zones, [&](const Zone &z) { return z.rect.intersects(r); });
    };
    if (free(zone.rect)) {
        mine.zones.append(zone);
        return true;
    }
    const Zone *above = nullptr;
    for (const Zone &z : fresh.zones)
        if (z.rect.x() == zone.rect.x() && z.rect.width() == zone.rect.width() && z.rect.bottom() < zone.rect.top()
            && (!above || z.rect.bottom() > above->rect.bottom()) && mine.zone(z.id))
            above = &z;
    if (!above)
        return false;
    const Zone *anchor = mine.zone(above->id);
    // The column here: the anchor and the buttons right under it, one after another.
    QList<Zone *> column;
    for (Zone &z : mine.zones)
        if (z.rect.x() == anchor->rect.x() && z.rect.width() == anchor->rect.width() && z.rect.top() >= anchor->rect.top())
            column << &z;
    std::ranges::sort(column, {}, [](const Zone *z) { return z->rect.top(); });
    int gap = 0;
    for (qsizetype i = 1; i < column.size(); ++i) {
        const int g = column[i]->rect.top() - column[i - 1]->rect.bottom() - 1;
        if (g < 0 || (i > 1 && g != gap) || g > 40) {
            column.resize(i);
            break;
        }
        gap = g;
    }
    if (column.size() == 1)
        gap = 12;
    const int top = column.front()->rect.top(), bottom = column.back()->rect.bottom();
    const int n = int(column.size()) + 1;
    const int h = ((bottom - top + 1) - gap * (n - 1)) / n;
    if (h < 64)
        return false;
    const qsizetype at = std::ranges::find(column, mine.zone(anchor->id)) - column.begin() + 1;
    zone.rect.setWidth(anchor->rect.width());
    zone.rect.moveLeft(anchor->rect.x());
    // Pointers into mine.zones: lay out first, then append (which may move them).
    QList<QRect> rects;
    for (int i = 0, y = top; i < n; ++i, y += h + gap)
        rects << QRect(anchor->rect.x(), y, anchor->rect.width(), h);
    for (qsizetype i = 0, r = 0; i < column.size(); ++i, ++r) {
        if (r == at)
            ++r;
        column[i]->rect = rects[r];
    }
    zone.rect = rects[at];
    mine.zones.append(zone);
    return true;
}

} // namespace

Layout::StarterUpdate Layout::updateFromStarter(const Layout &starter, const QStringList &seen,
                                                const QHash<QString, QString> &installed)
{
    StarterUpdate out;
    const bool buttonsTracked = std::ranges::any_of(seen, [](const QString &s) { return s.contains(u'#'); });
    // Buttons new in this version, onto a starter page kept but edited here.
    const auto addNewButtons = [&](Page &mine, const Page &fresh) {
        for (const Zone &z : fresh.zones) {
            const QString key = fresh.id + u'#' + z.id;
            if (mine.zone(z.id) || seen.contains(key) || (!buttonsTracked && !newStarterButtons().contains(key)))
                continue;
            if (placeNewButton(mine, fresh, z))
                out.buttonsAdded << key;
            else
                out.buttonsNoRoom << key;
        }
    };
    for (const Page &fresh : starter.pages) {
        const QString print = fingerprint(fresh);
        Page *mine = page(fresh.id);
        if (!mine) {
            if (!seen.contains(fresh.id)) {   // new in this version (not deleted on purpose)
                pages.append(fresh);
                out.added << fresh.id;
                out.installed.insert(fresh.id, print);
            }
            continue;
        }
        const auto was = installed.constFind(fresh.id);
        if (was == installed.cend()) {
            addNewButtons(*mine, fresh);
            continue;   // not known to be the starter version: leave it
        }
        if (*was == print) {
            out.installed.insert(fresh.id, print);   // unchanged starter page
        } else if (fingerprint(*mine) == *was) {   // never edited: take the new one
            *mine = fresh;
            out.updated << fresh.id;
            out.installed.insert(fresh.id, print);
        } else {
            out.installed.insert(fresh.id, *was);   // edited here: keep theirs
            addNewButtons(*mine, fresh);
        }
    }
    syncVariants();
    return out;
}

const Page *Layout::variantFor(const QString &pageId, const QString &formFactor) const
{
    for (const Page &p : pages) {
        if (!p.variantOf.isEmpty() && p.variantOf == pageId && p.formFactor == formFactor)
            return &p;
    }
    return nullptr;
}

Layout::PlacedZone Layout::sourceOf(const Page &variant, const QString &zoneId) const
{
    // The page's own zone first, then its templates'.
    for (const Page *p : templateChain(variant.variantOf)) {
        if (const Zone *z = p->zone(zoneId))
            return {z, p, p->id != variant.variantOf};
    }
    return {};
}

QStringList Layout::clearFloors()
{
    const Page *floor = pageByRole(u"tables"_s);
    if (!floor)
        return {};
    const QString floorId = floor->id;
    QStringList gone;
    for (const Page &p : pages)
        if (p.id != floorId && p.variantOf.isEmpty() && p.kind != u"template" && p.kind != u"library"
            && std::ranges::any_of(p.zones, [](const Zone &z) { return z.kind == u"table"; }))
            gone << p.id;
    for (const Page &p : pages)
        if (gone.contains(p.variantOf))
            gone << p.id;
    pages.removeIf([&](const Page &p) { return gone.contains(p.id); });
    // A button to a plan that's gone, and on the floor its own tab (no
    // other plans to switch to), its tables and decor.
    const auto goesTo = [](const Zone &z, const QString &id) {
        return std::ranges::any_of(z.actions, [&](const Action &a) {
            return a.type() == u"jump" && a.str(u"page") == id;
        });
    };
    for (Page &p : pages) {
        p.zones.removeIf([&](const Zone &z) {
            if (p.id == floorId && (z.kind == u"table" || z.kind == u"decor" || goesTo(z, floorId)))
                return true;
            return std::ranges::any_of(gone, [&](const QString &id) { return goesTo(z, id); });
        });
        for (Zone &z : p.zones)
            if (z.kind == u"floorPlan" && gone.contains(z.props.value(u"plan").toString()))
                z.props.remove(u"plan");
    }
    syncVariants();
    return gone;
}

void Layout::syncVariants()
{
    for (qsizetype i = 0; i < pages.size(); ++i) {
        const Page &v = std::as_const(pages)[i];
        if (v.variantOf.isEmpty() || !page(v.variantOf))
            continue;   // no page to follow (the page check says so)
        QList<Zone> zones;
        bool changed = false;
        for (const Zone &mine : v.zones) {
            if (!mine.shared) {
                zones.append(mine);
                continue;
            }
            const PlacedZone from = sourceOf(v, mine.id);
            if (!from.zone) {   // deleted there: gone here too
                changed = true;
                continue;
            }
            // Theirs, but where it sits and the fields kept here.
            QJsonObject json = from.zone->toJson();
            const QJsonObject here = mine.toJson();
            json.insert(u"rect", here.value(u"rect"));
            json.remove(u"z");
            json.remove(u"rotation");
            for (const char16_t *key : {u"z", u"rotation"}) {
                if (here.contains(QStringView(key)))
                    json.insert(QStringView(key), here.value(QStringView(key)));
            }
            for (const QString &path : mine.own)
                jsonSet(json, path, jsonGet(here, path));
            Zone z = Zone::fromJson(json);
            z.shared = true;
            z.own = mine.own;
            changed = changed || !(z == mine);
            zones.append(z);
        }
        // Hidden zones that are gone there are forgotten.
        QStringList hidden = v.hiddenZones;
        hidden.removeIf([&](const QString &id) { return !sourceOf(v, id).zone; });
        if (hidden != v.hiddenZones)
            changed = true;
        if (changed) {   // only then: unchanged pages stay shared with the undo snapshots
            pages[i].zones = zones;
            pages[i].hiddenZones = hidden;
        }
    }
}

QList<Layout::NotShown> Layout::notOnVariant(const QString &variantId) const
{
    QList<NotShown> out;
    const Page *v = page(variantId);
    if (!v || v->variantOf.isEmpty() || !page(v->variantOf))
        return out;
    for (const PlacedZone &pz : effectiveZones(v->variantOf)) {
        const Zone &z = *pz.zone;
        if ((pz.inherited && !v->templateId.isEmpty()) || z.kind == u"comment" || z.kind == u"decor"
            || z.extra.value(u"showWhen").toObject().value(u"screen").toString() == u"standard")
            continue;
        if (v->zone(z.id))
            continue;
        out.append({&z, v->hiddenZones.contains(z.id)});
    }
    return out;
}

QStringList Layout::tableLabels() const
{
    QStringList out;
    for (const Page &p : pages) {
        for (const Zone &z : p.zones) {
            if (z.kind == u"table")
                out.append(z.label.trimmed());
        }
    }
    return out;
}

QString Layout::nextTableLabel(const QString &like) const
{
    static const QRegularExpression numbered(u"^(.*?)(\\d+)$"_s);
    const QStringList taken = tableLabels();
    const QRegularExpressionMatch m = numbered.match(like.trimmed());
    const QString prefix = m.hasMatch() ? m.captured(1) : like.trimmed() + u' ';
    int highest = m.hasMatch() ? 0 : 1;   // "Patio" counts as "Patio 1"
    for (const QString &t : taken) {
        if (t.startsWith(prefix, Qt::CaseInsensitive)) {
            bool ok = false;
            const int n = t.mid(prefix.size()).toInt(&ok);
            if (ok)
                highest = std::max(highest, n);
        }
    }
    QString label = prefix + QString::number(highest + 1);
    for (int n = highest + 2; taken.contains(label, Qt::CaseInsensitive); ++n)
        label = prefix + QString::number(n);
    return label;
}

QStringList Layout::validate() const
{
    QStringList issues;
    QSet<QString> pageIds;

    for (const QString &role : kRequiredRoles) {
        if (!pageByRole(role))
            issues << u"no page has required role '%1'"_s.arg(role);
    }

    // A table's name is the table: on one floor plan once; on another plan
    // (Main and Dining Area) the same name is the same table.
    for (const Page &p : pages) {
        QSet<QString> tables;
        for (const Zone &z : p.zones) {
            if (z.kind != u"table")
                continue;
            const QString t = z.label.trimmed();
            if (t.isEmpty())
                issues << u"a table has no name"_s;
            else if (tables.contains(t.toLower()))
                issues << u"table '%1' is on page '%2' more than once"_s.arg(t, p.id);
            tables.insert(t.toLower());
        }
    }

    QSet<QString> roles;
    for (const Page &p : pages) {
        const QString where = u"page '%1'"_s.arg(p.id);
        if (p.id.isEmpty())
            issues << u"a page has an empty id"_s;
        if (pageIds.contains(p.id))
            issues << u"%1: duplicate page id"_s.arg(where);
        pageIds.insert(p.id);
        if (!p.role.isEmpty()) {
            if (roles.contains(p.role))
                issues << u"%1: role '%2' is used by more than one page"_s.arg(where, p.role);
            roles.insert(p.role);
        }
        if (!p.templateId.isEmpty()) {
            if (!page(p.templateId))
                issues << u"%1: template '%2' does not exist"_s.arg(where, p.templateId);
            const QList<const Page *> chain = templateChain(p.id);
            if (!chain.isEmpty() && !chain.last()->templateId.isEmpty() && page(chain.last()->templateId))
                issues << u"%1: template chain loops or is deeper than %2"_s.arg(where).arg(MaxTemplateDepth);
        }

        if (!p.variantOf.isEmpty()) {
            if (!page(p.variantOf))
                issues << u"%1: it is a version of '%2', which does not exist"_s.arg(where, p.variantOf);
            else if (variantFor(p.variantOf, p.formFactor) != &p)
                issues << u"%1: '%2' has more than one %3 version"_s.arg(where, p.variantOf, p.formFactor);
            for (const NotShown &n : notOnVariant(p.id)) {
                if (!n.hidden)
                    issues << u"%1: '%2' of '%3' is not on it (place it, or hide it there)"_s
                                  .arg(where, n.zone->label.isEmpty() ? n.zone->id : n.zone->label.simplified(), p.variantOf);
            }
        }

        QSet<QString> zoneIds;
        for (const Zone &z : p.zones) {
            const QString zwhere = u"%1 zone '%2'"_s.arg(where, z.id);
            if (z.id.isEmpty())
                issues << u"%1: a zone has an empty id"_s.arg(where);
            if (zoneIds.contains(z.id))
                issues << u"%1: duplicate zone id"_s.arg(zwhere);
            zoneIds.insert(z.id);
            if (z.rect.width() < 16 || z.rect.height() < 16)
                issues << u"%1: smaller than 16x16"_s.arg(zwhere);
            if (!QRect(QPoint(0, 0), p.canvas).contains(z.rect))
                issues << u"%1: extends outside the %2x%3 canvas"_s.arg(zwhere).arg(p.canvas.width()).arg(p.canvas.height());

            for (const Action &a : z.actions) {
                if (a.type() == u"jump") {
                    const QString mode = a.data.value(u"mode").toString(u"push"_s);
                    if (!kJumpModes.contains(mode))
                        issues << u"%1: unknown jump mode '%2'"_s.arg(zwhere, mode);
                    else if (kTargetedJumpModes.contains(mode) && resolveTarget(a.data).isEmpty())
                        issues << u"%1: jump target does not resolve"_s.arg(zwhere);
                } else if (a.type() == u"addItem") {
                    if (a.str(u"item").isEmpty())
                        issues << u"%1: addItem without an item"_s.arg(zwhere);
                    for (const QJsonValue &v : a.data.value(u"modifierSequence").toArray()) {
                        if (!page(v.toString()))
                            issues << u"%1: modifier page '%2' does not exist"_s.arg(zwhere, v.toString());
                    }
                } else if (a.type().isEmpty()) {
                    issues << u"%1: action without a type"_s.arg(zwhere);
                }
            }
        }
    }
    return issues;
}

std::optional<Layout> Layout::fromJson(const QJsonObject &o, QStringList *errors)
{
    if (!Layout::checkSchema(o, u"layout"_s, errors))
        return std::nullopt;
    Layout l;
    l.theme = Theme::fromJson(o.value(u"theme").toObject());
    for (const QJsonValue &v : o.value(u"pages").toArray())
        l.pages.append(Page::fromJson(v.toObject()));
    l.syncVariants();
    return l;
}

QJsonObject Layout::toJson() const
{
    QJsonArray arr;
    for (const Page &p : pages)
        arr.append(p.toJson());
    return {{u"schemaVersion"_s, SchemaVersion}, {u"theme"_s, theme.toJson()}, {u"pages"_s, arr}};
}

std::optional<Layout> Layout::loadDirectory(const QString &dir, QStringList *errors)
{
    const QDir root(dir);
    Layout l;

    const auto themeJson = readJsonObject(root.filePath(u"theme.json"_s), errors);
    if (!themeJson || !Layout::checkSchema(*themeJson, u"theme.json"_s, errors))
        return std::nullopt;
    l.theme = Theme::fromJson(*themeJson);

    const QDir pagesDir(root.filePath(u"pages"_s));
    const QStringList files = pagesDir.entryList({u"*.json"_s}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString path = pagesDir.filePath(file);
        const auto obj = readJsonObject(path, errors);
        if (!obj || !Layout::checkSchema(*obj, path, errors))
            continue;
        l.pages.append(Page::fromJson(*obj));
    }
    if (l.pages.isEmpty()) {
        addError(errors, u"%1: no pages found"_s.arg(pagesDir.path()));
        return std::nullopt;
    }
    l.syncVariants();
    return l;
}

bool Layout::saveDirectory(const QString &dir, QStringList *errors) const
{
    QDir root(dir);
    if (!root.mkpath(u"pages"_s)) {
        addError(errors, u"%1: cannot create directory"_s.arg(dir));
        return false;
    }

    QJsonObject themeJson = theme.toJson();
    themeJson.insert(u"schemaVersion", SchemaVersion);
    bool ok = writeJsonObject(root.filePath(u"theme.json"_s), themeJson, errors);

    QDir pagesDir(root.filePath(u"pages"_s));
    QSet<QString> written;
    for (const Page &p : pages) {
        QJsonObject o = p.toJson();
        o.insert(u"schemaVersion", SchemaVersion);
        const QString file = p.id + u".json"_s;
        ok = writeJsonObject(pagesDir.filePath(file), o, errors) && ok;
        written.insert(file);
    }
    for (const QString &file : pagesDir.entryList({u"*.json"_s}, QDir::Files)) {
        if (!written.contains(file))
            pagesDir.remove(file);
    }
    return ok;
}

QString protectedEdit(const Layout &before, const Layout &after, bool *system)
{
    if (system)
        *system = false;
    for (const Page &p : before.pages)
        if (!after.page(p.id) && !p.role.isEmpty()) {
            if (system)
                *system = true;
            return QObject::tr("the %1 page (it's one of the store's own)").arg(p.name);
        }
    for (const Page &p : before.pages) {
        const Page *now = after.page(p.id);
        if (now && p.kind == u"template" && !(*now == p))
            return QObject::tr("the %1 template (every page built on it changes)").arg(p.name);
    }
    for (const Page &p : after.pages)
        if (const Page *was = before.page(p.id); was && was->kind != p.kind && (was->kind == u"template" || p.kind == u"template"))
            return QObject::tr("the %1 template").arg(p.name);
    return {};
}

} // namespace vt::layout
