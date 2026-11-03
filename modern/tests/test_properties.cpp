#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QString>

#include <cmath>
#include <cstdlib>
#include <random>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Rules that hold whatever the staff do, in whatever order: random checks
// worked on with real actions (items, quantities, voids, discounts, part
// payments, gratuity, tips, splits, closes), the rules checked after each.
// Fixed seeds, so a failure happens again; VTM_FUZZ_ROUNDS for more.

namespace {

int rounds(int normal)
{
    const char *v = std::getenv("VTM_FUZZ_ROUNDS");
    return v ? std::max(1, std::atoi(v)) : normal;
}

// "Total with tax" -> cents, from a report's rows.
std::int64_t reportCents(const QVariantMap &report, const QString &line)
{
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (cells.size() >= 2 && cells[0] == line) {
            QString money = cells[1];
            money.remove(u'$').remove(u',');
            const bool negative = money.startsWith(u'-') || money.startsWith(u'(');
            money.remove(u'-').remove(u'(').remove(u')');
            const std::int64_t c = std::llround(money.toDouble() * 100);
            return negative ? -c : c;
        }
    }
    FAIL("no line " << line.toStdString());
    return 0;
}

const QStringList kItems = {u"cobb"_s, u"coffee"_s, u"tea"_s, u"soda"_s, u"lemonade"_s, u"draft-beer"_s,
                            u"house-wine"_s, u"fries"_s, u"house-salad"_s, u"caesar"_s, u"pancakes"_s};

void everyCheckAddsUp(PosService &pos)
{
    const app::PosShared &s = *pos.shared();
    for (const auto &[id, c] : s.open) {
        INFO("open check " << id);
        CHECK(core::booksProblem(c.totals(s.settings.tax)).empty());
    }
    for (const core::Check &c : s.closedToday) {
        INFO("closed check " << c.id);
        const core::Totals t = c.totals(c.closedRates.value_or(s.settings.tax));
        CHECK(core::booksProblem(t).empty());
        if (c.status == core::CheckStatus::Closed)
            CHECK(t.balance.cents() <= 0);
    }
}

} // namespace

