#include "app/legacy/legacy_files.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <QSet>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace vt::legacy {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("vt::legacy", s); }

// Each legacy zone type's own fields, in PosZone::Read's order
// (tools/legacy/zone_fields.json, made from the legacy source).
struct ZoneFields {
    QList<std::pair<QString, QString>> order;   // field, its kind (int, flt, str, always-str)
    QHash<int, std::pair<QString, QSet<QString>>> types;   // type -> its ZONE_ name, its fields
};

const ZoneFields &zoneFields()
{
    static const ZoneFields table = [] {
        ZoneFields t;
        QFile f(QStringLiteral(":/legacy/zone_fields.json"));
        if (!f.open(QIODevice::ReadOnly))
            return t;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &v : o.value(u"fields").toArray())
            t.order.append({v.toArray().at(0).toString(), v.toArray().at(1).toString()});
        const QJsonObject types = o.value(u"types").toObject();
        for (auto it = types.begin(); it != types.end(); ++it) {
            const QJsonObject ty = it.value().toObject();
            QSet<QString> fields;
            for (const QJsonValue &v : ty.value(u"fields").toArray())
                fields.insert(v.toString());
            t.types.insert(it.key().toInt(), {ty.value(u"zone").toString(), fields});
        }
        return t;
    }();
    return table;
}

std::optional<Tokens> openFile(const QString &path, QString *error, int minVersion, int maxVersion)
{
    std::optional<Tokens> t = Tokens::open(path, error);
    if (t && (t->version() < minVersion || t->version() > maxVersion)) {
        if (error)
            *error = tr("%1 is from a ViewTouch this can't read (version %2; it reads %3 to %4).")
                         .arg(QFileInfo(path).fileName())
                         .arg(t->version())
                         .arg(minVersion)
                         .arg(maxVersion);
        return std::nullopt;
    }
    return t;
}

template<typename T>
std::optional<T> done(Tokens &t, T value, QString *error)
{
    if (t.finished())
        return value;
    if (error)
        *error = t.error();
    return std::nullopt;
}

// --- pages ---

Zone readZone(Tokens &f, int version)
{
    Zone z;
    z.type = f.i();
    z.name = f.s();
    z.group = f.i();
    z.x = f.i();
    z.y = f.i();
    z.w = f.i();
    z.h = f.i();
    z.behave = f.i();
    z.font = f.i();
    for (int s = 0; s < 3; ++s) {
        z.frame[s] = f.i();
        z.texture[s] = f.i();
        z.color[s] = f.i();
        z.image[s] = f.i();
    }
    z.shadow = f.i();
    z.shape = f.i();
    const ZoneFields &table = zoneFields();
    const auto type = table.types.constFind(z.type);
    if (type == table.types.cend()) {
        f.fail(tr("a button of a kind ViewTouch doesn't know (%1, \"%2\")").arg(z.type).arg(z.name));
        return z;
    }
    z.kind = type->first;
    for (const auto &[field, kind] : table.order) {
        if ((field == u"ImagePath" && version < 29) || !type->second.contains(field))
            continue;
        if (kind == u"int")
            z.fields.insert(field, f.i());
        else if (kind == u"flt")
            z.fields.insert(field, f.f());
        else
            z.fields.insert(field, f.s());
    }
    z.key = f.i();
    return z;
}

Page readPage(Tokens &f, int version)
{
    Page p;
    p.name = f.s();
    p.id = f.i();
    p.image = f.i();
    p.titleColor = f.i();
    p.parent = f.i();
    p.type = f.i();
    p.index = f.i();
    p.size = f.i();
    p.font = f.i();
    for (Look &l : p.defaults) {
        l.frame = f.i();
        l.texture = f.i();
        l.color = f.i();
    }
    p.spacing = f.i();
    p.shadow = f.i();
    const int n = f.i();
    for (int k = 0; k < n && f.ok(); ++k)
        p.zones.append(readZone(f, version));
    return p;
}

void readPages(Tokens &f, PageFile &out)
{
    const int n = f.i();
    for (int k = 0; k < n && f.ok(); ++k)
        out.pages.append(readPage(f, out.version));
}

// --- settings ---

Discount readDiscount(Tokens &f, int v)   // DiscountInfo::Read
{
    Discount d;
    d.id = f.i();
    d.name = f.s();
    d.flags = f.i();
    d.amount = f.i();
    if (v >= 40)
        d.local = f.i();
    if (v >= 43)
        d.active = f.i();
    return d;
}

Discount readComp(Tokens &f, int v)   // CompInfo::Read
{
    Discount d;
    d.id = f.i();
    d.name = f.s();
    d.flags = f.i();
    if (v >= 40)
        d.local = f.i();
    if (v >= 43)
        d.active = f.i();
    return d;
}

