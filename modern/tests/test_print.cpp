#include <catch2/catch_test_macros.hpp>
#include <QDir>
#include <QImage>
#include <QBuffer>
#include "print/raster.hh"

#include "pos_fixture.hh"
#include "print/document.hh"
#include "print/spooler.hh"
#include "print/ticket_printer.hh"
#include "print/tickets.hh"
#include "qt_catch.hh"

#include <QElapsedTimer>
#include <atomic>
#include <QFile>
#include <QLocale>
#include <QDateTime>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;
using namespace vt::print;

namespace {

bool contains(const std::string &hay, const std::string &needle) { return hay.find(needle) != std::string::npos; }

TicketContext ctx(const core::PosSettings &s)
{
    return {s, [](std::int64_t) { return std::string("Sep 29, 2026 10:31 AM"); },
            [](std::int64_t) { return std::string("10:31 AM"); }, 0};
}

core::Check burgerCheck(const core::PosSettings &s, const std::vector<core::MenuItem> &menu)
{
    auto item = [&](const char *id) {
        for (const auto &m : menu)
            if (m.id == id) return m;
        FAIL("no item " << id);
        return core::MenuItem{};
    };
    core::Check c;
    c.id = 42;
    c.label = "T3";
    c.guests = 2;
    c.serverName = "Sam";
    const auto &b = c.addItem(item("classic-burger"));
    c.addModifier(b.id, item("medium-rare"));
    c.addModifier(b.id, item("onion-rings"));
    c.addItem(item("draft-beer"));
    c.addComment("No pickles");
    c.addPayment(*s.tender("cash"), Money::fromCents(3000));
    return c;
}

QString readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

} // namespace

TEST_CASE("Text rendering: columns, wrapping, centering", "[print]")
{
    Document d;
    d.center("Café");
    d.columns("Classic Burger", "$11.50");
    d.columns("A very long item name that has to wrap onto the next line", "$1.00");
    d.rule();
    const std::string out = renderText(d, 24);
    CHECK(contains(out, "          Café\n"));                 // (24 - 4) / 2 spaces
    CHECK(contains(out, "Classic Burger    $11.50\n"));      // exactly 24 wide
    CHECK(contains(out, std::string(24, '-')));
    for (const char *line : {"A very long item", "$1.00"})
        CHECK(contains(out, line));
    std::istringstream in(out);
    for (std::string row; std::getline(in, row);)
        CHECK(displayWidth(row) <= 24);
}

TEST_CASE("ESC/POS: init, emphasis, cut, drawer kick, letters", "[print]")
{
    Document d;
    d.text("Crème brûlée", Document::Align::Left, true);
    d.kickDrawer = true;
    // Accented letters: the PC858 character set.
    const std::string bytes = renderEscPos(d, 42);
    CHECK(bytes.rfind("\x1b@\x1bt\x13", 0) == 0);              // ESC @, then ESC t 19
    CHECK(contains(bytes, std::string("\x1b" "E" "\x01", 3)));  // bold on
    CHECK(contains(bytes, "Cr\x8A" "me br\x96" "l\x82" "e"));     // è û é in PC858
    CHECK(contains(bytes, std::string("\x1bp\x00\x19\xfa", 5)));
    CHECK(contains(bytes, std::string("\x1dV\x42\x00", 4)));   // partial cut
    // Plain letters: no character set, ASCII only.
    const std::string plain = renderEscPos(d, 42, false);
    CHECK_FALSE(contains(plain, "\x1bt"));
    CHECK(contains(plain, "Creme brulee"));
    CHECK(std::ranges::none_of(plain, [](char c) { return static_cast<unsigned char>(c) >= 0x80 && c != '\xfa'; }));
}

TEST_CASE("Receipt and kitchen ticket contents", "[print]")
{
    const auto seed = test::seedPosData();
    const core::Check c = burgerCheck(seed.settings, seed.menu);

    const std::string r = renderText(receipt(c, ctx(seed.settings)), 42);
    CHECK(contains(r, "ViewTouch Café"));
    CHECK(contains(r, "123 Main Street"));
    CHECK(contains(r, "Check #42"));
    CHECK(contains(r, "Classic Burger"));
    CHECK(contains(r, "Onion Rings"));
    CHECK_FALSE(contains(r, "No pickles"));    // kitchen notes stay off the receipt
    // 11.50 + 1.00 food, 6.00 beer: tax 1.03 + 0.60 = 1.63; total 20.13; change 9.87
    CHECK(contains(r, "$20.13"));
    CHECK(contains(r, "Change"));
    CHECK(contains(r, "$9.87"));
    CHECK(contains(r, "Thank you for visiting!"));

    const std::string k = renderText(kitchenTicket(c, {c.lines[0], c.lines[2]}, "Kitchen", false, ctx(seed.settings)), 42);
    CHECK(contains(k, "KITCHEN"));
    CHECK(contains(k, "T3"));
    CHECK(contains(k, "1 Classic Burger"));
    CHECK(contains(k, "> MR"));
    CHECK(contains(k, "** No pickles **"));
    CHECK_FALSE(contains(k, "$"));             // no prices in the kitchen
    CHECK(contains(renderText(kitchenTicket(c, {c.lines[0]}, "Kitchen", true, ctx(seed.settings)), 42), "*** VOID ***"));
}

