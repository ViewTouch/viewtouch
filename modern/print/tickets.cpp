#include "print/tickets.hh"

#include <QString>

#include <algorithm>
#include <sstream>

using namespace vt::core;

namespace vt::print {

namespace {

std::vector<std::string> splitLines(const std::string &s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);)
        out.push_back(line);
    return out;
}

std::string join(const std::vector<std::string> &v, const std::string &sep)
{
    std::string out;
    for (const std::string &x : v)
        out += (out.empty() ? "" : sep) + x;
    return out;
}

// Accented letters too ("niño" -> "NIÑO").
std::string upper(const std::string &s)
{
    return QString::fromStdString(s).toUpper().toStdString();
}

std::string taxName(TaxClass c, const TicketContext &ctx)
{
    switch (c) {
    case TaxClass::Food: return ctx.tr("Food tax");
    case TaxClass::Alcohol: return ctx.tr("Alcohol tax");
    case TaxClass::Merchandise: return ctx.tr("Merchandise tax");
    case TaxClass::Room: return ctx.tr("Room tax");
    case TaxClass::None: break;
    }
    return ctx.tr("Tax");
}

std::string typeName(CheckType t, const TicketContext &ctx)
{
    switch (t) {
    case CheckType::Takeout: return ctx.tr("Takeout");
    case CheckType::Quick: return ctx.tr("Quick");
    case CheckType::Delivery: return ctx.tr("Delivery");
    case CheckType::Tab: return ctx.tr("Tab");
    case CheckType::DineIn: break;
    }
    return ctx.tr("For here");
}

} // namespace

std::string TicketContext::tr(const std::string &english, std::initializer_list<std::string> args) const
{
    std::string out = words ? words(english) : english;
    if (out.empty())
        out = english;
    int n = 0;
    for (const std::string &a : args) {
        const std::string mark = "%" + std::to_string(++n);
        if (const std::size_t at = out.find(mark); at != std::string::npos)
            out.replace(at, mark.size(), a);
    }
    return out;
}

std::string TicketContext::money(Money m) const
{
    const std::string s = m.toString();
    return s.front() == '-' ? "-" + settings.currencySymbol + s.substr(1) : settings.currencySymbol + s;
}

