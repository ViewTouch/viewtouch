#include <catch2/catch_test_macros.hpp>

#include "layout_fixture.hh"
#include "app/pos_json.hh"
#include "fake_stripe.hh"
#include "layoutcontroller.hh"
#include "net/remote_session.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QDateTime>
#include <QSettings>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::PosShared;

// What a store does when a terminal drops off mid-way, or two terminals
// reach for the same check: money taken or given back is always recorded,
// and never twice.

namespace {

void login(PosService &pos, const char *pin = "1234")
{
    REQUIRE(pos.loginWithPin(QString::fromLatin1(pin)));
}

void openDrawer(PosService &pos)
{
    login(pos);
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

core::Check oldCheck(std::int64_t id, const char *label, std::int64_t cents, const char *pi)
{
    core::Check c;
    c.id = id;
    c.label = label;
    c.status = core::CheckStatus::Closed;
    c.closedAt = QDateTime::currentMSecsSinceEpoch() - 400LL * 24 * 3'600'000;
    c.openedAt = c.closedAt - 3'600'000;
    core::Payment p;
    p.id = 1;
    p.tenderName = "Credit Card";
    p.kind = core::TenderKind::Card;
    p.amount = Money::fromCents(cents);
    p.processor = "stripe";
    p.reference = pi;
    c.payments.push_back(p);
    return c;
}

void find(PosService &pos, std::int64_t id)
{
    REQUIRE(pos.searchChecks(u"#%1"_s.arg(id), 0));
    for (int i = 0; i < 300 && pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    pos.selectFoundCheck(id);
}

} // namespace

TEST_CASE("A countertop card is recorded even if its register disconnects", "[robust][counter]")
{
    PosShared shared(test::seedPosData(), nullptr);
    auto *register1 = new PosService(&shared, u"Register"_s);
    openDrawer(*register1);
    test::FakeStripe stripe;
    stripe.install(*register1);
    shared.settings.stripeSecretKey = "sk_test_x";
    shared.settings.stripeReaders.push_back({"tmr_1", "Front", "simulated_wisepos_e"});
    REQUIRE(register1->adminSave(u"terminals"_s, -1, {{u"name"_s, u"Register"_s}, {u"cardReader"_s, u"counter:tmr_1"_s}}));
    REQUIRE(register1->startCheck(core::CheckType::Quick));
    REQUIRE(register1->addItem(u"coffee"_s));
    const qint64 check = register1->checkInfo()[u"id"_s].toLongLong();
    REQUIRE(register1->startCounterCharge(u"credit"_s));
    REQUIRE(register1->counterCharge()[u"status"_s] == u"waiting"_s);

    delete register1;                                     // the Wi-Fi drops; its session goes
    stripe.action = u"succeeded"_s;                       // ...and the guest taps their card
    test::waitFor([&] { return !shared.open.at(check).payments.empty(); });
    REQUIRE(shared.open.at(check).payments.size() == 1);
    CHECK(shared.open.at(check).payments[0].reference == "pi_1");
}

TEST_CASE("A refund is recorded even if its register disconnects; never twice from two", "[robust][refunds]")
{
    PosShared shared(test::seedPosData(), nullptr);
    shared.settings.stripeSecretKey = "sk_test_x";
    std::vector<std::function<void(const QString &, const QString &)>> answers;
    shared.stripeRefund = [&](const QString &, std::int64_t, auto done) { answers.push_back(done); };
    const core::Check old = oldCheck(77, "T4", 2000, "pi_old");
    shared.findChecks = [&](const PosShared::CheckFind &) { return std::vector<core::Check>{old}; };

    auto *front = new PosService(&shared, u"Front"_s);
    PosService bar(&shared, u"Bar"_s);
    login(*front);
    login(bar);
    find(*front, 77);
    find(bar, 77);

    // The front asks Stripe for $15, then drops off before Stripe answers.
    REQUIRE(front->refundPayment(77, 1, 1500, u"Wrong item"_s));
    CHECK_FALSE(bar.refundPayment(77, 1, 1000, u"Same guest"_s));   // that payment's refund is on its way
    delete front;
    REQUIRE(answers.size() == 1);
    answers[0](u"re_1"_s, {});
    REQUIRE(shared.refundsToday.size() == 1);
    CHECK(shared.refundsToday[0].reference == "re_1");

    // The bar sees it (its copy of the check is brought up to date)...
    const QVariantMap seen = bar.checkSearch()[u"selected"_s].toMap();
    CHECK(seen[u"refunds"_s].toList().size() == 1);
    CHECK(seen[u"payments"_s].toList()[0].toMap()[u"leftCents"_s] == 500);
    // ...and can't give back more than was paid.
    CHECK_FALSE(bar.refundPayment(77, 1, 1000, u"Same guest"_s));
    REQUIRE(bar.refundPayment(77, 1, 500, u"The rest"_s));
    answers.back()(u"re_2"_s, {});
    CHECK(shared.refundsToday.size() == 2);
    CHECK_FALSE(bar.refundPayment(77, 1, 0, u"Again"_s));
}

TEST_CASE("Undo Payment's card refund comes off the check even if the register disconnects", "[robust][refunds]")
{
    PosShared shared(test::seedPosData(), nullptr);
    shared.settings.stripeSecretKey = "sk_test_x";
    std::function<void(const QString &, const QString &)> answer;
    shared.stripeRefund = [&](const QString &, std::int64_t, auto done) { answer = done; };
    auto *reg = new PosService(&shared, u"Register"_s);
    login(*reg);
    REQUIRE(reg->startCheck(core::CheckType::Quick));
    REQUIRE(reg->addItem(u"coffee"_s));
    const qint64 check = reg->checkInfo()[u"id"_s].toLongLong();
    REQUIRE(reg->recordCardPayment({{u"reference"_s, u"pi_x"_s}, {u"amountCents"_s, 298}, {u"checkId"_s, check},
                                    {u"tenderId"_s, u"credit"_s}, {u"processor"_s, u"stripe"_s}}));
    REQUIRE(reg->removePayment());
    delete reg;
    answer(u"re_x"_s, {});
    CHECK(shared.open.at(check).payments.empty());
    CHECK(shared.open.at(check).events.back().kind == "unpay");
}

TEST_CASE("Small things: a name before closing, last time's course, walk-ins and the wait", "[robust]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);

    // Closing a takeout sends it: the name rule holds there too.
    pos.shared()->settings.requireOrderName = true;
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.tender(u"cash"_s));
    CHECK_FALSE(pos.closeCheck());
    REQUIRE(pos.setCustomer({{u"name"_s, u"Sam"_s}}));
    REQUIRE(pos.closeCheck());
    pos.shared()->settings.requireOrderName = false;

    // Same as Last Time: on this order's course, not held for a later one.
    REQUIRE(pos.saveCustomer({{u"name"_s, u"Dana"_s}, {u"phone"_s, u"555-0101"_s}}));
    core::CustomerRecord &dana = pos.shared()->customers.back();
    core::OrderLine later;
    later.itemId = "coffee";
    later.name = "Coffee";
    later.unitPrice = Money::fromCents(275);
    later.course = 2;
    later.seat = 3;
    dana.lastOrder = {later};
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.useCustomer(QString::fromStdString(dana.id)));
    REQUIRE(pos.sameAsLastTime());
    CHECK(pos.lines()[0].toMap()[u"held"_s] == false);
    CHECK(pos.lines()[0].toMap()[u"seat"_s] == 0);
    pos.releaseCheck();

