#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace vt::app {

// One sheet of a workbook: its rows of cells as text, in order (row 1 first;
// a row or cell left empty is an empty one).
struct Sheet {
    QString name;
    QList<QStringList> rows;
};

// The sheets of an Excel workbook (.xlsx), as their text: what the cells show
// for text and whole numbers ("012345678905" stays text; 12.5 is "12.5").
// No Excel needed: the file is a zip of XML, read here. nullopt and `error`
// for a file that isn't one, is damaged or has a password.
std::optional<QList<Sheet>> readXlsx(const QByteArray &file, QString *error = nullptr);

// The files in a zip (name -> contents), each checked; stored or deflated
// entries only. nullopt for a damaged zip, one with a password, or one that
// would unpack past `limit` bytes in all.
std::optional<QList<std::pair<QString, QByteArray>>> unzip(const QByteArray &zip, qint64 limit, QString *error = nullptr);

// Raw DEFLATE (RFC 1951) data unpacked, at most `limit` bytes; nullopt if
// it's damaged or longer.
std::optional<QByteArray> inflate(const QByteArray &deflated, qint64 limit);

} // namespace vt::app
