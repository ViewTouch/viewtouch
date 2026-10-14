#include "net/stress.hh"

#include "app/pos_json.hh"
#include "app/pos_service.hh"
#include "layout/layout.hh"
#include "net/layout_hub.hh"
#include "net/pairing.hh"
#include "net/pos_server.hh"
#include "net/remote_session.hh"
#include "storage/async_writer.hh"
#include "storage/pos_store.hh"

#include <QDateTime>
#include <QFile>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace vt::net {

// --- pretend printers: a slow receipt printer, kitchen and bar ---------------

// ESC/POS printers on this computer, on their own thread like real ones on
// the network: each takes a moment over every ticket, and answers the
// status check (DLE EOT) with "fine".
class FakePrinters : public QObject {
public:
    FakePrinters()
    {
        thread_.setObjectName(u"vt-stress-printers"_s);
        moveToThread(&thread_);
        thread_.start();
        QMetaObject::invokeMethod(this, [this] { listen(); }, Qt::BlockingQueuedConnection);
    }
    ~FakePrinters() override
    {
        QMetaObject::invokeMethod(this, [this] {
            for (QTcpServer *s : servers_)
                delete s;
            servers_.clear();
        }, Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }
    quint16 port(int i) const { return ports_.value(i); }
    int jobs() const { return jobs_; }
    qint64 bytes() const { return bytes_; }

private:
    void listen()
    {
        for (int i = 0; i < 3; ++i) {
            auto *s = new QTcpServer;
            s->listen(QHostAddress::LocalHost, 0);
            ports_ << s->serverPort();
            servers_ << s;
            QObject::connect(s, &QTcpServer::newConnection, s, [this, s] {
                while (QTcpSocket *c = s->nextPendingConnection()) {
                    auto got = std::make_shared<qint64>(0);
                    QObject::connect(c, &QTcpSocket::readyRead, c, [this, c, got] {
                        const QByteArray data = c->readAll();
                        if (data.startsWith("\x10\x04")) {   // how are you?
                            c->write(QByteArray(data.size() / 3 + 1, '\x12'));
                            return;
                        }
                        *got += data.size();
                        bytes_ += data.size();
                        QThread::msleep(40);   // printing takes a moment
                    });
                    QObject::connect(c, &QTcpSocket::disconnected, c, [this, c, got] {
                        if (*got > 16)
                            ++jobs_;
                        c->deleteLater();
                    });
                }
            });
        }
    }

    QThread thread_;
    QList<QTcpServer *> servers_;
    QList<quint16> ports_;
    std::atomic<int> jobs_{0};
    std::atomic<qint64> bytes_{0};
};

// --- the pretend staff -------------------------------------------------------

namespace {

const char *const kFirst[] = {"Ava", "Ben", "Cora", "Dev", "Eli", "Faye", "Gus", "Hana", "Ivan", "June", "Kai", "Lena",
                              "Milo", "Nora", "Omar", "Pia", "Quinn", "Rosa", "Sol", "Tess", "Uma", "Vic", "Wren", "Zoe"};
const char *const kLast[] = {"Garcia", "Kim", "Patel", "Nguyen", "Smith", "Lopez", "Chen", "Brown", "Rossi", "Ali"};
const char *const kStreets[] = {"Oak Ave", "Elm St", "Pine Rd", "Main St", "Lake Dr", "Hill Way"};
const char *const kBrands[] = {"visa", "visa", "mastercard", "amex", "discover"};

// One step for a pretend person: a command (as a screen sends it), or more
// steps worked out at that moment from what their screen shows.
struct Op {
    QString method;
    QVariantList args;
    std::function<QList<Op>()> then;
    bool mayFail = false;   // refused now and then for good reason (sold out, nothing to send)
};
Op cmd(const QString &method, const QVariantList &args = {}, bool mayFail = false)
{
    return Op{method, args, {}, mayFail};
}
Op then(std::function<QList<Op>()> f)
{
    return Op{{}, {}, std::move(f), false};
}

std::int64_t nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

struct StressRunner::Bot {
    enum class Job { Server, Bartender, Host, Phone, Driver, Kitchen, Expo, Manager };

    StressRunner *run;
    Job job;
    QString name, pin, id;
    bool remote = false;
    std::unique_ptr<app::PosSession> owned;
    app::PosSession *s = nullptr;
    QObject ctx;                       // the timers' and replies' owner (gone with the bot)
    QTimer next;
    QRandomGenerator rng;
    QString lastNotice;
    std::atomic<bool> busy{false}, ready{false};
    std::atomic<qint64> busySince{0};

    // Their tables, tabs, phone orders: what's next, and when.
    enum class Stage { Drinks, Food, Eating, Check, Pay, Ready, Out };
    struct Work { Stage stage; std::int64_t due; QString label; int guests = 2; int rounds = 0; };
    QHash<qint64, Work> work;
    // The host's dirty tables (since when), and the next arrivals.
    QHash<QString, std::int64_t> dirtySince;
    double arrivals = 0;
    std::int64_t lastTick = 0;
    QHash<QString, std::int64_t> soldOutUntil;   // the manager's 86es
    int managerTurn = 0;

    Bot(StressRunner *r, Job j, QString n, QString p, QString i, quint32 seed)
        : run(r), job(j), name(std::move(n)), pin(std::move(p)), id(std::move(i)), rng(seed)
    {
        next.setSingleShot(true);
        QObject::connect(&next, &QTimer::timeout, &ctx, [this] { loop(); });
    }

    int pick(int n) { return n > 0 ? int(rng.bounded(n)) : 0; }
    bool chance(int percent) { return pick(100) < percent; }
    std::int64_t minutes(double m) const { return std::int64_t(m * double(run->simMinute())); }
    // A person's touch, then the next one.
    int gap() { return 40 + pick(160); }

    void attach(app::PosSession *session)
    {
        s = session;
        QObject::connect(s, &app::PosSession::notice, &ctx, [this](const QString &m) { lastNotice = m; });
    }

    // Steps one after another, each when the last one's answer is back.
    void go(QList<Op> ops, std::function<void(bool)> done)
    {
        while (!ops.isEmpty() && ops.front().then) {
            const auto f = ops.takeFirst().then;
            ops = f() + ops;
        }
        if (ops.isEmpty()) {
            done(true);
            return;
        }
        const Op op = ops.takeFirst();
        lastNotice.clear();
        QElapsedTimer t;
        t.start();
        run->actionsThisSecond_++;
        s->invoke(op.method, op.args, [this, op, ops, done, t](const QVariant &r) {
            run->recordCommand(op.method, remote, t.nsecsElapsed());
            const bool ok = !(r.metaType().id() == QMetaType::Bool && !r.toBool());
            if (!ok)
                run->recordRefused(name, op.method, lastNotice, op.mayFail);
            if (!ok && !op.mayFail) {
                QTimer::singleShot(gap(), &ctx, [this, done] { recover(); done(false); });
                return;
            }
            QTimer::singleShot(gap(), &ctx, [this, ops, done] { go(ops, done); });
        });
    }

    // Back to a clean screen after something didn't go.
    void recover()
    {
        if (s->approvalInfo().value(u"needed"_s).toBool())
            s->invoke(u"cancelApproval"_s, {});
        if (!s->reasonAsk().isEmpty())
            s->invoke(u"giveReason"_s, {QString()});
        if (s->choosingInfo().value(u"active"_s).toBool())
            s->invoke(u"cancelChoosing"_s, {});
        s->invoke(u"releaseCheck"_s, {});
    }

