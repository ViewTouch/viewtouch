#include <catch2/catch_test_macros.hpp>

#include "core/report.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "reportexport.hh"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Sales by hour and category, the audit trail, and saving reports.

namespace {

std::int64_t todayAt(int hour, int minute = 0)
{
    return QDateTime(QDate::currentDate(), QTime(hour, minute)).toMSecsSinceEpoch();
}

void sell(PosService &pos, const char *item, const QString &table)
{
    REQUIRE(pos.selectTable(table) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(QString::fromLatin1(item));
    pos.finishChoosing();
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());
}

QStringList cellsOf(const QVariantMap &report, const QString &first)
{
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (!cells.isEmpty() && cells.first() == first)
            return cells;
    }
    return {};
}

} // namespace

TEST_CASE("Sales by hour and by category", "[reports]")
{
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = todayAt(11, 30);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1111"_s));
    sell(pos, "coffee", u"T1"_s);                          // $2.75 at 11:30
    clock = todayAt(12, 15);
    sell(pos, "cobb", u"T2"_s);                            // $12.50 at 12:15
    sell(pos, "tea", u"T3"_s);                             // $2.50 at 12:15
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));

    const QVariantMap hourly = pos.report(u"hourly"_s);
    CHECK(cellsOf(hourly, u"11 AM - 12 PM"_s) == QStringList{u"11 AM - 12 PM"_s, u"1"_s, u"1"_s, u"$2.75"_s});
    CHECK(cellsOf(hourly, u"12 PM - 1 PM"_s) == QStringList{u"12 PM - 1 PM"_s, u"2"_s, u"2"_s, u"$15.00"_s});
    CHECK(cellsOf(hourly, u"Total"_s).last() == u"$17.75"_s);

    const QVariantMap cats = pos.report(u"categories"_s);
    CHECK(cellsOf(cats, u"Salads"_s) == QStringList{u"Salads"_s, u"1"_s, u"$12.50"_s, u"70.4%"_s});
    CHECK(cellsOf(cats, u"Drinks"_s) == QStringList{u"Drinks"_s, u"2"_s, u"$5.25"_s, u"29.6%"_s});
    // Biggest first.
    CHECK(cats[u"rows"_s].toList().first().toMap()[u"cells"_s].toStringList().first() == u"Salads"_s);
}

TEST_CASE("Audit trail: voids, discounts, reopened, transferred", "[reports][audit]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    CHECK(cellsOf(pos.report(u"audit"_s), u"No voids, discounts, reopened, moved, transferred or merged checks."_s)
              .size() == 1);

    REQUIRE(pos.selectTable(u"T4"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"coffee"_s);
    pos.addItem(u"tea"_s);
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.voidItem());                               // the tea
    REQUIRE(pos.tender(u"discount"_s));
    REQUIRE(pos.tender(u"cash"_s));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.reopenCheck(id));

    const QVariantMap audit = pos.report(u"audit"_s);
    // How many, and what they were worth.
    CHECK(cellsOf(audit, u"Voids"_s).value(3) == u"1"_s);
    CHECK(cellsOf(audit, u"Voids"_s).value(4) == u"$2.50"_s);
    CHECK(cellsOf(audit, u"Discounts"_s).value(3) == u"1"_s);
    CHECK(cellsOf(audit, u"Reopened checks"_s).value(3) == u"1"_s);
    const QStringList voided = cellsOf(audit, u"Voided Tea ($2.50)"_s);
    REQUIRE(voided.size() == 5);
    CHECK(voided[4] == u"$2.50"_s);
    CHECK(voided[1] == u"T4 #%1"_s.arg(id));
    CHECK(voided[2].startsWith(u"Morgan"_s));
}

TEST_CASE("Reports are kept with the day at End of Day", "[reports]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    sell(pos, "coffee", u"T1"_s);
    REQUIRE(pos.endOfDay());
    const qint64 past = pos.days().at(1).toMap()[u"id"_s].toLongLong();
    for (const char16_t *id : {u"hourly", u"categories", u"audit"})
        CHECK_FALSE(pos.report(QString::fromUtf16(id), past)[u"rows"_s].toList().isEmpty());
}

