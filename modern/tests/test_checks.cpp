#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "print/document.hh"
#include "print/tickets.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;

// Managing checks: transfer, move, merge, reopen, and their history.

namespace {

QString idOf(const PosService &pos, const QString &name)
{
    for (const QVariant &v : pos.staff()) {
        if (v.toMap()[u"name"_s].toString().startsWith(name))
            return v.toMap()[u"id"_s].toString();
    }
    return {};
}

qint64 openTable(PosService &pos, const QString &table, const char *item = "coffee")
{
    REQUIRE(pos.selectTable(table) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.addItem(QString::fromLatin1(item));
    return pos.checkInfo()[u"id"_s].toLongLong();
}

QStringList history(const PosService &pos)
{
    QStringList out;
    for (const QVariant &v : pos.checkHistory())
        out << v.toMap()[u"what"_s].toString();
    return out;
}

} // namespace

TEST_CASE("Transfer a check to another server", "[checks]")
{
    PosShared shared(test::seedPosData(), nullptr);
    PosService front(&shared, u"Front"_s);
    PosService bar(&shared, u"Bar"_s);
    REQUIRE(front.loginWithPin(u"1111"_s));              // Sam
    const qint64 id = openTable(front, u"T1"_s);
    CHECK(front.staff().size() >= 3);
    const QString morgan = idOf(front, u"Morgan"_s);
    REQUIRE_FALSE(morgan.isEmpty());

    CHECK_FALSE(front.transferCheck(u"nobody"_s));
    REQUIRE(front.transferCheck(morgan));                // Sam hands over his own check
    CHECK(front.checkInfo()[u"server"_s].toString().startsWith(u"Morgan"_s));
    CHECK(history(front).last().contains(u"Transferred from Sam to Morgan"_s));
    front.releaseCheck();

    // Now it's Morgan's: another server can't take it.
    REQUIRE(bar.loginWithPin(u"2222"_s));
    REQUIRE(bar.openCheck(id));
    CHECK_FALSE(bar.transferCheck(idOf(bar, u"Sam"_s)));
    bar.releaseCheck();
    bar.logout();
    REQUIRE(bar.loginWithPin(u"1234"_s));                // a manager can
    REQUIRE(bar.openCheck(id));
    REQUIRE(bar.transferCheck(idOf(bar, u"Sam"_s)));
}

TEST_CASE("Move a check to another table", "[checks]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    openTable(pos, u"T1"_s);
    CHECK_FALSE(pos.moveCheck(u"T1"_s));
    CHECK_FALSE(pos.moveCheck(u"  "_s));
    REQUIRE(pos.moveCheck(u"T5"_s));
    CHECK(pos.checkInfo()[u"label"_s] == u"T5"_s);
    CHECK(pos.tableStatus(u"T5"_s)[u"open"_s].toBool());
    CHECK_FALSE(pos.tableStatus(u"T1"_s)[u"open"_s].toBool());
    CHECK(history(pos).last() == u"Moved from T1 to T5"_s);

    pos.releaseCheck();
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    pos.addItem(u"coffee"_s);
    CHECK_FALSE(pos.moveCheck(u"T2"_s));                  // only table checks
}

TEST_CASE("Merge a check into the current one", "[checks]")
{
    test::RecordingSink sink;
    PosShared shared(test::seedPosData(), &sink);
    PosService front(&shared, u"Front"_s);
    PosService bar(&shared, u"Bar"_s);
    REQUIRE(front.loginWithPin(u"1111"_s));
    const qint64 a = openTable(front, u"T1"_s, "coffee");
    front.releaseCheck();
    const qint64 b = openTable(front, u"T2"_s, "cobb");
    REQUIRE(front.tender(u"cash"_s, 500));
    front.releaseCheck();

    // Not while the other check is open elsewhere.
    REQUIRE(bar.loginWithPin(u"1234"_s));
    REQUIRE(bar.openCheck(b));
    REQUIRE(front.openCheck(a));
    CHECK_FALSE(front.mergeCheck(b));
    bar.releaseCheck();

    CHECK_FALSE(front.mergeCheck(a));                     // not into itself
    REQUIRE(front.mergeCheck(b));
    CHECK(front.lines().size() == 2);
    CHECK(front.payments().size() == 1);
    CHECK(front.checkInfo()[u"guests"_s].toInt() == 2);
    CHECK(history(front).last().contains(u"merged into T1"_s));
    CHECK_FALSE(shared.open.contains(b));
    CHECK(sink.checks.at(b).status == core::CheckStatus::Merged);
    CHECK(sink.checks.at(b).lines.empty());
}

TEST_CASE("Reopen a closed check; its cash leaves the drawer until it closes again", "[checks]")
{
    PosService pos(test::seedPosData(), nullptr);   // drawer per terminal
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    pos.logout();

    REQUIRE(pos.loginWithPin(u"1111"_s));
    const qint64 id = openTable(pos, u"T3"_s, "cobb");   // $13.53
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$113.53"_s);
    CHECK(pos.closedChecks().isEmpty());                  // servers don't see the list
    CHECK_FALSE(pos.reopenCheck(id));
    pos.logout();

    REQUIRE(pos.loginWithPin(u"1234"_s));
    REQUIRE(pos.closedChecks().size() == 1);
    CHECK(pos.closedChecks().first().toMap()[u"label"_s] == u"T3"_s);
    CHECK_FALSE(pos.reopenCheck(9999));
    REQUIRE(pos.reopenCheck(id));
    CHECK(pos.hasCheck());
    CHECK(pos.checkInfo()[u"id"_s].toLongLong() == id);
    CHECK(pos.closedChecks().isEmpty());
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$100.00"_s);
    CHECK(history(pos).last() == u"Reopened"_s);

    // A refund: take the item off, give the cash back, close again.
    REQUIRE(pos.voidItem());                              // sent: voided by a manager
    pos.selectPayment(pos.payments().first().toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.removePayment());
    REQUIRE(pos.closeCheck());
    CHECK(pos.drawerInfo()[u"expected"_s].toString() == u"$100.00"_s);
    REQUIRE(pos.closedChecks().size() == 1);
}

TEST_CASE("Check history: kept with the check, voids and discounts included", "[checks]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    openTable(pos, u"T4"_s, "cobb");
    REQUIRE(pos.sendOrder());
    REQUIRE(pos.voidItem());
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.tender(u"discount"_s));
    const QStringList h = history(pos);
    REQUIRE(h.size() == 2);
    CHECK(h[0].startsWith(u"Voided"_s));
    CHECK(h[1].startsWith(u"Discount"_s));
    CHECK(pos.checkHistory().first().toMap()[u"who"_s].toString().startsWith(u"Morgan"_s));

