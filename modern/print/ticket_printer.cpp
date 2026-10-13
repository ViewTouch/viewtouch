#include "print/ticket_printer.hh"

#include "print/labels.hh"

#include "app/i18n.hh"
#include "app/pos_session.hh"
#include "print/raster.hh"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QLocale>

#include <map>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::print {

TicketPrinter::TicketPrinter(PrintSpooler &spooler, QString outputDir)
    : spooler_(spooler)
    , outputDir_(std::move(outputDir))
    , now_([] { return QDateTime::currentMSecsSinceEpoch(); })
{
}

TicketContext TicketPrinter::context(const PosSettings &settings) const
{
    return TicketContext{
        settings,
        // In the store's language, like the rest of what's printed.
        [loc = i18n::locale(i18n::printLanguage(QString::fromStdString(settings.language)))](std::int64_t ms) {
            return (loc.toString(QDateTime::fromMSecsSinceEpoch(ms).date(), QLocale::ShortFormat) + u' '
                    + loc.toString(QDateTime::fromMSecsSinceEpoch(ms).time(), u"h:mm AP"_s)).toStdString();
        },
        [loc = i18n::locale(i18n::printLanguage(QString::fromStdString(settings.language)))](std::int64_t ms) {
            return loc.toString(QDateTime::fromMSecsSinceEpoch(ms).time(), u"h:mm AP"_s).toStdString();
        },
        now_(),
        nullptr,
        [](const std::vector<std::string> &ids) {
            std::vector<std::string> out;
            for (const QString &n : app::PosSession::allergenNames(ids))
                out.push_back(n.toStdString());
            return out;
        },
        [code = i18n::printLanguage(QString::fromStdString(settings.language))](const std::string &english) {
            return i18n::phrase(code, QString::fromStdString(english)).toStdString();
        },
    };
}

void TicketPrinter::send(const PosSettings &, const PrinterConfig &printer, const Document &doc,
                         const QString &description)
{
    PrinterConfig target = printer;
    if (target.type == "file" && QDir::isRelativePath(QString::fromStdString(target.path)))
        target.path = QDir(outputDir_).filePath(QString::fromStdString(target.path)).toStdString();
    Document d = doc;
    d.cut = d.cut && printer.cutter;
    const std::string bytes = printer.effectiveFormat() == "escpos"
                                  ? renderEscPos(d, printer.width, printer.charset != "ascii")
                                  : renderText(d, printer.width);
    spooler_.submit(target, QByteArray::fromStdString(bytes), description);
}

void TicketPrinter::printKitchen(const PosSettings &settings, const Check &check,
                                 const std::vector<OrderLine> &lines, bool voids)
{
    const i18n::Scope language(i18n::printLanguage(QString::fromStdString(settings.language)));   // the store's, not the server's
    // Group by station; items without a printer (or with an unknown one) go
    // to the kitchen, and nowhere if there is no kitchen printer either.
    std::map<std::string, std::vector<OrderLine>> byStation;
    for (const OrderLine &l : lines) {
        if (!l.isComment() && !l.forKitchen())   // gift cards, hidden items: nothing to make
            continue;
        std::string station = l.printer.empty() ? "kitchen" : l.printer;
        if (!settings.printer(station))
            station = "kitchen";
        byStation[station].push_back(l);
    }
    const TicketContext ctx = context(settings);
    for (const auto &[station, stationLines] : byStation) {
        const PrinterConfig *p = settings.printer(station);
        if (!p)
            continue;
        send(settings, *p, kitchenTicket(check, stationLines, p->name, voids, ctx),
             (voids ? u"Void ticket %1"_s : u"Ticket %1"_s).arg(QString::fromStdString(check.label)));
    }
}

namespace {
const PrinterConfig *receiptPrinter(const PosSettings &settings, const std::string &id)
{
    const PrinterConfig *p = settings.printer(id);
    return p ? p : settings.printer("receipt");
}
} // namespace

std::shared_ptr<const Raster> TicketPrinter::logoFor(const PosSettings &settings, int widthChars)
{
    if (!imageSource_ || settings.displayLogo.empty())
        return nullptr;
    const QByteArray picture = imageSource_(QString::fromStdString(settings.displayLogo));
    if (picture.isEmpty())
        return nullptr;
    const auto key = std::pair{QCryptographicHash::hash(picture, QCryptographicHash::Sha1), widthChars};
    if (const auto it = logos_.find(key); it != logos_.end())
        return it->second;
    // 12 dots a character (80 mm paper: 48 chars, 576 dots); the logo takes
    // three quarters of it at most, and about 2.5 cm of paper.
    const int paper = std::clamp(widthChars * 12, 192, 576);
    auto raster = rasterize(picture, paper * 3 / 4, 200);
    logos_[key] = raster;
    return raster;
}

void TicketPrinter::printReceipt(const PosSettings &settings, const Check &check, const std::string &printerId)
{
    const i18n::Scope language(i18n::printLanguage(QString::fromStdString(settings.language)));   // the guest's: the store's
    if (const PrinterConfig *p = receiptPrinter(settings, printerId)) {
        TicketContext ctx = context(settings);
        if (settings.receiptLogo && p->effectiveFormat() == "escpos")
            ctx.logo = logoFor(settings, p->width);
        send(settings, *p, receipt(check, ctx), u"Receipt #%1"_s.arg(check.id));
    }
}

