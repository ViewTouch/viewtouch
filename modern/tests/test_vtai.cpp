#include <catch2/catch_test_macros.hpp>

#include "app/menu_import.hh"
#include "app/pos_service.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::StringLiterals;
using namespace vt;

// VT Ai, the small AI on the store's computer: what it proposes reaches Menu
// Builder's import preview checked and cleaned, only a manager asks, and it
// never changes the menu by itself. (A stand-in answers here; the real one,
// tools/vtai_bench.py.)

namespace {

// The server's reply around VT Ai's lines.
QJsonObject replyWith(const QString &lines)
{
    return {{u"choices"_s, QJsonArray{QJsonObject{{u"message"_s, QJsonObject{{u"content"_s, lines}}}}}}};
}

QJsonObject parse(const char *json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

TEST_CASE("VT Ai: a menu's answer checked and cleaned for the import preview", "[vtai]")
{
    const app::MenuImport read = app::readVtAiMenu({parse(R"({
      "groups": [{"name": "Toppings", "options": [{"name": "Mushrooms", "price": 1.5}, {"name": "mushrooms", "price": 9}, {"name": "", "price": 1}]},
                 {"name": "Temperature", "options": [{"name": "Rare", "price": 0}, {"name": "Well Done", "price": -3}]},
                 {"name": "Empty", "options": []}],
      "items": [
        {"name": "  Cheese   Pizza ", "price": 11, "category": "Pizza", "sizes": [{"name": "10\"", "price": 11}, {"name": "14\"", "price": 15}], "groups": ["Toppings", "Made Up"]},
        {"name": "Burger", "price": 12.499, "category": "Burgers", "groups": ["Temperature"]},
        {"name": "French Toast", "price": null, "category": "Breakfast"},
        {"name": "Burger", "price": 99, "category": "Burgers"},
        {"name": "", "price": 5},
        {"name": "Gold Plate", "price": 1000000},
        {"name": "Water", "price": 0, "sizes": [{"name": "Glass", "price": 0}]}
      ]})"),
                                                     parse(R"({"items": [{"name": "Iced Tea", "price": 3, "category": "Drinks"}],
                                                               "groups": [{"name": "Toppings", "options": [{"name": "Olives", "price": 1}]}]})"),
                                                     QJsonObject()});
    CHECK(read.from == u"VT Ai"_s);
    REQUIRE(read.items.size() == 6);   // the empty name gone, the second Burger once
    CHECK(read.items[0].name == u"Cheese Pizza"_s);
    CHECK(read.items[0].sizes.size() == 2);
    CHECK(read.items[0].price == 11);
    CHECK(read.items[0].groups == QStringList{u"Toppings"_s});   // one it named but didn't give: left out
    CHECK(read.items[1].price == 12.5);                            // to the cent
    CHECK(read.items[1].groups == QStringList{u"Temperature"_s});
    CHECK(read.items[2].price == 0);                               // none on the menu: none made up
    CHECK(read.items[3].name == u"Gold Plate"_s);
    CHECK(read.items[3].price == 0);                               // nonsense: none
    CHECK(read.items[4].sizes.isEmpty());                          // one size is just the price
    CHECK(read.items[5].name == u"Iced Tea"_s);                    // from the second part
    CHECK(read.problems.filter(u"No price for French Toast"_s).size() == 1);
    CHECK(read.problems.filter(u"No price for Gold Plate"_s).size() == 1);
    CHECK(read.problems.filter(u"Burger was there twice"_s).size() == 1);
    // Groups: no empty one, options once each, never below zero; parts merged.
    REQUIRE(read.groups.size() == 2);
    CHECK(read.groups[0].name == u"Toppings"_s);
    CHECK(read.groups[0].options.size() == 2);                     // Mushrooms (once), Olives
    CHECK(read.groups[0].min == 0);
    CHECK(read.groups[0].max == 0);                                // any number
    CHECK(read.groups[1].name == u"Temperature"_s);
    CHECK(read.groups[1].min == 1);                                // one required
    CHECK(read.groups[1].max == 1);
    CHECK(read.groups[1].options[1].price == 0);

