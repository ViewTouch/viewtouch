#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "app/pos_json.hh"
#include "storage/pos_store.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// The Menu Builder: categories, an item's card, what's on it.

namespace {

QVariantMap category(PosService &pos, const QString &id)
{
    for (const QVariant &c : pos.menuCategories())
        if (c.toMap()[u"id"_s] == id)
            return c.toMap();
    return {};
}

QVariantMap item(PosService &pos, const QString &name)
{
    for (const QVariant &m : pos.menuItems())
        if (m.toMap()[u"name"_s] == name)
            return m.toMap();
    return {};
}

} // namespace

TEST_CASE("Menu Builder: a category, then an item in it with what's on it", "[menubuilder]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveCategory({{u"name"_s, u"Tacos"_s}, {u"color"_s, u"#b83232"_s},
                              {u"periods"_s, QStringList{u"lunch"_s, u"dinner"_s}},
                              {u"printer"_s, u"kitchen"_s}, {u"station"_s, u"grill"_s}}));
    QVariantMap tacos = category(pos, u"tacos"_s);
    REQUIRE_FALSE(tacos.isEmpty());
    CHECK(tacos[u"color"_s] == u"#b83232"_s);
    CHECK(tacos[u"periods"_s].toStringList() == QStringList{u"lunch"_s, u"dinner"_s});
    CHECK_FALSE(pos.saveCategory({{u"name"_s, u"tacos"_s}}));   // already one

    // Every meal: all day.
    REQUIRE(pos.saveCategory({{u"id"_s, u"tacos"_s}, {u"name"_s, u"Tacos"_s},
                              {u"periods"_s, QStringList{u"breakfast"_s, u"lunch"_s, u"dinner"_s}}}));
    CHECK(category(pos, u"tacos"_s)[u"periods"_s].toStringList().isEmpty());

    REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"Carne Asada"_s}, {u"price"_s, u"3.50"_s}, {u"family"_s, u"tacos"_s},
                                  {u"onIt"_s, u"Cilantro, Onion, Salsa Verde"_s}, {u"groups"_s, QStringList{u"salad-protein"_s}}}));
    const QVariantMap asada = item(pos, u"Carne Asada"_s);
    REQUIRE_FALSE(asada.isEmpty());
    CHECK(asada[u"price"_s] == u"$3.50"_s);
    CHECK(asada[u"station"_s] == u"grill"_s);                    // its category's
    CHECK(asada[u"onIt"_s].toStringList() == QStringList{u"Cilantro"_s, u"Onion"_s, u"Salsa Verde"_s});
    CHECK(asada[u"groups"_s].toStringList() == QStringList{u"salad-protein"_s});
    CHECK_FALSE(pos.saveMenuItemCard({{u"name"_s, u"carne asada"_s}, {u"price"_s, 4}, {u"family"_s, u"tacos"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"name"_s, u"Al Pastor"_s}, {u"price"_s, u"abc"_s}, {u"family"_s, u"tacos"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"name"_s, u"Al Pastor"_s}, {u"price"_s, 3}}));   // which category?

    // Ordered: what's on it can be had No / Lite / Extra / on the Side.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(asada[u"id"_s].toString()));
    const QVariantList groups = pos.choosingInfo()[u"groups"_s].toList();
    REQUIRE(groups.size() == 2);
    const QVariantMap onIt = groups[0].toMap();
    CHECK(onIt[u"included"_s].toBool());
    REQUIRE(pos.setChoice(onIt[u"id"_s].toString(), 1, u"no"_s));   // no onion
    REQUIRE(pos.finishChoosing());
    CHECK(pos.lines()[0].toMap()[u"modifiers"_s].toList().size() == 1);
    pos.releaseCheck();

    // Changing what's on it; the category can't go while it has items.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, asada[u"id"_s]}, {u"name"_s, u"Carne Asada"_s}, {u"onIt"_s, QStringList{u"Cilantro"_s}}}));
    CHECK(item(pos, u"Carne Asada"_s)[u"onIt"_s].toStringList() == QStringList{u"Cilantro"_s});
    CHECK_FALSE(pos.deleteCategory(u"tacos"_s));
    REQUIRE(pos.deleteMenuItemCard(asada[u"id"_s].toString()));
    CHECK(item(pos, u"Carne Asada"_s).isEmpty());
    CHECK_FALSE(pos.shared()->settings.modifierGroup("on-" + asada[u"id"_s].toString().toStdString()));
    REQUIRE(pos.deleteCategory(u"tacos"_s));
    CHECK(category(pos, u"tacos"_s).isEmpty());
}

TEST_CASE("Menu Builder: categories in order; moving an item takes its kitchen along", "[menubuilder]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto order = [&] {
        QStringList ids;
        for (const QVariant &c : pos.menuCategories())
            ids << c.toMap()[u"id"_s].toString();
        return ids;
    };
    const int burgers = int(order().indexOf(u"burgers"_s));
    REQUIRE(pos.moveCategory(u"burgers"_s, -1));
    CHECK(order().indexOf(u"burgers"_s) == burgers - 1);

    // Coffee (drinks: the bar) moved to Breakfast Plates (the kitchen).
    const QVariantMap coffee = item(pos, u"Coffee"_s);
    REQUIRE(coffee[u"printer"_s] == u"bar"_s);
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, coffee[u"id"_s]}, {u"name"_s, u"Coffee"_s}, {u"family"_s, u"breakfast"_s}}));
    CHECK(item(pos, u"Coffee"_s)[u"printer"_s] == u"kitchen"_s);
    CHECK(item(pos, u"Coffee"_s)[u"family"_s] == u"breakfast"_s);

    // Choice groups list who uses them.
    for (const QVariant &g : pos.choiceGroups())
        if (g.toMap()[u"id"_s] == u"temperature"_s) {
            CHECK(g.toMap()[u"rule"_s] == u"Pick 1"_s);
            CHECK(g.toMap()[u"usedBy"_s].toStringList().contains(u"Classic Burger"_s));
        }

    // Not a manager: no.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.saveCategory({{u"name"_s, u"Pies"_s}}));
}

TEST_CASE("Menu Builder: a choice group made, used, changed, removed", "[menubuilder][choices]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto group = [&](const QString &name) {
        for (const QVariant &g : pos.choiceGroups())
            if (g.toMap()[u"name"_s] == name)
                return g.toMap();
        return QVariantMap{};
    };
    const QVariantList options{QVariantMap{{u"name"_s, u"Pico de gallo"_s}, {u"price"_s, u""_s}},
                               QVariantMap{{u"name"_s, u"Salsa Verde"_s}, {u"price"_s, u"0.50"_s}},
                               QVariantMap{{u"name"_s, u""_s}},   // a row left empty
                               QVariantMap{{u"name"_s, u"Onion"_s}, {u"included"_s, true}}};
    REQUIRE(pos.saveChoiceGroup({{u"name"_s, u"Salsa"_s}, {u"min"_s, 1}, {u"max"_s, 1}, {u"askHow"_s, true},
                                 {u"options"_s, options}}));
    QVariantMap salsa = group(u"Salsa"_s);
    REQUIRE_FALSE(salsa.isEmpty());
    CHECK(salsa[u"rule"_s] == u"Pick 1"_s);
    CHECK(salsa[u"options"_s].toList().size() == 3);
    CHECK(salsa[u"options"_s].toList()[1].toMap()[u"price"_s].toDouble() == 0.5);
    CHECK(salsa[u"options"_s].toList()[2].toMap()[u"included"_s].toBool());
    CHECK_FALSE(pos.saveChoiceGroup({{u"name"_s, u"Bad"_s}, {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"A"_s}, {u"price"_s, u"x"_s}}}}}));
    CHECK_FALSE(pos.saveChoiceGroup({{u"name"_s, u"Empty"_s}, {u"options"_s, QVariantList{}}}));
    CHECK_FALSE(pos.saveChoiceGroup({{u"name"_s, u"Twice"_s},
                                     {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"A"_s}}, QVariantMap{{u"name"_s, u"a"_s}}}}}));

    // On an item's card; changed to "up to 2"; removed: the item stops asking.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"classic-burger"_s}, {u"name"_s, u"Classic Burger"_s},
                                  {u"groups"_s, QStringList{u"temperature"_s, salsa[u"id"_s].toString()}}}));
    CHECK(group(u"Salsa"_s)[u"usedBy"_s].toStringList() == QStringList{u"Classic Burger"_s});
    REQUIRE(pos.saveChoiceGroup({{u"id"_s, salsa[u"id"_s]}, {u"name"_s, u"Salsa"_s}, {u"min"_s, 0}, {u"max"_s, 2},
                                 {u"options"_s, options}}));
    CHECK(group(u"Salsa"_s)[u"rule"_s] == u"Up to 2"_s);
    REQUIRE(pos.deleteChoiceGroup(salsa[u"id"_s].toString()));
    CHECK(group(u"Salsa"_s).isEmpty());
    for (const core::MenuItem &m : pos.shared()->menu)
        if (m.id == "classic-burger")
            CHECK(std::ranges::find(m.modifierGroups, salsa[u"id"_s].toString().toStdString()) == m.modifierGroups.end());
}

TEST_CASE("Menu Builder: many items typed at once, in any of the usual ways", "[menubuilder][fast]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // A new category, then items several ways; another category's line.
    const int added = pos.addMenuItemsFromText(u"burgers"_s,
        u"Tacos: Carne Asada 3.50, Al Pastor $3.25, Pollo - 3\n"
        u"Lengua 4,25\n"
        u"\n"
        u"Drinks: Horchata 2.75\n"
        u"Agua Fresca 2.50"_s);
    CHECK(added == 6);
    const auto priceOf = [&](const QString &name) {
        for (const QVariant &m : pos.menuItems())
            if (m.toMap()[u"name"_s] == name)
                return m.toMap()[u"family"_s].toString() + u' ' + m.toMap()[u"price"_s].toString();
        return QString();
    };
    CHECK(priceOf(u"Carne Asada"_s) == u"tacos $3.50"_s);
    CHECK(priceOf(u"Al Pastor"_s) == u"tacos $3.25"_s);
    CHECK(priceOf(u"Pollo"_s) == u"tacos $3.00"_s);
    CHECK(priceOf(u"Lengua"_s) == u"tacos $4.25"_s);   // a comma for the cents
    CHECK(priceOf(u"Horchata"_s) == u"drinks $2.75"_s);
    CHECK(priceOf(u"Agua Fresca"_s) == u"drinks $2.50"_s);

    // In the category chosen; one without a price says which; already there: skipped.
    CHECK(pos.addMenuItemsFromText(u"tacos"_s, u"Barbacoa 3.75\nCarne Asada 3.50"_s) == 1);
    CHECK(pos.addMenuItemsFromText(u"tacos"_s, u"Suadero\nChorizo 3"_s) == 0);   // nothing added: Suadero has no price
    CHECK(priceOf(u"Chorizo"_s).isEmpty());
    CHECK(pos.addMenuItemsFromText({}, u"Mole 9"_s) == 0);                        // which category?
}

