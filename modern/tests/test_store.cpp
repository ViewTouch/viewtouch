#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "storage/layout_store.hh"
#include "qt_catch.hh"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using vt::storage::LayoutStore;
using vt::layout::Layout;

TEST_CASE("LayoutStore saves and loads the whole layout", "[store]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString dbPath = dir.filePath(u"vt.db"_s);
    auto seed = vt::test::loadTestLayout();
    REQUIRE(seed);

    {
        LayoutStore store(dbPath);
        QString error;
        REQUIRE(store.open(&error));
        CHECK_FALSE(store.hasLayout());
        CHECK_FALSE(store.load());
        REQUIRE(store.save(*seed, &error));
        CHECK(store.hasLayout());
    }

    // Reopen (runs migrations again, idempotently) and read back.
    LayoutStore store(dbPath);
    REQUIRE(store.open());
    QStringList errors;
    const auto loaded = store.load(&errors);
    REQUIRE(loaded);
    CHECK(errors.isEmpty());
    CHECK(*loaded == *seed);   // page order is preserved

    // Saving a smaller layout removes pages that are gone.
    Layout smaller = *seed;
    smaller.pages.removeIf([](const vt::layout::Page &p) { return p.id == u"library"; });
    REQUIRE(store.save(smaller));
    CHECK(*store.load() == smaller);
}

TEST_CASE("LayoutStore refuses a database from a newer version", "[store]")
{
    QTemporaryDir dir;
    const QString dbPath = dir.filePath(u"vt.db"_s);
    {
        LayoutStore store(dbPath);
        REQUIRE(store.open());
    }
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"poke"_s);
        db.setDatabaseName(dbPath);
        REQUIRE(db.open());
        QSqlQuery(db).exec(u"UPDATE meta SET value = '99' WHERE key = 'schema_version'"_s);
        db.close();
    }
    QSqlDatabase::removeDatabase(u"poke"_s);

    LayoutStore store(dbPath);
    QString error;
    CHECK_FALSE(store.open(&error));
    CHECK(error.contains(u"newer"_s));
}

TEST_CASE("LayoutStore remembers which starter pages it was given", "[store][upgrade]")
{
    QTemporaryDir dir;
    const QString dbPath = dir.filePath(u"pages.db"_s);
    {
        LayoutStore store(dbPath);
        REQUIRE(store.open());
        CHECK(store.starterState().seen.isEmpty());   // a store from before this was kept
        LayoutStore::StarterState s;
        s.seen = {u"login"_s, u"tables"_s};
        s.installed.insert(u"login"_s, u"abc"_s);
        REQUIRE(store.setStarterState(s));
    }
    LayoutStore store(dbPath);
    REQUIRE(store.open());
    const LayoutStore::StarterState back = store.starterState();
    CHECK(back.seen == QStringList{u"login"_s, u"tables"_s});
    CHECK(back.installed.value(u"login"_s) == u"abc"_s);
}