    // As the preview takes it.
    const QVariantMap rows = app::importRowsOf(read);
    CHECK(rows[u"from"_s] == u"VT Ai"_s);
    CHECK(rows[u"items"_s].toList().size() == 6);
    CHECK(rows[u"items"_s].toList()[0].toMap()[u"sizes"_s].toList().size() == 2);

    CHECK(app::readVtAiMenu({}).problems.filter(u"found no items"_s).size() == 1);
}

TEST_CASE("VT Ai: a manager pastes a menu; it comes back for the preview, the menu unchanged", "[vtai]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    QList<QJsonObject> asked;
    QString giveError;
    pos.shared()->vtaiCall = [&](const QJsonObject &request, std::function<void(const QJsonObject &, const QString &)> done) {
        asked.append(request);
        if (!giveError.isEmpty()) {
            done({}, giveError);
            return;
        }
        done(replyWith(u"C Tacos\nI Carne Asada Taco | 3.50 |  | Salsa\nG Salsa | Roja=0; Verde=0\n"_s), {});
    };

    // Not running: nothing to ask.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.vtai()[u"available"_s].toBool());
    CHECK_FALSE(pos.vtaiMenuFromText(u"Taco 3.50"_s));
    pos.shared()->vtaiRunning = true;
    CHECK(pos.vtai()[u"available"_s].toBool());

    // Not a manager's: no.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.vtai()[u"available"_s].toBool());
    CHECK_FALSE(pos.vtaiMenuFromText(u"Taco 3.50"_s));
    CHECK(asked.isEmpty());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));

    CHECK_FALSE(pos.vtaiMenuFromText(u"   "_s));
    const auto before = pos.shared()->menu.size();
    REQUIRE(pos.vtaiMenuFromText(u"TACOS\nCarne Asada Taco .... 3.50\nSalsa roja or verde"_s));
    REQUIRE(asked.size() == 1);
    // What it was told: the instructions, an example, the menu; its lines kept to the grammar, ended by END.
    const QJsonArray messages = asked[0][u"messages"_s].toArray();
    REQUIRE(messages.size() == 4);
    CHECK(messages[0].toObject()[u"role"_s] == u"system"_s);
    CHECK(messages[2].toObject()[u"role"_s] == u"assistant"_s);
    CHECK(messages[3].toObject()[u"content"_s].toString().contains(u"Carne Asada Taco"_s));
    CHECK(asked[0][u"grammar"_s].toString().contains(u"END"_s));
    CHECK(asked[0][u"stop"_s].toArray().contains(u"END"_s));
    const QVariantMap result = pos.vtai()[u"result"_s].toMap();
    CHECK_FALSE(pos.vtai()[u"busy"_s].toBool());
    REQUIRE(result[u"items"_s].toList().size() == 1);
    CHECK(result[u"items"_s].toList()[0].toMap()[u"name"_s] == u"Carne Asada Taco"_s);
    CHECK(pos.shared()->menu.size() == before);   // only proposed

    // The manager imports it, as from a spreadsheet: one undo step.
    REQUIRE(pos.importMenuRows(result[u"items"_s].toList(), {}, false, result[u"groups"_s].toList()) == 1);
    CHECK(pos.shared()->menu.size() == before + 1);
    CHECK(pos.findItem(u"carne-asada-taco"_s));
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.shared()->menu.size() == before);

    // It didn't answer: said, nothing changed.
    giveError = u"VT Ai didn't answer: connection refused"_s;
    REQUIRE(pos.vtaiMenuFromText(u"Taco 3.50"_s));
    CHECK(pos.vtai()[u"error"_s].toString().contains(u"connection refused"_s));
    CHECK(pos.vtai()[u"result"_s].toMap().isEmpty());
    REQUIRE(pos.vtaiClear());
    CHECK(pos.vtai()[u"error"_s].toString().isEmpty());
}