TEST_CASE("Menu Builder: Duplicate copies an item with its choices and what's on it", "[menubuilder][fast]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"Fish Tacos"_s}, {u"price"_s, u"13.50"_s}, {u"family"_s, u"plates"_s},
                                  {u"onIt"_s, u"cabbage, crema"_s}, {u"groups"_s, QStringList{u"dressing"_s}}}));
    QString id;
    for (const QVariant &m : pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Fish Tacos"_s)
            id = m.toMap()[u"id"_s].toString();
    REQUIRE(pos.duplicateMenuItem(id));
    QVariantMap copy;
    for (const QVariant &m : pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Fish Tacos 2"_s)
            copy = m.toMap();
    REQUIRE_FALSE(copy.isEmpty());
    CHECK(copy[u"price"_s] == u"$13.50"_s);
    CHECK(copy[u"onIt"_s].toStringList() == QStringList{u"cabbage"_s, u"crema"_s});
    CHECK(copy[u"groups"_s].toStringList() == QStringList{u"dressing"_s});
    // Its own What's on it: changing it doesn't change the original's.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, copy[u"id"_s]}, {u"name"_s, u"Shrimp Tacos"_s}, {u"onIt"_s, u"slaw"_s}}));
    for (const QVariant &m : pos.menuItems())
        if (m.toMap()[u"name"_s] == u"Fish Tacos"_s)
            CHECK(m.toMap()[u"onIt"_s].toStringList() == QStringList{u"cabbage"_s, u"crema"_s});
}

TEST_CASE("Menu Builder: an item or a category moved to an exact place", "[menubuilder][drag]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto burgers = [&] {
        QStringList ids;
        for (const QVariant &m : pos.menuItems())
            if (m.toMap()[u"family"_s] == u"burgers"_s && !m.toMap()[u"modifier"_s].toBool())
                ids << m.toMap()[u"id"_s].toString();
        return ids;
    };
    const QStringList before = burgers();
    REQUIRE(before.first() == u"classic-burger"_s);
    REQUIRE(pos.moveMenuItemTo(u"classic-burger"_s, 3));
    QStringList expected = before;
    expected.move(0, 3);
    CHECK(burgers() == expected);
    REQUIRE(pos.moveMenuItemTo(u"classic-burger"_s, 0));
    CHECK(burgers() == before);
    // The other categories' items keep their places.
    CHECK(pos.shared()->menu.front().id == "classic-burger");

    const auto order = [&] {
        QStringList ids;
        for (const QVariant &c : pos.menuCategories())
            ids << c.toMap()[u"id"_s].toString();
        return ids;
    };
    REQUIRE(pos.moveCategoryTo(u"drinks"_s, 0));
    CHECK(order().first() == u"drinks"_s);
}

#include "app/menu_import.hh"
#include "app/xlsx.hh"
#include "layoutcontroller.hh"
#include "layout_fixture.hh"

#include <QTemporaryDir>

TEST_CASE("Menu import: the usual spreadsheet shapes", "[menubuilder][import]")
{
    using app::readMenuCsv;
    // A header in any order, quotes, a $ and a thousands comma.
    auto r = readMenuCsv(u"Category,Item,Price,Description\n"
                         u"Tacos,Carne Asada,$3.50,\"Steak, onion, cilantro\"\n"
                         u"Platters,\"Family Pack \"\"Big\"\"\",\"1,250.00\",\n"
                         u",,,\n"
                         u"Tacos,Nachos,,\n"_s);
    REQUIRE(r.items.size() == 2);
    CHECK(r.items[0].name == u"Carne Asada"_s);
    CHECK(r.items[0].price == 3.5);
    CHECK(r.items[0].category == u"Tacos"_s);
    CHECK(r.items[0].description == u"Steak, onion, cilantro"_s);
    CHECK(r.items[1].name == u"Family Pack \"Big\""_s);
    CHECK(r.items[1].price == 1250.0);
    CHECK(r.problems == QStringList{u"Row 5: no price for Nachos"_s});

    // No header: name, price, category. Semicolons and a comma for the cents.
    r = readMenuCsv(u"Horchata;2,75;Bebidas\r\nAgua de Jamaica;2,50;Bebidas\r\n"_s);
    REQUIRE(r.items.size() == 2);
    CHECK(r.items[0].price == 2.75);
    CHECK(r.items[1].category == u"Bebidas"_s);

    // Tabs; Spanish headers; what's on it.
    r = readMenuCsv(u"Nombre\tPrecio\tCategoría\tIngredientes\nTorta\t9.00\tTortas\tfrijol, aguacate\n"_s);
    REQUIRE(r.items.size() == 1);
    CHECK(r.items[0].category == u"Tortas"_s);
    CHECK(r.items[0].onIt == u"frijol, aguacate"_s);
    CHECK(r.columns.contains(u"onIt"_s));

    // A shop's sheet with what it pays and what it sells for: the price is
    // the selling price, whichever column comes first.
    r = readMenuCsv(u"Product,Cost,Retail Price,UPC\nT-Shirt,6.00,19.99,012345678905\n"_s);
    REQUIRE(r.items.size() == 1);
    CHECK(r.items[0].price == 19.99);
    // Only a cost (some menus call the price that): it is the price.
    r = readMenuCsv(u"Item,Cost\nTaco,3.25\n"_s);
    REQUIRE(r.items.size() == 1);
    CHECK(r.items[0].price == 3.25);
}

TEST_CASE("Menu import: into the menu, categories made, prices updated if asked", "[menubuilder][import]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QVariantList rows{
        QVariantMap{{u"name"_s, u"Carne Asada"_s}, {u"price"_s, 3.5}, {u"category"_s, u"Tacos"_s}, {u"onIt"_s, u"onion, cilantro"_s}},
        QVariantMap{{u"name"_s, u"Al Pastor"_s}, {u"price"_s, 3.25}, {u"category"_s, u"tacos"_s}},
        QVariantMap{{u"name"_s, u"Classic Burger"_s}, {u"price"_s, 12.0}},   // already on the menu
        QVariantMap{{u"name"_s, u"Garden Burger"_s}, {u"price"_s, 11.0}}};   // no category: the one chosen
    CHECK(pos.importMenuRows(rows, u"burgers"_s, false) == 3);
    const auto item = [&](const QString &name) {
        for (const QVariant &m : pos.menuItems())
            if (m.toMap()[u"name"_s] == name)
                return m.toMap();
        return QVariantMap{};
    };
    CHECK(item(u"Carne Asada"_s)[u"family"_s] == u"tacos"_s);
    CHECK(item(u"Al Pastor"_s)[u"family"_s] == u"tacos"_s);              // the same category, any case
    CHECK(item(u"Carne Asada"_s)[u"onIt"_s].toStringList() == QStringList{u"onion"_s, u"cilantro"_s});
    CHECK(item(u"Garden Burger"_s)[u"family"_s] == u"burgers"_s);
    CHECK(item(u"Classic Burger"_s)[u"price"_s] == u"$11.50"_s);         // left as it was
    CHECK(pos.importMenuRows({rows[2]}, u"burgers"_s, true) == 1);       // the new price, asked for
    CHECK(item(u"Classic Burger"_s)[u"price"_s] == u"$12.00"_s);
}

TEST_CASE("Menu import: reading the file on this device", "[menubuilder][import]")
{
    QTemporaryDir dir;
    QFile csv(dir.filePath(u"menu.csv"_s));
    REQUIRE(csv.open(QIODevice::WriteOnly));
    csv.write("Item,Price,Category\nCaf\xe9 de Olla,2.50,Drinks\n");   // Latin-1, as older spreadsheets save
    csv.close();
    auto l = test::loadTestLayout();
    REQUIRE(l);
    LayoutController c(*l);
    QVariantMap read = c.readMenuFile(QUrl::fromLocalFile(csv.fileName()));
    REQUIRE(read[u"items"_s].toList().size() == 1);
    CHECK(read[u"items"_s].toList()[0].toMap()[u"name"_s] == u"Café de Olla"_s);
    QFile xlsx(dir.filePath(u"menu.xlsx"_s));
    REQUIRE(xlsx.open(QIODevice::WriteOnly));
    xlsx.write("PK\x03\x04 a spreadsheet");
    xlsx.close();
    read = c.readMenuFile(QUrl::fromLocalFile(xlsx.fileName()));
    CHECK(read[u"error"_s].toString().contains(u"damaged"_s));
}

namespace {

// Raw DEFLATE, as a zip holds it: qCompress's zlib stream without its
// length, header and checksum.
QByteArray deflated(const QByteArray &data, int level = -1)
{
    const QByteArray z = qCompress(data, level);
    return z.mid(6, z.size() - 10);
}

quint32 crc(const QByteArray &data)
{
    quint32 c = 0xffffffffu;
    for (const char ch : data) {
        c ^= uchar(ch);
        for (int k = 0; k < 8; ++k)
            c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}

// A zip of these files, deflated, as Excel saves a workbook.
QByteArray zipOf(const QList<std::pair<QString, QByteArray>> &files)
{
    QByteArray out, dir;
    const auto le = [](QByteArray &b, quint32 v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            b.append(char((v >> (8 * i)) & 0xff));
    };
    for (const auto &[name, data] : files) {
        const QByteArray packed = deflated(data), n = name.toUtf8();
        const quint32 at = quint32(out.size());
        le(out, 0x04034b50, 4); le(out, 20, 2); le(out, 0, 2); le(out, 8, 2); le(out, 0, 4);
        le(out, crc(data), 4); le(out, quint32(packed.size()), 4); le(out, quint32(data.size()), 4);
        le(out, quint32(n.size()), 2); le(out, 0, 2);
        out += n + packed;
        le(dir, 0x02014b50, 4); le(dir, 20, 2); le(dir, 20, 2); le(dir, 0, 2); le(dir, 8, 2); le(dir, 0, 4);
        le(dir, crc(data), 4); le(dir, quint32(packed.size()), 4); le(dir, quint32(data.size()), 4);
        le(dir, quint32(n.size()), 2); le(dir, 0, 2); le(dir, 0, 2); le(dir, 0, 2); le(dir, 0, 2); le(dir, 0, 4);
        le(dir, at, 4);
        dir += n;
    }
    const quint32 dirAt = quint32(out.size());
    out += dir;
    le(out, 0x06054b50, 4); le(out, 0, 2); le(out, 0, 2); le(out, quint32(files.size()), 2); le(out, quint32(files.size()), 2);
    le(out, quint32(dir.size()), 4); le(out, dirAt, 4); le(out, 0, 2);
    return out;
}

// A workbook of these sheets; text cells as shared strings, numbers as numbers.
QByteArray workbookOf(const QList<std::pair<QString, QList<QStringList>>> &sheets)
{
    QStringList strings;
    QList<std::pair<QString, QByteArray>> files;
    QByteArray book = "<?xml version=\"1.0\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                      "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>";
    QByteArray rels = "<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    for (int s = 0; s < sheets.size(); ++s) {
        QByteArray xml = "<worksheet><sheetData>";
        for (int r = 0; r < sheets[s].second.size(); ++r) {
            xml += "<row r=\"" + QByteArray::number(r + 1) + "\">";
            for (int c = 0; c < sheets[s].second[r].size(); ++c) {
                const QString &v = sheets[s].second[r][c];
                if (v.isEmpty())
                    continue;   // as Excel does: an empty cell isn't written
                const QByteArray ref = QByteArray(1, char('A' + c)) + QByteArray::number(r + 1);
                bool number = false;
                v.toDouble(&number);
                if (number && !v.startsWith(u'0')) {
                    xml += "<c r=\"" + ref + "\"><v>" + v.toUtf8() + "</v></c>";
                } else {
                    if (!strings.contains(v))
                        strings << v;
                    xml += "<c r=\"" + ref + "\" t=\"s\"><v>" + QByteArray::number(strings.indexOf(v)) + "</v></c>";
                }
            }
            xml += "</row>";
        }
        xml += "</sheetData></worksheet>";
        const QByteArray n = QByteArray::number(s + 1);
        files.append({u"xl/worksheets/sheet%1.xml"_s.arg(s + 1), xml});
        book += "<sheet name=\"" + sheets[s].first.toUtf8() + "\" sheetId=\"" + n + "\" r:id=\"rId" + n + "\"/>";
        rels += "<Relationship Id=\"rId" + n + "\" Target=\"worksheets/sheet" + n + ".xml\" Type=\"x/worksheet\"/>";
    }
    rels += "<Relationship Id=\"rS\" Target=\"sharedStrings.xml\" Type=\"x/sharedStrings\"/></Relationships>";
    QByteArray shared = "<sst>";
    for (const QString &v : strings)
        shared += "<si><t>" + v.toHtmlEscaped().toUtf8() + "</t></si>";
    shared += "</sst>";
    files.append({u"xl/workbook.xml"_s, book + "</sheets></workbook>"});
    files.append({u"xl/_rels/workbook.xml.rels"_s, rels});
    files.append({u"xl/sharedStrings.xml"_s, shared});
    files.append({u"_rels/.rels"_s, "<Relationships><Relationship Id=\"r1\" Type=\"x/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"});
    return zipOf(files);
}

} // namespace

