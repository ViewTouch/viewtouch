#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "print/document.hh"
#include "print/tickets.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Easier for the people on the floor: servers, bartenders, the kitchen, hosts.

namespace {

qint64 todayAt(int hour, int minute = 0)
{
    return QDateTime(QDate::currentDate(), QTime(hour, minute)).toMSecsSinceEpoch();
}

QVariantMap lastLine(PosService &pos)
{
    const QVariantList lines = pos.lines();
    return lines.isEmpty() ? QVariantMap() : lines.last().toMap();
}

struct Floor {
    PosService pos{test::seedPosData(), nullptr};
    qint64 clock = todayAt(12);
    Floor()
    {
        pos.shared()->setClock([this] { return clock; });
        REQUIRE(pos.loginWithPin(u"1111"_s));
    }
    void table(const QString &label)
    {
        REQUIRE(pos.selectTable(label) == PosService::TableNeedsGuests);
        REQUIRE(pos.startCheck(core::CheckType::DineIn));
    }
    core::Check &check() { return pos.shared()->open.at(pos.checkInfo().value(u"id"_s).toLongLong()); }
};

} // namespace

TEST_CASE("Servers: the × key rings six at once; a line's count typed", "[floorstaff][qty]")
{
    Floor f;
    f.table(u"T1"_s);
    REQUIRE(f.pos.setNextQuantity(6));
    CHECK(f.pos.nextQuantity() == 6);
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK(lastLine(f.pos)[u"quantity"_s] == 6);
    CHECK(f.pos.lines().size() == 1);       // one line of six
    CHECK(f.pos.nextQuantity() == 1);       // then as usual
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK(lastLine(f.pos)[u"quantity"_s] == 1);

    // Touched again before an item: as usual.
    REQUIRE(f.pos.setNextQuantity(4));
    REQUIRE(f.pos.setNextQuantity(1));
    REQUIRE(f.pos.addItem(u"cobb"_s));
    CHECK(lastLine(f.pos)[u"quantity"_s] == 1);

    // The pad on a line: 3 of them.
    REQUIRE(f.pos.setLineQuantity(lastLine(f.pos)[u"id"_s].toLongLong(), 3));
    CHECK(lastLine(f.pos)[u"quantity"_s] == 3);

    // Logging out forgets it.
    REQUIRE(f.pos.setNextQuantity(5));
    f.pos.logout();
    CHECK(f.pos.nextQuantity() == 1);
}

TEST_CASE("Servers: a note on the item goes to the kitchen with it", "[floorstaff][itemnote]")
{
    Floor f;
    f.table(u"T1"_s);
    REQUIRE(f.pos.addItem(u"cobb"_s));
    f.pos.addText(u"No onions"_s);
    f.pos.addText(u"On the side"_s);
    CHECK(f.pos.textEntry() == u"No onions, On the side"_s);
    REQUIRE(f.pos.addComment());
    CHECK(f.pos.lines().size() == 1);       // on the Cobb, not a line of its own
    CHECK(lastLine(f.pos)[u"note"_s] == u"No onions, On the side"_s);
    CHECK(f.pos.textEntry().isEmpty());

    // For the whole order: a line of its own, as before.
    f.pos.addText(u"Birthday"_s);
    REQUIRE(f.pos.addComment(true));
    CHECK(f.pos.lines().size() == 2);
    CHECK(lastLine(f.pos)[u"comment"_s].toBool());

    REQUIRE(f.pos.sendOrder());
    const QVariantList ticket = f.pos.kitchenTickets().first().toMap()[u"lines"_s].toList();
    CHECK(ticket.first().toMap()[u"note"_s] == u"No onions, On the side"_s);
    // On the printed ticket, and kept with the check.
    const core::Check &c = f.check();
    print::TicketContext ctx{f.pos.shared()->settings, [](std::int64_t) { return std::string("1/1"); },
                             [](std::int64_t) { return std::string("12:00"); }, 0};
    const std::string printed = print::renderText(print::kitchenTicket(c, c.lines, "Kitchen", false, ctx), 42);
    CHECK(printed.find("** No onions, On the side **") != std::string::npos);
    CHECK(app::checkFromJson(app::toJson(c))->lines.front().note == "No onions, On the side");

    // Sent already, touched: a line of its own that says which.
    f.pos.selectLine(c.lines.front().id);
    f.pos.addText(u"Well done"_s);
    REQUIRE(f.pos.addComment());
    CHECK(lastLine(f.pos)[u"comment"_s].toBool());
    CHECK(lastLine(f.pos)[u"name"_s].toString().startsWith(u"For "_s));
    CHECK(lastLine(f.pos)[u"name"_s].toString().endsWith(u": Well done"_s));
}

