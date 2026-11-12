#include <catch2/catch_test_macros.hpp>

#include "app/legacy/legacy_files.hh"
#include "app/legacy/legacy_pages.hh"
#include "qt_catch.hh"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;

// Reading the original ViewTouch's files: a real store's menu, pages and
// dining room (tests/fixtures/legacy/elmirador, by its owner's leave), and
// the system pages it downloads (tools/legacy/data/vt_data).

namespace {

QString fixture(const QString &name)
{
    return QStringLiteral(VTM_FIXTURE_DIR "/../legacy/") + name;
}

int zones(const legacy::PageFile &f)
{
    int n = 0;
    for (const legacy::Page &p : f.pages)
        n += int(p.zones.size());
    return n;
}

} // namespace

TEST_CASE("Legacy files: the words of a ViewTouch data file", "[legacy][files]")
{
    QString error;
    // Numbers in base 64; -1 written as 64 bits; "_" a space, "~" nothing;
    // a date as seconds into the year and the year.
    auto t = legacy::Tokens::fromBytes("vtpos 0 7\nB Bk P////////// Hello_there ~ 0.085\ni6Rf fq A A"_ba, u"x"_s, &error);
    REQUIRE(t);
    CHECK(t->version() == 7);
    CHECK(t->i() == 1);
    CHECK(t->i() == 100);
    CHECK(t->i() == -1);
    CHECK(t->s() == u"Hello there"_s);
    CHECK(t->s().isEmpty());
    CHECK(t->f() == 0.085);
    const legacy::WallTime when = t->t();
    CHECK(when.date == QDate(2026, 4, 16));
    CHECK(when.time == QTime(22, 6, 23));
    CHECK_FALSE(t->t().isSet());
    CHECK(t->finished());
    // Ending early, or something left over, is an error that says where.
    t = legacy::Tokens::fromBytes("vtpos 0 1 B"_ba, u"x"_s, &error);
    t->i();
    t->i();
    CHECK_FALSE(t->ok());
    CHECK(t->error().contains(u"ends early"_s));
    t = legacy::Tokens::fromBytes("vtpos 0 1 B C"_ba, u"x"_s, &error);
    t->i();
    CHECK_FALSE(t->finished());
    CHECK(t->error().contains(u"1 words left over"_s));
    // Not one of legacy's files at all.
    CHECK_FALSE(legacy::Tokens::fromBytes("hello"_ba, u"x"_s, &error));
    CHECK(error.contains(u"isn't a ViewTouch data file"_s));
}

TEST_CASE("Legacy files: a store's own pages, menu and dining room, to their last word", "[legacy][files]")
{
    QString error;
    const auto pages = legacy::readPageFile(fixture(u"elmirador/zone_db.dat"_s), &error);
    INFO(error.toStdString());
    REQUIRE(pages);
    CHECK(pages->version == 29);
    CHECK(pages->pages.size() == 112);
    CHECK(zones(*pages) == 1949);
    CHECK(pages->defaults);
    CHECK(pages->pages.first().name == u"APPS"_s);
    const auto index = std::ranges::find(pages->pages, 60, &legacy::Page::id);
    REQUIRE(index != pages->pages.end());
    CHECK(index->name == u"Index"_s);
    CHECK(index->type == 2);

    const auto tables = legacy::readPageFile(fixture(u"elmirador/tables.dat"_s), &error);
    REQUIRE(tables);
    REQUIRE(tables->pages.size() == 1);
    CHECK(tables->pages.first().id == 50);
    CHECK(tables->pages.first().zones.size() == 25);
    CHECK(std::ranges::count(tables->pages.first().zones, u"ZONE_TABLE"_s, &legacy::Zone::kind) > 0);

    const auto menu = legacy::readMenu(fixture(u"elmirador/menu.dat"_s), &error);
    INFO(error.toStdString());
    REQUIRE(menu);
    CHECK(menu->version == 17);
    CHECK(menu->items.size() == 1458);
    const auto plain = std::ranges::find(menu->items, u"*PLAIN*"_s, &legacy::MenuItem::name);
    REQUIRE(plain != menu->items.end());
    CHECK(plain->type == 1);   // a modifier
    CHECK(std::ranges::any_of(menu->items, [](const legacy::MenuItem &m) { return m.cost == 995; }));

    // The system pages legacy downloads, kept in ViewTouch Modern.
    const auto system = legacy::readVtData(QStringLiteral(VTM_SEED_DIR "/../tools/legacy/data/vt_data"), &error);
    INFO(error.toStdString());
    REQUIRE(system);
    CHECK(system->pages.size() == 168);
    CHECK(zones(*system) == 4156);
}

