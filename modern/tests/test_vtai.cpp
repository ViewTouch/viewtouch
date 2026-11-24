#include <catch2/catch_test_macros.hpp>

#include "app/menu_import.hh"
#include "app/menu_reader.hh"
#include "app/pos_demo.hh"
#include "app/pos_starter.hh"
#include "net/vtai_client.hh"
#include "app/pos_service.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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

namespace {

// Stand-ins for pdftotext, pdftoppm and tesseract (VTM_VTAI_TOOLS).
struct FakeTools {
    QTemporaryDir dir;
    QByteArray oldTools = qgetenv("VTM_VTAI_TOOLS");
    explicit FakeTools(bool scanned)
    {
        const auto script = [&](const QString &name, const QByteArray &body) {
            QFile f(dir.filePath(name));
            REQUIRE(f.open(QIODevice::WriteOnly));
            f.write("#!/bin/sh\n" + body);
            f.close();
            f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
        };
        script(u"pdftotext"_s, scanned ? QByteArray("printf ' \\f'\n") : QByteArray("printf 'TACOS\\nCarne Asada 3.50\\nAl Pastor 3.25\\nBirria 4.25\\n'\n"));
        // Its last argument: where the pages go ("…/page" -> page-1.png, page-2.png).
        script(u"pdftoppm"_s, "for a; do out=$a; done\nprintf x > \"$out-1.png\"; printf x > \"$out-2.png\"\n");
        script(u"tesseract"_s, "case \"$1\" in *page-1*) echo 'TACOS'; echo 'Carne Asada 3.50';; *page-2*) echo 'DRINKS'; echo 'Horchata 2.50';;"
                               " *) echo 'BURGERS'; echo 'Cheeseburger 12';; esac\n");
        qputenv("VTM_VTAI_TOOLS", dir.path().toLocal8Bit());
    }
    ~FakeTools() { qputenv("VTM_VTAI_TOOLS", oldTools); }
};

QString readFile(const QByteArray &data, QString *error = nullptr, QStringList *said = nullptr)
{
    app::MenuReader reader(data);
    bool done = false;
    QString text;
    reader.start([&](const QString &s) { if (said) *said << s; },
                 [&](const QString &t, const QString &e) { text = t; if (error) *error = e; done = true; });
    for (int i = 0; i < 300 && !done; ++i)
        QTest::qWait(10);
    REQUIRE(done);
    return text;
}

} // namespace

TEST_CASE("VT Ai: a menu's PDF or photo read into words", "[vtai]")
{
    const QByteArray png = QByteArray("\x89PNG\r\n\x1a\n", 8) + QByteArray(64, 'x');
    const QByteArray pdf = "%PDF-1.7\n" + QByteArray(64, 'x');
    CHECK(app::MenuReader::kindOf(pdf) == u"pdf"_s);
    CHECK(app::MenuReader::kindOf(png) == u"image"_s);
    CHECK(app::MenuReader::kindOf(QByteArray("\xff\xd8\xff\xe0", 4)) == u"image"_s);
    CHECK(app::MenuReader::kindOf("RIFF1234WEBPVP8 ") == u"image"_s);
    CHECK(app::MenuReader::kindOf("hello") == QString());

    QString error;
    QStringList said;
    {
        FakeTools tools(false);
        // A PDF made on a computer: its own words.
        CHECK(readFile(pdf, &error).contains(u"Birria 4.25"_s));
        CHECK(error.isEmpty());
        // A photo: read by tesseract.
        CHECK(readFile(png, &error, &said).contains(u"Cheeseburger 12"_s));
        CHECK(said.contains(u"Reading the photo…"_s));
        // Neither: said why, nothing run.
        CHECK(readFile("just some words", &error).isEmpty());
        CHECK(error.contains(u"isn't a PDF or a photo"_s));
        // Too big.
        readFile(pdf + QByteArray(app::MenuReader::MostBytes, 'x'), &error);
        CHECK(error.contains(u"over 20 MB"_s));
    }
    {
        FakeTools tools(true);
        // A scan: its pages as pictures, each read, in order.
        said.clear();
        const QString text = readFile(pdf, &error, &said);
        CHECK(error.isEmpty());
        CHECK(text.indexOf(u"Carne Asada 3.50"_s) >= 0);
        CHECK(text.indexOf(u"Horchata 2.50"_s) > text.indexOf(u"Carne Asada"_s));
        CHECK(said.contains(u"Reading page 2 of 2…"_s));
    }
    {
        // Not on this computer: said which, and where it comes from.
        const QByteArray oldPath = qgetenv("PATH");
        QTemporaryDir empty;
        qputenv("VTM_VTAI_TOOLS", empty.path().toLocal8Bit());
        qputenv("PATH", empty.path().toLocal8Bit());
        readFile(png, &error);
        qputenv("PATH", oldPath);
        qunsetenv("VTM_VTAI_TOOLS");
        CHECK(error.contains(u"tesseract isn't on this computer"_s));
    }
}

