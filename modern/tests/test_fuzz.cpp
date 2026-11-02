#include <catch2/catch_test_macros.hpp>

#include "app/pos_json.hh"
#include "layout_fixture.hh"
#include "storage/backup.hh"
#include "storage/layout_store.hh"
#include "storage/pos_store.hh"
#include "storage/sealed.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <limits>
#include <random>

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;

// Fuzzing: what a screen, a tablet or a file can send that nobody meant to.
// Fixed seeds (the same rounds every run, so a failure comes back); more
// rounds with VTM_FUZZ_ROUNDS=<n>. What must hold whatever comes: nothing
// crashes, the books balance, and nobody does what they aren't allowed to.

namespace {

int rounds(int normal)
{
    const int asked = qEnvironmentVariableIntValue("VTM_FUZZ_ROUNDS");
    return asked > 0 ? asked : normal;
}

struct Fuzz {
    std::mt19937 random;
    explicit Fuzz(unsigned seed) : random(seed) {}
    int pick(int n) { return int(random() % unsigned(std::max(1, n))); }
    bool chance(int percent) { return pick(100) < percent; }

    // Something that might be an argument: real ids now and then, else odd values.
    QVariant value(PosService &pos, int depth = 0)
    {
        const QVariantList lines = pos.lines();
        switch (pick(depth > 1 ? 14 : 17)) {
        case 0: return QString();
        case 1: return u"x"_s;
        case 2: return QString(5000, u'A');
        case 3: return QStringList{u"1234"_s, u"1111"_s, u"0"_s, u"-1"_s, u"1e308"_s, u"NaN"_s, u"T1"_s, u"RESET"_s,
                                   u"cash"_s, u"credit"_s, u"discount"_s, u"comp"_s, u"18,5"_s, u"<b>"_s,
                                   u"' OR 1=1 --"_s, u"‮evil"_s}[pick(16)];
        case 4: return qint64(std::numeric_limits<qint64>::max());
        case 5: return qint64(std::numeric_limits<qint64>::min());
        case 6: return pick(2) ? -1 : 0;
        case 7: return pick(100000);
        case 8: return std::numeric_limits<double>::quiet_NaN();
        case 9: return pick(2) ? std::numeric_limits<double>::infinity() : -1e300;
        case 10: return chance(50);
        case 11: return QVariant();
        case 12:   // a real menu item
            return QString::fromStdString(pos.shared()->menu[std::size_t(pick(int(pos.shared()->menu.size())))].id);
        case 13:   // the check, a line, a payment
            if (pick(3) == 0 && !lines.isEmpty())
                return lines[pick(int(lines.size()))].toMap()[u"id"_s];
            if (pick(2) == 0 && !pos.payments().isEmpty())
                return pos.payments().first().toMap()[u"id"_s];
            return pos.checkInfo()[u"id"_s];
        case 14: {   // a list
            QVariantList l;
            for (int i = pick(4); i > 0; --i)
                l << value(pos, depth + 1);
            return l;
        }
        case 15: {   // a record, with keys forms use
            static const QStringList keys{u"name"_s, u"price"_s, u"id"_s, u"qty"_s, u"amount"_s, u"phone"_s, u"item"_s,
                                          u"reason"_s, u"tax"_s, u"role"_s, u"pin"_s, u"ingredient"_s, u"lines"_s,
                                          u"onHand"_s, u"taxClass"_s, u"category"_s, u"storeName"_s, u"size"_s};
            QVariantMap m;
            for (int i = 1 + pick(5); i > 0; --i)
                m.insert(keys[pick(int(keys.size()))], value(pos, depth + 1));
            return m;
        }
        default:   // an employee
            return QString::fromStdString(pos.shared()->employees[std::size_t(pick(int(pos.shared()->employees.size())))].id);
        }
    }
    QVariantList args(PosService &pos)
    {
        QVariantList a;
        for (int i = pick(5); i > 0; --i)
            a << value(pos);
        return a;
    }
};

// Every closed check: paid in full, its parts adding up.
void booksBalance(PosService &pos)
{
    for (const core::Check &c : pos.shared()->closedToday) {
        if (c.training || c.status != core::CheckStatus::Closed)
            continue;
        const core::Totals t = c.totals(pos.shared()->settings.tax);
        INFO("check " << c.id);
        CHECK(t.balance.cents() <= 0);
        CHECK(t.total == t.subtotal + t.tax + t.gratuity);
    }
}

QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

// What only a manager may change: the settings, the staff, the menu, by
// name ("settings.taxes", "employee sam", "item cobb") for saying what did.
std::map<std::string, QByteArray> guarded(PosService &pos)
{
    std::map<std::string, QByteArray> out;
    const QJsonObject settings = app::toJson(pos.shared()->settings);
    for (auto it = settings.begin(); it != settings.end(); ++it)
        out["settings." + it.key().toStdString()] = QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact);
    for (const core::Employee &e : pos.shared()->employees)
        out["employee " + e.id] = json(app::toJson(e));
    for (const core::MenuItem &m : pos.shared()->menu) {
        QJsonObject item = app::toJson(m);
        for (const char *staffs : {"available", "soldOutToday", "autoSoldOut"})   // 86'ing an item: staff's job
            item.remove(QLatin1StringView(staffs));
        out["item " + m.id] = json(item);
    }
    return out;
}
std::string changed(const std::map<std::string, QByteArray> &a, const std::map<std::string, QByteArray> &b)
{
    std::string out;
    for (const auto &[k, v] : a)
        if (!b.contains(k) || b.at(k) != v)
            out += k + (b.contains(k) ? " (" + v.left(120).toStdString() + " -> " + b.at(k).left(120).toStdString() + ") " : " (gone) ");
    for (const auto &[k, v] : b)
        if (!a.contains(k))
            out += k + " (new) ";
    return out;
}

} // namespace

