#include <catch2/catch_test_macros.hpp>

#include "app/legacy/legacy_apply.hh"
#include "app/legacy/legacy_import.hh"
#include "app/pos_json.hh"
#include "core/report.hh"
#include "layout/menu_screens.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <set>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::Money;

// A legacy ViewTouch store's menu and sales brought over: a made-up store's
// sales (tests/fixtures/legacy/sample, tools/legacy/make_sample.py) with a
// real store's menu and pages (tests/fixtures/legacy/elmirador).

namespace {

// The two fixtures as one dat folder.
QString datFolder(QTemporaryDir &dir)
{
    const QString fixtures = QStringLiteral(VTM_FIXTURE_DIR "/../legacy/");
    for (const QString &from : {fixtures + u"elmirador"_s, fixtures + u"sample"_s}) {
        QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            const QString to = dir.filePath(u"dat/"_s + QDir(from).relativeFilePath(file));
            QDir().mkpath(QFileInfo(to).absolutePath());
            QFile::copy(file, to);
        }
    }
    return dir.filePath(u"dat"_s);
}

// "Net sales" -> "$6245.45": a report's lines by their first cell.
std::map<QString, QString> lines(const QVariantList &rows)
{
    std::map<QString, QString> out;
    for (const QVariant &v : rows) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (cells.size() >= 2)
            out.emplace(cells[0], cells[1]);
    }
    return out;
}

QStringList warnings;
void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        warnings << msg;
}

} // namespace

TEST_CASE("Legacy import: a store's menu and sales, worked out before anything changes", "[legacy][import]")
{
    QTemporaryDir dir;
    const legacy::Store store = legacy::readStore(datFolder(dir));
    INFO(store.errors.join(u'\n').toStdString());
    REQUIRE(store.errors.isEmpty());

    legacy::ImportContext cx;
    cx.zone = QTimeZone::UTC;
    const legacy::Imported in = legacy::convert(store, cx);
    const QVariantMap p = legacy::preview(store, in);
    CHECK(p.value(u"storeName"_s) == u"Casa Sample"_s);
    CHECK(p.value(u"items"_s).toInt() == 1457);
    CHECK(p.value(u"categories"_s).toInt() == 16);
    CHECK(p.value(u"choices"_s).toInt() > 10);
    CHECK(p.value(u"days"_s).toInt() == 3);
    CHECK(p.value(u"checks"_s).toInt() == 6);        // the practice checks left out
    CHECK(p.value(u"sales"_s).toInt() == 3 * (2858 + 1456));
    CHECK(p.value(u"from"_s).toString() == u"2025-01-10"_s);
    CHECK(p.value(u"to"_s).toString() == u"2025-01-12"_s);
    CHECK(in.mismatches.empty());                    // every check comes to what legacy took
    CHECK(in.errors.isEmpty());

    // The menu: items found by name, their families, prices and taxes.
    std::map<std::string, const core::MenuItem *> items;
    for (const core::MenuItem &m : in.menu)
        items[m.id] = &m;
    const auto plain = std::ranges::find(in.menu, "*PLAIN*", &core::MenuItem::name);
    REQUIRE(plain != in.menu.end());
    CHECK(plain->isModifier);
    // Legacy's modifier pages as choices: each a page's modifiers, asked by
    // the items whose buttons went to it.
    std::set<std::string> groups;
    for (const core::ModifierGroup &g : in.modifierGroups) {
        INFO(g.name);
        groups.insert(g.id);
        CHECK_FALSE(g.options.empty());
        CHECK(g.min == 0);
        for (const core::ModifierOption &o : g.options) {
            REQUIRE(items.contains(o.itemId));
            CHECK(items.at(o.itemId)->isModifier);
        }
    }
    int asking = 0;
    for (const core::MenuItem &m : in.menu)
        for (const std::string &g : m.modifierGroups) {
            CHECK(groups.contains(g));
            CHECK_FALSE(m.isModifier);
            ++asking;
        }
    CHECK(asking > 20);

    // The days: their checks, as legacy rang them.
    REQUIRE(in.days.size() == 3);
    const legacy::ImportedDay &d = in.days[0];
    CHECK(d.checks.size() == 2);
    CHECK(d.punches.empty());                        // (the time clock is staff's: not brought over)
    REQUIRE(d.drawers.size() == 1);
    CHECK(d.drawers[0].counted.cents() == 22858);
    const core::Check &cash = d.checks[0];
    CHECK(cash.label == "5");
    CHECK(cash.serverName == "Sam Server");          // who served it, by name
    CHECK(cash.type == core::CheckType::DineIn);
    REQUIRE(cash.lines.size() == 2);
    CHECK(cash.lines[0].quantity == 2);
    CHECK(cash.lines[0].unitPrice.cents() == 1195);
    CHECK(items.contains(cash.lines[0].itemId));     // the item it is on the menu brought over
    CHECK(cash.payments[0].tenderId == "cash");
    const core::Totals t = cash.totals(*cash.closedRates);
    CHECK(t.total.cents() == 2858);
    CHECK(t.change.cents() == 142);
    const core::Check &takeout = d.checks[1];
    CHECK(takeout.type == core::CheckType::Takeout);
    REQUIRE(takeout.lines.size() == 1);
    REQUIRE(takeout.lines[0].modifiers.size() == 1);   // "NO! RICE" on the burrito
    CHECK(takeout.lines[0].modifiers[0].name == "NO! RICE");
    CHECK(takeout.totals(*takeout.closedRates).total.cents() == 1456);
}