TEST_CASE("Report ticket lays out columns", "[print]")
{
    const auto seed = test::seedPosData();
    core::Report rep;
    rep.title = "Item Sales";
    rep.section("Burgers");
    rep.line({"Classic Burger", "3", "$34.50"});
    rep.total({"Burgers total", "3", "$34.50"});
    const std::string out = renderText(reportTicket(rep, ctx(seed.settings)), 42);
    CHECK(contains(out, "Item Sales"));
    // 42 wide: name, padding, then each value right-aligned in 9 columns.
    CHECK(contains(out, "Classic Burger" + std::string(18, ' ') + "3   $34.50\n"));
}

TEST_CASE("TicketPrinter routes kitchen lines by printer and writes files", "[print]")
{
    QTemporaryDir dir;
    auto seed = test::seedPosData();
    PrintSpooler spooler;
    TicketPrinter printer(spooler, dir.path());
    const core::Check c = burgerCheck(seed.settings, seed.menu);

    printer.printKitchen(seed.settings, c, c.lines, false);
    printer.printReceipt(seed.settings, c, "receipt");
    printer.openDrawer(seed.settings, "receipt");
    REQUIRE(spooler.waitIdle(5000));

    const QString kitchen = readFile(dir.filePath(u"kitchen.txt"_s));
    const QString bar = readFile(dir.filePath(u"bar.txt"_s));
    const QString receiptText = readFile(dir.filePath(u"receipt.txt"_s));
    CHECK(kitchen.contains(u"Classic Burger"_s));
    CHECK(kitchen.contains(u"No pickles"_s));        // comments follow the kitchen
    CHECK_FALSE(kitchen.contains(u"Draft Beer"_s));
    CHECK(bar.contains(u"Draft Beer"_s));
    CHECK(receiptText.contains(u"TOTAL"_s));
    CHECK(receiptText.contains(u"[drawer opened]"_s));   // receipt printer has the drawer
}

TEST_CASE("TicketPrinter sends labels in the label printer's language, or as slips", "[print][labels]")
{
    QTemporaryDir dir;
    auto seed = test::seedPosData();
    core::PrinterConfig zpl;
    zpl.id = "labels";
    zpl.name = "Labels";
    zpl.type = "file";
    zpl.path = "labels.zpl";
    zpl.format = "zpl";
    seed.settings.printers.push_back(zpl);
    PrintSpooler spooler;
    TicketPrinter printer(spooler, dir.path());
    printer.printLabels(seed.settings, {{"Pico de gallo", {"Made Mon 9:00 AM"}}, {"Salsa", {"Made Mon 9:00 AM"}}}, "labels");
    printer.printLabels(seed.settings, {{"Rice", {"Made Mon 9:00 AM"}}}, "receipt");
    REQUIRE(spooler.waitIdle(5000));
    const QString labels = readFile(dir.filePath(u"labels.zpl"_s));
    CHECK(labels.count(u"^XA"_s) == 2);
    CHECK(labels.contains(u"^FDSalsa^FS"_s));
    CHECK(readFile(dir.filePath(u"receipt.txt"_s)).contains(u"Rice"_s));
}

TEST_CASE("A French store prints in French; a Chinese store's tickets are in English", "[print][i18n]")
{
    for (const char *lang : {"fr", "zh"}) {
        QTemporaryDir dir;
        auto seed = test::seedPosData();
        seed.settings.language = lang;
        PrintSpooler spooler;
        TicketPrinter printer(spooler, dir.path());
        core::Check c = burgerCheck(seed.settings, seed.menu);
        c.type = core::CheckType::Takeout;
        printer.printKitchen(seed.settings, c, {c.lines[0]}, true);
        REQUIRE(spooler.waitIdle(5000));
        const QString kitchen = readFile(dir.filePath(u"kitchen.txt"_s));
        INFO(lang);
        if (std::string(lang) == "fr")
            CHECK(kitchen.contains(u"*** ANNULÉ ***"_s));
        else
            CHECK(kitchen.contains(u"*** VOID ***"_s));
    }
}

