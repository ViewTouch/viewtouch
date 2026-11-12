#include <catch2/catch_test_macros.hpp>

#include "app/legacy/legacy_apply.hh"
#include "app/legacy/legacy_import.hh"
#include "app/pos_json.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTest>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>

#include <set>

using namespace Qt::StringLiterals;
using namespace vt;

// Bringing a store over from legacy ViewTouch: a made-up store's people and
// sales (tests/fixtures/legacy/sample, tools/legacy/make_sample.py) with a
// real store's menu, pages and dining room (tests/fixtures/legacy/elmirador).

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

const core::Employee *employee(app::PosService &pos, const std::string &id)
{
    for (const core::Employee &e : pos.shared()->employees)
        if (e.id == id)
            return &e;
    return nullptr;
}

} // namespace

TEST_CASE("Legacy import: what a store's files say, worked out before anything changes", "[legacy][import]")
{
    QTemporaryDir dir;
    const legacy::Store store = legacy::readStore(datFolder(dir));
    INFO(store.errors.join(u'\n').toStdString());
    REQUIRE(store.errors.isEmpty());
    REQUIRE(store.settings);
    CHECK(store.settings->storeName == u"Casa Sample"_s);
    CHECK(store.settings->taxFood == 0.0825);
    CHECK(store.settings->drawerDayFloat == 20000);
    CHECK(store.employees.size() == 3);
    CHECK(store.customers.size() == 2);
    CHECK(store.archives.size() == 3);
    REQUIRE(store.current);
    CHECK(store.current->drawers.size() == 1);

    legacy::ImportContext cx;
    cx.zone = QTimeZone::UTC;
    const legacy::Imported in = legacy::convert(store, cx);
    const QVariantMap p = legacy::preview(store, in);
    CHECK(p.value(u"storeName"_s) == u"Casa Sample"_s);
    CHECK(p.value(u"items"_s).toInt() == 1457);
    CHECK(p.value(u"pages"_s).toInt() == 112);
    CHECK(p.value(u"tables"_s).toInt() == 25);
    CHECK(p.value(u"days"_s).toInt() == 3);
    CHECK(p.value(u"checks"_s).toInt() == 6);        // the practice checks left out
    CHECK(p.value(u"sales"_s).toInt() == 3 * (2858 + 1456));
    CHECK(p.value(u"from"_s).toString() == u"2025-01-10"_s);
    CHECK(p.value(u"to"_s).toString() == u"2025-01-12"_s);
    CHECK(in.mismatches.empty());                    // every check comes to what legacy took
    CHECK(in.errors.isEmpty());

    // Settings: the store's name, its receipt lines, taxes, discounts.
    CHECK(in.receiptHeader == u"100 Main Street\nSpringfield, ST 00000\nPhone 555-0100"_s);
    CHECK(in.receiptFooter == u"Thank you!\nCome again"_s);
    REQUIRE(in.tax);
    CHECK(in.tax->foodPpm == 82500);
    CHECK(in.tax->merchandisePpm == 70000);
    const auto tender = [&](const std::string &name) -> const core::Tender * {
        const auto it = std::ranges::find(in.tenders, name, &core::Tender::name);
        return it == in.tenders.end() ? nullptr : &*it;
    };
    REQUIRE(tender("Ten Percent"));
    CHECK(tender("Ten Percent")->percentBp == 1000);
    REQUIRE(tender("Manager Comp"));
    CHECK(tender("Manager Comp")->percentBp == 10000);
    REQUIRE(tender("Staff Meal"));
    CHECK(tender("Staff Meal")->staffMeal);
    CHECK(tender("Staff Meal")->percentBp == 5000);
    REQUIRE(in.promotions.size() == 1);
    CHECK(in.promotions[0].name == "Happy Hour");
    CHECK(in.promotions[0].percentBp == 1500);
    CHECK(in.promotions[0].startMinute == 15 * 60);
    CHECK(in.promotions[0].endMinute == 18 * 60);
    CHECK(in.promotions[0].code.empty());            // automatic: no code
    REQUIRE(in.printers.size() == 2);
    CHECK(in.printers[0].id == "kitchen");
    CHECK(in.printers[0].host == "192.168.1.60");
    CHECK(in.printers[1].id == "receipt");

    // Staff: jobs as roles, pay, PINs; a short manager PIN asks for a new one.
    REQUIRE(in.employees.size() == 3);
    const core::Employee &pat = in.employees[0], &sam = in.employees[1], &lee = in.employees[2];
    CHECK(pat.name == "Pat Manager");
    CHECK(pat.role == "manager");
    CHECK(pat.payRate.cents() == 2000);
    CHECK(pat.pinMustChange);                        // "42": too short for a manager
    CHECK(pat.pinHash == app::hashPin(u"42"_s, pat.pinSalt));
    CHECK(sam.role == "server");
    CHECK_FALSE(sam.pinMustChange);
    CHECK(lee.role == "kitchen");                    // a line cook
    REQUIRE(lee.otherJobs.size() == 1);
    CHECK(lee.otherJobs[0].role == "cashier");
    CHECK(in.managersNewPin == QStringList{u"Pat Manager"_s});

    // Customers: never their card numbers.
    REQUIRE(in.customers.size() == 2);
    CHECK(in.customers[0].name == "Dana Regular");
    CHECK(in.customers[0].note == "Cross street: Oak Ave; no onions");
    CHECK(QString::fromStdString(app::toJson(in.customers[0]).value(u"note"_s).toString().toStdString())
              .contains(u"4111"_s) == false);
    for (const core::CustomerRecord &c : in.customers)
        CHECK(QJsonDocument(app::toJson(c)).toJson().contains("4111") == false);

    // The days: their checks, as legacy rang them.
    REQUIRE(in.days.size() == 3);
    const legacy::ImportedDay &d = in.days[0];
    CHECK(d.checks.size() == 2);
    CHECK(d.punches.size() == 2);
    REQUIRE(d.drawers.size() == 1);
    CHECK(d.drawers[0].counted.cents() == 22858);
    const core::Check &cash = d.checks[0];
    CHECK(cash.label == "5");
    CHECK(cash.serverName == "Sam Server");
    CHECK(cash.type == core::CheckType::DineIn);
    REQUIRE(cash.lines.size() == 2);
    CHECK(cash.lines[0].quantity == 2);
    CHECK(cash.lines[0].unitPrice.cents() == 1195);
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

TEST_CASE("Legacy import: brought into a store, it works there", "[legacy][import]")
{
    QTemporaryDir dir;
    const legacy::Store store = legacy::readStore(datFolder(dir));
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
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

    CHECK(s->settings.storeName == "Casa Sample");
    CHECK(s->settings.style == "legacy");
    CHECK(s->settings.setupDone);
    CHECK(s->menu.size() == 1457);
    CHECK(sink.menu.size() == 1457);   // the starter's items gone from the store, the store's in
    // The starter's sample staff stop; the store's own log in.
    for (const core::Employee &e : s->employees)
        if (e.sample)
            CHECK_FALSE(e.active);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.userName() == u"Sam Server"_s);
    CHECK_FALSE(pos.userPrefs().value(u"newPin"_s).toBool());
    CHECK_FALSE(pos.chooseNewPin(u"9999"_s));   // not theirs to change: a manager's
    pos.logout();
    // A manager with a short legacy PIN: once with it, then a new one.
    REQUIRE(pos.loginWithPin(u"42"_s));
    CHECK(pos.userPrefs().value(u"newPin"_s).toBool());
    CHECK_FALSE(pos.chooseNewPin(u"5678"_s));       // a manager's is 6 to 8 digits
    CHECK_FALSE(pos.chooseNewPin(u"12345"_s));
    REQUIRE(pos.chooseNewPin(u"424242"_s));
    CHECK_FALSE(pos.userPrefs().value(u"newPin"_s).toBool());
    pos.logout();
    CHECK_FALSE(pos.loginWithPin(u"42"_s));
    REQUIRE(pos.loginWithPin(u"424242"_s));
    CHECK(employee(pos, "legacy-1")->role == "manager");

    // The past days: each with its reports, the checks in them.
    REQUIRE(s->pastDays.size() == 3);
    const app::PastDay &last = s->pastDays.front();
    CHECK(QDateTime::fromMSecsSinceEpoch(last.day.closedAt, QTimeZone::UTC).date() == QDate(2025, 1, 12));
    CHECK(last.reports.contains(u"sales"_s));
    CHECK(sink.checks.size() == 6);
    for (const auto &[id, c] : sink.checks)
        CHECK(c.businessDay > 0);
    CHECK(sink.punches.size() == 6);
    // A new day, open since the last one closed: nothing open, nothing sold.
    CHECK(s->closedToday.empty());
    CHECK(s->day.openedAt == last.day.closedAt);

    // Its pages: the store's own menu pages in the Legacy style, its tables.
    const layout::Layout shown = l->forStyle(u"legacy"_s);
    const auto index = std::ranges::find_if(shown.pages, [](const layout::Page &p) { return p.kind == u"index"; });
    REQUIRE(index != shown.pages.end());
    CHECK(index->id == u"legacy-60"_s);
    CHECK_FALSE(shown.page(u"menu-all"_s));   // Modern's menu pages step aside
    CHECK(l->page(u"menu-all"_s));            // (kept, for Classic and Modern)
    const layout::Page *tables = l->page(u"tables"_s);
    REQUIRE(tables);
    CHECK(std::ranges::count(tables->zones, u"table"_s, &layout::Zone::kind) == 25);
    CHECK(std::ranges::none_of(tables->zones, [](const layout::Zone &z) { return z.label == u"T1"; }));
    // Every item button on the store's pages adds one of its items.
    std::set<std::string> ids;
    for (const core::MenuItem &m : s->menu)
        ids.insert(m.id);
    int buttons = 0;
    for (const layout::Page &p : l->pages)
        for (const layout::Zone &z : p.zones)
            for (const layout::Action &a : z.actions)
                if (a.type() == u"addItem" && p.extra.value(u"storeLegacy"_s).toBool()) {
                    ++buttons;
                    INFO(p.id.toStdString() << " " << z.id.toStdString() << " " << a.str(u"item"_s).toStdString());
                    CHECK(ids.contains(a.str(u"item"_s).toStdString()));
                }
    CHECK(buttons > 1000);
    INFO(l->validate().join(u'\n').toStdString());
    CHECK(l->validate().isEmpty());
}

