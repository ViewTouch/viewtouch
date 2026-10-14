#pragma once

// The starter layout for tests, plus the hand-built item pages in
// tests/fixtures/pages (the store's menu screens fill themselves; the page
// editor's tests need pages of buttons placed by hand).

#include "layout/layout.hh"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <optional>

namespace vt::test {

inline std::optional<layout::Layout> loadTestLayout()
{
    auto l = layout::Layout::loadDirectory(QStringLiteral(VTM_SEED_DIR));
    if (!l)
        return l;
    const QDir fixtures(QStringLiteral(VTM_FIXTURE_DIR));
    for (const QString &name : fixtures.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        QFile f(fixtures.filePath(name));
        if (f.open(QIODevice::ReadOnly))
            l->pages.push_back(layout::Page::fromJson(QJsonDocument::fromJson(f.readAll()).object()));
    }
    return l;
}

} // namespace vt::test