Discount readCoupon(Tokens &f, int v)   // CouponInfo::Read
{
    Discount d;
    d.id = f.i();
    d.name = f.s();
    d.flags = f.i();
    d.amount = f.i();
    if (v >= 40)
        d.local = f.i();
    if (v >= 43)
        d.active = f.i();
    if (v >= 75) {
        if (v < 80) {
            if (f.i())
                d.flags |= 2048;   // TF_ITEM_SPECIFIC
            if (f.i())
                d.flags |= 1024;   // TF_APPLY_EACH
        }
        d.family = f.i();
        d.itemId = f.i();
    }
    if (v >= 76) {
        if (v < 78)
            f.i();
        d.startTime = f.t();
        d.endTime = f.t();
        d.days = f.i();
        d.months = f.i();
    }
    if (v >= 77)
        d.automatic = f.i();
    if (v >= 78) {
        d.startDate = f.t();
        d.endDate = f.t();
    }
    if (v >= 81)
        d.itemName = f.s();
    return d;
}

Discount readCreditCard(Tokens &f, int v)   // CreditCardInfo::Read
{
    Discount d;
    d.id = f.i();
    d.name = f.s();
    if (v >= 40)
        d.local = f.i();
    if (v >= 43)
        d.active = f.i();
    return d;
}

Discount readMeal(Tokens &f, int v)   // MealInfo::Read
{
    Discount d;
    d.id = f.i();
    d.name = f.s();
    d.flags = f.i();
    d.amount = f.i();
    if (v >= 43)
        d.active = f.i();
    return d;
}

template<typename Fn>
void readList(Tokens &f, QList<Discount> &out, int v, Fn read, bool lastId)
{
    if (lastId)
        f.i();   // the last id given
    const int n = f.i();
    if (n < 0 || n > 10000) {
        f.fail(tr("a list of %1").arg(n));
        return;
    }
    for (int k = 0; k < n && f.ok(); ++k) {
        Discount d = read(f, v);
        if (!d.name.isEmpty())
            out.append(d);
    }
}

void readMedia(Tokens &f, Media &m, int v, bool lastIds)
{
    readList(f, m.discounts, v, readDiscount, lastIds);
    readList(f, m.coupons, v, readCoupon, lastIds);
    readList(f, m.creditCards, v, readCreditCard, lastIds);
    readList(f, m.comps, v, readComp, lastIds);
    readList(f, m.meals, v, readMeal, lastIds);
}

Terminal readTerminal(Tokens &f, int v)   // TermInfo::Read
{
    Terminal t;
    t.name = f.s();
    if (v >= 32)
        t.hardware = f.i();
    t.type = f.i();
    t.displayHost = f.s();
    t.printerHost = f.s();
    t.printerModel = f.i();
    t.printerPort = f.i();
    t.drawers = f.i();
    f.i();   // stripe_reader
    t.kitchen = f.i();
    if (v >= 30)
        f.i();   // sound_volume
    if (v >= 33)
        f.i();   // sortorder
    if (v >= 41)
        t.isServer = f.i();
    if (v >= 46) {
        f.i();   // cdu_type
        f.s();   // cdu_path
    }
    if (v >= 55)
        f.i();   // dpulse
    if (v >= 57) {
        f.s();   // cc_credit_termid
        f.s();   // cc_debit_termid
    }
    if (v >= 92)
        f.i();   // print_workorder
    if (v >= 93)
        f.i();   // workorder_heading
    if (v >= 94)
        f.skip(4);   // tax_inclusive
    return t;
}

Printer readPrinter(Tokens &f, int v)   // PrinterInfo::Read
{
    Printer p;
    if (v >= 28)
        p.name = f.s();
    p.host = f.s();
    p.port = f.i();
    p.model = f.i();
    p.type = f.i();
    if (v >= 50)
        f.i();   // kitchen_mode
    if (v >= 93)
        f.i();   // order_margin
    return p;
}

double percent(int v) { return v / 10000.0; }   // legacy PercentToFlt: 850 is 8.5%