TEST_CASE("Legacy import: the menu on Modern's own screens, the sales in its reports", "[legacy][import]")
{
    QTemporaryDir dir;
    const legacy::Store store = legacy::readStore(datFolder(dir));
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    const core::PosSettings before = pos.shared()->settings;
    const std::size_t staff = pos.shared()->employees.size();
    legacy::ImportContext cx;
    cx.zone = QTimeZone::UTC;
    cx.current = &pos.shared()->settings;
    const legacy::Imported in = legacy::convert(store, cx);
    auto l = test::loadTestLayout();
    REQUIRE(l);
    const legacy::Applied applied = legacy::applyImport(pos, &*l, in, {});
    INFO(applied.error.toStdString());
    REQUIRE(applied.error.isEmpty());
    app::PosShared *s = pos.shared();

    // The menu, in place of the store's: items, families, choices.
    CHECK(s->menu.size() == 1457);
    CHECK(sink.menu.size() == 1457);
    CHECK(s->settings.menuCategories.size() == 16);
    CHECK(s->settings.modifierGroups.size() == in.modifierGroups.size());
    // Nothing else changed: the store's name, style, taxes, staff and pages are its own.
    CHECK(s->settings.storeName == before.storeName);
    CHECK(s->settings.style == before.style);
    CHECK(s->settings.tax == before.tax);
    CHECK(s->settings.tenders == before.tenders);
    CHECK(s->employees.size() == staff);
    CHECK(layout::Layout::styleOf(l->theme) == layout::Layout::styleOf(test::loadTestLayout()->theme));
    // Modern's own menu screens: they fill themselves from the menu.
    CHECK_FALSE(layout::hasHandBuiltMenu(*l));
    QStringList families;
    for (const core::MenuCategory &c : s->categories())
        families << QString::fromStdString(c.name);
    CHECK(families.contains(u"Seafood"_s));
    CHECK(families.contains(u"Beverage"_s));
    // An imported item rings up on a new check.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto pepsi = std::ranges::find_if(s->menu, [](const core::MenuItem &m) { return m.name == "Pepsi"; });
    REQUIRE(pepsi != s->menu.end());
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(QString::fromStdString(pepsi->id)));
    REQUIRE(pos.currentCheck());
    CHECK(pos.currentCheck()->lines.back().unitPrice == pepsi->price);

    // The past days: each with its reports, the checks in them.
    REQUIRE(s->pastDays.size() == 3);
    const app::PastDay &last = s->pastDays.front();
    CHECK(QDateTime::fromMSecsSinceEpoch(last.day.closedAt, QTimeZone::UTC).date() == QDate(2025, 1, 12));
    CHECK(last.reports.contains(u"sales"_s));
    const auto sales = lines(last.reports[u"sales"_s].toObject()[u"rows"_s].toArray().toVariantList());
    CHECK(sales.at(u"Checks"_s) == u"2"_s);
    CHECK(sales.at(u"Total with tax"_s) == u"$43.14"_s);
    int closedChecks = 0;
    for (const auto &[id, c] : sink.checks)
        if (c.status == core::CheckStatus::Closed) {
            ++closedChecks;
            CHECK(c.businessDay > 0);
        }
    CHECK(closedChecks == 6);   // (and the one just begun, open)
    CHECK(sink.punches.empty());
    CHECK(s->day.openedAt == last.day.closedAt);
}

