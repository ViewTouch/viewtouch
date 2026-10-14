#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Refunds on closed checks (today's and older), by a manager; every closed
// check kept and found, back to the first.

namespace {

void openDrawer(PosService &pos)
{
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
}

qint64 paidCheck(PosService &pos, const QString &tender, const QString &ref = {})
{
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                   // $2.98
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    if (ref.isEmpty()) {
        REQUIRE(pos.tender(tender));
    } else {
        REQUIRE(pos.recordCardPayment({{u"reference"_s, ref}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
                                       {u"amountCents"_s, 298}, {u"tipCents"_s, 0}, {u"checkId"_s, id},
                                       {u"tenderId"_s, tender}, {u"processor"_s, u"stripe"_s}}));
    }
    REQUIRE(pos.closeCheck());
    return id;
}

bool hasRow(const QVariantMap &report, const QString &first)
{
    for (const QVariant &v : report[u"rows"_s].toList())
        if (v.toMap()[u"cells"_s].toStringList().value(0).startsWith(first))
            return true;
    return false;
}

QString rowAmount(const QVariantMap &report, const QString &first)
{
    for (const QVariant &v : report[u"rows"_s].toList()) {
        const QStringList cells = v.toMap()[u"cells"_s].toStringList();
        if (cells.size() > 1 && cells.first() == first)
            return cells.last();
    }
    return {};
}

void waitForSearch(PosService &pos)
{
    for (int i = 0; i < 300 && pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
}

} // namespace

TEST_CASE("Refunds: a manager's, through Stripe or the drawer, partial, on today's reports", "[refunds]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    QStringList refunded;
    pos.shared()->stripeRefund = [&](const QString &pi, std::int64_t cents, auto done) {
        refunded << u"%1 %2"_s.arg(pi).arg(cents);
        done(u"re_%1"_s.arg(refunded.size()), {});
    };
    const qint64 card = paidCheck(pos, u"credit"_s, u"pi_A"_s);
    const qint64 cash = paidCheck(pos, u"cash"_s);
    const auto payment = [&](qint64 check) {
        for (const core::Check &c : pos.shared()->closedToday)
            if (c.id == check)
                return qint64(c.payments.front().id);
        return qint64(0);
    };

    // A server can't: it asks for a manager.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    pos.invoke(u"refundPayment"_s, {card, payment(card), 100, u"Wrong item"_s});   // as the screen asks
    CHECK(pos.approvalInfo()[u"needed"_s].toBool());
    CHECK(refunded.isEmpty());

    // A manager: part of it, back through Stripe.
    REQUIRE(pos.loginWithPin(u"1234"_s));
    CHECK_FALSE(pos.refundPayment(card, payment(card), 100, u"  "_s));          // why?
    CHECK_FALSE(pos.refundPayment(card, payment(card), 299, u"Too much"_s));    // more than was paid
    REQUIRE(pos.refundPayment(card, payment(card), 100, u"Wrong item"_s));
    CHECK(refunded == QStringList{u"pi_A 100"_s});
    const auto checkOf = [&](qint64 id) -> const core::Check & {
        return *std::ranges::find(pos.shared()->closedToday, id, &core::Check::id);
    };
    REQUIRE(checkOf(card).refunds.size() == 1);
    CHECK(checkOf(card).refunds[0].reference == "re_1");
    CHECK(checkOf(card).refunds[0].by == "Morgan (Manager)");
    CHECK(checkOf(card).refunds[0].method == "stripe");
    CHECK(checkOf(card).events.back().kind == "refund");
    CHECK(QString::fromStdString(checkOf(card).events.back().what).contains(u"Wrong item"_s));
    // The rest (0: all that's left); then nothing more.
    REQUIRE(pos.refundPayment(card, payment(card), 0, u"Guest complaint"_s));
    CHECK(refunded.last() == u"pi_A 198"_s);
    CHECK(checkOf(card).refunded(payment(card)).cents() == 298);
    CHECK_FALSE(pos.refundPayment(card, payment(card), 0, u"Again"_s));

    // Cash: out of the drawer, as a refund pay-out.
    REQUIRE(pos.refundPayment(cash, payment(cash), 0, u"Order canceled"_s));
    const core::DrawerSession &d = pos.shared()->drawers.back();
    REQUIRE_FALSE(d.movements.empty());
    CHECK(d.movements.back().kind == core::CashMovement::Kind::Payout);
    CHECK(d.movements.back().amount.cents() == 298);
    CHECK(d.movements.back().category == "Refunds");
    CHECK(refunded.size() == 2);                                              // not through Stripe

    // On today's reports, the day the money went back.
    CHECK(pos.shared()->refundsToday.size() == 3);
    const QVariantMap sales = pos.report(u"sales"_s);
    CHECK(rowAmount(sales, u"Refunds"_s) == u"-$5.96"_s);
    CHECK(hasRow(sales, u"Collected after refunds"_s));
    CHECK(rowAmount(pos.report(u"deposit"_s), u"Card refunds"_s) == u"-$2.98"_s);

    // Kept with the check.
    CHECK(app::checkFromJson(app::toJson(checkOf(card)))->refunds == checkOf(card).refunds);

    // An open check (Sam's): Undo Payment on a Stripe card needs a manager too.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));
    const qint64 open = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.recordCardPayment({{u"reference"_s, u"pi_B"_s}, {u"amountCents"_s, 298}, {u"checkId"_s, open},
                                   {u"tenderId"_s, u"credit"_s}, {u"processor"_s, u"stripe"_s}}));
    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.openCheck(open));
    pos.invoke(u"removePayment"_s, {});
    CHECK(pos.approvalInfo()[u"needed"_s].toBool());
    CHECK(pos.payments().size() == 1);
}