TEST_CASE("A Spanish store prints in Spanish: receipts, kitchen tickets, reports", "[print][i18n]")
{
    QTemporaryDir dir;
    auto seed = test::seedPosData();
    seed.settings.language = "es";
    PrintSpooler spooler;
    TicketPrinter printer(spooler, dir.path());
    core::Check c = burgerCheck(seed.settings, seed.menu);
    c.type = core::CheckType::Takeout;
    c.rush = true;
    c.customer.note = "puerta de atrás";
    c.lines[0].seat = 2;

    printer.printKitchen(seed.settings, c, c.lines, false);
    printer.printKitchen(seed.settings, c, {c.lines[0]}, true);
    printer.printReceipt(seed.settings, c, "receipt");
    core::Report rep;
    rep.title = "Sales Summary";
    rep.line({"Net sales", "$34.50"});
    rep.line({"Voided items (3)", "$4.00"});
    printer.printReport(seed.settings, rep, "receipt");
    REQUIRE(spooler.waitIdle(5000));

    const QString kitchen = readFile(dir.filePath(u"kitchen.txt"_s));
    const QString receiptText = readFile(dir.filePath(u"receipt.txt"_s));
    CHECK(kitchen.contains(u"*** URGENTE ***"_s));
    CHECK(kitchen.contains(u"PARA LLEVAR"_s));
    CHECK(kitchen.contains(u"NOTA: puerta de atrás"_s));
    CHECK(kitchen.contains(u"A2 1 Classic Burger"_s));       // the seat: asiento
    CHECK(kitchen.contains(u"*** ANULADO ***"_s));
    CHECK_FALSE(kitchen.contains(u"RUSH"_s));
    CHECK(receiptText.contains(u"Cuenta #42"_s));
    CHECK(receiptText.contains(u"Mesero: Sam"_s));
    CHECK(receiptText.contains(u"Impuesto de comida"_s));
    CHECK(receiptText.contains(u"Cambio"_s));
    CHECK_FALSE(receiptText.contains(u"Subtotal "_s + u"Food"_s));
    CHECK(receiptText.contains(u"Resumen de ventas"_s));
    CHECK(receiptText.contains(u"Ventas netas"_s));
    CHECK(receiptText.contains(u"(3)"_s));
    CHECK_FALSE(receiptText.contains(u"Voided items"_s));
    // The store's own words (items, the footer) print as typed.
    CHECK(receiptText.contains(u"Classic Burger"_s));
}

TEST_CASE("Network printer receives ESC/POS over TCP", "[print]")
{
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    QByteArray received;
    QObject::connect(&server, &QTcpServer::newConnection, [&] {
        QTcpSocket *s = server.nextPendingConnection();
        QObject::connect(s, &QTcpSocket::readyRead, [s, &received] { received += s->readAll(); });
    });

    core::PrinterConfig p;
    p.id = "kitchen";
    p.name = "Line";
    p.type = "network";
    p.host = "127.0.0.1";
    p.port = server.serverPort();
    PrintSpooler spooler;
    QSignalSpy printed(&spooler, &PrintSpooler::jobPrinted);
    spooler.submit(p, QByteArray::fromStdString(renderEscPos(Document().text("Hello kitchen"), 42)), u"test"_s);
    REQUIRE(spooler.waitIdle(5000));
    QElapsedTimer t;
    t.start();
    while (!received.contains("Hello kitchen") && t.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(received.startsWith("\x1b@"));
    CHECK(received.contains("Hello kitchen"));
    CHECK(printed.size() == 1);
}

TEST_CASE("A dead printer never blocks the caller and is reported", "[print]")
{
    // A port nothing listens on: connection refused, retried, then reported.
    QTcpServer probe;
    REQUIRE(probe.listen(QHostAddress::LocalHost));
    const quint16 port = probe.serverPort();
    probe.close();

    core::PrinterConfig p;
    p.id = "kitchen";
    p.name = "Dead";
    p.type = "network";
    p.host = "127.0.0.1";
    p.port = port;
    PrintSpooler spooler;
    spooler.setTimings(300, 50);
    QSignalSpy failed(&spooler, &PrintSpooler::jobFailed);

    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 20; ++i)
        spooler.submit(p, "ticket", u"ticket %1"_s.arg(i));
    CHECK(t.elapsed() < 100);   // queuing is instant

    // A file printer keeps working meanwhile.
    QTemporaryDir dir;
    core::PrinterConfig f;
    f.id = "receipt";
    f.name = "File";
    f.type = "file";
    f.path = dir.filePath(u"out.txt"_s).toStdString();
    spooler.submit(f, "receipt\n", u"receipt"_s);

    REQUIRE(spooler.waitIdle(20000));
    CHECK(failed.size() == 20);
    CHECK(failed.first()[0].toString() == u"Dead"_s);
    CHECK(readFile(dir.filePath(u"out.txt"_s)) == u"receipt\n"_s);
}

TEST_CASE("Logo: a picture becomes black-and-white dots, fitted to the paper", "[print][logo]")
{
    // Left half black, right half white, the bottom row transparent.
    QImage img(100, 50, QImage::Format_ARGB32);
    img.fill(Qt::white);
    for (int y = 0; y < 50; ++y)
        for (int x = 0; x < 50; ++x)
            img.setPixel(x, y, qRgb(0, 0, 0));
    for (int x = 0; x < 100; ++x)
        img.setPixel(x, 49, qRgba(0, 0, 0, 0));
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");

    const auto r = print::rasterize(png, 400, 200);
    REQUIRE(r);
    CHECK(r->width == 200);                       // at most twice its size
    CHECK(r->height == 100);
    CHECK(r->bits.size() == std::size_t(r->rowBytes() * r->height));
    CHECK(r->dot(10, 10));                        // black stays black
    CHECK_FALSE(r->dot(190, 10));                 // white stays paper
    CHECK_FALSE(r->dot(150, 99));                 // transparent is paper
    const auto small = print::rasterize(png, 60, 200);
    REQUIRE(small);
    CHECK(small->width == 60);                    // fitted
    CHECK(small->height == 30);
    CHECK_FALSE(print::rasterize("not a picture", 100, 100));
}