TEST_CASE("Legacy import: what it won't do", "[legacy][import]")
{
    QTemporaryDir dir;
    const QString dat = datFolder(dir);
    legacy::ImportContext cx;
    cx.zone = QTimeZone::UTC;
    // Past days don't go in after sales of the store's own.
    {
        test::RecordingSink sink;
        app::PosService pos(test::seedPosData(), &sink);
        REQUIRE(pos.loginWithPin(u"1234"_s));
        pos.addItem(u"classic-burger"_s);
        pos.tender(u"cash"_s, 2000);
        const legacy::Imported in = legacy::convert(legacy::readStore(dat), cx);
        const legacy::Applied applied = legacy::applyImport(pos, nullptr, in, {});
        CHECK(applied.error.contains(u"already has sales"_s));
        CHECK(pos.shared()->settings.storeName != "Casa Sample");   // nothing changed
        // Without the history, the rest comes.
        legacy::ImportParts parts;
        parts.history = false;
        cx.parts = parts;
        const legacy::Applied rest = legacy::applyImport(pos, nullptr, legacy::convert(legacy::readStore(dat), cx), parts);
        CHECK(rest.error.isEmpty());
        CHECK(pos.shared()->settings.storeName == "Casa Sample");
        cx.parts = {};
    }
    // A day still open in legacy: close it there first.
    QFile open(QDir(dat).filePath(u"current/check_7"_s));   // (dat: the copy's folder)
    REQUIRE(open.open(QIODevice::WriteOnly));
    open.write("vtpos 0 25\nH i6Rf fq C C A B P////////// I A A A A H A A A A A A ~ C A Cc A A");
    open.close();
    const legacy::Store store = legacy::readStore(dat);
    INFO(store.errors.join(u'\n').toStdString());
    REQUIRE(store.current);
    CHECK(store.current->checks.size() == 1);
    const legacy::Imported in = legacy::convert(store, cx);
    REQUIRE(in.errors.size() == 1);
    CHECK(in.errors[0].contains(u"Close the day in legacy"_s));
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    CHECK_FALSE(legacy::applyImport(pos, nullptr, in, {}).error.isEmpty());
    CHECK(pos.shared()->settings.storeName != "Casa Sample");
}

