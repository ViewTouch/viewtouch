#include <catch2/catch_test_macros.hpp>

#include "editorcontroller.hh"
#include "layout_fixture.hh"
#include "app/i18n.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QCoreApplication>
#include <QDate>
#include <QSignalSpy>

using namespace Qt::StringLiterals;
using namespace vt;

// Languages: each person's screens in their language; guests get the store's.

namespace {

// Back to English after each test: the screen's language is app-wide.
struct Spanish {
    Spanish() { i18n::install(); }
    ~Spanish()
    {
        i18n::setLanguage(u"en"_s);
        i18n::setGuestLanguage(u"en"_s);
    }
};

QString tr(const char *text, int n = -1)
{
    return QCoreApplication::translate("Test", text, nullptr, n);
}

} // namespace

TEST_CASE("Languages: phrases, plurals, and English when there is none", "[i18n]")
{
    Spanish es;
    CHECK(tr("Send") == u"Send"_s);   // English until asked
    CHECK(tr("%n guest(s)", 1) == u"1 guest"_s);   // English plurals
    CHECK(tr("%n guest(s)", 3) == u"3 guests"_s);
    REQUIRE(i18n::setLanguage(u"es"_s));
    CHECK_FALSE(i18n::setLanguage(u"es"_s));   // no change
    CHECK(tr("Send") == u"Enviar"_s);
    CHECK(tr("Welcome, %1").arg(u"Rosa"_s) == u"Bienvenido, Rosa"_s);
    CHECK(tr("It took over from another main server %n time(s).", 1) == u"Tomó el control de otro servidor principal 1 vez."_s);
    CHECK(tr("It took over from another main server %n time(s).", 3) == u"Tomó el control de otro servidor principal 3 veces."_s);
    CHECK(tr("Not a phrase anyone wrote") == u"Not a phrase anyone wrote"_s);

    // Guests' screens (the customer display): the store's language.
    i18n::setGuestLanguage(u"en"_s);
    CHECK(QCoreApplication::translate("CustomerDisplay", "Thank you!") == u"Thank you!"_s);
    i18n::setGuestLanguage(u"es"_s);
    CHECK(QCoreApplication::translate("CustomerDisplay", "Thank you!") == u"¡Gracias!"_s);

    // A while in another language (a terminal's operation on a server).
    {
        const i18n::Scope english(u"en"_s);
        CHECK(tr("Send") == u"Send"_s);
    }
    CHECK(tr("Send") == u"Enviar"_s);
}

TEST_CASE("Languages: French, Portuguese and Chinese too", "[i18n]")
{
    Spanish reset;
    QStringList codes;
    for (const i18n::Language &l : i18n::languages())
        codes << l.code;
    CHECK(codes == QStringList{u"en"_s, u"es"_s, u"fr"_s, u"pt"_s, u"zh"_s});
    REQUIRE(i18n::setLanguage(u"fr"_s));
    CHECK(tr("Send") == u"Envoyer"_s);
    CHECK(tr("%n guest(s)", 1) == u"1 couvert"_s);
    CHECK(tr("%n guest(s)", 4) == u"4 couverts"_s);
    CHECK(tr("Welcome, %1").arg(u"Rosa"_s) == u"Bienvenue, Rosa"_s);
    REQUIRE(i18n::setLanguage(u"pt"_s));
    CHECK(tr("Send") == u"Enviar"_s);
    CHECK(tr("%n guest(s)", 2) == u"2 pessoas"_s);
    REQUIRE(i18n::setLanguage(u"zh"_s));
    CHECK(tr("Send") == u"发送"_s);
    CHECK(tr("%n guest(s)", 2) == u"2 位客人"_s);
    // Printers can't print Chinese: a Chinese store's tickets are in English.
    CHECK(i18n::printLanguage(u"zh"_s) == u"en"_s);
    CHECK(i18n::printLanguage(u"fr"_s) == u"fr"_s);
}

TEST_CASE("Languages: each person's own, else the store's", "[i18n]")
{
    Spanish es;
    app::PosService pos(test::seedPosData(), nullptr);
    CHECK(pos.language() == u"en"_s);   // the store's, nobody logged in
    QSignalSpy notices(&pos, &app::PosSession::notice);
    pos.invoke(u"loginWithPin"_s, {u"5555"_s});   // Rosa
    CHECK(pos.language() == u"es"_s);
    REQUIRE_FALSE(notices.isEmpty());
    CHECK(notices.last()[0].toString() == u"Bienvenido, Rosa"_s);   // already in hers
    pos.invoke(u"logout"_s);
    CHECK(pos.language() == u"en"_s);

    // The store in Spanish: everyone without their own.
    QVariantMap store = pos.adminRecords(u"store"_s).value(0).toMap();
    pos.invoke(u"loginWithPin"_s, {u"1234"_s});
    store[u"language"_s] = u"es"_s;
    QVariant saved;
    pos.invoke(u"adminSave"_s, {u"store"_s, 0, store}, [&](const QVariant &r) { saved = r; });
    CHECK(pos.shared()->settings.language == "es");
    CHECK(pos.language() == u"es"_s);
    store[u"language"_s] = u"de"_s;   // not one we have
    pos.invoke(u"adminSave"_s, {u"store"_s, 0, store});
    CHECK(pos.shared()->settings.language == "es");
}

