#include <catch2/catch_test_macros.hpp>

#include "livelist.hh"
#include "qt_catch.hh"

#include <QSignalSpy>

using namespace Qt::StringLiterals;

// A list given again whole: only what changed reaches the view.

namespace {
QVariantMap row(int id, const QString &name) { return {{u"id"_s, id}, {u"name"_s, name}}; }
QStringList names(const LiveList &m)
{
    QStringList out;
    for (int i = 0; i < m.rowCount(); ++i)
        out << m.data(m.index(i), Qt::UserRole + 1).toMap()[u"name"_s].toString();
    return out;
}
} // namespace

TEST_CASE("LiveList: rows matched by id: added, removed, moved, changed, the rest left alone", "[livelist]")
{
    LiveList m;
    QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted), removed(&m, &QAbstractItemModel::rowsRemoved),
        moved(&m, &QAbstractItemModel::rowsMoved), changed(&m, &QAbstractItemModel::dataChanged),
        reset(&m, &QAbstractItemModel::modelReset);
    CHECK(m.roleNames().value(Qt::UserRole + 1) == "modelData");

    m.setRows({row(1, u"Cobb"_s), row(2, u"Coffee"_s)});
    CHECK(names(m) == QStringList{u"Cobb"_s, u"Coffee"_s});
    CHECK(inserted.size() == 2);

    // One more at the end: one insert, nothing else touched.
    inserted.clear();
    m.setRows({row(1, u"Cobb"_s), row(2, u"Coffee"_s), row(3, u"Tea"_s)});
    CHECK(inserted.size() == 1);
    CHECK(changed.isEmpty());
    CHECK(removed.isEmpty());

    // The middle one changed: one row's data, no rows added or taken away.
    inserted.clear();
    m.setRows({row(1, u"Cobb"_s), row(2, u"2 Coffee"_s), row(3, u"Tea"_s)});
    CHECK(changed.size() == 1);
    CHECK(changed.first()[0].toModelIndex().row() == 1);
    CHECK(inserted.isEmpty());
    CHECK(names(m) == QStringList{u"Cobb"_s, u"2 Coffee"_s, u"Tea"_s});

    // Voided: one removal. Moved to the top: one move.
    changed.clear();
    m.setRows({row(1, u"Cobb"_s), row(3, u"Tea"_s)});
    CHECK(removed.size() == 1);
    m.setRows({row(3, u"Tea"_s), row(1, u"Cobb"_s)});
    CHECK(moved.size() == 1);
    CHECK(names(m) == QStringList{u"Tea"_s, u"Cobb"_s});

    // The same list again: nothing at all.
    inserted.clear(); removed.clear(); moved.clear(); changed.clear();
    m.setRows({row(3, u"Tea"_s), row(1, u"Cobb"_s)});
    CHECK(inserted.isEmpty());
    CHECK(removed.isEmpty());
    CHECK(moved.isEmpty());
    CHECK(changed.isEmpty());
    CHECK(reset.isEmpty());   // never the whole list again

    // A new check: the old rows go, the new come.
    m.setRows({row(9, u"Burger"_s)});
    CHECK(names(m) == QStringList{u"Burger"_s});
    CHECK(m.rowCount() == 1);
    m.setRows({});
    CHECK(m.rowCount() == 0);
}
