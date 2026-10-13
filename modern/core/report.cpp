#include "core/report.hh"

#include <algorithm>
#include <cctype>
#include <tuple>
#include <cmath>
#include <map>
#include <optional>
#include <cstdio>
#include <set>

namespace vt::core {

namespace {

std::string count(std::int64_t n) { return std::to_string(n); }

std::string hours(std::int64_t ms)
{
    const std::int64_t hundredths = (ms * 100 + 1'800'000) / 3'600'000;   // rounded
    std::string frac = std::to_string(hundredths % 100);
    if (frac.size() < 2)
        frac.insert(0, "0");
    return std::to_string(hundredths / 100) + "." + frac;
}

std::string capitalized(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

} // namespace

std::vector<std::string> Report::find(const std::string &label) const
{
    for (const ReportRow &r : rows) {
        if (!r.cells.empty() && r.cells.front() == label && r.kind != ReportRow::Kind::Section)
            return r.cells;
    }
    return {};
}

std::string ReportContext::money(Money m) const
{
    const std::string s = m.toString();
    return s.front() == '-' ? "-" + settings.currencySymbol + s.substr(1) : settings.currencySymbol + s;
}

Money cashIntoDrawer(const Check &check, const TaxRates &rates)
{
    return check.totals(rates).cashNet();
}

Money expectedCash(const DrawerSession &drawer, const std::vector<Check> &closed, const TaxRates &rates)
{
    Money cash = drawer.startingCash + drawer.movementsTotal();
    for (const Check &c : closed) {
        if (c.drawerSession == drawer.id)
            cash += cashIntoDrawer(c, rates) + c.cashTip;   // a cash tip handed over for the server is here too
    }
    return cash;
}

Money tipsOwed(const std::string &employeeId, const std::vector<Check> &closed,
               const std::vector<DrawerSession> &drawers, const TaxRates &rates)
{
    Money owed;
    for (const Check &c : closed) {
        if (c.serverId == employeeId) {
            const Totals t = c.totals(rates);
            owed += t.tips + t.gratuity + c.cashTip;
        }
    }
    for (const DrawerSession &d : drawers) {
        for (const CashMovement &m : d.movements) {
            if (m.kind == CashMovement::Kind::TipPayout && m.employeeId == employeeId)
                owed -= m.amount;
        }
    }
    return owed;
}

Report salesSummary(const std::vector<Check> &closed, const ReportContext &ctx, const std::vector<Refund> *given)
{
    const TaxRates &rates = ctx.settings.tax;
    Report r;
    r.id = "sales";
    r.title = "Sales Summary";
    r.subtitle = ctx.period;
    r.columns = {"", "Amount"};

    Money items, discounts, net, tax, total, change, rounding, gratuity, tips, staffMeals;
    std::int64_t staffMealChecks = 0;
    std::map<TaxClass, Money> taxByClass;
    std::map<std::string, Money> byTender;           // tender name -> amount
    std::vector<std::string> tenderOrder;
    int guests = 0, voidCount = 0;
    Money voidValue;

    for (const Check &c : closed) {
        const Totals t = c.totals(rates);
        items += t.items;
        discounts += t.discounts;
        net += t.subtotal;
        tax += t.tax;
        total += t.total;
        change += t.change;
        rounding += t.rounding;
        staffMeals += t.staffMeals;
        if (t.staffMeals.cents() > 0)
            ++staffMealChecks;
        gratuity += t.gratuity;
        tips += t.tips;
        guests += c.guests;
        for (const auto &[cls, amount] : t.taxByClass)
            taxByClass[cls] += amount;
        for (const Payment &p : c.payments) {
            if (p.kind == TenderKind::Discount)
                continue;
            if (!byTender.contains(p.tenderName))
                tenderOrder.push_back(p.tenderName);
            byTender[p.tenderName] += p.amount;
        }
        for (const OrderLine &l : c.lines) {
            if (l.voided) {
                OrderLine live = l;
                live.voided = false;
                voidValue += live.total();
                ++voidCount;
            }
        }
    }

    r.section("Sales");
    r.line({"Checks", count(std::int64_t(closed.size()))});
    r.line({"Guests", count(guests)});
    r.line({"Item sales", ctx.money(items)});
    r.line({"Discounts & comps", ctx.money(-discounts)});
    if (staffMeals.cents() > 0)
        r.line({"  of which staff meals (" + count(staffMealChecks) + ")", ctx.money(-staffMeals)});
    r.total({"Net sales", ctx.money(net)});
    for (const auto &[cls, amount] : taxByClass)
        r.line({capitalized(toString(cls)) + " tax", ctx.money(amount)});
    if (gratuity.cents() != 0)
        r.line({"Gratuity", ctx.money(gratuity)});
    r.total({"Total with tax", ctx.money(total)});

    r.section("Payments");
    for (const std::string &name : tenderOrder)
        r.line({name, ctx.money(byTender[name])});
    if (change.cents() > 0)
        r.line({"Change given", ctx.money(-change)});
    if (rounding.cents() != 0)
        r.line({"Cash rounding", ctx.money(rounding)});
    Money collected = -change;
    for (const auto &[name, amount] : byTender)
        collected += amount;
    r.total({"Collected", ctx.money(collected)});
    if (tips.cents() != 0)
        r.line({"Card tips (owed to staff)", ctx.money(tips)});

    // Money given back: on the day it went back (or, over a range, on these checks).
    std::vector<Refund> onChecks;
    if (!given)
        for (const Check &c : closed)
            onChecks.insert(onChecks.end(), c.refunds.begin(), c.refunds.end());
    const std::vector<Refund> &refunds = given ? *given : onChecks;
    if (!refunds.empty()) {
        r.section("Refunds");
        Money back;
        for (const Refund &x : refunds) {
            r.line({"#" + std::to_string(x.checkId) + " " + x.checkLabel + " · " + x.tenderName + " · " + x.reason
                        + (x.by.empty() ? std::string() : " (" + x.by + ")"),
                    ctx.money(-x.amount)});
            back += x.amount;
        }
        r.total({"Refunds", ctx.money(-back)});
        r.total({"Collected after refunds", ctx.money(collected + tips - back)});
    }

    r.section("Averages");
    const auto avg = [](Money m, std::int64_t n) { return n > 0 ? Money::fromCents(m.cents()).scaled(1, n) : Money(); };
    r.line({"Per check", ctx.money(avg(net, std::int64_t(closed.size())))});
    r.line({"Per guest", ctx.money(avg(net, guests))});
    if (voidCount > 0) {
        r.section("Voids");
        r.line({"Voided items (" + count(voidCount) + ")", ctx.money(voidValue)});
    }
    return r;
}

Report itemSales(const std::vector<Check> &closed, const std::vector<MenuItem> &menu, const ReportContext &ctx)
{
    Report r;
    r.id = "items";
    r.title = "Item Sales";
    r.subtitle = ctx.period;
    r.columns = {"Item", "Qty", "Sales"};

    std::map<std::string, std::string> familyOf;
    for (const MenuItem &m : menu)
        familyOf[m.id] = m.family.empty() ? "other" : m.family;

    struct Tally { std::int64_t qty = 0; Money sales; };
    std::map<std::string, std::map<std::string, Tally>> families;   // family -> item name -> tally
    for (const Check &c : closed) {
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment())
                continue;
            const auto it = familyOf.find(l.itemId);
            Tally &t = families[it == familyOf.end() ? "other" : it->second][l.name];
            t.qty += l.counted();
            t.sales += l.total();
        }
    }

    Money grand;
    std::int64_t grandQty = 0;
    for (const auto &[family, items] : families) {
        r.section(capitalized(family));
        Money sum;
        std::int64_t qty = 0;
        for (const auto &[name, t] : items) {
            r.line({name, count(t.qty), ctx.money(t.sales)});
            sum += t.sales;
            qty += t.qty;
        }
        r.total({capitalized(family) + " total", count(qty), ctx.money(sum)});
        grand += sum;
        grandQty += qty;
    }
    if (families.empty())
        r.note("No sales yet.");
    else
        r.total({"All items", count(grandQty), ctx.money(grand)});