    void start(int afterMs)
    {
        QList<Op> ops{cmd(u"loginWithPin"_s, {pin})};
        if (job != Job::Kitchen && job != Job::Expo && job != Job::Manager)
            ops << cmd(u"clockIn"_s, {}, true);
        // A bank for cash: servers, the bar, the counter, the driver.
        if (job == Job::Server || job == Job::Bartender || job == Job::Phone || job == Job::Driver)
            ops << cmd(u"entryKey"_s, {u"clear"_s}, true) << cmd(u"entryKey"_s, {u"20000"_s}, true)
                << cmd(u"openDrawerSession"_s, {}, true);
        QTimer::singleShot(afterMs, &ctx, [this, ops, afterMs] {
            if (!s->online()) {   // a handheld still connecting
                start(200);
                return;
            }
            busy = true;
            busySince = nowMs();
            go(ops, [this](bool) {
                busy = false;
                ready = true;
                lastTick = nowMs();
                next.start(200 + pick(400));
            });
        });
    }

    void loop()
    {
        if (busy || run->finished_)
            return;
        busy = true;
        busySince = nowMs();
        act([this] {
            busy = false;
            if (!run->finished_)
                next.start(think());
        });
    }

    int think()
    {
        // Busier: quicker. Kitchen screens look constantly.
        const double i = run->intensity();
        if (job == Job::Kitchen || job == Job::Expo)
            return 150 + pick(250);
        if (job == Job::Manager)
            return 4000 + pick(6000);
        return int((250 + pick(700)) * (1.4 - 0.6 * i));
    }

    // ---- ordering ----

    // An item, its required choices (and some others), as the screen asks.
    QList<Op> order(const QString &item, int depth = 0)
    {
        QList<Op> ops{cmd(u"addItem"_s, {item}, true), then([this, depth] { return choose(depth); })};
        if (item.contains(u"burger"_s) || item == u"mushroom-swiss") {
            static const char *temps[] = {"medium-rare", "medium", "medium-well", "well-done"};
            static const char *sides[] = {"fries", "fries", "side-salad", "onion-rings", "sweet-potato-fries"};
            if (item != u"kids-burger") {
                ops << cmd(u"addItem"_s, {QString::fromLatin1(temps[pick(4)])}, true)
                    << cmd(u"addItem"_s, {QString::fromLatin1(sides[pick(5)])}, true);
            }
        }
        return ops;
    }
    QList<Op> choose(int depth)
    {
        const QVariantMap info = s->choosingInfo();
        if (!info.value(u"active"_s).toBool())
            return {};
        if (depth > 6)
            return {cmd(u"cancelChoosing"_s, {}, true)};
        QList<Op> ops;
        for (const QVariant &gv : info.value(u"groups"_s).toList()) {
            const QVariantMap g = gv.toMap();
            const int options = int(g.value(u"options"_s).toList().size());
            if (options == 0)
                continue;
            if (g.value(u"id"_s) == u"toppings"_s && g.value(u"chosen"_s).toInt() == 0 && chance(30))
                ops << cmd(u"chooseOptionAs"_s, {g.value(u"id"_s), pick(options), chance(60) ? u"no"_s : u"extra"_s}, true);
            else if (!g.value(u"done"_s).toBool() || (g.value(u"chosen"_s).toInt() == 0 && chance(25)))
                ops << cmd(u"chooseOption"_s, {g.value(u"id"_s), pick(options)}, true);
        }
        ops << cmd(u"finishChoosing"_s, {}, true) << then([this, depth] { return choose(depth + 1); });
        return ops;
    }
    QString any(const QStringList &from) { return from.isEmpty() ? QString() : from.at(pick(int(from.size()))); }

    // Something sent comes off: why, then the manager's PIN.
    QList<Op> voidOne()
    {
        for (const QVariant &v : s->lines()) {
            const QVariantMap l = v.toMap();
            if (l.value(u"sent"_s).toBool() && !l.value(u"voided"_s).toBool() && !l.value(u"comment"_s).toBool()
                && !l.value(u"fee"_s).toBool()) {
                return {cmd(u"selectLine"_s, {l.value(u"id"_s)}), cmd(u"voidItem"_s, {}, true), then([this]() -> QList<Op> {
                            if (!s->reasonAsk().isEmpty())
                                return {cmd(u"giveReason"_s, {chance(50) ? u"Guest changed their mind"_s : u"Took too long"_s}, true),
                                        then([this] { return approveIfAsked(); })};
                            return approveIfAsked();
                        })};
            }
        }
        return {};
    }
    QList<Op> approveIfAsked()
    {
        if (s->approvalInfo().value(u"needed"_s).toBool()) {
            run->counted("manager approvals");
            return {cmd(u"approve"_s, {u"1234"_s}, true)};
        }
        return {};
    }

    // ---- paying ----

    // Every check of theirs at this table (after a split, each): card with a
    // tip on the reader, cash rounded up, now and then a gift card's worth.
    QList<Op> payAll(const QString &label, qint64 first)
    {
        QList<qint64> ids;
        for (const QVariant &v : s->openChecks()) {
            const QVariantMap c = v.toMap();
            if (c.value(u"mine"_s).toBool() && (c.value(u"label"_s).toString() == label || c.value(u"id"_s).toLongLong() == first))
                ids << c.value(u"id"_s).toLongLong();
        }
        if (ids.isEmpty())
            ids << first;
        QList<Op> ops;
        for (qint64 checkId : ids)
            ops << cmd(u"openCheck"_s, {checkId}, true) << then([this] { return payThis(); });
        return ops;
    }
    QList<Op> payThis()
    {
        const QVariantMap check = s->checkInfo();
        if (check.isEmpty())
            return {};
        const qint64 due = s->totals().value(u"balanceCents"_s).toLongLong();
        QList<Op> ops;
        if (due > 0) {
            const int how = pick(100);
            if (how < 65) {   // the card reader: the card, the tip with it
                const qint64 tip = due * (15 + pick(10)) / 100;
                ops << cmd(u"recordCardPayment"_s,
                           {QVariantMap{{u"reference"_s, u"stress_%1"_s.arg(rng.generate64(), 0, 16)},
                                        {u"brand"_s, QString::fromLatin1(kBrands[pick(5)])},
                                        {u"last4"_s, QString::number(1000 + pick(9000))},
                                        {u"amountCents"_s, due + tip}, {u"tipCents"_s, tip},
                                        {u"checkId"_s, check.value(u"id"_s)}, {u"tenderId"_s, u"credit"_s},
                                        {u"processor"_s, u"simulated"_s}}});
                run->counted("card payments");
            } else {          // cash, rounded up to the next five
                ops << cmd(u"tender"_s, {u"cash"_s, (due + 499) / 500 * 500});
                run->counted("cash payments");
            }
        }
        ops << cmd(u"closeCheck"_s, {}, true) << then([this]() -> QList<Op> {
            // Still open: whatever is left, in cash.
            if (!s->checkInfo().isEmpty() && s->totals().value(u"balanceCents"_s).toLongLong() > 0)
                return {cmd(u"tender"_s, {u"cash"_s}), cmd(u"closeCheck"_s)};
            run->counted("checks closed");
            return {};
        }) << cmd(u"releaseCheck"_s, {}, true);
        return ops;
    }