TEST_CASE("VT Ai: a photo of a menu from a phone, read on the store's computer", "[vtai]")
{
    FakeTools tools(false);
    app::PosService pos(test::seedPosData(), nullptr);
    QString told;
    pos.shared()->vtaiRunning = true;
    pos.shared()->vtaiCall = [&](const QJsonObject &request, std::function<void(const QJsonObject &, const QString &)> done) {
        told = request[u"messages"_s].toArray().last().toObject()[u"content"_s].toString();
        done(replyWith(u"C Burgers\nI Cheeseburger | 12\n"_s), {});
    };
    const QString photo = QString::fromLatin1((QByteArray("\xff\xd8\xff\xe0", 4) + QByteArray(64, 'x')).toBase64());
    // A manager's only.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.vtaiMenuFromFile(photo));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int before = pos.menuItems().size();
    REQUIRE(pos.vtaiMenuFromFile(photo));
    CHECK(pos.vtai()[u"busy"_s].toBool());
    for (int i = 0; i < 300 && pos.vtai()[u"busy"_s].toBool(); ++i)
        QTest::qWait(10);
    CHECK(told.contains(u"Cheeseburger 12"_s));
    const QVariantList items = pos.vtai()[u"result"_s].toMap()[u"items"_s].toList();
    REQUIRE(items.size() == 1);
    CHECK(items[0].toMap()[u"name"_s] == u"Cheeseburger"_s);
    CHECK(pos.menuItems().size() == before);

    // Not a menu's file: said, nothing asked.
    pos.vtaiClear();
    told.clear();
    REQUIRE(pos.vtaiMenuFromFile(QString::fromLatin1(QByteArray("plain words").toBase64())));
    for (int i = 0; i < 300 && pos.vtai()[u"busy"_s].toBool(); ++i)
        QTest::qWait(10);
    CHECK(pos.vtai()[u"error"_s].toString().contains(u"isn't a PDF or a photo"_s));
    CHECK(told.isEmpty());
}

namespace {

// A stand-in VT Ai that answers each question with the next of `answers`.
struct FakeVtAi {
    QList<QJsonObject> asked;
    QStringList answers;
    explicit FakeVtAi(app::PosService &pos)
    {
        pos.shared()->vtaiRunning = true;
        pos.shared()->vtaiCall = [this](const QJsonObject &request, std::function<void(const QJsonObject &, const QString &)> done) {
            asked << request;
            const QString a = answers.isEmpty() ? QString() : answers.takeFirst();
            QTimer::singleShot(0, [done, a] { done(replyWith(a), {}); });
        };
    }
};

void waitFor(app::PosService &pos)
{
    for (int i = 0; i < 500 && pos.vtai()[u"busy"_s].toBool(); ++i)
        QTest::qWait(10);
    REQUIRE_FALSE(pos.vtai()[u"busy"_s].toBool());
}

} // namespace

