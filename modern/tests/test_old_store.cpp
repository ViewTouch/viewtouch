#include <catch2/catch_test_macros.hpp>
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
using namespace Qt::StringLiterals;
using namespace vt;

// A store saved by an older ViewTouch: a business day of its own pretend
// staff (vtmodern --stress, 0.13), closed with End of Day. Today's
// ViewTouch opens it with nothing lost, and its reports still add up to
// what that day's End of Day froze.
namespace {

QString fixture(const char *name)
{
    return QDir(QStringLiteral(VTM_FIXTURE_DIR)).filePath(u"../stores/"_s + QString::fromLatin1(name));
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

} // namespace

TEST_CASE("A store saved by ViewTouch 0.13: opened, nothing lost, its day's books the same", "[upgrade][oldstore]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"viewtouch.db"_s);
    REQUIRE(QFile::copy(fixture("store-0.13.db"), path));
    QFile(path).setPermissions(QFile::ReadOwner | QFile::WriteOwner);

    storage::PosStore store(path);
    QString error;
    REQUIRE(store.open(&error));
    QStringList problems;
    std::optional<app::PosData> data = store.load(&problems);
    REQUIRE(data);
    CHECK(problems.isEmpty());
    CHECK(data->menu.size() > 20);
    CHECK(data->employees.size() > 5);

    // The day End of Day closed: its frozen reports.
    REQUIRE(data->pastDays.size() == 1);
    const app::PastDay day = data->pastDays.front();
    const auto frozen = lines(day.reports[u"sales"_s].toObject()[u"rows"_s].toArray().toVariantList());
    REQUIRE(frozen.at(u"Checks"_s) == u"159"_s);
    REQUIRE(frozen.at(u"Total with tax"_s) == u"$6988.46"_s);

    // Every closed check came back.
    const std::vector<core::Check> closed = storage::closedChecksBetween(path, day.day.openedAt, day.day.closedAt + 1);
    CHECK(closed.size() == 159);

    // The same day as a range report, worked out again by today's code.
    storage::AsyncWriter writer(path);
    storage::SqlPosSink sink(writer);
    app::PosService pos(std::move(*data), &sink);
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) { return storage::closedChecksBetween(path, from, to); };
    pos.shared()->refundHistory = [&](std::int64_t from, std::int64_t to) { return storage::refundsBetween(path, from, to); };
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QString date = QDateTime::fromMSecsSinceEpoch(day.day.openedAt).date().toString(u"yyyy-MM-dd"_s);
    REQUIRE(pos.requestRangeReport(u"sales"_s, u"custom"_s, date, date));
    for (int i = 0; i < 500 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
        QTest::qWait(20);
    REQUIRE_FALSE(pos.rangeReport()[u"loading"_s].toBool());
    const auto now = lines(pos.rangeReport()[u"report"_s].toMap()[u"rows"_s].toList());
    for (const QString &line : {u"Checks"_s, u"Guests"_s, u"Item sales"_s, u"Discounts & comps"_s, u"Net sales"_s,
                                u"Taxable food sales"_s, u"Taxable alcohol sales"_s, u"Food tax"_s, u"Alcohol tax"_s,
                                u"Sales tax"_s, u"Gratuity"_s, u"Total with tax"_s, u"Credit Card"_s, u"Cash"_s,
                                u"Change given"_s, u"Collected"_s, u"Card tips (owed to staff)"_s}) {
        INFO(line.toStdString());
        REQUIRE(frozen.contains(line));
        REQUIRE(now.contains(line));
        CHECK(now.at(line) == frozen.at(line));
    }

    // Its pages: still there, and today's new starter pages join them.
    storage::LayoutStore pages(path);
    REQUIRE(pages.open());
    const auto layout = pages.load();
    REQUIRE(layout);
    CHECK(layout->page(u"order-template"_s));
    CHECK(layout->page(u"truck-orders"_s));
}
