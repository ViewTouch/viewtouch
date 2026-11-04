#include <catch2/catch_test_macros.hpp>

#include "core/report.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>
#include <QTimeZone>

#include <ctime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Time as a store lives it: clocks that change for daylight saving (a day
// of 23 hours in March, 25 in November), a clock set back by the internet,
// a day left open over a long weekend. The store here is in Chicago.

namespace {

// The computer's zone for one test, put back after.
struct Zone {
    QByteArray was;
    bool had;
    explicit Zone(const char *zone) : was(qgetenv("TZ")), had(qEnvironmentVariableIsSet("TZ"))
    {
        qputenv("TZ", zone);
        tzset();
    }
    ~Zone()
    {
        if (had)
            qputenv("TZ", was);
        else
            qunsetenv("TZ");
        tzset();
    }
};

std::int64_t at(int y, int m, int d, int h, int min)
{
    return QDateTime(QDate(y, m, d), QTime(h, min)).toMSecsSinceEpoch();
}

// The labor report's hours today for one person, as ViewTouch works them out.
std::string hoursToday(const core::TimePunch &p, std::int64_t now)
{
    core::PosSettings settings;
    core::Employee e;
    e.id = p.employeeId;
    e.name = "Sam";
    const core::ReportContext ctx{settings, "Today", [](std::int64_t) { return std::string("1:00 AM"); }, now,
                                  [](std::int64_t ms) { return QDateTime::fromMSecsSinceEpoch(ms).time().hour(); },
                                  [](std::int64_t ms) { return int(QDateTime::fromMSecsSinceEpoch(ms).date().toJulianDay()); },
                                  now - 3 * 86'400'000LL};
    const core::Report r = core::laborReport({p}, {e}, ctx);
    bool hours = false;   // the "Hours and overtime" part: Sam, today, this week...
    for (const core::ReportRow &row : r.rows) {
        if (row.cells.size() == 1)
            hours = row.cells[0] == "Hours and overtime";
        else if (hours && row.cells.size() >= 3 && row.cells[0] == "Sam")
            return row.cells[1];
    }
    return {};
}

} // namespace

TEST_CASE("Time: a shift across the November change is the hours worked, not the clock's", "[time][dst]")
{
    Zone chicago("America/Chicago");
    REQUIRE(QDateTime::fromMSecsSinceEpoch(at(2026, 11, 1, 12, 0)).offsetFromUtc() == -6 * 3600);   // CST that day
    // 10 PM Saturday to 4 AM Sunday: the clock reads 6 hours; 7 were worked (1 AM came twice).
    core::TimePunch p;
    p.id = 1;
    p.employeeId = "sam";
    p.clockIn = at(2026, 10, 31, 22, 0);
    p.clockOut = at(2026, 11, 1, 4, 0);
    REQUIRE(p.clockOut - p.clockIn == 7 * 3'600'000LL);
    CHECK(p.workedMs(p.clockOut, false) == 7 * 3'600'000LL);
    CHECK(hoursToday(p, p.clockOut + 3'600'000) == "7.00");
}

TEST_CASE("Time: a shift across the March change is the hours worked, not the clock's", "[time][dst]")
{
    Zone chicago("America/Chicago");
    // 1:30 to 3:30 AM on March 8, 2026: 2 AM never came; 1 hour worked.
    core::TimePunch p;
    p.id = 1;
    p.employeeId = "sam";
    p.clockIn = at(2026, 3, 8, 1, 30);
    p.clockOut = at(2026, 3, 8, 3, 30);
    REQUIRE(p.clockOut - p.clockIn == 3'600'000LL);
    CHECK(hoursToday(p, p.clockOut + 3'600'000) == "1.00");
}