TEST_CASE("Menu import: unpacking a zip as Excel saves it", "[menubuilder][import][xlsx]")
{
    // Fixed codes (short), codes sent with the block (long), stored (level 0).
    QByteArray longText;
    for (int i = 0; i < 4000; ++i)
        longText += "Carne Asada " + QByteArray::number(i * 7919 % 1000) + ", ";
    for (const QByteArray &data : {QByteArray("Horchata"), longText, QByteArray(70000, 'x')})   // past one stored block
        for (const int level : {-1, 0, 9}) {
            INFO(data.size() << " bytes, level " << level);
            const auto out = app::inflate(deflated(data, level), data.size());
            REQUIRE(out);
            CHECK(*out == data);
        }
    CHECK_FALSE(app::inflate(deflated(longText), longText.size() - 1));   // longer than it should be
    QByteArray broken = deflated(longText);
    broken.truncate(broken.size() / 2);
    CHECK_FALSE(app::inflate(broken, longText.size()));

    const QByteArray zip = zipOf({{u"a.txt"_s, "one"}, {u"b/c.xml"_s, longText}});
    const auto files = app::unzip(zip, 1 << 20);
    REQUIRE(files);
    REQUIRE(files->size() == 2);
    CHECK(files->at(1).first == u"b/c.xml"_s);
    CHECK(files->at(1).second == longText);
    CHECK_FALSE(app::unzip(zip, 1000));   // unpacks to more than allowed
    QByteArray changed = zip;
    changed[40] = char(changed[40] ^ 0x55);   // inside a.txt's data: its check fails
    QString error;
    CHECK_FALSE(app::unzip(changed, 1 << 20, &error));
    CHECK(error.contains(u"damaged"_s));

    const auto sheets = app::readXlsx(workbookOf({{u"Menu"_s, {{u"Item"_s, u"Price"_s, u"Barcode"_s},
                                                               {u"Café de Olla"_s, u"2.5"_s, u"012345678905"_s},
                                                               {u"Tacos & Más"_s, u"3"_s}}}}));
    REQUIRE(sheets);
    REQUIRE(sheets->size() == 1);
    CHECK(sheets->at(0).name == u"Menu"_s);
    CHECK(sheets->at(0).rows.value(1) == QStringList{u"Café de Olla"_s, u"2.5"_s, u"012345678905"_s});
    CHECK(sheets->at(0).rows.value(2) == QStringList{u"Tacos & Más"_s, u"3"_s});
}

TEST_CASE("Menu import: Square's item library, a row a size", "[menubuilder][import]")
{
    const auto r = app::readMenuCsv(
        u"Token,Item Name,Variation Name,SKU,Description,Categories,Reporting Category,GTIN,Price,Archived,Sellable,"
        u"Contains Alcohol,Option Name 1,Option Value 1,Modifier Set - Milk,Modifier Set - Syrups,Tax - Sales Tax (8.5%)\n"
        u"T1,Latte,Small,,Espresso and milk,\"Coffee, Breakfast\",,,4.00,N,Y,N,,,Y,Y,Y\n"
        u"T2,Latte,Large,,,,,,5.25,N,Y,N,,,,,Y\n"
        u"T3,House IPA,Regular,,,,Beer,0012345,7.00,N,Y,Y,,,N,N,Y\n"
        u"T4,Old Soup,Regular,,,Soups,,,6.00,Y,Y,N,,,N,N,Y\n"
        u"T5,Tea,,,,Coffee,,,,N,Y,N,Size,Pot,N,N,Y\n"_s);
    CHECK(r.from == u"Square"_s);
    REQUIRE(r.items.size() == 2);
    const app::ImportedItem &latte = r.items[0];
    CHECK(latte.name == u"Latte"_s);
    CHECK(latte.category == u"Coffee"_s);   // its first category
    CHECK(latte.price == 4.0);
    REQUIRE(latte.sizes.size() == 2);
    CHECK(latte.sizes[1].name == u"Large"_s);
    CHECK(latte.sizes[1].price == 5.25);
    CHECK(latte.groups == QStringList{u"Milk"_s, u"Syrups"_s});
    const app::ImportedItem &ipa = r.items[1];
    CHECK(ipa.sizes.isEmpty());   // only Regular: no sizes
    CHECK(ipa.category == u"Beer"_s);   // its reporting category
    CHECK(ipa.alcohol);
    CHECK(ipa.barcode == u"0012345"_s);
    CHECK(r.problems.size() == 2);
    CHECK(r.problems.value(0).contains(u"Old Soup is archived"_s));
    CHECK(r.problems.value(1) == u"Row 6: no price for Tea (Pot)"_s);
}

TEST_CASE("Menu import: Clover's workbook, its items, categories and modifier groups", "[menubuilder][import][xlsx]")
{
    const QByteArray file = workbookOf({
        {u"Items"_s, {{u"Clover ID"_s, u"Name"_s, u"Alternate Name"_s, u"Price"_s, u"Price Type"_s, u"Tax Rates"_s,
                       u"Product Code"_s, u"Modifier Groups"_s, u"Hidden"_s},
                      {u"AB1"_s, u"Steak Frites"_s, u""_s, u"24.5"_s, u"Fixed"_s, u"Sales Tax"_s, u""_s, u"Temperature, Sides"_s, u"No"_s},
                      {u"AB2"_s, u"Market Fish"_s, u""_s, u"0"_s, u"Variable"_s, u""_s, u""_s, u""_s, u"No"_s},
                      {u"AB3"_s, u"Soda"_s, u""_s, u"2"_s, u"Fixed"_s, u""_s, u"049000050103"_s, u"Size"_s, u"No"_s},
                      {u"AB4"_s, u"Staff Water"_s, u""_s, u"0"_s, u"Fixed"_s, u""_s, u""_s, u""_s, u"Yes"_s}}},
        {u"Categories"_s, {{u"Category Name"_s, u"Items in Category"_s},
                           {u"Entrees"_s, u"Steak Frites, Market Fish"_s},
                           {u"Drinks"_s, u"Soda"_s}}},
        {u"Modifier Groups"_s, {{u"Modifier Group Name"_s, u"Modifier"_s, u"Price"_s, u"Min Required"_s, u"Max Allowed"_s},
                                {u"Temperature"_s, u"Rare"_s, u""_s, u"1"_s, u"1"_s},
                                {u""_s, u"Medium"_s},
                                {u""_s, u"Well Done"_s},
                                {u"Sides"_s, u"Salad"_s, u"0"_s},
                                {u""_s, u"Truffle Fries"_s, u"3.5"_s},
                                {u"Size"_s, u"Small"_s, u"0"_s, u"1"_s, u"1"_s},
                                {u""_s, u"Large"_s, u"1"_s}}}});
    const auto sheets = app::readXlsx(file);
    REQUIRE(sheets);
    const auto r = app::readMenuSheets(*sheets);
    CHECK(r.from == u"Clover"_s);
    REQUIRE(r.items.size() == 2);
    CHECK(r.items[0].name == u"Steak Frites"_s);
    CHECK(r.items[0].category == u"Entrees"_s);
    CHECK(r.items[0].groups == QStringList{u"Temperature"_s, u"Sides"_s});
    CHECK(r.items[1].category == u"Drinks"_s);
    CHECK(r.items[1].barcode == u"049000050103"_s);
    REQUIRE(r.items[1].sizes.size() == 2);   // the Size group: its sizes
    CHECK(r.items[1].sizes[1].price == 3.0);
    CHECK(r.items[1].groups.isEmpty());
    REQUIRE(r.groups.size() == 2);   // Size is gone
    CHECK(r.groups[0].name == u"Temperature"_s);
    CHECK(r.groups[0].min == 1);
    CHECK(r.groups[0].max == 1);
    CHECK(r.groups[0].options.size() == 3);
    CHECK(r.groups[1].max == 0);   // Sides: any number
    CHECK(r.groups[1].options[1].price == 3.5);
    CHECK(r.problems.size() == 2);
    CHECK(r.problems.value(0).contains(u"Market Fish is priced as it's sold"_s));
    CHECK(r.problems.value(1).contains(u"Staff Water is hidden"_s));
}