Document receipt(const Check &check, const TicketContext &ctx)
{
    const Totals t = check.totals(ctx.settings.tax);
    Document d;
    if (ctx.logo)
        d.image(ctx.logo);
    d.center(ctx.settings.storeName, true, true);
    for (const std::string &l : splitLines(ctx.settings.receiptHeader))
        d.center(l);
    d.blank();
    d.columns(ctx.tr("Check #%1", {std::to_string(check.id)}), check.label, true);
    d.columns(ctx.tr("Server: %1", {check.serverName}), ctx.tr("Guests: %1", {std::to_string(check.guests)}));
    d.text(ctx.dateTime(check.closedAt ? check.closedAt : ctx.now));
    if (!check.customer.empty()) {
        if (!check.customer.name.empty())
            d.columns(check.customer.name, check.customer.phone);
        if (!check.customer.address.empty())
            d.text(check.customer.address);
    }
    d.rule();

    for (const OrderLine &l : check.lines) {
        if (l.voided)
            continue;
        if (l.isComment())
            continue;   // kitchen notes are not for the guest
        if (l.isShare()) {   // its part of the whole, the choices under it unpriced
            d.columns(l.displayName(), ctx.money(l.total()));
            for (const Modifier &m : l.modifiers)
                if (m.price().cents() != 0 || ctx.settings.receiptFreeChoices)
                    d.text("  " + m.displayName());
            continue;
        }
        const std::string qty = l.quantity > 1 ? std::to_string(l.quantity) + " x " : "";
        d.columns(qty + l.displayName(), ctx.money(qualifiedPrice(l.unitPrice, l.qualifier) * l.quantity));
        for (const Modifier &m : l.modifiers) {
            if (m.price().cents() == 0 && !ctx.settings.receiptFreeChoices)
                continue;   // No onion, Medium rare: the kitchen's, not the guest's
            d.columns("  " + m.displayName(), m.price().cents() ? ctx.money(m.price() * l.quantity) : "");
        }
    }
    d.rule();
    d.columns(ctx.tr("Subtotal"), ctx.money(t.items));
    for (const Payment &p : check.payments) {
        if (p.kind == TenderKind::Discount)
            d.columns(p.tenderName, ctx.money(-t.items.percent(p.percentBp)));
    }
    for (const auto &[cls, amount] : t.taxByClass)
        d.columns(taxName(cls, ctx), ctx.money(amount));
    if (t.gratuity.cents() != 0)
        d.columns(ctx.tr("Gratuity %1%", {std::to_string(check.gratuityBp / 100)}), ctx.money(t.gratuity));
    d.columns(upper(ctx.tr("Total")), ctx.money(t.total), true, true);
    bool paid = false;
    for (const Payment &p : check.payments) {
        if (p.kind == TenderKind::Discount)
            continue;
        if (!paid) {
            d.blank();
            paid = true;
        }
        d.columns(p.tenderName, ctx.money(p.amount));
        // A card from a reader: which one, and its payment id for disputes.
        if (!p.last4.empty()) {
            std::string brand = p.cardBrand;
            for (char &ch : brand)
                ch = char(std::toupper(static_cast<unsigned char>(ch)));
            d.text("  " + (brand.empty() ? std::string() : brand + " ") + "**** " + p.last4);
        }
        // Taken while the internet was down: the bank hasn't answered yet.
        if (p.offline == "waiting")
            d.text("  " + ctx.tr("Approval pending (taken offline)"));
        else if (p.offline == "declined" || p.offline == "seen")
            d.text("  " + ctx.tr("Declined once sent: not paid"));
        if (p.processor == "stripe" && !p.reference.empty() && p.offline.empty())
            d.text("  " + p.reference);
        if (p.tip.cents() != 0)
            d.columns("  " + ctx.tr("Tip"), ctx.money(p.tip));
    }
    if (t.rounding.cents() != 0)
        d.columns(ctx.tr("Cash rounding"), ctx.money(t.rounding));
    if (t.tips.cents() != 0)
        d.columns(ctx.tr("Total with tip"), ctx.money(t.total + t.tips), true);
    if (t.change.cents() > 0)
        d.columns(ctx.tr("Change"), ctx.money(t.change), true);
    else if (t.balance.cents() > 0)
        d.columns(ctx.tr("Balance due"), ctx.money(t.balance), true);
    d.blank();
    const std::vector<std::string> footer = splitLines(ctx.settings.receiptFooter);
    if (footer.empty())
        d.center(ctx.tr("Thank you!"));
    for (const std::string &l : footer)
        d.center(l);
    return d;
}

Document orderSlip(const Check &check, bool sent, const TicketContext &ctx)
{
    const Totals t = check.totals(ctx.settings.tax);
    Document d;
    if (ctx.logo)
        d.image(ctx.logo);
    d.center(ctx.settings.storeName, true);
    d.blank();
    d.center(upper(ctx.tr("Your order number")), true);
    d.center(std::to_string(check.id), true, true);
    if (!check.customer.name.empty())
        d.center(check.customer.name, true, true);
    d.center(check.type == CheckType::DineIn ? ctx.tr("For here") : ctx.tr("To go"));
    d.center(ctx.dateTime(ctx.now));
    d.rule();
    for (const OrderLine &l : check.lines) {
        if (l.voided || l.isComment())
            continue;
        if (l.isShare()) {   // its part of the whole, the choices under it unpriced
            d.columns(l.displayName(), ctx.money(l.total()));
            for (const Modifier &m : l.modifiers)
                if (m.price().cents() != 0 || ctx.settings.receiptFreeChoices)
                    d.text("  " + m.displayName());
            continue;
        }
        const std::string qty = l.quantity > 1 ? std::to_string(l.quantity) + " x " : "";
        d.columns(qty + l.displayName(), ctx.money(qualifiedPrice(l.unitPrice, l.qualifier) * l.quantity));
        for (const Modifier &m : l.modifiers) {
            if (m.price().cents() == 0 && !ctx.settings.receiptFreeChoices)
                continue;
            d.columns("  " + m.displayName(), m.price().cents() ? ctx.money(m.price() * l.quantity) : "");
        }
    }
    d.rule();
    d.columns(ctx.tr("Subtotal"), ctx.money(t.items));
    for (const auto &[cls, amount] : t.taxByClass)
        d.columns(taxName(cls, ctx), ctx.money(amount));
    d.columns(upper(ctx.tr("Total")), ctx.money(t.total), true, true);
    d.blank();
    if (t.balance.cents() > 0)
        d.center(ctx.tr("Please pay at the counter"), true);
    if (sent)
        d.center(ctx.tr("We're making it now"), true);
    else
        d.center(ctx.tr("We'll make it once it's paid"));
    d.center(ctx.tr("Show this slip at the counter"));
    d.blank();
    return d;
}