TEST_CASE("Logo: printed at the top of receipts on ESC/POS printers, as GS v 0", "[print][logo]")
{
    QImage img(64, 32, QImage::Format_RGB32);
    img.fill(Qt::black);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");

    // Bytes on paper: GS v 0, then width in bytes and height in rows (little-endian).
    print::Document d;
    d.image(print::rasterize(png, 64, 32));
    d.center("Cafe");
    d.cut = false;
    const std::string bytes = print::renderEscPos(d, 42);
    const std::string head = std::string("\x1dv0\0", 4) + char(8) + char(0) + char(32) + char(0);
    REQUIRE(bytes.find(head) != std::string::npos);
    CHECK(bytes.find(std::string(8 * 32, '\xff')) != std::string::npos);   // all black
    CHECK(print::renderText(d, 42).find("Cafe") != std::string::npos);      // plain text: no picture

    QTemporaryDir dir;
    auto seed = test::seedPosData();
    seed.settings.displayLogo = "store:logo.png";
    seed.settings.receiptLogo = true;
    for (core::PrinterConfig &p : seed.settings.printers)
        if (p.id == "receipt")
            p.format = "escpos";
    PrintSpooler spooler;
    TicketPrinter printer(spooler, dir.path());
    printer.setImageSource([&](const QString &ref) { return ref == u"store:logo.png"_s ? png : QByteArray(); });
    const core::Check c = burgerCheck(seed.settings, seed.menu);
    printer.printReceipt(seed.settings, c, "receipt");
    REQUIRE(spooler.waitIdle(5000));
    QFile out(dir.filePath(u"receipt.txt"_s));
    REQUIRE(out.open(QIODevice::ReadOnly));
    const QByteArray printed = out.readAll();
    CHECK(printed.indexOf(QByteArray("\x1dv0\0", 4)) >= 0);
    CHECK(printed.indexOf(QByteArray("\x1dv0\0", 4)) < printed.indexOf("TOTAL"));   // at the top

    // Off: no picture.
    seed.settings.receiptLogo = false;
    out.close();
    QFile::remove(dir.filePath(u"receipt.txt"_s));
    printer.printReceipt(seed.settings, c, "receipt");
    REQUIRE(spooler.waitIdle(5000));
    REQUIRE(out.open(QIODevice::ReadOnly));
    CHECK(out.readAll().indexOf(QByteArray("\x1dv0", 3)) < 0);
}

TEST_CASE("A printer's test page: logo, text, the width ruler, a cut, the drawer", "[print][testpage]")
{
    QTemporaryDir dir;
    auto seed = test::seedPosData();
    seed.settings.displayLogo = "store:logo.png";
    for (core::PrinterConfig &p : seed.settings.printers)
        if (p.id == "receipt") {
            p.format = "escpos";
            p.drawerKick = true;
            p.cutter = true;
        }
    QImage img(64, 32, QImage::Format_RGB32);
    img.fill(Qt::black);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    PrintSpooler spooler;
    TicketPrinter printer(spooler, dir.path());
    printer.setImageSource([&](const QString &ref) { return ref == u"store:logo.png"_s ? png : QByteArray(); });
    CHECK_FALSE(printer.printTestPage(seed.settings, "no-such-printer", false));
    REQUIRE(printer.printTestPage(seed.settings, "receipt", true));
    REQUIRE(spooler.waitIdle(5000));
    QFile out(dir.filePath(u"receipt.txt"_s));
    REQUIRE(out.open(QIODevice::ReadOnly));
    const QByteArray printed = out.readAll();
    CHECK(printed.contains("Printer test"));
    CHECK(printed.contains("characters per line"));
    CHECK(printed.indexOf(QByteArray("\x1dv0\0", 4)) >= 0);       // the logo
    CHECK(printed.contains(QByteArray("\x1bp\0", 3)));             // drawer kick
    CHECK(printed.contains(QByteArray("\x1dV\x42", 3)));           // cut
}

