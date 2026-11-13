#include <catch2/catch_test_macros.hpp>

#include "app/legacy/legacy_apply.hh"
#include "app/legacy/legacy_import.hh"
#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;

// A legacy store's menu and sales on the screen: every page with the menu
// brought over, and the Reports page on the days brought over. The made-up
// store, or a whole real one (VTM_LEGACY_DAT); pictures with VTM_SHOTS.

namespace {

QStringList warnings;
void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        warnings << msg;
}

QString sampleFolder(QTemporaryDir &dir)
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

QQuickItem *findItem(QQuickItem *in, const char *type)
{
    if (!in)
        return nullptr;
    if (QString::fromLatin1(in->metaObject()->className()).startsWith(QString::fromLatin1(type)))
        return in;
    for (QQuickItem *child : in->childItems())
        if (QQuickItem *found = findItem(child, type))
            return found;
    return nullptr;
}

// The rows a report shows: "Checks" -> "283"...
QMap<QString, QString> rowsOf(const QVariant &report)
{
    QMap<QString, QString> out;
    for (const QVariant &r : report.toMap().value(u"rows"_s).toList()) {
        const QStringList cells = r.toMap().value(u"cells"_s).toStringList();
        if (cells.size() >= 2)
            out.insert(cells[0], cells[1]);
    }
    return out;
}

} // namespace

TEST_CASE("Legacy import on the screens: every page with its menu, Reports on its days", "[legacy][import][ui][screens]")
{
    QTemporaryDir dir;
    const QString dat = qEnvironmentVariableIsSet("VTM_LEGACY_DAT") ? qEnvironmentVariable("VTM_LEGACY_DAT")
                                                                    : sampleFolder(dir);
    const legacy::Store store = legacy::readStore(dat);
    REQUIRE(store.errors.isEmpty());
    // The pages a store starts with (what ships), not the tests' own.
    auto l = layout::Layout::loadDirectory(QStringLiteral(VTM_SEED_DIR));
    REQUIRE(l);
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    legacy::ImportContext cx;
    cx.current = &pos.shared()->settings;
    const legacy::Imported in = legacy::convert(store, cx);
    REQUIRE(legacy::applyImport(pos, &*l, in, {}).error.isEmpty());
    // Reports over several days read the closed checks (here, as saved).
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) {
        std::vector<core::Check> out;
        for (const auto &[id, c] : sink.checks)
            if (c.status == core::CheckStatus::Closed && c.closedAt >= from && c.closedAt < to)
                out.push_back(c);
        return out;
    };
    pos.shared()->settings.setupDone = true;
    LayoutController c(*l);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // A check with an item from the menu brought over, for the order screens.
    const auto item = std::ranges::find_if(pos.shared()->menu, [](const core::MenuItem &m) {
        return !m.isModifier && m.available && m.price.cents() > 0;
    });
    REQUIRE(item != pos.shared()->menu.end());
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(QString::fromStdString(item->id)));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    const QString shots = qEnvironmentVariable("VTM_SHOTS");
    if (!shots.isEmpty())
        QDir().mkpath(shots + u"/imported"_s);
    const auto shot = [&](const QString &name) {
        QTest::qWait(shots.isEmpty() ? 20 : 200);
        if (!shots.isEmpty())
            window->grabWindow().save(shots + u"/imported/"_s + name + u".png"_s);
    };

    // Every page.
    int shown = 0;
    for (const layout::Page &p : c.activeLayout().pages) {
        if (!p.variantOf.isEmpty() || p.id == u"factory-reset" || p.kind == u"kiosk")
            continue;
        INFO(p.id.toStdString());
        if (!c.showPage(p.id))
            continue;
        ++shown;
        shot(p.id);
    }
    CHECK(shown > 50);
    // The menu's sections: its families by name, the modifiers not one of them.
    QStringList sections;
    for (const core::MenuCategory &cat : pos.shared()->categories())
        sections << QString::fromStdString(cat.name);
    CHECK_FALSE(sections.contains(u"Modifier"_s));
    for (const QString &name : sections)
        CHECK_FALSE(name.startsWith(u"Family-"_s));

    // The Reports page on the days brought over.
    REQUIRE(c.showPage(u"reports"_s));
    QTest::qWait(50);
    QQuickItem *reports = findItem(window->contentItem(), "WidgetReportView");
    REQUIRE(reports);
    const QVariantList days = reports->property("days").toList();
    REQUIRE(days.size() == qsizetype(std::min<std::size_t>(in.days.size(), 60) + 1));   // today, then the days
    // The last day brought over: its own report, as it closed.
    const legacy::ImportedDay &last = in.days.back();
    for (const char *id : {"sales", "items", "categories", "hourly", "servers", "drawer"}) {
        reports->setProperty("reportId", QString::fromLatin1(id));
        reports->setProperty("dayIndex", 1);
        QTest::qWait(20);
        const QVariant report = reports->property("report");
        INFO(id);
        CHECK_FALSE(report.toMap().value(u"rows"_s).toList().isEmpty());
        if (QByteArrayView(id) == "sales")
            CHECK(rowsOf(report).value(u"Checks"_s).toInt() == int(last.checks.size()));
        // Families by their names, as legacy called them (not their ids).
        for (const QVariant &r : report.toMap().value(u"rows"_s).toList())
            for (const QString &cell : r.toMap().value(u"cells"_s).toStringList())
                CHECK_FALSE(cell.startsWith(u"Family-"_s));
        shot(u"reports-last-day-"_s + QString::fromLatin1(id));
    }
    // A day further back.
    if (days.size() > 40) {
        reports->setProperty("reportId", u"sales"_s);
        reports->setProperty("dayIndex", 40);
        QTest::qWait(20);
        const legacy::ImportedDay &d = in.days[in.days.size() - 40];
        CHECK(rowsOf(reports->property("report")).value(u"Checks"_s).toInt() == int(d.checks.size()));
        shot(u"reports-day-40-back"_s);
    }
    // A month brought over, read from its checks.
    const QDate end = QDateTime::fromMSecsSinceEpoch(last.closedAt).date();
    reports->setProperty("reportId", u"sales"_s);
    reports->setProperty("fromDate", end.addDays(-30).toString(Qt::ISODate));
    reports->setProperty("toDate", end.toString(Qt::ISODate));
    reports->setProperty("period", u"custom"_s);
    for (int i = 0; i < 500 && pos.rangeReport().value(u"loading"_s).toBool(); ++i)
        QTest::qWait(20);
    QTest::qWait(50);
    const auto month = rowsOf(reports->property("report"));
    CHECK(month.value(u"Checks"_s).toInt() > 0);
    shot(u"reports-a-month"_s);

    qInstallMessageHandler(previous);
    CHECK(warnings == QStringList{});
}
