// PosService: the profit coach. Plain advice from the store's own last four
// weeks: an item that sells well but earns little (raise its price a
// little: what that's worth a month), one that earns well but few order
// (show it off), one that does neither (rethink it), items nobody ordered,
// a quiet time of the week (a Happy Hour?), and discounts eating the profit.
// Read away from the screen once an hour; worked out here (coachFrom).

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QPointer>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kDay = 86'400'000;
constexpr int kDays = 28;

// A price rise that reads as one: a quarter under $5, fifty cents to $10, a dollar above.
Money raiseFor(Money price)
{
    return Money::fromCents(price.cents() >= 1000 ? 100 : price.cents() >= 500 ? 50 : 25);
}

} // namespace

QVariantMap PosService::coachFrom(const std::vector<Check> &closed) const
{
    const PosSettings &st = s_->settings;
    const std::int64_t t = now(), from = t - kDays * kDay;
    // Days with sales: under two weeks, too soon to say.
    std::set<QDate> days;
    for (const Check &c : closed)
        if (!c.training && c.closedAt >= from)
            days.insert(QDateTime::fromMSecsSinceEpoch(c.closedAt).date());
    if (days.size() < 14)
        return {{u"waiting"_s, tr("Tips start after two weeks of sales (%n day(s) so far).", nullptr, int(days.size()))},
                {u"tips"_s, QVariantList{}}};

    QVariantList tips;
    const auto tip = [&](const QString &id, const QString &kind, const QString &title, const QString &why, qint64 worth,
                         const QString &item = {}, const QString &price = {}, const QStringList &names = {}) {
        tips.append(QVariantMap{{u"id"_s, id}, {u"kind"_s, kind}, {u"title"_s, title}, {u"why"_s, why},
                                {u"worth"_s, worth}, {u"worthText"_s, worth > 0 ? tr("≈ %1 a month").arg(format(Money::fromCents(worth))) : QString()},
                                {u"item"_s, item}, {u"price"_s, price}, {u"names"_s, names}});
    };

    // --- the menu: how each item sells and earns, against its category ---
    struct Row { std::int64_t sold = 0; Money sales, cost; bool costed = false; };
    std::map<std::string, Row> rows;
    Money items, discounts;
    std::map<std::string, Money> byDiscount;
    for (const Check &c : closed) {
        if (c.training || c.closedAt < from)
            continue;
        const Totals tot = c.totals(st.tax);
        items += tot.items - tot.giftCardsSold;
        discounts += tot.discounts;
        for (const Payment &p : c.payments)
            if (p.kind == TenderKind::Discount)
                if (const auto it = tot.discountOf.find(p.id); it != tot.discountOf.end())
                    byDiscount[p.tenderName] += it->second;
        for (const OrderLine &l : c.lines) {
            if (l.voided || l.isComment() || l.isGiftCard() || l.isFee())
                continue;
            Row &r = rows[l.itemId];
            r.sold += l.counted();
            r.sales += l.total();
            r.cost += lineCost(l);
        }
    }
    std::map<std::string, std::vector<const MenuItem *>> families;
    for (const MenuItem &m : s_->menu)
        if (!m.isModifier && !m.family.empty() && !m.id.starts_with("giftcard") && m.available)
            families[m.family].push_back(&m);
    std::map<std::string, QString> familyNames;
    for (const MenuCategory &c : s_->categories())
        familyNames[c.id] = qs(c.name);
    QStringList never;
    QString neverFirst;
    for (const auto &[family, list] : families) {
        const QString fname = familyNames.contains(family) ? familyNames.at(family) : qs(family);
        std::int64_t sold = 0;
        Money earned;
        bool anyCost = false;
        std::vector<std::int64_t> prices;
        for (const MenuItem *m : list) {
            Row &r = rows[m->id];
            r.costed = !m->recipe.empty();
            anyCost = anyCost || r.costed;
            sold += r.sold;
            earned += r.sales - r.cost;
            prices.push_back(m->price.cents());
            if (r.sold == 0) {
                if (never.isEmpty())
                    neverFirst = qs(m->id);
                never << qs(m->name);
            }
        }
        if (sold == 0 || list.size() < 2)
            continue;
        std::ranges::sort(prices);
        const std::int64_t median = prices[prices.size() / 2];
        const double n = double(list.size());
        const double popular = 0.7 / n;
        const double average = double(earned.cents()) / double(sold);   // earned per item, cents
        // A hidden gem: the one that earns most of those clearly slow to sell (one a category).
        const MenuItem *gem = nullptr;
        double gemEach = 0;
        for (const MenuItem *m : list) {
            const Row &r = rows[m->id];
            if (!r.costed || r.sold == 0 || double(r.sold) / double(sold) >= popular * 0.5)
                continue;
            const double each = double((r.sales - r.cost).cents()) / double(r.sold);
            if (each >= average * 1.15 && each > gemEach) {
                gem = m;
                gemEach = each;
            }
        }
        for (const MenuItem *m : list) {
            const Row &r = rows[m->id];
            if (r.sold == 0)
                continue;
            const double mix = double(r.sold) / double(sold);
            const double each = double((r.sales - r.cost).cents()) / double(r.sold);
            const QString name = qs(m->name);
            // Sells well, earns little (or, with no costs known, sells best and costs less than most).
            const bool workhorse = r.costed ? (mix >= popular && each < average)
                                            : (!anyCost && mix >= 1.5 / n && m->price.cents() < median);
            if (workhorse && r.sold >= 20 && m->price.cents() > 0) {
                const Money raise = raiseFor(m->price);
                const qint64 worth = std::llround(double(r.sold) * 0.9 * double(raise.cents()));
                if (worth >= 2000) {
                    const Money now = m->price + raise;
                    tip(u"raise-"_s + qs(m->id), u"raise"_s, tr("Raise %1 to %2").arg(name, format(now)),
                        r.costed ? tr("It sells well (%1 in 4 weeks) but earns %2 less each than the average in %3. %4 more is "
                                      "about %5 a month, even if a few people order something else.")
                                       .arg(r.sold).arg(format(Money::fromCents(std::llround(average - each))), fname,
                                            format(raise), format(Money::fromCents(worth)))
                                 : tr("It's one of the best sellers in %1 (%2 in 4 weeks) and costs less than most of them. "
                                      "%3 more is about %4 a month, even if a few people order something else.")
                                       .arg(fname).arg(r.sold).arg(format(raise), format(Money::fromCents(worth))),
                        worth, qs(m->id), QString::number(double(now.cents()) / 100.0, 'f', 2));
                }
                continue;
            }
            if (!r.costed)
                continue;
            if (m == gem) {
                // Earns well, few order it: half of what selling like the others would bring.
                const double gap = std::max(0.0, double(sold) / n - double(r.sold));
                const qint64 worth = std::llround(gap * each * 0.5);
                tip(u"gem-"_s + qs(m->id), u"gem"_s, tr("Show off %1").arg(name),
                    tr("It earns %1 each, more than most in %2, but few order it (%3 in 4 weeks). Make it a Favorite, put "
                       "it first with a picture, or have servers suggest it.")
                        .arg(format(Money::fromCents(std::llround(each))), fname).arg(r.sold),
                    worth >= 2000 ? worth : 0, qs(m->id));
            } else if (mix < popular * 0.5 && each < average) {
                tip(u"weak-"_s + qs(m->id), u"weak"_s, tr("Rethink %1").arg(name),
                    tr("Few order it (%1 in 4 weeks) and it earns less than the rest of %2. Change it, or take it off to "
                       "make room for something that sells.")
                        .arg(r.sold).arg(fname),
                    0, qs(m->id));
            }
        }
    }
    if (!never.isEmpty() && days.size() >= 21) {
        const QString names = never.size() <= 3 ? never.join(u", "_s)
                                                 : never.mid(0, 3).join(u", "_s) + u' ' + tr("and %n more", nullptr, int(never.size() - 3));
        tip(u"never"_s, u"never"_s, tr("Nobody ordered %1 in 4 weeks").arg(names),
            tr("A shorter menu is quicker to order from and to make. Take them off, or mark them sold out for now."), 0,
            neverFirst, {}, never);
    }

    // --- a quiet time of the week ---
    {
        // Sales by weekday, hour and week.
        double sales[8][24][5] = {};
        bool any[8][5] = {};
        for (const Check &c : closed) {
            if (c.training || c.closedAt < from)
                continue;
            const QDateTime at = QDateTime::fromMSecsSinceEpoch(c.closedAt);
            const int week = std::clamp(int((c.closedAt - from) / (7 * kDay)), 0, 4);
            const int wd = at.date().dayOfWeek();
            sales[wd][at.time().hour()][week] += double(c.totals(st.tax).netSales().cents());
            any[wd][week] = true;
        }
        struct Quiet { int wd = 0, start = 0, end = 0; double ratio = 1; };
        Quiet best;
        for (int wd = 1; wd <= 7; ++wd) {
            int weeks = 0;
            for (int w = 0; w < 5; ++w)
                weeks += any[wd][w];
            if (weeks < 3)
                continue;
            // Its open hours: sales in that hour in at least two of those weeks.
            double avg[24] = {};
            int first = -1, last = -1;
            for (int h = 0; h < 24; ++h) {
                int seen = 0;
                for (int w = 0; w < 5; ++w) {
                    avg[h] += sales[wd][h][w];
                    seen += sales[wd][h][w] > 0;
                }
                avg[h] /= weeks;
                if (seen >= 2) {
                    if (first < 0)
                        first = h;
                    last = h;
                }
            }
            if (first < 0 || last - first < 4)
                continue;
            double day = 0;
            for (int h = first; h <= last; ++h)
                day += avg[h];
            day /= double(last - first + 1);
            // Two hours or more, inside the day (not its first or last hour), under 40% of a usual hour.
            for (int h = first + 1; h < last;) {
                if (avg[h] >= 0.4 * day) {
                    ++h;
                    continue;
                }
                int e = h;
                double sum = 0;
                while (e < last && avg[e] < 0.4 * day)
                    sum += avg[e++];
                const double ratio = sum / double(e - h) / day;
                if (e - h >= 2 && ratio < best.ratio)
                    best = {wd, h, e, ratio};
                h = e;
            }
        }
        if (best.wd > 0) {
            const QString dayName = i18n::locale().standaloneDayName(best.wd);
            tip(u"quiet-%1-%2"_s.arg(best.wd).arg(best.start), u"quiet"_s,
                tr("Quiet on %1, %2 to %3").arg(dayName, i18n::timeText(QTime(best.start, 0)), i18n::timeText(QTime(best.end % 24, 0))),
                (best.ratio < 0.05 ? tr("Those hours sell almost nothing on that day.")
                                   : tr("Those hours sell %1% of a usual hour on that day.").arg(std::lround(best.ratio * 100)))
                    + u' ' + tr("A Happy Hour (Meal Periods, with its own prices) or a special could bring people in."),
                0);
        }
    }

    // --- discounts eating the profit ---
    if (items.cents() > 0 && discounts.cents() * 100 >= items.cents() * 8) {
        std::string most;
        Money mostAmount;
        for (const auto &[name, amount] : byDiscount)
            if (amount > mostAmount) {
                most = name;
                mostAmount = amount;
            }
        const qint64 over = discounts.cents() - items.cents() * 5 / 100;
        tip(u"discounts"_s, u"discounts"_s,
            tr("Discounts are %1% of sales").arg(std::lround(100.0 * double(discounts.cents()) / double(items.cents()))),
            tr("%1 in 4 weeks; the most: %2 (%3). Worth checking each one is needed: the Exceptions report has who gave them.")
                .arg(format(discounts), most.empty() ? tr("several") : qs(most), format(mostAmount)),
            over > 0 ? over : 0);
    }

    std::ranges::stable_sort(tips, [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"worth"_s).toLongLong() > b.toMap().value(u"worth"_s).toLongLong();
    });
    return {{u"tips"_s, tips}, {u"days"_s, int(days.size())}};
}