TEST_CASE("Fuzz: every command with odd arguments, as nobody, a server and a manager", "[fuzz][commands]")
{
    const QStringList names = PosService::commandNames();
    REQUIRE(names.size() > 200);
    for (unsigned seed = 1; seed <= unsigned(rounds(6)); ++seed) {
        INFO("seed " << seed);
        PosService pos(test::seedPosData(), nullptr);
        Fuzz f(seed);
        for (int call = 0; call < 300; ++call) {
            // Who's there: nobody, a server or a manager, now and then changing.
            if (f.chance(4)) {
                pos.logout();
                if (f.chance(70))
                    pos.loginWithPin(f.chance(50) ? u"1111"_s : u"1234"_s);
            }
            const QString method = names[f.pick(int(names.size()))];
            const QVariantList a = f.args(pos);
            INFO("call " << call << ": " << method.toStdString() << " "
                         << QJsonDocument::fromVariant(a).toJson(QJsonDocument::Compact).toStdString());
            pos.invoke(method, a);
            booksBalance(pos);
        }
    }
}

TEST_CASE("Fuzz: a server's odd commands never change the settings, the staff or the menu", "[fuzz][commands][security]")
{
    // Logging in (or a manager's approval) aside: those are how a server
    // gets a manager's say, and they're tested on their own.
    const QStringList skip{u"login"_s, u"loginWithPin"_s, u"pinKey"_s, u"logout"_s, u"approve"_s, u"switchCounterPerson"_s,
                           u"clockInAs"_s, u"chooseOptionAs"_s};
    QStringList names = PosService::commandNames();
    names.removeIf([&](const QString &n) { return skip.contains(n); });
    for (unsigned seed = 1; seed <= unsigned(rounds(6)); ++seed) {
        INFO("seed " << seed);
        PosService pos(test::seedPosData(), nullptr);
        REQUIRE(pos.loginWithPin(u"1111"_s));
        REQUIRE_FALSE(pos.can(u"manager"_s));
        Fuzz f(seed * 7919);
        auto before = guarded(pos);
        before.erase("settings.tableStates");
        before.erase("settings.notices");
        for (int call = 0; call < 300; ++call) {
            const QString method = names[f.pick(int(names.size()))];
            const QVariantList a = f.args(pos);
            pos.invoke(method, a);
            INFO("call " << call << ": " << method.toStdString() << " "
                         << QJsonDocument::fromVariant(a).toJson(QJsonDocument::Compact).left(300).toStdString());
            REQUIRE(pos.loggedIn());
            REQUIRE_FALSE(pos.can(u"manager"_s));
            auto now = guarded(pos);
            // Table states (clean, dirty, seated) are the floor's, kept with the
            // settings: staff change them all service long.
            now.erase("settings.tableStates");
            // Posted messages: anyone's (the kitchen's screen too), a few, a month at most.
            REQUIRE(pos.shared()->settings.notices.size() <= 20);
            for (const auto &n : pos.shared()->settings.notices)
                REQUIRE(n.until <= pos.shared()->now() + 31LL * 24 * 3'600'000);
            now.erase("settings.notices");
            // A kitchen screen sets its own station (nobody logs in there): that, and only that.
            if (method == u"setKitchenStation" && now != before) {
                const QString station = pos.kitchenStation();
                REQUIRE((station.isEmpty() || std::ranges::any_of(pos.kitchenStations(), [&](const QVariant &v) {
                    return v.toMap()[u"id"_s].toString() == station;
                })));
                before = now;   // its own station, a fair change
            }
            INFO("changed: " << changed(before, now));
            REQUIRE(now == before);
        }
    }
}