    // Menu items chosen with another (a combo's side and drink): how many
    // went out, and what they added (already in the items above).
    std::map<std::string, Tally> chosen;
    for (const Check &c : closed) {
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment())
                continue;
            for (const Modifier &m : l.modifiers) {
                if (m.itemId.empty() || m.group.empty() || m.qualifier == Qualifier::No)
                    continue;
                Tally &t = chosen[m.name];
                t.qty += l.counted();
                t.sales += m.price() * l.counted();
            }
        }
    }
    if (!chosen.empty()) {
        r.section("Chosen with other items");
        for (const auto &[name, t] : chosen)
            r.line({name, count(t.qty), ctx.money(t.sales)});
    }
    return r;
}

namespace {
std::string hourLabel(int h)
{
    const int twelve = h % 12 == 0 ? 12 : h % 12;
    return std::to_string(twelve) + (h < 12 ? " AM" : " PM");
}

std::string percent(Money part, Money whole)
{
    if (whole.cents() == 0)
        return "0%";
    const std::int64_t tenths = (part.cents() * 1000 + whole.cents() / 2) / whole.cents();
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}
} // namespace

Report hourlySales(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "hourly";
    r.title = "Sales by Hour";
    r.subtitle = ctx.period;
    r.columns = {"Hour", "Checks", "Guests", "Net sales"};
    struct Tally { std::int64_t checks = 0; std::int64_t guests = 0; Money net; };
    std::map<int, Tally> hours;
    Tally all;
    for (const Check &c : closed) {
        const int h = ctx.hourOf ? ctx.hourOf(c.closedAt) : int((c.closedAt / 3'600'000) % 24);
        Tally &t = hours[h];
        const Money net = c.totals(ctx.settings.tax).subtotal;
        t.checks += 1;
        t.guests += c.guests;
        t.net += net;
        all.checks += 1;
        all.guests += c.guests;
        all.net += net;
    }
    for (const auto &[h, t] : hours)
        r.line({hourLabel(h) + " - " + hourLabel((h + 1) % 24), count(t.checks), count(t.guests), ctx.money(t.net)});
    if (hours.empty())
        r.note("No sales yet.");
    else
        r.total({"Total", count(all.checks), count(all.guests), ctx.money(all.net)});
    return r;
}

Report categorySales(const std::vector<Check> &closed, const std::vector<MenuItem> &menu, const ReportContext &ctx)
{
    Report r;
    r.id = "categories";
    r.title = "Sales by Category";
    r.subtitle = ctx.period;
    r.columns = {"Category", "Qty", "Sales", "Share"};
    std::map<std::string, std::string> familyOf;
    for (const MenuItem &m : menu)
        familyOf[m.id] = m.family.empty() ? "other" : m.family;
    struct Tally { std::int64_t qty = 0; Money sales; };
    std::map<std::string, Tally> families;
    Money total;
    std::int64_t qty = 0;
    for (const Check &c : closed) {
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment())
                continue;
            const auto it = familyOf.find(l.itemId);
            Tally &t = families[it == familyOf.end() ? "other" : it->second];
            t.qty += l.counted();
            t.sales += l.total();
            total += l.total();
            qty += l.counted();
        }
    }
    std::vector<std::pair<std::string, Tally>> sorted(families.begin(), families.end());
    std::ranges::sort(sorted, [](const auto &a, const auto &b) { return a.second.sales > b.second.sales; });
    for (const auto &[family, t] : sorted)
        r.line({capitalized(family), count(t.qty), ctx.money(t.sales), percent(t.sales, total)});
    if (sorted.empty())
        r.note("No sales yet.");
    else
        r.total({"All categories", count(qty), ctx.money(total), "100.0%"});
    return r;
}

Report auditReport(const std::vector<const Check *> &checks, const ReportContext &ctx)
{
    Report r;
    r.id = "audit";
    r.title = "Audit Trail";
    r.subtitle = ctx.period;
    r.columns = {"What", "Check", "Who", "Time"};
    struct Row { std::int64_t at; const Check *check; const CheckEvent *event; };
    std::vector<Row> rows;
    std::map<std::string, std::int64_t> counts;
    for (const Check *c : checks) {
        for (const CheckEvent &e : c->events) {
            rows.push_back({e.at, c, &e});
            counts[e.kind.empty() ? "other" : e.kind] += 1;
        }
    }
    std::ranges::sort(rows, {}, &Row::at);
    if (rows.empty()) {
        r.note("No voids, discounts, reopened, moved, transferred or merged checks.");
        return r;
    }
    r.section("Summary");
    const std::pair<const char *, const char *> kinds[] = {
        {"void", "Voids"}, {"discount", "Discounts"}, {"reopen", "Reopened checks"},
        {"transfer", "Transfers"}, {"move", "Table moves"}, {"merge", "Merges"}};
    for (const auto &[kind, label] : kinds) {
        if (const auto it = counts.find(kind); it != counts.end())
            r.line({label, "", "", count(it->second)});
    }
    r.section("Everything, in order");
    for (const Row &row : rows)
        r.line({row.event->what, row.check->label + " #" + std::to_string(row.check->id), row.event->who,
                ctx.clock(row.at)});
    return r;
}

Report serverSales(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "servers";
    r.title = "Server Sales";
    r.subtitle = ctx.period;
    r.columns = {"Server", "Checks", "Guests", "Net sales"};

    struct Tally { std::int64_t checks = 0, guests = 0; Money net; };
    std::map<std::string, Tally> servers;
    for (const Check &c : closed) {
        Tally &t = servers[c.serverName.empty() ? "(unknown)" : c.serverName];
        ++t.checks;
        t.guests += c.guests;
        t.net += c.totals(ctx.settings.tax).subtotal;
    }
    Tally all;
    for (const auto &[name, t] : servers) {
        r.line({name, count(t.checks), count(t.guests), ctx.money(t.net)});
        all.checks += t.checks;
        all.guests += t.guests;
        all.net += t.net;
    }
    if (servers.empty())
        r.note("No sales yet.");
    else
        r.total({"All servers", count(all.checks), count(all.guests), ctx.money(all.net)});
    return r;
}