void PosService::refreshCoach() const
{
    coachAt_ = now();
    if (!s_->history) {   // (no database: today's only)
        coach_ = coachFrom(s_->closedToday);
        return;
    }
    QPointer<PosService> self(const_cast<PosService *>(this));
    const std::int64_t t = now();
    QThreadPool::globalInstance()->start([self, history = s_->history, t, today = s_->closedToday] {
        std::vector<Check> checks = history(t - kDays * kDay, t + kDay);
        for (const Check &c : today)   // (not written yet)
            if (std::ranges::none_of(checks, [&](const Check &x) { return x.id == c.id; }))
                checks.push_back(c);
        QMetaObject::invokeMethod(self, [self, checks = std::move(checks)] {
            if (!self)
                return;
            self->coach_ = self->coachFrom(checks);
            emit self->sessionChanged();
        });
    });
}

QVariantMap PosService::coach() const
{
    if (!user() || !user()->can(perm::Manager))
        return {};
    if (coachAt_ == 0 || now() - coachAt_ > 3'600'000 || coachDay_ != s_->day.id) {
        coachDay_ = s_->day.id;
        refreshCoach();
    }
    if (coach_.isEmpty())
        return {{u"busy"_s, true}, {u"tips"_s, QVariantList{}}};
    // Without the ones put off for now (Not Now).
    QVariantMap out = coach_;
    QVariantList shown;
    for (const QVariant &v : coach_.value(u"tips"_s).toList()) {
        const auto it = s_->settings.coachDismissed.find(ss(v.toMap().value(u"id"_s).toString()));
        if (it == s_->settings.coachDismissed.end() || it->second < now())
            shown.append(v);
    }
    out.insert(u"tips"_s, shown);
    return out;
}