    // ---- what each job does next ----

    void act(std::function<void()> done)
    {
        const auto finish = [done](bool) { done(); };
        switch (job) {
        case Job::Server:
        case Job::Bartender: tables(finish); return;
        case Job::Host: host(finish); return;
        case Job::Phone: phone(finish); return;
        case Job::Driver: driver(finish); return;
        case Job::Kitchen: kitchen(finish); return;
        case Job::Expo: expo(finish); return;
        case Job::Manager: manager(finish); return;
        }
    }

    // New arrivals since last time (`perMinute` at the rush).
    int arrivalsNow(double perMinute)
    {
        const std::int64_t t = nowMs();
        if (run->winding())
            return 0;
        arrivals += perMinute * run->intensity() * double(t - lastTick) / double(run->simMinute());
        lastTick = t;
        const int n = int(arrivals);
        arrivals -= n;
        return std::min(n, 3);
    }

    void tables(std::function<void(bool)> done)
    {
        const bool bar = job == Job::Bartender;
        // Theirs now: what the host seated, what they opened.
        QSet<qint64> open;
        for (const QVariant &v : s->openChecks()) {
            const QVariantMap c = v.toMap();
            if (!c.value(u"mine"_s).toBool())
                continue;
            const QString type = c.value(u"type"_s).toString();
            const QString label = c.value(u"label"_s).toString();
            const bool atBar = type == u"tab"_s || run->o_.barSeats.contains(label);
            if (type != u"dineIn"_s && type != u"tab"_s)
                continue;
            if (atBar != bar)
                continue;
            const qint64 checkId = c.value(u"id"_s).toLongLong();
            open.insert(checkId);
            if (!work.contains(checkId))
                work.insert(checkId, {Stage::Drinks, nowMs() + minutes(1 + pick(3)), label, std::max(1, c.value(u"guests"_s).toInt())});
        }
        for (auto it = work.begin(); it != work.end();)
            it = open.contains(it.key()) ? std::next(it) : work.erase(it);

        // The bar: new faces, a tab or a seat.
        if (bar) {
            if (arrivalsNow(0.3) > 0) {
                QList<Op> ops;
                if (chance(50)) {
                    ops << cmd(u"openTab"_s, {QString::fromLatin1(kFirst[pick(24)])}, true);
                } else {
                    QStringList seats;
                    for (const QString &seat : run->o_.barSeats)
                        if (!s->tableStatus(seat).value(u"open"_s).toBool())
                            seats << seat;
                    if (seats.isEmpty())
                        ops << cmd(u"openTab"_s, {QString::fromLatin1(kFirst[pick(24)])}, true);
                    else
                        ops << cmd(u"selectTable"_s, {any(seats)}) << cmd(u"entryKey"_s, {u"clear"_s})
                            << cmd(u"entryKey"_s, {QString::number(1 + pick(2))}) << cmd(u"startCheck"_s, {u"dineIn"_s}, true);
                }
                ops << then([this] {
                    QList<Op> more;
                    for (int i = 0; i < 1 + pick(2); ++i)
                        more << order(any(run->drinks_));
                    return more << cmd(u"sendOrder"_s, {}, true) << cmd(u"releaseCheck"_s, {}, true);
                });
                run->counted("bar guests");
                go(ops, done);
                return;
            }
        }

        // The table that's waiting longest for them.
        const std::int64_t t = nowMs();
        qint64 checkId = 0;
        for (auto it = work.begin(); it != work.end(); ++it) {
            if (run->winding() && it->stage != Stage::Pay && it->stage != Stage::Check)
                it->stage = it->stage == Stage::Drinks || it->stage == Stage::Food ? Stage::Check : it->stage,
                it->due = std::min(it->due, t);
            if (it->due <= t && (checkId == 0 || it->due < work[checkId].due))
                checkId = it.key();
        }
        if (checkId == 0) {
            done(true);
            return;
        }
        Work &w = work[checkId];
        QList<Op> ops{cmd(u"openCheck"_s, {checkId})};
        switch (w.stage) {
        case Stage::Drinks:
            for (int g = 0; g < w.guests; ++g)
                if (chance(75))
                    ops << order(any(run->drinks_));
            ops << cmd(u"sendOrder"_s, {}, true) << cmd(u"releaseCheck"_s);
            if (bar) {
                w.stage = ++w.rounds >= 1 + pick(3) ? Stage::Check : Stage::Drinks;
                w.due = t + minutes(8 + pick(10));
            } else {
                w.stage = Stage::Food;
                w.due = t + minutes(3 + pick(4));
            }
            break;
        case Stage::Food:
            for (int g = 0; g < w.guests; ++g) {
                ops << cmd(u"setSeat"_s, {g + 1}, true) << order(any(run->food_));
                if (chance(20))
                    ops << order(any(run->food_));
            }
            ops << cmd(u"setSeat"_s, {0}, true) << cmd(u"sendOrder"_s, {}, true) << cmd(u"releaseCheck"_s);
            w.stage = Stage::Eating;
            w.due = t + minutes(18 + pick(15));
            break;
        case Stage::Eating:
            if (chance(7)) {
                ops << then([this] { return voidOne(); });
                run->counted("voids");
            }
            if (chance(40)) {   // another round, coffee
                for (int g = 0; g < std::max(1, w.guests / 2); ++g)
                    ops << order(any(run->drinks_));
                ops << cmd(u"sendOrder"_s, {}, true);
            }
            ops << cmd(u"releaseCheck"_s, {}, true);
            w.stage = Stage::Check;
            w.due = t + minutes(5 + pick(8));
            break;
        case Stage::Check:
            ops << cmd(u"printReceipt"_s, {}, true) << cmd(u"releaseCheck"_s, {}, true);
            w.stage = Stage::Pay;
            w.due = t + minutes(2 + pick(4));
            break;
        case Stage::Pay:
        default: {
            if (!bar && w.guests >= 2 && chance(10)) {   // separate checks
                ops << cmd(u"splitEvenly"_s, {2}, true);
                run->counted("split checks");
            }
            const QString label = w.label;
            ops << cmd(u"releaseCheck"_s, {}, true) << then([this, label, checkId] { return payAll(label, checkId); });
            work.remove(checkId);
            break;
        }
        }
        go(ops, done);
    }

