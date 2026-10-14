#pragma once

#include "layout/layout.hh"

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace vt::storage {

// Persists the layout in SQLite: one row per page (JSON), plus the theme.
// A save replaces everything in a single transaction, so a crash mid-save
// leaves the previous layout intact. WAL journaling keeps readers unblocked.
class LayoutStore {
public:
    static constexpr int DbSchemaVersion = 1;

    explicit LayoutStore(QString databasePath);
    ~LayoutStore();
    LayoutStore(const LayoutStore &) = delete;
    LayoutStore &operator=(const LayoutStore &) = delete;

    // Opens (creating if needed) and migrates the database.
    bool open(QString *error = nullptr);
    QString path() const { return path_; }

    bool hasLayout() const;
    std::optional<layout::Layout> load(QStringList *errors = nullptr) const;
    bool save(const layout::Layout &layout, QString *error = nullptr);

    // Which starter pages this store has been given, and the fingerprint of
    // each as installed (see Layout::updateFromStarter). Empty for stores
    // from before this was kept.
    struct StarterState {
        QStringList seen;
        QHash<QString, QString> installed;
    };
    StarterState starterState() const;
    bool setStarterState(const StarterState &state, QString *error = nullptr);

private:
    QString path_;
    QString connection_;
};

} // namespace vt::storage