TEST_CASE("Refunds: a check from years ago, found and refunded today", "[refunds]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    pos.shared()->settings.stripeSecretKey = "sk_test_x";
    pos.shared()->stripeRefund = [](const QString &, std::int64_t, auto done) { done(u"re_old"_s, {}); };
    // The database: one check closed three years ago.
    core::Check old;
    old.id = 42;
    old.label = "T5";
    old.status = core::CheckStatus::Closed;
    old.closedAt = QDateTime::currentMSecsSinceEpoch() - 3LL * 365 * 24 * 3'600'000;
    old.openedAt = old.closedAt - 3'600'000;
    old.businessDay = 7;
    core::Payment p;
    p.id = 1;
    p.tenderName = "Credit Card";
    p.kind = core::TenderKind::Card;
    p.amount = Money::fromCents(1000);
    p.processor = "stripe";
    p.reference = "pi_OLD";
    old.payments.push_back(p);
    QList<app::PosShared::CheckFind> asked;
    pos.shared()->findChecks = [&](const app::PosShared::CheckFind &f) {
        asked << f;
        return std::vector<core::Check>{old};
    };

    REQUIRE(pos.searchChecks(u""_s, 0));                                      // every check, ever
    waitForSearch(pos);
    REQUIRE(asked.size() == 1);
    CHECK(asked[0].from == 0);
    CHECK(asked[0].words.isEmpty());
    REQUIRE(pos.checkSearch()[u"results"_s].toList().size() == 1);
    pos.selectFoundCheck(42);
    const QVariantList payments = pos.checkSearch()[u"selected"_s].toMap()[u"payments"_s].toList();
    REQUIRE(payments.size() == 1);
    CHECK(payments[0].toMap()[u"refundable"_s].toBool());
    CHECK(payments[0].toMap()[u"how"_s] == u"stripe"_s);

    REQUIRE(pos.refundPayment(42, 1, 0, u"Charged twice"_s));
    REQUIRE(pos.shared()->refundsToday.size() == 1);
    const core::Refund &r = pos.shared()->refundsToday[0];
    CHECK(r.checkId == 42);
    CHECK(r.checkLabel == "T5");
    CHECK(r.day == pos.shared()->day.id);                                     // today's reports
    const QVariantMap seen = pos.checkSearch()[u"selected"_s].toMap();
    CHECK(seen[u"refunds"_s].toList().size() == 1);
    CHECK(seen[u"payments"_s].toList()[0].toMap()[u"refundable"_s] == false);
    bool inHistory = false;
    for (const QVariant &e : seen[u"events"_s].toList())
        inHistory = inHistory || e.toMap()[u"what"_s].toString().contains(u"Charged twice"_s);
    CHECK(inHistory);

    // Searching: words the record must contain; a check number; a range.
    REQUIRE(pos.searchChecks(u"Cobb"_s, 730));
    waitForSearch(pos);
    CHECK(asked.last().words == QStringList{u"Cobb"_s});
    CHECK(asked.last().from > 0);
    REQUIRE(pos.searchChecks(u"#42"_s, 0));
    waitForSearch(pos);
    CHECK(asked.last().words == QStringList{u"\"id\":42,"_s});
    REQUIRE(pos.searchChecks(u"17.62"_s, 0, 100));                            // an amount: every record, page 2
    waitForSearch(pos);
    CHECK(asked.last().words.isEmpty());
    CHECK(asked.last().offset == 100);
}