// A small INI file (legacy src/core/conf_file.cc): [Section] then key = value.
QHash<QString, QString> readIni(const QString &path)
{
    QHash<QString, QString> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QString section;
    for (const QString &raw : QString::fromUtf8(f.readAll()).split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';'))
            continue;
        if (line.startsWith(u'[') && line.endsWith(u']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        const qsizetype eq = line.indexOf(u'=');
        if (eq > 0)
            out.insert(section + u'/' + line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return out;
}

// --- a day's records ---

void readCredit(Tokens &f, Payment *p)   // Credit::Read (a card payment's record)
{
    f.skip(14);          // number, expire, name, country, approval, forced, codes, auth, AVS, CV
    f.i64();             // batch
    f.i64();             // item
    f.i64();             // ttid
    f.skip(4);           // trans_success, last_action, state, auth_state
    const int cardType = f.i();
    f.skip(2);           // credit_type, debit_acct
    f.skip(8);           // term ids, reference, sequence, server date and time, receipt and display lines
    f.skip(5);           // the users, check_id
    f.i();               // amount
    const int tip = f.i();
    f.skip(4);           // preauth, auth, refund, void amounts
    f.i();               // processor
    for (int k = 0; k < 6; ++k)
        f.t();           // preauth, auth, void, refund, refund cancel, settle times
    const int errors = f.i();
    if (errors < 0 || errors > 10000) {
        f.fail(tr("a card record with %1 errors").arg(errors));
        return;
    }
    for (int k = 0; k < errors && f.ok(); ++k)
        readCredit(f, nullptr);
    if (p) {
        p->cardType = cardType;
        p->cardTip = tip;
    }
}

Order readOrder(Tokens &f, int v)   // Order::Read
{
    Order o;
    o.name = f.s();
    o.type = f.i();
    o.cost = f.i();
    o.family = f.i();
    if (o.family == 999)
        o.family = 255;
    o.salesType = f.i();
    o.count = f.i();
    o.qualifier = f.i();
    o.status = f.i();
    o.userId = f.i();
    o.seat = f.i();
    if (v >= 19)
        o.employeeMeal = f.i();
    if (v >= 20)
        o.isReduced = f.i();
    if (v >= 21)
        o.reducedCost = f.i();
    if (v >= 22)
        o.autoCouponId = f.i();
    return o;
}

Payment readPayment(Tokens &f, int v)   // Payment::Read
{
    Payment p;
    p.tenderType = f.i();
    p.tenderId = f.i();
    p.amount = f.i();
    p.flags = f.i();
    if (v >= 8)
        p.drawerId = f.i();
    p.userId = f.i();
    if (p.tenderType == 11 || p.tenderType == 12)
        readCredit(f, &p);
    return p;
}

bool sane(Tokens &f, int n, const char *what)
{
    if (n >= 0 && n < 10000)
        return true;
    f.fail(tr("%1 %2").arg(n).arg(tr(what)));
    return false;
}

SubCheck readSubCheck(Tokens &f, int v)   // SubCheck::Read
{
    SubCheck s;
    s.status = f.i();
    s.settleUser = f.i();
    s.settleTime = f.t();
    s.drawerId = f.i();
    int n = f.i();
    if (!sane(f, n, "items on a check"))
        return s;
    for (int k = 0; k < n && f.ok(); ++k)
        s.orders.append(readOrder(f, v));
    n = f.i();
    if (!sane(f, n, "payments on a check"))
        return s;
    for (int k = 0; k < n && f.ok(); ++k) {
        Payment p = readPayment(f, v);
        if (v < 8)
            p.drawerId = s.drawerId;
        s.payments.append(p);
    }
    if (v >= 17)
        s.taxExempt = f.s();
    if (v >= 18)
        f.i();   // new_QST_method
    if (v >= 23)
        s.tabTotal = f.i();
    if (v >= 25)
        s.deliveryCharge = f.i();
    return s;
}

// Check::Read, from version 13 (a check with a customer of its own inside
// came before; no store kept those this long).
Check readCheck(Tokens &f, int v)
{
    Check c;
    if (v < 13 || v > 25) {
        f.fail(tr("checks of version %1 (this reads 13 to 25)").arg(v));
        return c;
    }
    c.serial = f.i();
    c.opened = f.t();
    c.userOpen = f.i();
    c.userOwner = f.i();
    c.flags = f.i();
    c.type = f.i();
    c.customerId = f.i();
    c.state = f.i();
    f.t();   // chef_time
    f.t();   // made_time
    c.number = f.i();
    c.checkIn = f.t();
    c.checkOut = f.t();
    c.date = f.t();
    c.comment = f.s();
    if (v >= 14)
        c.guests = f.i();
    if (v >= 15)
        f.i();   // has_takeouts
    if (v >= 16)
        c.label = f.s();
    if (v >= 24)
        f.i();   // call_center_id
    const int n = f.i();
    if (!sane(f, n, "parts of a check"))
        return c;
    for (int k = 0; k < n && f.ok(); ++k)
        c.subchecks.append(readSubCheck(f, v));
    return c;
}

Drawer readDrawer(Tokens &f, int v)   // Drawer::Read (version 5)
{
    Drawer d;
    if (v != 5) {
        f.fail(tr("drawers of version %1 (this reads 5)").arg(v));
        return d;
    }
    d.serial = f.i();
    d.host = f.s();
    d.position = f.i();
    d.start = f.t();
    d.pulled = f.t();
    d.balanced = f.t();
    d.owner = f.i();
    d.puller = f.i();
    d.number = f.i();
    d.mediaBalanced = f.i();
    int n = f.i();
    if (!sane(f, n, "counts in a drawer"))
        return d;
    for (int k = 0; k < n && f.ok(); ++k)
        d.balances.append({f.i(), f.i(), f.i()});
    n = f.i();
    if (!sane(f, n, "payments from a drawer"))
        return d;
    for (int k = 0; k < n && f.ok(); ++k) {
        Drawer::Move m;
        m.tenderType = f.i();
        m.amount = f.i();
        m.userId = f.i();
        m.targetId = f.i();
        m.time = f.t();
        d.payments.append(m);
    }
    return d;
}

WorkEntry readWork(Tokens &f, int v)   // WorkEntry::Read
{
    WorkEntry w;
    w.userId = f.i();
    w.job = f.i();
    if (v >= 4)
        w.payRate = f.i();
    w.payAmount = f.i();
    w.tips = f.i();
    w.start = f.t();
    w.end = f.t();
    w.endShift = f.i();
    // legacy keeps them to the minute
    if (w.start.isSet())
        w.start.time = QTime(w.start.time.hour(), w.start.time.minute());
    if (w.end.isSet())
        w.end.time = QTime(w.end.time.hour(), w.end.time.minute());
    const int editId = f.i();
    if (editId > 0) {
        w.edited = true;
        readWork(f, v);   // the entry as it was before
    }
    return w;
}

QStringList filesIn(const QString &folder, const QString &prefix)
{
    QStringList out;
    const QDir dir(folder);
    for (const QString &name : dir.entryList({prefix + u'*'}, QDir::Files))
        out.append(dir.filePath(name));
    // by the number after the prefix
    std::ranges::sort(out, {}, [&](const QString &p) {
        return QFileInfo(p).fileName().mid(prefix.size()).toLongLong();
    });
    return out;
}

} // namespace

// ---------------------------------------------------------------------------

std::optional<PageFile> readVtData(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 1, 1);
    if (!f)
        return std::nullopt;
    PageFile out;
    out.version = f->i();
    readPages(*f, out);
    f->i();   // the accounts' version
    const int n = f->i();
    for (int k = 0; k < n && f->ok(); ++k) {
        f->i();
        f->s();
    }
    return done(*f, out, error);
}