Report driverReport(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "drivers";
    r.title = "Drivers";
    r.subtitle = ctx.period;
    r.columns = {"Driver", "Deliveries", "Sales", "Fees", "Tips", "Avg out"};
    struct Tally { std::int64_t runs = 0, outMs = 0, timed = 0; Money sales, fees, tips; };
    std::map<std::string, Tally> drivers;
    for (const Check &c : closed) {
        if (c.type != CheckType::Delivery || c.training)
            continue;
        Tally &t = drivers[c.driverName.empty() ? "(not sent out)" : c.driverName];
        ++t.runs;
        const Totals totals = c.totals(ctx.settings.tax);
        t.sales += totals.subtotal;
        t.tips += totals.tips;
        for (const OrderLine &l : c.lines)
            if (l.isFee() && !l.voided)
                t.fees += l.total();
        if (c.outAt && c.deliveredAt > c.outAt) {
            t.outMs += c.deliveredAt - c.outAt;
            ++t.timed;
        }
    }
    const auto minutes = [](const Tally &t) {
        return t.timed ? std::to_string((t.outMs / t.timed + 30'000) / 60'000) + " min" : std::string("-");
    };
    Tally all;
    for (const auto &[name, t] : drivers) {
        r.line({name, count(t.runs), ctx.money(t.sales), ctx.money(t.fees), ctx.money(t.tips), minutes(t)});
        all.runs += t.runs;
        all.sales += t.sales;
        all.fees += t.fees;
        all.tips += t.tips;
        all.outMs += t.outMs;
        all.timed += t.timed;
    }
    if (drivers.empty())
        r.note("No deliveries.");
    else
        r.total({"All drivers", count(all.runs), ctx.money(all.sales), ctx.money(all.fees), ctx.money(all.tips), minutes(all)});
    return r;
}

Report laborReport(const std::vector<TimePunch> &punches, const std::vector<Employee> &employees,
                   const ReportContext &ctx, const std::vector<TimePunch> &earlier, Money netSales)
{
    Report r;
    r.id = "labor";
    r.title = "Labor";
    r.subtitle = ctx.period;
    r.columns = {"Employee", "In", "Out", "Breaks", "Hours"};
    const PosSettings &s = ctx.settings;

    std::map<std::string, std::string> names;
    std::set<std::string> severalJobs;   // whose shifts say which job
    for (const Employee &e : employees) {
        names[e.id] = e.name;
        if (e.jobs().size() > 1)
            severalJobs.insert(e.id);
    }
    auto nameOf = [&](const std::string &id) {
        const auto it = names.find(id);
        return it == names.end() ? id : it->second;
    };

    std::int64_t totalMs = 0;
    std::vector<TimePunch> sorted = punches;
    std::ranges::sort(sorted, {}, &TimePunch::clockIn);
    // Punches a manager changed are marked (*), and the changes listed below.
    std::set<std::int64_t> changed;
    for (const PosSettings::PunchChange &c : s.punchChanges)
        changed.insert(c.punchId);
    for (const TimePunch &p : sorted) {
        const std::int64_t ms = p.workedMs(ctx.now, s.paidBreaks);
        totalMs += ms;
        const std::string out = !p.open() ? ctx.clock(p.clockOut) : p.onBreak() ? "on break" : "on clock";
        r.line({nameOf(p.employeeId) + (p.job.empty() || !severalJobs.contains(p.employeeId) ? "" : " (" + p.job + ")")
                    + (changed.contains(p.id) ? " *" : ""),
                ctx.clock(p.clockIn), out,
                p.breaks.empty() ? "" : hours(p.breakMs(ctx.now)), hours(ms)});
    }
    if (sorted.empty()) {
        r.note("Nobody has clocked in.");
        return r;
    }
    r.total({"Total hours", "", "", "", hours(totalMs)});

    // Hours and overtime per person: daily rule per day worked, weekly rule
    // on the pay week so far; whichever gives more overtime counts.
    const std::int64_t hourMs = 3'600'000;
    struct Person {
        std::map<int, std::int64_t> byDay;
        std::int64_t today = 0;
        double payToday = 0;   // cents: today's shifts at their pay
        std::int64_t otTodayMs = 0;
    };
    std::map<std::string, Person> people;
    auto dayKey = [&](std::int64_t ms) { return ctx.dayOf ? ctx.dayOf(ms) : int(ms / (24 * hourMs)); };
    for (const TimePunch &p : sorted) {
        const std::int64_t ms = p.workedMs(ctx.now, s.paidBreaks);
        people[p.employeeId].today += ms;
        people[p.employeeId].byDay[dayKey(p.clockIn)] += ms;
        people[p.employeeId].payToday += double(ms) / hourMs * double(p.rate.cents());
    }
    for (const TimePunch &p : earlier) {
        if (p.clockIn >= ctx.weekStart && people.contains(p.employeeId))
            people[p.employeeId].byDay[dayKey(p.clockIn)] += p.workedMs(ctx.now, s.paidBreaks);
    }
    r.section("Hours and overtime");
    r.line({"", "Today", "This week", "Regular", "Overtime"});
    for (auto &[id, person] : people) {
        std::int64_t week = 0, dailyOt = 0;
        for (const auto &[day, ms] : person.byDay) {
            week += ms;
            if (s.overtimeDailyHours > 0)
                dailyOt += std::max<std::int64_t>(0, ms - s.overtimeDailyHours * hourMs);
        }
        const auto weeklyOver = [&](std::int64_t ms) {
            return s.overtimeWeeklyHours > 0 ? std::max<std::int64_t>(0, ms - s.overtimeWeeklyHours * hourMs) : 0;
        };
        const std::int64_t weeklyOt = weeklyOver(week);
        const std::int64_t ot = std::max(dailyOt, weeklyOt);
        r.line({nameOf(id), hours(person.today), hours(week), hours(week - ot), ot > 0 ? hours(ot) : "-"});
        // The overtime that falls on today: today's own daily overtime, or
        // what today added to the week's.
        const std::int64_t dailyToday = s.overtimeDailyHours > 0
                                            ? std::max<std::int64_t>(0, person.today - s.overtimeDailyHours * hourMs) : 0;
        person.otTodayMs = std::max(dailyToday, weeklyOt - weeklyOver(week - person.today));
    }
    if (s.overtimeDailyHours == 0 && s.overtimeWeeklyHours == 0)
        r.note("No overtime rule is set (Manager -> Settings).");

    // What today's labor costs: each shift at its pay, overtime at time and
    // a half (the extra half at the average pay of the day).
    double pay = 0, premium = 0;
    bool anyPay = false;
    for (const TimePunch &p : sorted)
        anyPay = anyPay || p.rate.cents() > 0;
    r.section("Labor cost today");
    if (!anyPay) {
        r.note("No pay rates are set (Manager -> Employees -> Pay rate).");
        return r;
    }
    r.line({"", "Hours", "Pay", "Overtime", "Cost"});
    for (const auto &[id, person] : people) {
        const double avg = person.today > 0 ? person.payToday / (double(person.today) / hourMs) : 0;
        const double extra = double(person.otTodayMs) / hourMs * avg * 0.5;
        pay += person.payToday;
        premium += extra;
        r.line({nameOf(id), hours(person.today), ctx.money(Money::fromCents(std::llround(person.payToday))),
                extra > 0 ? ctx.money(Money::fromCents(std::llround(extra))) : "-",
                ctx.money(Money::fromCents(std::llround(person.payToday + extra)))});
    }
    const Money cost = Money::fromCents(std::llround(pay + premium));
    r.total({"Labor cost", hours(totalMs), ctx.money(Money::fromCents(std::llround(pay))),
             premium > 0 ? ctx.money(Money::fromCents(std::llround(premium))) : "-", ctx.money(cost)});
    r.line({"Net sales", "", "", "", ctx.money(netSales)});
    if (netSales.cents() > 0) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.1f%%", 100.0 * double(cost.cents()) / double(netSales.cents()));
        r.line({"Labor % of sales", "", "", "", pct});
    }
    // Changes managers made: to today's punches, or made today.
    std::set<std::int64_t> shown;
    for (const TimePunch &p : sorted)
        shown.insert(p.id);
    bool header = false;
    for (const PosSettings::PunchChange &c : s.punchChanges) {
        if (!shown.contains(c.punchId) && dayKey(c.at) != dayKey(ctx.now))
            continue;
        if (!header) {
            r.section("Changes to time punches (*)");
            r.line({"Employee", "When", "Change", "By", "Why"});
            header = true;
        }
        r.line({c.employee, ctx.clock(c.at), c.what, c.by, c.why});
    }
    return r;
}

Report expensesReport(const std::vector<DrawerSession> &drawers, const ReportContext &ctx)
{
    Report r;
    r.id = "expenses";
    r.title = "Expenses";
    r.subtitle = ctx.period;
    r.columns = {"", "When", "Who", "Amount"};
    struct Row { const CashMovement *m; const DrawerSession *d; };
    std::vector<Row> rows;
    std::map<std::string, Money> byCategory;
    Money total;
    for (const DrawerSession &d : drawers) {
        for (const CashMovement &m : d.movements) {
            if (m.kind != CashMovement::Kind::Payout)
                continue;
            rows.push_back({&m, &d});
            byCategory[m.category.empty() ? "Not sorted" : m.category] += m.amount;
            total += m.amount;
        }
    }
    if (rows.empty()) {
        r.note("No cash was paid out.");
        return r;
    }
    r.section("By category");
    for (const auto &[category, amount] : byCategory)
        r.line({category, "", "", ctx.money(amount)});
    r.total({"Total paid out", "", "", ctx.money(total)});
    r.section("Each one");
    std::ranges::sort(rows, {}, [](const Row &x) { return x.m->at; });
    for (const Row &x : rows) {
        std::string what = x.m->category.empty() ? "Not sorted" : x.m->category;
        if (!x.m->reason.empty())
            what += ": " + x.m->reason;
        r.line({what, ctx.clock(x.m->at), x.m->by, ctx.money(x.m->amount)});
    }
    return r;
}