// A real printer on the network (hidden; run with the printer's address:
// VTM_PRINTER_HOST=192.168.1.101 vtm_tests "[printerlive]"). Prints the
// Test Print page: logo, text sizes, a sample item and total, the ruler.
TEST_CASE("A network printer, live: the Test Print page", "[.][printerlive]")
{
    const QString host = qEnvironmentVariable("VTM_PRINTER_HOST");
    if (host.isEmpty())
        SKIP("Set VTM_PRINTER_HOST to the printer's address.");
    core::PosSettings settings;
    settings.storeName = "ViewTouch";
    core::PrinterConfig p;
    p.id = "receipt";
    p.name = "Receipt";
    p.type = "network";
    p.host = host.toStdString();
    p.port = qEnvironmentVariableIntValue("VTM_PRINTER_PORT") ? qEnvironmentVariableIntValue("VTM_PRINTER_PORT") : 9100;
    p.width = qEnvironmentVariableIntValue("VTM_PRINTER_WIDTH") ? qEnvironmentVariableIntValue("VTM_PRINTER_WIDTH") : 42;
    settings.printers = {p};
    print::PrintSpooler spooler;
    print::TicketPrinter printer(spooler, QDir::tempPath());
    QString result;
    QObject::connect(&spooler, &print::PrintSpooler::jobPrinted, [&](const QString &, const QString &) { result = u"printed"_s; });
    QObject::connect(&spooler, &print::PrintSpooler::jobFailed,
                     [&](const QString &, const QString &, const QString &error) { result = u"failed: "_s + error; });
    REQUIRE(printer.printTestPage(settings, "receipt", false));
    for (int i = 0; i < 300 && result.isEmpty(); ++i)
        QTest::qWait(50);
    INFO(result.toStdString());
    CHECK(result == u"printed"_s);
}

namespace {

// The text rows of ESC/POS bytes: commands taken out, one entry a line.
std::vector<std::string> escPosRows(const std::string &bytes)
{
    std::vector<std::string> rows;
    std::string row;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];
        if (c == '\x1b' || c == '\x1d') {
            const char cmd = i + 1 < bytes.size() ? bytes[i + 1] : 0;
            if (cmd == '@') { i += 1; continue; }
            if (c == '\x1d' && cmd == 'v') {   // a picture: GS v 0 m xL xH yL yH data
                const std::size_t w = std::size_t((unsigned char)bytes[i + 4] | (unsigned char)bytes[i + 5] << 8);
                const std::size_t h = std::size_t((unsigned char)bytes[i + 6] | (unsigned char)bytes[i + 7] << 8);
                i += 7 + w * h;
                continue;
            }
            if (cmd == 'p') { i += 4; continue; }
            if (cmd == 'V') { i += 3; continue; }
            i += 2;   // ESC/GS x n
            continue;
        }
        if (c == '\n') {
            rows.push_back(row);
            row.clear();
        } else {
            row += c;
        }
    }
    return rows;
}

core::Check awkwardCheck()
{
    core::Check c;
    c.id = 3465;
    c.label = "Patio 12 — the long table by the fountain";
    c.serverName = "Maria-José Fernández de la Peña";
    c.guests = 12;
    c.customer.name = "Ñoño “The Regular” O’Brien";
    c.customer.phone = "(555) 123-4567";
    c.customer.address = "1234 Avenida de los Insurgentes Sur\nApt 5B\tGate code #4321\x1b@";
    c.customer.note = "Leave at the door 🍕🚪 — ring twice…";
    core::OrderLine l;
    l.id = 1;
    l.itemId = "burger";
    l.name = "Super Deluxe Triple Bacon Cheeseburger with Jalapeños and Extra Everything";
    l.unitPrice = Money::fromCents(123456);
    l.quantity = 12;
    core::Modifier free;
    free.name = "No onion";
    core::Modifier extra;
    extra.name = "Extra bacon";
    extra.unitPrice = Money::fromCents(250);
    core::Modifier temp;
    temp.name = "Medium rare";
    l.modifiers = {free, extra, temp};
    c.lines.push_back(l);
    core::OrderLine crepe;
    crepe.id = 2;
    crepe.itemId = "creme";
    crepe.name = "Crème brûlée ½ · café";
    crepe.unitPrice = Money::fromCents(899);
    crepe.quantity = 1;
    c.lines.push_back(crepe);
    core::Payment pay;
    pay.id = 1;
    pay.kind = core::TenderKind::Card;
    pay.tenderName = "Credit Card";
    pay.amount = Money::fromCents(1600000);
    pay.tip = Money::fromCents(250000);
    pay.cardBrand = "mastercard";
    pay.last4 = "4242";
    pay.processor = "stripe";
    pay.reference = "pi_3PqLongStripeReferenceThatIsLongerThanAnyPaperWidth";
    c.payments.push_back(pay);
    return c;
}

} // namespace