std::optional<PageFile> readPageFile(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 15, 29);
    if (!f)
        return std::nullopt;
    PageFile out;
    out.version = f->version();
    readPages(*f, out);
    if (out.version >= 28) {   // ZoneDB::Load: the store's defaults
        PageDefaults d;
        d.font = f->i();
        d.shadow = f->i();
        d.spacing = f->i();
        for (Look &l : d.looks) {
            l.frame = f->i();
            l.texture = f->i();
            l.color = f->i();
        }
        d.image = f->i();
        d.titleColor = f->i();
        d.size = f->i();
        out.defaults = d;
    }
    return done(*f, out, error);
}

std::optional<Menu> readMenu(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 8, 17);
    if (!f)
        return std::nullopt;
    const int v = f->version();
    Menu out;
    out.version = v;
    const int n = f->i();
    if (n < 0 || n > 100000) {
        f->fail(tr("%1 items").arg(n));
        return done(*f, out, error);
    }
    for (int k = 0; k < n && f->ok(); ++k) {   // SalesItem::Read
        MenuItem m;
        m.id = f->i();
        m.name = f->s();
        if (v >= 9)
            m.buttonName = f->s();
        m.printName = f->s();
        m.type = f->i();
        if (v >= 14)
            f->skip(5);   // location, event time, tickets, tickets left, price label
        m.cost = f->i();
        m.subCost = f->i();
        m.employeeCost = v >= 10 ? f->i() : m.cost;
        if (v >= 9) {
            m.takeoutCost = f->i();
            m.taxId = f->i();
            m.takeoutTaxId = f->i();
        }
        m.callOrder = std::clamp(f->i(), 0, 4);
        m.printer = f->i();
        m.family = f->i();
        if (m.family == 999)
            m.family = 255;
        m.itemClass = f->i();
        m.salesType = f->i();
        m.period = f->i();
        m.stocked = f->i();
        if (v >= 9) {
            f->i();   // components (none)
            m.prepareTime = f->i();
        }
        if (v >= 11)
            m.allowIncrease = f->i();
        if (v >= 12) {
            m.callCenterName = f->s();
            m.deliveryCost = f->i();
        }
        if (v >= 13)
            m.itemCode = f->s();
        if (v >= 15)
            f->i();   // ignore_split
        if (v >= 16)
            f->s();   // image_path (a file on that computer)
        if (v >= 17)
            m.outOfStock = f->i();
        out.items.append(m);
    }
    return done(*f, out, error);
}