TEST_CASE("VT Ai: a sales question answered from a report it chose", "[vtai]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    FakeVtAi ai(pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    ai.answers = {u"R items week"_s, u"Cheeseburgers sold best this week."_s};
    REQUIRE(pos.vtaiAsk(u"What sold best this week?"_s));
    waitFor(pos);
    const QVariantMap v = pos.vtai();
    CHECK(v[u"answer"_s] == u"Cheeseburgers sold best this week."_s);
    CHECK(v[u"report"_s].toMap()[u"id"_s] == u"items"_s);
    CHECK(pos.rangeReport()[u"id"_s] == u"items"_s);   // the manager can open it
    REQUIRE(ai.asked.size() == 2);
    // The choice kept to the reports allowed; the answer given the report's rows.
    CHECK(ai.asked[0][u"grammar"_s].toString().contains(u"\"engineering\""_s));
    CHECK_FALSE(ai.asked[0][u"grammar"_s].toString().contains(u"\"labor\""_s));
    CHECK(ai.asked[1][u"messages"_s].toArray()[1].toObject()[u"content"_s].toString().startsWith(u"Report:"_s));

    // Not about the store: said what it does, no report made.
    ai.asked.clear();
    ai.answers = {u"N"_s};
    REQUIRE(pos.vtaiAsk(u"Write me a poem"_s));
    waitFor(pos);
    CHECK(pos.vtai()[u"answer"_s].toString().contains(u"sales"_s));
    CHECK(ai.asked.size() == 1);

    // Managers only.
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.vtaiAsk(u"What sold best?"_s));
}

TEST_CASE("VT Ai: menu fixes and descriptions proposed, a manager's ticks made as one step", "[vtai]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    FakeVtAi ai(pos);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const QVariantList categories = pos.menuCategories();
    REQUIRE(categories.size() > 1);
    const QString firstCategory = categories[0].toMap()[u"name"_s].toString();

    const QString firstCategoryId = categories[0].toMap()[u"id"_s].toString();
    // The starter menu's Water is free on purpose; a new item has no price yet.
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, QString()}, {u"name"_s, u"Mystery Plate"_s}, {u"price"_s, u"0"_s},
                                  {u"family"_s, firstCategoryId}}));
    const core::MenuItem *made = pos.findItem(u"Mystery Plate"_s);
    REQUIRE(made);
    const QString id = QString::fromStdString(made->id);
    const QString water = QString::fromStdString(pos.findItem(u"Water"_s)->id);
    ai.answers = {u"P 1 | 0 | water is free\nP 2 | 12.50 | like the other plates\nP 9 | 3 | not asked\nEND"_s};
    REQUIRE(pos.vtaiFixMenu());
    waitFor(pos);
    const QVariantList fixes = pos.vtai()[u"fixes"_s].toList();
    REQUIRE(fixes.size() == 1);   // free stays free; the one it wasn't asked about, left out
    CHECK(fixes[0].toMap()[u"item"_s] == id);
    CHECK(fixes[0].toMap()[u"value"_s] == u"12.50"_s);
    CHECK(ai.asked[0][u"messages"_s].toArray()[1].toObject()[u"content"_s].toString().contains(u"2. Mystery Plate needs a price"_s));
    CHECK(ai.asked[0][u"grammar"_s].toString().contains(firstCategory));
    // Nothing changed until a manager says; then price and category as one step.
    CHECK(pos.findItem(id)->price.cents() == 0);
    const QString otherCategory = categories[1].toMap()[u"id"_s].toString();
    REQUIRE(pos.vtaiApply({fixes[0], QVariantMap{{u"item"_s, id}, {u"field"_s, u"family"_s}, {u"value"_s, otherCategory}}}));
    CHECK(pos.findItem(id)->price.cents() == 1250);
    CHECK(QString::fromStdString(pos.findItem(id)->family) == otherCategory);
    CHECK(pos.findItem(water)->price.cents() == 0);
    // One Undo takes both back.
    REQUIRE(pos.undoMenuChange());
    CHECK(pos.findItem(id)->price.cents() == 0);
    CHECK(QString::fromStdString(pos.findItem(id)->family) == firstCategoryId);

    // Checked again when made: nothing else, nothing wrong.
    CHECK_FALSE(pos.vtaiApply({QVariantMap{{u"item"_s, id}, {u"field"_s, u"available"_s}, {u"value"_s, u"false"_s}}}));
    CHECK_FALSE(pos.vtaiApply({QVariantMap{{u"item"_s, id}, {u"field"_s, u"price"_s}, {u"value"_s, u"-4"_s}}}));
    CHECK_FALSE(pos.vtaiApply({QVariantMap{{u"item"_s, id}, {u"field"_s, u"family"_s}, {u"value"_s, u"nowhere"_s}}}));

    // Descriptions and kitchen names.
    ai.answers = {u"D 1 | A hearty plate of the chef's surprise, made to order.\nEND"_s};
    REQUIRE(pos.vtaiDescribe({id}, u"description"_s));
    waitFor(pos);
    QVariantList d = pos.vtai()[u"fixes"_s].toList();
    REQUIRE(d.size() == 1);
    REQUIRE(pos.vtaiApply(d));
    CHECK(pos.findItem(id)->description == "A hearty plate of the chef's surprise, made to order.");
    ai.answers = {u"K 1 | mys plt\nEND"_s};
    REQUIRE(pos.vtaiDescribe({id}, u"kitchenName"_s));
    waitFor(pos);
    d = pos.vtai()[u"fixes"_s].toList();
    REQUIRE(d.size() == 1);
    CHECK(d[0].toMap()[u"value"_s] == u"MYS PLT"_s);
    CHECK(ai.asked.last()[u"grammar"_s].toString().contains(u"{1,14}"_s));
    CHECK_FALSE(pos.vtaiDescribe({id}, u"price"_s));
    CHECK_FALSE(pos.vtaiDescribe({u"nope"_s}, u"description"_s));
}