TEST_CASE("Every printout fits the paper and stays readable", "[print][fit]")
{
    auto seed = test::seedPosData();
    seed.settings.storeName = "La Taquería del Barrio — Since 1987";
    seed.settings.receiptHeader = "123 Main St\nSpringfield ☎ 555-0100";
    seed.settings.receiptFooter = "¡Gracias! Vuelva pronto 😊";
    const core::Check c = awkwardCheck();
    core::Report report;
    report.title = "Server Sales (comparison)";
    report.subtitle = "This week vs. last week";
    report.rows.push_back({core::ReportRow::Kind::Section, {"Servers"}});
    report.rows.push_back({core::ReportRow::Kind::Line, {"Maria-José Fernández", "$12,345.67", "$11,000.00", "+12.2%", "142"}});
    report.rows.push_back({core::ReportRow::Kind::Total, {"Total", "$123,456.78", "$110,000.00", "+12.2%"}});
    report.rows.push_back({core::ReportRow::Kind::Note, {"A note that is longer than any paper is wide, so it must wrap neatly."}});
    for (const char *currency : {"$", "€", "£"}) {
        seed.settings.currencySymbol = currency;
        for (const int width : {32, 42, 48}) {
            const auto context = ctx(seed.settings);
            const std::vector<Document> docs = {receipt(c, context), kitchenTicket(c, c.lines, "Kitchen", false, context),
                                                kitchenTicket(c, c.lines, "Bar", true, context), reportTicket(report, context)};
            for (const Document &d : docs) {
                const std::string text = renderText(d, width);
                std::istringstream in(text);
                for (std::string row; std::getline(in, row);) {
                    INFO(width << " text: " << row);
                    CHECK(displayWidth(row) <= std::size_t(width));
                    CHECK(row.find('\x1b') == std::string::npos);
                    CHECK(row.find('\t') == std::string::npos);
                    CHECK(row.find('?') == std::string::npos);
                }
                for (const bool accents : {true, false}) {
                    for (const std::string &row : escPosRows(renderEscPos(d, width, accents))) {
                        INFO(width << (accents ? " PC858: " : " plain: ") << row);
                        CHECK(row.size() <= std::size_t(width));   // one byte, one column
                        CHECK(row.find('?') == std::string::npos);
                        if (!accents)
                            CHECK(std::ranges::none_of(row, [](char ch) { return static_cast<unsigned char>(ch) >= 0x80; }));
                    }
                }
            }
        }
    }
    // Readable: the currency and the accents survive, the address keeps its lines.
    seed.settings.currencySymbol = "€";
    const std::string r = renderText(receipt(c, ctx(seed.settings)), 42);
    CHECK(contains(r, "€"));
    CHECK(contains(r, "Crème brûlée ½ · café"));
    CHECK(contains(r, "\nApt 5B Gate code #4321"));   // its own row; the tab and ESC gone
    CHECK_FALSE(contains(r, "\n \n"));                // no row left with only an indent
    CHECK(contains(r, "Ñoño \"The Regular\" O'Brien"));
    CHECK(contains(renderEscPos(receipt(c, ctx(seed.settings)), 42, false), "EUR"));
}

TEST_CASE("Receipts leave out free choices; the kitchen gets them all", "[print][fit]")
{
    auto seed = test::seedPosData();
    const core::Check c = awkwardCheck();
    std::string r = renderText(receipt(c, ctx(seed.settings)), 42);
    CHECK(contains(r, "Extra bacon"));
    CHECK_FALSE(contains(r, "No onion"));
    CHECK_FALSE(contains(r, "Medium rare"));
    const std::string k = renderText(kitchenTicket(c, c.lines, "Kitchen", false, ctx(seed.settings)), 42);
    CHECK(contains(k, "No onion"));
    CHECK(contains(k, "Medium rare"));
    CHECK(contains(k, "Extra bacon"));
    seed.settings.receiptFreeChoices = true;   // Store Settings: shown
    r = renderText(receipt(c, ctx(seed.settings)), 42);
    CHECK(contains(r, "No onion"));
    CHECK(contains(r, "Medium rare"));
}

TEST_CASE("Every kind of space prints as a space (the time before AM/PM)", "[print][fit]")
{
    Document d;
    d.text("11:23 PM");                            // how Qt writes the time
    d.text("a b c d　e");            // no-break, thin, figure, ideographic
    for (const bool accents : {true, false}) {
        const std::string bytes = renderEscPos(d, 42, accents);
        CHECK(contains(bytes, "11:23 PM"));
        CHECK(contains(bytes, "a b c d e"));
        CHECK_FALSE(contains(bytes, "?"));
    }
    // The real clock, as tickets print it.
    Document now;
    now.text(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat).toStdString());
    CHECK_FALSE(contains(renderEscPos(now, 42), "?"));
}

// --- out of paper, cover open, not answering --------------------------------------------

#include "print/printer_monitor.hh"

TEST_CASE("Printer status: what the ESC/POS answers mean", "[print][status]")
{
    const auto b = [](unsigned char c) { return QByteArray(1, char(c)); };
    CHECK(problemFromStatus(b(0x12), b(0x12)).isEmpty());          // all fine
    CHECK(problemFromStatus(b(0x16), b(0x12)) == u"coverOpen"_s);   // bit 2 of DLE EOT 2
    CHECK(problemFromStatus(b(0x32), b(0x12)) == u"paperOut"_s);    // stopped at the end of the paper
    CHECK(problemFromStatus(b(0x12), b(0x72)) == u"paperOut"_s);    // the paper sensor: out
    CHECK(problemFromStatus(b(0x52), b(0x12)) == u"error"_s);       // a jam, the cutter
    CHECK(problemFromStatus(b(0x12), b(0x1E)) == u"paperLow"_s);    // near the end of the roll
    CHECK(problemFromStatus(b(0x36), b(0x7E)) == u"coverOpen"_s);   // the cover first: that's what to fix
    CHECK(problemFromStatus({}, {}).isEmpty());                     // a printer that doesn't say
    CHECK(problemFromStatus(b(0xFF), b(0xFF)).isEmpty());           // not a status byte: ignored
}