std::optional<Settings> readSettings(const QString &datFolder, QString *error)
{
    std::optional<Tokens> f = openFile(QDir(datFolder).filePath(QStringLiteral("settings.dat")), error, 27, 107);
    if (!f)
        return std::nullopt;
    const int v = f->version();
    Settings s;
    s.version = v;
    s.storeName = f->s();
    s.storeAddress = f->s();
    if (v >= 58)
        s.storeAddress2 = f->s();
    f->i();   // region
    f->i();   // store
    s.taxFood = percent(f->i());
    s.taxAlcohol = percent(f->i());
    if (v >= 30) {
        s.taxRoom = percent(f->i());
        s.taxMerchandise = percent(f->i());
    }
    s.taxGst = percent(f->i());
    s.taxPst = percent(f->i());
    s.taxHst = percent(f->i());
    s.taxQst = percent(f->i());
    if (v >= 45)
        s.royalty = percent(f->i());
    if (v >= 52)
        s.taxVat = percent(f->i());
    f->i();   // screen_blank_time
    if (v >= 73)
        f->i();   // start_page_timeout
    if (v >= 31)
        f->skip(2);   // delay times
    s.useSeats = f->i();
    f->i();   // password_mode
    f->i();   // sale_credit
    s.drawerMode = f->i();
    f->i();   // receipt_print
    f->i();   // shifts_used
    f->i();   // split_kitchen
    f->i();   // developer_key
    s.priceRounding = f->i();
    s.doubleMult = v >= 104 ? f->f() : f->i();
    s.doubleAdd = f->i();
    f->skip(4);   // combine_accounts, change for checks, credit, gift
    if (v >= 28)
        f->i();   // change_for_roomcharge
    f->i();   // sales_period
    f->t();   // sales_start
    f->i();   // labor_period
    f->t();   // labor_start
    f->i();   // discount_alcohol
    // (version 26 and before: an older layout; this reads from 27)
    s.language = f->s();
    s.overtimeShift = f->i();
    s.overtimeWeek = f->i();
    s.wageWeekStart = f->i();
    f->i();   // authorize_method
    f->i();   // always_open
    if (v >= 28) {
        f->i();   // use_item_target
        s.timeFormat = f->i();
        s.dateFormat = f->i();
        f->i();   // number_format
        f->i();   // measure_system
    }
    const int jobs = f->i();
    for (int k = 0; k < jobs && f->ok(); ++k) {
        const int job = f->i();
        s.jobFlags.insert(job, f->i());
        s.jobLevel.insert(job, f->i());
    }
    if (v >= 47)
        for (int k = 0; k < 2; ++k)
            s.displayHeader.append(f->s());
    for (int k = 0; k < 4; ++k)
        s.receiptHeader.append(f->s());
    for (int k = 0; k < 4; ++k)
        s.receiptFooter.append(f->s());
    f->i();   // header_flags
    f->i();   // footer_flags
    for (int &x : s.shiftStart)
        x = f->i();
    for (int k = 0; k < 12; ++k) {
        s.mealActive[k] = f->i();
        s.mealStart[k] = f->i();
    }
    for (int k = 0; k < 64; ++k) {
        s.familyGroup[k] = f->i();
        s.familyPrinter[k] = f->i();
        s.videoTarget[k] = v >= 34 ? f->i() : s.familyPrinter[k];
    }
    int n = f->i();
    for (int k = 0; k < n && f->ok(); ++k)
        s.terminals.append(readTerminal(*f, v));
    n = f->i();
    for (int k = 0; k < n && f->ok(); ++k)
        s.printers.append(readPrinter(*f, v));
    readMedia(*f, s.media, v, true);
    if (v >= 28) {
        f->i();   // last tax id
        n = f->i();
        for (int k = 0; k < n && f->ok(); ++k) {
            Tax t;
            t.id = f->i();
            t.name = f->s();
            t.shortName = f->s();
            t.flags = f->i();
            t.amount = f->i();
            s.taxes.append(t);
        }
        f->i();   // last money id
        n = f->i();
        for (int k = 0; k < n && f->ok(); ++k) {   // MoneyInfo
            f->i();
            f->s();
            f->s();
            f->s();
            f->i();
            f->i();
            f->f();
        }
    }
    // The rest, as Settings::Load reads it (most of it not used here).
    if (v >= 29)
        f->skip(16 + 10);   // order entry windows; VisaNet (4 texts, 6 numbers)
    if (v >= 30)
        f->skip(3);   // update address, user, password
    if (v >= 100)
        f->i();   // auto_update_vt_data
    if (v >= 35)
        f->skip(2);   // account numbers
    if (v >= 36) {
        f->i();   // allow_user_select
        s.drawerDayFloat = f->i();
        s.drawerNightFloat = f->i();
    }
    if (v >= 37)
        f->skip(3);   // country_code, store_code, drawer_account
    if (v >= 38)
        f->i();   // min_day_length
    if (v >= 39)
        f->skip(2);   // drawer day and night start
    if (v >= 42 && v < 95)
        f->s();   // license_key
    if (v >= 48)
        f->s();   // email_send_server
    if (v >= 49)
        f->s();   // email_replyto
    if (v >= 100)
        f->skip(3);   // QuickBooks
    if (v >= 51)
        s.fastTakeouts = f->i();
    if (v >= 53)
        s.moneySymbol = f->s();
    if (v >= 54)
        f->i();   // require_drawer_balance
    if (v >= 56)
        f->i();   // default_report_period
    if (v >= 57) {
        f->skip(4);   // card number handling
        if (v >= 61)
            f->i();
        if (v >= 62)
            f->skip(2);
        if (v >= 63)
            f->i();
        if (v >= 67)
            f->s();   // merchant id
        f->skip(4);   // card server, port, user, password
        if (v >= 65)
            f->i();
        if (v >= 68)
            f->i();
        if (v >= 66)
            f->skip(8);   // card messages
    }
    if (v >= 59)
        f->i();
    if (v >= 70)
        f->i();
    if (v >= 60)
        f->i();
    if (v >= 64)
        f->i();
    if (v >= 69)
        f->i();
    if (v >= 70)
        f->skip(4);   // expire messages
    if (v >= 72)
        f->skip(3);
    if (v >= 73)
        f->i();
    if (v >= 79)
        f->i();
    if (v >= 107)
        f->i();
    if (v >= 82)
        f->i();
    if (v >= 96)
        f->i();
    if (v >= 97)
        f->skip(5);
    if (v >= 98)
        f->i();
    if (v >= 99)
        f->skip(3);
    if (v >= 102)
        f->skip(2);
    if (v >= 83)
        f->i();
    if (v >= 84)
        f->skip(3);
    if (v >= 85)
        f->i();
    if (v >= 86)
        f->i();
    if (v >= 87)
        f->i();
    if (v >= 88)
        f->i();
    if (v >= 89)
        s.advertiseFund = f->f();
    if (v >= 90)
        f->i();
    if (v >= 91)
        f->i();
    if (v >= 101)
        f->skip(6);   // kitchen video times and colors
    if (v >= 106)
        f->skip(2);
    std::optional<Settings> out = done(*f, s, error);
    if (!out)
        return out;
    // conf/*.ini, read after settings.dat by legacy, win over it.
    const QDir conf(QDir(datFolder).filePath(QStringLiteral("conf")));
    const QHash<QString, QString> tax = readIni(conf.filePath(QStringLiteral("tax.ini")));
    const QHash<QString, QString> fees = readIni(conf.filePath(QStringLiteral("fees.ini")));
    const QHash<QString, QString> fast = readIni(conf.filePath(QStringLiteral("fastfood.ini")));
    const auto num = [](const QHash<QString, QString> &ini, const QString &key, double &into) {
        bool ok = false;
        const double d = ini.value(key).toDouble(&ok);
        if (ok)
            into = d;
    };
    num(tax, QStringLiteral("Sales Tax Canada/merchandise_tax"), out->taxMerchandise);
    num(tax, QStringLiteral("Sales Tax Canada/GST"), out->taxGst);
    num(tax, QStringLiteral("Sales Tax Canada/PST"), out->taxPst);
    num(tax, QStringLiteral("Sales Tax Canada/HST"), out->taxHst);
    num(tax, QStringLiteral("Sales Tax Canada/QST"), out->taxQst);
    num(fees, QStringLiteral("Miscellany/royalty_rate"), out->royalty);
    num(fees, QStringLiteral("Miscellany/advertise_fund"), out->advertiseFund);
    const QStringList inclusive{u"food_inclusive"_s, u"room_inclusive"_s, u"alcohol_inclusive"_s,
                                u"merchandise_inclusive"_s};
    for (int k = 0; k < 4; ++k)
        out->taxInclusive[k] = tax.value(u"Miscellany/"_s + inclusive[k]).toInt();
    if (fast.contains(u"Miscellany/tax_takeout_food"_s))
        out->taxTakeoutFood = fast.value(u"Miscellany/tax_takeout_food"_s).toInt() != 0;
    return out;
}

