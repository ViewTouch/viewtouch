#include <catch2/catch_test_macros.hpp>

#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QDeadlineTimer>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Finding any check, today's or from earlier days, and reprinting it.

namespace {

struct Receipts : app::PosPrinter {
    std::vector<std::int64_t> printed;
    void printKitchen(const core::PosSettings &, const core::Check &, const std::vector<core::OrderLine> &, bool) override {}
    void printReceipt(const core::PosSettings &, const core::Check &c, const std::string &) override { printed.push_back(c.id); }
    void printReport(const core::PosSettings &, const core::Report &, const std::string &) override {}
    void openDrawer(const core::PosSettings &, const std::string &) override {}
};

bool waitFor(const std::function<bool()> &done, int msec = 5000)
{
    QDeadlineTimer deadline(msec);
    while (!done()) {
        if (deadline.hasExpired())
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return true;
}

// Search and wait for the answer: the ids found.
QList<qint64> search(PosService &pos, const QString &query)
{
    REQUIRE(pos.searchChecks(query));
    REQUIRE(waitFor([&] { return !pos.checkSearch()[u"loading"_s].toBool(); }));
    QList<qint64> ids;
    for (const QVariant &r : pos.checkSearch()[u"results"_s].toList())
        ids << r.toMap()[u"id"_s].toLongLong();
    return ids;
}

} // namespace

TEST_CASE("Find a check: by number, amount, name, phone, item; today's and earlier", "[search]")
{
    PosService pos(test::seedPosData(), nullptr);
    Receipts receipts;
    pos.setPrinter(&receipts);
    // Two checks from last month, in the store's history.
    std::vector<core::Check> history;
    {
        core::Check old;
        old.id = 101;
        old.label = "T4";
        old.serverName = "Sam";
        old.status = core::CheckStatus::Closed;
        old.openedAt = QDateTime::currentMSecsSinceEpoch() - 30LL * 24 * 3'600'000;
        old.closedAt = old.openedAt + 3'600'000;
        core::MenuItem cobb;
        cobb.id = "cobb";
        cobb.name = "Cobb";
        cobb.price = Money::fromCents(1250);
        old.addItem(cobb);
        old.customer.name = "Dana Ruiz";
        old.customer.phone = "(555) 010-2244";
        history.push_back(old);
        core::Check other = old;
        other.id = 102;
        other.label = "Bar 2";
        other.customer = {};
        other.closedAt -= 3'600'000;
        history.push_back(other);
    }
    std::int64_t askedFrom = 0;
    pos.shared()->history = [&](std::int64_t from, std::int64_t to) {
        askedFrom = from;
        std::vector<core::Check> out;
        for (const core::Check &c : history)
            if (c.closedAt >= from && c.closedAt < to)
                out.push_back(c);
        return out;
    };

    REQUIRE(pos.loginWithPin(u"1111"_s));
    REQUIRE(pos.startCheck(core::CheckType::Quick));   // today, still open: #1
    pos.addItem(u"soda"_s);
    pos.finishChoosing();
    pos.releaseCheck();

    CHECK(search(pos, u"#101"_s) == QList<qint64>{101});
    CHECK(search(pos, u"101"_s) == QList<qint64>{101});
    CHECK(search(pos, u"dana"_s) == QList<qint64>{101});         // the customer
    CHECK(search(pos, u"0102244"_s) == QList<qint64>{101});      // their phone, digits only
    CHECK(search(pos, u"cobb"_s) == QList<qint64>{101, 102});    // an item; newest first
    const QString total = QString::fromStdString(history[0].totals(pos.shared()->settings.tax).total.toString());
    CHECK(search(pos, total) == QList<qint64>{101, 102});        // the amount
    CHECK(search(pos, u"soda"_s) == QList<qint64>{1});           // today's open check
    CHECK(search(pos, u"nothing like it"_s).isEmpty());
    CHECK(askedFrom < QDateTime::currentMSecsSinceEpoch() - 360LL * 24 * 3'600'000);   // a year back
    CHECK_FALSE(pos.searchChecks(u"x"_s));                         // too short

    // See one, and print a copy.
    search(pos, u"#101"_s);
    pos.selectFoundCheck(101);
    const QVariantMap seen = pos.checkSearch()[u"selected"_s].toMap();
    CHECK(seen[u"label"_s] == u"T4"_s);
    CHECK(seen[u"customer"_s] == u"Dana Ruiz"_s);
    REQUIRE(seen[u"lines"_s].toList().size() == 1);
    CHECK(seen[u"lines"_s].toList()[0].toMap()[u"name"_s] == u"Cobb"_s);
    REQUIRE(pos.reprintCheck(101));
    CHECK(receipts.printed == std::vector<std::int64_t>{101});
    CHECK_FALSE(pos.reprintCheck(999));                             // not found
}
