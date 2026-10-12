#include "layout/page.hh"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace vt::layout {

namespace {
const QStringList kPageKeys = {
    u"id"_s, u"name"_s, u"kind"_s, u"role"_s, u"templateId"_s, u"mealPeriod"_s, u"permission"_s,
    u"variantOf"_s, u"formFactor"_s, u"hiddenZones"_s, u"contentArea"_s,
    u"canvas"_s, u"grid"_s, u"background"_s, u"style"_s, u"zones"_s,
    u"schemaVersion"_s,
};
const QStringList kThemeKeys = {u"name"_s, u"background"_s, u"style"_s, u"kinds"_s,
                                u"schemaVersion"_s};
} // namespace

const Zone *Page::zone(const QString &zoneId) const
{
    for (const Zone &z : zones) {
        if (z.id == zoneId)
            return &z;
    }
    return nullptr;
}

namespace {

// Floor plans used to be one "tableMap" zone listing its tables in
// props.tables. Each listed table becomes a "table" zone of its own, in the
// same place, so older pages keep working.
QList<Zone> tablesOfMap(const Zone &map, const QSet<QString> &takenIds)
{
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QList<Zone> out;
    QSet<QString> ids = takenIds;
    for (const QJsonValue &v : map.props.value(u"tables").toArray()) {
        const QJsonObject t = v.toObject();
        Zone z;
        z.kind = u"table"_s;
        z.label = t.value(u"label").toString();
        QString id = u"table-"_s + z.label.toLower().replace(nonWord, u"-"_s);
        for (int n = 2; ids.contains(id); ++n)
            id = u"table-%1-%2"_s.arg(z.label.toLower().replace(nonWord, u"-"_s)).arg(n);
        ids.insert(id);
        z.id = id;
        z.rect = QRect(map.rect.x() + t.value(u"x").toInt(), map.rect.y() + t.value(u"y").toInt(),
                       t.value(u"w").toInt(200), t.value(u"h").toInt(200));
        z.z = map.z;
        z.shape = t.value(u"shape").toString(u"rect"_s);
        z.behavior = u"none"_s;
        if (t.contains(u"seats"))
            z.props.insert(u"seats"_s, t.value(u"seats").toInt());
        out.append(z);
    }
    return out;
}

} // namespace

Page Page::fromJson(const QJsonObject &o)
{
    Page p;
    p.id = o.value(u"id").toString();
    p.name = o.value(u"name").toString();
    p.kind = o.value(u"kind").toString(p.kind);
    p.role = o.value(u"role").toString();
    p.templateId = o.value(u"templateId").toString();
    p.mealPeriod = o.value(u"mealPeriod").toString();
    p.permission = o.value(u"permission").toString();
    p.variantOf = o.value(u"variantOf").toString();
    p.formFactor = o.value(u"formFactor").toString();
    for (const QJsonValue &v : o.value(u"hiddenZones").toArray())
        p.hiddenZones.append(v.toString());
    if (const QJsonObject a = o.value(u"contentArea").toObject(); !a.isEmpty())
        p.contentArea = QRect(a.value(u"x").toInt(), a.value(u"y").toInt(), a.value(u"w").toInt(), a.value(u"h").toInt());
    const QJsonObject c = o.value(u"canvas").toObject();
    p.canvas = QSize(c.value(u"w").toInt(p.canvas.width()), c.value(u"h").toInt(p.canvas.height()));
    p.grid = o.value(u"grid").toInt(p.grid);
    p.background = o.value(u"background").toObject();
    p.style = Style::fromJson(o.value(u"style").toObject());
    for (const QJsonValue &z : o.value(u"zones").toArray())
        p.zones.append(Zone::fromJson(z.toObject()));
    for (qsizetype i = 0; i < p.zones.size(); ++i) {
        if (p.zones[i].kind != u"tableMap")
            continue;
        QSet<QString> ids;
        for (const Zone &z : std::as_const(p.zones))
            ids.insert(z.id);
        const QList<Zone> tables = tablesOfMap(p.zones[i], ids);
        p.zones.removeAt(i);
        for (qsizetype k = 0; k < tables.size(); ++k)
            p.zones.insert(i + k, tables[k]);
        i += tables.size() - 1;
    }

    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!kPageKeys.contains(it.key()))
            p.extra.insert(it.key(), it.value());
    }
    return p;
}

QJsonObject Page::toJson() const
{
    QJsonObject o = extra;
    o.insert(u"id", id);
    o.insert(u"name", name);
    o.insert(u"kind", kind);
    if (!role.isEmpty()) o.insert(u"role", role);
    if (!templateId.isEmpty()) o.insert(u"templateId", templateId);
    if (!mealPeriod.isEmpty()) o.insert(u"mealPeriod", mealPeriod);
    if (!permission.isEmpty()) o.insert(u"permission", permission);
    if (!variantOf.isEmpty()) o.insert(u"variantOf", variantOf);
    if (!formFactor.isEmpty()) o.insert(u"formFactor", formFactor);
    if (!hiddenZones.isEmpty()) o.insert(u"hiddenZones", QJsonArray::fromStringList(hiddenZones));
    if (!contentArea.isEmpty())
        o.insert(u"contentArea", QJsonObject{{u"x"_s, contentArea.x()}, {u"y"_s, contentArea.y()},
                                             {u"w"_s, contentArea.width()}, {u"h"_s, contentArea.height()}});
    o.insert(u"canvas", QJsonObject{{u"w"_s, canvas.width()}, {u"h"_s, canvas.height()}});
    o.insert(u"grid", grid);
    if (!background.isEmpty()) o.insert(u"background", background);
    if (!style.isEmpty()) o.insert(u"style", style.toJson());
    QJsonArray arr;
    for (const Zone &z : zones)
        arr.append(z.shared ? z.sharedJson() : z.toJson());
    o.insert(u"zones", arr);
    return o;
}

Theme Theme::fromJson(const QJsonObject &o)
{
    Theme t;
    t.name = o.value(u"name").toString();
    t.background = o.value(u"background").toObject();
    t.style = Style::fromJson(o.value(u"style").toObject());
    const QJsonObject kinds = o.value(u"kinds").toObject();
    for (auto it = kinds.begin(); it != kinds.end(); ++it)
        t.kinds.insert(it.key(), Style::fromJson(it.value().toObject()));

    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!kThemeKeys.contains(it.key()))
            t.extra.insert(it.key(), it.value());
    }
    return t;
}

QJsonObject Theme::toJson() const
{
    QJsonObject o = extra;
    if (!name.isEmpty()) o.insert(u"name", name);
    if (!background.isEmpty()) o.insert(u"background", background);
    if (!style.isEmpty()) o.insert(u"style", style.toJson());
    if (!kinds.isEmpty()) {
        QJsonObject k;
        for (auto it = kinds.begin(); it != kinds.end(); ++it)
            k.insert(it.key(), it.value().toJson());
        o.insert(u"kinds", k);
    }
    return o;
}

} // namespace vt::layout