std::optional<QList<Employee>> readEmployees(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 7, 8);
    if (!f)
        return std::nullopt;
    const int v = f->version();
    QList<Employee> out;
    const int n = f->i();
    for (int k = 0; k < n && f->ok(); ++k) {   // Employee::Read
        Employee e;
        e.systemName = f->s();
        e.lastName = f->s();
        e.firstName = f->s();
        e.address = f->s();
        e.city = f->s();
        e.state = f->s();
        e.phone = f->s();
        f->s();   // the SSN: never kept
        e.description = f->s();
        e.id = f->i();
        e.key = f->i();
        e.employeeNo = f->i();
        const int dept = v <= 7 ? f->i() : 0;
        e.training = f->i();
        e.password = f->s();
        e.active = f->i();
        const int jobs = f->i();
        for (int j = 0; j < jobs && f->ok(); ++j) {   // JobInfo::Read
            Job job;
            job.job = f->i();
            job.payRate = f->i();
            job.payAmount = f->i();
            job.startPage = f->i();
            job.dept = v >= 8 ? f->i() : dept;
            e.jobs.append(job);
        }
        out.append(e);
    }
    return done(*f, out, error);
}

std::optional<Customer> readCustomer(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 1, 14);
    if (!f)
        return std::nullopt;
    int v = f->version();
    if (v == 1)
        v = 13;
    if (v < 13) {
        if (error)
            *error = tr("%1 is from a ViewTouch this can't read (version %2; it reads 13 and 14).")
                         .arg(f->name())
                         .arg(v);
        return std::nullopt;
    }
    Customer c;   // CustomerInfo::Read
    c.id = f->i();
    c.lastName = f->s();
    c.firstName = f->s();
    c.company = f->s();
    c.phone = f->s();
    c.address = f->s();
    c.city = f->s();
    c.state = f->s();
    c.postal = f->s();
    f->s();   // card number: never kept
    f->s();   // its expiry
    f->s();   // license
    c.comment = f->s();
    if (v >= 14) {
        c.extension = f->s();
        c.address2 = f->s();
        c.crossStreet = f->s();
    }
    return done(*f, c, error);
}