Report exceptionsReport(const std::vector<const Check *> &checks, const std::vector<DrawerSession> &drawers,
                        const ReportContext &ctx)
{
    Report r;
    r.id = "exceptions";
    r.title = "Exceptions";
    r.subtitle = ctx.period;
    r.columns = {"Employee", "Times", "Amount"};

    struct Tally { std::int64_t times = 0; Money amount; };
    struct Kind { const char *title; std::vector<std::string> kinds; bool money; };
    const Kind kinds[] = {
        {"Voids (items already sent)", {"void"}, true},
        {"Discounts and comps", {"discount"}, true},
        {"Payments and discounts taken back", {"unpay", "undiscount"}, true},
        {"Reopened checks", {"reopen"}, true},
        {"Moved, transferred or merged", {"move", "transfer", "merge"}, false},
    };
    struct Big { Money amount; std::string what; const Check *check; std::string who; };
    std::vector<Big> biggest;
    bool any = false;

    // First, anyone far above everyone else on shift: 3 times or more, and at
    // least 3 times as often as whoever else did it most. (Who was on shift:
    // whoever had checks, or did any of these.)
    {
        std::set<std::string> onShift;
        for (const Check *c : checks)
            if (!c->serverName.empty())
                onShift.insert(c->serverName);
        struct Watch { const char *title; std::vector<std::string> kinds; };
        const Watch watch[] = {
            {"Unusual: voids", {"void"}},
            {"Unusual: discounts and comps", {"discount"}},
            {"Unusual: payments and discounts taken back", {"unpay", "undiscount"}},
            {"Unusual: reopened checks", {"reopen"}},
            {"Unusual: drawer opened with no sale", {"noSale"}},
        };
        bool noted = false;
        for (const Watch &wch : watch) {
            std::map<std::string, std::int64_t> n;
            if (wch.kinds.front() == "noSale") {
                for (const DrawerSession &d : drawers)
                    for (const CashMovement &m : d.movements)
                        if (m.kind == CashMovement::Kind::NoSale && !m.by.empty())
                            ++n[m.by];
            } else {
                for (const Check *c : checks)
                    for (const CheckEvent &e : c->events)
                        if (!e.who.empty() && std::ranges::find(wch.kinds, e.kind) != wch.kinds.end())
                            ++n[e.who];
            }
            for (const auto &[who, times] : n)
                onShift.insert(who);
            std::vector<std::pair<std::string, std::int64_t>> flagged;
            for (const auto &[who, times] : n) {
                std::int64_t othersMost = 0;
                int others = 0;
                for (const std::string &o : onShift)
                    if (o != who) {
                        ++others;
                        othersMost = std::max(othersMost, n.contains(o) ? n.at(o) : 0);
                    }
                if (others > 0 && times >= 3 && times >= 3 * std::max<std::int64_t>(1, othersMost))
                    flagged.push_back({who, othersMost});
            }
            if (flagged.empty())
                continue;
            if (!noted) {
                r.columns = {"Employee", "Times", "Amount"};
                r.note("Far more than anyone else on shift (the third column: the most anyone else did). Often there's a "
                       "good reason; worth a look.");
                noted = true;
            }
            any = true;
            r.section(wch.title);
            for (const auto &[who, othersMost] : flagged)
                r.line({who, count(n.at(who)), "≤ " + std::to_string(othersMost)});
        }
    }

    for (const Kind &k : kinds) {
        std::map<std::string, Tally> byWho;
        for (const Check *c : checks) {
            for (const CheckEvent &e : c->events) {
                if (std::ranges::find(k.kinds, e.kind) == k.kinds.end())
                    continue;
                Tally &t = byWho[e.who.empty() ? std::string("(nobody logged in)") : e.who];
                ++t.times;
                t.amount += e.amount;
                if (k.money && e.amount.cents() != 0)
                    biggest.push_back({e.amount, e.reason.empty() ? e.what : e.what + " (" + e.reason + ")", c, e.who});
            }
        }
        if (byWho.empty())
            continue;
        any = true;
        r.section(k.title);
        std::vector<std::pair<std::string, Tally>> rows(byWho.begin(), byWho.end());
        std::ranges::stable_sort(rows, [](const auto &a, const auto &b) {
            return a.second.amount.cents() != b.second.amount.cents() ? a.second.amount.cents() > b.second.amount.cents()
                                                                      : a.second.times > b.second.times;
        });
        Tally all;
        for (const auto &[who, t] : rows) {
            r.line({who, count(t.times), k.money ? ctx.money(t.amount) : ""});
            all.times += t.times;
            all.amount += t.amount;
        }
        r.total({"All", count(all.times), k.money ? ctx.money(all.amount) : ""});

        // Voids and comps: why, when the screen asked.
        if (k.kinds.front() != "void" && k.kinds.front() != "discount")
            continue;
        std::map<std::string, Tally> byReason;
        bool reasons = false;
        for (const Check *c : checks)
            for (const CheckEvent &e : c->events)
                if (std::ranges::find(k.kinds, e.kind) != k.kinds.end()) {
                    reasons = reasons || !e.reason.empty();
                    Tally &t = byReason[e.reason.empty() ? std::string("(no reason given)") : e.reason];
                    ++t.times;
                    t.amount += e.amount;
                }
        if (!reasons)
            continue;
        r.section(k.kinds.front() == "void" ? "Voids by reason" : "Discounts and comps by reason");
        std::vector<std::pair<std::string, Tally>> why(byReason.begin(), byReason.end());
        std::ranges::stable_sort(why, [](const auto &a, const auto &b) { return a.second.amount.cents() > b.second.amount.cents(); });
        for (const auto &[reason, t] : why)
            r.line({reason, count(t.times), ctx.money(t.amount)});
    }

    std::map<std::string, std::int64_t> noSales;
    for (const DrawerSession &d : drawers)
        for (const CashMovement &m : d.movements)
            if (m.kind == CashMovement::Kind::NoSale)
                ++noSales[m.by.empty() ? std::string("(nobody logged in)") : m.by];
    if (!noSales.empty()) {
        any = true;
        r.section("Drawer opened with no sale");
        std::int64_t all = 0;
        for (const auto &[who, n] : noSales) {
            r.line({who, count(n), ""});
            all += n;
        }
        r.total({"All", count(all), ""});
    }

    if (!any) {
        r.note("No voids, discounts, payments taken back, reopened or moved checks, or drawers opened with no sale.");
        return r;
    }
    if (!biggest.empty()) {
        std::ranges::stable_sort(biggest, [](const Big &a, const Big &b) { return a.amount.cents() > b.amount.cents(); });
        r.section("The biggest ones");
        for (std::size_t i = 0; i < biggest.size() && i < 10; ++i) {
            const Big &b = biggest[i];
            r.line({b.what + " · " + b.check->label + " #" + std::to_string(b.check->id), b.who, ctx.money(b.amount)});
        }
    }
    return r;
}

Report changesReport(const std::vector<Change> &changes, const ReportContext &ctx)
{
    Report r;
    r.id = "changes";
    r.title = "Changes";
    r.subtitle = ctx.period;
    r.columns = {"", "Before", "After"};
    if (changes.empty()) {
        r.note("Nothing was changed on the manager screens, the menu or the pages.");
        return r;
    }
    std::string area;
    for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
        const Change &c = *it;
        const std::string what = c.item.empty() ? c.field
                                 : c.field.empty() ? c.item
                                                   : c.item + " · " + c.field;
        r.line({ctx.clock(c.at) + " · " + c.who + " · " + c.area + (what.empty() ? "" : ": " + what), c.before, c.after});
    }
    r.total({"All", count(std::int64_t(changes.size())), ""});
    return r;
}

