#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Staff: permissions per person, breaks, overtime.

namespace {

int indexOf(PosService &pos, const QString &name)
{
    const QVariantList staff = pos.adminRecords(u"employees"_s);
    for (int i = 0; i < staff.size(); ++i) {
        if (staff[i].toMap()[u"name"_s].toString().startsWith(name))
            return i;
    }
    return -1;
}

} // namespace

TEST_CASE("Permissions per person: yes or no over the role", "[staff][permissions]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int sam = indexOf(pos, u"Sam"_s);
    REQUIRE(sam >= 0);
    QVariantMap r = pos.adminRecords(u"employees"_s)[sam].toMap();
    CHECK(r[u"perm:order.void"_s].toString().isEmpty());       // as the role
    r[u"perm:order.void"_s] = u"allow"_s;                       // Sam may void sent items
    r[u"perm:check.discount"_s] = u"deny"_s;                    // but not give discounts
    REQUIRE(pos.adminSave(u"employees"_s, sam, r));
    const core::Employee &e = pos.shared()->employees[sam];
    CHECK(e.can(core::perm::Void));
    CHECK_FALSE(e.can(core::perm::Discount));
    CHECK(e.can(core::perm::Order));                           // the rest as a server
    CHECK(pos.adminRecords(u"employees"_s)[sam].toMap()[u"perm:order.void"_s] == u"allow"_s);
    pos.logout();

    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.permissions().contains(u"order.void"_s));
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    CHECK(pos.voidItem());                                     // allowed for Sam
    pos.addItem(u"tea"_s);
    CHECK_FALSE(pos.tender(u"discount"_s));                    // denied for Sam
    CHECK(pos.tender(u"cash"_s, 100));                         // payments still fine
    pos.logout();

    // A manager can't take their own manager rights away.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const int morgan = indexOf(pos, u"Morgan"_s);
    QVariantMap me = pos.adminRecords(u"employees"_s)[morgan].toMap();
    me[u"perm:manager"_s] = u"deny"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, morgan, me));

    // Saved with the employee.
    CHECK(app::employeeFromJson(app::toJson(e)) == e);
}

namespace {
std::int64_t todayAt(int hour, int minute = 0)
{
    return QDateTime(QDate::currentDate(), QTime(hour, minute)).toMSecsSinceEpoch();
}

// The row starting with `first` in the section titled `section`.
std::vector<std::string> rowIn(const core::Report &r, const std::string &section, const std::string &first)
{
    std::string current;
    for (const core::ReportRow &x : r.rows) {
        if (x.kind == core::ReportRow::Kind::Section)
            current = x.cells.empty() ? std::string() : x.cells.front();
        else if (current == section && !x.cells.empty() && x.cells.front() == first)
            return x.cells;
    }
    return {};
}

std::vector<std::string> row(const core::Report &r, const std::string &first, bool last = false)
{
    std::vector<std::string> found;
    for (const core::ReportRow &x : r.rows) {
        if (!x.cells.empty() && x.cells.front() == first) {
            found = x.cells;
            if (!last)
                break;
        }
    }
    return found;
}
} // namespace

TEST_CASE("Breaks: unpaid by default, ended by clocking out", "[staff][breaks]")
{
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = todayAt(9);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK_FALSE(pos.toggleBreak());                         // not on the clock
    REQUIRE(pos.clockIn());
    clock = todayAt(12);
    REQUIRE(pos.toggleBreak());
    CHECK_FALSE(pos.onBreakSince().isEmpty());
    clock = todayAt(12, 30);
    REQUIRE(pos.toggleBreak());                             // back
    CHECK(pos.onBreakSince().isEmpty());
    clock = todayAt(15);
    REQUIRE(pos.toggleBreak());
    clock = todayAt(15, 15);
    REQUIRE(pos.clockOut());                                // ends the break too
    const core::TimePunch &p = pos.shared()->punches.front();
    REQUIRE(p.breaks.size() == 2);
    CHECK(p.breaks.back().end == todayAt(15, 15));

    // 6h15m on the clock, 45 min on breaks: 5.50 h worked.
    const auto labor = pos.buildReport(u"labor"_s);
    CHECK(row(labor, "Sam")[3] == "0.75");
    CHECK(row(labor, "Sam")[4] == "5.50");
    pos.shared()->settings.paidBreaks = true;
    CHECK(row(pos.buildReport(u"labor"_s), "Sam")[4] == "6.25");

    // Saved with the punch.
    CHECK(app::punchFromJson(app::toJson(p)) == p);
}