TEST_CASE("Menu import: Toast's menu export", "[menubuilder][import]")
{
    const QByteArray json = R"({
      "restaurantGuid": "x",
      "menus": [
        {"name": "Dinner", "menuGroups": [
          {"name": "Pizza", "menuItems": [
            {"name": "Margherita", "description": "Tomato, basil", "price": null, "plu": "101", "modifierGroupReferences": [1, 2]}
          ], "menuGroups": [
            {"name": "White Pies", "menuItems": [{"name": "Bianca", "price": 14, "modifierGroupReferences": [2]}]}
          ]},
          {"name": "Drinks", "menuItems": [
            {"name": "Lemonade", "price": 3.5, "modifierGroupReferences": [3]},
            {"name": "Mystery", "price": null}
          ]}
        ]},
        {"name": "Lunch", "menuGroups": [{"name": "Pizza", "menuItems": [{"name": "Margherita", "price": 9}]}]}
      ],
      "modifierGroupReferences": {
        "1": {"referenceId": 1, "name": "Size", "minSelections": 1, "maxSelections": 1, "modifierOptionReferences": [10, 11]},
        "2": {"referenceId": 2, "name": "Toppings", "minSelections": 0, "maxSelections": null, "modifierOptionReferences": [12, 13]},
        "3": {"referenceId": 3, "name": "Size", "minSelections": 1, "maxSelections": 1, "modifierOptionReferences": [14, 15]}
      },
      "modifierOptionReferences": {
        "10": {"referenceId": 10, "name": "12 inch", "price": 12},
        "11": {"referenceId": 11, "name": "16 inch", "price": 16},
        "12": {"referenceId": 12, "name": "Basil", "price": 0},
        "13": {"referenceId": 13, "name": "Pepperoni", "price": 2.5},
        "14": {"referenceId": 14, "name": "Small", "price": 0},
        "15": {"referenceId": 15, "name": "Large", "price": 1}
      }
    })";
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    REQUIRE(app::isToastMenu(doc));
    const auto r = app::readToastMenu(doc);
    CHECK(r.from == u"Toast"_s);
    REQUIRE(r.items.size() == 3);   // Margherita once
    const app::ImportedItem &m = r.items[0];
    CHECK(m.category == u"Pizza"_s);
    CHECK(m.number == u"101"_s);
    REQUIRE(m.sizes.size() == 2);   // priced by its size
    CHECK(m.sizes[0].price == 12.0);
    CHECK(m.price == 12.0);
    CHECK(m.groups == QStringList{u"Toppings"_s});
    CHECK(r.items[1].name == u"Bianca"_s);
    CHECK(r.items[1].section == u"White Pies"_s);
    CHECK(r.items[2].name == u"Lemonade"_s);
    REQUIRE(r.items[2].sizes.size() == 2);   // the other Size (Size 2), its own prices
    CHECK(r.items[2].sizes[1].price == 4.5);
    REQUIRE(r.groups.size() == 1);
    CHECK(r.groups[0].max == 0);
    CHECK(r.problems == QStringList{u"No price for Mystery"_s});
}

TEST_CASE("Menu import: Toast's item list (MenuItem_Export.csv)", "[menubuilder][import]")
{
    // Its items and modifier options together, one row each; the same item
    // once for each menu it's on; Latin-1, as Toast saves it.
    const QByteArray file =
        "Item ID,GUID,Name,Number,Imported ID,Base Price,Created Date,Archived,Modifier,SKU,PLU\n"
        "1800000000766735067,003eb2af-4dd9,Cilantro,1800000000766735068,,0.00,9/14/2026,No,Yes,,\n"
        "1800000000433270312,00ad0874-5d38,Carajillo,1800000000433270313,,10.99,8/14/2026,No,No,,\n"
        "1800000000343959572,052235e4-8bdf,Pi\xf1" "a,1800000000343959573,,4.99,8/4/2026,No,No,,\n"
        "1800000000909596644,08fb4269-cc71,Beans,1800000000909596645,,0.00,9/26/2026,No,Yes,,\n"
        "1800000000343959369,09a5fddf-a217,Burrito Regular,1800000000343959370,,15.99,8/4/2026,No,No,,104\n"
        "1800000000343959371,09a5fddf-a218,Burrito Regular,1800000000343959372,,15.99,8/4/2026,No,No,,\n"
        "1800000000343959373,09a5fddf-a219,Old Special,1800000000343959374,,9.99,8/4/2026,Yes,No,,\n";
    const auto r = app::readMenuCsv(QString::fromLatin1(file));
    CHECK(r.from == u"Toast"_s);
    REQUIRE(r.items.size() == 3);
    CHECK(r.items[0].name == u"Carajillo"_s);
    CHECK(r.items[0].price == 10.99);
    CHECK(r.items[1].name == u"Piña"_s);
    CHECK(r.items[2].number == u"104"_s);   // its PLU, not Toast's own Number
    REQUIRE(r.problems.size() == 3);
    CHECK(r.problems[0].contains(u"2 modifier option"_s));
    CHECK(r.problems[1].contains(u"Burrito Regular"_s));
    CHECK(r.problems[2].contains(u"Old Special is archived"_s));

    // Toast's other lists: no items, said so (not each one as an item at $0).
    const auto groups = app::readMenuCsv(QString::fromLatin1(
        "Item ID,GUID,Name,Number,Imported ID,Base Price,Created Date,Archived,Modifier,Parent Name,Parent Id,Parent Class\n"
        "1800000000343959195,825b6096-0c22,Burritos,1800000000343959196,,0.00,8/4/2026,No,No,Food,1800000000343959004,Menu\n"));
    CHECK(groups.items.isEmpty());
    REQUIRE(groups.problems.size() == 1);
    CHECK(groups.problems[0].contains(u"menu groups"_s));
    const auto options = app::readMenuCsv(QString::fromLatin1(
        "Item ID,GUID,Name,Number,Imported ID,Base Price,Created Date,Archived,Modifier\n"
        "1800000000343958720,02e5367e-946b,Canadian Bacon,1800000000343958724,,2.00,8/4/2026,No,No\n"));
    CHECK(options.items.isEmpty());
    REQUIRE(options.problems.size() == 1);
    CHECK(options.problems[0].contains(u"modifier options"_s));
}

TEST_CASE("Menu import: sizes, choice groups and alcohol onto the menu", "[menubuilder][import]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QVariantList groups{
        QVariantMap{{u"name"_s, u"Steak Temp"_s}, {u"min"_s, 1}, {u"max"_s, 1},
                    {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"Rare"_s}, {u"price"_s, u"0.00"_s}},
                                                QVariantMap{{u"name"_s, u"Well"_s}, {u"price"_s, u"0.00"_s}}}}},
        QVariantMap{{u"name"_s, u"Unused"_s}, {u"min"_s, 0}, {u"max"_s, 0},
                    {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"A"_s}, {u"price"_s, u"1.00"_s}}}}}};
    const QVariantList rows{
        QVariantMap{{u"name"_s, u"Flat White"_s}, {u"price"_s, 4.0}, {u"category"_s, u"Coffee"_s}, {u"number"_s, u"555"_s},
                    {u"sizes"_s, QVariantList{QVariantMap{{u"name"_s, u"Small"_s}, {u"price"_s, 4.0}},
                                              QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, 5.25}}}},
                    {u"groups"_s, QStringList{u"Milk"_s}}},
        QVariantMap{{u"name"_s, u"Ribeye"_s}, {u"price"_s, 31.0}, {u"category"_s, u"Steaks"_s}, {u"section"_s, u"From the Grill"_s},
                    {u"groups"_s, QStringList{u"steak temp"_s}}},
        QVariantMap{{u"name"_s, u"Pilsner"_s}, {u"price"_s, 6.0}, {u"category"_s, u"Beer"_s}, {u"alcohol"_s, true}}};
    CHECK(pos.importMenuRows(rows, {}, false, groups) == 3);
    const auto item = [&](const QString &name) {
        for (const QVariant &m : pos.menuItems())
            if (m.toMap()[u"name"_s] == name)
                return m.toMap();
        return QVariantMap{};
    };
    const auto group = [&](const QString &name) {
        for (const core::ModifierGroup &g : pos.settings().modifierGroups)
            if (QString::fromStdString(g.name) == name)
                return &g;
        return static_cast<const core::ModifierGroup *>(nullptr);
    };
    const QVariantMap flat = item(u"Flat White"_s);
    CHECK(flat[u"price"_s] == u"$4.00"_s);
    CHECK(flat[u"number"_s] == u"555"_s);
    const QVariantList sizes = flat[u"sizes"_s].toList();
    REQUIRE(sizes.size() == 2);
    CHECK(sizes[1].toMap()[u"name"_s] == u"Large"_s);
    CHECK(sizes[1].toMap()[u"price"_s] == u"5.25"_s);
    const QVariantMap ribeye = item(u"Ribeye"_s);
    CHECK(ribeye[u"section"_s] == u"From the Grill"_s);
    REQUIRE(group(u"Steak Temp"_s));
    CHECK(group(u"Steak Temp"_s)->min == 1);
    CHECK(ribeye[u"groups"_s].toStringList().contains(QString::fromStdString(group(u"Steak Temp"_s)->id)));
    CHECK_FALSE(group(u"Unused"_s));   // no item asks for it
    CHECK(item(u"Pilsner"_s)[u"taxClass"_s] == u"alcohol"_s);
    // Again: nothing doubles, and the group here is used as it is.
    CHECK(pos.importMenuRows({rows[1]}, {}, false, groups) == 0);
    int temps = 0;
    for (const core::ModifierGroup &g : pos.settings().modifierGroups)
        temps += g.name == "Steak Temp";
    CHECK(temps == 1);
}

TEST_CASE("Menu import: an Excel file and a Toast file, read on this device", "[menubuilder][import][xlsx]")
{
    QTemporaryDir dir;
    auto l = test::loadTestLayout();
    REQUIRE(l);
    LayoutController c(*l);
    QFile xlsx(dir.filePath(u"menu.xlsx"_s));
    REQUIRE(xlsx.open(QIODevice::WriteOnly));
    xlsx.write(workbookOf({{u"Sheet1"_s, {{u"Item"_s, u"Price"_s, u"Category"_s}, {u"Agua Fresca"_s, u"3.25"_s, u"Drinks"_s}}}}));
    xlsx.close();
    QVariantMap read = c.readMenuFile(QUrl::fromLocalFile(xlsx.fileName()));
    REQUIRE(read[u"items"_s].toList().size() == 1);
    CHECK(read[u"items"_s].toList()[0].toMap()[u"price"_s].toDouble() == 3.25);

    QFile toast(dir.filePath(u"MenuExport.json"_s));
    REQUIRE(toast.open(QIODevice::WriteOnly));
    toast.write(R"([{"name": "All Day", "menuGroups": [{"name": "Sides", "menuItems": [{"name": "Fries", "price": 4}]}]}])");
    toast.close();
    read = c.readMenuFile(QUrl::fromLocalFile(toast.fileName()));
    CHECK(read[u"from"_s] == u"Toast"_s);
    REQUIRE(read[u"items"_s].toList().size() == 1);
    CHECK(read[u"items"_s].toList()[0].toMap()[u"category"_s] == u"Sides"_s);
}

