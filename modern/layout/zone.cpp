#include "layout/zone.hh"

#include <QJsonArray>

#include <cmath>

using namespace Qt::StringLiterals;

namespace vt::layout {

namespace {
const QStringList kZoneKeys = {
    u"id"_s, u"kind"_s, u"name"_s, u"label"_s, u"rect"_s, u"z"_s, u"shape"_s,
    u"behavior"_s, u"style"_s, u"hotkey"_s, u"group"_s, u"imagePath"_s,
    u"enabled"_s, u"actions"_s, u"props"_s,
};
} // namespace

int rotationOf(const Zone &z)
{
    const int r = int(std::lround(z.extra.value(u"rotation").toDouble())) % 360;
    return r < 0 ? r + 360 : r;
}

Zone Zone::fromJson(const QJsonObject &o)
{
    Zone z;
    z.id = o.value(u"id").toString();
    z.kind = o.value(u"kind").toString(z.kind);
    z.name = o.value(u"name").toString();
    z.label = o.value(u"label").toString();
    const QJsonObject r = o.value(u"rect").toObject();
    z.rect = QRect(r.value(u"x").toInt(), r.value(u"y").toInt(),
                   r.value(u"w").toInt(), r.value(u"h").toInt());
    z.z = o.value(u"z").toInt();
    z.shape = o.value(u"shape").toString(z.shape);
    z.behavior = o.value(u"behavior").toString(z.behavior);
    z.style = Style::fromJson(o.value(u"style").toObject());
    z.hotkey = o.value(u"hotkey").toString();
    z.group = o.value(u"group").toString();
    z.imagePath = o.value(u"imagePath").toString();
    z.enabled = o.value(u"enabled").toBool(true);
    for (const QJsonValue &a : o.value(u"actions").toArray())
        z.actions.append(Action{a.toObject()});
    z.props = o.value(u"props").toObject();

    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!kZoneKeys.contains(it.key()))
            z.extra.insert(it.key(), it.value());
    }
    return z;
}

QJsonObject Zone::toJson() const
{
    QJsonObject o = extra;
    o.insert(u"id", id);
    o.insert(u"kind", kind);
    if (!name.isEmpty()) o.insert(u"name", name);
    if (!label.isEmpty()) o.insert(u"label", label);
    o.insert(u"rect", QJsonObject{{u"x"_s, rect.x()}, {u"y"_s, rect.y()},
                                  {u"w"_s, rect.width()}, {u"h"_s, rect.height()}});
    if (z != 0) o.insert(u"z", z);
    o.insert(u"shape", shape);
    o.insert(u"behavior", behavior);
    if (!style.isEmpty()) o.insert(u"style", style.toJson());
    if (!hotkey.isEmpty()) o.insert(u"hotkey", hotkey);
    if (!group.isEmpty()) o.insert(u"group", group);
    if (!imagePath.isEmpty()) o.insert(u"imagePath", imagePath);
    if (!enabled) o.insert(u"enabled", false);
    if (!actions.isEmpty()) {
        QJsonArray arr;
        for (const Action &a : actions)
            arr.append(a.data);
        o.insert(u"actions", arr);
    }
    if (!props.isEmpty()) o.insert(u"props", props);
    return o;
}

} // namespace vt::layout