TEST_CASE("Time: time off on the 25-hour day sees a shift that starts late that night", "[time][dst]")
{
    Zone chicago("America/Chicago");
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    const std::string sam = pos.shared()->employees.front().id;
    // November 1 is 25 hours long: a shift at 11:15 PM is still on it.
    core::Shift late;
    late.id = 77;
    late.employeeId = sam;
    late.start = at(2026, 11, 1, 23, 15);
    late.end = at(2026, 11, 1, 23, 45);
    pos.shared()->shifts.push_back(late);
    core::PosSettings::StaffRequest off;
    off.id = 1;
    off.kind = "timeOff";
    off.employeeId = sam;
    off.at = at(2026, 10, 20, 9, 0);
    off.day = at(2026, 11, 1, 0, 0);
    off.status = "pending";
    pos.shared()->settings.staffRequests.push_back(off);
    const QVariantList records = pos.adminRecords(u"requests"_s);
    REQUIRE(records.size() == 1);
    CHECK(records[0].toMap()[u"summary"_s].toString().contains(u"on the schedule that day"_s));
}

TEST_CASE("Time: the clock set back (the internet fixing it): no order waits a negative time", "[time][clock]")
{
    app::PosData d = test::seedPosData();
    d.settings.orderNumbers = true;
    app::PosShared shared(std::move(d), nullptr);
    std::int64_t clock = at(2026, 6, 5, 12, 10);
    shared.setClock([&clock] { return clock; });
    PosService pos(&shared, u"Counter"_s);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"soda"_s));
    REQUIRE(pos.setCustomer({{u"name"_s, u"Ana"_s}}));
    REQUIRE(pos.sendOrder());
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();
    REQUIRE(pos.orderReady(id));
    clock -= 9 * 60'000;   // set back nine minutes
    const QVariantList orders = pos.truckOrders();
    REQUIRE(orders.size() == 1);
    CHECK(orders[0].toMap()[u"minutes"_s].toInt() >= 0);
    CHECK(orders[0].toMap()[u"readyMinutes"_s].toInt() >= 0);
    for (const QVariant &t : pos.kitchenTickets())
        CHECK(t.toMap()[u"sentAt"_s].toLongLong() <= clock + 9 * 60'000);
}

TEST_CASE("Time: a day left open over a long weekend closes as one day, its reports dated", "[time][days]")
{
    Zone chicago("America/Chicago");
    PosService pos(test::seedPosData(), nullptr);
    std::int64_t clock = at(2026, 7, 3, 11, 0);   // Friday
    pos.shared()->setClock([&clock] { return clock; });
    pos.shared()->day.openedAt = clock;
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    for (int day = 0; day < 3; ++day) {   // Friday, Saturday, Sunday
        clock = at(2026, 7, 3 + day, 13, 0);
        REQUIRE(pos.startCheck(core::CheckType::Takeout));
        REQUIRE(pos.addItem(u"cobb"_s));
        REQUIRE(pos.tender(u"cash"_s));
        REQUIRE(pos.closeCheck());
    }
    clock = at(2026, 7, 6, 9, 0);   // Monday morning: End of Day at last
    pos.entryKey(QString::number(core::expectedCash(pos.shared()->drawers.back(), pos.shared()->closedToday,
                                                    pos.shared()->settings.tax).cents()));
    REQUIRE(pos.countDrawer());
    REQUIRE(pos.endOfDay());
    const app::PastDay &past = pos.shared()->pastDays.front();
    CHECK(past.day.openedAt == at(2026, 7, 3, 11, 0));
    CHECK(past.day.closedAt == clock);
    const QVariantMap sales = pos.report(u"sales"_s, past.day.id);
    bool three = false;
    for (const QVariant &row : sales[u"rows"_s].toList()) {
        const QStringList cells = row.toMap()[u"cells"_s].toStringList();
        if (cells.size() >= 2 && cells[0] == u"Checks"_s)
            three = cells[1] == u"3"_s;
    }
    CHECK(three);
    // Its title says from Friday to Monday, not "today".
    const QString subtitle = sales[u"subtitle"_s].toString();
    CHECK(subtitle.contains(u" to "_s));
    CHECK_FALSE(subtitle.startsWith(u"Today"_s));
}