TEST_CASE("Starter menus: added alongside, ready to order", "[menubuilder][templates]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QVariantList templates = pos.menuTemplates();
    REQUIRE(templates.size() == 4);
    const int before = int(pos.menuItems().size());
    for (const QVariant &t : templates) {
        INFO(t.toMap()[u"id"_s].toString().toStdString());
        REQUIRE(pos.applyMenuTemplate(t.toMap()[u"id"_s].toString()));
    }
    CHECK(pos.menuItems().size() > before + 30);   // those already on the menu (Classic Burger, Fries...) skipped
    QVariantMap taco, latte;
    for (const QVariant &m : pos.menuItems()) {
        if (m.toMap()[u"name"_s] == u"Carne Asada Taco"_s)
            taco = m.toMap();
        if (m.toMap()[u"name"_s] == u"Latte"_s)
            latte = m.toMap();
    }
    REQUIRE_FALSE(taco.isEmpty());
    CHECK(taco[u"price"_s] == u"$3.50"_s);
    CHECK(taco[u"onIt"_s].toStringList() == QStringList{u"onion"_s, u"cilantro"_s});
    CHECK(taco[u"station"_s] == u"grill"_s);
    CHECK(latte[u"printer"_s] == u"bar"_s);
    // Applied again: nothing doubled.
    const auto count = pos.menuItems().size();
    REQUIRE(pos.applyMenuTemplate(u"taqueria"_s));
    CHECK(pos.menuItems().size() == count);
    // A taco orders, asks its salsa and tortilla, and can be had No onion.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(taco[u"id"_s].toString()));
    QStringList asked;
    for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList())
        asked << g.toMap()[u"name"_s].toString();
    CHECK(asked.contains(u"Salsa"_s));
    CHECK(asked.contains(u"Tortilla"_s));
    CHECK_FALSE(pos.finishChoosing());   // salsa and tortilla are required
}

TEST_CASE("Ready to go: the starter menu is; mistakes are caught", "[menubuilder][check]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto problems = [&](bool seriousOnly) {
        QStringList out;
        for (const QVariant &p : pos.menuProblems())
            if (!seriousOnly || p.toMap()[u"serious"_s].toBool())
                out << p.toMap()[u"text"_s].toString();
        return out;
    };
    INFO(problems(false).join(u"\n"_s).toStdString());
    CHECK(problems(true).isEmpty());

    // Mistakes.
    auto &menu = pos.shared()->menu;
    menu[0].printer = "pizza-oven";
    menu[1].modifierGroups.push_back("gone");
    menu[2].name = menu[3].name;
    pos.shared()->settings.modifierGroups.push_back({"empty", "Salsa", 1, 1, {}, false, false});
    REQUIRE(pos.saveCategory({{u"name"_s, u"Desserts"_s}}));
    const QStringList found = problems(false);
    INFO(found.join(u"\n"_s).toStdString());
    CHECK(found.filter(u"pizza-oven"_s).size() == 1);
    CHECK(found.filter(u"choice group that's gone"_s).size() == 1);
    CHECK(found.filter(u"There are two"_s).size() == 1);
    CHECK(found.filter(u"Salsa has no options"_s).size() == 1);
    CHECK(found.filter(u"Desserts has no items yet"_s).size() == 1);
    CHECK(problems(true).size() == 3);   // an empty category is a note; the kitchen prints pizza-oven's
    CHECK(problems(true).filter(u"pizza-oven"_s).isEmpty());

    // No kitchen printer to take them: they print nowhere.
    auto &printers = pos.shared()->settings.printers;
    std::erase_if(printers, [](const core::PrinterConfig &p) { return p.id == "kitchen"; });
    REQUIRE_FALSE(printers.empty());
    CHECK(problems(true).filter(u"pizza-oven"_s).size() == 1);
    // A store with no printers at all (kitchen screens): nothing to say.
    printers.clear();
    CHECK(problems(false).filter(u"printer"_s).isEmpty());
}

TEST_CASE("Menu Builder: prices typed with a comma", "[menubuilder]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto priceOf = [&](const QString &name) {
        for (const auto &m : pos.shared()->menu)
            if (QString::fromStdString(m.name) == name)
                return m.price.cents();
        return decltype(pos.shared()->menu.front().price.cents())(-1);
    };
    const QString family = QString::fromStdString(pos.shared()->menu.front().family);
    REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"Churros"_s}, {u"price"_s, u"3,50"_s}, {u"family"_s, family}}));
    CHECK(priceOf(u"Churros"_s) == 350);
    REQUIRE(pos.saveMenuItemCard({{u"name"_s, u"Party Tray"_s}, {u"price"_s, u"$1,250.00"_s}, {u"family"_s, family}}));
    CHECK(priceOf(u"Party Tray"_s) == 125000);
    CHECK_FALSE(pos.saveMenuItemCard({{u"name"_s, u"Flan"_s}, {u"price"_s, u"cheap"_s}, {u"family"_s, family}}));
}

// Prints what the check finds in a store's database: VTM_PROBLEMS_DB=<file>.
TEST_CASE("Ready to go: a store's database", "[.][problemsdb]")
{
    const QByteArray path = qgetenv("VTM_PROBLEMS_DB");
    REQUIRE_FALSE(path.isEmpty());
    vt::storage::PosStore store(QString::fromLocal8Bit(path));
    QString error;
    REQUIRE(store.open(&error));
    QStringList errors;
    auto data = store.load(&errors);
    REQUIRE(data);
    PosService pos(std::move(*data), nullptr);
    for (const QVariant &p : pos.menuProblems())
        WARN((p.toMap()[u"serious"_s].toBool() ? "SERIOUS " : "note ") << p.toMap()[u"text"_s].toString().toStdString());
}

TEST_CASE("Printers: what was learned about one is kept, until its address changes", "[print][learned]")
{
    PosService pos(test::seedPosData(true), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    auto &printers = pos.shared()->settings.printers;
    REQUIRE_FALSE(printers.empty());
    const int at = 0;
    printers[at].type = "network";
    printers[at].host = "192.168.1.101";
    printers[at].reportsStatus = true;
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).printers[at].reportsStatus);   // stored
    QVariantMap record = app::toJson(printers[at]).toVariantMap();
    record.remove(u"reportsStatus"_s);                    // the form doesn't have it
    record[u"name"_s] = u"Kitchen Epson"_s;
    REQUIRE(pos.adminSave(u"printers"_s, at, record));
    CHECK(printers[at].reportsStatus);                    // renamed: the same printer
    record[u"host"_s] = u"192.168.88.40"_s;
    REQUIRE(pos.adminSave(u"printers"_s, at, record));
    CHECK_FALSE(printers[at].reportsStatus);              // another address: maybe another printer
}

TEST_CASE("Sent by a screen: reports and settings only for managers", "[security][managersonly]")
{
    PosService pos(test::seedPosData(true), nullptr);
    const auto ask = [&](const QString &m, const QVariantList &a) {
        QVariant out;
        pos.invoke(m, a, [&](const QVariant &v) { out = v; });
        return out;
    };
    REQUIRE(pos.loginWithPin(u"2222"_s));   // a server
    CHECK(ask(u"adminRecords"_s, {u"employees"_s}).toList().isEmpty());
    CHECK(ask(u"adminFields"_s, {u"store"_s}).toList().isEmpty());
    CHECK(ask(u"adminNewRecord"_s, {u"employees"_s}).toMap().isEmpty());
    CHECK(ask(u"report"_s, {u"sales"_s, 0}).toMap().value(u"rows"_s).toList().isEmpty());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));   // a manager
    CHECK_FALSE(ask(u"adminRecords"_s, {u"employees"_s}).toList().isEmpty());
    CHECK_FALSE(ask(u"report"_s, {u"sales"_s, 0}).toMap().value(u"title"_s).toString().contains(u"managers"_s));
}

TEST_CASE("Menu Builder: undo, a change at a time; not over a change made elsewhere", "[menubuilder][menuundo]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(true), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto menu0 = pos.shared()->menu;
    const auto groups0 = pos.shared()->settings.modifierGroups;
    CHECK(pos.menuUndoText().isEmpty());

    // A starter menu: many saves, one step.
    REQUIRE(pos.applyMenuTemplate(u"pizza"_s));
    CHECK(pos.menuUndoText() == u"Starter menu"_s);
    REQUIRE(pos.deleteMenuItemCard(u"classic-burger"_s));
    CHECK(pos.menuUndoText() == u"Remove an item"_s);
    // A save that changes nothing: no step.
    const QString name = QString::fromStdString(pos.shared()->menu.front().name);
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, QString::fromStdString(pos.shared()->menu.front().id)}, {u"name"_s, name}}));
    CHECK(pos.menuUndoText() == u"Remove an item"_s);

    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(u"classic-burger"_s));          // back, and stored again
    CHECK(sink.savedMenuIds().contains(u"classic-burger"_s));
    CHECK(pos.menuUndoText() == u"Starter menu"_s);
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.shared()->menu == menu0);                 // as it was, choice groups too
    CHECK(pos.shared()->settings.modifierGroups == groups0);
    CHECK_FALSE(pos.undoMenuChange());                  // nothing more

    // Changed elsewhere since (Manager -> Menu): not undone over it.
    REQUIRE(pos.saveCategory({{u"name"_s, u"Desserts"_s}}));
    pos.shared()->menu.front().price = vt::Money::fromCents(1);   // as another editor would
    CHECK_FALSE(pos.undoMenuChange());
    CHECK(pos.menuUndoText().isEmpty());
    CHECK(pos.shared()->menu.front().price.cents() == 1);

    // Only managers.
    REQUIRE(pos.saveCategory({{u"name"_s, u"Sides"_s}}));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"2222"_s));
    CHECK_FALSE(pos.undoMenuChange());
}

