#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

#include <map>
#include <string>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// A store that runs for months without a restart (a register stays on): a
// day's work every day, End of Day each night. Nothing it keeps in memory
// may grow day after day: what's kept is today's, or a window of days.

namespace {

// How much of each thing the store keeps in memory now.
std::map<std::string, std::size_t> kept(app::PosShared &s)
{
    return {
        {"open checks", s.open.size()},
        {"closed today", s.closedToday.size()},
        {"drawers", s.drawers.size()},
        {"punches", s.punches.size()},
        {"earlier punches", s.earlierPunches.size()},
        {"older punches", s.olderPunches.size()},
        {"parties", s.parties.size()},
        {"deliveries", s.deliveries.size()},
        {"past days", s.pastDays.size()},
        {"refunds today", s.refundsToday.size()},
        {"changes today", s.changesToday.size()},
        {"stock counts", s.stockCounts.size()},
        {"waste", s.waste.size()},
        {"problems", s.problems.size()},
        {"bumped", s.bumped.size()},
        {"served", s.served.size()},
        {"messages", s.messages.size()},
        {"menu undo", s.menuUndo.size()},
        {"receipt tries", s.receiptTried.size()},
        {"resume checks", s.resumeChecks.size()},
        {"wrong PINs", s.wrongPinsLately.size()},
        {"locked by", s.lockedBy.size()},
        {"shifts", s.shifts.size()},
        {"notices", s.settings.notices.size()},
        {"table states", s.settings.tableStates.size()},
        {"punch changes", s.settings.punchChanges.size()},
        {"today's pairs", s.todayTogether.size()},
    };
}

// One day: a lunch's checks (sent, bumped, paid), a server's shift, waste,
// a message, a party on the waitlist, now and then a refund; End of Day.
void workADay(PosService &pos, std::int64_t &clock, int day)
{
    app::PosShared &s = *pos.shared();
    const QDate date = QDate(2026, 1, 5).addDays(day);
    clock = QDateTime(date, QTime(10, 0)).toMSecsSinceEpoch();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.clockIn());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    // Every third morning, a delivery: each ingredient from its vendor.
    if (day % 3 == 0) {
        std::map<std::string, QVariantList> byVendor;
        for (const core::Ingredient &g : s.ingredients)
            if (!g.id.starts_with("item-"))
                byVendor[g.vendor.empty() ? std::string("city-bakery") : g.vendor].append(
                    QVariantMap{{u"ingredient"_s, QString::fromStdString(g.id)}, {u"qty"_s, 200}});
        for (const auto &[vendor, lines] : byVendor)
            REQUIRE(pos.receiveDelivery({{u"vendor"_s, QString::fromStdString(vendor)}, {u"lines"_s, lines}}));
    }
    for (int n = 0; n < 12; ++n) {
        clock += 20 * 60'000;
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        REQUIRE(pos.addItem(n % 2 ? u"cobb"_s : u"coffee"_s));
        REQUIRE(pos.sendOrder());
        const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
        for (const QVariant &t : pos.kitchenTickets())
            if (t.toMap()[u"checkId"_s].toLongLong() == id)
                pos.bumpTicket(id, t.toMap()[u"sentAt"_s].toLongLong());
        REQUIRE(pos.tender(n % 3 ? u"cash"_s : u"credit"_s));
        REQUIRE(pos.closeCheck());
        if (n == 5 && day % 7 == 0) {   // a refund now and then
            const core::Check &c = s.closedToday.back();
            pos.refundPayment(c.id, c.payments.front().id, 100, u"Cold"_s);
        }
    }
    pos.logWaste({{u"ingredient"_s, u"lettuce"_s}, {u"qty"_s, u"1"_s}, {u"reason"_s, u"Expired or spoiled"_s}});
    pos.sendMessage(u"all"_s, u"Need a runner"_s, 0);
    const qint64 party = pos.addToWaitlist({{u"name"_s, u"Dana"_s}, {u"size"_s, 2}});
    if (party > 0)
        pos.seatParty(party, u"T1"_s);
    if (pos.selectTable(u"T1"_s) == app::PosService::TableOpened) {   // seated: their check, paid
        pos.addItem(u"coffee"_s);
        pos.tender(u"cash"_s);
        pos.closeCheck();
    } else {
        pos.releaseCheck();
    }
    clock = QDateTime(date, QTime(21, 0)).toMSecsSinceEpoch();
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.clockOut();
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    for (const core::DrawerSession &d : s.drawers)
        if (d.open())
            pos.entryKey(QString::number(core::expectedCash(d, s.closedToday, s.settings.tax).cents()));
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.endOfDay());
    pos.logout();
}

} // namespace

TEST_CASE("Soak: four months of days, nothing kept in memory keeps growing", "[soak]")
{
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = 0;
    pos.shared()->setClock([&clock] { return clock; });
    std::map<std::string, std::size_t> atTwoMonths;
    for (int day = 0; day < 120; ++day) {
        INFO("day " << day);
        workADay(pos, clock, day);
        if (day == 59)
            atTwoMonths = kept(*pos.shared());
    }
    const auto atFourMonths = kept(*pos.shared());
    for (const auto &[what, then] : atTwoMonths) {
        INFO(what << ": " << then << " after two months, " << atFourMonths.at(what) << " after four");
        CHECK(atFourMonths.at(what) <= then + 2);
    }
    // The day's own: empty after End of Day.
    CHECK(pos.shared()->closedToday.empty());
    CHECK(pos.shared()->refundsToday.empty());
}