namespace {

// A network printer that answers DLE EOT 2 and 4 with what the test sets.
struct FakeStatusPrinter {
    QTcpServer server;
    unsigned char offline = 0x12, paper = 0x12;
    bool quiet = false;   // takes the connection, says nothing (stuck)
    int connections = 0;
    FakeStatusPrinter()
    {
        REQUIRE(server.listen(QHostAddress::LocalHost));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (QTcpSocket *s = server.nextPendingConnection()) {
                ++connections;
                QObject::connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    const QByteArray in = s->readAll();
                    if (quiet)
                        return;
                    for (int i = 0; i + 2 < in.size(); ++i)
                        if (in[i] == 0x10 && in[i + 1] == 0x04)
                            s->write(QByteArray(1, char(in[i + 2] == 2 ? offline : paper)));
                });
                QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }
    core::PrinterConfig config() const
    {
        core::PrinterConfig p;
        p.id = "kitchen";
        p.name = "Kitchen";
        p.type = "network";
        p.host = "127.0.0.1";
        p.port = server.serverPort();
        return p;
    }
};

} // namespace

TEST_CASE("Printer status: out of paper is seen, then fixed, then unplugged", "[print][status]")
{
    FakeStatusPrinter printer;
    PrinterMonitor monitor;
    monitor.setTimings(150, 400);
    QStringList seen;
    QObject::connect(&monitor, &PrinterMonitor::statusChanged,
                     [&](const QString &id, const QString &problem) { seen << id + u'=' + problem; });
    const auto waitFor = [&](const QString &what) {
        for (int i = 0; i < 100 && !seen.contains(what); ++i)
            QTest::qWait(20);
        return seen.contains(what);
    };
    monitor.setPrinters({printer.config()});
    QTest::qWait(400);
    CHECK(seen.isEmpty());                                // fine: nothing to say
    printer.paper = 0x72;                                 // the roll runs out
    REQUIRE(waitFor(u"kitchen=paperOut"_s));
    printer.paper = 0x12;                                 // a new roll
    REQUIRE(waitFor(u"kitchen="_s));
    printer.offline = 0x16;
    REQUIRE(waitFor(u"kitchen=coverOpen"_s));
    printer.offline = 0x12;
    seen.clear();
    REQUIRE(waitFor(u"kitchen="_s));
    printer.server.close();                               // unplugged
    REQUIRE(waitFor(u"kitchen=offline"_s));
}

TEST_CASE("Printer status: never while a ticket is on its way; not for printers turned off", "[print][status]")
{
    FakeStatusPrinter printer;
    PrinterMonitor monitor;
    std::atomic<bool> busy = true;
    monitor.setBusy([&](const QString &, int) { return busy.load(); });
    monitor.setTimings(100, 300);
    monitor.setPrinters({printer.config()});
    QTest::qWait(500);
    CHECK(printer.connections == 0);   // a ticket is going: it waits
    busy = false;
    for (int i = 0; i < 50 && printer.connections == 0; ++i)
        QTest::qWait(20);
    CHECK(printer.connections > 0);

    core::PrinterConfig off = printer.config();
    off.watch = false;                 // Manager -> Printers: don't warn
    monitor.setPrinters({off});
    QTest::qWait(100);
    const int before = printer.connections;
    QTest::qWait(400);
    CHECK(printer.connections == before);
}

// A real printer's status (hidden): VTM_PRINTER_HOST=192.168.1.101 vtm_tests "[printerstatuslive]"
TEST_CASE("A network printer, live: its status", "[.][printerstatuslive]")
{
    const QString host = qEnvironmentVariable("VTM_PRINTER_HOST");
    if (host.isEmpty())
        SKIP("Set VTM_PRINTER_HOST to the printer's address.");
    core::PrinterConfig p;
    p.id = "receipt";
    p.name = "Receipt";
    p.type = "network";
    p.host = host.toStdString();
    PrinterMonitor monitor;
    monitor.setTimings(1000, 1500);
    QString last = u"(fine)"_s;
    QObject::connect(&monitor, &PrinterMonitor::statusChanged, [&](const QString &, const QString &problem) {
        last = problem.isEmpty() ? u"(fine)"_s : problem;
    });
    monitor.setPrinters({p});
    QTest::qWait(5000);
    WARN("Printer says: " << last.toStdString());
}