TEST_CASE("Properties: whatever is done to checks, each adds up, and the day's report is their sum", "[properties]")
{
    for (int round = 0; round < rounds(25); ++round) {
        INFO("round " << round);
        std::mt19937 rng{unsigned(round) * 7919u + 1u};
        const auto pick = [&](int n) { return int(rng() % unsigned(n)); };
        PosService pos(test::seedPosData(), nullptr);
        std::int64_t clock = 1'800'000'000'000;
        pos.shared()->setClock([&clock] { return clock; });
        REQUIRE(pos.loginWithPin(u"1234"_s));
        pos.entryKey(u"10000"_s);
        REQUIRE(pos.openDrawerSession());
        for (int checkNo = 0; checkNo < 6; ++checkNo) {
            REQUIRE(pos.startCheck(checkNo % 2 ? core::CheckType::Takeout : core::CheckType::Quick));
            for (int step = 0; step < 14; ++step) {
                clock += 15'000;
                const core::Check *c = pos.currentCheck();
                if (!c)
                    break;
                switch (pick(9)) {
                case 0: case 1: case 2:
                    pos.addItem(kItems[pick(int(kItems.size()))]);
                    break;
                case 3:
                    if (!c->lines.empty())
                        pos.setLineQuantity(c->lines[std::size_t(pick(int(c->lines.size())))].id, 1 + pick(5));
                    break;
                case 4:
                    if (!c->lines.empty()) {
                        pos.selectLine(c->lines[std::size_t(pick(int(c->lines.size())))].id);
                        pos.voidItem();
                    }
                    break;
                case 5:
                    pos.tender(pick(2) ? u"discount"_s : u"staff-meal"_s);
                    break;
                case 6: {   // part of what's due, in cash
                    const std::int64_t due = c->totals(pos.shared()->settings.tax).balance.cents();
                    if (due > 1)
                        pos.tender(u"cash"_s, 1 + std::int64_t(rng() % std::uint64_t(due)));
                    break;
                }
                case 7:
                    pos.setGratuity(std::int64_t(pick(4)) * 500);   // none, 5, 10 or 15%
                    break;
                case 8:
                    if (!c->payments.empty()) {
                        pos.selectPayment(c->payments.back().id);
                        pos.removePayment();
                    }
                    break;
                }
                everyCheckAddsUp(pos);
            }
            // Now and then split it evenly first: the pieces add up to the whole.
            if (const core::Check *c = pos.currentCheck(); c && pick(3) == 0 && c->payments.empty()
                                                         && c->totals(pos.shared()->settings.tax).total.cents() > 300) {
                const std::int64_t whole = c->totals(pos.shared()->settings.tax).total.cents();
                std::set<qint64> before;
                for (const auto &[id, o] : pos.shared()->open)
                    before.insert(id);
                const int ways = 2 + pick(3);
                if (pos.splitEvenly(ways)) {
                    std::int64_t pieces = 0;
                    for (const auto &[id, o] : pos.shared()->open)
                        if (!before.contains(id) || id == pos.currentCheck()->id)
                            pieces += o.totals(pos.shared()->settings.tax).total.cents();
                    INFO("split " << ways << " ways");
                    CHECK(std::llabs(pieces - whole) <= ways);   // a cent of tax each, at most
                }
            }
            // Paid in full and closed; the split pieces too.
            while (pos.currentCheck()) {
                const core::Check *c = pos.currentCheck();
                const std::int64_t due = c->totals(pos.shared()->settings.tax).balance.cents();
                if (due > 0)
                    REQUIRE(pos.tender(pick(2) ? u"cash"_s : u"credit"_s));
                const qint64 id = c->id;
                if (!pos.closeCheck())
                    FAIL("couldn't close check " << id);
                everyCheckAddsUp(pos);
                for (const auto &[oid, o] : pos.shared()->open)
                    if (!o.training) {
                        pos.openCheck(oid);
                        break;
                    }
            }
        }
        // The day's sales report is the closed checks added up.
        std::int64_t total = 0;
        for (const core::Check &c : pos.shared()->closedToday)
            if (c.status == core::CheckStatus::Closed && !c.training)
                total += c.totals(c.closedRates.value_or(pos.shared()->settings.tax)).total.cents();
        CHECK(reportCents(pos.report(u"sales"_s), u"Total with tax"_s) == total);
        for (const core::Problem &p : pos.shared()->problems)
            CHECK(p.kind != "books");
    }
}

TEST_CASE("Properties: money adds, takes away and takes a percent the way arithmetic says", "[properties][money]")
{
    std::mt19937_64 rng{42};
    for (int i = 0; i < rounds(20) * 400; ++i) {
        const auto a = vt::Money::fromCents(std::int64_t(rng() % 20'000'000) - 10'000'000);
        const auto b = vt::Money::fromCents(std::int64_t(rng() % 20'000'000) - 10'000'000);
        INFO(a.cents() << ", " << b.cents());
        CHECK((a + b) - b == a);
        CHECK(a + b == b + a);
        CHECK(a - a == vt::Money());
        // A percent of a sum is the sum of the percents, give or take a cent.
        const std::int64_t ppm = std::int64_t(rng() % 300'000);
        CHECK(std::llabs((a + b).scaled(ppm, 1'000'000).cents() - (a.scaled(ppm, 1'000'000) + b.scaled(ppm, 1'000'000)).cents()) <= 1);
        // A bigger amount never has a smaller percent.
        if (a.cents() >= 0 && b.cents() >= 0 && a.cents() <= b.cents())
            CHECK(a.scaled(ppm, 1'000'000).cents() <= b.scaled(ppm, 1'000'000).cents());
    }
}