TEST_CASE("What the fuzzing found: a station that isn't one, a table that isn't one, messages without end", "[fuzz][found]")
{
    PosService pos(test::seedPosData(), nullptr);
    // A kitchen screen picks one of the store's stations, or none; not any text.
    REQUIRE_FALSE(pos.kitchenStations().isEmpty());
    const QString real = pos.kitchenStations().first().toMap()[u"id"_s].toString();
    CHECK_FALSE(pos.setKitchenStation(QString(5000, u'A')));
    CHECK(pos.kitchenStation().isEmpty());
    CHECK(pos.setKitchenStation(real));
    CHECK(pos.kitchenStation() == real);
    CHECK(pos.setKitchenStation({}));

    // A table: a name as the floor has them, not empty or a page of text.
    REQUIRE(pos.loginWithPin(u"1111"_s));
    CHECK(pos.selectTable(u""_s) == app::PosService::TableFailed);
    CHECK(pos.selectTable(u"   "_s) == app::PosService::TableFailed);
    CHECK(pos.selectTable(QString(5000, u'A')) == app::PosService::TableFailed);
    CHECK(pos.selectTable(u"T1"_s) == app::PosService::TableNeedsGuests);

    // Posted messages: a month ahead at most, twenty at a time.
    const qint64 now = pos.shared()->now();
    CHECK_FALSE(pos.sendMessage(u"all"_s, u"Forever"_s, std::numeric_limits<qint64>::max()));
    for (int i = 0; i < 20; ++i)
        REQUIRE(pos.sendMessage(u"all"_s, u"Note %1"_s.arg(i), now + 3'600'000));
    CHECK_FALSE(pos.sendMessage(u"all"_s, u"One more"_s, now + 3'600'000));
    CHECK(pos.shared()->settings.notices.size() == 20);
    CHECK(pos.sendMessage(u"all"_s, u"Said, not posted"_s, 0));   // a message now: no limit to those
}