TEST_CASE("Legacy import: what it won't do", "[legacy][import]")
{
    QTemporaryDir dir;
    const QString dat = datFolder(dir);
    legacy::ImportContext cx;
    cx.zone = QTimeZone::UTC;
    // Past days don't go in after sales of the store's own; the menu does.
    {
        test::RecordingSink sink;
        app::PosService pos(test::seedPosData(), &sink);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        pos.addItem(u"classic-burger"_s);
        pos.tender(u"cash"_s, 2000);
        const std::size_t menu = pos.shared()->menu.size();
        const legacy::Applied applied = legacy::applyImport(pos, nullptr, legacy::convert(legacy::readStore(dat), cx), {});
        CHECK(applied.error.contains(u"already has sales"_s));
        CHECK(pos.shared()->menu.size() == menu);   // nothing changed
        legacy::ImportParts parts;
        parts.history = false;
        cx.parts = parts;
        const legacy::Applied menuOnly = legacy::applyImport(pos, nullptr, legacy::convert(legacy::readStore(dat), cx), parts);
        CHECK(menuOnly.error.isEmpty());
        CHECK(pos.shared()->menu.size() == 1457);
        cx.parts = {};
    }
    // A day still open in legacy: close it there first.
    QFile open(QDir(dat).filePath(u"current/check_7"_s));
    REQUIRE(open.open(QIODevice::WriteOnly));
    open.write("vtpos 0 25\nH i6Rf fq C C A B P////////// I A A A A H A A A A A A ~ C A Cc A A");
    open.close();
    const legacy::Store store = legacy::readStore(dat);
    REQUIRE(store.current);
    CHECK(store.current->checks.size() == 1);
    const legacy::Imported in = legacy::convert(store, cx);
    REQUIRE(in.errors.size() == 1);
    CHECK(in.errors[0].contains(u"Close the day in legacy"_s));
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    const std::size_t menu = pos.shared()->menu.size();
    CHECK_FALSE(legacy::applyImport(pos, nullptr, in, {}).error.isEmpty());
    CHECK(pos.shared()->menu.size() == menu);
}

TEST_CASE("Legacy import on the screen: choose the folder, see what comes, bring it over", "[legacy][import][ui]")
{
    QTemporaryDir dir;
    const QString dat = datFolder(dir);
    auto l = test::loadTestLayout();
    REQUIRE(l);
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(*l);
    c.setPos(&pos);
    layout::Layout pages = *l;
    pos.shared()->requestLegacyImport = [&](const QString &folder, const QVariantMap &parts, bool apply) {
        legacy::Request r;
        r.pages = [&] { return pages; };
        r.savePages = [&](const layout::Layout &saved) {
            pages = saved;
            c.replaceLayout(saved);
            return QString();
        };
        r.backup = [&](QString *) { return dir.filePath(u"backup.db"_s); };
        return legacy::request(*pos.shared(), folder, parts, apply, r);
    };
    // Only the owner (or the maintainer) brings it over.
    REQUIRE(pos.loginWithPin(u"1111"_s));   // a server
    CHECK_FALSE(pos.legacyPreviewOf(dat));
    pos.logout();
    REQUIRE(test::loginMaintainer(pos));
    pos.shared()->settings.setupDone = true;

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    const QString shots = qEnvironmentVariable("VTM_SHOTS");
    const auto shot = [&](const QString &name) {
        QTest::qWait(shots.isEmpty() ? 30 : 250);
        if (!shots.isEmpty())
            window->grabWindow().save(shots + u'/' + name + u".png"_s);
    };
    // Store Settings has the way in.
    REQUIRE(c.showPage(u"admin-store"_s));
    CHECK(c.activeLayout().page(u"admin-store"_s)->zone(u"legacy-import"_s));
    REQUIRE(c.showPage(u"legacy-import"_s));
    shot(u"legacy-import-1-choose"_s);
    // A folder that isn't one: said, nothing else.
    CHECK_FALSE(pos.legacyPreviewOf(dir.filePath(u"dat/customers"_s)));
    CHECK(pos.legacyImport().value(u"error"_s).toString().contains(u"isn't legacy ViewTouch's dat folder"_s));
    pos.legacyClearState();
    // The store's folder: what it holds, nothing changed yet.
    REQUIRE(pos.legacyPreviewOf(dat));
    const QVariantMap preview = pos.legacyImport().value(u"preview"_s).toMap();
    CHECK(preview.value(u"items"_s).toInt() == 1457);
    CHECK_FALSE(preview.value(u"hasSales"_s).toBool());
    CHECK(pos.shared()->menu.size() != 1457);
    shot(u"legacy-import-2-preview"_s);
    // Bring it over: the menu and the sales, the screens as they were.
    REQUIRE(pos.legacyImportRun(dat, {}));
    CHECK(pos.legacyImport().value(u"done"_s).toStringList().size() == 2);
    CHECK(pos.legacyImport().value(u"backup"_s).toString().endsWith(u"backup.db"_s));
    CHECK(pos.shared()->menu.size() == 1457);
    CHECK(pos.shared()->pastDays.size() == 3);
    CHECK(layout::Layout::styleOf(pages.theme) == layout::Layout::styleOf(l->theme));
    shot(u"legacy-import-3-done"_s);
    qInstallMessageHandler(previous);
    CHECK(warnings == QStringList{});
}