TEST_CASE("Saving a report as CSV and PDF", "[reports][export]")
{
    QTemporaryDir dir;
    const QVariantMap report{
        {u"title"_s, u"Sales by Category"_s}, {u"subtitle"_s, u"Today"_s},
        {u"columns"_s, QStringList{u"Category"_s, u"Sales"_s}},
        {u"rows"_s, QVariantList{
            QVariantMap{{u"kind"_s, u"line"_s}, {u"cells"_s, QStringList{u"Fish, \"fresh\""_s, u"$1,200.00"_s}}},
            // A guest's name a spreadsheet would run, and a refund (a number).
            QVariantMap{{u"kind"_s, u"line"_s}, {u"cells"_s, QStringList{u"=HYPERLINK(\"http://x\")"_s, u"-5.00"_s}}},
            QVariantMap{{u"kind"_s, u"total"_s}, {u"cells"_s, QStringList{u"All"_s, u"$1,200.00"_s}}}}},
    };
    QString error;
    const QString csv = exportReportCsv(report, dir.filePath(u"out"_s), &error);
    REQUIRE_FALSE(csv.isEmpty());
    CHECK(csv.endsWith(u".csv"_s));
    CHECK(QFileInfo(csv).fileName().startsWith(u"sales-by-category-"_s));
    QFile f(csv);
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    CHECK(text.contains(u"Category,Sales"_s));
    CHECK(text.contains(u"\"Fish, \"\"fresh\"\"\",\"$1,200.00\""_s));
    CHECK(text.contains(u"\"'=HYPERLINK(\"\"http://x\"\")\",-5.00"_s));   // text, not a formula

    const QString pdf = exportReportPdf(report, dir.filePath(u"out"_s), &error);
    REQUIRE_FALSE(pdf.isEmpty());
    QFile p(pdf);
    REQUIRE(p.open(QIODevice::ReadOnly));
    CHECK(p.read(5) == "%PDF-");
    CHECK(p.size() > 500);
}

#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Until the range report has been read (it comes from a worker thread).
bool waitForRange(const app::PosService &pos)
{
    for (int i = 0; i < 500 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    return !pos.rangeReport()[u"loading"_s].toBool();
}

// A closed check with `count` Cobb salads ($12.50 each), closed at `when`.
core::Check pastSale(std::int64_t id, std::int64_t when, int count, const app::PosService &pos)
{
    core::Check c;
    c.id = id;
    c.type = core::CheckType::Takeout;
    c.status = core::CheckStatus::Closed;
    c.label = "Takeout";
    c.serverId = "sam";
    c.serverName = "Sam";
    c.openedAt = when - 600'000;
    c.closedAt = when;
    for (const core::MenuItem &m : pos.shared()->menu) {
        if (m.id == "cobb") {
            core::OrderLine &l = c.addItem(m);
            l.quantity = count;
        }
    }
    c.addPayment({"credit", "Credit Card", core::TenderKind::Card, 0}, c.totals(pos.shared()->settings.tax).total);
    return c;
}

QStringList rangeCells(const QVariantMap &range, const QString &first)
{
    for (const QVariant &v : range[u"report"_s].toMap()[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (!cells.isEmpty() && cells.first() == first)
            return cells;
    }
    return {};
}

} // namespace

TEST_CASE("Reports over a range of days, against the same days last year", "[reports][range]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    storage::AsyncWriter writer(path);
    storage::SqlPosSink sink(writer);
    PosService pos(*store.load(), &sink);
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) { return storage::closedChecksBetween(path, from, to); };

    // This year: 4 salads on the 3rd, 6 on the 10th of last month; last
    // year, 5 over the same month; one outside it.
    const QDate first = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1).addMonths(-1);
    const auto at = [](QDate d) { return QDateTime(d, QTime(12, 0)).toMSecsSinceEpoch(); };
    sink.saveCheck(pastSale(9001, at(first.addDays(2)), 4, pos));
    sink.saveCheck(pastSale(9002, at(first.addDays(9)), 6, pos));
    sink.saveCheck(pastSale(9003, at(first.addDays(9).addYears(-1)), 5, pos));
    sink.saveCheck(pastSale(9004, at(first.addMonths(-2)), 9, pos));
    writer.flush();

    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.requestRangeReport(u"sales"_s, u"lastMonth"_s));       // managers only
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.requestRangeReport(u"sales"_s, u"custom"_s, u"first"_s, u"last"_s));

    REQUIRE(pos.requestRangeReport(u"items"_s, u"lastMonth"_s, {}, {}, true));
    CHECK(pos.rangeReport()[u"loading"_s].toBool());
    REQUIRE(waitForRange(pos));
    const QVariantMap range = pos.rangeReport();
    CHECK(range[u"checks"_s] == 2);
    const QStringList cobb = rangeCells(range, u"Cobb"_s);
    REQUIRE(cobb.size() >= 4);
    CHECK(cobb[1] == u"10"_s);                                               // sold this period
    CHECK(cobb[cobb.size() - 2] == u"$62.50"_s);                             // last year: 5 x $12.50
    CHECK(cobb.last() == u"+100.0%"_s);                                      // $125.00 vs $62.50
    CHECK(range[u"report"_s].toMap()[u"columns"_s].toStringList().contains(u"Change"_s));

    // The same dates typed in; a report that's one day at a time says so.
    const QString a = first.toString(u"yyyy-MM-dd"_s), b = first.addMonths(1).addDays(-1).toString(u"yyyy-MM-dd"_s);
    REQUIRE(pos.requestRangeReport(u"labor"_s, u"custom"_s, b, a));          // backwards is fine
    REQUIRE(waitForRange(pos));
    CHECK(pos.rangeReport()[u"report"_s].toMap()[u"id"_s] == u"sales"_s);
    CHECK_FALSE(rangeCells(pos.rangeReport(), u"That report is one day at a time (pick Day). Showing sales."_s).isEmpty());

    // Today's checks count even before they are written.
    REQUIRE(pos.requestRangeReport(u"sales"_s, u"week"_s));
    REQUIRE(waitForRange(pos));
    const int before = pos.rangeReport()[u"checks"_s].toInt();
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    REQUIRE(pos.requestRangeReport(u"sales"_s, u"week"_s));
    REQUIRE(waitForRange(pos));
    CHECK(pos.rangeReport()[u"checks"_s].toInt() == before + 1);
}