TEST_CASE("VT Ai: a long menu goes in parts, each told the heading it goes on from", "[vtai]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    pos.shared()->vtaiRunning = true;
    QList<QJsonObject> asked;
    QList<std::function<void(const QJsonObject &, const QString &)>> waiting;
    pos.shared()->vtaiCall = [&](const QJsonObject &request, std::function<void(const QJsonObject &, const QString &)> done) {
        asked.append(request);
        waiting.append(std::move(done));
    };
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QString menu = u"TACOS\n"_s;
    for (int i = 0; i < 80; ++i)
        menu += u"Taco number %1 .......... 3.%2\n\n"_s.arg(i).arg(i % 10);
    REQUIRE(pos.vtaiMenuFromText(menu));
    CHECK(pos.vtai()[u"busy"_s].toBool());
    REQUIRE(asked.size() == 1);   // one at a time
    const int steps = pos.vtai()[u"steps"_s].toInt();
    CHECK(steps >= 2);
    waiting.takeFirst()(replyWith(u"C Tacos\nI Taco Number 0 | 3\n"_s), {});
    REQUIRE(asked.size() == 2);
    CHECK(asked[1][u"messages"_s].toArray()[3].toObject()[u"content"_s].toString().contains(u"\"Tacos\""_s));
    // A part it couldn't read: said, the rest still comes.
    waiting.takeFirst()(replyWith(u"Sorry, I can't read that."_s), {});
    // Given up halfway: nothing comes back after.
    if (pos.vtai()[u"busy"_s].toBool()) {
        REQUIRE(pos.vtaiCancel());
        CHECK_FALSE(pos.vtai()[u"busy"_s].toBool());
        for (auto &done : waiting)
            done(replyWith(u"C X\nI Late | 1\n"_s), {});
        CHECK(pos.vtai()[u"result"_s].toMap().isEmpty());
    } else {
        CHECK(pos.vtai()[u"result"_s].toMap()[u"problems"_s].toStringList().filter(u"Part 2"_s).size() == 1);
    }
}

TEST_CASE("VT Ai: its lines read into items, sizes and choices", "[vtai]")
{
    const QJsonObject a = app::parseVtAiLines(u"C Pizza\n"
                                              "I Cheese Pizza | 11 | 10\"=11; 14\"=15 | Toppings; Crust\n"
                                              "I French Toast | ? |  | \n"
                                              "I Iced Tea | $3.00\n"
                                              "garbage line\n"
                                              "X Not a kind\n"
                                              "I  | 5\n"
                                              "C Drinks\n"
                                              "I Soda | 2.95 | Small=x; Large=3.50\n"
                                              "G Toppings | Mushrooms=1.50; Extra Cheese=2; Basil\n"
                                              "G Crust | Thin=0; Deep=2.00\n"
                                              "END"_s);
    const QJsonArray items = a[u"items"_s].toArray();
    REQUIRE(items.size() == 4);
    CHECK(items[0][u"name"_s] == u"Cheese Pizza"_s);
    CHECK(items[0][u"category"_s] == u"Pizza"_s);
    CHECK(items[0][u"price"_s].toDouble() == 11);
    CHECK(items[0][u"sizes"_s].toArray().size() == 2);
    CHECK(items[0][u"groups"_s].toArray().size() == 2);
    CHECK(items[1][u"price"_s].isNull());          // "?": none
    CHECK(items[2][u"price"_s].toDouble() == 3);
    CHECK(items[3][u"category"_s] == u"Drinks"_s);
    CHECK(items[3][u"sizes"_s].toArray().size() == 1);   // "Small=x": no price, left out
    const QJsonArray groups = a[u"groups"_s].toArray();
    REQUIRE(groups.size() == 2);
    CHECK(groups[0][u"options"_s].toArray().size() == 3);
    CHECK(groups[0][u"options"_s].toArray()[2][u"price"_s].toDouble() == 0);   // "Basil": free
    // Through the checks to the preview.
    const app::MenuImport read = app::readVtAiMenu({a});
    REQUIRE(read.items.size() == 4);
    CHECK(read.items[0].groups == QStringList({u"Toppings"_s, u"Crust"_s}));
    CHECK(read.items[3].sizes.isEmpty());          // one size: just the price
    CHECK(read.groups[1].min == 1);                // a crust: one required
}