TEST_CASE("Every closed check is kept and found: 40 years back, by word, a page at a time", "[refunds][history]")
{
    QTemporaryDir dir;
    const QString path = dir.filePath(u"vt.db"_s);
    const auto seed = test::seedPosData();
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
    }
    // 120 checks, one every 4 months for 40 years, every 10th for "Dana".
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"seed-history"_s);
        db.setDatabaseName(path);
        REQUIRE(db.open());
        QSqlQuery q(db);
        REQUIRE(db.transaction());
        for (int i = 0; i < 120; ++i) {
            core::Check c;
            c.id = 1000 + i;
            c.label = "T1";
            c.status = core::CheckStatus::Closed;
            c.closedAt = now - std::int64_t(i) * 122 * 24 * 3'600'000;
            c.openedAt = c.closedAt - 3'600'000;
            c.customer.name = i % 10 == 0 ? "Dana Ruiz" : "Walk-in";
            q.prepare(u"INSERT INTO checks (id, status, label, server_id, opened_at, closed_at, business_day, json) "
                      "VALUES (?, 'closed', 'T1', '', ?, ?, 1, ?)"_s);
            q.addBindValue(qint64(c.id));
            q.addBindValue(qint64(c.openedAt));
            q.addBindValue(qint64(c.closedAt));
            q.addBindValue(QString::fromUtf8(QJsonDocument(app::toJson(c)).toJson(QJsonDocument::Compact)));
            REQUIRE(q.exec());
        }
        REQUIRE(db.commit());
        db.close();
    }
    QSqlDatabase::removeDatabase(u"seed-history"_s);

    // All of them, newest first, a page at a time.
    const auto page1 = storage::findClosedChecks(path, 0, 0, {}, 50, 0);
    const auto page3 = storage::findClosedChecks(path, 0, 0, {}, 50, 100);
    REQUIRE(page1.size() == 50);
    CHECK(page1.front().id == 1000);                       // the newest
    REQUIRE(page3.size() == 20);
    CHECK(page3.back().id == 1119);                        // nearly 40 years ago
    // A word, any time.
    const auto dana = storage::findClosedChecks(path, 0, 0, {u"dana"_s}, 50, 0);
    CHECK(dana.size() == 12);
    // The last year only.
    const auto year = storage::findClosedChecks(path, now - 365LL * 24 * 3'600'000, 0, {}, 50, 0);
    CHECK(year.size() == 3);
    // Wildcards in what's typed are just letters.
    CHECK(storage::findClosedChecks(path, 0, 0, {u"%"_s}, 50, 0).empty());

    // Refunds are kept, and today's come back after a restart.
    {
        storage::PosStore store(path);
        REQUIRE(store.open());
        storage::AsyncWriter writer(path);
        storage::SqlPosSink sink(writer);
        PosService pos(*store.load(), &sink);
        openDrawer(pos);
        const qint64 id = paidCheck(pos, u"cash"_s);
        REQUIRE(pos.refundPayment(id, pos.shared()->closedToday.back().payments.front().id, 100, u"Wrong item"_s));
    }
    storage::PosStore store(path);
    REQUIRE(store.open());
    const auto data = store.load();
    REQUIRE(data);
    REQUIRE(data->refundsToday.size() == 1);
    CHECK(data->refundsToday[0].reason == "Wrong item");
    REQUIRE_FALSE(data->closedToday.empty());
    CHECK(data->closedToday.back().refunds.size() == 1);
}