namespace {

// From the command line into a store on disk, then that store opened again
// as ViewTouch opens it: its days and reports; a sale from the menu brought
// over; End of Day after its history.
void endToEnd(const QString &dat)
{
    const legacy::Store legacy = legacy::readStore(dat);
    REQUIRE(legacy.errors.isEmpty());
    QTemporaryDir dir;
    QProcess run;
    run.setProcessChannelMode(QProcess::MergedChannels);
    run.start(QStringLiteral(VTM_PROGRAM), {u"--data-dir"_s, dir.path(), u"--import-legacy"_s, dat});
    REQUIRE(run.waitForFinished(600'000));
    const QString said = QString::fromUtf8(run.readAll());
    INFO(said.toStdString());
    REQUIRE(run.exitCode() == 0);
    CHECK(said.contains(u"Brought over: "_s));

    const QString path = dir.filePath(u"viewtouch.db"_s);
    storage::PosStore store(path);
    REQUIRE(store.open());
    QStringList problems;
    std::optional<app::PosData> data = store.load(&problems);
    REQUIRE(data);
    CHECK(problems.isEmpty());
    CHECK(data->settings.style == "classic");   // the store's own look
    const std::size_t days = std::size_t(legacy.archives.size());
    CHECK(data->pastDays.size() == std::min<std::size_t>(days, 60));
    // Every closed check back, each paid in full for what legacy took.
    const std::vector<core::Check> all = storage::closedChecksBetween(path, 0, std::numeric_limits<std::int64_t>::max() / 2);
    legacy::ImportContext cx;
    const legacy::Imported in = legacy::convert(legacy, cx);
    std::size_t checks = 0;
    for (const legacy::ImportedDay &d : in.days)
        checks += d.checks.size();
    CHECK(all.size() == checks);
    Money owing;
    for (const core::Check &c : all) {
        const core::Totals t = c.totals(*c.closedRates);
        CHECK(core::booksProblem(t).empty());
        if (t.balance.cents() > 0)
            owing += t.balance;
    }
    CHECK(owing.cents() == 0);

    storage::AsyncWriter writer(path);
    storage::SqlPosSink sink(writer);
    app::PosService pos(std::move(*data), &sink);
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) { return storage::closedChecksBetween(path, from, to); };
    pos.shared()->refundHistory = [&](std::int64_t from, std::int64_t to) { return storage::refundsBetween(path, from, to); };
    pos.shared()->punchHistory = [&](std::int64_t from, std::int64_t to) { return storage::punchesBetween(path, from, to); };

    // The last day's frozen reports = the same day worked out again.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const app::PastDay last = pos.shared()->pastDays.front();
    const auto frozen = lines(last.reports[u"sales"_s].toObject()[u"rows"_s].toArray().toVariantList());
    const QString date = QDateTime::fromMSecsSinceEpoch(last.day.closedAt - 5 * 3'600'000).date().toString(u"yyyy-MM-dd"_s);
    REQUIRE(pos.requestRangeReport(u"sales"_s, u"custom"_s, date, date));
    for (int i = 0; i < 1000 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    const auto again = lines(pos.rangeReport()[u"report"_s].toMap()[u"rows"_s].toList());
    for (const QString &line : {u"Checks"_s, u"Item sales"_s, u"Discounts & comps"_s, u"Net sales"_s, u"Sales tax"_s,
                                u"Total with tax"_s}) {
        INFO(line.toStdString());
        REQUIRE(frozen.contains(line));
        REQUIRE(again.contains(line));
        CHECK(again.at(line) == frozen.at(line));
    }
    // The store's pages: Modern's own, filling themselves from the menu.
    storage::LayoutStore pages(path);
    REQUIRE(pages.open());
    std::optional<layout::Layout> l = pages.load();
    REQUIRE(l);
    CHECK_FALSE(layout::hasHandBuiltMenu(*l));
    // A sale from the menu brought over, paid, closed.
    const auto item = std::ranges::find_if(pos.shared()->menu, [](const core::MenuItem &m) {
        return !m.isModifier && m.available && m.price.cents() > 0 && m.modifierGroups.empty();
    });
    REQUIRE(item != pos.shared()->menu.end());
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(QString::fromStdString(item->id)));
    const std::int64_t due = pos.totals().value(u"balanceCents"_s).toLongLong();
    CHECK(due > 0);
    REQUIRE(pos.tender(u"cash"_s, (due + 499) / 500 * 500));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.shared()->closedToday.size() == 1);
    // The day ends, after all the days before it.
    for (const core::DrawerSession &d : std::vector<core::DrawerSession>(pos.shared()->drawers)) {
        if (!d.open())
            continue;
        const Money expected = core::expectedCash(d, pos.shared()->closedToday, pos.shared()->settings.tax);
        for (const QChar ch : QString::number(expected.cents()))
            pos.entryKey(QString(ch));
        REQUIRE(pos.countDrawerById(d.id));
    }
    QString said2;
    QObject::connect(&pos, &app::PosService::notice, [&](const QString &m) { said2 = m; });
    const bool closed = pos.endOfDay();
    INFO(said2.toStdString());
    REQUIRE(closed);
    CHECK(pos.shared()->pastDays.front().day.id > last.day.id);
    for (const auto &p : pos.shared()->problems)
        CHECK(p.kind != "books");
    writer.flush();
}

} // namespace