TEST_CASE("Menu Builder: the card's More settings; many prices at once", "[menubuilder][cardmore]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QString id = u"classic-burger"_s;
    const QString name = item(pos, u"Classic Burger"_s)[u"name"_s].toString();
    REQUIRE(name == u"Classic Burger"_s);

    REQUIRE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"kitchenName"_s, u" CLASSIC "_s},
                                  {u"number"_s, u"904"_s}, {u"prepMinutes"_s, u"12"_s}, {u"takeoutPrice"_s, u"$13.25"_s},
                                  {u"periodPrices"_s, QVariantMap{{u"dinner"_s, u"14.50"_s}, {u"lunch"_s, u""_s}}},
                                  {u"buttonColor"_s, u"#b83232"_s}, {u"taxClass"_s, u"alcohol"_s},
                                  {u"description"_s, u"Two patties"_s}}));
    const core::MenuItem *m = pos.findItem(id);
    REQUIRE(m);
    CHECK(m->kitchenName == "CLASSIC");
    CHECK(m->number == "904");
    CHECK(m->prepMinutes == 12);
    CHECK(m->takeoutPrice.cents() == 1325);
    CHECK(m->periodPrices.size() == 1);
    CHECK(m->periodPrices.at("dinner").cents() == 1450);
    CHECK(m->buttonColor == "#b83232");
    CHECK(m->taxClass == core::TaxClass::Alcohol);
    CHECK(m->description == "Two patties");
    CHECK(sink.savedMenuIds().contains(id));
    // Read back the way the card shows it.
    const QVariantMap shown = item(pos, name);
    CHECK(shown[u"kitchenName"_s] == u"CLASSIC"_s);
    CHECK(shown[u"prepMinutes"_s].toInt() == 12);
    CHECK(shown[u"takeoutPrice"_s] == u"13.25"_s);
    CHECK(shown[u"periodPrices"_s].toMap() == QVariantMap{{u"dinner"_s, u"14.50"_s}});

    // Mistakes are refused, nothing changed.
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, u"cheeseburger"_s}, {u"name"_s, u"Cheeseburger"_s}, {u"number"_s, u"904"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"number"_s, u"1a"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"prepMinutes"_s, u"soon"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"periodPrices"_s, QVariantMap{{u"brunch"_s, u"9"_s}}}}));
    CHECK(pos.findItem(id)->number == "904");
    // Emptied: back to the regular price, learned kitchen time, no number.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"number"_s, u""_s}, {u"prepMinutes"_s, u""_s},
                                  {u"takeoutPrice"_s, u""_s}, {u"periodPrices"_s, QVariantMap{{u"dinner"_s, u""_s}}}}));
    CHECK(pos.findItem(id)->number.empty());
    CHECK(pos.findItem(id)->prepMinutes == 0);
    CHECK(pos.findItem(id)->takeoutPrice.cents() == 0);
    CHECK(pos.findItem(id)->periodPrices.empty());

    // Prices…: as previewed, its other prices too; one Undo step.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"cheeseburger"_s}, {u"name"_s, u"Cheeseburger"_s}, {u"takeoutPrice"_s, u"9.00"_s},
                                  {u"periodPrices"_s, QVariantMap{{u"lunch"_s, u"10.00"_s}}}}));
    const qint64 classic = pos.findItem(id)->price.cents(), cheese = pos.findItem(u"cheeseburger"_s)->price.cents();
    REQUIRE(pos.setMenuPrices({QVariantMap{{u"id"_s, id}, {u"price"_s, u"20.00"_s}},
                               QVariantMap{{u"id"_s, u"cheeseburger"_s}, {u"price"_s, u"21.05"_s}, {u"takeoutPrice"_s, u"9.90"_s},
                                           {u"periodPrices"_s, QVariantMap{{u"lunch"_s, u"11.00"_s}}}}}));
    CHECK(pos.findItem(id)->price.cents() == 2000);
    CHECK(pos.findItem(u"cheeseburger"_s)->price.cents() == 2105);
    CHECK(pos.findItem(u"cheeseburger"_s)->takeoutPrice.cents() == 990);
    CHECK(pos.findItem(u"cheeseburger"_s)->periodPrices.at("lunch").cents() == 1100);
    CHECK(pos.menuUndoText() == u"Change 2 prices"_s);
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(id)->price.cents() == classic);
    CHECK(pos.findItem(u"cheeseburger"_s)->price.cents() == cheese);
    CHECK(pos.findItem(u"cheeseburger"_s)->periodPrices.at("lunch").cents() == 1000);
    // One gone or a bad price: nothing changes.
    CHECK_FALSE(pos.setMenuPrices({QVariantMap{{u"id"_s, id}, {u"price"_s, u"1.00"_s}},
                                   QVariantMap{{u"id"_s, u"no-such"_s}, {u"price"_s, u"2.00"_s}}}));
    CHECK_FALSE(pos.setMenuPrices({QVariantMap{{u"id"_s, id}, {u"price"_s, u"cheap"_s}}}));
    CHECK_FALSE(pos.setMenuPrices({QVariantMap{{u"id"_s, id}, {u"price"_s, u"1.00"_s},
                                               {u"periodPrices"_s, QVariantMap{{u"dinner"_s, u"2.00"_s}}}}}));   // it has none
    CHECK(pos.findItem(id)->price.cents() == classic);
    // Only managers.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"2222"_s));
    CHECK_FALSE(pos.setMenuPrices({QVariantMap{{u"id"_s, id}, {u"price"_s, u"1.00"_s}}}));
}

TEST_CASE("Colors the store mixed: newest first, a dozen kept, stored", "[menubuilder][colors]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.addCustomColor(u"blue"_s));
    REQUIRE(pos.addCustomColor(u"#ABCDEF"_s));
    REQUIRE(pos.addCustomColor(u"#123456"_s));
    REQUIRE(pos.addCustomColor(u"#abcdef"_s));             // again: first, once
    CHECK(pos.customColors() == QStringList{u"#abcdef"_s, u"#123456"_s});
    for (int i = 0; i < 20; ++i)
        REQUIRE(pos.addCustomColor(u"#0000%1"_s.arg(i, 2, 10, QChar(u'0'))));
    CHECK(pos.customColors().size() == 12);
    CHECK(pos.customColors().front() == u"#000019"_s);
    // With the store's settings.
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(back.customColors == pos.shared()->settings.customColors);
    pos.logout();
    REQUIRE(pos.loginWithPin(u"2222"_s));
    CHECK_FALSE(pos.addCustomColor(u"#ffffff"_s));
}

TEST_CASE("Menu Builder: a category's buttons: their size, photos, prices", "[menubuilder][categorylook]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"buttonSize"_s, u"large"_s},
                              {u"photos"_s, true}, {u"hidePrice"_s, true}}));
    const QVariantMap c = category(pos, u"burgers"_s);
    CHECK(c[u"buttonSize"_s] == u"large"_s);
    CHECK(c[u"photos"_s].toBool());
    CHECK(c[u"hidePrice"_s].toBool());
    CHECK_FALSE(pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"buttonSize"_s, u"huge"_s}}));
    // Kept with the store's settings, and in a menu file for another store.
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    const auto it = std::ranges::find_if(back.menuCategories, [](const core::MenuCategory &x) { return x.id == "burgers"; });
    REQUIRE(it != back.menuCategories.end());
    CHECK(it->buttonSize == "large");
    CHECK(it->photos);
    CHECK(it->hidePrice);
    bool exported = false;
    for (const QVariant &x : pos.menuExport()[u"categories"_s].toList())
        if (x.toMap()[u"id"_s] == u"burgers"_s)
            exported = x.toMap()[u"buttonSize"_s] == u"large"_s && x.toMap()[u"hidePrice"_s].toBool();
    CHECK(exported);
    // Back to fitting.
    REQUIRE(pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"buttonSize"_s, u""_s}}));
    CHECK(category(pos, u"burgers"_s)[u"buttonSize"_s] == u""_s);
}

TEST_CASE("Menu Builder: text size for a category's items and for one item", "[menubuilder][textsize]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"textSize"_s, 125}}));
    CHECK(category(pos, u"burgers"_s)[u"textSize"_s].toInt() == 125);
    QVariantMap card = item(pos, u"Classic Burger"_s);
    REQUIRE_FALSE(card.isEmpty());
    card[u"textSize"_s] = 80;
    REQUIRE(pos.saveMenuItemCard(card));
    CHECK(item(pos, u"Classic Burger"_s)[u"textSize"_s].toInt() == 80);
    CHECK(item(pos, u"Cheeseburger"_s)[u"textSize"_s].toInt() == 0);   // its category's
    // Kept: the category with the settings, the item with the menu.
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    const auto it = std::ranges::find_if(back.menuCategories, [](const core::MenuCategory &x) { return x.id == "burgers"; });
    REQUIRE(it != back.menuCategories.end());
    CHECK(it->textSize == 125);
    const core::MenuItem *m = pos.findItem(u"classic-burger"_s);
    REQUIRE(m);
    CHECK(app::menuItemFromJson(app::toJson(*m)).textSize == 80);
    // Nonsense is the usual; back to the category's.
    card[u"textSize"_s] = -5;
    REQUIRE(pos.saveMenuItemCard(card));
    CHECK(pos.findItem(u"classic-burger"_s)->textSize == 0);
}

TEST_CASE("Menu Builder: several prices moved at once, by an amount or a share", "[menubuilder][several]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QStringList two{u"classic-burger"_s, u"cheeseburger"_s};
    const Money burger = pos.findItem(u"classic-burger"_s)->price, cheese = pos.findItem(u"cheeseburger"_s)->price;

    REQUIRE(pos.changeMenuItems(two, {{u"priceAdd"_s, 50}}));
    CHECK(pos.findItem(u"classic-burger"_s)->price == burger + Money::fromCents(50));
    CHECK(pos.findItem(u"cheeseburger"_s)->price == cheese + Money::fromCents(50));

    REQUIRE(pos.changeMenuItems(two, {{u"pricePercent"_s, -1000}}));   // 10% less, each from its own
    const Money b = burger + Money::fromCents(50);
    CHECK(pos.findItem(u"classic-burger"_s)->price == b - b.percent(1000));

    REQUIRE(pos.changeMenuItems(two, {{u"priceAdd"_s, -100000}}));   // never below zero
    CHECK(pos.findItem(u"cheeseburger"_s)->price == Money());
    CHECK_FALSE(pos.changeMenuItems(two, {{u"pricePercent"_s, -10000}}));

    REQUIRE(pos.undoMenuChange());
    REQUIRE(pos.undoMenuChange());
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(u"classic-burger"_s)->price == burger);
    // Tax for several at once.
    REQUIRE(pos.changeMenuItems(two, {{u"taxClass"_s, u"alcohol"_s}}));
    CHECK(pos.findItem(u"cheeseburger"_s)->taxClass == core::TaxClass::Alcohol);
}

TEST_CASE("Menu Builder: several items changed or removed at once, one Undo", "[menubuilder][several]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QStringList two{u"classic-burger"_s, u"cheeseburger"_s};
    REQUIRE(pos.changeMenuItems(two, {{u"family"_s, u"salads"_s}, {u"buttonColor"_s, u"#123456"_s}}));
    for (const QString &id : two) {
        CHECK(pos.findItem(id)->family == "salads");
        CHECK(pos.findItem(id)->buttonColor == "#123456");
    }
    CHECK(pos.menuUndoText() == u"Change 2 items"_s);
    REQUIRE(pos.changeMenuItems(two, {{u"available"_s, false}}));
    CHECK_FALSE(pos.findItem(u"cheeseburger"_s)->available);
    REQUIRE(pos.undoMenuChange());
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(u"cheeseburger"_s)->family == "burgers");
    CHECK(pos.findItem(u"cheeseburger"_s)->buttonColor.empty());
    // One not there: nothing changes.
    CHECK_FALSE(pos.changeMenuItems({u"classic-burger"_s, u"no-such"_s}, {{u"family"_s, u"salads"_s}}));
    CHECK(pos.findItem(u"classic-burger"_s)->family == "burgers");
    CHECK_FALSE(pos.removeMenuItems({u"classic-burger"_s, u"no-such"_s}));
    CHECK(pos.findItem(u"classic-burger"_s));

    REQUIRE(pos.removeMenuItems(two));
    CHECK_FALSE(pos.findItem(u"classic-burger"_s));
    CHECK_FALSE(pos.findItem(u"cheeseburger"_s));
    CHECK(pos.menuUndoText() == u"Remove 2 items"_s);
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(u"classic-burger"_s));
    CHECK(pos.findItem(u"cheeseburger"_s));
}