TEST_CASE("Overtime: daily and weekly rules, the larger counts", "[staff][overtime]")
{
    app::PosData data = test::seedPosData();
    const std::string sam = [&] {
        for (const core::Employee &e : data.employees)
            if (e.name == "Sam") return e.id;
        return std::string();
    }();
    // Earlier this week: four 9-hour days (36 h).
    const std::int64_t day = 24LL * 3'600'000;
    for (int i = 1; i <= 4; ++i)
        data.earlierPunches.push_back({100 + i, sam, todayAt(8) - i * day, todayAt(17) - i * day, {}});
    PosService pos(data, nullptr);
    std::int64_t clock = todayAt(8);
    pos.shared()->setClock([&] { return clock; });
    // A week that started long enough ago to include those days.
    pos.shared()->settings.weekStartsOn = QDate::currentDate().addDays(-5).dayOfWeek() % 7;
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.clockIn());
    clock = todayAt(18);                                     // 10 h today
    REQUIRE(pos.clockOut());

    // Weekly 40: 46 h this week -> 6 h overtime.
    auto r = pos.buildReport(u"labor"_s);
    auto sams = rowIn(r, "Hours and overtime", "Sam");
    REQUIRE(sams.size() == 5);
    CHECK(sams[1] == "10.00");
    CHECK(sams[2] == "46.00");
    CHECK(sams[3] == "40.00");
    CHECK(sams[4] == "6.00");

    // Daily 8 as well: 1 h on each earlier day + 2 h today = 6 h; weekly also 6.
    pos.shared()->settings.overtimeDailyHours = 8;
    pos.shared()->settings.overtimeWeeklyHours = 0;
    sams = rowIn(pos.buildReport(u"labor"_s), "Hours and overtime", "Sam");
    CHECK(sams[4] == "6.00");

    // No rules: no overtime, and the report says so.
    pos.shared()->settings.overtimeDailyHours = 0;
    r = pos.buildReport(u"labor"_s);
    CHECK(rowIn(r, "Hours and overtime", "Sam")[4] == "-");
    CHECK_FALSE(row(r, "No overtime rule is set (Manager -> Settings).").empty());
}

namespace {
QStringList tipRow(PosService &pos, const QString &name)
{
    for (const QVariant &v : pos.report(u"tips"_s)[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (!cells.isEmpty() && cells.first() == name)
            return cells;
    }
    return {};
}
} // namespace

TEST_CASE("Schedule: shifts, next shift, clock in only on the schedule, manager override", "[staff][schedule]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(15);
    pos.shared()->setClock([&] { return clock; });
    const QString today = QDate::currentDate().toString(u"yyyy-MM-dd"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.addShift({{u"employeeId"_s, u"sam"_s}, {u"start"_s, today + u" 16:00"_s}, {u"end"_s, today + u" 22:00"_s},
                          {u"note"_s, u"patio"_s}}));
    CHECK_FALSE(pos.addShift({{u"employeeId"_s, u"sam"_s}, {u"start"_s, today + u" 20:00"_s},
                              {u"end"_s, today + u" 23:00"_s}}));            // overlaps
    CHECK_FALSE(pos.addShift({{u"employeeId"_s, u"nobody"_s}, {u"start"_s, today + u" 9:00"_s}}));
    // Riley closes: past midnight.
    REQUIRE(pos.addShift({{u"employeeId"_s, u"riley"_s}, {u"start"_s, today + u" 18:00"_s}, {u"end"_s, today + u" 01:00"_s}}));
    CHECK(pos.shared()->shifts.back().hours() == 7);

    const QVariantMap week = pos.scheduleInfo();
    QVariantMap day;
    for (const QVariant &d : week[u"days"_s].toList())
        if (d.toMap()[u"today"_s].toBool()) day = d.toMap();
    REQUIRE(day[u"shifts"_s].toList().size() == 2);
    CHECK(day[u"shifts"_s].toList()[0].toMap()[u"time"_s] == u"4 PM - 10 PM"_s);
    CHECK(week[u"totals"_s].toList().size() == 2);
    pos.logout();

    // Clock in only on the schedule.
    pos.shared()->settings.scheduleRequired = true;
    REQUIRE(pos.loginWithPin(u"1111"_s));                                    // Sam
    CHECK(pos.nextShift() == u"today 4 PM - 10 PM"_s);
    CHECK_FALSE(pos.clockIn());                                               // 3 PM: too early
    clock = todayAt(15, 50);                                                  // 10 minutes before
    REQUIRE(pos.clockIn());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"2222"_s));                                     // Casey: no shift
    CHECK_FALSE(pos.clockIn());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));                                     // managers always can
    REQUIRE(pos.clockIn());
    REQUIRE(pos.clockInEmployee(u"casey"_s));                                 // and can clock others in
    CHECK_FALSE(pos.clockInEmployee(u"casey"_s));
    const qint64 rileyShift = pos.shared()->shifts.back().id;
    REQUIRE(pos.removeShift(rileyShift));
    CHECK_FALSE(pos.removeShift(rileyShift));

    // Saved as it is.
    const core::Shift &s = pos.shared()->shifts.front();
    CHECK(app::shiftFromJson(app::toJson(s)) == s);
}