TEST_CASE("Servers: a table waiting on its server turns orange, then red; Checked On starts again", "[floorstaff][checkback]")
{
    Floor f;
    f.table(u"T1"_s);
    const auto level = [&] { return f.pos.floor().value(u"T1"_s).toMap().value(u"nudgeLevel"_s).toInt(); };
    CHECK(level() == 0);
    f.clock += 8 * 60'000;                  // nothing ordered in 8 minutes
    CHECK(level() == 1);
    f.clock += 8 * 60'000;
    CHECK(level() == 2);
    REQUIRE(f.pos.checkedOn());
    CHECK(level() == 0);
    REQUIRE(f.pos.addItem(u"cobb"_s));
    REQUIRE(f.pos.sendOrder());
    f.clock += 19 * 60'000;
    CHECK(level() == 0);
    f.clock += 2 * 60'000;                  // 21 minutes since the order
    CHECK(level() == 1);
    CHECK(f.pos.tableStatus(u"T1"_s).value(u"nudgeLevel"_s).toInt() == 1);
    // The store's own limits.
    f.pos.shared()->settings.checkBackMinutes = 10;
    CHECK(level() == 2);
}

TEST_CASE("Servers: the food is up: the server hears it when the whole order is made", "[floorstaff][foodready]")
{
    Floor f;
    f.table(u"T5"_s);
    REQUIRE(f.pos.addItem(u"cobb"_s));
    REQUIRE(f.pos.setNextQuantity(2));
    REQUIRE(f.pos.addItem(u"caesar"_s));
    REQUIRE(f.pos.sendOrder());
    const QVariantMap t = f.pos.kitchenTickets().first().toMap();
    const auto ready = [&] {
        QVariantList out;
        for (const QVariant &v : f.pos.messages())
            if (v.toMap().value(u"kind"_s) == u"ready"_s)
                out << v;
        return out;
    };
    CHECK(ready().isEmpty());
    REQUIRE(f.pos.bumpTicket(t[u"checkId"_s].toLongLong(), t[u"sentAt"_s].toLongLong()));
    REQUIRE(ready().size() == 1);
    const QVariantMap m = ready().first().toMap();
    CHECK(m[u"to"_s] == f.pos.userName());
    CHECK(m[u"text"_s].toString().startsWith(u"T5: food is up ("_s));
    CHECK(m[u"text"_s].toString().contains(u"2 "_s));

    // Off: nothing.
    f.pos.shared()->settings.foodReadyNotice = false;
    REQUIRE(f.pos.addItem(u"coffee"_s));
    REQUIRE(f.pos.addItem(u"cobb"_s));
    REQUIRE(f.pos.sendOrder());
    const QVariantMap t2 = f.pos.kitchenTickets().first().toMap();
    REQUIRE(f.pos.bumpTicket(t2[u"checkId"_s].toLongLong(), t2[u"sentAt"_s].toLongLong()));
    CHECK(ready().size() == 1);
}

TEST_CASE("Bartenders: a tab's card shows how long it's open, the last round and the card held", "[floorstaff][tabs]")
{
    Floor f;
    REQUIRE(f.pos.openTab(u"Riley"_s));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    REQUIRE(f.pos.sendOrder());
    core::Check &c = f.check();
    c.hold.reference = "pi_test";
    c.hold.cardBrand = "Visa";
    c.hold.last4 = "4242";
    c.hold.amount = Money::fromCents(500);
    f.clock += 25 * 60'000;
    QVariantMap tab;
    for (const QVariant &v : f.pos.openChecks())
        if (v.toMap().value(u"type"_s) == u"tab"_s)
            tab = v.toMap();
    REQUIRE_FALSE(tab.isEmpty());
    CHECK(tab[u"hold"_s] == u"Visa 4242 · held $5.00"_s);
    CHECK(tab[u"lastSentAt"_s].toLongLong() > 0);
    CHECK_FALSE(tab[u"nearHold"_s].toBool());
    // Near the hold: 80% of it.
    REQUIRE(f.pos.addItem(u"cobb"_s));
    for (const QVariant &v : f.pos.openChecks())
        if (v.toMap().value(u"type"_s) == u"tab"_s)
            tab = v.toMap();
    CHECK(tab[u"nearHold"_s].toBool());
}