Report depositReport(const std::vector<DrawerSession> &drawers, const std::vector<Check> &closed,
                     const std::vector<const Check *> &open, const ReportContext &ctx, const std::vector<Refund> &refunds)
{
    const TaxRates &rates = ctx.settings.tax;
    Report r;
    r.id = "deposit";
    r.title = "Deposit and Balance";
    r.subtitle = ctx.period;
    r.columns = {"", "Amount"};

    // Cash: each drawer (or server bank), counted or still expected.
    r.section("Cash to take to the bank");
    Money deposit, overShort;
    bool uncounted = false;
    for (const DrawerSession &d : drawers) {
        Money cash;
        for (const Check &c : closed)
            if (c.drawerSession == d.id)
                cash += cashIntoDrawer(c, rates);
        const Money inDrawer = d.open() ? d.startingCash + cash + d.movementsTotal() : d.counted;
        if (d.open())
            uncounted = true;
        else
            overShort += d.overShort();
        r.line({d.name + (d.open() ? " (not counted yet: expected)" : " (counted)"), ctx.money(inDrawer)});
        r.line({"  keep as starting cash", ctx.money(-d.startingCash)});
        deposit += inDrawer - d.startingCash;
    }
    if (drawers.empty())
        r.line({"No drawer was opened", ctx.money(Money())});
    r.total({"Cash deposit", ctx.money(deposit)});
    if (uncounted)
        r.note("Some drawers aren't counted yet: their cash is what should be there.");

    // What was sold, and how it was paid.
    Money due, cashNet, cards, cardTips, giftCards, house, other;
    for (const Check &c : closed) {
        const Totals t = c.totals(rates);
        due += t.total + t.rounding;
        cashNet += t.cashNet();
        cardTips += t.tips;
        for (const Payment &p : c.payments) {
            switch (p.kind) {
            case TenderKind::Card: cards += p.amount; break;
            case TenderKind::GiftCard: giftCards += p.amount; break;
            case TenderKind::HouseAccount: house += p.amount; break;
            case TenderKind::Cash:
            case TenderKind::Discount: break;
            }
        }
    }
    r.section("Cards to settle");
    r.line({"Card payments", ctx.money(cards)});
    r.line({"Card tips", ctx.money(cardTips)});
    // Card refunds today (on any day's checks) come off the batch; cash
    // refunds already came out of the drawers.
    Money cardRefunds;
    for (const Refund &x : refunds)
        if (x.method != "cash")
            cardRefunds += x.amount;
    if (cardRefunds.cents() != 0)
        r.line({"Card refunds", ctx.money(-cardRefunds)});
    r.total({"Card batch", ctx.money(cards + cardTips - cardRefunds)});

    r.section("Book balance");
    r.line({"Sold (with tax)", ctx.money(due)});
    r.line({"Cash", ctx.money(cashNet)});
    r.line({"Cards", ctx.money(cards)});
    if (giftCards.cents() != 0)
        r.line({"Gift cards (already paid for)", ctx.money(giftCards)});
    if (house.cents() != 0)
        r.line({"House accounts (to collect)", ctx.money(house)});
    const Money collected = cashNet + cards + giftCards + house;
    r.total({"Collected", ctx.money(collected)});
    const Money diff = collected - due;
    r.total({diff.cents() == 0 ? "Balanced" : diff.cents() > 0 ? "Collected more than sold" : "Collected less than sold",
             ctx.money(diff)});
    if (!drawers.empty() && !uncounted)
        r.line({overShort.cents() < 0 ? "Drawers short" : overShort.cents() > 0 ? "Drawers over" : "Drawers balanced",
                ctx.money(overShort)});
    if (!open.empty()) {
        Money owed;
        for (const Check *c : open)
            owed += c->totals(rates).balance;
        r.line({"Still open: " + count(std::int64_t(open.size())) + " checks", ctx.money(owed)});
    }
    return r;
}

Report drawerReport(const std::vector<DrawerSession> &drawers, const std::vector<Check> &closed,
                    const ReportContext &ctx)
{
    Report r;
    r.id = "drawer";
    r.title = "Drawers";
    r.subtitle = ctx.period;
    r.columns = {"", "Amount"};
    if (drawers.empty()) {
        r.note("No drawer has been opened.");
        return r;
    }
    const TaxRates &rates = ctx.settings.tax;
    for (const DrawerSession &d : drawers) {
        Money cash;
        std::int64_t checks = 0;
        for (const Check &c : closed) {
            if (c.drawerSession == d.id) {
                cash += cashIntoDrawer(c, rates);
                ++checks;
            }
        }
        r.section(d.name);
        r.line({"Opened by " + d.openedBy, ctx.clock(d.openedAt)});
        r.line({"Starting cash", ctx.money(d.startingCash)});
        r.line({"Cash sales (" + count(checks) + ")", ctx.money(cash)});   // (how many checks)
        std::int64_t noSales = 0;
        for (const CashMovement &m : d.movements) {
            if (m.kind == CashMovement::Kind::NoSale) {
                ++noSales;
                continue;
            }
            const std::string what = m.kind == CashMovement::Kind::PaidIn ? "Paid in"
                                     : m.kind == CashMovement::Kind::TipPayout ? "Tips paid out"
                                                                               : "Paid out";
            r.line({what + (m.reason.empty() ? "" : ": " + m.reason), ctx.money(m.effect())});
        }
        if (noSales > 0)
            r.line({"Opened with no sale (" + count(noSales) + ")", ""});
        const Money expected = d.open() ? d.startingCash + cash + d.movementsTotal() : d.expected;
        r.total({"Expected in drawer", ctx.money(expected)});
        if (!d.open()) {
            for (const auto &[cents, n] : d.countedPieces)
                r.line({"   " + std::to_string(n) + " x " + ctx.money(Money::fromCents(cents)),
                        ctx.money(Money::fromCents(cents * n))});
            r.line({"Counted by " + d.closedBy + (d.blind ? " (blind)" : ""), ctx.money(d.counted)});
            const Money diff = d.overShort();
            r.total({diff.cents() < 0 ? "Short" : diff.cents() > 0 ? "Over" : "Balanced", ctx.money(diff)});
        }
    }
    return r;
}

Report tipsReport(const std::map<std::string, TipShare> &shares, const ReportContext &ctx)
{
    Report r;
    r.id = "tips";
    r.title = "Tips";
    r.subtitle = ctx.period;
    const bool outs = !ctx.settings.tipOuts.empty(), pool = !ctx.settings.tipPool.empty();
    const bool cash = std::ranges::any_of(shares, [](const auto &x) { return x.second.cash.cents() != 0; });
    // The columns that mean something here: cash told, tip-outs, the pool.
    const auto row = [&](const std::string &name, const TipShare &x) {
        std::vector<std::string> cells{name, ctx.money(x.tips)};
        if (cash) cells.push_back(ctx.money(x.cash));
        cells.push_back(ctx.money(x.gratuity));
        if (outs) cells.push_back(ctx.money(x.tipOut));
        if (pool) cells.push_back(ctx.money(x.toPool));
        if (outs || pool) cells.push_back(ctx.money(x.fromPool));
        cells.push_back(ctx.money(x.paid));
        cells.push_back(ctx.money(x.owed()));
        return cells;
    };
    r.columns = {outs || pool ? "Staff" : "Server", "Card tips"};
    if (cash) r.columns.push_back("Cash tips");
    r.columns.push_back("Gratuity");
    if (outs) r.columns.push_back("Tipped out");
    if (pool) r.columns.push_back("Into pool");
    if (outs || pool) r.columns.push_back("From pools");
    r.columns.push_back("Paid out");
    r.columns.push_back("Owed");
    TipShare all;
    for (const auto &[id, x] : shares) {
        if (x.earned.cents() == 0 && x.fromPool.cents() == 0 && x.paid.cents() == 0)
            continue;
        r.line(row(x.name, x));
        all.tips += x.tips;
        all.cash += x.cash;
        all.gratuity += x.gratuity;
        all.earned += x.earned;
        all.tipOut += x.tipOut;
        all.toPool += x.toPool;
        all.fromPool += x.fromPool;
        all.paid += x.paid;
    }
    if (all.earned.cents() == 0 && all.paid.cents() == 0) {
        r.note("No tips yet.");
        return r;
    }
    r.total(row("All staff", all));
    if (pool) {
        std::string jobs;
        for (const auto &[role, share] : ctx.settings.tipPool)
            jobs += (jobs.empty() ? "" : ", ") + role + "s " + std::to_string(share) + "%";
        r.note("Tip pool: everyone in it puts in their tips and takes out by hours worked times their share (" + jobs + ").");
    }
    if (cash)
        r.note("Cash tips: told at clock out; already in their pockets, so counted as paid out.");
    for (const PosSettings::TipOut &t : ctx.settings.tipOuts)
        r.note("Tip-out: " + std::to_string(t.percentBp / 100) + (t.percentBp % 100 ? "." + std::to_string(t.percentBp % 100) : "")
               + "% of " + (t.basis == "sales" ? "sales" : "tips") + " to " + t.role + "s, split by hours.");
    return r;
}