TEST_CASE("Menu Builder: an item's section and what comes before it", "[menubuilder][sections]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"veggie-burger"_s}, {u"name"_s, u"Veggie Burger"_s},
                                  {u"section"_s, u"  Meatless  "_s}, {u"breakBefore"_s, u"row"_s}}));
    CHECK(pos.findItem(u"veggie-burger"_s)->section == "Meatless");
    CHECK(pos.findItem(u"veggie-burger"_s)->breakBefore == "row");
    CHECK(item(pos, u"Veggie Burger"_s)[u"section"_s] == u"Meatless"_s);
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, u"veggie-burger"_s}, {u"name"_s, u"Veggie Burger"_s}, {u"breakBefore"_s, u"page"_s}}));
    const core::MenuItem back = app::menuItemFromJson(app::toJson(*pos.findItem(u"veggie-burger"_s)));
    CHECK(back.section == "Meatless");
    CHECK(back.breakBefore == "row");
    // A copy: its section, not the space before.
    REQUIRE(pos.duplicateMenuItem(u"veggie-burger"_s));
    for (const core::MenuItem &m : pos.shared()->menu)
        if (m.name == "Veggie Burger 2") {
            CHECK(m.section == "Meatless");
            CHECK(m.breakBefore.empty());
        }
    REQUIRE(pos.saveCategory({{u"id"_s, u"burgers"_s}, {u"name"_s, u"Burgers"_s}, {u"shades"_s, true}}));
    CHECK(category(pos, u"burgers"_s)[u"shades"_s].toBool());
}

TEST_CASE("Colors…: every category's color at once, one Undo", "[menubuilder][themes]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QString before = category(pos, u"salads"_s)[u"color"_s].toString();
    REQUIRE(pos.setCategoryColors({{u"burgers"_s, u"#E53935"_s}, {u"salads"_s, u"#43a047"_s}}));
    CHECK(category(pos, u"burgers"_s)[u"color"_s] == u"#e53935"_s);
    CHECK(category(pos, u"salads"_s)[u"color"_s] == u"#43a047"_s);
    CHECK(pos.menuUndoText() == u"Category colors"_s);
    CHECK_FALSE(pos.setCategoryColors({{u"burgers"_s, u"red"_s}}));
    CHECK_FALSE(pos.setCategoryColors({{u"nope"_s, u"#000000"_s}}));
    REQUIRE(pos.undoMenuChange());
    CHECK(category(pos, u"salads"_s)[u"color"_s] == before);
}

TEST_CASE("Menu Builder: sizes in one step, asked when it's ordered", "[menubuilder][sizes]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // An item that asks for nothing yet.
    QString id;
    for (const core::MenuItem &m : pos.shared()->menu)
        if (!m.isModifier && m.modifierGroups.empty() && m.available && !m.byWeight && m.ticketCapacity == 0) {
            id = QString::fromStdString(m.id);
            break;
        }
    REQUIRE_FALSE(id.isEmpty());
    const QString name = QString::fromStdString(pos.findItem(id)->name);
    CHECK_FALSE(pos.setItemSizes(id, {QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, u"4"_s}}}));   // one isn't sizes
    CHECK_FALSE(pos.setItemSizes(id, {QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, u"4"_s}},
                                      QVariantMap{{u"name"_s, u"large"_s}, {u"price"_s, u"5"_s}}}));
    REQUIRE(pos.setItemSizes(id, {QVariantMap{{u"name"_s, u"Small"_s}, {u"price"_s, u"3.00"_s}},
                                  QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, u"4.50"_s}},
                                  QVariantMap{{u"name"_s, u""_s}, {u"price"_s, u""_s}}}));   // an empty row: left out
    const core::MenuItem *m = pos.findItem(id);
    CHECK(m->price.cents() == 300);
    REQUIRE(m->modifierGroups.size() == 1);
    CHECK(m->modifierGroups.front() == "size-" + m->id);
    const QVariantMap shown = item(pos, name);
    CHECK(shown[u"groups"_s].toStringList().isEmpty());                      // not one of the shared ones
    REQUIRE(shown[u"sizes"_s].toList().size() == 2);
    CHECK(shown[u"sizes"_s].toList()[1].toMap()[u"price"_s] == u"4.50"_s);
    for (const QVariant &g : pos.choiceGroups())
        if (g.toMap()[u"id"_s] == u"size-"_s + id)
            CHECK(g.toMap()[u"own"_s].toBool());
    // Its card saved with other choices: the size is still asked first.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"onIt"_s, u"ice"_s}}));
    CHECK(pos.findItem(id)->modifierGroups.front() == "size-" + id.toStdString());

    // Ordered: the size asked; Large is 4.50.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(id);
    pos.chooseOption(u"size-"_s + id, 1);
    pos.finishChoosing();
    REQUIRE(pos.lines().size() == 1);
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$4.50"_s);
    pos.releaseCheck();

    // Copied: its own sizes; removed: they go too.
    REQUIRE(pos.duplicateMenuItem(id));
    const core::MenuItem *copy = pos.findItem(name + u" 2"_s);
    REQUIRE(copy);
    CHECK(copy->modifierGroups.front() == "size-" + copy->id);
    CHECK(pos.shared()->settings.modifierGroup("size-" + copy->id));
    const std::string copyId = copy->id;
    REQUIRE(pos.deleteMenuItemCard(QString::fromStdString(copyId)));
    CHECK_FALSE(pos.shared()->settings.modifierGroup("size-" + copyId));
    // None again: one size.
    REQUIRE(pos.setItemSizes(id, {}));
    CHECK_FALSE(pos.shared()->settings.modifierGroup("size-" + id.toStdString()));
    CHECK(pos.findItem(id)->price.cents() == 300);
}

TEST_CASE("Prices set for later: put on the menu when they're due", "[menubuilder][scheduledprices]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    qint64 clock = QDateTime(QDate::currentDate(), QTime(15, 0)).toMSecsSinceEpoch();
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const qint64 classic = pos.findItem(u"classic-burger"_s)->price.cents();
    const qint64 monday = clock + 3 * 86'400'000;
    const QVariantList up{QVariantMap{{u"id"_s, u"classic-burger"_s}, {u"price"_s, u"20.00"_s}},
                          QVariantMap{{u"id"_s, u"cheeseburger"_s}, {u"price"_s, u"21.00"_s}}};
    CHECK_FALSE(pos.schedulePrices(up, clock - 1, u"Too late"_s));
    CHECK_FALSE(pos.schedulePrices({QVariantMap{{u"id"_s, u"nope"_s}, {u"price"_s, u"1"_s}}}, monday, u"x"_s));
    REQUIRE(pos.schedulePrices(up, monday, u"Burgers +10%"_s));
    REQUIRE(pos.priceChanges().size() == 1);
    CHECK(pos.priceChanges()[0].toMap()[u"label"_s] == u"Burgers +10%"_s);
    CHECK(pos.priceChanges()[0].toMap()[u"count"_s].toInt() == 2);
    // Kept with the store's settings.
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).priceChanges == pos.shared()->settings.priceChanges);

    pos.applyDuePriceChanges();                                    // not yet
    CHECK(pos.findItem(u"classic-burger"_s)->price.cents() == classic);
    // A second one, canceled.
    REQUIRE(pos.schedulePrices(up, monday + 1000, u"Oops"_s));
    REQUIRE(pos.cancelPriceChange(pos.priceChanges()[1].toMap()[u"id"_s].toLongLong()));
    CHECK(pos.priceChanges().size() == 1);
    // Cheeseburger taken off the menu meanwhile: the rest still change.
    REQUIRE(pos.deleteMenuItemCard(u"cheeseburger"_s));

    clock = monday + 5000;
    pos.logout();                                                  // nobody needs to be there
    pos.applyDuePriceChanges();
    CHECK(pos.findItem(u"classic-burger"_s)->price.cents() == 2000);
    CHECK(pos.priceChanges().isEmpty());
    CHECK(sink.savedMenuIds().contains(u"classic-burger"_s));
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.menuUndoText() == u"Burgers +10%"_s);                 // and taken back like any change
    // Only managers set them.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.schedulePrices(up, clock + 86'400'000, u"x"_s));
}

TEST_CASE("Choices priced by size: extra cheese costs more on a Large", "[menubuilder][sizeprices]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QString id;
    for (const core::MenuItem &m : pos.shared()->menu)
        if (!m.isModifier && m.modifierGroups.empty() && m.available && !m.byWeight && m.ticketCapacity == 0) {
            id = QString::fromStdString(m.id);
            break;
        }
    REQUIRE_FALSE(id.isEmpty());
    const QString name = QString::fromStdString(pos.findItem(id)->name);
    REQUIRE(pos.setItemSizes(id, {QVariantMap{{u"name"_s, u"Small"_s}, {u"price"_s, u"8.00"_s}},
                                  QVariantMap{{u"name"_s, u"Large"_s}, {u"price"_s, u"10.00"_s}}}));
    CHECK_FALSE(pos.saveChoiceGroup({{u"name"_s, u"Add"_s}, {u"min"_s, 0}, {u"max"_s, 0},
                                     {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"Cheese"_s}, {u"price"_s, u"1"_s},
                                                                             {u"sizePrices"_s, QVariantMap{{u"Large"_s, u"lots"_s}}}}}}}));
    REQUIRE(pos.saveChoiceGroup({{u"name"_s, u"Add"_s}, {u"min"_s, 0}, {u"max"_s, 0},
                                 {u"options"_s, QVariantList{QVariantMap{{u"name"_s, u"Cheese"_s}, {u"price"_s, u"1.00"_s},
                                                                         {u"sizePrices"_s, QVariantMap{{u"Large"_s, u"1.50"_s}, {u"Small"_s, u""_s}}}},
                                                             QVariantMap{{u"name"_s, u"Bacon"_s}, {u"price"_s, u"2.00"_s}}}}}));
    QString add;
    for (const QVariant &g : pos.choiceGroups())
        if (g.toMap()[u"name"_s] == u"Add"_s) {
            add = g.toMap()[u"id"_s].toString();
            CHECK(g.toMap()[u"options"_s].toList()[0].toMap()[u"sizePrices"_s].toMap() == QVariantMap{{u"large"_s, u"1.50"_s}});
        }
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, id}, {u"name"_s, name}, {u"groups"_s, QStringList{add}}}));
    // Kept with the store's settings.
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(back.modifierGroup(add.toStdString())->options[0].sizePrices.at("large").cents() == 150);

    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(id);
    const QString size = u"size-"_s + id;
    pos.chooseOption(add, 0);                                     // cheese before the size: $1.00
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$9.00"_s);
    pos.chooseOption(size, 1);                                    // Large: $10 + cheese $1.50
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$11.50"_s);
    pos.chooseOption(add, 1);                                     // bacon: the same on every size
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$13.50"_s);
    pos.chooseOption(size, 0);                                    // back to Small: cheese $1.00
    CHECK(pos.totals()[u"subtotal"_s].toString() == u"$11.00"_s);
    // The choices screen shows the price for the size chosen.
    pos.chooseOption(size, 1);
    for (const QVariant &g : pos.choosingInfo()[u"groups"_s].toList())
        if (g.toMap()[u"id"_s] == add)
            CHECK(g.toMap()[u"options"_s].toList()[0].toMap()[u"price"_s] == u"$1.50"_s);
    REQUIRE(pos.finishChoosing());
}