TEST_CASE("Fuzz: a damaged backup is refused, and the store's data is left as it was", "[fuzz][backup]")
{
    QTemporaryDir dir;
    const QString db = dir.filePath(u"viewtouch.db"_s);
    {
        storage::PosStore store(db);
        REQUIRE(store.open());
        const auto seed = test::seedPosData();
        REQUIRE(store.seed(seed.settings, seed.menu, seed.employees));
        storage::LayoutStore pages(db);
        REQUIRE(pages.open());
        REQUIRE(pages.save(*test::loadTestLayout()));
    }
    // A good backup: the store itself.
    const QString good = dir.filePath(u"good.db"_s);
    REQUIRE(QFile::copy(db, good));
    QFile f(good);
    REQUIRE(f.open(QIODevice::ReadOnly));
    const QByteArray goodBytes = f.readAll();
    f.close();
    REQUIRE(storage::verifyDatabase(good));
    const auto storeBytes = [&] {
        QFile s(db);
        return s.open(QIODevice::ReadOnly) ? s.readAll() : QByteArray();
    };
    std::mt19937 rng{7};
    const auto pick = [&](int n) { return int(rng() % unsigned(std::max(1, n))); };
    int refused = 0;
    for (int round = 0; round < rounds(40); ++round) {
        INFO("round " << round);
        QByteArray bent = goodBytes;
        if (pick(2)) {
            bent.truncate(pick(int(bent.size())));
        } else {
            for (int i = 1 + pick(30); i > 0; --i)
                bent[pick(int(bent.size()))] = char(pick(256));
        }
        const QString file = dir.filePath(u"bent-%1.db"_s.arg(round));
        QFile out(file);
        REQUIRE(out.open(QIODevice::WriteOnly));
        out.write(bent);
        out.close();
        const QByteArray before = storeBytes();
        QString keptAs, error;
        if (!storage::restoreDatabase(file, db, &keptAs, &error)) {
            ++refused;
            CHECK(storeBytes() == before);   // untouched
            CHECK_FALSE(error.isEmpty());   // and it says why
        } else {
            // Taken (the damage missed what matters): it opens and reads.
            {
                storage::PosStore store(db);
                REQUIRE(store.open());
                CHECK(store.load());
            }
            const bool back = storage::restoreDatabase(good, db, &keptAs, &error);   // the good one back
            INFO("restoring the good one: " << error.toStdString());
            REQUIRE(back);
        }
        QFile::remove(file);
    }
    CHECK(refused > 0);

    // Restored twice in the same second (the wrong backup, then the right
    // one): both taken, each store before kept under its own name.
    QString first, second, error;
    REQUIRE(storage::restoreDatabase(good, db, &first, &error));
    REQUIRE(storage::restoreDatabase(good, db, &second, &error));
    CHECK(first != second);
    CHECK(QFile::exists(first));
    CHECK(QFile::exists(second));

    // Encrypted: any change at all is caught (the header too), nothing written.
    if (storage::sealingAvailable()) {
        const QByteArray salt = storage::newSalt();
        const QByteArray key = storage::sealingKey(u"pass-word"_s, salt);
        const QString sealed = dir.filePath(u"good.vtbak"_s);
        REQUIRE(storage::sealFile(good, sealed, key, salt));
        QFile s(sealed);
        REQUIRE(s.open(QIODevice::ReadOnly));
        const QByteArray sealedBytes = s.readAll();
        s.close();
        for (int round = 0; round < 12; ++round) {
            QByteArray bent = sealedBytes;
            bent[pick(int(bent.size()))] ^= char(1 + pick(255));
            const QString file = dir.filePath(u"bent-%1.vtbak"_s.arg(round));
            QFile out(file);
            REQUIRE(out.open(QIODevice::WriteOnly));
            out.write(bent);
            out.close();
            const QString plain = dir.filePath(u"plain-%1.db"_s.arg(round));
            INFO("sealed round " << round);
            CHECK_FALSE(storage::openSealedFileWithKey(file, plain, key));
            CHECK_FALSE(QFile::exists(plain));
        }
        CHECK_FALSE(storage::openSealedFile(sealed, dir.filePath(u"wrong.db"_s), u"not-it"_s));
    }
}

TEST_CASE("Fuzz: saved records with odd fields are read without a crash, and a check still totals", "[fuzz][json]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    pos.entryKey(u"10000"_s);
    REQUIRE(pos.openDrawerSession());
    REQUIRE(pos.startCheck(core::CheckType::Takeout));
    REQUIRE(pos.addItem(u"cobb"_s));
    REQUIRE(pos.addItem(u"coffee"_s));
    pos.setGratuity(1800);
    REQUIRE(pos.tender(u"credit"_s));
    pos.addTip(300);
    const qint64 id = pos.checkInfo()[u"id"_s].toLongLong();
    const QJsonObject check = app::toJson(pos.shared()->open.at(id));
    const QJsonObject settings = app::toJson(pos.shared()->settings);
    const QJsonObject item = app::toJson(pos.shared()->menu.front());
    const QJsonObject employee = app::toJson(pos.shared()->employees.front());
    const QJsonObject drawer = app::toJson(pos.shared()->drawers.front());

    std::mt19937 rng{11};
    const auto pick = [&](int n) { return int(rng() % unsigned(std::max(1, n))); };
    const auto odd = [&]() -> QJsonValue {
        switch (pick(9)) {
        case 0: return QJsonValue();
        case 1: return pick(2) == 1;
        case 2: return 9.2e18;
        case 3: return -9.2e18;
        case 4: return std::numeric_limits<double>::max();
        case 5: return u"x"_s;
        case 6: return QJsonArray{1, u"two"_s, QJsonObject{}};
        case 7: return QJsonObject{{u"a"_s, 1}};
        default: return -1;
        }
    };
    // Bent: a few fields (or fields of its lines and payments) something else.
    const auto bend = [&](QJsonObject o) {
        for (int i = 1 + pick(4); i > 0; --i) {
            const QStringList keys = o.keys();
            const QString k = keys[pick(int(keys.size()))];
            if (o.value(k).isArray() && pick(2) && !o.value(k).toArray().isEmpty()) {
                QJsonArray a = o.value(k).toArray();
                const int at = pick(int(a.size()));
                if (a[at].isObject()) {
                    QJsonObject inner = a[at].toObject();
                    const QStringList ik = inner.keys();
                    if (!ik.isEmpty())
                        inner.insert(ik[pick(int(ik.size()))], odd());
                    a[at] = inner;
                } else {
                    a[at] = odd();
                }
                o.insert(k, a);
            } else {
                o.insert(k, odd());
            }
        }
        return o;
    };
    for (int round = 0; round < rounds(60) * 10; ++round) {
        INFO("round " << round);
        if (const auto c = app::checkFromJson(bend(check))) {
            const core::Totals t = c->totals(pos.shared()->settings.tax);
            CHECK(t.total == t.subtotal + t.tax + t.gratuity);
        }
        const core::PosSettings s = app::settingsFromJson(bend(settings));
        CHECK(s.tax.foodPpm >= 0);
        app::menuItemFromJson(bend(item));
        app::employeeFromJson(bend(employee));
        app::drawerFromJson(bend(drawer));
    }
}