Document kitchenTicket(const Check &check, const std::vector<OrderLine> &lines, const std::string &station,
                       bool voids, const TicketContext &ctx)
{
    Document d;
    d.center(upper(station), true);
    if (voids)
        d.center(ctx.tr("*** VOID ***"), true, true);
    if (check.rush)
        d.center("*** " + upper(ctx.tr("Rush")) + " ***", true, true);
    if (check.vip)
        d.center("* VIP *", true, true);
    if (!check.allergies.empty()) {
        d.center("!! " + upper(ctx.tr("Allergy")) + " !!", true, true);
        d.center(upper(join(ctx.allergens(check.allergies), ", ")), true, true);
    }
    if (check.dueAt > 0)
        d.center(upper(ctx.tr("Ready at %1", {ctx.time(check.dueAt)})), true, true);
    d.text(check.label, Document::Align::Left, true, true);
    d.columns("#" + std::to_string(check.id) + "  " + check.serverName, ctx.time(ctx.now));
    if (check.type != CheckType::DineIn)
        d.text(upper(typeName(check.type, ctx)), Document::Align::Left, true);
    if (!check.customer.name.empty())
        d.text(check.customer.name + (check.customer.phone.empty() ? "" : "  " + check.customer.phone));
    if (!check.customer.note.empty())
        d.text(upper(ctx.tr("Note")) + ": " + check.customer.note, Document::Align::Left, true);
    d.rule();
    // A later course says so up front; each line says its seat.
    int course = 0;
    for (const OrderLine &l : lines)
        course = std::max(course, l.course);
    if (course > 1)
        d.center(upper(ctx.tr("Course %1", {std::to_string(course)})), true, true);
    for (const OrderLine &l : lines) {
        const std::string seat = l.seat > 0 ? ctx.tr("S%1", {std::to_string(l.seat)}) + " " : std::string();
        if (l.isComment()) {
            d.text(seat + "** " + l.name + " **", Document::Align::Left, true);
            continue;
        }
        if (!l.forKitchen())
            continue;
        d.text(seat + std::to_string(l.quantity) + " " + l.kitchenText(), Document::Align::Left, true, true);
        std::vector<std::string> hits;
        for (const std::string &a : l.allergens)
            if (std::ranges::find(check.allergies, a) != check.allergies.end())
                hits.push_back(a);
        if (!hits.empty())
            d.text("   !! " + upper(ctx.tr("Contains %1", {join(ctx.allergens(hits), ", ")})), Document::Align::Left, true);
        for (const Modifier &m : l.modifiers) {
            if (!m.kitchenHide)
                d.text("   > " + m.kitchenText(), Document::Align::Left, true);
        }
    }
    d.rule();
    return d;
}

Document reportTicket(const Report &report, const TicketContext &ctx)
{
    Document d;
    d.center(ctx.settings.storeName, true);
    d.center(ctx.tr(report.title), true, true);
    if (!report.subtitle.empty())
        d.center(ctx.tr(report.subtitle));
    d.text(ctx.dateTime(ctx.now), Document::Align::Center);
    d.rule();
    for (const ReportRow &r : report.rows) {
        if (r.cells.empty())
            continue;
        switch (r.kind) {
        case ReportRow::Kind::Section:
            d.blank();
            d.text(ctx.tr(r.cells.front()), Document::Align::Left, true);
            break;
        case ReportRow::Kind::Note:
            d.text(ctx.tr(r.cells.front()));
            break;
        case ReportRow::Kind::Line:
        case ReportRow::Kind::Total: {
            // Every column after the first is right-aligned in 9 characters.
            std::string right;
            for (std::size_t i = 1; i < r.cells.size(); ++i) {
                const std::string c = ctx.tr(r.cells[i]);
                const std::size_t w = displayWidth(c);
                right += (w < 9 ? std::string(9 - w, ' ') : " ") + c;
            }
            d.columns(ctx.tr(r.cells.front()), right, r.kind == ReportRow::Kind::Total);
            break;
        }
        }
    }
    d.rule();
    return d;
}

Document drawerKick()
{
    Document d;
    d.cut = false;
    d.kickDrawer = true;
    return d;
}

} // namespace vt::print