TEST_CASE("Tip pooling: tip-outs to bussers and bartenders, split by hours", "[staff][tips]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(17);
    pos.shared()->setClock([&] { return clock; });
    pos.shared()->settings.tipOuts = {{"busser", 1500, "tips"}, {"bartender", 200, "sales"}};
    // Riley (busser) and Jo (bartender) work; another busser works half as long.
    REQUIRE(pos.clockInEmployee(u"riley"_s) == false);                       // nobody logged in
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.clockInEmployee(u"riley"_s));
    REQUIRE(pos.clockInEmployee(u"jo"_s));
    pos.logout();

    // Sam sells $100 with a $20 card tip.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 8; ++i)
        pos.addItem(u"cobb"_s);                                              // 8 x $12.50 = $100 subtotal
    REQUIRE(pos.tender(u"credit"_s));
    pos.entryKey(u"2000"_s);
    REQUIRE(pos.addTip(0));
    REQUIRE(pos.closeCheck());
    clock += 4 * 3'600'000;

    // Sam: $20 - 15% ($3.00) to bussers - 2% of $100 sales ($2.00) to the bar.
    QStringList sam = tipRow(pos, u"Sam"_s);
    REQUIRE(sam.size() == 7);
    CHECK(sam[1] == u"$20.00"_s);
    CHECK(sam[3] == u"$5.00"_s);
    CHECK(sam[6] == u"$15.00"_s);
    CHECK(pos.tipsOwed() == u"$15.00"_s);
    CHECK(tipRow(pos, u"Riley"_s)[4] == u"$3.00"_s);
    CHECK(tipRow(pos, u"Jo"_s)[4] == u"$2.00"_s);

    // Sam cashes out what's left; the busser cashes out the pool share.
    REQUIRE(pos.cashOutTips());
    CHECK(tipRow(pos, u"Sam"_s)[6] == u"$0.00"_s);
    pos.logout();

    // A second busser for half the hours: the pool splits 2:1.
    core::Employee second = *pos.shared()->employee("riley");
    second.id = "kai";
    second.name = "Kai";
    pos.shared()->employees.push_back(second);
    pos.shared()->punches.push_back({999, "kai", clock - 2 * 3'600'000, 0, {}});
    CHECK(tipRow(pos, u"Riley"_s)[4] == u"$2.00"_s);
    CHECK(tipRow(pos, u"Kai"_s)[4] == u"$1.00"_s);

    // Without tip-out rules, servers keep their tips.
    pos.shared()->settings.tipOuts.clear();
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.tipsOwed() == u"$5.00"_s);                                     // $20 earned, $15 paid
}

TEST_CASE("Tip pool: everyone's tips in one pot, out by hours and job share; cash tips told", "[staff][tips][tippool]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(17);
    pos.shared()->setClock([&] { return clock; });
    pos.shared()->settings.tipPool = {{"server", 100}, {"bartender", 100}, {"busser", 50}};
    pos.shared()->settings.declareCashTips = true;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    for (const char16_t *who : {u"sam", u"jo", u"riley"})
        REQUIRE(pos.clockInEmployee(QString::fromUtf16(who)));
    CHECK(pos.cashTipsAsk().isEmpty());
    pos.logout();

    // Sam: a $20 card tip.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 8; ++i)
        pos.addItem(u"cobb"_s);
    REQUIRE(pos.tender(u"credit"_s));
    pos.entryKey(u"2000"_s);
    REQUIRE(pos.addTip(0));
    REQUIRE(pos.closeCheck());
    pos.logout();
    clock += 4 * 3'600'000;

    // Jo clocks out after 4 hours: asked for cash tips, kept $10.
    REQUIRE(pos.loginWithPin(u"4444"_s));
    REQUIRE(pos.clockOut());
    REQUIRE(pos.cashTipsAsk()[u"who"_s] == u"Jo"_s);
    CHECK_FALSE(pos.declareCashTips(u"lots"_s));
    REQUIRE(pos.declareCashTips(u"10"_s));
    CHECK(pos.cashTipsAsk().isEmpty());
    CHECK_FALSE(pos.declareCashTips(u"5"_s));                             // asked once
    pos.logout();

    // In: Sam's $20 and Jo's $10 cash. Out by hours x share: Sam 4x100,
    // Jo 4x100, Riley 4x50 -> $12, $12, $6.
    // Columns: Staff, Card tips, Cash tips, Gratuity, Into pool, From pools, Paid out, Owed.
    QStringList sam = tipRow(pos, u"Sam"_s), jo = tipRow(pos, u"Jo"_s), riley = tipRow(pos, u"Riley"_s);
    REQUIRE(sam.size() == 8);
    CHECK(sam[4] == u"$20.00"_s);
    CHECK(sam[5] == u"$12.00"_s);
    CHECK(sam[7] == u"$12.00"_s);
    CHECK(jo[2] == u"$10.00"_s);
    CHECK(jo[4] == u"$10.00"_s);
    CHECK(jo[5] == u"$12.00"_s);
    CHECK(jo[6] == u"$10.00"_s);                                           // kept in cash
    CHECK(jo[7] == u"$2.00"_s);
    CHECK(riley[5] == u"$6.00"_s);
    CHECK(riley[7] == u"$6.00"_s);

    // Riley keeps their own tips (their own setting): Sam and Jo split it.
    pos.shared()->employees[2].tipPool = "out";
    REQUIRE(pos.shared()->employees[2].id == "riley");
    CHECK(tipRow(pos, u"Sam"_s)[5] == u"$15.00"_s);
    CHECK(tipRow(pos, u"Jo"_s)[5] == u"$15.00"_s);
    CHECK(tipRow(pos, u"Riley"_s).isEmpty());
    // Kept on the shift.
    const core::TimePunch &p = *std::ranges::find_if(pos.shared()->punches, [](const core::TimePunch &x) { return x.employeeId == "jo"; });
    const core::TimePunch back = app::punchFromJson(app::toJson(p));
    CHECK(back.cashTipsDeclared);
    CHECK(back.cashTips.cents() == 1000);
    // Settings and the override kept.
    const auto settings = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(settings.tipPool == pos.shared()->settings.tipPool);
    CHECK(settings.declareCashTips);
    CHECK(app::employeeFromJson(app::toJson(pos.shared()->employees[2])).tipPool == "out");
}