    const core::Check &c = pos.shared()->open.begin()->second;
    const auto back = app::checkFromJson(app::toJson(c));
    REQUIRE(back);
    CHECK(back->events == c.events);
}

TEST_CASE("Seats and courses: later courses wait until they are fired", "[checks][courses]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.selectTable(u"T2"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    REQUIRE(pos.setSeat(1));
    pos.addItem(u"cobb"_s);
    REQUIRE(pos.setSeat(2));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.setCourse(2));
    pos.addItem(u"cobb"_s);                                // seat 2, second course

    QVariantList lines = pos.lines();
    REQUIRE(lines.size() == 3);
    CHECK(lines[0].toMap()[u"seat"_s] == 1);
    CHECK(lines[1].toMap()[u"seat"_s] == 2);
    CHECK(lines[2].toMap()[u"course"_s] == 2);
    CHECK(lines[2].toMap()[u"held"_s].toBool());
    CHECK(pos.checkInfo()[u"heldCount"_s] == 1);

    // Send: the first course goes, the second waits.
    REQUIRE(pos.sendOrder());
    lines = pos.lines();
    CHECK(lines[0].toMap()[u"sent"_s].toBool());
    CHECK_FALSE(lines[2].toMap()[u"sent"_s].toBool());
    CHECK_FALSE(pos.sendOrder());                          // nothing more until it's fired
    CHECK(pos.kitchenTickets().size() == 1);
    CHECK(pos.kitchenTickets().first().toMap()[u"lines"_s].toList().first().toMap()[u"seat"_s] == 1);

    REQUIRE(pos.fireCourse());
    lines = pos.lines();
    CHECK(lines[2].toMap()[u"sent"_s].toBool());
    CHECK(pos.checkInfo()[u"firedCourse"_s] == 2);
    CHECK(pos.kitchenTickets().size() == 2);
    CHECK_FALSE(pos.fireCourse());                         // nothing left on hold

    // Re-seating the selected line.
    pos.selectLine(lines[0].toMap()[u"id"_s].toLongLong());
    REQUIRE(pos.setSeat(3));
    CHECK(pos.lines()[0].toMap()[u"seat"_s] == 3);

    // The kitchen ticket says the seat and the course.
    const core::Check &c = pos.shared()->open.begin()->second;
    print::TicketContext ctx{pos.shared()->settings, [](std::int64_t) { return std::string("1/1"); },
                             [](std::int64_t) { return std::string("12:00"); }, 0};
    const std::string ticket = print::renderText(print::kitchenTicket(c, {c.lines[2]}, "Kitchen", false, ctx), 42);
    CHECK(ticket.find("COURSE 2") != std::string::npos);
    CHECK(ticket.find("S2 1") != std::string::npos);

    // Saved with the check.
    const auto back = app::checkFromJson(app::toJson(c));
    REQUIRE(back);
    CHECK(back->lines[2].course == 2);
    CHECK(back->firedCourse == 2);
}

TEST_CASE("Closing a check sends courses still on hold", "[checks][courses]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.selectTable(u"T6"_s) == PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    REQUIRE(pos.setCourse(3));
    pos.addItem(u"coffee"_s);
    REQUIRE(pos.sendOrder() == false);                     // all of it is on hold
    REQUIRE(pos.tender(u"cash"_s));
    REQUIRE(pos.closeCheck());
    CHECK(pos.kitchenTickets().size() == 1);               // it went out on close
}

