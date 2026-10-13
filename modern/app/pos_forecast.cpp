// PosService: what a coming day will probably bring, from the same weekday in
// past weeks (the most recent count more): guests, checks and sales, by hour;
// how many of each item; what the kitchen will use of each ingredient, beside
// what's on hand; and what to order from each vendor.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QPointer>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <map>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr int kWeeksBack = 6;   // looked at
constexpr int kWeeksUsed = 4;   // used: the most recent with sales
// A check opened after midnight belongs to the night before (a bar's day).
constexpr std::int64_t kDayStartsMs = 4 * 3600 * 1000;

QDate dayOf(const Check &c)
{
    return QDateTime::fromMSecsSinceEpoch(c.openedAt - kDayStartsMs).date();
}

QString amountText(double v)
{
    return std::abs(v - std::round(v)) < 0.005 ? QString::number(std::llround(v)) : QString::number(v, 'f', 1);
}

// What one past day did.
struct DaySample {
    int guests = 0;
    int checks = 0;
    Money sales;
    std::map<int, std::pair<int, Money>> byHour;   // hour -> checks, sales
    std::map<std::string, int> items;              // item id -> sold
    std::map<std::string, double> used;            // ingredient id -> used
};

} // namespace

bool PosService::requestForecast(int daysAhead)
{
    if (!require(perm::Manager, tr("Reports")))
        return false;
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    const QDate target = today.addDays(std::clamp(daysAhead, 0, 6));
    const auto startOf = [](QDate d) { return QDateTime(d, QTime(0, 0)).toMSecsSinceEpoch() + kDayStartsMs; };
    const std::int64_t a = startOf(target.addDays(-7 * kWeeksBack)), b = startOf(today);
    const QString label = i18n::locale().toString(target, u"dddd MMM d"_s);

    const int request = ++rangeRequest_;
    rangeReport_ = {{u"loading"_s, true}, {u"id"_s, u"forecast"_s}, {u"label"_s, label}};
    emit sessionChanged();

    auto history = s_->history;
    std::vector<Check> today_ = s_->closedToday;
    QPointer<PosService> self(this);
    QThreadPool::globalInstance()->start([=, today_ = std::move(today_)]() mutable {
        std::vector<Check> checks = history ? history(a - 7 * 24 * 3600 * 1000LL, b) : std::vector<Check>{};
        for (Check &c : today_)
            if (std::ranges::none_of(checks, [&](const Check &x) { return x.id == c.id; }))
                checks.push_back(std::move(c));
        QMetaObject::invokeMethod(self, [self, request, target, label, checks = std::move(checks)] {
            if (!self || request != self->rangeRequest_)
                return;
            self->forecast_ = self->forecastReport(target, checks, self->reportContext(label));
            self->rangeReport_ = {{u"loading"_s, false}, {u"id"_s, u"forecast"_s}, {u"label"_s, label},
                                  {u"report"_s, self->reportVariant(self->forecast_)}};
            emit self->sessionChanged();
        }, Qt::QueuedConnection);
    });
    return true;
}

