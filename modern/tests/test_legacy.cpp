#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "layoutcontroller.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "zonemodel.hh"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;

// The Legacy style: the original ViewTouch's own pages (seed/legacy, made by
// `vtmodern --legacy-pages` from its vt_data, zone_db.dat and tables.dat).

namespace {

layout::Theme theme(const QString &id)
{
    QFile f(id == u"classic" ? u":/seed/theme.json"_s : u":/seed/themes/"_s + id + u".json"_s);
    REQUIRE(f.open(QIODevice::ReadOnly));
    return layout::Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object());
}

QStringList warnings;
void collect(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        warnings << msg;
}

bool onScreen(LayoutController &c, const QString &zoneId)
{
    ZoneModel *m = c.zones();
    for (int r = 0; r < m->rowCount(); ++r)
        if (m->data(m->index(r), ZoneModel::ZoneIdRole).toString() == zoneId)
            return true;
    return false;
}

} // namespace

TEST_CASE("Legacy: every page of the original ViewTouch, its login, Pay and order screen in Modern's places", "[legacy]")
{
    const QList<layout::Page> pages = layout::Layout::stylePages(u"legacy"_s);
    CHECK(pages.size() == 183);
    CHECK(layout::Layout::stylePages(u"modern"_s).isEmpty());
    const auto find = [&](const QString &id) -> const layout::Page * {
        const auto it = std::ranges::find(pages, id, &layout::Page::id);
        return it == pages.end() ? nullptr : &*it;
    };
    // Legacy's login (its 1920x1080 one), with the PIN pad where legacy had it.
    const layout::Page *login = find(u"legacy-n1"_s);
    REQUIRE(login);
    CHECK(login->variantOf == u"login"_s);
    CHECK(login->inStyle == u"legacy"_s);
    CHECK(std::ranges::any_of(login->zones, [](const layout::Zone &z) { return z.kind == u"loginPad"; }));
    CHECK(find(u"legacy-n20"_s)->variantOf == u"settle"_s);
    CHECK(find(u"legacy-n99"_s)->variantOf == u"order-template"_s);
    CHECK(find(u"legacy-50"_s)->variantOf.isEmpty());   // the store's own tables, not legacy's sample's
    CHECK(find(u"legacy-n999"_s));   // SuperUser Desktop: every page, not only those with a Modern one
    for (const layout::Page &p : pages)
        CHECK(p.inStyle == u"legacy"_s);

    // In a store's pages: nothing missing, no page or button pointing nowhere.
    auto l = test::loadTestLayout();
    REQUIRE(l);
    REQUIRE(l->useStyle(u"legacy"_s, theme(u"legacy"_s), pages));
    INFO(l->validate().join(u'\n').toStdString());
    CHECK(l->validate().isEmpty());
}

TEST_CASE("Legacy: switching styles keeps every page's buttons where they were, Legacy's pages too", "[legacy]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    const QRect classicPay = l->page(u"order-template"_s)->zone(u"flow-pay"_s)->rect;
    const QList<layout::Page> legacy = layout::Layout::stylePages(u"legacy"_s);

    REQUIRE(l->useStyle(u"modern"_s, theme(u"modern"_s)));
    const QRect modernPay = l->page(u"order-template"_s)->zone(u"flow-pay"_s)->rect;
    CHECK(modernPay != classicPay);
    // Legacy: its pages come; the store's stay in Modern's places underneath.
    REQUIRE(l->useStyle(u"legacy"_s, theme(u"legacy"_s), legacy));
    CHECK(layout::Layout::styleOf(l->theme) == u"legacy"_s);
    CHECK(layout::Layout::arrangedStyle(l->theme) == u"modern"_s);
    CHECK(l->page(u"order-template"_s)->zone(u"flow-pay"_s)->rect == modernPay);
    REQUIRE(l->page(u"legacy-n1"_s));
    l->page(u"legacy-n1"_s)->name = u"Our login"_s;   // an edit to a Legacy page
    // Classic: from Modern's places to Classic's; Legacy's pages kept (out of sight).
    REQUIRE(l->useStyle(u"classic"_s, theme(u"classic"_s)));
    CHECK(l->page(u"order-template"_s)->zone(u"flow-pay"_s)->rect == classicPay);
    CHECK(l->forStyle(u"classic"_s).page(u"legacy-n1"_s) == nullptr);
    CHECK(l->forStyle(u"classic"_s).page(u"login"_s)->kind == u"login"_s);
    // Legacy again: its pages as they were left, not added twice.
    REQUIRE(l->useStyle(u"legacy"_s, theme(u"legacy"_s), legacy));
    CHECK(l->page(u"legacy-n1"_s)->name == u"Our login"_s);
    CHECK(std::ranges::count(l->pages, u"legacy-n1"_s, &layout::Page::id) == 1);
    CHECK(layout::Layout::arrangedStyle(l->theme) == u"classic"_s);

    // As Legacy shows them: legacy's login in the login's place, under its id and role.
    const layout::Layout shown = l->forStyle(u"legacy"_s);
    const layout::Page *login = shown.pageByRole(u"login"_s);
    REQUIRE(login);
    CHECK(login->id == u"login"_s);
    CHECK(login->zones.size() == l->page(u"legacy-n1"_s)->zones.size());
    // The menu pages in legacy's order screen: its menu area right of the check.
    const auto f = l->styleFrame(u"menu-all"_s, u"legacy"_s);
    REQUIRE(f);
    CHECK(f->to == QRect(460, 8, 1452, 944));
}