TEST_CASE("Comparing reports: matched rows, changes, new rows", "[reports][range]")
{
    core::Report now, before;
    now.columns = {"Item", "Sold", "Sales"};
    now.line({"Cobb", "10", "$125.00"});
    now.line({"Tea", "3", "$7.50"});
    now.total({"Total", "", "$132.50"});
    before.line({"Cobb", "5", "$62.50"});
    before.line({"Tea", "0", "$0.00"});
    before.total({"Total", "", "$150.00"});
    const core::Report r = core::compareReports(now, before, "2025");
    CHECK(r.columns.back() == "Change");
    CHECK(r.rows[0].cells == std::vector<std::string>{"Cobb", "10", "$125.00", "$62.50", "+100.0%"});
    CHECK(r.rows[1].cells.back() == "new");
    CHECK(r.rows[2].cells.back() == "-11.7%");
}

TEST_CASE("Exceptions: someone far above everyone else on shift is flagged", "[reports][unusual]")
{
    core::PosSettings settings;
    const core::ReportContext ctx{settings, "Today", [](std::int64_t) { return std::string("9:00 PM"); }};
    const auto checkBy = [](const char *server, int voids, const char *voider) {
        core::Check c;
        c.serverName = server;
        for (int i = 0; i < voids; ++i)
            c.note(0, voider, "Void Coffee", "void", Money::fromCents(300));
        return c;
    };
    const auto rowsOf = [](const core::Report &r) {
        std::vector<std::string> out;
        for (const core::ReportRow &row : r.rows)
            out.push_back(row.cells.empty() ? std::string() : row.cells.front());
        return out;
    };
    // Jo voids 6, Sam 1, Casey none.
    const core::Check jo = checkBy("Jo", 6, "Jo"), sam = checkBy("Sam", 1, "Sam"), casey = checkBy("Casey", 0, "");
    const core::Report flagged = core::exceptionsReport({&jo, &sam, &casey}, {}, ctx);
    const std::vector<std::string> rows = rowsOf(flagged);
    REQUIRE(std::ranges::find(rows, "Unusual: voids") != rows.end());
    bool joLine = false;
    for (const core::ReportRow &row : flagged.rows)
        if (row.cells.size() == 3 && row.cells[0] == "Jo" && row.cells[2] == "≤ 1")
            joLine = true;
    CHECK(joLine);

    // An even spread (4, 3, 3): nobody stands out.
    const core::Check a = checkBy("Jo", 4, "Jo"), b = checkBy("Sam", 3, "Sam"), c = checkBy("Casey", 3, "Casey");
    const std::vector<std::string> even = rowsOf(core::exceptionsReport({&a, &b, &c}, {}, ctx));
    CHECK(std::ranges::find(even, "Unusual: voids") == even.end());

    // Alone on shift: nobody to compare with.
    const std::vector<std::string> alone = rowsOf(core::exceptionsReport({&jo}, {}, ctx));
    CHECK(std::ranges::find(alone, "Unusual: voids") == alone.end());
}
