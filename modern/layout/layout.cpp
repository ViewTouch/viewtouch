#include "layout/layout.hh"

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

Layout::StarterUpdate Layout::updateFromStarter(const Layout &starter, const QStringList &seen,
                                                const QHash<QString, QString> &installed)
{
    StarterUpdate out;
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
        if (was == installed.cend())
            continue;   // not known to be the starter version: leave it
        if (*was == print) {
            out.installed.insert(fresh.id, print);   // unchanged starter page
        } else if (fingerprint(*mine) == *was) {   // never edited: take the new one
            *mine = fresh;
            out.updated << fresh.id;
            out.installed.insert(fresh.id, print);
        } else {
            out.installed.insert(fresh.id, *was);   // edited here: keep theirs
        }
    }
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

} // namespace vt::layout