    // A walk-in seated from the door didn't wait: the average wait is the line's.
    const qint64 kim = pos.addToWaitlist({{u"name"_s, u"Kim"_s}, {u"size"_s, 2}});
    pos.shared()->setClock([] { return QDateTime::currentMSecsSinceEpoch() + 20 * 60'000; });
    REQUIRE(pos.seatPartyAt(kim, {u"T1"_s}, {}));
    REQUIRE(pos.seatWalkIn(2, {u"T2"_s}, {}));
    CHECK(pos.waitlistInfo()[u"seatedToday"_s] == 1);
    CHECK(pos.waitlistInfo()[u"averageWait"_s] == 20);
}

TEST_CASE("Reports over a range: refunds by the day they were made", "[robust][refunds]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    core::Check old = oldCheck(77, "T4", 2000, "pi_old");   // closed over a year ago
    old.payments[0].processor.clear();                       // a card typed in: refunds are recorded
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) {
        return old.closedAt >= from && old.closedAt < to ? std::vector<core::Check>{old} : std::vector<core::Check>{};
    };
    pos.shared()->findChecks = [&](const PosShared::CheckFind &) { return std::vector<core::Check>{old}; };
    std::vector<core::Refund> written;
    pos.shared()->refundHistory = [&](std::int64_t from, std::int64_t to) {
        std::vector<core::Refund> out;
        for (const core::Refund &r : written)
            if (r.at >= from && r.at < to)
                out.push_back(r);
        return out;
    };
    find(pos, 77);
    REQUIRE(pos.refundPayment(77, 1, 0, u"Charged twice"_s));   // recorded (a typed card): today
    const auto range = [&](const QString &period) {
        REQUIRE(pos.requestRangeReport(u"sales"_s, period, {}, {}, false));
        for (int i = 0; i < 300 && pos.rangeReport()[u"loading"_s].toBool(); ++i)
            QTest::qWait(10);
        QString refunds;
        for (const QVariant &row : pos.rangeReport()[u"report"_s].toMap()[u"rows"_s].toList()) {
            const QStringList cells = row.toMap()[u"cells"_s].toStringList();
            if (cells.size() > 1 && cells.first() == u"Refunds"_s)
                refunds = cells.last();
        }
        return refunds;
    };
    CHECK(range(u"week"_s) == u"-$20.00"_s);    // made this week, on a check from last year
    // Written down (as the database would have it): still this week's, once.
    written = pos.shared()->refundsToday;
    CHECK(range(u"week"_s) == u"-$20.00"_s);
}

TEST_CASE("Moving or merging a table's check leaves the old table to bus", "[robust][host]")
{
    PosService pos(test::seedPosData(), nullptr);
    login(pos);
    const auto table = [&](const char *t, int guests) {
        REQUIRE(pos.selectTable(QString::fromLatin1(t)) == PosService::TableNeedsGuests);
        pos.entryKey(QString::number(guests));
        REQUIRE(pos.startCheck(core::CheckType::DineIn));
        REQUIRE(pos.addItem(u"coffee"_s));
        return pos.checkInfo()[u"id"_s].toLongLong();
    };
    const auto state = [&](const QString &t) { return pos.floor().value(t).toMap().value(u"state"_s).toString(); };
    table("T1", 2);
    REQUIRE(pos.moveCheck(u"T4"_s));
    CHECK(state(u"T1"_s) == u"dirty"_s);
    CHECK(state(u"T4"_s) == u"seated"_s);
    pos.releaseCheck();
    const qint64 t2 = table("T2", 2);
    pos.releaseCheck();
    REQUIRE(pos.selectTable(u"T4"_s) != PosService::TableNeedsGuests);
    REQUIRE(pos.mergeCheck(t2));
    CHECK(state(u"T2"_s) == u"dirty"_s);
    CHECK(state(u"T4"_s) == u"seated"_s);
}

TEST_CASE("A card approved while the store is out of reach survives the app closing", "[robust][cards]")
{
    QSettings().remove(u"cards/unsent"_s);
    auto l = vt::test::loadTestLayout();
    REQUIRE(l);
    const QVariantMap approved{{u"approved"_s, true}, {u"reference"_s, u"pi_offline"_s}, {u"brand"_s, u"visa"_s},
                               {u"last4"_s, u"4242"_s}, {u"amountCents"_s, 298}, {u"tipCents"_s, 0},
                               {u"checkId"_s, 1}, {u"tenderId"_s, u"credit"_s}, {u"processor"_s, u"stripe"_s}};
    {
        // A handheld whose store can't be reached: approved, kept on the device.
        net::RemoteSession offline(u"Handheld"_s);
        LayoutController c(*l);
        c.setPos(&offline);
        emit c.cardReader()->finished(approved);
        CHECK(QSettings().value(u"cards/unsent"_s).toString().contains(u"pi_offline"_s));
    }   // ...and the app closes
    // Back, connected: it goes on its check, and is no longer kept.
    PosService pos(test::seedPosData(), nullptr);
    login(pos);
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));
    REQUIRE(pos.checkInfo()[u"id"_s].toLongLong() == 1);
    LayoutController c(*l);
    c.setPos(&pos);
    for (int i = 0; i < 100 && pos.payments().isEmpty(); ++i)
        QTest::qWait(10);
    REQUIRE(pos.payments().size() == 1);
    CHECK(pos.payments()[0].toMap()[u"name"_s].toString().contains(u"4242"_s));
    CHECK_FALSE(QSettings().contains(u"cards/unsent"_s));
}