TEST_CASE("Refunds: cash is what was kept, not what was handed over", "[refunds]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    // $2.98, paid with a $5 bill: $2.02 back.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.tender(u"cash"_s, 500));
    REQUIRE(pos.closeCheck());
    const core::Check &c = pos.shared()->closedToday.back();
    const core::Payment &p = c.payments.front();
    CHECK(p.amount.cents() == 500);
    CHECK(c.changeFrom(p, pos.shared()->settings.tax).cents() == 202);
    CHECK(c.kept(p, pos.shared()->settings.tax).cents() == 298);
    CHECK_FALSE(pos.refundPayment(id, p.id, 500, u"Wrong item"_s));        // not the $5
    REQUIRE(pos.refundPayment(id, p.id, 0, u"Wrong item"_s));              // all of it: $2.98
    CHECK(pos.shared()->refundsToday.back().amount.cents() == 298);
    CHECK(pos.shared()->drawers.back().movements.back().amount.cents() == 298);

    // A card for $1, the rest in cash with change: the change comes off the cash.
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));
    const qint64 split = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.tender(u"credit"_s, 100));
    REQUIRE(pos.tender(u"cash"_s, 500));
    REQUIRE(pos.closeCheck());
    const core::Check &s = pos.shared()->closedToday.back();
    CHECK(s.kept(s.payments[0], pos.shared()->settings.tax).cents() == 100);
    CHECK(s.kept(s.payments[1], pos.shared()->settings.tax).cents() == 198);
    pos.searchChecks(u"#%1"_s.arg(split), 0);
    for (int i = 0; i < 300 && pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    pos.selectFoundCheck(split);
    const QVariantMap cash = pos.checkSearch()[u"selected"_s].toMap()[u"payments"_s].toList()[1].toMap();
    CHECK(cash[u"leftCents"_s] == 198);
    CHECK(cash[u"change"_s] == u"$3.02"_s);
}

TEST_CASE("Refunds: the bill comes back, the tip stays with the staff", "[refunds]")
{
    PosService pos(test::seedPosData(), nullptr);
    openDrawer(pos);
    REQUIRE(pos.startCheck(core::CheckType::Quick));
    REQUIRE(pos.addItem(u"coffee"_s));                                    // $2.98
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    REQUIRE(pos.recordCardPayment({{u"reference"_s, u"sim_tip"_s}, {u"amountCents"_s, 348}, {u"tipCents"_s, 50},
                                   {u"checkId"_s, id}, {u"tenderId"_s, u"credit"_s}, {u"processor"_s, u"simulated"_s}}));
    REQUIRE(pos.closeCheck());
    const qint64 payment = pos.shared()->closedToday.back().payments.front().id;
    pos.searchChecks(u"#%1"_s.arg(id), 0);
    for (int i = 0; i < 300 && pos.checkSearch()[u"loading"_s].toBool(); ++i)
        QTest::qWait(10);
    pos.selectFoundCheck(id);
    CHECK(pos.checkSearch()[u"selected"_s].toMap()[u"payments"_s].toList()[0].toMap()[u"leftCents"_s] == 298);
    CHECK_FALSE(pos.refundPayment(id, payment, 348, u"Everything"_s));   // not the tip
    REQUIRE(pos.refundPayment(id, payment, 0, u"Wrong item"_s));        // all of it: the bill
    CHECK(pos.shared()->refundsToday.back().amount.cents() == 298);
}