TEST_CASE("Manager approval: a manager's PIN lets one void through, on the spot", "[staff][approval]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));                          // Sam: may not void sent items
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(u"cobb"_s);
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());

    // Through invoke (as the screens do): it waits for a manager.
    QVariant result;
    pos.invoke(u"voidItem"_s, {}, [&](const QVariant &r) { result = r; });
    CHECK_FALSE(result.toBool());
    REQUIRE(pos.giveReason(u"Took too long"_s));   // why, first
    QVariantMap approval = pos.approvalInfo();
    REQUIRE(approval[u"needed"_s].toBool());
    CHECK(approval[u"who"_s].toString().startsWith(u"Sam"_s));
    CHECK_FALSE(pos.approve(u"2222"_s));                            // a cashier can't approve it
    CHECK(pos.approvalInfo()[u"needed"_s].toBool());
    REQUIRE(pos.approve(u"1234"_s));                                // Morgan can
    CHECK_FALSE(pos.approvalInfo()[u"needed"_s].toBool());
    CHECK(pos.lines().last().toMap()[u"voided"_s].toBool());        // the coffee is voided
    const QVariantList history = pos.checkHistory();
    REQUIRE_FALSE(history.isEmpty());
    bool approved = false;
    for (const QVariant &h : history)
        approved = approved || h.toMap()[u"what"_s].toString().contains(u"approved by Morgan"_s);
    CHECK(approved);

    // Once only: the next void asks again; Cancel puts it away.
    pos.selectLine(pos.lines().first().toMap()[u"id"_s].toLongLong());
    pos.invoke(u"voidItem"_s, {}, {});
    CHECK_FALSE(pos.reasonAsk().isEmpty());                         // the reason was for that one too
    REQUIRE(pos.giveReason(u"Took too long"_s));
    CHECK(pos.approvalInfo()[u"needed"_s].toBool());
    REQUIRE(pos.cancelApproval());
    CHECK_FALSE(pos.approvalInfo()[u"needed"_s].toBool());
    CHECK_FALSE(pos.lines().first().toMap()[u"voided"_s].toBool());
    CHECK_FALSE(pos.approve(u"1234"_s));                            // nothing waiting
}

TEST_CASE("Practice mode: checks that never reach the kitchen, sales, stock or cash", "[staff][training]")
{
    PosService pos(test::seedPosData(), nullptr);
    // Riley is in training.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    int sam = -1;
    for (int i = 0; i < int(pos.shared()->employees.size()); ++i)
        if (pos.shared()->employees[i].id == "sam") sam = i;
    QVariantMap r = pos.adminRecords(u"employees"_s)[sam].toMap();
    r[u"training"_s] = true;
    REQUIRE(pos.adminSave(u"employees"_s, sam, r));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    const QString drawerBefore = pos.drawerInfo()[u"expected"_s].toString();
    pos.logout();

    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.training());
    const double buns = pos.shared()->ingredient("bun")->onHand;
    REQUIRE(pos.selectTable(u"T1"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    CHECK(pos.checkInfo()[u"label"_s] == u"T1 (practice)"_s);
    CHECK_FALSE(pos.tableStatus(u"T1"_s)[u"open"_s].toBool());      // the real table stays free
    pos.addItem(u"classic-burger"_s);
    REQUIRE(pos.sendOrder());
    CHECK(pos.kitchenTickets().isEmpty());
    CHECK(pos.expoTickets().isEmpty());
    CHECK(pos.shared()->ingredient("bun")->onHand == buns);
    CHECK_FALSE(pos.tender(u"house"_s));                            // no real accounts
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.shared()->closedToday.empty());                       // not a sale
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == drawerBefore);
    pos.logout();

    // A manager can switch their own screen to practice; End of Day clears
    // practice checks left open.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.training());
    REQUIRE(pos.setTraining(true));
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"coffee"_s);
    pos.releaseCheck();
    REQUIRE(pos.setTraining(false));
    REQUIRE(pos.countDrawer() == false);                            // (needs a count)
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.endOfDay());
    CHECK(pos.shared()->open.empty());
    pos.logout();
    REQUIRE(pos.loginWithPin(u"2222"_s));
    CHECK_FALSE(pos.setTraining(true));                             // managers only
}