std::map<std::string, TipShare> tipShares(const std::vector<Check> &closed, const std::vector<DrawerSession> &drawers,
                                          const PosSettings &settings, const std::vector<Employee> &employees,
                                          const std::map<std::string, double> &hours,
                                          const std::map<std::string, Money> &cashTips)
{
    std::map<std::string, TipShare> out;
    std::map<std::string, Money> sales;
    for (const Check &c : closed) {
        const Totals t = c.totals(settings.tax);
        TipShare &s = out[c.serverId];
        s.name = c.serverName;
        s.tips += t.tips + c.cashTip;   // card tips, and cash ones handed to whoever took the payment
        s.gratuity += t.gratuity;
        s.earned += t.tips + t.gratuity + c.cashTip;
        sales[c.serverId] += t.subtotal;
    }
    std::map<std::string, const Employee *> byId;
    for (const Employee &e : employees) {
        byId[e.id] = &e;
        if (out.contains(e.id))
            out[e.id].name = e.name;
    }
    const auto roleOf = [&](const std::string &id) { return byId.contains(id) ? byId[id]->role : std::string(); };
    // Cash tips told at clock out: theirs, and already in their pocket.
    for (const auto &[id, cash] : cashTips) {
        if (cash.cents() <= 0)
            continue;
        TipShare &s = out[id];
        if (s.name.empty() && byId.contains(id))
            s.name = byId[id]->name;
        s.cash += cash;
        s.earned += cash;
        s.paid += cash;
    }

    for (const PosSettings::TipOut &rule : settings.tipOuts) {
        // Who shares this pool: that role, worked today.
        std::vector<std::pair<std::string, double>> crew;
        double crewHours = 0;
        for (const auto &[id, h] : hours) {
            if (h > 0 && roleOf(id) == rule.role) {
                crew.emplace_back(id, h);
                crewHours += h;
            }
        }
        if (crew.empty())
            continue;   // nobody to tip out to today
        Money pool;
        for (auto &[id, s] : out) {
            if (roleOf(id) == rule.role)
                continue;
            const Money base = rule.basis == "sales" ? sales[id] : s.earned;
            Money share = base.percent(rule.percentBp);
            const Money left = s.earned - s.tipOut;
            if (share > left)
                share = left;   // never more than they took in
            if (share.cents() <= 0)
                continue;
            s.tipOut += share;
            pool += share;
        }
        // Split by hours; the cents left over go to whoever worked longest.
        Money given;
        std::string longest;
        double most = -1;
        for (const auto &[id, h] : crew) {
            const Money part = Money::fromCents(std::int64_t(double(pool.cents()) * h / crewHours));
            TipShare &s = out[id];
            if (s.name.empty() && byId.contains(id))
                s.name = byId[id]->name;
            s.fromPool += part;
            given += part;
            if (h > most) {
                most = h;
                longest = id;
            }
        }
        out[longest].fromPool += pool - given;
    }

    // The tip pool: everyone in it who worked today puts in their tips (after
    // tip-outs) and takes out by hours times their job's share. Someone's
    // own setting wins: "in" (a full share) or "out" (keeps their tips).
    if (!settings.tipPool.empty()) {
        const auto shareOf = [&](const std::string &id) {
            const std::string own = byId.contains(id) ? byId[id]->tipPool : std::string();
            if (own == "out")
                return 0;
            if (own == "in")
                return 100;
            const auto it = settings.tipPool.find(roleOf(id));
            return it == settings.tipPool.end() ? 0 : it->second;
        };
        std::vector<std::pair<std::string, double>> members;   // id, hours x share
        double weight = 0;
        for (const auto &[id, h] : hours) {
            if (h <= 0 || shareOf(id) <= 0)
                continue;
            members.emplace_back(id, h * shareOf(id));
            weight += h * shareOf(id);
        }
        if (!members.empty() && weight > 0) {
            Money pool;
            for (const auto &[id, w] : members) {
                TipShare &s = out[id];
                if (s.name.empty() && byId.contains(id))
                    s.name = byId[id]->name;
                const Money in = s.earned - s.tipOut;
                if (in.cents() > 0) {
                    s.toPool += in;
                    pool += in;
                }
            }
            Money given;
            std::string most;
            double heaviest = -1;
            for (const auto &[id, w] : members) {
                const Money part = Money::fromCents(std::int64_t(double(pool.cents()) * w / weight));
                out[id].fromPool += part;
                given += part;
                if (w > heaviest) {
                    heaviest = w;
                    most = id;
                }
            }
            out[most].fromPool += pool - given;   // the cents left over
        }
    }
    for (const DrawerSession &d : drawers) {
        for (const CashMovement &m : d.movements) {
            if (m.kind == CashMovement::Kind::TipPayout) {
                TipShare &s = out[m.employeeId];
                if (s.name.empty())
                    s.name = byId.contains(m.employeeId) ? byId[m.employeeId]->name : m.employeeId;
                s.paid += m.amount;
            }
        }
    }
    return out;
}

Report accountsReport(const std::vector<GiftCard> &cards, const std::vector<CustomerRecord> &customers,
                      std::int64_t since, const ReportContext &ctx)
{
    Report r;
    r.id = "accounts";
    r.title = "Gift Cards & Accounts";
    r.subtitle = ctx.period;
    r.columns = {"", "Count", "Amount"};

    std::int64_t sold = 0, spent = 0, live = 0;
    Money soldAmount, spentAmount, outstanding;
    for (const GiftCard &g : cards) {
        for (const LedgerEntry &e : g.history) {
            if (e.at < since)
                continue;
            if (e.kind == "sale") {
                ++sold;
                soldAmount += e.amount;
            } else if (e.kind == "reopen") {   // a sale taken back
                --sold;
                soldAmount += e.amount;
            } else if (e.kind == "spend") {
                ++spent;
                spentAmount -= e.amount;
            } else if (e.kind == "refund") {   // a card payment removed
                --spent;
                spentAmount -= e.amount;
            }
        }
        if (g.balance.cents() > 0) {
            ++live;
            outstanding += g.balance;
        }
    }
    r.section("Gift cards");
    r.line({"Sold and reloaded", count(sold), ctx.money(soldAmount)});
    r.line({"Spent", count(spent), ctx.money(spentAmount)});
    r.total({"Still on cards (owed by the store)", count(live), ctx.money(outstanding)});

    r.section("House accounts");
    Money charged, paid, owed;
    std::vector<const CustomerRecord *> owing;
    for (const CustomerRecord &c : customers) {
        for (const LedgerEntry &e : c.account) {
            if (e.at < since)
                continue;
            if (e.kind == "payment")
                paid -= e.amount;
            else
                charged += e.amount;   // charges, less any taken back
        }
        if (c.houseAccount && c.accountBalance.cents() != 0) {
            owing.push_back(&c);
            owed += c.accountBalance;
        }
    }
    r.line({"Charged", "", ctx.money(charged)});
    r.line({"Paid", "", ctx.money(paid)});
    std::ranges::sort(owing, [](const CustomerRecord *a, const CustomerRecord *b) {
        return a->accountBalance > b->accountBalance;
    });
    for (const CustomerRecord *c : owing)
        r.line({(c->name.empty() ? c->phone : c->name) + (c->accountLimit.cents() > 0 && c->accountBalance >= c->accountLimit
                                                             ? "  (at the limit)" : ""),
                "", ctx.money(c->accountBalance)});
    r.total({"Owed to the store", count(std::int64_t(owing.size())), ctx.money(owed)});
    return r;
}