std::optional<Archive> readArchive(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 10, 14);
    if (!f)
        return std::nullopt;
    Archive a;   // Archive::LoadPacked
    a.version = f->version();
    a.id = f->i();
    a.start = f->t();
    a.end = f->t();
    int ver = f->i();
    int n = f->i();
    if (sane(*f, n, "drawers"))
        for (int k = 0; k < n && f->ok(); ++k)
            a.drawers.append(readDrawer(*f, ver));
    ver = f->i();
    n = f->i();
    if (sane(*f, n, "checks"))
        for (int k = 0; k < n && f->ok(); ++k)
            a.checks.append(readCheck(*f, ver));
    f->i();   // tips' version
    n = f->i();
    if (sane(*f, n, "tips"))
        for (int k = 0; k < n && f->ok(); ++k)
            a.tips.append({f->i(), f->i(), f->i()});
    // ExceptionDB::Read (version 3): voids and comps, table moves, rebuilds
    if (f->i() != 3)
        f->fail(tr("a voids and comps list of another version"));
    n = f->i();
    for (int k = 0; k < n && f->ok(); ++k) {
        f->t();
        f->i();
        const int type = f->i();
        a.comps += type == 1;
        a.voids += type == 2;
        f->i();
        f->i();
        f->s();
        f->i();
        f->i();
    }
    n = f->i();
    for (int k = 0; k < n && f->ok(); ++k) {
        f->t();
        f->skip(3);
        f->s();
        f->i();
    }
    n = f->i();
    for (int k = 0; k < n && f->ok(); ++k) {
        f->t();
        f->skip(2);
    }
    // ExpenseDB::Read
    ver = f->i();
    if (ver >= 4)
        f->i();   // entered
    n = f->i();
    a.expenses = n;
    for (int k = 0; k < n && f->ok(); ++k) {
        f->skip(5);   // eid, account, employee, drawer, amount
        f->s();       // document
        if (ver >= 2)
            f->s();
        if (ver >= 3)
            f->skip(2);
        if (ver >= 5)
            f->i();
        if (ver >= 6)
            f->i();
        if (ver >= 7)
            f->i();
        if (ver >= 8)
            f->t();
    }
    // The day's discounts, coupons, cards, comps and meals, as they were.
    const int mediaVersion = f->i();
    a.media = Media{};
    readMedia(*f, *a.media, mediaVersion, false);
    if (a.version >= 11) {
        a.taxFood = f->f();
        a.taxAlcohol = f->f();
        a.taxRoom = f->f();
        a.taxMerchandise = f->f();
        a.taxGst = f->f();
        a.taxPst = f->f();
        a.taxHst = f->f();
        a.taxQst = f->f();
        f->f();   // royalty
        a.priceRounding = f->i();
        f->skip(5);   // change for credit, room, checks, gift; discount_alcohol
    }
    if (a.version >= 12)
        a.taxVat = f->f();
    if (a.version >= 13) {
        for (int db = 0; db < 3; ++db) {   // CreditDB: exceptions, refunds, voids
            f->i();
            n = f->i();
            a.cardRecords += n;
            for (int k = 0; k < n && f->ok(); ++k)
                readCredit(*f, nullptr);
        }
        f->i();   // CCInit
        n = f->i();
        f->skip(n);
        f->i();   // CCSAFDetails
        n = f->i();
        f->skip(16 * n);
        f->i();   // CCSettle
        n = f->i();
        f->skip((14 + 10 * 8) * n);
    }
    if (a.version >= 14)
        f->f();   // advertise_fund
    return done(*f, a, error);
}