TEST_CASE("Messages between screens: sent with or without a login, the last hour's", "[staff][messages]")
{
    app::PosShared shared(test::seedPosData(), nullptr);
    qint64 clock = todayAt(12);
    shared.setClock([&] { return clock; });
    PosService kitchen(&shared, u"Kitchen"_s);
    PosService floor(&shared, u"Front"_s);
    CHECK_FALSE(kitchen.sendMessage(u"floor"_s, u"   "_s));
    REQUIRE(kitchen.sendMessage(u"floor"_s, u"86 salmon"_s));      // a kitchen screen, no login
    REQUIRE(floor.loginWithPin(u"1111"_s));
    REQUIRE(floor.sendMessage(u"kitchen"_s, u"Allergy: please check T4"_s));
    REQUIRE(floor.sendMessage(u"Morgan (Manager)"_s, u"Manager please"_s));
    const QVariantList all = floor.messages();
    REQUIRE(all.size() == 3);
    CHECK(all.first().toMap()[u"text"_s] == u"Manager please"_s);  // newest first
    CHECK(all.last().toMap()[u"from"_s] == u"Kitchen"_s);
    CHECK(all.first().toMap()[u"from"_s] == u"Sam"_s);
    clock += 61 * 60'000;
    CHECK(floor.messages().isEmpty());                              // an hour later
}

