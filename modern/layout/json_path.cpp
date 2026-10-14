#include "layout/json_path.hh"

namespace vt::layout {

QJsonValue jsonGet(const QJsonObject &root, QStringView path)
{
    const qsizetype dot = path.indexOf(u'.');
    if (dot < 0)
        return root.value(path);
    const QJsonValue child = root.value(path.left(dot));
    if (!child.isObject())
        return QJsonValue(QJsonValue::Undefined);
    return jsonGet(child.toObject(), path.mid(dot + 1));
}

void jsonSet(QJsonObject &root, QStringView path, const QJsonValue &value)
{
    const qsizetype dot = path.indexOf(u'.');
    if (dot < 0) {
        if (value.isUndefined())
            root.remove(path);
        else
            root.insert(path, value);
        return;
    }
    const QStringView head = path.left(dot);
    QJsonObject child = root.value(head).toObject();
    jsonSet(child, path.mid(dot + 1), value);
    if (child.isEmpty())
        root.remove(head);
    else
        root.insert(head, child);
}

} // namespace vt::layout