TEST_CASE("Legacy import end to end: from the command line, the store opened again and used", "[legacy][import][e2e]")
{
    QTemporaryDir dir;
    endToEnd(datFolder(dir));
}

// The whole of a real store (never in the repo), VTM_LEGACY_DAT=<its dat folder>.
TEST_CASE("Legacy import end to end: a whole real store", "[legacy][import][e2e][local]")
{
    const QString dat = qEnvironmentVariable("VTM_LEGACY_DAT");
    if (dat.isEmpty())
        SKIP("VTM_LEGACY_DAT not set");
    endToEnd(dat);
}

namespace {

// "$1,234.56" -> 123456
std::int64_t centsOf(QString text)
{
    text.remove(u'$').remove(u',');
    return std::llround(text.toDouble() * 100);
}

// Brought over (from the command line) and checked in full: every legacy
// item on the menu as legacy had it, every closed day kept with its reports,
// each report's checks and total what legacy took that day.
void everyDayAndItem(const QString &dat)
{
    const legacy::Store legacy = legacy::readStore(dat);
    REQUIRE(legacy.errors.isEmpty());
    REQUIRE(legacy.menu);
    const legacy::Imported in = legacy::convert(legacy, {});
    QTemporaryDir dir;
    QProcess run;
    run.setProcessChannelMode(QProcess::MergedChannels);
    run.start(QStringLiteral(VTM_PROGRAM), {u"--data-dir"_s, dir.path(), u"--import-legacy"_s, dat});
    REQUIRE(run.waitForFinished(600'000));
    INFO(QString::fromUtf8(run.readAll()).toStdString());
    REQUIRE(run.exitCode() == 0);
    const QString path = dir.filePath(u"viewtouch.db"_s);

    // --- the menu: every item legacy had ---
    storage::PosStore store(path);
    REQUIRE(store.open());
    std::optional<app::PosData> data = store.load();
    REQUIRE(data);
    std::map<QString, const core::MenuItem *> byName;
    for (const core::MenuItem &m : data->menu)
        byName.emplace(QString::fromStdString(m.name).toLower(), &m);
    std::map<std::string, std::string> family;
    for (const core::MenuCategory &c : data->settings.menuCategories)
        family[c.id] = c.name;
    QSet<QString> seen;
    int items = 0;
    for (const legacy::MenuItem &l : legacy.menu->items) {
        const QString key = l.name.trimmed().toLower();
        if (key.isEmpty() || seen.contains(key))
            continue;   // (legacy finds an item by its name: the first of a name)
        seen.insert(key);
        ++items;
        INFO(l.name.toStdString());
        REQUIRE(byName.contains(key));
        const core::MenuItem &m = *byName.at(key);
        CHECK(m.price.cents() == l.cost);
        CHECK(family[m.family] == legacy::familyName(l.family).toStdString());
        CHECK(m.isModifier == (l.type == 1 || l.type == 2));
        CHECK(m.available == !l.outOfStock);
        if (l.takeoutCost > 0 && l.takeoutCost != l.cost)
            CHECK(m.takeoutPrice.cents() == l.takeoutCost);
        const core::TaxClass tax = (l.salesType & 2) ? core::TaxClass::None
                                   : (l.salesType & 1) ? core::TaxClass::Alcohol
                                   : (l.salesType & 4) ? core::TaxClass::Room
                                   : (l.salesType & 8) ? core::TaxClass::Merchandise
                                                       : core::TaxClass::Food;
        CHECK(m.taxClass == tax);
    }
    CHECK(data->menu.size() == std::size_t(items));
    WARN(items << " items, every one as legacy had it");

    // --- every closed day: kept, with its reports ---
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"days"_s);
        db.setDatabaseName(path);
        REQUIRE(db.open());
        QSqlQuery q(db);
        REQUIRE(q.exec(u"SELECT id, closed_at, reports FROM business_days WHERE closed_at != 0 ORDER BY id"_s));
        std::size_t k = 0;
        Money total;
        while (q.next()) {
            REQUIRE(k < in.days.size());
            const legacy::ImportedDay &d = in.days[k++];
            INFO("legacy day " << d.legacyId);
            CHECK(q.value(1).toLongLong() == d.closedAt);
            const QJsonObject reports = QJsonDocument::fromJson(q.value(2).toByteArray()).object();
            for (const char *id : {"sales", "items", "categories", "hourly", "servers", "drawer", "deposit", "exceptions"})
                CHECK(reports.contains(QString::fromLatin1(id)));
            const auto sales = lines(reports[u"sales"_s].toObject()[u"rows"_s].toArray().toVariantList());
            REQUIRE(sales.contains(u"Checks"_s));
            CHECK(sales.at(u"Checks"_s).toInt() == int(d.checks.size()));
            REQUIRE(sales.contains(u"Total with tax"_s));
            CHECK(centsOf(sales.at(u"Total with tax"_s)) == d.sales.cents());   // what legacy took that day
            total += d.sales;
        }
        CHECK(k == in.days.size());
        WARN(k << " closed days, every one with its reports, " << total.toString() << " in all");
    }
    QSqlDatabase::removeDatabase(u"days"_s);
}

} // namespace