    void host(std::function<void(bool)> done)
    {
        const std::int64_t t = nowMs();
        const QVariantMap floor = s->floor();
        QList<Op> ops;
        // People at the door.
        for (int i = arrivalsNow(0.5); i > 0; --i) {
            QVariantMap party{{u"name"_s, QString::fromLatin1(kFirst[pick(24)]) + u' ' + QString::fromLatin1(kLast[pick(10)])},
                              {u"size"_s, 1 + pick(6)}};
            if (chance(70))
                party[u"phone"_s] = u"555-%1-%2"_s.arg(100 + pick(900)).arg(1000 + pick(9000));
            ops << cmd(u"addToWaitlist"_s, {party}, true);
            run->counted("parties arrived");
        }
        // Bussed: a dirty table is clean again after a couple of minutes.
        for (const QString &table : run->o_.tables) {
            const QVariantMap f = floor.value(table).toMap();
            if (f.value(u"state"_s).toString() != u"dirty") {
                dirtySince.remove(table);
                continue;
            }
            if (!dirtySince.contains(table))
                dirtySince.insert(table, t);
            else if (t - dirtySince.value(table) > minutes(2)) {
                ops << cmd(u"setTableState"_s, {table, u"clean"_s}, true);
                dirtySince.remove(table);
                run->counted("tables bussed");
            }
        }
        // The longest waiting party, at a free table, with whoever's next up.
        const QStringList free = run->freeTables(floor);
        const QVariantList waiting = s->waitlistInfo().value(u"waiting"_s).toList();
        if (!free.isEmpty() && !waiting.isEmpty() && !run->winding()) {
            const QVariantMap party = waiting.first().toMap();
            QString server = floor.value(u"_next"_s).toMap().value(u"id"_s).toString();
            if (server.isEmpty() || !run->serverIds().contains(server))
                server = any(run->serverIds());
            ops << cmd(u"seatParty"_s, {party.value(u"id"_s), any(free), server}, true);
            run->counted("parties seated");
        }
        // Parties that gave up waiting.
        if (waiting.size() > 12 && chance(20))
            ops << cmd(u"partyGone"_s, {waiting.last().toMap().value(u"id"_s), false}, true);
        go(ops, done);
    }

    void phone(std::function<void(bool)> done)
    {
        const std::int64_t t = nowMs();
        // The phone rings.
        if (arrivalsNow(0.3) > 0) {
            const bool delivery = chance(35);
            QVariantMap who{{u"name"_s, QString::fromLatin1(kFirst[pick(24)]) + u' ' + QString::fromLatin1(kLast[pick(10)])},
                            {u"phone"_s, u"555-%1-%2"_s.arg(100 + pick(900)).arg(1000 + pick(9000))}};
            if (delivery)
                who[u"address"_s] = u"%1 %2"_s.arg(10 + pick(990)).arg(QString::fromLatin1(kStreets[pick(6)]));
            QList<Op> ops{cmd(u"startCheck"_s, {delivery ? u"delivery"_s : u"takeout"_s}), cmd(u"setCustomer"_s, {who})};
            for (int i = 0; i < 1 + pick(3); ++i)
                ops << order(any(run->food_));
            if (chance(50))
                ops << order(any(run->drinks_));
            ops << then([this, delivery, t] {
                const qint64 checkId = s->checkInfo().value(u"id"_s).toLongLong();
                if (checkId)
                    work.insert(checkId, {delivery ? Stage::Out : Stage::Ready, t + minutes(delivery ? 12 + pick(8) : 15 + pick(10)), {}, 1});
                return QList<Op>{cmd(u"sendOrder"_s, {}, true), cmd(u"releaseCheck"_s, {}, true)};
            });
            run->counted(delivery ? "deliveries" : "takeouts");
            go(ops, done);
            return;
        }
        // Picked up (pay), or out with the driver.
        for (auto it = work.begin(); it != work.end(); ++it) {
            if (it->due > t && !run->winding())
                continue;
            const qint64 checkId = it.key();
            const Stage stage = it->stage;
            work.erase(it);
            if (stage == Stage::Out) {
                go({cmd(u"sendOut"_s, {QVariantList{checkId}, run->driverId_}, true)}, done);
                return;
            }
            go({cmd(u"openCheck"_s, {checkId}, true), then([this] { return payThis(); })}, done);
            return;
        }
        done(true);
    }

    void driver(std::function<void(bool)> done)
    {
        const std::int64_t t = nowMs();
        for (const QVariant &v : s->deliveries()) {
            const QVariantMap d = v.toMap();
            const qint64 checkId = d.value(u"id"_s).toLongLong();
            if (d.value(u"state"_s).toString() != u"out"_s || d.value(u"driverId"_s).toString() != id)
                continue;
            if (!work.contains(checkId))
                work.insert(checkId, {Stage::Out, t + minutes(10 + pick(12)), {}, 1});
        }
        for (auto it = work.begin(); it != work.end(); ++it) {
            if (it->due > t && !run->winding())
                continue;
            const qint64 checkId = it.key();
            work.erase(it);
            go({cmd(u"deliveryBack"_s, {checkId}, true), cmd(u"openCheck"_s, {checkId}, true),
                then([this] { return payThis(); })},
               done);
            return;
        }
        done(true);
    }

    void kitchen(std::function<void(bool)> done)
    {
        // The oldest ticket that's had its time (6 to 14 minutes, by ticket).
        const std::int64_t t = nowMs();
        for (const QVariant &v : s->kitchenTickets()) {
            const QVariantMap k = v.toMap();
            const qint64 checkId = k.value(u"checkId"_s).toLongLong(), sentAt = k.value(u"sentAt"_s).toLongLong();
            const double cook = 6 + double((checkId * 7 + sentAt / 1000) % 9);
            if (t - sentAt >= minutes(run->winding() ? 0.5 : cook)) {
                run->counted("tickets bumped");
                go({cmd(u"bumpTicket"_s, {checkId, sentAt, k.value(u"station"_s)}, true)}, done);
                return;
            }
        }
        done(true);
    }

    void expo(std::function<void(bool)> done)
    {
        const std::int64_t t = nowMs();
        for (const QVariant &v : s->expoTickets()) {
            const QVariantMap k = v.toMap();
            const qint64 sentAt = k.value(u"sentAt"_s).toLongLong();
            if (k.value(u"ready"_s).toBool() && t - sentAt >= minutes(1)) {   // all made: out it goes
                run->counted("expo runs");
                go({cmd(u"expoBump"_s, {k.value(u"checkId"_s), sentAt}, true)}, done);
                return;
            }
        }
        done(true);
    }