TEST_CASE("Every screen setting comes back as saved", "[robust][settings]")
{
    core::PosSettings s;
    core::TerminalConfig t;
    t.name = "Patio";
    t.receiptPrinter = "ask";
    t.drawer = "no";
    t.id = "dev-1";
    t.key = "a2V5";
    t.pairedAt = 1234;
    t.screen = "phone";
    t.station = "grill";
    t.look = "dark";
    t.keyboard = "off";
    t.requireName = "yes";
    t.cardReader = "stripe";
    t.afterPaying = "ask";
    s.terminals = {t};
    const core::PosSettings back = app::settingsFromJson(app::toJson(s));
    REQUIRE(back.terminals.size() == 1);
    CHECK(back.terminals[0] == t);
}

TEST_CASE("This screen's card reader: saved, left, and after a restart", "[robust][settings]")
{
    test::RecordingSink sink;
    PosService pos(test::seedPosData(), &sink);
    login(pos);
    const QVariantList records = pos.adminRecords(u"terminals"_s);   // this screen is listed
    int self = -1;
    for (int i = 0; i < records.size(); ++i)
        if (records[i].toMap()[u"name"_s] == pos.terminalName())
            self = i;
    REQUIRE(self >= 0);
    QVariantMap r = records[self].toMap();
    r[u"cardReader"_s] = u"simulated"_s;
    REQUIRE(pos.adminSave(u"terminals"_s, self, r));
    CHECK(pos.adminRecords(u"terminals"_s)[self].toMap()[u"cardReader"_s] == u"simulated"_s);
    CHECK(pos.terminalCardReader() == u"simulated"_s);
    CHECK(pos.adminRecords(u"terminals"_s)[self].toMap()[u"afterPaying"_s].toString().isEmpty());
    // Read back as a restart would.
    const core::PosSettings again = app::settingsFromJson(app::toJson(pos.shared()->settings));
    CHECK(again.terminals[self].cardReader == "simulated");
    CHECK(again.terminals[self].afterPaying.empty());
}