std::optional<OpenDay> readCurrent(const QString &folder, QString *error)
{
    OpenDay out;
    for (const QString &path : filesIn(folder, QStringLiteral("check_"))) {
        std::optional<Tokens> f = openFile(path, error, 13, 25);
        if (!f)
            return std::nullopt;
        Check c = readCheck(*f, f->version());
        if (!done(*f, c, error))
            return std::nullopt;
        out.checks.append(c);
    }
    for (const QString &path : filesIn(folder, QStringLiteral("drawer_"))) {
        std::optional<Tokens> f = openFile(path, error, 5, 5);
        if (!f)
            return std::nullopt;
        Drawer d = readDrawer(*f, f->version());
        if (!done(*f, d, error))
            return std::nullopt;
        out.drawers.append(d);
    }
    return out;
}

std::optional<LaborPeriod> readLabor(const QString &path, QString *error)
{
    std::optional<Tokens> f = openFile(path, error, 2, 4);
    if (!f)
        return std::nullopt;
    LaborPeriod p;   // LaborPeriod::Load
    p.serial = f->i();
    p.end = f->t();
    const int n = f->i();
    if (n < 0 || n > 1000000)
        f->fail(tr("%1 punches").arg(n));
    for (int k = 0; k < n && f->ok(); ++k)
        p.entries.append(readWork(*f, f->version()));
    return done(*f, p, error);
}

bool isDatFolder(const QString &folder)
{
    const QDir d(folder);
    return d.exists(QStringLiteral("menu.dat")) || d.exists(QStringLiteral("settings.dat"))
           || d.exists(QStringLiteral("zone_db.dat"));
}

Store readStore(const QString &folder)
{
    Store s;
    s.folder = folder;
    const QDir d(folder);
    QString error;
    const auto note = [&] {
        if (!error.isEmpty())
            s.errors.append(error);
        error.clear();
    };
    if (d.exists(QStringLiteral("vt_data"))) {
        s.vtData = readVtData(d.filePath(QStringLiteral("vt_data")), &error);
        note();
    }
    if (d.exists(QStringLiteral("zone_db.dat"))) {
        s.pages = readPageFile(d.filePath(QStringLiteral("zone_db.dat")), &error);
        note();
    }
    if (d.exists(QStringLiteral("tables.dat"))) {
        s.tables = readPageFile(d.filePath(QStringLiteral("tables.dat")), &error);
        note();
    }
    if (d.exists(QStringLiteral("menu.dat"))) {
        s.menu = readMenu(d.filePath(QStringLiteral("menu.dat")), &error);
        note();
    }
    if (d.exists(QStringLiteral("settings.dat"))) {
        s.settings = readSettings(folder, &error);
        note();
    }
    if (d.exists(QStringLiteral("employee.dat"))) {
        if (auto e = readEmployees(d.filePath(QStringLiteral("employee.dat")), &error))
            s.employees = *e;
        note();
    }
    for (const QString &path : filesIn(d.filePath(QStringLiteral("customers")), QStringLiteral("customer_"))) {
        if (auto c = readCustomer(path, &error))
            s.customers.append(*c);
        note();
    }
    for (const QString &path : filesIn(d.filePath(QStringLiteral("archive")), QStringLiteral("archive_"))) {
        if (auto a = readArchive(path, &error))
            s.archives.append(*a);
        note();
    }
    if (d.exists(QStringLiteral("current"))) {
        s.current = readCurrent(d.filePath(QStringLiteral("current")), &error);
        note();
    }
    for (const QString &path : filesIn(d.filePath(QStringLiteral("labor")), QStringLiteral("labor_"))) {
        if (auto l = readLabor(path, &error))
            s.labor.append(*l);
        note();
    }
    return s;
}

} // namespace vt::legacy