TEST_CASE("Legacy import: every item on the menu, every closed day with its reports", "[legacy][import]")
{
    QTemporaryDir dir;
    everyDayAndItem(datFolder(dir));
}

TEST_CASE("Legacy import: every item and every closed day of a whole real store", "[legacy][import][local]")
{
    const QString dat = qEnvironmentVariable("VTM_LEGACY_DAT");
    if (dat.isEmpty())
        SKIP("VTM_LEGACY_DAT not set");
    everyDayAndItem(dat);
}

TEST_CASE("Legacy import: a whole store, worked out", "[legacy][import][local]")
{
    const QString dat = qEnvironmentVariable("VTM_LEGACY_DAT");
    if (dat.isEmpty())
        SKIP("VTM_LEGACY_DAT not set");
    const legacy::Store store = legacy::readStore(dat);
    REQUIRE(store.errors.isEmpty());
    const legacy::Imported in = legacy::convert(store, {});
    std::size_t checks = 0;
    for (const legacy::ImportedDay &d : in.days)
        checks += d.checks.size();
    WARN(in.days.size() << " days, " << checks << " checks, " << in.mismatches.size() << " evened, "
                        << in.menu.size() << " items, " << in.modifierGroups.size() << " choice groups");
    CHECK(in.days.size() == std::size_t(store.archives.size()));
    CHECK(in.mismatches.size() * 200 < checks);   // under half a percent
}