TEST_CASE("Fuzz: a check one screen is paying can't be changed from another", "[fuzz][locks]")
{
    // Kitchen and expo work on it is fair (made, served); nothing else.
    const auto essence = [](const core::Check &c) {
        QJsonObject o = app::toJson(c);
        for (const char *k : {"events"})
            o.remove(QLatin1StringView(k));
        QJsonArray lines;
        for (QJsonValue l : o.value(u"lines"_s).toArray()) {
            QJsonObject line = l.toObject();
            for (const char *k : {"made", "madeAt", "servedAt", "queueAtSend"})
                line.remove(QLatin1StringView(k));
            QJsonArray mods;
            for (QJsonValue m : line.value(u"modifiers"_s).toArray()) {
                QJsonObject mo = m.toObject();
                mo.remove(u"made"_s);
                mo.remove(u"madeAt"_s);
                mods.append(mo);
            }
            line.insert(u"modifiers"_s, mods);
            lines.append(line);
        }
        o.insert(u"lines"_s, lines);
        for (const char *k : {"readyAt", "pickedUpAt", "outAt", "deliveredAt"})
            o.remove(QLatin1StringView(k));
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    };
    const QStringList names = PosService::commandNames();
    for (unsigned seed = 1; seed <= unsigned(rounds(6)); ++seed) {
        INFO("seed " << seed);
        app::PosShared shared(test::seedPosData(), nullptr);
        PosService front(&shared, u"Front"_s), bar(&shared, u"Bar"_s);
        REQUIRE(front.loginWithPin(u"1111"_s));
        REQUIRE(bar.loginWithPin(u"1234"_s));   // a manager, at that
        bar.entryKey(u"10000"_s);
        bar.openDrawerSession();
        // Front: a table's check, sent, half paid, open on its screen.
        REQUIRE(front.selectTable(u"T4"_s) == PosService::TableNeedsGuests);
        REQUIRE(front.startCheck(core::CheckType::DineIn));
        REQUIRE(front.addItem(u"cobb"_s));
        REQUIRE(front.addItem(u"coffee"_s));
        REQUIRE(front.sendOrder());
        front.entryKey(u"500"_s);
        front.tender(u"cash"_s);
        const qint64 held = front.checkInfo()[u"id"_s].toLongLong();
        REQUIRE(shared.open.contains(held));
        const QByteArray before = essence(shared.open.at(held));
        // Bar: another table's check, to merge or move into.
        REQUIRE(bar.selectTable(u"T5"_s) == PosService::TableNeedsGuests);
        REQUIRE(bar.startCheck(core::CheckType::DineIn));
        REQUIRE(bar.addItem(u"soda"_s));
        bar.releaseCheck();
        Fuzz f(seed * 31);
        for (int call = 0; call < 400; ++call) {
            const QString method = names[f.pick(int(names.size()))];
            if (method == u"logout" || method == u"login" || method == u"loginWithPin" || method == u"pinKey")
                continue;
            QVariantList a = f.args(bar);
            // Mostly aimed at the held check, by its number or its table.
            if (f.chance(70)) {
                if (a.isEmpty())
                    a << QVariant();
                a[f.pick(int(a.size()))] = f.chance(70) ? QVariant(held) : QVariant(u"T4"_s);
            }
            bar.invoke(method, a);
            INFO("call " << call << ": " << method.toStdString() << " "
                         << QJsonDocument::fromVariant(a).toJson(QJsonDocument::Compact).left(200).toStdString());
            REQUIRE(shared.open.contains(held));
            REQUIRE(essence(shared.open.at(held)) == before);
        }
    }
}