TEST_CASE("Legacy files: a damaged file says which, and where", "[legacy][files]")
{
    QTemporaryDir dir;
    QFile f(dir.filePath(u"menu.dat"_s));
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("vtpos 0 17\nC B Tacos ~ ~ A ~ ~ ~ ~ ~ D/ A");   // two items said, one begun
    f.close();
    QString error;
    CHECK_FALSE(legacy::readMenu(f.fileName(), &error));
    CHECK(error.startsWith(u"menu.dat: it ends early"_s));
    // A version this doesn't know.
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("vtpos 0 99\nA");
    f.close();
    CHECK_FALSE(legacy::readMenu(f.fileName(), &error));
    CHECK(error.contains(u"version 99"_s));
    // The folder: what's there is read, each failure said on its own.
    CHECK(legacy::isDatFolder(dir.path()));
    const legacy::Store s = legacy::readStore(dir.path());
    CHECK_FALSE(s.menu);
    CHECK(s.errors.size() == 1);
}

// The whole of a real store, when one is at hand (never in the repo: its
// staff, customers and sales are its own). VTM_LEGACY_DAT=<its dat folder>.
TEST_CASE("Legacy files: a whole store's dat folder", "[legacy][files][local]")
{
    const QString dat = qEnvironmentVariable("VTM_LEGACY_DAT");
    if (dat.isEmpty())
        SKIP("VTM_LEGACY_DAT not set");
    const legacy::Store s = legacy::readStore(dat);
    INFO(s.errors.join(u'\n').toStdString());
    CHECK(s.errors.isEmpty());
    REQUIRE(s.settings);
    CHECK_FALSE(s.settings->storeName.isEmpty());
    CHECK(s.settings->receiptHeader.size() == 4);
    CHECK(s.menu);
    CHECK(s.pages);
    CHECK_FALSE(s.employees.isEmpty());
    CHECK_FALSE(s.archives.isEmpty());
    qsizetype checks = 0, orders = 0;
    for (const legacy::Archive &a : s.archives) {
        checks += a.checks.size();
        for (const legacy::Check &c : a.checks)
            for (const legacy::SubCheck &sc : c.subchecks)
                orders += sc.orders.size();
    }
    WARN(s.archives.size() << " days, " << checks << " checks, " << orders << " items, " << s.employees.size()
                           << " staff, " << s.customers.size() << " customers");
    // Days in order, each starting where the one before ended.
    for (qsizetype k = 1; k < s.archives.size(); ++k)
        CHECK(s.archives[k].id > s.archives[k - 1].id);
}

TEST_CASE("Legacy pages: the Legacy style's pages are legacy's own, converted", "[legacy][files]")
{
    const QString data = QStringLiteral(VTM_SEED_DIR "/../tools/legacy/data/");
    QString error;
    const auto system = legacy::readVtData(data + u"vt_data"_s, &error);
    const auto menu = legacy::readPageFile(data + u"zone_db.dat"_s, &error);
    const auto tables = legacy::readPageFile(data + u"tables.dat"_s, &error);
    REQUIRE(system);
    REQUIRE(menu);
    REQUIRE(tables);
    REQUIRE(menu->defaults);
    const legacy::PageSet set = legacy::convertPages({&*system, &*menu, &*tables}, {*menu->defaults, {}});
    // seed/legacy is what this makes (vtmodern --legacy-pages): page for page.
    const QDir seed(QStringLiteral(VTM_SEED_DIR "/legacy"));
    CHECK(set.pages.size() == qsizetype(seed.entryList({u"*.json"_s}).size()));
    for (const QJsonObject &page : set.pages) {
        const QString id = page.value(u"id"_s).toString();
        INFO(id.toStdString());
        QFile f(seed.filePath(id + u".json"_s));
        REQUIRE(f.open(QIODevice::ReadOnly));
        const QJsonObject want = QJsonDocument::fromJson(f.readAll()).object();
        if (want != page) {
            for (auto it = want.begin(); it != want.end(); ++it)
                if (page.value(it.key()) != it.value())
                    UNSCOPED_INFO("differs: " << it.key().toStdString());
            for (auto it = page.begin(); it != page.end(); ++it)
                if (!want.contains(it.key()))
                    UNSCOPED_INFO("extra: " << it.key().toStdString());
        }
        CHECK(want == page);
    }
    CHECK(set.missing.size() == 966);
}