TEST_CASE("Legacy on the screen: its pages, and a button Modern hasn't got says so", "[legacy][ui]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    REQUIRE(l->useStyle(u"legacy"_s, theme(u"legacy"_s), layout::Layout::stylePages(u"legacy"_s)));
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(*l);
    c.setPos(&pos);
    // Legacy's login: its zones (n1-1, n1-2...), not Modern's.
    CHECK(c.pageId() == u"login"_s);
    CHECK(onScreen(c, u"n1-1"_s));
    CHECK_FALSE(onScreen(c, u"login-pad"_s));
    // Legacy's own number keys type the PIN, its Enter logs in.
    const auto keyOf = [&](const QString &label) {
        for (const layout::Zone &z : l->page(u"legacy-n1"_s)->zones)
            if (z.label == label && !z.actions.isEmpty() && z.actions.first().type() == u"key")
                return z.id;
        return QString();
    };
    for (const QString &k : {u"1"_s, u"2"_s, u"3"_s, u"4"_s}) {
        REQUIRE_FALSE(keyOf(k).isEmpty());
        c.activate(keyOf(k));
    }
    if (const QString enter = keyOf(u"Enter"_s); !enter.isEmpty())
        c.activate(enter);
    else
        c.login();
    CHECK(pos.loggedIn());
    // A legacy page with a button for something Modern doesn't do.
    QString notIn;
    for (const layout::Page &p : l->pages)
        for (const layout::Zone &z : p.zones)
            if (notIn.isEmpty() && p.variantOf.isEmpty() && !z.actions.isEmpty() && z.actions.first().type() == u"notInModern")
                notIn = p.id + u'#' + z.id;
    REQUIRE_FALSE(notIn.isEmpty());
    REQUIRE(c.showPage(notIn.section(u'#', 0, 0)));
    c.activate(notIn.section(u'#', 1));
    CHECK(c.statusText().startsWith(u"Not in ViewTouch Modern yet: "_s));
}

TEST_CASE("Legacy: every page renders cleanly", "[legacy][ui][pages]")
{
    auto l = test::loadTestLayout();
    REQUIRE(l);
    REQUIRE(l->useStyle(u"legacy"_s, theme(u"legacy"_s), layout::Layout::stylePages(u"legacy"_s)));
    QStringList ids;
    for (const layout::Page &p : l->forStyle(u"legacy"_s).pages)
        if (p.variantOf.isEmpty())
            ids << p.id;
    test::RecordingSink sink;
    app::PosService pos(test::seedPosData(), &sink);
    LayoutController c(*l);
    c.setPos(&pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.addItem(u"classic-burger"_s);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{u"controller"_s, QVariant::fromValue(&c)}, {u"width"_s, 1600}, {u"height"_s, 900}});
    warnings.clear();
    const QtMessageHandler previous = qInstallMessageHandler(collect);
    engine.loadFromModule("ViewTouch", "Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    REQUIRE(window);
    REQUIRE(QTest::qWaitForWindowExposed(window));
    const QByteArray shots = qgetenv("VTM_SHOTS");
    for (const QString &id : ids) {
        INFO(id.toStdString());
        if (!c.showPage(id))
            continue;
        QTest::qWait(20);
        if (!shots.isEmpty()) {
            QTest::qWait(150);
            QDir().mkpath(QString::fromLocal8Bit(shots) + u"/pages/legacy"_s);
            window->grabWindow().save(QString::fromLocal8Bit(shots) + u"/pages/legacy/"_s + id + u".png"_s);
        }
    }
    qInstallMessageHandler(previous);
    CHECK(warnings == QStringList{});
}