TEST_CASE("Bartenders: Cash Exact and $20 pay and close a tab or a quick sale; not a table", "[floorstaff][fastcash]")
{
    Floor f;
    REQUIRE(f.pos.openDrawerSession());
    REQUIRE(f.pos.openTab(u"Riley"_s));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK(f.pos.checkInfo()[u"fastCash"_s].toBool());
    REQUIRE(f.pos.fastCash(0));
    CHECK_FALSE(f.pos.hasCheck());   // paid and closed
    CHECK(f.pos.shared()->closedToday.back().totals(f.pos.shared()->settings.tax).balance.cents() == 0);

    // A $20 for a quick sale: change.
    REQUIRE(f.pos.startCheck(core::CheckType::Quick));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    REQUIRE(f.pos.fastCash(2000));
    CHECK_FALSE(f.pos.hasCheck());
    CHECK(f.pos.shared()->closedToday.back().totals(f.pos.shared()->settings.tax).change.cents() > 0);

    // Not enough: Pay for the rest.
    REQUIRE(f.pos.startCheck(core::CheckType::Quick));
    REQUIRE(f.pos.setNextQuantity(9));
    REQUIRE(f.pos.addItem(u"cobb"_s));
    CHECK_FALSE(f.pos.fastCash(2000));
    CHECK(f.pos.hasCheck());
    REQUIRE(f.pos.voidItem());
    f.pos.releaseCheck();

    // A table: the Pay screen.
    f.table(u"T3"_s);
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK_FALSE(f.pos.checkInfo()[u"fastCash"_s].toBool());
    CHECK_FALSE(f.pos.fastCash(0));
    // The store turned it off.
    f.pos.releaseCheck();
    f.pos.shared()->settings.fastCash = false;
    REQUIRE(f.pos.openTab(u"Sam"_s));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK_FALSE(f.pos.checkInfo()[u"fastCash"_s].toBool());
}

TEST_CASE("Bartenders: a drink asks for the guest's ID once a check; under age takes it off", "[floorstaff][idcheck]")
{
    Floor f;
    f.pos.shared()->settings.idCheck = true;
    REQUIRE(f.pos.openTab(u"Riley"_s));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());           // not a drink
    REQUIRE(f.pos.addItem(u"draft-beer"_s));
    const QVariantMap ask = f.pos.checkInfo()[u"idAsk"_s].toMap();
    REQUIRE_FALSE(ask.isEmpty());
    CHECK(ask[u"item"_s].toString().contains(u"Beer"_s));
    CHECK(ask[u"bornBy"_s].toString().contains(QString::number(QDate::currentDate().year() - 21)));
    CHECK_FALSE(f.pos.sendOrder());                          // not before the ID
    REQUIRE(f.pos.addItem(u"house-wine"_s));                 // asked once
    REQUIRE(f.pos.idChecked(true));
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());
    CHECK(f.check().idCheckedAt > 0);
    CHECK(f.check().events.back().kind == "id");
    REQUIRE(f.pos.sendOrder());
    REQUIRE(f.pos.addItem(u"draft-beer"_s));                 // the same guest: not again
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());
    f.pos.releaseCheck();

    // Under age: the drinks come off, the coffee stays.
    REQUIRE(f.pos.openTab(u"Kid"_s));
    REQUIRE(f.pos.addItem(u"coffee"_s));
    REQUIRE(f.pos.addItem(u"draft-beer"_s));
    REQUIRE(f.pos.addItem(u"house-wine"_s));
    REQUIRE(f.pos.idChecked(false));
    REQUIRE(f.pos.lines().size() == 1);
    CHECK(lastLine(f.pos)[u"name"_s] == u"Coffee"_s);
    CHECK(f.check().idCheckedAt == 0);
    REQUIRE(f.pos.sendOrder());

    // An item that always asks; a drink that never does.
    for (core::MenuItem &m : f.pos.shared()->menu) {
        if (m.id == "house-wine") m.idCheck = "no";
        if (m.id == "coffee") m.idCheck = "yes";
    }
    f.pos.releaseCheck();
    REQUIRE(f.pos.openTab(u"Third"_s));
    REQUIRE(f.pos.addItem(u"house-wine"_s));
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK_FALSE(f.pos.checkInfo()[u"idAsk"_s].isNull());
    // Taken off again: nothing to ask.
    REQUIRE(f.pos.voidItem());
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());
    // The store doesn't ask.
    f.pos.shared()->settings.idCheck = false;
    REQUIRE(f.pos.addItem(u"coffee"_s));
    CHECK(f.pos.checkInfo()[u"idAsk"_s].isNull());
}