TEST_CASE("Barcodes: on an item's card, one item's each; scanned: an item, a coupon, a gift card", "[menubuilder][barcode]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // Scanned into the card's field: several, commas or spaces between.
    QVariantMap card{{u"id"_s, u"draft-beer"_s}, {u"name"_s, u"Draft Beer"_s}, {u"barcodes"_s, u"012345678905, 4006381333931"_s}};
    REQUIRE(pos.saveMenuItemCard(card));
    CHECK(pos.findItem(u"draft-beer"_s)->barcodes == std::vector<std::string>{"012345678905", "4006381333931"});
    CHECK(item(pos, u"Draft Beer"_s)[u"barcodes"_s] == u"012345678905, 4006381333931"_s);
    CHECK(pos.findItem(u"4006381333931"_s)->id == "draft-beer");
    // Another item can't have it; a barcode is letters and digits.
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, u"Cobb Salad"_s}, {u"barcodes"_s, u"012345678905"_s}}));
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, u"Cobb Salad"_s}, {u"barcodes"_s, u"12"_s}}));
    // Manager -> Menu has them too.
    int index = 0;
    const QVariantList records = pos.adminRecords(u"menu"_s);
    for (int i = 0; i < records.size(); ++i)
        if (records[i].toMap()[u"id"_s] == u"cobb"_s)
            index = i;
    QVariantMap r = records[index].toMap();
    r[u"barcodes"_s] = u"012345678905"_s;
    CHECK_FALSE(pos.adminSave(u"menu"_s, index, r));
    r[u"barcodes"_s] = u"BOX-77 BOX-78"_s;
    REQUIRE(pos.adminSave(u"menu"_s, index, r));
    CHECK(pos.findItem(u"cobb"_s)->barcodes == std::vector<std::string>{"BOX-77", "BOX-78"});
    CHECK(pos.adminRecords(u"menu"_s)[index].toMap()[u"barcodes"_s] == u"BOX-77, BOX-78"_s);
    // Kept with the menu, not copied by Duplicate.
    const core::MenuItem &beer = *pos.findItem(u"draft-beer"_s);
    CHECK(app::menuItemFromJson(app::toJson(beer)).barcodes == beer.barcodes);
    REQUIRE(pos.duplicateMenuItem(u"draft-beer"_s));
    CHECK(pos.findItem(u"Draft Beer 2"_s)->barcodes.empty());

    // Other codes: a coupon goes on the open check; a gift card is looked up.
    core::PosSettings::Promotion coupon;
    coupon.id = "five-off";
    coupon.name = "Five Off";
    coupon.code = "SAVE5";
    coupon.amount = Money::fromCents(500);
    pos.shared()->settings.promotions.push_back(coupon);
    pos.shared()->giftCards.push_back({"6006491234567890", Money::fromCents(2500), 0, {}});
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    pos.addItem(u"cobb"_s);
    CHECK(pos.scanCode(u"save5"_s) == u"coupon"_s);
    CHECK(pos.totals()[u"hasDiscount"_s] == true);
    CHECK(pos.scanCode(u"6006491234567890"_s) == u"giftCard"_s);
    CHECK(pos.giftCardInfo()[u"found"_s] == true);
    CHECK(pos.scanCode(u"99999999"_s).isEmpty());

    // A spreadsheet's Barcode (UPC) column.
    const auto read = app::readMenuCsv(u"Item,Price,Category,UPC\nChips,1.50,Snacks,028400090858\n"_s);
    REQUIRE(read.items.size() == 1);
    CHECK(read.items[0].barcode == u"028400090858"_s);
    QStringList said;
    QObject::connect(&pos, &PosService::notice, [&](const QString &m) { said << m; });
    const int imported = pos.importMenuRows({QVariantMap{{u"name"_s, u"Chips"_s}, {u"price"_s, 1.5}, {u"barcodes"_s, u"028400090858"_s}}},
                             u"burgers"_s, false);
    INFO(said.join(u" | "_s).toStdString());
    CHECK(imported == 1);
    REQUIRE(pos.findItem(u"028400090858"_s));
    CHECK(pos.findItem(u"028400090858"_s)->name == "Chips");
    // A spreadsheet that dropped its first 0; a scanner that adds one.
    REQUIRE(pos.importMenuRows({QVariantMap{{u"name"_s, u"Pretzels"_s}, {u"price"_s, 2.0}, {u"barcodes"_s, u"49000050103"_s}}},
                               u"burgers"_s, false) == 1);
    REQUIRE(pos.findItem(u"049000050103"_s));
    CHECK(pos.findItem(u"049000050103"_s)->name == "Pretzels");
    CHECK(pos.findItem(u"0028400090858"_s)->name == "Chips");
    CHECK_FALSE(pos.findItem(u"0000000"_s));
    // One another item has (but for its 0): left off, the rest imported.
    said.clear();
    CHECK(pos.importMenuRows({QVariantMap{{u"name"_s, u"Popcorn"_s}, {u"price"_s, 2.0}, {u"barcodes"_s, u"28400090858, 77777777"_s}}},
                             u"burgers"_s, false) == 1);
    CHECK(pos.findItem(u"Popcorn"_s)->barcodes == std::vector<std::string>{"77777777"});
    CHECK(said.join(u" "_s).contains(u"28400090858 (Chips's)"_s));
    CHECK_FALSE(pos.saveMenuItemCard({{u"id"_s, QString::fromStdString(pos.findItem(u"Popcorn"_s)->id)}, {u"name"_s, u"Popcorn"_s}, {u"barcodes"_s, u"0028400090858"_s}}));
}

TEST_CASE("Menu Builder: an item's own stock, counted down as it sells, sold out at none", "[menubuilder][stock]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"lemonade"_s}, {u"name"_s, u"Lemonade"_s}, {u"stockTrack"_s, true},
                                  {u"stockOnHand"_s, u"2"_s}, {u"stockLowAt"_s, u"1"_s}, {u"cost"_s, u"0.80"_s}}));
    const core::Ingredient *own = pos.shared()->ingredient(app::ownStockId("lemonade"));
    REQUIRE(own);
    CHECK(own->onHand == 2);
    CHECK(own->cost == Money::fromCents(80));
    const auto row = [&] {
        for (const QVariant &v : pos.menuItems())
            if (v.toMap()[u"id"_s] == u"lemonade"_s)
                return v.toMap();
        return QVariantMap{};
    };
    CHECK(row()[u"stockTrack"_s].toBool());
    CHECK(row()[u"stockOnHand"_s] == u"2"_s);
    CHECK(row()[u"cost"_s] == u"0.80"_s);

    // A shop's sale: rung up and paid, never sent.
    pos.shared()->settings.serviceMode = "retail";
    REQUIRE(pos.openDrawerSession());
    const auto sell = [&] {
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        pos.addItem(u"lemonade"_s);
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
    };
    sell();
    CHECK(pos.shared()->ingredient(app::ownStockId("lemonade"))->onHand == 1);
    CHECK(row()[u"stockLow"_s].toBool());
    sell();
    CHECK(pos.shared()->ingredient(app::ownStockId("lemonade"))->onHand == 0);
    CHECK_FALSE(pos.findItem(u"lemonade"_s)->available);   // none left: sold out by itself

    // Saving the card without touching the count keeps what was sold.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"lemonade"_s}, {u"name"_s, u"Lemonade"_s}, {u"price"_s, u"3.25"_s}}));
    CHECK(pos.shared()->ingredient(app::ownStockId("lemonade"))->onHand == 0);
    // Restocked on the card: back for sale.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"lemonade"_s}, {u"name"_s, u"Lemonade"_s}, {u"stockTrack"_s, true}, {u"stockOnHand"_s, u"12"_s}}));
    CHECK(pos.findItem(u"lemonade"_s)->available);
    // Not counted any more: no longer sold out for want of it.
    sell();
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"lemonade"_s}, {u"name"_s, u"Lemonade"_s}, {u"stockTrack"_s, false}}));
    CHECK(pos.findItem(u"lemonade"_s)->recipe.empty());
    CHECK_FALSE(row()[u"stockTrack"_s].toBool());
}

TEST_CASE("Menu import: stock and cost from a shop's sheet; the same item found by its barcode", "[menubuilder][import][stock]")
{
    // A sheet as Export writes it, with stock.
    const auto read = app::readMenuCsv(u"Name,Category,Price,Cost,Tax,PLU,Barcode,On Hand,Low At,Order Up To,Sold Out\n"
                                       u"Cobb Salad,Salads,12.50,4.10,food,203,0712345678911,30,5,40,\n"
                                       u"Beanie,Merch,15.00,6.00,merchandise,,0099887766554,12,3,,\n"_s);
    REQUIRE(read.items.size() == 2);
    CHECK(read.items[0].cost == u"4.10"_s);
    CHECK(read.items[0].onHand == u"30"_s);
    CHECK(read.items[0].lowAt == u"5"_s);
    CHECK(read.items[0].par == u"40"_s);
    CHECK(read.columns.contains(u"onHand"_s));

    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    // The Cobb on the menu has that barcode, under its old name.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, u"cobb"_s}, {u"name"_s, u"Cobb"_s}, {u"barcodes"_s, u"0712345678911"_s}}));
    const auto row = [](const app::ImportedItem &i) {
        return QVariantMap{{u"name"_s, i.name}, {u"price"_s, i.price}, {u"category"_s, i.category}, {u"barcodes"_s, i.barcode},
                           {u"number"_s, i.number}, {u"cost"_s, i.cost}, {u"stockOnHand"_s, i.onHand},
                           {u"stockLowAt"_s, i.lowAt}, {u"stockPar"_s, i.par}};
    };
    const QVariantList rows{row(read.items[0]), row(read.items[1])};
    // Without Update stock: the new item gets its stock, the Cobb doesn't.
    REQUIRE(pos.importMenuRows(rows, {}, false) > 0);
    CHECK_FALSE(pos.shared()->ingredient(app::ownStockId("cobb")));
    const auto beanie = std::ranges::find_if(pos.shared()->menu, [](const core::MenuItem &m) { return m.name == "Beanie"; });
    REQUIRE(beanie != pos.shared()->menu.end());
    const core::Ingredient *b = pos.shared()->ingredient(app::ownStockId(beanie->id));
    REQUIRE(b);
    CHECK(b->onHand == 12);
    CHECK(b->cost == Money::fromCents(600));
    CHECK(pos.findItem(u"cobb"_s)->name == "Cobb");   // found by its barcode, not added again
    CHECK(std::ranges::count_if(pos.shared()->menu, [](const core::MenuItem &m) { return m.name == "Cobb Salad"; }) == 0);
    // With Update stock: the Cobb's too.
    REQUIRE(pos.importMenuRows(rows, {}, false, {}, true) > 0);
    const core::Ingredient *c = pos.shared()->ingredient(app::ownStockId("cobb"));
    REQUIRE(c);
    CHECK(c->onHand == 30);
    CHECK(c->par == 40);
}