    void manager(std::function<void(bool)> done)
    {
        // The office: reports during service, an item 86ed and back, the dashboard.
        static const char *reports[] = {"sales", "servers", "hourly", "exceptions", "items", "tips", "kitchen", "foodcost"};
        QList<Op> ops;
        switch (managerTurn++ % 4) {
        case 0:
        case 2:
            ops << then([this] {
                QElapsedTimer t;
                t.start();
                const QString id = QString::fromLatin1(reports[pick(8)]);
                s->report(id);   // made on the spot, like a manager touching it
                run->recordCommand(u"report:"_s + id, remote, t.nsecsElapsed());
                run->counted("reports run");
                return QList<Op>{};
            });
            break;
        case 1: {
            const std::int64_t t = nowMs();
            for (auto it = soldOutUntil.begin(); it != soldOutUntil.end(); ++it)
                if (it.value() <= t) {
                    ops << cmd(u"setAvailable"_s, {it.key(), true}, true);
                    soldOutUntil.erase(it);
                    break;
                }
            if (ops.isEmpty() && soldOutUntil.isEmpty() && chance(50)) {
                const QString item = any(run->food_);
                ops << cmd(u"setAvailable"_s, {item, false}, true);
                soldOutUntil.insert(item, t + minutes(15));
                run->counted("items 86ed");
            }
            break;
        }
        case 3:
            ops << cmd(u"requestForecast"_s, {1}, true);
            break;
        }
        go(ops, done);
    }
};

// --- the run -------------------------------------------------------------------

StressRunner::StressRunner(app::PosShared *shared, LayoutHub *hub, StressOptions options, QObject *parent)
    : QObject(parent)
    , shared_(shared)
    , hub_(hub)
    , o_(std::move(options))
{
    o_.minutes = std::clamp(o_.minutes, 1, 600);
    o_.speed = std::clamp(o_.speed, 1.0, 120.0);
    o_.servers = std::clamp(o_.servers, 1, 24);
    o_.watchers = std::clamp(o_.watchers, 0, 24);
    sampler_.setTimerType(Qt::PreciseTimer);
    sampler_.setInterval(20);
    connect(&sampler_, &QTimer::timeout, this, &StressRunner::sample);
    second_.setInterval(1000);
    connect(&second_, &QTimer::timeout, this, &StressRunner::everySecond);
}

StressRunner::~StressRunner()
{
    // The handhelds go first, on their own thread (then their bots come home).
    if (handhelds_.isRunning()) {
        QThread *home = thread();
        QMetaObject::invokeMethod(handheldHome_, [this, home] {
            for (auto &b : bots_)
                if (b->remote) {
                    b->next.stop();
                    b->owned.reset();
                    b->ctx.moveToThread(home);
                    b->next.moveToThread(home);
                }
            watchers_.clear();
            handheldHome_->moveToThread(home);
        }, Qt::BlockingQueuedConnection);
        handhelds_.quit();
        handhelds_.wait();
    }
    delete handheldHome_;
    bots_.clear();
}

std::int64_t StressRunner::simMinute() const
{
    return std::max<std::int64_t>(200, std::llround(60'000.0 / o_.speed));
}

double StressRunner::intensity() const
{
    // Opening, the rush in the middle, and quieter at the end.
    const double f = std::clamp(double(clock_.elapsed()) / (o_.minutes * 60'000.0), 0.0, 1.0);
    if (f < 0.3)
        return 0.35 + 0.65 * f / 0.3;
    if (f < 0.7)
        return 1.0;
    return 1.0 - 0.5 * (f - 0.7) / 0.3;
}

void StressRunner::tablesOn(const layout::Layout &layout, QStringList &tables, QStringList &barSeats)
{
    for (const layout::Page &p : layout.pages) {
        if (!p.variantOf.isEmpty())
            continue;   // a phone's version: the same tables
        for (const layout::Zone &z : p.zones) {
            const QString name = z.label.trimmed();
            if (z.kind != u"table" || name.isEmpty() || tables.contains(name) || barSeats.contains(name))
                continue;
            (name.startsWith(u"Bar"_s, Qt::CaseInsensitive) ? barSeats : tables) << name;
        }
    }
}

QStringList StressRunner::freeTables(const QVariantMap &floor) const
{
    QStringList out;
    for (const QString &t : o_.tables)
        if (!floor.contains(t))
            out << t;
    return out;
}

bool StressRunner::start(QString *error)
{
    const auto fail = [error](const QString &e) {
        if (error)
            *error = e;
        return false;
    };
    // The menu: drinks, and plates (no breakfast, no tickets).
    for (const core::MenuItem &m : shared_->menu) {
        if (m.isModifier || m.price.cents() <= 0 || !m.available)
            continue;
        if (m.family == "drinks")
            drinks_ << QString::fromStdString(m.id);
        else if (m.family != "breakfast" && m.family != "events" && m.family != "temperature" && m.family != "sides")
            food_ << QString::fromStdString(m.id);
    }
    if (food_.isEmpty() || drinks_.isEmpty())
        return fail(u"The menu needs drinks and food."_s);
    if (o_.tables.isEmpty())
        return fail(u"No tables on the floor plans."_s);

    // The staff: servers, a host, a driver, the kitchen (and the store's own
    // bartender, cashier and manager).
    auto &staff = shared_->employees;
    const auto person = [&](const QString &name, const char *role, const QString &pin) {
        core::Employee e;
        e.id = "stress-" + std::to_string(staff.size());
        e.name = name.toStdString();
        e.role = role;
        e.pinSalt = app::newSalt();
        e.pinHash = app::hashPin(pin, e.pinSalt);
        staff.push_back(e);
        return QString::fromStdString(e.id);
    };
    const auto idOfPin = [&](const char *pin) {
        for (const core::Employee &e : staff)
            if (e.active && e.pinHash == app::hashPin(QString::fromLatin1(pin), e.pinSalt))
                return QString::fromStdString(e.id);
        return QString();
    };
    if (idOfPin("1234").isEmpty() || idOfPin("4444").isEmpty() || idOfPin("2222").isEmpty())
        return fail(u"The starter staff (PINs 1234, 4444, 2222) are needed."_s);
    struct Who { Bot::Job job; QString name, pin, id; };
    std::vector<Who> who;
    for (int i = 0; i < o_.servers; ++i) {
        const QString name = QString::fromLatin1(kFirst[i % 24]) + u" (server)"_s;
        const QString pin = QString::number(6100 + i);
        who.push_back({Bot::Job::Server, name, pin, person(name, "server", pin)});
        serverIds_ << who.back().id;
    }
    who.push_back({Bot::Job::Host, u"Hostess"_s, u"6201"_s, person(u"Hostess"_s, "host", u"6201"_s)});
    driverId_ = person(u"Lou (driver)"_s, "driver", u"6301"_s);
    who.push_back({Bot::Job::Driver, u"Lou (driver)"_s, u"6301"_s, driverId_});
    who.push_back({Bot::Job::Kitchen, u"Kitchen"_s, u"6401"_s, person(u"Kitchen"_s, "cook", u"6401"_s)});
    who.push_back({Bot::Job::Bartender, u"Jo"_s, u"4444"_s, idOfPin("4444")});
    who.push_back({Bot::Job::Phone, u"Casey"_s, u"2222"_s, idOfPin("2222")});
    who.push_back({Bot::Job::Expo, u"Expo"_s, u"1234"_s, idOfPin("1234")});
    who.push_back({Bot::Job::Manager, u"Manager"_s, u"1234"_s, idOfPin("1234")});

    // Tickets go to slow pretend printers, over the network like real ones.
    printers_ = std::make_unique<FakePrinters>();
    for (int i = 0; i < int(shared_->settings.printers.size()); ++i) {
        core::PrinterConfig &p = shared_->settings.printers[i];
        p.type = "network";
        p.host = "127.0.0.1";
        p.port = printers_->port(i % 3);
        p.format = "escpos";
    }
    emit shared_->adminChanged();

    // The handhelds' way in: this store's own server, on this computer only.
    server_ = std::make_unique<PosServer>(shared_, hub_);
    if (!server_->listen(QHostAddress::LocalHost, 0))
        return fail(u"Could not serve the handhelds: "_s + server_->errorString());
    const auto credentials = [this](const QString &name) {
        core::TerminalConfig t;
        t.name = name.toStdString();
        t.id = newDeviceId().toStdString();
        const QByteArray key = newDeviceKey();
        t.key = key.toBase64().toStdString();
        t.screen = "phone";
        shared_->settings.terminals.push_back(t);
        Credentials c;
        c.host = u"127.0.0.1"_s;
        c.port = server_->port();
        c.terminalId = QString::fromStdString(t.id);
        c.terminalName = name;
        c.key = key;
        return c;
    };
    handheldHome_ = new QObject;
    handheldHome_->moveToThread(&handhelds_);
    handhelds_.setObjectName(u"vt-stress-handhelds"_s);
    handhelds_.start();

    int n = 0;
    for (const Who &w : who) {
        auto bot = std::make_unique<Bot>(this, w.job, w.name, w.pin, w.id, quint32(20261013 + n));
        // Every other server, the kitchen screen and the driver: handhelds.
        bot->remote = (w.job == Bot::Job::Server && n % 2 == 1) || w.job == Bot::Job::Kitchen || w.job == Bot::Job::Driver;
        const QString terminal = u"%1's %2"_s.arg(w.name, bot->remote ? u"handheld"_s : u"screen"_s);
        if (bot->remote) {
            // On the handhelds' thread: it connects, then starts work.
            bot->ctx.moveToThread(&handhelds_);
            bot->next.moveToThread(&handhelds_);
            const Credentials c = credentials(terminal);
            Bot *b = bot.get();
            const int after = 150 * n;
            QMetaObject::invokeMethod(&b->ctx, [b, c, terminal, after] {
                auto r = std::make_unique<RemoteSession>(terminal);
                r->setCredentials(c);
                r->connectTo(c.host, c.port);
                b->owned = std::move(r);
                b->attach(b->owned.get());
                b->start(after);
            });
        } else {
            bot->owned = std::make_unique<app::PosService>(shared_, terminal);
            bot->attach(bot->owned.get());
            bot->start(150 * n);
        }
        bots_.push_back(std::move(bot));
        ++n;
    }
    // Screens that only watch (the host stand's iPad, the office...), logged in.
    for (int i = 0; i < o_.watchers; ++i) {
        const Credentials c = credentials(u"Watching screen %1"_s.arg(i + 1));
        QMetaObject::invokeMethod(handheldHome_, [this, c] {
            auto r = std::make_unique<RemoteSession>(c.terminalName);
            r->setCredentials(c);
            r->connectTo(c.host, c.port);
            RemoteSession *raw = r.get();
            QObject::connect(raw, &app::PosSession::onlineChanged, raw, [raw] {
                if (raw->online() && !raw->loggedIn())
                    raw->invoke(u"loginWithPin"_s, {u"1234"_s});
            });
            watchers_.push_back(std::move(r));
        });
    }
    // Everyone on the clock (the manager does it, as on a real night).
    if (auto *office = qobject_cast<app::PosService *>(bots_.back()->owned.get())) {
        office->loginWithPin(u"1234"_s);
        for (const Who &w : who)
            if (w.job != Bot::Job::Manager && w.job != Bot::Job::Expo)
                office->clockInEmployee(w.id);
    }

    clock_.start();
    lastSample_ = clock_.elapsed();
    cpuAt_ = clock_.elapsed();
    sampler_.start();
    second_.start();
    return true;
}

void StressRunner::sample()
{
    const qint64 t = clock_.elapsed();
    const int lag = int(std::max<qint64>(0, t - lastSample_ - 20));
    lastSample_ = t;
    lagWindow_.push_back(lag);
    lagAll_.push_back(lag);
    worstLag_ = std::max(worstLag_, lag);
    if (lag >= 100) ++stalls100_;
    if (lag >= 250) ++stalls250_;
    if (lag >= 1000) ++stalls1000_;
}

void StressRunner::recordCommand(const QString &method, bool remote, qint64 nsecs)
{
    const QMutexLocker lock(&mutex_);
    Timing &t = (remote ? remote_ : local_)[method];
    ++t.n;
    t.total += nsecs;
    t.worst = std::max(t.worst, nsecs);
    t.us.push_back(int(std::min<qint64>(nsecs / 1000, 2'000'000'000)));
}

void StressRunner::recordRefused(const QString &who, const QString &method, const QString &why, bool expected)
{
    const QMutexLocker lock(&mutex_);
    ++refused_;
    if (!expected)
        ++unexpected_;
    refusedWhy_[u"%1%2: %3 - %4"_s.arg(expected ? QString() : u"(!) "_s, who, method, why.isEmpty() ? u"(no reason given)"_s : why)]++;
}

namespace {

int percentile(std::vector<int> v, double p)
{
    if (v.empty())
        return 0;
    const size_t i = std::min(v.size() - 1, size_t(std::ceil(p * double(v.size())) - 1));
    std::nth_element(v.begin(), v.begin() + std::ptrdiff_t(i), v.end());
    return v[i];
}

// This process's memory (MB) and CPU time (clock ticks).
double residentMb()
{
    QFile f(u"/proc/self/statm"_s);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const QList<QByteArray> parts = f.readAll().split(' ');
    return parts.size() > 1 ? double(parts[1].toLongLong()) * double(sysconf(_SC_PAGESIZE)) / 1048576.0 : 0;
}
qint64 cpuTicks()
{
    QFile f(u"/proc/self/stat"_s);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const QByteArray s = f.readAll();
    const QList<QByteArray> parts = s.mid(s.lastIndexOf(')') + 2).split(' ');
    return parts.size() > 12 ? parts[11].toLongLong() + parts[12].toLongLong() : 0;
}

} // namespace

void StressRunner::everySecond()
{
    actionsPerSecond_.push_back(actionsThisSecond_);
    actionsThisSecond_ = 0;
    fps_ = frames_;
    frames_ = 0;
    if (o_.writer) {
        dbPending_ = o_.writer->pending();
        dbWorst_ = std::max(dbWorst_, dbPending_);
    }
    const qint64 ticks = cpuTicks(), t = clock_.elapsed();
    if (cpuTicks_ > 0 && t > cpuAt_)
        cpu_ = 100.0 * double(ticks - cpuTicks_) / double(sysconf(_SC_CLK_TCK)) / (double(t - cpuAt_) / 1000.0);
    cpuTicks_ = ticks;
    cpuAt_ = t;
    rssMb_ = residentMb();
    rssWorstMb_ = std::max(rssWorstMb_, rssMb_);

    // Anyone stuck on an answer that never came: back to work.
    for (auto &b : bots_)
        if (b->busy && b->ready && nowMs() - b->busySince > 30'000) {
            recordRefused(b->name, u"(waiting)"_s, u"no answer for 30 seconds"_s, false);
            Bot *raw = b.get();
            QMetaObject::invokeMethod(&raw->ctx, [raw] {
                raw->busy = false;
                raw->recover();
                raw->next.start(500);
            });
        }

    if (server_ && actionsPerSecond_.size() % 10 == 0 && !finished_) {
        const int rounds = server_->updateRounds() - lastUpdateRounds_;
        const double ms = rounds ? double(server_->updateNsecs() - lastUpdateNsecs_) / 1e6 / rounds : 0;
        lastUpdateRounds_ = server_->updateRounds();
        lastUpdateNsecs_ = server_->updateNsecs();
        int open = 0;
        for (const auto &[checkId, c] : shared_->open)
            open += c.training ? 0 : 1;
        timeline_ << u"  %1 s: %2 open, %3 closed; update rounds %4 x %5 ms; screen delay 1 in 100: %6 ms"_s
                         .arg(actionsPerSecond_.size(), 4).arg(open, 3).arg(shared_->closedToday.size(), 4).arg(rounds, 4)
                         .arg(ms, 5, 'f', 1).arg(percentile(lagWindow_, 0.99));
    }
    if (!measured_ && clock_.elapsed() >= qint64(o_.minutes) * 30'000) {
        measured_ = true;
        measureUpdate();
    }
    if (!winding_ && clock_.elapsed() >= qint64(o_.minutes) * 60'000)
        windDown();
    if (winding_ && !finished_) {
        // Done when every check is closed (or after three minutes of trying).
        bool anyOpen = false;
        for (const auto &[checkId, c] : shared_->open)
            if (!c.training)
                anyOpen = true;
        if (!anyOpen || clock_.elapsed() - windStartedMs_ > 180'000)
            finish();
    }
    if (lagWindow_.size() > 3000)
        lagWindow_.erase(lagWindow_.begin(), lagWindow_.end() - 3000);   // the last minute
    last_ = {};
    emit statsChanged();
}

void StressRunner::measureUpdate()
{
    // A manager's screen in the middle of the rush: the whole update (what
    // every screen gets after any change), and its slowest pieces.
    app::PosService probe(shared_, u"Measuring"_s);
    probe.loginWithPin(u"1234"_s);
    if (!shared_->open.empty())
        probe.openCheck(shared_->open.begin()->first);
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 5; ++i)
        probe.snapshot();
    const double whole = double(t.nsecsElapsed()) / 5e6;
    std::vector<std::pair<double, QString>> parts;
    for (const QString &key : app::PosSession::stateKeys()) {
        if (probe.metaObject()->indexOfProperty(key.toLatin1().constData()) < 0)
            continue;
        t.restart();
        for (int i = 0; i < 5; ++i)
            probe.property(key.toLatin1().constData());
        parts.emplace_back(double(t.nsecsElapsed()) / 5e6, key);
    }
    std::ranges::sort(parts, std::greater<>());
    updateCost_ = u"one screen's update takes %1 ms to build (%2 open checks); %3 screens get one after each change. Slowest parts:"_s
                      .arg(whole, 0, 'f', 2).arg(shared_->open.size()).arg(server_ ? server_->terminalCount() + 1 : 1);
    for (size_t i = 0; i < std::min<size_t>(8, parts.size()); ++i)
        updateCost_ += u" %1 %2 ms;"_s.arg(parts[i].second).arg(parts[i].first, 0, 'f', 2);
    probe.releaseCheck();
    probe.logout();
}

void StressRunner::windDown()
{
    winding_ = true;
    windStartedMs_ = clock_.elapsed();
}

QVariantMap StressRunner::stats() const
{
    const QMutexLocker lock(&mutex_);
    int open = 0, guests = 0, tickets = 0;
    for (const auto &[checkId, c] : shared_->open) {
        if (c.training)
            continue;
        ++open;
        guests += c.guests;
        for (const core::OrderLine &l : c.lines)
            if (l.sent && !l.voided && !l.isComment())
                ++tickets;
    }
    qint64 sales = 0;
    for (const core::Check &c : shared_->closedToday)
        sales += c.totals(shared_->settings.tax).subtotal.cents();
    const auto timing = [](const QHash<QString, Timing> &h) {
        std::vector<int> all;
        qint64 worst = 0;
        int n = 0;
        for (const Timing &t : h) {
            all.insert(all.end(), t.us.begin(), t.us.end());
            worst = std::max(worst, t.worst);
            n += t.n;
        }
        return QVariantMap{{u"n"_s, n}, {u"p50"_s, percentile(all, 0.5) / 1000.0}, {u"p95"_s, percentile(all, 0.95) / 1000.0},
                           {u"worst"_s, double(worst) / 1e6}};
    };
    int perMinute = 0;
    for (size_t i = actionsPerSecond_.size() > 60 ? actionsPerSecond_.size() - 60 : 0; i < actionsPerSecond_.size(); ++i)
        perMinute += actionsPerSecond_[i];
    QVariantMap counts;
    for (auto it = counts_.cbegin(); it != counts_.cend(); ++it)
        counts.insert(it.key(), it.value());
    const qint64 elapsed = clock_.isValid() ? clock_.elapsed() : 0;
    return {
        {u"elapsed"_s, elapsed / 1000},
        {u"total"_s, o_.minutes * 60},
        {u"phase"_s, finished_ ? (passed_ ? u"Passed"_s : u"Problems found"_s) : winding_ ? u"Last checks"_s
                     : intensity() >= 0.99 ? u"The rush"_s : u"Service"_s},
        {u"intensity"_s, intensity()},
        {u"speed"_s, o_.speed},
        {u"people"_s, int(bots_.size())},
        {u"handhelds"_s, server_ ? server_->terminalCount() : 0},
        {u"open"_s, open},
        {u"guests"_s, guests},
        {u"closed"_s, int(shared_->closedToday.size())},
        {u"sales"_s, QString::number(double(sales) / 100.0, 'f', 2)},
        {u"waiting"_s, [this] {
             int n = 0;
             for (const core::Party &p : shared_->parties)
                 if (p.inLine()) ++n;
             return n;
         }()},
        {u"actionsPerMinute"_s, perMinute},
        {u"lagP50"_s, percentile(lagWindow_, 0.5)},
        {u"lagP99"_s, percentile(lagWindow_, 0.99)},
        {u"lagWorst"_s, worstLag_},
        {u"stalls"_s, stalls250_},
        {u"fps"_s, fps_},
        {u"local"_s, timing(local_)},
        {u"remote"_s, timing(remote_)},
        {u"db"_s, dbPending_},
        {u"dbWorst"_s, dbWorst_},
        {u"printed"_s, printers_ ? printers_->jobs() : 0},
        {u"memory"_s, int(std::lround(rssMb_))},
        {u"cpu"_s, int(std::lround(cpu_))},
        {u"refused"_s, refused_},
        {u"unexpected"_s, unexpected_},
        {u"counts"_s, counts},
        {u"finished"_s, finished_.load()},
        {u"passed"_s, passed_},
    };
}

void StressRunner::checkEverything(QStringList &problems, QStringList &notes)
{
    // Every check closed, every closed one paid in full.
    int left = 0;
    for (const auto &[checkId, c] : shared_->open)
        if (!c.training)
            ++left;
    if (left)
        problems << u"%1 check(s) still open after the last call."_s.arg(left);
    qint64 sales = 0, paid = 0;
    int unpaid = 0;
    for (const core::Check &c : shared_->closedToday) {
        const core::Totals t = c.totals(shared_->settings.tax);
        sales += t.total.cents();
        paid += t.paid.cents() - t.change.cents();
        if (t.balance.cents() > 0 && c.status == core::CheckStatus::Closed)
            ++unpaid;
    }
    if (unpaid)
        problems << u"%1 closed check(s) not paid in full."_s.arg(unpaid);
    notes << u"Closed checks: %1, totals $%2, paid $%3."_s.arg(shared_->closedToday.size())
                 .arg(double(sales) / 100.0, 0, 'f', 2).arg(double(paid) / 100.0, 0, 'f', 2);
    // Tickets the kitchen never saw done.
    int stuck = 0;
    for (const core::Check &c : shared_->closedToday)
        for (const core::OrderLine &l : c.lines)
            if (l.sent && !l.voided && !l.isComment() && !l.kitchenHide && !l.made)
                ++stuck;
    if (stuck)
        notes << u"%1 sent item(s) on closed checks the kitchen never bumped (paid before it was out)."_s.arg(stuck);

    // The database: what's saved is what the screen has.
    if (o_.writer && !o_.dbPath.isEmpty()) {
        o_.writer->flush();
        storage::PosStore store(o_.dbPath);
        QString error;
        if (!store.open(&error)) {
            problems << u"Could not open the database to check it: "_s + error;
        } else if (const auto saved = store.load()) {
            if (saved->closedToday.size() != shared_->closedToday.size())
                problems << u"The database has %1 closed checks; the screen %2."_s.arg(saved->closedToday.size())
                                .arg(shared_->closedToday.size());
            qint64 savedPaid = 0;
            for (const core::Check &c : saved->closedToday)
                savedPaid += c.totals(shared_->settings.tax).paid.cents() - c.totals(shared_->settings.tax).change.cents();
            if (savedPaid != paid)
                problems << u"Payments in the database ($%1) differ from the screen ($%2)."_s
                                .arg(double(savedPaid) / 100.0, 0, 'f', 2).arg(double(paid) / 100.0, 0, 'f', 2);
            for (const core::Check &c : saved->closedToday) {
                const auto mine = std::ranges::find_if(shared_->closedToday, [&](const core::Check &x) { return x.id == c.id; });
                if (mine == shared_->closedToday.end() || !(*mine == c)) {
                    problems << u"Check #%1 in the database differs from the screen's."_s.arg(c.id);
                    break;
                }
            }
            if (problems.isEmpty() || !problems.last().startsWith(u"Check #"_s))
                notes << u"Database: every closed check matches the screen's, payments and all."_s;
        } else {
            problems << u"Could not read the database back."_s;
        }
    }

    // The screen kept up.
    const int p99 = percentile(lagAll_, 0.99);
    if (worstLag_ >= 1000)
        problems << u"The screen froze for %1 ms at worst (%2 time(s) over a second)."_s.arg(worstLag_).arg(stalls1000_);
    if (p99 >= 150)
        problems << u"The screen was slow: 1 in 100 moments waited %1 ms or more."_s.arg(p99);
    if (unexpected_ > 0)
        problems << u"%1 step(s) refused that shouldn't have been (listed below)."_s.arg(unexpected_);
}

void StressRunner::finish()
{
    finished_ = true;
    sampler_.stop();
    for (auto &b : bots_) {
        Bot *raw = b.get();
        QMetaObject::invokeMethod(&raw->ctx, [raw] { raw->next.stop(); });
    }
    const QMutexLocker lock(&mutex_);
    QStringList problems, notes;
    checkEverything(problems, notes);
    passed_ = problems.isEmpty();

    const QVariantMap st = stats();
    const QVariantMap local = st[u"local"_s].toMap(), remote = st[u"remote"_s].toMap();
    QString r;
    r += u"ViewTouch stress test - %1\n"_s.arg(QDateTime::currentDateTime().toString(u"yyyy-MM-dd h:mm AP"_s));
    r += u"%1 minutes at %2x (a %3-minute dinner), %4 pretend staff, %5 handhelds.\n\n"_s.arg(o_.minutes)
             .arg(o_.speed).arg(int(o_.minutes * o_.speed)).arg(bots_.size()).arg(st[u"handhelds"_s].toInt());
    r += passed_ ? u"RESULT: PASSED\n"_s : u"RESULT: PROBLEMS FOUND\n"_s;
    for (const QString &p : problems)
        r += u"  ! "_s + p + u'\n';
    r += u"\nService\n"_s;
    QStringList keys = st[u"counts"_s].toMap().keys();
    keys.sort();
    for (const QString &k : keys)
        r += u"  %1: %2\n"_s.arg(k).arg(st[u"counts"_s].toMap().value(k).toInt());
    r += u"  tickets printed: %1\n"_s.arg(st[u"printed"_s].toInt());
    for (const QString &n : notes)
        r += u"  "_s + n + u'\n';
    r += u"\nScreen (the event loop, every 20 ms)\n"_s;
    r += u"  delay: typical %1 ms, 1 in 100: %2 ms, worst %3 ms\n"_s.arg(percentile(lagAll_, 0.5))
             .arg(percentile(lagAll_, 0.99)).arg(worstLag_);
    r += u"  hiccups: %1 over 100 ms, %2 over 250 ms, %3 over a second\n"_s.arg(stalls100_).arg(stalls250_).arg(stalls1000_);
    r += u"\nCommands\n"_s;
    r += u"  on this computer: %1, typical %2 ms, 1 in 20: %3 ms, worst %4 ms\n"_s.arg(local[u"n"_s].toInt())
             .arg(local[u"p50"_s].toDouble(), 0, 'f', 2).arg(local[u"p95"_s].toDouble(), 0, 'f', 2)
             .arg(local[u"worst"_s].toDouble(), 0, 'f', 1);
    r += u"  from handhelds (round trip): %1, typical %2 ms, 1 in 20: %3 ms, worst %4 ms\n"_s.arg(remote[u"n"_s].toInt())
             .arg(remote[u"p50"_s].toDouble(), 0, 'f', 2).arg(remote[u"p95"_s].toDouble(), 0, 'f', 2)
             .arg(remote[u"worst"_s].toDouble(), 0, 'f', 1);
    // The slowest kinds.
    std::vector<std::pair<double, QString>> slow;
    for (const auto *h : {&local_, &remote_})
        for (auto it = h->cbegin(); it != h->cend(); ++it)
            slow.emplace_back(double(it->worst) / 1e6, it.key() + (h == &remote_ ? u" (handheld)"_s : QString()));
    std::ranges::sort(slow, std::greater<>());
    r += u"  slowest:"_s;
    for (size_t i = 0; i < std::min<size_t>(6, slow.size()); ++i)
        r += u" %1 %2 ms;"_s.arg(slow[i].second).arg(slow[i].first, 0, 'f', 1);
    r += u"\n\nComputer\n"_s;
    r += u"  memory: %1 MB now, %2 MB at most; CPU %3% of one core at the end\n"_s.arg(int(rssMb_))
             .arg(int(rssWorstMb_)).arg(int(cpu_));
    r += u"  database: up to %1 change(s) waiting to be written\n"_s.arg(dbWorst_);
    if (server_ && server_->updateRounds() > 0)
        r += u"  updates to the handhelds: %1 rounds, %2 ms each on average, %3 s in all\n"_s.arg(server_->updateRounds())
                 .arg(double(server_->updateNsecs()) / 1e6 / server_->updateRounds(), 0, 'f', 2)
                 .arg(double(server_->updateNsecs()) / 1e9, 0, 'f', 1);
    if (!updateCost_.isEmpty())
        r += u"  in the rush, "_s + updateCost_ + u'\n';
    if (!timeline_.isEmpty())
        r += u"\nAs the night went on\n"_s + timeline_.join(u'\n') + u'\n';
    if (!refusedWhy_.isEmpty()) {
        r += u"\nRefused steps (%1; (!) = shouldn't have been)\n"_s.arg(refused_);
        std::vector<std::pair<int, QString>> why;
        for (auto it = refusedWhy_.cbegin(); it != refusedWhy_.cend(); ++it)
            why.emplace_back(it.value(), it.key());
        std::ranges::sort(why, std::greater<>());
        for (size_t i = 0; i < std::min<size_t>(25, why.size()); ++i)
            r += u"  %1 x %2\n"_s.arg(why[i].first).arg(why[i].second);
    }
    report_ = r;
    emit statsChanged();
    emit done(passed_);
}

} // namespace vt::net