TEST_CASE("Languages: two terminals on one server, each in its own", "[i18n]")
{
    Spanish es;
    app::PosShared shared(test::seedPosData(), nullptr);
    app::PosService bar(&shared, u"Bar"_s);
    app::PosService patio(&shared, u"Patio"_s);
    QSignalSpy barNotices(&bar, &app::PosSession::notice);
    QSignalSpy patioNotices(&patio, &app::PosSession::notice);
    bar.invoke(u"loginWithPin"_s, {u"5555"_s});     // Rosa: Spanish
    patio.invoke(u"loginWithPin"_s, {u"1111"_s});   // Sam: the store's (English)
    CHECK(barNotices.last()[0].toString() == u"Bienvenido, Rosa"_s);
    CHECK(patioNotices.last()[0].toString() == u"Welcome, Sam"_s);
    // What the server sends each terminal is in its language too.
    CHECK(bar.snapshot()[u"language"_s] == u"es"_s);
    CHECK(patio.snapshot()[u"language"_s] == u"en"_s);
    const QVariantList barFields = bar.snapshot()[u"messages"_s].toList();
    Q_UNUSED(barFields)
    bar.invoke(u"sendMessage"_s, {u"everyone"_s, QString()});
    CHECK(barNotices.last()[0].toString() == u"Escriba el mensaje."_s);
    patio.invoke(u"sendMessage"_s, {u"everyone"_s, QString()});
    CHECK(patioNotices.last()[0].toString() == u"Type the message."_s);
}

TEST_CASE("The page editor's fields in Spanish", "[i18n][editori18n]")
{
    Spanish es;
    REQUIRE(i18n::setLanguage(u"es"_s));
    EditorController e(*vt::test::loadTestLayout());
    e.setPageId(u"items-burgers"_s);
    const auto find = [](const QVariantList &fields, const QString &path) {
        for (const QVariant &f : fields)
            if (f.toMap().value(u"path"_s) == path)
                return f.toMap();
        return QVariantMap();
    };
    const QVariantList button = e.zoneFields(u"button"_s);
    const QVariantMap fill = find(button, u"style.selected.fill"_s);
    CHECK(fill[u"label"_s] == u"Color"_s);
    CHECK(fill[u"group"_s] == u"Estilo encendido"_s);
    CHECK(fill[u"groupKey"_s] == u"Look when lit"_s);      // the Inspector starts it collapsed by this
    const QVariantMap shape = find(button, u"shape"_s);
    CHECK(shape[u"label"_s] == u"Forma"_s);
    CHECK(shape[u"options"_s].toList().first().toMap()[u"text"_s] == u"Rectángulo"_s);
    CHECK(shape[u"options"_s].toList().first().toMap()[u"value"_s] == u"rect"_s);   // values stay
    // Composed: Hide “Station…”, both parts.
    const QVariantMap hide = find(e.zoneFields(u"kitchenDisplay"_s), u"props.buttons.station.hide"_s);
    CHECK(hide[u"label"_s] == u"Ocultar “Estación…”"_s);
    CHECK(hide[u"hint"_s] == u"Comando: kitchenStation"_s);
    // Pages, the theme, actions.
    CHECK(find(e.pageFields(), u"templateId"_s)[u"label"_s] == u"Página plantilla"_s);
    CHECK(find(e.themeFields(), u"status.soldOut"_s)[u"hint"_s] == u"Normalmente #b83232"_s);
    bool jump = false;
    for (const QVariant &t : e.actionTypes())
        if (t.toMap()[u"type"_s] == u"jump"_s) {
            jump = true;
            CHECK(t.toMap()[u"label"_s] == u"Ir a página"_s);
            CHECK(t.toMap()[u"fields"_s].toList().first().toMap()[u"label"_s] == u"Cómo"_s);
        }
    CHECK(jump);
    // The page list: kinds and roles by name.
    CHECK(e.pageKindName(u"manager"_s) == u"Gerente"_s);
    CHECK(e.pageRoleName(u"weigh"_s) == u"Pesado"_s);
    CHECK(e.pageKindName(u"somethingNew"_s) == u"somethingNew"_s);
    bool kindOption = false;
    for (const QVariant &o : find(e.pageFields(), u"kind"_s)[u"options"_s].toList())
        kindOption |= o.toMap()[u"value"_s] == u"library"_s && o.toMap()[u"text"_s] == u"Biblioteca de botones"_s;
    CHECK(kindOption);
}

TEST_CASE("Spanish: reports, the tax line, dates and the staff list", "[i18n]")
{
    Spanish es;
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const i18n::Scope spanish(u"es"_s);
    // A report's title, columns and whole-phrase rows; the count stays.
    const QVariantMap sales = pos.report(u"sales"_s);
    CHECK(sales[u"title"_s] == u"Resumen de ventas"_s);
    bool netSales = false;
    for (const QVariant &row : sales[u"rows"_s].toList())
        netSales = netSales || row.toMap()[u"cells"_s].toStringList().value(0) == u"Ventas netas"_s;
    CHECK(netSales);
    // The tax on a check, by its kind.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"cobb"_s);
    const QVariantList taxes = pos.totals()[u"taxLines"_s].toList();
    REQUIRE_FALSE(taxes.isEmpty());
    CHECK(taxes[0].toMap()[u"name"_s] == u"Impuesto de comida"_s);
    // Dates in Spanish.
    CHECK(i18n::locale(u"es"_s).toString(QDate(2026, 10, 10), u"dddd"_s) == u"sábado"_s);
    // Jobs in the staff list.
    bool server = false;
    for (const QVariant &r : pos.adminRecords(u"employees"_s))
        server = server || r.toMap()[u"_detail"_s].toString().startsWith(u"Mesero"_s);
    CHECK(server);
}