TEST_CASE("Bar tabs: opened under a name, kept open, found again", "[checks][tabs]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"4444"_s));                 // Jo, the bartender
    CHECK_FALSE(pos.openTab());                           // needs a name
    for (const char ch : {'M', 'i', 'k', 'e'})
        pos.textKey(QString(QChar(ch)));
    REQUIRE(pos.openTab());
    CHECK(pos.checkInfo()[u"label"_s] == u"Mike"_s);
    CHECK(pos.textEntry().isEmpty());
    const qint64 mike = pos.checkInfo()[u"id"_s].toLongLong();
    pos.releaseCheck();                                   // empty, but a tab stays open
    REQUIRE(pos.shared()->open.contains(mike));
    CHECK(pos.shared()->open.at(mike).type == core::CheckType::Tab);

    REQUIRE(pos.openTab(u"Ana"_s));
    pos.addItem(u"draft-beer"_s);
    pos.chooseOption(u"draft"_s, 0);
    pos.finishChoosing();
    REQUIRE(pos.sendOrder());
    pos.releaseCheck();

    // Both on the list as tabs; a dinner table isn't.
    REQUIRE(pos.selectTable(u"T1"_s) == app::PosService::TableNeedsGuests);
    REQUIRE(pos.startCheck(core::CheckType::DineIn));
    pos.releaseCheck();
    int tabs = 0;
    for (const QVariant &c : pos.openChecks())
        tabs += c.toMap()[u"type"_s] == u"tab"_s;
    CHECK(tabs == 2);

    // Back to Mike's tab later.
    REQUIRE(pos.openCheck(mike));
    pos.addItem(u"soda"_s);
    pos.finishChoosing();
    CHECK(pos.lines().size() == 1);
    CHECK(app::checkFromJson(app::toJson(pos.shared()->open.at(mike)))->type == core::CheckType::Tab);
}

TEST_CASE("Orders for later: held, sent by themselves before they're due, another day's wait", "[checks][later]")
{
    app::PosService pos(test::seedPosData(), nullptr);
    const QDate today = QDate::currentDate();
    const auto at = [](QDate d, int h, int m) { return QDateTime(d, QTime(h, m)).toMSecsSinceEpoch(); };
    qint64 clock = at(today, 12, 0);
    pos.setClock([&] { return clock; });
    QStringList notices;
    QObject::connect(&pos, &app::PosService::notice, [&](const QString &n) { notices << n; });
    REQUIRE(pos.loginWithPin(u"1234"_s));

    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    pos.addItem(u"cobb"_s);
    CHECK_FALSE(pos.setDueAt(at(today, 11, 0)));                // already past
    REQUIRE(pos.setDueAt(at(today, 18, 30)));
    CHECK(pos.checkInfo()[u"due"_s].toString().contains(u"6:30"_s));
    REQUIRE(pos.sendOrder());                                    // saved, not sent
    CHECK(notices.last().contains(u"6:10"_s));
    CHECK(pos.shared()->open.at(id).unsentCount() == 1);
    CHECK_FALSE(pos.closeCheck());                               // not until it's picked up
    pos.releaseCheck();

    CHECK(pos.fireDueOrders() == 0);
    clock = at(today, 18, 9);
    CHECK(pos.fireDueOrders() == 0);
    clock = at(today, 18, 10);                                   // 20 minutes before
    CHECK(pos.fireDueOrders() == 1);
    CHECK(pos.shared()->open.at(id).unsentCount() == 0);
    CHECK(notices.last().contains(u"sent to the kitchen"_s));
    CHECK(pos.fireDueOrders() == 0);                             // once
    CHECK(pos.kitchenTickets().last().toMap()[u"due"_s].toString().contains(u"6:30"_s));
    print::TicketContext ctx{pos.shared()->settings, [](std::int64_t) { return std::string("1/1"); },
                             [](std::int64_t) { return std::string("6:30 PM"); }, 0};
    const core::Check &later = pos.shared()->open.at(id);
    CHECK(print::renderText(print::kitchenTicket(later, later.lines, "Kitchen", false, ctx), 42).find("READY AT 6:30 PM")
          != std::string::npos);

    REQUIRE(pos.openCheck(id));
    CHECK_FALSE(pos.setDueAt(at(today, 19, 0)));                 // the kitchen has it
    REQUIRE(pos.tender(u"credit"_s));
    REQUIRE(pos.closeCheck());

    // Tomorrow's: paid tomorrow, and it doesn't hold up tonight's end of day.
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    const qint64 tomorrow = pos.checkInfo()[u"id"_s].toLongLong();
    pos.addItem(u"caesar"_s);
    REQUIRE(pos.setDueAt(at(today.addDays(1), 12, 0)));
    CHECK(pos.checkInfo()[u"due"_s].toString().startsWith(u"tomorrow"_s));
    CHECK_FALSE(pos.tender(u"credit"_s));
    CHECK(app::checkFromJson(app::toJson(pos.shared()->open.at(tomorrow)))->dueAt == at(today.addDays(1), 12, 0));
    pos.releaseCheck();
    notices.clear();
    pos.endOfDay();
    CHECK_FALSE(notices.join(u'|').contains(u"open check"_s));
    CHECK(pos.shared()->open.contains(tomorrow));
}