TEST_CASE("Table turns: minutes from seated to paid, by party size and table", "[staff][turns]")
{
    PosService pos(test::seedPosData(), nullptr);
    qint64 clock = todayAt(12);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    const auto seat = [&](const QString &table, int guests, int minutes) {
        REQUIRE(pos.selectTable(table) == PosService::TableNeedsGuests);
        pos.entryKey(QString::number(guests));
        REQUIRE(pos.startCheck(core::CheckType::DineIn));
        pos.addItem(u"cobb"_s);
        clock += minutes * 60'000;
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
    };
    seat(u"T1"_s, 2, 40);
    seat(u"T1"_s, 2, 60);
    seat(u"T5"_s, 6, 90);
    const QVariantMap r = pos.report(u"turns"_s);
    QStringList t1, small, all;
    for (const QVariant &v : r[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (cells.value(0) == u"T1"_s) t1 = cells;
        if (cells.value(0) == u"1 - 2 guests"_s) small = cells;
        if (cells.value(0) == u"All tables"_s) all = cells;
    }
    REQUIRE(t1.size() == 5);
    CHECK(t1[1] == u"2"_s);
    CHECK(t1[2] == u"50"_s);
    CHECK(small[2] == u"50"_s);
    CHECK(all[1] == u"3"_s);
    CHECK(all[2] == u"63"_s);                                       // (40 + 60 + 90) / 3

    // The floor plan knows when each table was seated.
    REQUIRE(pos.selectTable(u"T3"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    const QVariantMap t3 = pos.tableStatus(u"T3"_s);
    CHECK(t3[u"since"_s].toLongLong() == clock);
    CHECK(t3[u"longAfter"_s].toInt() == 90);
}

TEST_CASE("Pay: a rate per person, other jobs, and which job at clock in", "[staff][pay]")
{
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = todayAt(9);
    pos.shared()->setClock([&] { return clock; });
    REQUIRE(pos.loginWithPin(u"1234"_s));

    // Manager -> Employees: pay and other jobs, checked.
    const int sam = indexOf(pos, u"Sam"_s);
    QVariantMap rec = pos.adminRecords(u"employees"_s)[sam].toMap();
    CHECK(rec[u"payRate"_s].toDouble() == 7.25);
    rec[u"otherJobs"_s] = u"bartender nine"_s;
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, rec));
    rec[u"otherJobs"_s] = u"chef 20.00"_s;               // not a job
    CHECK_FALSE(pos.adminSave(u"employees"_s, sam, rec));
    rec[u"otherJobs"_s] = u"Bartender 9.50\nhost 8"_s;
    REQUIRE(pos.adminSave(u"employees"_s, sam, rec));
    const core::Employee *e = pos.shared()->employee("sam");
    REQUIRE(e);
    REQUIRE(e->jobs().size() == 3);
    CHECK(e->jobs()[1].role == "bartender");
    CHECK(e->jobs()[1].rate.cents() == 950);
    CHECK(app::employeeFromJson(app::toJson(*e)) == *e);
    pos.logout();

    // One job: straight in, at its pay.
    pos.pinKey(u"2"_s); pos.pinKey(u"2"_s); pos.pinKey(u"2"_s); pos.pinKey(u"2"_s);   // Casey
    REQUIRE(pos.clockIn());
    CHECK(pos.clockInJobs().isEmpty());
    CHECK(pos.shared()->punches.back().job == "cashier");
    CHECK(pos.shared()->punches.back().rate.cents() == 1550);

    // Several: which one today? Then that job's pay.
    for (const char *k : {"1", "1", "1", "1"})
        pos.pinKey(QString::fromLatin1(k));
    REQUIRE(pos.clockIn());
    const QVariantMap ask = pos.clockInJobs();
    CHECK(ask[u"who"_s] == u"Sam"_s);
    REQUIRE(ask[u"jobs"_s].toList().size() == 3);
    CHECK(ask[u"jobs"_s].toList()[1].toMap()[u"name"_s] == u"Bartender"_s);
    CHECK_FALSE(pos.shared()->punches.back().employeeId == "sam");   // not in yet
    REQUIRE(pos.clockInAs(u"bartender"_s));
    CHECK(pos.clockInJobs().isEmpty());
    const core::TimePunch &p = pos.shared()->punches.back();
    CHECK(p.employeeId == "sam");
    CHECK(p.job == "bartender");
    CHECK(p.rate.cents() == 950);
    CHECK(app::punchFromJson(app::toJson(p)) == p);

    // A manager clocking someone in uses their main job.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.clockInEmployee(u"riley"_s));
    CHECK(pos.shared()->punches.back().job == "busser");
    CHECK(pos.shared()->punches.back().rate.cents() == 1100);
}

TEST_CASE("Labor cost: each shift at its pay, overtime at time and a half, against sales", "[staff][pay][labor]")
{
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = todayAt(8);
    pos.shared()->setClock([&] { return clock; });
    pos.shared()->settings.overtimeDailyHours = 8;
    pos.shared()->settings.overtimeWeeklyHours = 0;
    pos.shared()->settings.paidBreaks = true;
    REQUIRE(pos.loginWithPin(u"4444"_s));            // Jo: bartender $9, or server $7.25
    REQUIRE(pos.clockIn());
    REQUIRE(pos.clockInAs(u"bartender"_s));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1111"_s));            // Sam: server $7.25
    REQUIRE(pos.clockIn());
    clock = todayAt(12);                             // Sam: 4 h
    REQUIRE(pos.clockOut());

    // $200 of food sold.
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    for (int i = 0; i < 20; ++i)
        pos.addItem(u"cobb"_s);                       // 20 x $12.50 = $250
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    pos.logout();

    clock = todayAt(18);                             // Jo: 10 h, 2 of them overtime
    const core::Report r = pos.buildReport(u"labor"_s);
    CHECK(row(r, "Jo (bartender)").size() == 5);       // Jo has two jobs: the shift says which
    CHECK(rowIn(r, "Labor cost today", "Sam") == std::vector<std::string>{"Sam", "4.00", "$29.00", "-", "$29.00"});
    // 10 h x $9 = $90, + 2 h x $4.50 = $9.
    CHECK(rowIn(r, "Labor cost today", "Jo") == std::vector<std::string>{"Jo", "10.00", "$90.00", "$9.00", "$99.00"});
    CHECK(rowIn(r, "Labor cost today", "Labor cost") == std::vector<std::string>{"Labor cost", "14.00", "$119.00", "$9.00", "$128.00"});
    CHECK(rowIn(r, "Labor cost today", "Net sales")[4] == "$250.00");
    CHECK(rowIn(r, "Labor cost today", "Labor % of sales")[4] == "51.2%");
}

TEST_CASE("Posted messages: shown until they expire, kept, taken down by their author or a manager",
          "[staff][messages][posted]")
{
    app::PosShared shared(test::seedPosData(), nullptr);
    qint64 clock = todayAt(12);
    shared.setClock([&] { return clock; });
    PosService front(&shared, u"Front"_s);
    PosService office(&shared, u"Office"_s);
    REQUIRE(front.loginWithPin(u"1111"_s));                              // Sam
    CHECK_FALSE(front.sendMessage(u"all"_s, u"Old news"_s, todayAt(11)));  // already past
    REQUIRE(front.sendMessage(u"all"_s, u"86 salmon tonight"_s, todayAt(23, 59)));
    QVariantList shown = office.messages();
    REQUIRE(shown.size() == 1);
    CHECK(shown[0].toMap()[u"posted"_s].toBool());
    CHECK(shown[0].toMap()[u"id"_s].toString().startsWith(u'n'));

    clock = todayAt(18);                                                 // still up hours later
    CHECK(office.messages().size() == 1);
    CHECK(app::settingsFromJson(app::toJson(shared.settings)).notices == shared.settings.notices);

    REQUIRE(office.loginWithPin(u"3333"_s));                             // Riley, a busser: not theirs
    CHECK_FALSE(office.removeMessage(shown[0].toMap()[u"id"_s].toString()));
    office.logout();
    REQUIRE(office.loginWithPin(u"1234"_s));                             // a manager can
    REQUIRE(office.removeMessage(shown[0].toMap()[u"id"_s].toString()));
    CHECK(office.messages().isEmpty());

    REQUIRE(front.sendMessage(u"floor"_s, u"Patio closed"_s, todayAt(20)));
    clock = todayAt(20, 1);                                              // gone once it expires
    CHECK(front.messages().isEmpty());
}

TEST_CASE("Setup guide: name, receipt, taxes, items, your own manager, sample staff off", "[staff][setup]")
{
    auto data = test::seedPosData();
    data.settings.setupDone = false;
    PosService pos(std::move(data), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));                           // a server: not theirs
    CHECK_FALSE(pos.setupStore(u"Taco Loco"_s, {}));
    pos.logout();
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.setupInfo()[u"done"_s].toBool());
    CHECK(pos.setupInfo()[u"samples"_s].toInt() == 6);

    REQUIRE(pos.setupStore(u"Taco Loco"_s, u"123 Main St\n555-0100"_s));
    CHECK(pos.shared()->settings.storeName == "Taco Loco");
    CHECK(pos.shared()->settings.receiptHeader == "123 Main St\n555-0100");
    REQUIRE(pos.setupTaxes(8.25, 9.5));
    CHECK(pos.shared()->settings.tax.foodPpm == 82500);
    CHECK(pos.shared()->settings.tax.alcoholPpm == 95000);
    REQUIRE(pos.setupAddItem(u"Fish Tacos"_s, 12.5, u"Tacos"_s));
    CHECK(pos.findItem(u"fish-tacos"_s));
    CHECK(pos.findItem(u"fish-tacos"_s)->family == "tacos");
    CHECK(pos.setupInfo()[u"families"_s].toStringList().contains(u"tacos"_s));

    // The sample staff's PINs are public: off, once there's a manager of our own.
    CHECK_FALSE(pos.setupRetireSamples());
    REQUIRE(pos.setupAddEmployee(u"Ana Ruiz"_s, u"manager"_s, u"8642"_s));
    REQUIRE(pos.setupRetireSamples());
    CHECK(pos.setupInfo()[u"samples"_s].toInt() == 0);
    CHECK(pos.loggedIn());                                          // Morgan finishes the guide first
    REQUIRE(pos.setupFinish());
    CHECK(pos.shared()->settings.setupDone);
    CHECK_FALSE(pos.loggedIn());                                    // then is off, and logged out
    CHECK_FALSE(pos.loginWithPin(u"1234"_s));                        // Morgan, a sample, is off
    REQUIRE(pos.loginWithPin(u"8642"_s));                            // Ana
    CHECK(app::settingsFromJson(app::toJson(pos.shared()->settings)).setupDone);
}