bool PosService::coachNotNow(const QString &id)
{
    if (!require(perm::Manager, tr("The profit coach")))
        return false;
    auto &dismissed = s_->settings.coachDismissed;
    std::erase_if(dismissed, [&](const auto &e) { return e.second < now(); });   // (long past)
    dismissed[ss(id)] = now() + 30 * kDay;
    if (s_->sink)
        s_->sink->saveSettings(s_->settings);
    emit sessionChanged();
    return true;
}

Report PosService::coachReport(const ReportContext &ctx) const
{
    Report r;
    r.id = "coach";
    r.title = "Profit Coach";
    r.subtitle = "The last 4 weeks";
    r.columns = {"What to try", "Worth"};
    const QVariantMap c = coach();
    if (c.value(u"busy"_s).toBool()) {
        r.note("Working it out from the last 4 weeks: open it again in a moment.");
        return r;
    }
    if (const QString waiting = c.value(u"waiting"_s).toString(); !waiting.isEmpty()) {
        r.note(ss(waiting));
        return r;
    }
    const QVariantList tips = c.value(u"tips"_s).toList();
    if (tips.isEmpty())
        r.note("Nothing to change: the menu, its prices and the week look healthy.");
    for (const QVariant &v : tips) {
        const QVariantMap t = v.toMap();
        r.section(ss(t.value(u"title"_s).toString()));
        if (const qint64 worth = t.value(u"worth"_s).toLongLong(); worth > 0)
            r.line({"About a month", ctx.money(Money::fromCents(worth))});
        r.note(ss(t.value(u"why"_s).toString()));
    }
    return r;
}

} // namespace vt::app