void TicketPrinter::printOrderSlip(const PosSettings &settings, const Check &check, const std::string &printerId,
                                   bool sent)
{
    const i18n::Scope language(i18n::printLanguage(QString::fromStdString(settings.language)));
    if (const PrinterConfig *p = receiptPrinter(settings, printerId)) {
        TicketContext ctx = context(settings);
        if (settings.receiptLogo && p->effectiveFormat() == "escpos")
            ctx.logo = logoFor(settings, p->width);
        send(settings, *p, orderSlip(check, sent, ctx), u"Order slip #%1"_s.arg(check.id));
    }
}

void TicketPrinter::printReport(const PosSettings &settings, const Report &report, const std::string &printerId)
{
    if (const PrinterConfig *p = receiptPrinter(settings, printerId))
        send(settings, *p, reportTicket(report, context(settings)), QString::fromStdString(report.title));
}

void TicketPrinter::printLabels(const PosSettings &settings,
                                const std::vector<std::pair<std::string, std::vector<std::string>>> &labels,
                                const std::string &printerId)
{
    const PrinterConfig *p = settings.printer(printerId);
    if (!p || labels.empty())
        return;
    std::vector<Label> list;
    for (const auto &[title, lines] : labels)
        list.push_back({title, lines});
    const std::string format = p->effectiveFormat();
    if (format == "zpl" || format == "tspl") {
        PrinterConfig target = *p;
        if (target.type == "file" && QDir::isRelativePath(QString::fromStdString(target.path)))
            target.path = QDir(outputDir_).filePath(QString::fromStdString(target.path)).toStdString();
        const std::string bytes = format == "zpl" ? renderZpl(list, p->labelWidthMm, p->labelHeightMm)
                                                 : renderTspl(list, p->labelWidthMm, p->labelHeightMm);
        spooler_.submit(target, QByteArray::fromStdString(bytes), u"Labels"_s);
        return;
    }
    for (const Label &l : list)   // a receipt printer: a slip each
        send(settings, *p, labelSlip(l), u"Label"_s);
}

void TicketPrinter::openDrawer(const PosSettings &settings, const std::string &printerId)
{
    // The terminal's own printer when its drawer is wired there, else any
    // printer with a drawer.
    const PrinterConfig *mine = receiptPrinter(settings, printerId);
    if (mine && mine->drawerKick) {
        send(settings, *mine, drawerKick(), u"Open drawer"_s);
        return;
    }
    for (const PrinterConfig &p : settings.printers) {
        if (p.drawerKick) {
            send(settings, p, drawerKick(), u"Open drawer"_s);
            return;
        }
    }
}

bool TicketPrinter::printTestPage(const PosSettings &settings, const std::string &printerId, bool kickDrawer)
{
    const auto it = std::ranges::find_if(settings.printers, [&](const PrinterConfig &p) { return p.id == printerId; });
    if (it == settings.printers.end())
        return false;
    const PrinterConfig &p = *it;
    const TicketContext ctx = context(settings);
    const int width = std::max(16, p.width);
    Document d;
    if (p.effectiveFormat() == "escpos")
        d.image(logoFor(settings, width));   // nothing when no logo is set
    d.text(settings.storeName, Document::Align::Center, true, true);
    d.text(ctx.tr("Printer test: %1", {p.name}), Document::Align::Center, true);
    d.text(QDateTime::fromMSecsSinceEpoch(now_ ? now_() : QDateTime::currentMSecsSinceEpoch())
               .toString(u"yyyy-MM-dd hh:mm"_s).toStdString(), Document::Align::Center);
    d.rule();
    d.text(ctx.tr("Normal text"));
    d.text(ctx.tr("Bold text"), Document::Align::Left, true);
    d.text(ctx.tr("Big text"), Document::Align::Left, true, true);
    d.text(ctx.tr("Right"), Document::Align::Right);
    d.columns("2 x Bacon Burger", ctx.money(Money::fromCents(2650)));
    d.columns("    Medium Rare", "");
    d.columns(ctx.tr("Total"), ctx.money(Money::fromCents(2869)), true);
    d.rule();
    // Every column: the paper width setting is right when this row exactly fills a line.
    std::string ruler;
    for (int i = 1; i <= width; ++i)
        ruler += char('0' + i % 10);
    d.text(ruler);
    d.text(ctx.tr("%1 characters per line: the row above should fill exactly one line.", {std::to_string(width)}));
    // Accents (Letters: accented, PC858), else plain letters.
    d.text(p.charset == "ascii" ? "Cafe, nino, senor (plain letters)" : "Café, niño, señor, ¿Qué tal? £5 €5");
    d.blank();
    d.cut = p.cutter;
    d.kickDrawer = kickDrawer && p.drawerKick;
    send(settings, p, d, u"Test page"_s);
    return true;
}

} // namespace vt::print