TEST_CASE("Voids and comps made on a screen ask why; the Exceptions report adds them up", "[reasons]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));   // a manager: no approval needed
    const auto run = [&](const QString &method, const QVariantList &args = {}) {
        bool ok = false;
        pos.invoke(method, args, [&](const QVariant &r) { ok = r.toBool(); });
        return ok;
    };
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    const auto check = [&]() -> core::Check & {
        return pos.shared()->open.at(pos.checkInfo()[u"id"_s].toLongLong());
    };
    const auto voided = [&] {
        return std::ranges::count_if(check().lines, [](const core::OrderLine &l) { return l.voided; });
    };

    // Voiding a sent item: asks first; nothing happens until answered.
    CHECK_FALSE(run(u"voidItem"_s));
    QVariantMap ask = pos.reasonAsk();
    REQUIRE(ask[u"kind"_s] == u"void"_s);
    CHECK(ask[u"reasons"_s].toStringList().contains(u"Took too long"_s));
    CHECK(voided() == 0);
    // Cancel: not voided.
    REQUIRE(pos.giveReason(QString()));
    CHECK(pos.reasonAsk().isEmpty());
    CHECK(voided() == 0);
    // A reason: voided, with it.
    CHECK_FALSE(run(u"voidItem"_s));
    REQUIRE(pos.giveReason(u"Took too long"_s));
    CHECK(pos.reasonAsk().isEmpty());
    CHECK(voided() == 1);
    CHECK(check().events.back().reason == "Took too long");
    CHECK(app::checkFromJson(app::toJson(check()))->events.back().reason == "Took too long");

    // A comp asks from its own list.
    pos.addItem(u"coffee"_s);
    CHECK_FALSE(run(u"tender"_s, {u"comp"_s}));
    REQUIRE(pos.reasonAsk()[u"kind"_s] == u"comp"_s);
    CHECK(pos.reasonAsk()[u"reasons"_s].toStringList().contains(u"Guest unhappy"_s));
    REQUIRE(pos.giveReason(u"Guest unhappy"_s));
    CHECK(check().events.back().kind == "discount");
    CHECK(check().events.back().reason == "Guest unhappy");

    // Without a screen (or with the store's setting off) nothing is asked.
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.voidItem());
    CHECK(pos.reasonAsk().isEmpty());

    const core::Report r = pos.buildReport(u"exceptions"_s);
    CHECK(r.find("Took too long") == std::vector<std::string>{"Took too long", "1", r.find("Took too long")[2]});
    CHECK(!r.find("Guest unhappy").empty());
    CHECK(!r.find("(no reason given)").empty());

    // Per employee: this manager isn't asked.
    QVariantList staff = pos.adminRecords(u"employees"_s);
    for (qsizetype i = 0; i < staff.size(); ++i) {
        QVariantMap e = staff[i].toMap();
        if (e[u"name"_s] != pos.userName())
            continue;
        e[u"reasons"_s] = u"skip"_s;
        REQUIRE(pos.adminSave(u"employees"_s, int(i), e));
        CHECK(app::employeeFromJson(app::toJson(pos.shared()->employees[i])).reasons == "skip");
    }
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    CHECK(run(u"voidItem"_s));
    CHECK(pos.reasonAsk().isEmpty());

    // Store Settings: the lists, and asking at all.
    QVariantMap store = pos.adminRecords(u"store"_s)[0].toMap();
    store[u"voidReasons"_s] = u"Spilled\nWrong table"_s;
    store[u"askReasons"_s] = false;
    REQUIRE(pos.adminSave(u"store"_s, 0, store));
    CHECK(pos.shared()->settings.voidReasons == std::vector<std::string>{"Spilled", "Wrong table"});
    CHECK_FALSE(pos.shared()->settings.askReasons);
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(back.voidReasons == pos.shared()->settings.voidReasons);
    CHECK_FALSE(back.askReasons);
}

