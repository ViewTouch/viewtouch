#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <csignal>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The program checking itself while it runs: each check's totals as it
// closes, the database in full and a restore drill after each backup, and
// where in the code a crash was.

namespace {

QString seeded(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(u"viewtouch.db"_s);
    const auto seed = test::seedPosData();
    storage::LayoutStore pages(path);
    REQUIRE(pages.open());
    storage::PosStore store(path);
    REQUIRE(store.open());
    REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    return path;
}

bool hasProblem(PosService &pos, const std::string &kind)
{
    return std::ranges::any_of(pos.shared()->problems, [&](const core::Problem &p) { return p.kind == kind; });
}

} // namespace

TEST_CASE("Books: a check's totals that don't add up are named, ones that do aren't", "[integrity][books]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"cobb"_s));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    const core::Check &c = pos.shared()->closedToday.back();
    core::Totals t = c.totals(*c.closedRates);
    CHECK(core::booksProblem(t).empty());
    CHECK_FALSE(hasProblem(pos, "books"));   // closing checked it: nothing to say

    core::Totals wrong = t;
    wrong.total = wrong.total + vt::Money::fromCents(1);
    CHECK(core::booksProblem(wrong).starts_with("total "));
    wrong = t;
    wrong.subtotal = wrong.subtotal - vt::Money::fromCents(5);
    CHECK(core::booksProblem(wrong).starts_with("subtotal "));
    wrong = t;
    wrong.balance = wrong.balance + vt::Money::fromCents(1);
    CHECK(core::booksProblem(wrong).starts_with("balance "));
    wrong = t;
    wrong.change = wrong.change + vt::Money::fromCents(100);
    CHECK(core::booksProblem(wrong).starts_with("change "));

    // Other rates than it closed with: still adds up (all its parts move together).
    core::Check other = c;
    other.closedRates->foodPpm += 10'000;
    pos.shared()->checkBooks(other);
    CHECK_FALSE(hasProblem(pos, "books"));
    // A check that doesn't add up goes to Store Check, by its number.
    pos.shared()->noteProblem("books", "#" + std::to_string(c.id), "total $1.00 isn't ...");
    const QVariantMap check = pos.storeCheck();
    CHECK(QString::fromUtf8(QJsonDocument::fromVariant(check).toJson()).contains(
        u"Check #%1's totals don't add up"_s.arg(c.id)));
}

TEST_CASE("Backups: each one opened as a store (a restore drill), the database checked in full", "[integrity][backup]")
{
    QTemporaryDir dir;
    const QString db = seeded(dir);
    storage::BackupScheduler scheduler(db, dir.filePath(u"backups"_s), 5, 0);
    QSignalSpy done(&scheduler, &storage::BackupScheduler::finished);
    QSignalSpy checked(&scheduler, &storage::BackupScheduler::checked);
    scheduler.backupNow();
    REQUIRE(checked.wait(20000));
    REQUIRE(done.size() == 1);
    CHECK(done.first()[0].toBool());
    CHECK(checked.first()[0].toString().isEmpty());   // the backup loads
    CHECK(checked.first()[1].toString().isEmpty());   // the database is whole
    // The drill leaves nothing behind, and the backup as it was.
    const QString backup = done.first()[1].toString();
    CHECK(QDir(dir.filePath(u"backups"_s)).entryList({u"*.drill*"_s}, QDir::Files).isEmpty());
    CHECK(storage::verifyDatabase(backup));

    // A backup that's a database but not a store (no staff): the drill says so.
    QTemporaryDir other;
    const QString empty = other.filePath(u"viewtouch.db"_s);
    {
        storage::LayoutStore pages(empty);
        REQUIRE(pages.open());
        storage::PosStore store(empty);
        REQUIRE(store.open());
    }
    QString why;
    CHECK_FALSE(storage::backupLoads(empty, &why));
    CHECK_FALSE(why.isEmpty());
    CHECK(storage::backupLoads(backup, &why));

    // The database damaged in use: the full check after the backup finds it.
    {
        QFile f(db);
        REQUIRE(f.open(QIODevice::ReadWrite));
        for (qint64 at = 4096 + 100; at < f.size(); at += 4096) {
            f.seek(at);
            f.write(QByteArray(400, '\x5a'));
        }
    }
    QString damage;
    CHECK_FALSE(storage::databaseIntact(db, &damage, true));
    CHECK_FALSE(damage.isEmpty());
    checked.clear();
    scheduler.backupNow();
    REQUIRE(checked.wait(20000));
    CHECK_FALSE(checked.first()[1].toString().isEmpty());
}

TEST_CASE("Crashes: where in the code it was, saved in the data folder", "[integrity][crash][program]")
{
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    SKIP("the sanitizers report crashes themselves");
#endif
    QTemporaryDir dir;
    QProcess program;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"VTM_HEALTHY_AFTER_MS"_s, u"300"_s);
    program.setProcessEnvironment(env);
    program.start(QString::fromLocal8Bit(VTM_PROGRAM),
                  {u"--data-dir"_s, dir.path(), u"--headless"_s, u"--serve"_s, u"--port"_s, u"47392"_s});
    REQUIRE(program.waitForStarted(10000));
    for (int i = 0; i < 300 && !QFile::exists(dir.filePath(u"healthy"_s)); ++i)
        QTest::qWait(50);
    REQUIRE(QFile::exists(dir.filePath(u"healthy"_s)));
    ::kill(pid_t(program.processId()), SIGSEGV);
    REQUIRE(program.waitForFinished(10000));
    CHECK(program.exitStatus() == QProcess::CrashExit);
    const QFileInfoList crashes = QDir(dir.filePath(u"crashes"_s)).entryInfoList({u"crash-*.txt"_s}, QDir::Files);
    REQUIRE(crashes.size() == 1);
    QFile f(crashes.first().filePath());
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    INFO(text.toStdString());
    CHECK(text.startsWith(u"ViewTouch "_s));
    CHECK(text.contains(u"crashed: SIGSEGV"_s));
    CHECK(text.count(u'\n') > 3);   // the frames
}