Report kitchenReport(const std::vector<const Check *> &checks, int lateMinutes, const ReportContext &ctx)
{
    Report r;
    r.id = "kitchen";
    r.title = "Kitchen Times";
    r.subtitle = ctx.period;
    r.columns = {"", "Tickets", "Average", "Longest", "Late"};

    // A ticket: one send from one check to one station.
    struct Ticket { const Check *check; std::int64_t sentAt; std::string station; std::int64_t madeAt = 0; bool done = true; };
    std::map<std::tuple<std::int64_t, std::int64_t, std::string>, Ticket> tickets;
    for (const Check *c : checks) {
        for (const OrderLine &l : c->lines) {
            if (!l.sent || l.voided || l.isComment() || l.isGiftCard())
                continue;
            const std::string station = l.printer.empty() ? "kitchen" : l.printer;
            Ticket &t = tickets.try_emplace({c->id, l.sentAt, station}, Ticket{c, l.sentAt, station}).first->second;
            if (l.made)
                t.madeAt = std::max(t.madeAt, l.madeAt);
            else
                t.done = false;
        }
    }
    const auto minutes = [](std::int64_t ms) {
        const std::int64_t s = ms / 1000;
        return std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
    };
    struct Times { std::int64_t count = 0, total = 0, longest = 0, late = 0, waiting = 0; };
    std::map<std::string, Times> byStation;
    std::vector<const Ticket *> made;
    for (const auto &[key, t] : tickets) {
        Times &s = byStation[t.station];
        if (!t.done || t.madeAt == 0) {
            ++s.waiting;
            continue;
        }
        const std::int64_t took = t.madeAt - t.sentAt;
        ++s.count;
        s.total += took;
        s.longest = std::max(s.longest, took);
        if (took > std::int64_t(lateMinutes) * 60'000)
            ++s.late;
        made.push_back(&t);
    }
    if (byStation.empty()) {
        r.note("No orders have gone to the kitchen yet.");
        return r;
    }
    r.section("By station (sent to made)");
    Times all;
    for (const auto &[station, s] : byStation) {
        std::string name = station;
        if (!name.empty())
            name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
        r.line({name, count(s.count), s.count ? minutes(s.total / s.count) : "-", s.count ? minutes(s.longest) : "-",
                count(s.late)});
        all.count += s.count;
        all.total += s.total;
        all.longest = std::max(all.longest, s.longest);
        all.late += s.late;
        all.waiting += s.waiting;
    }
    r.total({"All", count(all.count), all.count ? minutes(all.total / all.count) : "-",
             all.count ? minutes(all.longest) : "-", count(all.late)});
    if (all.waiting)
        r.note(std::to_string(all.waiting) + (all.waiting == 1 ? " ticket is" : " tickets are") + " still being made.");
    r.note("Late: over " + std::to_string(lateMinutes) + " minutes (Store Settings).");

    std::ranges::sort(made, [](const Ticket *a, const Ticket *b) { return a->madeAt - a->sentAt > b->madeAt - b->sentAt; });
    if (!made.empty()) {
        r.section("Slowest tickets");
        for (std::size_t i = 0; i < std::min<std::size_t>(5, made.size()); ++i) {
            const Ticket *t = made[i];
            r.line({t->check->label + " #" + std::to_string(t->check->id) + " (" + t->station + ")", "",
                    ctx.clock(t->sentAt), minutes(t->madeAt - t->sentAt), t->check->rush ? "rush" : ""});
        }
    }
    return r;
}

namespace {
// "$1,234.50", "-$3.00", "12", "70.4%" -> the number; nullopt if none.
std::optional<double> numberIn(const std::string &text)
{
    std::string digits;
    bool any = false;
    for (char ch : text) {
        if ((ch >= '0' && ch <= '9') || ch == '.') {
            digits += ch;
            any = any || ch != '.';
        } else if (ch == '-' && digits.empty()) {
            digits += ch;
        }
    }
    if (!any)
        return std::nullopt;
    try {
        return std::stod(digits);
    } catch (...) {
        return std::nullopt;
    }
}
} // namespace

Report compareReports(const Report &now, const Report &before, const std::string &beforeLabel)
{
    Report r = now;
    if (r.columns.empty())
        r.columns = {"", "Amount"};
    r.columns.push_back(beforeLabel);
    r.columns.push_back("Change");
    for (ReportRow &row : r.rows) {
        if (row.kind != ReportRow::Kind::Line && row.kind != ReportRow::Kind::Total)
            continue;
        if (row.cells.size() < 2)
            row.cells.resize(2);
        std::string was = "-";
        for (const ReportRow &old : before.rows) {
            if (old.kind == row.kind && !old.cells.empty() && old.cells.front() == row.cells.front()) {
                was = old.cells.back();
                break;
            }
        }
        // Rows line up with the value columns: pad short ones.
        while (row.cells.size() < now.columns.size())
            row.cells.push_back("");
        std::string change;
        const auto a = numberIn(row.cells.back());
        const auto b = numberIn(was);
        if (a && b && *b != 0) {
            const double pct = (*a - *b) / std::abs(*b) * 100.0;
            char buf[32];
            std::snprintf(buf, sizeof buf, "%+.1f%%", pct);
            change = buf;
        } else if (a && b && *a == *b) {
            change = "0.0%";
        } else if (a && was != "-" && b && *b == 0) {
            change = "new";
        }
        row.cells.push_back(was);
        row.cells.push_back(change.empty() ? "-" : change);
    }
    r.subtitle = now.subtitle + " vs " + beforeLabel;
    return r;
}

Report tableTurns(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "turns";
    r.title = "Table Turns";
    r.subtitle = ctx.period;
    r.columns = {"", "Checks", "Avg minutes", "Avg check", "Per guest"};
    struct Tally { std::int64_t checks = 0, minutes = 0, guests = 0; Money total; };
    std::map<int, Tally> bySize;   // 1-2, 3-4, 5-6, 7+ (by the first of each)
    std::map<std::string, Tally> byTable;
    Tally all;
    for (const Check &c : closed) {
        if (c.type != CheckType::DineIn || c.closedAt <= c.openedAt)
            continue;
        const std::int64_t minutes = (c.closedAt - c.openedAt) / 60'000;
        const Money total = c.totals(ctx.settings.tax).total;
        const int band = c.guests <= 2 ? 1 : c.guests <= 4 ? 3 : c.guests <= 6 ? 5 : 7;
        for (Tally *t : {&bySize[band], &byTable[c.label], &all}) {
            ++t->checks;
            t->minutes += minutes;
            t->guests += std::max(1, c.guests);
            t->total += total;
        }
    }
    if (all.checks == 0) {
        r.note("No dine-in checks closed yet.");
        return r;
    }
    const auto line = [&](const std::string &label, const Tally &t, bool total) {
        std::vector<std::string> cells{label, count(t.checks), std::to_string(t.minutes / t.checks),
                                       ctx.money(Money::fromCents(t.total.cents() / t.checks)),
                                       ctx.money(Money::fromCents(t.total.cents() / std::max<std::int64_t>(1, t.guests)))};
        if (total)
            r.total(cells);
        else
            r.line(cells);
    };
    r.section("By party size");
    for (const auto &[band, t] : bySize) {
        const std::string label = band == 1 ? "1 - 2 guests" : band == 3 ? "3 - 4 guests" : band == 5 ? "5 - 6 guests" : "7 or more";
        line(label, t, false);
    }
    r.section("By table");
    for (const auto &[table, t] : byTable)
        line(table, t, false);
    line("All tables", all, true);
    return r;
}

Report customersReport(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "customers";
    r.title = "Customers";
    r.subtitle = ctx.period;
    r.columns = {"Customer · orders most", "Visits", "Spent", "Average", "Last visit"};
    struct Tally {
        std::string name;
        std::int64_t visits = 0, last = 0;
        Money spent;
        std::map<std::string, std::int64_t> items;
    };
    std::map<std::string, Tally> byCustomer;   // their id, else name + phone
    for (const Check &c : closed) {
        if (c.training || (c.customerId.empty() && c.customer.name.empty() && c.customer.phone.empty()))
            continue;
        const std::string key = !c.customerId.empty() ? c.customerId : c.customer.name + "|" + c.customer.phone;
        Tally &t = byCustomer[key];
        if (t.name.empty())
            t.name = !c.customer.name.empty() ? c.customer.name : c.customer.phone;
        ++t.visits;
        t.spent += c.totals(ctx.settings.tax).total;
        t.last = std::max(t.last, c.closedAt);
        for (const OrderLine &l : c.lines)
            if (!l.voided && !l.isComment())
                t.items[l.name] += l.counted();
    }
    if (byCustomer.empty()) {
        r.note("No checks with a customer on them (takeout, delivery, or Check... -> Customer).");
        return r;
    }
    std::vector<const Tally *> rows;
    for (const auto &[key, t] : byCustomer)
        rows.push_back(&t);
    std::ranges::stable_sort(rows, [](const Tally *a, const Tally *b) { return a->spent.cents() > b->spent.cents(); });
    r.section("Best customers first");
    std::int64_t visits = 0;
    Money spent;
    for (const Tally *t : rows) {
        std::string favorite;
        std::int64_t most = 0;
        for (const auto &[item, n] : t->items)
            if (n > most) {
                most = n;
                favorite = item;
            }
        r.line({favorite.empty() ? t->name : t->name + "  ·  " + favorite, count(t->visits), ctx.money(t->spent),
                ctx.money(t->spent.scaled(1, std::max<std::int64_t>(1, t->visits))),
                ctx.date ? ctx.date(t->last) : ctx.clock(t->last)});
        visits += t->visits;
        spent += t->spent;
    }
    r.total({count(std::int64_t(rows.size())) + " customers", count(visits), ctx.money(spent),
             ctx.money(spent.scaled(1, std::max<std::int64_t>(1, visits))), ""});
    return r;
}

Report royaltyReport(const std::vector<Check> &closed, const ReportContext &ctx)
{
    Report r;
    r.id = "royalty";
    r.title = "Royalty";
    r.subtitle = ctx.period;
    r.columns = {"", "Amount"};
    Money gross, discounts, net;
    std::int64_t checks = 0;
    for (const Check &c : closed) {
        if (c.training)
            continue;
        const Totals t = c.totals(ctx.settings.tax);
        gross += t.items;
        discounts += t.discounts;
        net += t.subtotal;
        ++checks;
        for (const OrderLine &l : c.lines)   // gift cards sold aren't sales yet
            if (l.isGiftCard() && !l.voided)
                net -= l.total();
    }
    const auto pct = [](std::int64_t bp) {
        return std::to_string(bp / 100) + (bp % 100 ? "." + std::to_string(100 + bp % 100).substr(1) : "") + "%";
    };
    r.section("Sales");
    r.line({"Checks", count(checks)});
    r.line({"Item sales", ctx.money(gross)});
    r.line({"Discounts & comps", ctx.money(-discounts)});
    r.total({"Net sales (no tax, no gift cards sold)", ctx.money(net)});
    r.section("Owed to the franchise");
    const Money royalty = net.percent(ctx.settings.royaltyBp), adFund = net.percent(ctx.settings.adFundBp);
    r.line({"Royalty (" + pct(ctx.settings.royaltyBp) + ")", ctx.money(royalty)});
    r.line({"Advertising fund (" + pct(ctx.settings.adFundBp) + ")", ctx.money(adFund)});
    r.total({"Total", ctx.money(royalty + adFund)});
    if (ctx.settings.royaltyBp == 0 && ctx.settings.adFundBp == 0)
        r.note("Set the percents in Manager -> Store Settings -> Royalty.");
    return r;
}

Report accountingReport(const std::vector<Check> &closed, const std::map<std::string, std::string> &familyOf,
                        const ReportContext &ctx)
{
    Report r;
    r.id = "accounting";
    r.title = "Accounting";
    r.subtitle = ctx.period;
    r.columns = {"Account", "Debit", "Credit"};
    std::map<std::string, Money> debit, credit;   // by key
    std::map<std::string, std::string> tenderNames;
    for (const Check &c : closed) {
        if (c.training)
            continue;
        const Totals t = c.totals(ctx.settings.tax);
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment())
                continue;
            if (l.isGiftCard()) {
                credit["giftCardsSold"] += l.total();
                continue;
            }
            const auto f = familyOf.find(l.itemId);
            credit["sales:" + (f == familyOf.end() || f->second.empty() ? std::string("other") : f->second)] += l.total();
        }
        for (const auto &[cls, amount] : t.taxByClass)
            credit["tax:" + toString(cls)] += amount;
        credit["gratuity"] += t.gratuity;
        credit["tips"] += t.tips;
        credit["rounding"] += t.rounding;
        debit["discounts"] += t.discounts - t.staffMeals;
        debit["staffMeals"] += t.staffMeals;
        bool cashBooked = false;
        for (const Payment &p : c.payments) {
            tenderNames[p.tenderId] = p.tenderName;
            const std::string key = "tender:" + p.tenderId;
            switch (p.kind) {
            case TenderKind::Cash:
                if (!cashBooked)   // cash kept: tendered less change, once a check
                    debit[key] += t.cashNet();
                cashBooked = true;
                break;
            case TenderKind::Card: debit[key] += p.amount + p.tip; break;
            case TenderKind::GiftCard:
            case TenderKind::HouseAccount: debit[key] += p.amount; break;
            case TenderKind::Discount: break;
            }
        }
    }
    // The store's account for a key, else the general one ("sales" for
    // "sales:burgers"), else the key itself.
    const auto account = [&](const std::string &key) {
        const auto &chart = ctx.settings.accounts;
        if (const auto it = chart.find(key); it != chart.end())
            return it->second;
        if (const auto colon = key.find(':'); colon != std::string::npos)
            if (const auto it = chart.find(key.substr(0, colon)); it != chart.end())
                return it->second + " (" + key.substr(colon + 1) + ")";
        if (key.starts_with("tender:") && tenderNames.contains(key.substr(7)))
            return tenderNames.at(key.substr(7));
        return key;
    };
    // Accounts that share a number (sales:burgers and sales:salads both
    // "4000 Food sales") add up on one line.
    std::map<std::string, std::pair<Money, Money>> lines;
    for (const auto &[k, m] : debit)
        if (m.cents() != 0)
            lines[account(k)].first += m;
    for (const auto &[k, m] : credit)
        if (m.cents() != 0)
            lines[account(k)].second += m;
    if (lines.empty()) {
        r.note("No sales in this period.");
        return r;
    }
    Money debits, credits;
    r.section("Journal");
    for (const auto &[name, dc] : lines) {
        // A negative amount goes on the other side (rounding given back).
        Money d = dc.first, c = dc.second;
        if (d.cents() < 0) { c -= d; d = Money(); }
        if (c.cents() < 0) { d -= c; c = Money(); }
        r.line({name, d.cents() ? ctx.money(d) : "", c.cents() ? ctx.money(c) : ""});
        debits += d;
        credits += c;
    }
    r.total({"Total", ctx.money(debits), ctx.money(credits)});
    if (debits != credits)
        r.note("Off by " + ctx.money(debits - credits) + ": a check may have been closed with a balance.");
    return r;
}

} // namespace vt::core