TEST_CASE("A void's reason waits through a manager's approval", "[reasons]")
{
    app::PosData data = test::seedPosData();
    PosService pos(data, nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));   // a server: voids need a manager
    REQUIRE_FALSE(pos.can(QString::fromLatin1(core::perm::Void)));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    pos.invoke(u"voidItem"_s, {});
    REQUIRE(pos.giveReason(u"Kitchen mistake"_s));
    REQUIRE(pos.approvalInfo()[u"needed"_s].toBool());
    REQUIRE(pos.approve(u"1234"_s));
    const core::Check &c = pos.shared()->open.at(pos.checkInfo()[u"id"_s].toLongLong());
    const core::CheckEvent &e = *std::ranges::find(c.events, std::string("void"), &core::CheckEvent::kind);
    CHECK(e.reason == "Kitchen mistake");
    // Used: the next void asks again.
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder());
    pos.invoke(u"voidItem"_s, {});
    CHECK_FALSE(pos.reasonAsk().isEmpty());
}

#include "app/looks.hh"
#include "layout_fixture.hh"

TEST_CASE("High Contrast: white outlines and bright status colors, undone by another look", "[looks][contrast]")
{
    auto layout = vt::test::loadTestLayout();
    REQUIRE(layout);
    layout::Theme theme = layout->theme;
    const auto look = [](const QString &id) {
        for (const app::Look &l : app::builtInLooks())
            if (l.id == id)
                return l;
        return app::Look{};
    };
    app::applyLook(theme, look(u"contrast"_s));
    CHECK(theme.style.normal.value(u"frame"_s) == u"border"_s);
    CHECK(theme.style.normal.value(u"borderColor"_s) == u"#ffffff"_s);
    CHECK(theme.style.selected.value(u"textColor"_s) == u"#000000"_s);
    CHECK(theme.extra.value(u"status"_s).toObject().value(u"kitchenLate"_s) == u"#ff1744"_s);
    app::applyLook(theme, look(u"dark"_s));
    CHECK(theme.style.normal.value(u"frame"_s) == u"raised"_s);
    CHECK_FALSE(theme.style.normal.contains(u"borderColor"_s));
    CHECK_FALSE(theme.extra.value(u"status"_s).toObject().contains(u"kitchenLate"_s));
}

TEST_CASE("Text size and look: a screen's own, a person's own wins while they're logged in", "[looks][textSize]")
{
    PosService pos(test::seedPosData(), nullptr);
    core::TerminalConfig here;
    here.name = pos.terminalName().toStdString();
    here.textSize = 130;
    here.look = "ocean";
    pos.shared()->settings.terminals.push_back(here);
    CHECK(pos.userPrefs()[u"textSize"_s] == 130);          // nobody logged in: the screen's
    CHECK(pos.terminalLook() == u"ocean"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK(pos.userPrefs()[u"textSize"_s] == 130);          // a person at normal size: the screen's

    // Their own: Largest, and High Contrast on any screen.
    QVariantList staff = pos.adminRecords(u"employees"_s);
    for (qsizetype i = 0; i < staff.size(); ++i) {
        QVariantMap e = staff[i].toMap();
        if (e[u"name"_s] != pos.userName())
            continue;
        e[u"textSize"_s] = u"150"_s;
        e[u"look"_s] = u"contrast"_s;
        REQUIRE(pos.adminSave(u"employees"_s, int(i), e));
        CHECK(app::employeeFromJson(app::toJson(pos.shared()->employees[i])).look == "contrast");
    }
    CHECK(pos.userPrefs()[u"textSize"_s] == 150);
    CHECK(pos.terminalLook() == u"contrast"_s);
    pos.logout();
    CHECK(pos.terminalLook() == u"ocean"_s);

    // Set on Terminals, kept.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantList terms = pos.adminRecords(u"terminals"_s);
    for (qsizetype i = 0; i < terms.size(); ++i) {
        QVariantMap t = terms[i].toMap();
        if (t[u"name"_s] != pos.terminalName())
            continue;
        CHECK(t[u"textSize"_s] == u"130"_s);
        t[u"textSize"_s] = u"150"_s;
        REQUIRE(pos.adminSave(u"terminals"_s, int(i), t));
    }
    const auto back = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(std::ranges::any_of(back.terminals, [](const core::TerminalConfig &t) { return t.textSize == 150; }));
}
