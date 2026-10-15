#pragma once

#include "core/check.hh"
#include "core/customer.hh"
#include "core/day.hh"
#include "core/employee.hh"
#include "core/menu.hh"
#include "core/settings.hh"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vt::core {

// A report as plain rows, rendered by the reportView widget on screen and by
// the ticket formatter on paper (legacy Report / ReportEntry).
struct ReportRow {
    enum class Kind { Line, Section, Total, Note };
    Kind kind = Kind::Line;
    std::vector<std::string> cells;   // first cell left-aligned, the rest right

    bool operator==(const ReportRow &) const = default;
};

struct Report {
    std::string id;
    std::string title;
    std::string subtitle;
    std::vector<std::string> columns;
    std::vector<ReportRow> rows;

    void section(const std::string &title) { rows.push_back({ReportRow::Kind::Section, {title}}); }
    void line(std::vector<std::string> cells) { rows.push_back({ReportRow::Kind::Line, std::move(cells)}); }
    void total(std::vector<std::string> cells) { rows.push_back({ReportRow::Kind::Total, std::move(cells)}); }
    void note(const std::string &text) { rows.push_back({ReportRow::Kind::Note, {text}}); }
    // First line whose first cell is `label`; empty if none. For tests.
    std::vector<std::string> find(const std::string &label) const;
};

struct ReportContext {
    const PosSettings &settings;
    std::string period;                                // "Today", "Tue Sep 29"...
    std::function<std::string(std::int64_t)> clock;   // epoch ms -> "10:31 AM"
    std::int64_t now = 0;
    std::function<int(std::int64_t)> hourOf;          // epoch ms -> local hour 0-23
    std::function<int(std::int64_t)> dayOf;           // epoch ms -> local day (any number per date)
    std::int64_t weekStart = 0;                       // start of the current pay week
    std::function<std::string(std::int64_t)> date;    // epoch ms -> "Oct 5" (empty: the clock)

    std::string money(Money m) const;
};

// Net cash a closed check left in its drawer.
Money cashIntoDrawer(const Check &check, const TaxRates &rates);
// Starting bank + net cash of the checks closed into this drawer + pay-ins
// - pay-outs.
Money expectedCash(const DrawerSession &drawer, const std::vector<Check> &closed, const TaxRates &rates);

// Card tips and gratuity an employee earned on these checks, less the tips
// already paid out to them from any drawer.
Money tipsOwed(const std::string &employeeId, const std::vector<Check> &closed,
               const std::vector<DrawerSession> &drawers, const TaxRates &rates);

// Everyone's tips after tip-outs: what they earned on their checks, what
// they tipped out (PosSettings::tipOuts), what they put in the tip pool and
// their share of the pools (by hours worked, `hours`: employee id -> hours
// today), what was paid out. Cash tips they told at clock out (`cashTips`)
// count as earned and as paid (they kept them).
struct TipShare {
    std::string name;
    Money tips, gratuity, cash;   // card tips and party gratuity on their checks; cash tips told
    Money earned, tipOut, toPool, fromPool, paid;
    Money owed() const { return earned - tipOut - toPool + fromPool - paid; }
};
std::map<std::string, TipShare> tipShares(const std::vector<Check> &closed, const std::vector<DrawerSession> &drawers,
                                          const PosSettings &settings, const std::vector<Employee> &employees,
                                          const std::map<std::string, double> &hours,
                                          const std::map<std::string, Money> &cashTips = {});

// Refunds: the day's (given), else those on these checks (a range).
Report salesSummary(const std::vector<Check> &closed, const ReportContext &ctx,
                    const std::vector<Refund> *refunds = nullptr);
Report itemSales(const std::vector<Check> &closed, const std::vector<MenuItem> &menu, const ReportContext &ctx);
Report serverSales(const std::vector<Check> &closed, const ReportContext &ctx);
// Deliveries by driver: how many, their sales, delivery fees, tips, and how
// long they were out on average.
Report driverReport(const std::vector<Check> &closed, const ReportContext &ctx);
// Today's punches with breaks and worked hours, then each person's hours
// today and this pay week with overtime (`earlier`: this week's punches
// from earlier days), then what today's labor costs (each shift at the pay
// it was clocked in at, overtime at time and a half) against `netSales`.
// Overtime as a state's law has it: hours over `dailyHours` in a day at time
// and a half, over `doubleDailyHours` at double time; the seventh day in a
// row of a work week (California): its first 8 hours at time and a half,
// the rest double. Then regular hours (only those: nothing counts twice)
// over `weeklyHours` in the week at time and a half. 0: no such rule.
struct OvertimeRules {
    int dailyHours = 0;
    int doubleDailyHours = 0;
    int weeklyHours = 40;
    bool seventhDay = false;
};
struct HoursSplit {
    std::int64_t regular = 0, overtime = 0, doubleTime = 0;   // ms
    HoursSplit operator-(const HoursSplit &o) const
    {
        return {regular - o.regular, overtime - o.overtime, doubleTime - o.doubleTime};
    }
};
// One person's work week, a day at a time (day 0 is the week's first; days
// not worked are 0).
HoursSplit splitHours(const std::vector<std::int64_t> &daysMs, const OvertimeRules &rules);

