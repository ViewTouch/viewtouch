#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QStringView>

namespace vt::layout {

// Dotted-path access into nested JSON objects, e.g. "style.normal.fill".
// The editor edits every field through these, so the inspector can stay
// generic (driven by the field schema) instead of one setter per property.

// Undefined when any segment is missing.
QJsonValue jsonGet(const QJsonObject &root, QStringView path);

// Creates intermediate objects as needed. Setting an undefined value removes
// the key and prunes parents that become empty, so "reset to inherited"
// leaves no residue in the saved file.
void jsonSet(QJsonObject &root, QStringView path, const QJsonValue &value);

} // namespace vt::layout