Report PosService::forecastReport(QDate target, const std::vector<Check> &checks, const ReportContext &ctx) const
{
    Report r;
    r.id = "forecast";
    r.title = "Forecast";
    // The same weekday, back from last week; the days that had sales.
    std::map<QDate, DaySample> days;
    for (int k = 1; k <= kWeeksBack; ++k)
        days[target.addDays(-7 * k)];
    for (const Check &c : checks) {
        if (c.status != CheckStatus::Closed || c.training)
            continue;
        const auto it = days.find(dayOf(c));
        if (it == days.end())
            continue;
        DaySample &d = it->second;
        const Money sales = c.totals(ctx.settings.tax).subtotal;
        d.guests += std::max(1, c.guests);
        ++d.checks;
        d.sales += sales;
        auto &hour = d.byHour[ctx.hourOf(c.openedAt)];
        ++hour.first;
        hour.second += sales;
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment() || l.isGiftCard())
                continue;
            d.items[l.itemId] += l.counted();
            for (const auto &[id, q] : stockUse(l))
                d.used[id] += q;
        }
    }
    std::vector<const DaySample *> used;   // newest first
    for (auto it = days.rbegin(); it != days.rend() && int(used.size()) < kWeeksUsed; ++it)
        if (it->second.checks > 0)
            used.push_back(&it->second);
    r.subtitle = ctx.period;
    if (!used.empty())
        r.note(ss(tr("From the same day in %n past week(s); recent weeks count more.", "", int(used.size()))));
    if (used.empty()) {
        r.note(ss(tr("Not enough sales yet: a forecast needs at least one past %1 with sales.")
                      .arg(i18n::locale().toString(target, u"dddd"_s))));
        return r;
    }
    // Weights 4, 3, 2, 1 from the newest.
    std::vector<double> weight;
    double sum = 0;
    for (int i = 0; i < int(used.size()); ++i) {
        weight.push_back(kWeeksUsed - i);
        sum += weight.back();
    }
    for (double &w : weight)
        w /= sum;
    const auto average = [&](const auto &value) {
        double v = 0;
        for (int i = 0; i < int(used.size()); ++i)
            v += weight[i] * value(*used[i]);
        return v;
    };
    const auto money = [&](double cents) { return ctx.money(Money::fromCents(std::llround(cents))); };

    r.section("The day");
    r.line({"Guests", std::to_string(std::lround(average([](const DaySample &d) { return double(d.guests); })))});
    r.line({"Checks", std::to_string(std::lround(average([](const DaySample &d) { return double(d.checks); })))});
    r.line({"Net sales", money(average([](const DaySample &d) { return double(d.sales.cents()); }))});

    r.section("By hour");
    for (int h = 0; h < 24; ++h) {
        const double n = average([h](const DaySample &d) {
            const auto it = d.byHour.find(h);
            return it == d.byHour.end() ? 0.0 : double(it->second.first);
        });
        if (n < 0.5)
            continue;
        const double sales = average([h](const DaySample &d) {
            const auto it = d.byHour.find(h);
            return it == d.byHour.end() ? 0.0 : double(it->second.second.cents());
        });
        r.line({ss(i18n::timeText(QTime(h, 0))), ss(tr("%n check(s)", "", int(std::lround(n)))),
                money(sales)});
    }

    // Staff: servers each hour needs (the store's checks per server an hour),
    // beside the servers scheduled then.
    {
        const int perServer = std::max(1, s_->settings.checksPerServerHour);
        std::vector<std::vector<std::string>> lines;
        bool shortAny = false;
        for (int h = 0; h < 24; ++h) {
            const double n = average([h](const DaySample &d) {
                const auto it = d.byHour.find(h);
                return it == d.byHour.end() ? 0.0 : double(it->second.first);
            });
            if (n < 0.5)
                continue;
            const int needed = std::max(1, int(std::ceil(n / perServer)));
            const std::int64_t from = QDateTime(target, QTime(h, 0)).toMSecsSinceEpoch();
            const std::int64_t to = from + 3'600'000;
            int scheduled = 0;
            for (const Shift &sh : s_->shifts) {
                if (sh.start >= to || sh.end <= from)
                    continue;
                const auto e = std::ranges::find_if(s_->employees, [&](const Employee &x) { return x.id == sh.employeeId; });
                if (e != s_->employees.end() && e->role == "server")
                    ++scheduled;
            }
            const bool short_ = scheduled < needed;
            shortAny = shortAny || short_;
            lines.push_back({ss(i18n::timeText(QTime(h, 0))), ss(tr("%n server(s) needed", "", needed)),
                             ss((short_ ? u"⚠ "_s : QString()) + tr("%n scheduled", "", scheduled))});
        }
        if (!lines.empty()) {
            r.section("Servers");
            r.note(ss(tr("About %n check(s) a server an hour (Store Settings -> Staff hours).", "", perServer)));
            for (auto &l : lines)
                r.line(std::move(l));
            if (shortAny)
                r.note(ss(tr("⚠: fewer servers scheduled than the forecast needs.")));
        }
    }

    // Items: the most first.
    std::map<std::string, double> items;
    for (int i = 0; i < int(used.size()); ++i)
        for (const auto &[id, n] : used[i]->items)
            items[id] += weight[i] * n;
    std::vector<std::pair<std::string, double>> order(items.begin(), items.end());
    std::ranges::sort(order, [](const auto &x, const auto &y) { return x.second > y.second; });
    r.section("Items");
    for (const auto &[id, n] : order) {
        if (n < 0.5)
            continue;
        const MenuItem *m = findItem(qs(id));
        r.line({m ? m->name : id, std::to_string(std::lround(n))});
    }

    // Ingredients: what the day uses, beside what's on hand.
    std::map<std::string, double> need;
    for (int i = 0; i < int(used.size()); ++i)
        for (const auto &[id, q] : used[i]->used)
            need[id] += weight[i] * q;
    if (!need.empty()) {
        r.section("Prep: what it will use");
        std::map<std::string, std::vector<std::vector<std::string>>> byVendor;
        for (const Ingredient &g : s_->ingredients) {
            const auto it = need.find(g.id);
            if (it == need.end() || it->second <= 0)
                continue;
            const double needed = std::ceil(it->second * 10) / 10;
            r.line({g.name, ss(u"%1 %2"_s.arg(amountText(needed), qs(g.unit))),
                    ss(tr("have %1").arg(amountText(g.onHand)))});
            // To order: the day's use and the low-stock level, less what's on hand.
            const double short_ = needed + g.lowAt - g.onHand;
            if (short_ > 0) {
                const auto v = std::ranges::find_if(s_->settings.vendors, [&](const Vendor &x) { return x.id == g.vendor; });
                const std::string vendor = v == s_->settings.vendors.end() ? std::string() : v->name;
                byVendor[vendor].push_back({g.name, ss(u"%1 %2"_s.arg(amountText(std::ceil(short_)), qs(g.unit)))});
            }
        }
        if (byVendor.empty()) {
            r.note("Enough on hand for the day.");
        } else {
            for (const auto &[vendor, lines] : byVendor) {
                r.section(vendor.empty() ? "To order" : ss(tr("To order from %1").arg(qs(vendor))));
                for (const auto &l : lines)
                    r.line(l);
            }
            r.note("To order: what the day will use and the low-stock level, less what's on hand.");
        }
    }
    return r;
}

} // namespace vt::app