Report laborReport(const std::vector<TimePunch> &punches, const std::vector<Employee> &employees,
                   const ReportContext &ctx, const std::vector<TimePunch> &earlier = {}, Money netSales = {});
Report drawerReport(const std::vector<DrawerSession> &drawers, const std::vector<Check> &closed,
                    const ReportContext &ctx);
// Cash paid out of drawers and banks for expenses: by category, then each one.
Report expensesReport(const std::vector<DrawerSession> &drawers, const ReportContext &ctx);
// Net sales by the hour checks closed in.
Report hourlySales(const std::vector<Check> &closed, const ReportContext &ctx);
// Net sales per family (category), with its share of the day.
Report categorySales(const std::vector<Check> &closed, const std::vector<MenuItem> &menu, const ReportContext &ctx);
// Every void, discount, reopen, transfer, move and merge, with who and when.
Report auditReport(const std::vector<const Check *> &checks, const ReportContext &ctx);
// The original's exception reports, by employee: voids, discounts and comps,
// payments taken back, reopened checks, moved / transferred / merged checks,
// and drawers opened with no sale (from `drawers`; none over a date range).
// What was changed on the manager screens, the Menu Builder and the pages, newest first.
Report changesReport(const std::vector<Change> &changes, const ReportContext &ctx);
Report exceptionsReport(const std::vector<const Check *> &checks, const std::vector<DrawerSession> &drawers,
                        const ReportContext &ctx, const std::vector<Refund> &refunds = {});
// The original's Deposit / Book Balance: the cash to take to the bank (each
// drawer counted, less the starting cash kept for tomorrow), card payments
// to settle, and whether what was collected matches what was sold.
// A house account paid (from the customers' accounts), for the day's books.
struct AccountPayment {
    std::string customer;
    Money amount;
    std::string method;   // "cash" (in a drawer as a paid in) or "card"
    std::int64_t at = 0;
};
// The day's money outside its checks: the Accounting journal's other entries.
struct DayMoney {
    const std::vector<DrawerSession> *drawers = nullptr;
    std::vector<Refund> refunds;
    std::vector<AccountPayment> accountPayments;
};

Report depositReport(const std::vector<DrawerSession> &drawers, const std::vector<Check> &closed,
                     const std::vector<const Check *> &open, const ReportContext &ctx,
                     const std::vector<Refund> &refunds = {}, const std::vector<AccountPayment> &accountPayments = {});
// Gift cards sold and spent since `since`, what is still on cards (owed by
// the store), and house account charges, payments and balances.
Report accountsReport(const std::vector<GiftCard> &cards, const std::vector<CustomerRecord> &customers,
                      std::int64_t since, const ReportContext &ctx);
// Kitchen ticket times by station: from sent to made (bumped) - how many,
// the average, the longest, how many were late - and the slowest tickets.
Report kitchenReport(const std::vector<const Check *> &checks, int lateMinutes, const ReportContext &ctx);
Report tipsReport(const std::map<std::string, TipShare> &shares, const ReportContext &ctx);
// Dine-in table turns: how long checks were open (seated to paid), by
// party size and by table, with the average check and per guest.
Report tableTurns(const std::vector<Check> &closed, const ReportContext &ctx);
// The original's Customer Detail: each customer's visits, spending, average,
// last visit and most-ordered item over the period (best customers first).
Report customersReport(const std::vector<Check> &closed, const ReportContext &ctx);
// The original's Royalty: the franchise fee and advertising fund on net sales.
Report royaltyReport(const std::vector<Check> &closed, const ReportContext &ctx);
// The sales as a balanced journal by account (Chart of Accounts): its CSV
// goes to QuickBooks or an accountant. `familyOf`: menu item id -> family.
// With `day`, the money outside the checks too (refunds, paid outs and ins,
// tips paid from drawers, house account payments, drawers over or short).
Report accountingReport(const std::vector<Check> &closed, const std::map<std::string, std::string> &familyOf,
                        const ReportContext &ctx, const DayMoney *day = nullptr);
// `now` with two more columns: the same row's last value in `before`, and
// the change in percent (rows matched by kind and first cell).
Report compareReports(const Report &now, const Report &before, const std::string &beforeLabel);

} // namespace vt::core