namespace {
QStringList warnings;
void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        warnings << msg;
}
} // namespace

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
    // Only the owner (or the maintainer) brings a store over.
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
    CHECK(preview.value(u"storeName"_s) == u"Casa Sample"_s);
    CHECK(preview.value(u"items"_s).toInt() == 1457);
    CHECK_FALSE(preview.value(u"hasSales"_s).toBool());
    CHECK(pos.shared()->settings.storeName != "Casa Sample");
    shot(u"legacy-import-2-preview"_s);
    // Bring it over: everything, the screens in the Legacy style.
    REQUIRE(pos.legacyImportRun(dat, {}));
    CHECK(pos.legacyImport().value(u"done"_s).toStringList().size() == 7);
    CHECK(pos.legacyImport().value(u"backup"_s).toString().endsWith(u"backup.db"_s));
    CHECK(pos.shared()->settings.storeName == "Casa Sample");
    CHECK(pos.shared()->pastDays.size() == 3);
    CHECK(layout::Layout::styleOf(pages.theme) == u"legacy"_s);
    shot(u"legacy-import-3-done"_s);
    qInstallMessageHandler(previous);
    CHECK(warnings == QStringList{});
}

// The whole of a real store (never in the repo), VTM_LEGACY_DAT=<its dat folder>.
TEST_CASE("Legacy import: a whole store, worked out", "[legacy][import][local]")
{
    const QString dat = qEnvironmentVariable("VTM_LEGACY_DAT");
    if (dat.isEmpty())
        SKIP("VTM_LEGACY_DAT not set");
    const legacy::Store store = legacy::readStore(dat);
    REQUIRE(store.errors.isEmpty());
    legacy::ImportContext cx;
    const legacy::Imported in = legacy::convert(store, cx);
    std::map<std::int64_t, int> byDiff;
    for (const legacy::Mismatch &m : in.mismatches)
        ++byDiff[(m.modern - m.legacy).cents()];
    for (const auto &[diff, n] : byDiff)
        WARN("off by " << diff << " cents: " << n);
    std::size_t checks = 0;
    for (const legacy::ImportedDay &d : in.days)
        checks += d.checks.size();
    WARN(in.days.size() << " days, " << checks << " checks, " << in.mismatches.size() << " not adding up");
    CHECK(in.days.size() == std::size_t(store.archives.size()));
    CHECK(in.mismatches.size() * 200 < checks);   // under half a percent
    // Into a store, every day.
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    const legacy::Applied applied = legacy::applyImport(pos, nullptr, in, {});
    CHECK(applied.error.isEmpty());
    CHECK(sink.checks.size() == checks);
}