TEST_CASE("Printer status: one that stops responding while still connected", "[print][status][silent]")
{
    FakeStatusPrinter printer;
    PrinterMonitor monitor;
    monitor.setTimings(150, 200);
    QStringList seen;
    QObject::connect(&monitor, &PrinterMonitor::statusChanged,
                     [&](const QString &id, const QString &problem) { seen << id + u'=' + problem; });
    const auto waitFor = [&](const QString &what) {
        for (int i = 0; i < 150 && !seen.contains(what); ++i)
            QTest::qWait(20);
        return seen.contains(what);
    };
    // A printer that never says: nothing to warn about.
    printer.quiet = true;
    monitor.setPrinters({printer.config()});
    QTest::qWait(1200);
    CHECK_FALSE(seen.contains(u"kitchen=silent"_s));
    // One that has said, then stops: warned; answering again: cleared.
    printer.quiet = false;
    for (int i = 0; i < 50 && printer.connections < 8; ++i)
        QTest::qWait(20);
    printer.quiet = true;
    REQUIRE(waitFor(u"kitchen=silent"_s));
    printer.quiet = false;
    seen.clear();
    REQUIRE(waitFor(u"kitchen="_s));
}

TEST_CASE("Printer status: paper low, then no connections: probably out of paper", "[print][status][offlinepaper]")
{
    FakeStatusPrinter printer;
    PrinterMonitor monitor;
    monitor.setTimings(150, 300);
    QStringList seen;
    QObject::connect(&monitor, &PrinterMonitor::statusChanged,
                     [&](const QString &id, const QString &problem) { seen << id + u'=' + problem; });
    const auto waitFor = [&](const QString &what) {
        for (int i = 0; i < 150 && !seen.contains(what); ++i)
            QTest::qWait(20);
        return seen.contains(what);
    };
    printer.paper = 0x1E;                 // near the end of the roll
    monitor.setPrinters({printer.config()});
    REQUIRE(waitFor(u"kitchen=paperLow"_s));
    printer.server.close();               // out: like an Epson, it takes no connections
    REQUIRE(waitFor(u"kitchen=offlinePaper"_s));
    CHECK_FALSE(seen.contains(u"kitchen=offline"_s));
}

TEST_CASE("Printer status: paper low, then printed to the end (connected, silent): probably out of paper", "[print][status][offlinepaper]")
{
    FakeStatusPrinter printer;
    PrinterMonitor monitor;
    monitor.setTimings(150, 200);
    QStringList seen;
    QObject::connect(&monitor, &PrinterMonitor::statusChanged,
                     [&](const QString &id, const QString &problem) { seen << id + u'=' + problem; });
    const auto waitFor = [&](const QString &what) {
        for (int i = 0; i < 150 && !seen.contains(what); ++i)
            QTest::qWait(20);
        return seen.contains(what);
    };
    printer.paper = 0x1E;
    monitor.setPrinters({printer.config()});
    REQUIRE(waitFor(u"kitchen=paperLow"_s));
    printer.quiet = true;                 // the roll printed to its end
    REQUIRE(waitFor(u"kitchen=offlinePaper"_s));
    CHECK_FALSE(seen.contains(u"kitchen=silent"_s));
    printer.quiet = false;                // a new roll
    printer.paper = 0x12;
    REQUIRE(waitFor(u"kitchen="_s));
}

TEST_CASE("Printer status: learned that it answers; silent from the start after a restart", "[print][status][learned]")
{
    FakeStatusPrinter printer;
    QStringList learned, seen;
    {
        PrinterMonitor monitor;
        monitor.setTimings(150, 200);
        QObject::connect(&monitor, &PrinterMonitor::answersStatus, [&](const QString &id) { learned << id; });
        monitor.setPrinters({printer.config()});
        for (int i = 0; i < 50 && learned.isEmpty(); ++i)
            QTest::qWait(20);
        QTest::qWait(400);
        CHECK(learned == QStringList{u"kitchen"_s});   // once
    }
    // The next day it's stuck before the screens start.
    printer.quiet = true;
    core::PrinterConfig known = printer.config();
    known.reportsStatus = true;
    PrinterMonitor monitor;
    monitor.setTimings(150, 200);
    QObject::connect(&monitor, &PrinterMonitor::statusChanged,
                     [&](const QString &id, const QString &problem) { seen << id + u'=' + problem; });
    monitor.setPrinters({known});
    for (int i = 0; i < 150 && !seen.contains(u"kitchen=silent"_s); ++i)
        QTest::qWait(20);
    CHECK(seen.contains(u"kitchen=silent"_s));
}

TEST_CASE("Receipts say what a service charge and a delivery fee are (fee laws)", "[print][law]")
{
    const auto seed = test::seedPosData();
    core::Check c = burgerCheck(seed.settings, seed.menu);
    c.gratuityBp = 1800;
    c.autoGratuity = true;
    core::OrderLine fee;
    fee.id = 99;
    fee.itemId = "fee:delivery";
    fee.name = "Delivery fee";
    fee.unitPrice = Money::fromCents(350);
    c.lines.push_back(fee);
    core::PosSettings s = seed.settings;
    s.gratuityMinGuests = 6;
    std::string r = renderText(receipt(c, ctx(s)), 42);
    CHECK(contains(r, "For 6+ guests; goes to the server."));
    CHECK(contains(r, "Store's fee, not the driver's tip."));
    s.deliveryFeeToDriver = true;
    c.autoGratuity = false;
    r = renderText(receipt(c, ctx(s)), 42);
    CHECK(contains(r, "Goes to the driver."));
    CHECK(contains(r, "Goes to the server."));
}
