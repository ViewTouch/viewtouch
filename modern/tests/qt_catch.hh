#pragma once

// Readable Catch2 failure messages for Qt types.

#include <catch2/catch_tostring.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Catch {
template <> struct StringMaker<QString> {
    static std::string convert(const QString &s) { return '"' + s.toStdString() + '"'; }
};
template <> struct StringMaker<QStringList> {
    static std::string convert(const QStringList &l) { return '[' + l.join(u", ").toStdString() + ']'; }
};
template <> struct StringMaker<QJsonObject> {
    static std::string convert(const QJsonObject &o)
    {
        return QJsonDocument(o).toJson(QJsonDocument::Compact).toStdString();
    }
};
} // namespace Catch