// The real VT Ai (VTM_VTAI_URL, else 127.0.0.1:7720): what it answers, printed.
// Hidden: ./vtm_tests "[.vtai-live]" -s with a server running.
TEST_CASE("VT Ai live: questions, fixes and descriptions on a demo restaurant", "[.vtai-live]")
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    app::PosShared shared(*app::starterData({u"restaurant"_s, {}}), nullptr);
    app::PosService pos(&shared, u"Live"_s);
    REQUIRE(app::fillDemoData(pos, now).startsWith(u"Added"_s));
    net::VtAiClient client;
    shared.vtaiCall = [&](const QJsonObject &body, std::function<void(const QJsonObject &, const QString &)> done) {
        client.chat(body, std::move(done));
    };
    shared.vtaiCancel = [&] { client.cancel(); };
    for (int i = 0; i < 300 && !client.running(); ++i)
        QTest::qWait(20);
    REQUIRE(client.running());
    shared.vtaiRunning = true;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const auto wait = [&] {
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 60000 && pos.vtai()[u"busy"_s].toBool(); ++i)
            QTest::qWait(20);
        return t.elapsed() / 1000.0;
    };
    for (const QString &q : {u"What were today's sales?"_s, u"What sold best today?"_s, u"Which hour was busiest today?"_s,
                             u"Who sold the most today?"_s, u"Write me a poem about tacos"_s}) {
        REQUIRE(pos.vtaiAsk(q));
        const double took = wait();
        const QVariantMap v = pos.vtai();
        WARN(q.toStdString() + " -> [" + v[u"report"_s].toMap()[u"id"_s].toString().toStdString() + "] "
             + (v[u"answer"_s].toString() + v[u"error"_s].toString()).toStdString() + "  (" + std::to_string(int(took)) + " s)");
    }
    REQUIRE(pos.saveMenuItemCard({{u"id"_s, QString()}, {u"name"_s, u"Double Bacon Burger"_s}, {u"price"_s, u"0"_s},
                                  {u"family"_s, pos.menuCategories()[2].toMap()[u"id"_s]}}));
    REQUIRE(pos.vtaiFixMenu());
    double took = wait();
    for (const QVariant &f : pos.vtai()[u"fixes"_s].toList())
        WARN("fix: " + f.toMap()[u"name"_s].toString().toStdString() + " " + f.toMap()[u"field"_s].toString().toStdString() + " = "
             + f.toMap()[u"shown"_s].toString().toStdString() + " (" + f.toMap()[u"why"_s].toString().toStdString() + ")");
    WARN("fix took " + std::to_string(int(took)) + " s " + pos.vtai()[u"error"_s].toString().toStdString());
    QStringList ids;
    for (const QVariant &m : pos.menuItems())
        if (ids.size() < 5)
            ids << m.toMap()[u"id"_s].toString();
    for (const QString &what : {u"description"_s, u"kitchenName"_s}) {
        REQUIRE(pos.vtaiDescribe(ids, what));
        took = wait();
        for (const QVariant &f : pos.vtai()[u"fixes"_s].toList())
            WARN(what.toStdString() + ": " + f.toMap()[u"name"_s].toString().toStdString() + " -> "
                 + f.toMap()[u"shown"_s].toString().toStdString());
        WARN(what.toStdString() + " took " + std::to_string(int(took)) + " s " + pos.vtai()[u"error"_s].toString().toStdString());
    }
}
