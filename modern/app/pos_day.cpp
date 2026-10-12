// PosService: business day, cash drawer, reports, receipts, split checks.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QThreadPool>
#include <QPointer>
#include <QLocale>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QString clockText(std::int64_t ms)
{
    return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms).time(), QLocale::ShortFormat);
}

// A report's words in the screen's language: its title, columns, and each
// cell that is a phrase on its own ("Net sales"; amounts and names stay).
QString reportWords(const std::string &text)
{
    return i18n::phrase(i18n::current(), qs(text));
}

QVariantMap toVariant(const Report &r)
{
    QVariantList rows;
    for (const ReportRow &row : r.rows) {
        QStringList cells;
        for (const std::string &c : row.cells)
            cells << reportWords(c);
        QString kind = u"line"_s;
        if (row.kind == ReportRow::Kind::Section) kind = u"section"_s;
        else if (row.kind == ReportRow::Kind::Total) kind = u"total"_s;
        else if (row.kind == ReportRow::Kind::Note) kind = u"note"_s;
        rows.append(QVariantMap{{u"kind"_s, kind}, {u"cells"_s, cells}});
    }
    QStringList columns;
    for (const std::string &c : r.columns)
        columns << reportWords(c);
    return {{u"id"_s, qs(r.id)}, {u"title"_s, reportWords(r.title)}, {u"subtitle"_s, reportWords(r.subtitle)},
            {u"columns"_s, columns}, {u"rows"_s, rows}};
}

const QStringList kReportIds = {u"sales"_s, u"items"_s, u"categories"_s, u"hourly"_s, u"servers"_s, u"tips"_s,
                                u"labor"_s, u"drawer"_s, u"expenses"_s, u"purchases"_s, u"audit"_s, u"accounts"_s, u"kitchen"_s, u"foodcost"_s, u"engineering"_s, u"turns"_s,
                                u"exceptions"_s, u"deposit"_s, u"customers"_s,
                                u"royalty"_s, u"accounting"_s, u"checklists"_s, u"drivers"_s};

} // namespace

void PosShared::startDay()
{
    day = BusinessDay{++lastDayId, now(), 0};
    if (sink)
        sink->saveDay(day, {});
}

QString PosService::dayLabel(const BusinessDay &day) const
{
    const QDateTime opened = QDateTime::fromMSecsSinceEpoch(day.openedAt);
    return i18n::locale().toString(opened.date(), QLocale::ShortFormat) + u"  "_s + clockText(day.openedAt);
}

ReportContext PosService::reportContext(const QString &period) const
{
    // The pay week started on the last weekStartsOn day (0 Sunday) at midnight.
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    const int back = (today.dayOfWeek() % 7 - s_->settings.weekStartsOn + 7) % 7;
    const std::int64_t weekStart = QDateTime(today.addDays(-back), QTime(0, 0)).toMSecsSinceEpoch();
    return ReportContext{s_->settings, ss(period), [](std::int64_t ms) { return ss(clockText(ms)); }, now(),
                         [](std::int64_t ms) { return QDateTime::fromMSecsSinceEpoch(ms).time().hour(); },
                         [](std::int64_t ms) { return int(QDateTime::fromMSecsSinceEpoch(ms).date().toJulianDay()); },
                         weekStart,
                         [](std::int64_t ms) {
                             return ss(i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms).date(), u"MMM d"_s));
                         }};
}

// --- receipts ------------------------------------------------------------------------

bool PosService::printReceipt()
{
    const Check *c = currentCheck();
    if (!c) {
        for (const Check &closed : s_->closedToday) {
            if (closed.id == lastClosedId_)
                c = &closed;
        }
    }
    if (!c)
        return fail(tr("No check to print."));
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    if (askReceiptPrinter()) {   // a handheld: where?
        offerReceipt(*c, true);
        return true;
    }
    s_->printer->printReceipt(s_->settings, *c, receiptPrinter());
    emit notice(tr("Printing receipt for %1").arg(qs(c->label)));
    return true;
}

bool PosService::noSale()
{
    if (!require(perm::Settle, tr("Opening the drawer")))
        return false;
    if (!terminalHasDrawer())
        return fail(tr("%1 has no cash drawer.").arg(terminal_));
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    s_->printer->openDrawer(s_->settings, receiptPrinter());
    // Kept with the drawer: who opened it with nothing sold (exceptions report).
    if (DrawerSession *d = myDrawer()) {
        CashMovement m;
        m.id = d->nextMovementId++;
        m.kind = CashMovement::Kind::NoSale;
        m.by = user() ? user()->name : std::string();
        m.at = now();
        d->movements.push_back(m);
        if (s_->sink)
            s_->sink->saveDrawer(*d);
        emit s_->drawerChanged();
    }
    emit notice(tr("Drawer opened (no sale)"));
    return true;
}

// --- split check ---------------------------------------------------------------------

void PosService::setCheckFilter(const QString &label)
{
    if (label == checkFilter_)
        return;
    checkFilter_ = label;
    emit openChecksChanged();   // this terminal only
}

Check &PosService::splitOff(qint64 fromId)
{
    const Check &from = s_->open.at(fromId);
    Check n;
    n.id = ++s_->lastCheckId;
    n.type = from.type;
    n.label = from.label;
    n.guests = 1;
    n.serverId = from.serverId;
    n.serverName = from.serverName;
    n.openedAt = now();
    n.training = from.training;
    const auto id = n.id;
    return s_->open.emplace(id, std::move(n)).first->second;
}

bool PosService::splitLine(qint64 targetCheckId, bool all)
{
    if (!require(perm::Order, tr("Splitting checks")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (!c->line(selectedLine_))
        return fail(tr("Touch an item on the check first."));
    if (!c->payments.empty())
        return fail(tr("Remove payments before splitting this check."));

    Check *target = nullptr;
    if (targetCheckId != 0) {
        auto it = s_->open.find(targetCheckId);
        if (it == s_->open.end() || it->second.label != c->label)
            return fail(tr("That check is not at this table."));
        if (!it->second.payments.empty())
            return fail(tr("Remove payments on check #%1 first.").arg(targetCheckId));
        if (const QString holder = lockHolder(targetCheckId); !holder.isEmpty())
            return fail(tr("Check #%1 is open on %2.").arg(targetCheckId).arg(holder));
        target = &it->second;
    } else {
        target = &splitOff(c->id);
        c = current();   // map insert keeps references, but be explicit
    }

    // One at a time (2 × Cheeseburger: one moves, one stays), or the line.
    std::optional<OrderLine> line = all ? c->takeLine(selectedLine_) : c->takeOne(selectedLine_);
    const int moved = line->quantity;
    const QString name = qs(line->displayName());
    target->adoptLine(std::move(*line));
    // What's left of it stays chosen, so another touch moves the next one.
    if (!c->line(selectedLine_))
        selectedLine_ = 0;
    if (s_->sink) {
        s_->sink->saveCheck(*target);
        s_->sink->saveCheck(*c);
    }
    emit notice(moved > 1 ? tr("Moved %1 × %2 to check #%3").arg(moved).arg(name).arg(target->id)
                          : tr("Moved %1 to check #%2").arg(name).arg(target->id));
    emit checkChanged();
    emit s_->checksChanged();
    return true;
}

bool PosService::shareLine(int parts)
{
    if (!require(perm::Order, tr("Splitting checks")))
        return false;
    Check *c = current();
    OrderLine *l = c ? c->line(selectedLine_) : nullptr;
    if (!l)
        return fail(tr("Touch an item on the check first."));
    if (!c->payments.empty())
        return fail(tr("Remove payments before splitting this check."));
    if (l->isComment() || l->voided)
        return fail(tr("%1 can't be shared.").arg(qs(l->displayName())));
    if (l->isShare())
        return fail(tr("%1 is already shared: Put Back Together first.").arg(qs(l->displayName())));
    parts = std::clamp(parts, 2, 20);
    // 2 × Wine: one of them is shared, the other stays as it is.
    if (l->quantity > 1) {
        OrderLine one = *c->takeOne(l->id);
        selectedLine_ = c->adoptLine(std::move(one)).id;
    }
    const QString name = qs(c->line(selectedLine_)->displayName());
    const std::vector<std::int64_t> ids = c->shareLine(selectedLine_, parts, c->id * 1000000 + selectedLine_);
    if (ids.empty())
        return fail(tr("%1 can't be shared.").arg(name));
    noteEvent(*c, tr("%1 shared %2 ways").arg(name).arg(parts), "split");
    selectedLine_ = ids[1];   // the next one to move
    changed(*c);
    emit notice(tr("%1 is in %2 pieces: move each to its check.").arg(name).arg(parts));
    return true;
}

bool PosService::splitEvenly(int ways)
{
    if (!require(perm::Order, tr("Splitting checks")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (!c->payments.empty())
        return fail(tr("Remove payments before splitting this check."));
    ways = std::clamp(ways, 2, 20);
    std::vector<std::int64_t> ids;
    for (const OrderLine &l : c->lines) {
        if (l.isShare())
            return fail(tr("%1 is already shared: Put Back Together first.").arg(qs(l.displayName())));
        if (!l.voided && !l.isComment())
            ids.push_back(l.id);
    }
    if (ids.empty())
        return fail(tr("Nothing on this check to split."));

    const std::int64_t here = c->id;
    std::vector<std::int64_t> checks{here};
    for (int i = 1; i < ways; ++i)
        checks.push_back(splitOff(here).id);
    c = &s_->open.at(here);
    // Every item split even: whole ones while they go around, the rest in
    // pieces, one on each check. Which check gets the first piece (and a
    // leftover cent) turns, so the cents spread out.
    int turn = 0;
    for (const std::int64_t id : ids) {
        // 4 × Tacos 3 ways: a whole one on each check, the last one shared.
        const int quantity = c->line(id)->quantity;
        const int each = quantity / ways;
        for (int k = 0; k < ways && each > 0; ++k) {
            const qint64 to = checks[k];
            if (to == here)
                continue;
            OrderLine whole = *c->line(id);
            whole.quantity = each;
            c->line(id)->quantity -= each;
            s_->open.at(to).adoptLine(std::move(whole));
        }
        std::vector<std::int64_t> ones;
        if (quantity % ways) {
            // What's left: one at a time, each in pieces.
            OrderLine *l = c->line(id);
            const int rest = quantity % ways;
            if (each > 0) {
                l->quantity = each;
                for (int k = 0; k < rest; ++k) {
                    OrderLine one = *l;
                    one.quantity = 1;
                    ones.push_back(c->adoptLine(std::move(one)).id);
                }
            } else {
                ones.push_back(id);
                for (int k = 1; k < rest; ++k)
                    ones.push_back(c->adoptLine(*c->takeOne(id)).id);
            }
        }
        for (const std::int64_t one : ones) {
            const std::vector<std::int64_t> pieces = c->shareLine(one, ways, here * 1000000 + one);
            for (int k = 0; k < ways; ++k) {
                const qint64 to = checks[(k + turn) % ways];
                if (to == here)
                    continue;
                if (std::optional<OrderLine> p = c->takeLine(pieces[k]))
                    s_->open.at(to).adoptLine(std::move(*p));
            }
            ++turn;
        }
    }
    // Courses, seats... stay as they were; the guests spread out.
    const int guests = c->guests;
    for (std::size_t i = 0; i < checks.size(); ++i) {
        Check &k = s_->open.at(checks[i]);
        k.guests = std::max(1, guests / ways + (int(i) < guests % ways ? 1 : 0));
        noteEvent(k, i == 0 ? tr("Split evenly %1 ways").arg(ways) : tr("1 of %1 from check #%2").arg(ways).arg(here), "split");
        if (s_->sink)
            s_->sink->saveCheck(k);
    }
    selectedLine_ = 0;
    emit notice(tr("%1: split evenly into %2 checks").arg(qs(c->label)).arg(ways));
    emit checkChanged();
    emit s_->checksChanged();
    return true;
}

bool PosService::unshareLine()
{
    if (!require(perm::Order, tr("Splitting checks")))
        return false;
    Check *c = current();
    const OrderLine *l = c ? c->line(selectedLine_) : nullptr;
    if (!l || !l->isShare())
        return fail(tr("Touch a shared item first."));
    const std::int64_t group = l->shareGroup;
    // Every piece must be at this table, on a check nobody is paying.
    std::vector<qint64> holding;
    int found = 0;
    for (auto &[id, k] : s_->open) {
        if (std::ranges::none_of(k.lines, [&](const OrderLine &x) { return x.isShare() && x.shareGroup == group; }))
            continue;
        if (k.label != c->label)
            return fail(tr("A piece of it is on check #%1, at %2.").arg(id).arg(qs(k.label)));
        if (!k.payments.empty())
            return fail(tr("Remove payments on check #%1 first.").arg(id));
        if (id != c->id)
            if (const QString holder = lockHolder(id); !holder.isEmpty())
                return fail(tr("Check #%1 is open on %2.").arg(id).arg(holder));
        holding.push_back(id);
        for (const OrderLine &x : k.lines)
            if (x.isShare() && x.shareGroup == group) {
                if (x.voided)
                    return fail(tr("A piece of it was voided."));
                ++found;
            }
    }
    // A piece already paid for (its check closed): the rest stay pieces.
    if (found < l->shareOf)
        return fail(tr("A piece of it was already paid for."));
    std::optional<OrderLine> whole;
    for (const qint64 id : holding) {
        Check &k = s_->open.at(id);
        std::vector<std::int64_t> ids;
        for (const OrderLine &x : k.lines)
            if (x.isShare() && x.shareGroup == group)
                ids.push_back(x.id);
        for (const std::int64_t x : ids) {
            OrderLine p = *k.takeLine(x);
            if (!whole || p.share == 1)
                whole = std::move(p);
        }
        if (id != c->id && s_->sink)
            s_->sink->saveCheck(k);
    }
    c = &s_->open.at(c->id);
    whole->share = 0;
    whole->shareOf = 0;
    whole->shareGroup = 0;
    const QString name = qs(whole->displayName());
    selectedLine_ = c->adoptLine(std::move(*whole)).id;
    emit notice(tr("%1 is back together").arg(name));
    changed(*c);
    emit s_->checksChanged();
    return true;
}

bool PosService::splitBySeat()
{
    if (!require(perm::Order, tr("Splitting checks")))
        return false;
    Check *c = current();
    if (!c || c->type != CheckType::DineIn)
        return fail(tr("Open a table's check first."));
    if (!c->payments.empty())
        return fail(tr("Remove payments before splitting this check."));
    std::set<int> seats;
    bool unseated = false;
    for (const OrderLine &l : c->lines) {
        if (l.seat > 0)
            seats.insert(l.seat);
        else
            unseated = true;
    }
    if (seats.empty() || (seats.size() == 1 && !unseated))
        return fail(tr("Give the items seats first (Seat − / +), then split by seat."));
    // Items with no seat stay here; else the first seat does.
    const int stays = unseated ? 0 : *seats.begin();
    const std::int64_t here = c->id;
    int made = 0;
    for (const int seat : seats) {
        if (seat == stays)
            continue;
        Check n;
        n.id = ++s_->lastCheckId;
        n.type = c->type;
        n.label = c->label;
        n.guests = 1;
        n.serverId = c->serverId;
        n.serverName = c->serverName;
        n.openedAt = now();
        n.training = c->training;
        const auto id = n.id;
        s_->open.emplace(id, std::move(n));
        c = &s_->open.at(here);
        Check &target = s_->open.at(id);
        std::vector<std::int64_t> ids;
        for (const OrderLine &l : c->lines)
            if (l.seat == seat)
                ids.push_back(l.id);
        for (const std::int64_t lineId : ids)
            if (std::optional<OrderLine> line = c->takeLine(lineId))
                target.adoptLine(std::move(*line));
        noteEvent(target, tr("Seat %1 from check #%2").arg(seat).arg(here), "split");
        if (s_->sink)
            s_->sink->saveCheck(target);
        ++made;
    }
    selectedLine_ = 0;
    noteEvent(*c, tr("Split by seat into %1 more checks").arg(made), "split");
    if (s_->sink)
        s_->sink->saveCheck(*c);
    emit notice(tr("%1: one check per seat (%2 checks)").arg(qs(c->label)).arg(made + 1));
    emit checkChanged();
    emit s_->checksChanged();
    return true;
}

bool PosService::testPrinter(const QString &printerId, bool kickDrawer)
{
    if (!require(perm::Manager, tr("Testing printers")))
        return false;
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    const auto it = std::ranges::find_if(s_->settings.printers, [&](const PrinterConfig &p) { return qs(p.id) == printerId; });
    if (it == s_->settings.printers.end())
        return fail(tr("Save the printer first."));
    if (it->type == "none")
        return fail(tr("%1 is off (Connection: Off).").arg(qs(it->name)));
    if (!s_->printer->printTestPage(s_->settings, it->id, kickDrawer))
        return fail(tr("Save the printer first."));
    emit notice(kickDrawer && it->drawerKick ? tr("Test page sent to %1, and its drawer opened").arg(qs(it->name))
                                             : tr("Test page sent to %1").arg(qs(it->name)));
    return true;
}

bool PosService::printTableChecks()
{
    const QVariantList checks = tableChecks();
    if (checks.isEmpty())
        return fail(tr("Open a table's check first."));
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    for (const QVariant &v : checks)
        if (const auto it = s_->open.find(v.toMap().value(u"id"_s).toLongLong()); it != s_->open.end())
            s_->printer->printReceipt(s_->settings, it->second, receiptPrinter());
    emit notice(tr("Printing %n receipt(s)", nullptr, int(checks.size())));
    return true;
}

bool PosService::combineTableChecks()
{
    const Check *c = currentCheck();
    if (!c || c->type != CheckType::DineIn)
        return fail(tr("Open a table's check first."));
    const std::int64_t here = c->id;
    // Check first, so it's all or nothing.
    for (const QVariant &v : tableChecks()) {
        const std::int64_t id = v.toMap().value(u"id"_s).toLongLong();
        if (id == here)
            continue;
        if (const QString holder = lockHolder(id); !holder.isEmpty())
            return fail(tr("Check #%1 is open on %2.").arg(id).arg(holder));
    }
    int merged = 0;
    for (const QVariant &v : tableChecks())
        if (const std::int64_t id = v.toMap().value(u"id"_s).toLongLong(); id != here) {
            if (!mergeCheck(id))
                return false;
            ++merged;
        }
    if (merged == 0)
        return fail(tr("This is the table's only check."));
    emit notice(tr("%1: one check again").arg(qs(currentCheck()->label)));
    return true;
}

// --- drawers (one per terminal) or server banks (one per person) ----------------------

namespace {
// Drawers from before per-terminal drawers have no terminal: any terminal may
// use them. Server banks are never a terminal's drawer.
bool belongsTo(const DrawerSession &d, const std::string &terminal)
{
    return d.employeeId.empty() && (d.terminal.empty() || d.terminal == terminal);
}
} // namespace

DrawerSession *PosShared::openBankFor(const std::string &employeeId)
{
    for (DrawerSession &d : drawers) {
        if (d.open() && !employeeId.empty() && d.employeeId == employeeId)
            return &d;
    }
    return nullptr;
}

const DrawerSession *PosShared::latestBankFor(const std::string &employeeId) const
{
    const DrawerSession *latest = nullptr;
    for (const DrawerSession &d : drawers) {
        if (!employeeId.empty() && d.employeeId == employeeId && (!latest || d.id > latest->id))
            latest = &d;
    }
    return latest;
}

QString PosService::noDrawerMessage() const
{
    return terminalHasDrawer()
               ? tr("Open this terminal's cash drawer first (Drawer…).")
               : tr("%1 has no cash drawer. Take cash on a terminal with one, or use your own bank.").arg(terminal_);
}

bool PosService::mustCloseChecksToCheckOut() const
{
    if (const Employee *e = user(); e && !e->checkout.empty())
        return e->checkout == "closeChecks";
    return s_->settings.checkoutNeedsClosedChecks;
}

bool PosService::serverBank() const
{
    if (const Employee *e = user(); e && !e->cashMode.empty())
        return e->cashMode == "serverBank";
    if (const Employee *e = user(); e && e->role == "driver")
        return true;   // drivers carry their own bank (the cash they collect, change, tips)
    return s_->settings.cashMode == CashMode::ServerBank;
}

DrawerSession *PosService::myDrawer()
{
    if (serverBank())
        return user() ? s_->openBankFor(user()->id) : nullptr;
    return s_->openDrawerFor(terminal_.toStdString());
}

const DrawerSession *PosService::latestMyDrawer() const
{
    if (serverBank())
        return user() ? s_->latestBankFor(user()->id) : nullptr;
    return s_->latestDrawerFor(terminal_.toStdString());
}

DrawerSession *PosService::ensureMyBank(Money start)
{
    if (DrawerSession *mine = myDrawer())
        return mine;
    const Employee *e = user();
    if (!e)
        return nullptr;
    DrawerSession d;
    d.id = ++s_->lastDrawerId;
    d.name = ss(tr("%1's bank").arg(qs(e->name)));
    d.employeeId = e->id;
    d.openedAt = now();
    d.openedBy = e->name;
    d.startingCash = start;
    s_->drawers.push_back(d);
    if (s_->sink)
        s_->sink->saveDrawer(d);
    return &s_->drawers.back();
}

Money PosService::expectedNow(const DrawerSession &d) const
{
    return d.open() ? expectedCash(d, s_->closedToday, s_->settings.tax) : d.expected;
}

DrawerSession *PosShared::openDrawerFor(const std::string &terminal)
{
    for (DrawerSession &d : drawers) {
        if (d.open() && belongsTo(d, terminal))
            return &d;
    }
    return nullptr;
}

const DrawerSession *PosShared::latestDrawerFor(const std::string &terminal) const
{
    const DrawerSession *latest = nullptr;
    for (const DrawerSession &d : drawers) {
        if (belongsTo(d, terminal) && (!latest || d.id > latest->id))
            latest = &d;
    }
    return latest;
}

bool PosService::openDrawerSession()
{
    if (!require(perm::Settle, serverBank() ? tr("Starting a bank") : tr("Opening a drawer")))
        return false;
    if (serverBank()) {
        if (myDrawer())
            return fail(tr("Your bank is already open."));
        const DrawerSession *d = ensureMyBank(Money::fromCents(entry_.toLongLong()));
        entry_.clear();
        emit entryChanged();
        emit notice(tr("%1 started with %2").arg(qs(d->name), format(d->startingCash)));
        emit s_->drawerChanged();
        emit s_->dayChanged();
        return true;
    }
    if (!terminalHasDrawer())
        return fail(tr("%1 has no cash drawer.").arg(terminal_));
    const std::string terminal = terminal_.toStdString();
    if (const DrawerSession *open = s_->openDrawerFor(terminal))
        return fail(tr("%1 is already open.").arg(qs(open->name)));
    DrawerSession d;
    d.id = ++s_->lastDrawerId;
    d.name = ss(tr("%1 drawer").arg(terminal_));
    d.terminal = terminal;
    d.openedAt = now();
    d.openedBy = user()->name;
    d.startingCash = Money::fromCents(entry_.toLongLong());
    s_->drawers.push_back(d);
    entry_.clear();
    emit entryChanged();
    if (s_->sink)
        s_->sink->saveDrawer(d);
    if (s_->printer)
        s_->printer->openDrawer(s_->settings, receiptPrinter());
    emit notice(tr("%1 opened with %2").arg(qs(d.name), format(d.startingCash)));
    emit s_->drawerChanged();
    emit s_->dayChanged();
    return true;
}

bool PosService::countDrawer()
{
    if (!require(perm::Settle, serverBank() ? tr("Checking out") : tr("Counting the drawer")))
        return false;
    DrawerSession *open = myDrawer();
    if (!open)
        return fail(serverBank() ? tr("You have no bank open.") : tr("No drawer is open on this terminal."));
    if (serverBank() && mustCloseChecksToCheckOut()) {
        const auto mine = std::ranges::count_if(s_->open, [&](const auto &entry) {
            return entry.second.serverId == user()->id;
        });
        if (mine > 0)
            return fail(mine == 1 ? tr("Close or hand over your open check before checking out.")
                                  : tr("Close or hand over your %1 open checks before checking out.").arg(mine));
    }
    return closeDrawer(*open);
}

bool PosService::countDrawerPieces(qint64 drawerId, const QVariantList &counts)
{
    // How many of each bill and coin, in the order of the store's list.
    std::vector<std::pair<std::int64_t, int>> pieces;
    std::int64_t total = 0;
    const auto &worth = s_->settings.cashDenominations;
    for (qsizetype i = 0; i < counts.size() && i < qsizetype(worth.size()); ++i) {
        const int n = counts[i].toInt();
        if (n < 0 || n > 100000)
            return fail(tr("That count doesn't look right."));
        if (n > 0) {
            pieces.emplace_back(worth[i], n);
            total += worth[i] * n;
        }
    }
    entry_ = QString::number(total);
    countedPieces_ = pieces;
    const bool done = drawerId == 0 ? countDrawer() : countDrawerById(drawerId);
    if (!done) {
        countedPieces_.clear();
        entry_.clear();
    }
    emit entryChanged();
    return done;
}

bool PosService::blindCount() const
{
    const Employee *e = user();
    if (e && !e->blindCount.empty())
        return e->blindCount == "blind";
    return s_->settings.blindDrawerCount && !can(QString::fromLatin1(perm::Manager));
}

bool PosService::countDrawerById(qint64 drawerId)
{
    if (!require(perm::Manager, tr("Counting another drawer")))
        return false;
    for (DrawerSession &d : s_->drawers) {
        if (d.id == drawerId && d.open())
            return closeDrawer(d);
    }
    return fail(tr("That drawer is not open."));
}

bool PosService::closeDrawer(DrawerSession &d)
{
    if (entry_.isEmpty())
        return fail(d.employeeId.empty() ? tr("Count the cash, enter the amount, then Count Drawer.")
                                         : tr("Count the cash, enter the amount, then Check Out."));
    d.counted = Money::fromCents(entry_.toLongLong());
    d.countedPieces = std::exchange(countedPieces_, {});
    d.blind = blindCount();
    d.expected = expectedCash(d, s_->closedToday, s_->settings.tax);
    d.closedAt = now();
    d.closedBy = user()->name;
    entry_.clear();
    emit entryChanged();
    if (s_->sink)
        s_->sink->saveDrawer(d);
    const Money diff = d.overShort();
    if (s_->printer)
        s_->printer->printReport(s_->settings, drawerReport({d}, s_->closedToday, reportContext(tr("Drawer count"))),
                                 receiptPrinter());
    const QString who = qs(d.name);
    emit notice(d.blind ? tr("%1 counted: %2").arg(who, format(d.counted))
                : diff.cents() == 0 ? tr("%1 balanced").arg(who)
                : diff.cents() < 0 ? tr("%1 is short %2").arg(who, format(-diff))
                                   : tr("%1 is over %2").arg(who, format(diff)));
    emit s_->drawerChanged();
    emit s_->dayChanged();
    return true;
}

bool PosService::payout(CashMovement::Kind kind)
{
    if (kind == CashMovement::Kind::TipPayout)
        return cashOutTips();
    if (!require(perm::Manager, kind == CashMovement::Kind::PaidIn ? tr("Paying in") : tr("Paying out")))
        return false;
    const Money amount = Money::fromCents(entry_.toLongLong());
    if (amount.cents() <= 0)
        return fail(tr("Enter the amount on the keypad first."));
    // A pay out is an expense: what was it for?
    if (kind == CashMovement::Kind::Payout && expenseCategory_.isEmpty() && !s_->settings.expenseCategories.empty()) {
        QStringList names;
        for (const std::string &c : s_->settings.expenseCategories)
            names << qs(c);
        return fail(tr("Choose what it was for: %1.").arg(names.join(u", "_s)));
    }
    DrawerSession *d = serverBank() ? ensureMyBank() : myDrawer();   // a bank starts with the first cash
    if (!d)
        return fail(noDrawerMessage());
    CashMovement m;
    m.id = d->nextMovementId++;
    m.kind = kind;
    m.amount = amount;
    m.reason = ss(text_.trimmed());
    m.by = user()->name;
    m.at = now();
    if (kind == CashMovement::Kind::Payout)
        m.category = ss(expenseCategory_);
    d->movements.push_back(m);
    entry_.clear();
    text_.clear();
    expenseCategory_.clear();
    emit entryChanged();
    if (s_->sink)
        s_->sink->saveDrawer(*d);
    if (s_->printer && !serverBank() && terminalHasDrawer())
        s_->printer->openDrawer(s_->settings, receiptPrinter());
    emit notice(kind == CashMovement::Kind::PaidIn ? tr("Paid in %1").arg(format(amount))
                                                   : tr("Paid out %1").arg(format(amount)));
    emit s_->drawerChanged();
    emit s_->dayChanged();
    return true;
}

void PosService::setExpenseCategory(const QString &category)
{
    expenseCategory_ = category == expenseCategory_ ? QString() : category;
    emit drawerChanged();
}

bool PosService::cashOutTips()
{
    if (!require(perm::Order, tr("Cashing out tips")))
        return false;
    const Employee *e = user();
    const Money owed = tipShareFor(e->id).owed();
    if (owed.cents() <= 0)
        return fail(tr("No tips are owed to %1.").arg(qs(e->name)));
    // With server banks the tips come out of the server's own cash.
    DrawerSession *d = serverBank() ? ensureMyBank() : myDrawer();
    if (!d)
        return fail(noDrawerMessage());
    CashMovement m;
    m.id = d->nextMovementId++;
    m.kind = CashMovement::Kind::TipPayout;
    m.amount = owed;
    m.reason = e->name;
    m.by = e->name;
    m.employeeId = e->id;
    m.at = now();
    d->movements.push_back(m);
    if (s_->sink)
        s_->sink->saveDrawer(*d);
    if (s_->printer && !serverBank() && terminalHasDrawer())
        s_->printer->openDrawer(s_->settings, receiptPrinter());
    emit notice(tr("Paid %1 in tips to %2").arg(format(owed), qs(e->name)));
    emit s_->drawerChanged();
    emit s_->dayChanged();
    return true;
}

QString PosService::tipsOwed() const
{
    const Employee *e = user();
    if (!e)
        return {};
    return format(tipShareFor(e->id).owed());
}

std::map<std::string, TipShare> PosService::allTipShares() const
{
    // Hours worked today, for splitting the tip pools.
    std::map<std::string, double> hours;
    std::map<std::string, std::string> jobToday;   // someone who bartended today is in the bar's pool
    std::map<std::string, Money> cash;             // cash tips told at clock out
    for (const TimePunch &p : s_->punches) {
        hours[p.employeeId] += double(p.workedMs(now(), s_->settings.paidBreaks)) / 3'600'000.0;
        if (p.cashTipsDeclared)
            cash[p.employeeId] += p.cashTips;
        if (!p.job.empty())
            jobToday[p.employeeId] = p.job;
    }
    std::vector<Employee> staff = s_->employees;
    for (Employee &e : staff)
        if (const auto it = jobToday.find(e.id); it != jobToday.end())
            e.role = it->second;
    return core::tipShares(s_->closedToday, s_->drawers, s_->settings, staff, hours, cash);
}

TipShare PosService::tipShareFor(const std::string &employeeId) const
{
    const auto shares = allTipShares();
    const auto it = shares.find(employeeId);
    return it == shares.end() ? TipShare{} : it->second;
}

QVariantMap PosService::drawerInfo() const
{
    const QString mode = serverBank() ? u"serverBank"_s : u"drawer"_s;   // this person's
    const DrawerSession *latest = latestMyDrawer();
    // Managers see everyone else's open drawers and banks, to count them.
    QVariantList others;
    if (can(QString::fromLatin1(perm::Manager))) {
        for (const DrawerSession &o : s_->drawers) {
            if (o.open() && &o != latest)
                others.append(QVariantMap{{u"id"_s, qint64(o.id)}, {u"name"_s, qs(o.name)},
                                          {u"openedBy"_s, qs(o.openedBy)}, {u"expected"_s, format(expectedNow(o))}});
        }
    }
    // Counting it by bills and coins: what each is worth.
    QVariantList pieces;
    for (std::int64_t cents : s_->settings.cashDenominations)
        pieces.append(QVariantMap{{u"cents"_s, qint64(cents)}, {u"text"_s, format(Money::fromCents(cents))}});
    if (!latest) {
        return {{u"exists"_s, false}, {u"open"_s, false}, {u"mode"_s, mode}, {u"others"_s, others},
                {u"hasDrawer"_s, terminalHasDrawer()}, {u"denominations"_s, pieces},
                {u"name"_s, serverBank() ? (user() ? tr("%1's bank").arg(userName()) : tr("Bank"))
                                         : tr("%1 drawer").arg(terminal_)}};
    }
    const DrawerSession &d = *latest;
    Money cash;
    for (const Check &c : s_->closedToday) {
        if (c.drawerSession == d.id)
            cash += cashIntoDrawer(c, s_->settings.tax);
    }
    QVariantList movements;
    for (const CashMovement &m : d.movements) {
        const QString what = m.kind == CashMovement::Kind::PaidIn ? tr("Paid in")
                             : m.kind == CashMovement::Kind::TipPayout ? tr("Tips to %1").arg(qs(m.reason))
                             : m.category.empty() ? tr("Paid out") : tr("Paid out · %1").arg(qs(m.category));
        movements.append(QVariantMap{{u"what"_s, m.kind == CashMovement::Kind::TipPayout || m.reason.empty()
                                                    ? what : what + u": "_s + qs(m.reason)},
                                     {u"amount"_s, format(m.effect())}, {u"time"_s, clockText(m.at)}});
    }
    const Money expected = expectedNow(d);
    // Counted blind: what should be there stays hidden until it's counted,
    // and then whether it came out over or short.
    const bool blind = blindCount();
    QVariantList counted;
    for (const auto &[cents, n] : d.countedPieces)
        counted.append(QVariantMap{{u"text"_s, u"%1 × %2"_s.arg(n).arg(format(Money::fromCents(cents)))},
                                   {u"amount"_s, format(Money::fromCents(cents * n))}});
    return {
        {u"blind"_s, blind && d.open()}, {u"hideResult"_s, blind && !d.open()},
        // Why it can't be checked out yet (open checks), before counting it.
        {u"countBlocked"_s, [&] {
             if (!d.open() || d.employeeId.empty() || !user() || d.employeeId != user()->id || !mustCloseChecksToCheckOut())
                 return QString();
             const auto mine = std::ranges::count_if(s_->open, [&](const auto &e) { return e.second.serverId == user()->id; });
             return mine > 0 ? tr("Close or hand over your open checks before checking out.") : QString();
         }()},
        {u"denominations"_s, pieces}, {u"countedPieces"_s, counted},
        {u"exists"_s, true}, {u"open"_s, d.open()}, {u"name"_s, qs(d.name)},
        {u"mode"_s, mode}, {u"others"_s, others}, {u"hasDrawer"_s, terminalHasDrawer()},
        {u"openedBy"_s, qs(d.openedBy)}, {u"opened"_s, clockText(d.openedAt)},
        {u"startingCash"_s, format(d.startingCash)}, {u"cashSales"_s, blind ? QString() : format(cash)},
        {u"movements"_s, movements},
        {u"expected"_s, blind ? QString() : format(expected)}, {u"counted"_s, format(d.counted)},
        {u"closedBy"_s, qs(d.closedBy)}, {u"overShort"_s, blind ? QString() : format(d.overShort())},
        {u"categories"_s, [&] {
             QVariantList out;
             for (const std::string &c : s_->settings.expenseCategories)
                 out.append(QVariantMap{{u"name"_s, qs(c)}, {u"chosen"_s, qs(c) == expenseCategory_}});
             return out;
         }()},
        {u"overShortCents"_s, blind ? qint64(0) : qint64(d.overShort().cents())},
    };
}

// --- tips and gratuity ----------------------------------------------------------------

bool PosService::addTip(std::int64_t percentBp)
{
    if (!require(perm::Settle, tr("Adding tips")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    // The selected card payment, else the most recent card payment.
    Payment *target = nullptr;
    for (Payment &p : c->payments) {
        if (p.kind == TenderKind::Card && (!target || p.id == selectedPayment_ || target->id != selectedPayment_))
            target = &p;
    }
    if (!target)
        return fail(tr("Tips go on a card payment. Add one first."));
    const Totals t = c->totals(s_->settings.tax);
    Money tip;
    if (percentBp > 0) {
        tip = (t.subtotal + t.tax).percent(percentBp);   // on the check before gratuity
    } else {
        if (entry_.isEmpty())
            return fail(tr("Type the tip on the keypad, then Tip."));
        tip = Money::fromCents(entry_.toLongLong());
        entry_.clear();
        emit entryChanged();
    }
    target->tip = tip;
    emit notice(tip.cents() ? tr("Tip %1 on %2").arg(format(tip), qs(target->tenderName)) : tr("Tip removed"));
    changed(*c);
    return true;
}

Money PosService::tipFor(const Check &c) const
{
    if (tipChoice_.none)
        return {};
    if (tipChoice_.percentBp > 0) {
        const Totals t = c.totals(s_->settings.tax);
        return (t.subtotal + t.tax).percent(tipChoice_.percentBp);
    }
    return tipChoice_.amount;
}

bool PosService::askForTip()
{
    if (!require(perm::Settle, tr("Asking for a tip")))
        return false;
    const Check *c = currentCheck();
    if (!c)
        return fail(tr("No check is open."));
    tipChoice_ = {true, false, false, 0, {}, c->id};
    emit notice(tr("The guest is choosing a tip"));
    emit checkChanged();
    return true;
}

bool PosService::customerTip(const QString &kind, std::int64_t value)
{
    Check *c = current();
    if (!c || !tipChoice_.asked || tipChoice_.checkId != c->id)
        return fail(tr("No tip was asked for."));
    tipChoice_.chosen = true;
    tipChoice_.none = kind == u"none";
    tipChoice_.percentBp = kind == u"percent" ? std::clamp<std::int64_t>(value, 0, 10000) : 0;
    tipChoice_.amount = kind == u"amount" ? Money::fromCents(std::clamp<std::int64_t>(value, 0, 1'000'000)) : Money();
    const Money tip = tipFor(*c);
    // On the card payment if there is one; else it waits for the card.
    Payment *card = nullptr;
    for (Payment &p : c->payments) {
        if (p.kind == TenderKind::Card)
            card = &p;
    }
    if (card) {
        card->tip = tip;
        changed(*c);
    }
    emit notice(tipChoice_.none ? tr("The guest chose no tip") : tr("The guest chose a %1 tip").arg(format(tip)));
    emit checkChanged();
    return true;
}

QVariantMap PosService::customerPrompt() const
{
    const Check *c = currentCheck();
    const bool mine = c && tipChoice_.asked && tipChoice_.checkId == c->id;
    QVariantList choices;
    if (mine) {
        const Totals t = c->totals(s_->settings.tax);
        for (int p : s_->settings.tipPercents)
            choices.append(QVariantMap{{u"percent"_s, p}, {u"amount"_s, format((t.subtotal + t.tax).percent(p * 100))}});
    }
    // Loyalty: who is on the check, their points, what this check earns.
    QVariantMap loyalty{{u"enabled"_s, s_->settings.loyaltyEnabled}};
    if (s_->settings.loyaltyEnabled && c) {
        const CustomerRecord *r = s_->customer(c->customerId);
        // On a screen the room can see: a first name, never the phone number.
        const std::string digits = r ? CustomerRecord::digits(r->phone) : std::string();
        loyalty.insert(u"member"_s, !r ? QString()
                                    : !r->name.empty() ? qs(r->name.substr(0, r->name.find(' ')))
                                    : tr("...%1").arg(qs(digits.size() > 4 ? digits.substr(digits.size() - 4) : digits)));
        loyalty.insert(u"named"_s, r && !r->name.empty());
        loyalty.insert(u"points"_s, r ? r->points : 0);
        loyalty.insert(u"earning"_s, pointsFor(*c));
        QVariantList rewards;
        for (int i = 0; i < int(s_->settings.rewards.size()); ++i) {
            const PosSettings::Reward &w = s_->settings.rewards[i];
            rewards.append(QVariantMap{{u"index"_s, i}, {u"points"_s, w.points}, {u"value"_s, format(w.value)},
                                       {u"ready"_s, r && r->points >= w.points}});
        }
        loyalty.insert(u"rewards"_s, rewards);
    }
    QVariantList slides;
    for (const std::string &sl : s_->settings.displaySlides)
        slides.append(qs(sl));
    for (const QVariant &p : promotionsNow())
        slides.append(tr("Now: %1").arg(p.toString()));
    return {
        {u"askingTip"_s, mine && !tipChoice_.chosen}, {u"tipChosen"_s, mine && tipChoice_.chosen},
        {u"tip"_s, mine && tipChoice_.chosen ? format(tipFor(*c)) : QString()},
        {u"choices"_s, choices}, {u"loyalty"_s, loyalty}, {u"slides"_s, slides},
        {u"logo"_s, qs(s_->settings.displayLogo)}, {u"accent"_s, qs(s_->settings.displayAccent)},
        {u"canText"_s, bool(s_->sendText) && !s_->settings.textWebhook.empty()},
        {u"label"_s, c ? qs(c->label) : QString()}, {u"checkId"_s, c ? qint64(c->id) : 0},
    };
}

bool PosService::setGratuity(std::int64_t percentBp)
{
    if (!require(perm::Order, tr("Changing gratuity")))
        return false;
    Check *c = current();
    if (!c)
        return fail(tr("No check is open."));
    if (percentBp < 0 || percentBp > 10000)
        return fail(tr("Gratuity is between 0 and 100%."));
    if (c->autoGratuity && percentBp < c->gratuityBp && !require(perm::Manager, tr("Lowering the party gratuity")))
        return false;
    c->gratuityBp = percentBp;
    c->autoGratuity = false;
    emit notice(percentBp ? tr("Gratuity %1%").arg(double(percentBp) / 100.0) : tr("Gratuity removed"));
    changed(*c);
    return true;
}

// --- business day --------------------------------------------------------------------

QVariantMap PosService::dayInfo() const
{
    QStringList blockers;
    if (!s_->open.empty())
        blockers << (s_->open.size() == 1 ? tr("1 check is still open")
                                       : tr("%1 checks are still open").arg(s_->open.size()));
    for (const DrawerSession &d : s_->drawers) {
        if (d.open())
            blockers << tr("%1 has not been counted").arg(qs(d.name));
    }
    Money net;
    for (const Check &c : s_->closedToday)
        net += c.totals(s_->settings.tax).subtotal;
    const bool anyOpen = std::ranges::any_of(s_->drawers, &DrawerSession::open);
    // Who's still on the clock: closing up, or they forgot (over 12 hours).
    QVariantList clockedIn;
    for (const TimePunch &p : s_->punches) {
        if (!p.open())
            continue;
        const Employee *e = s_->employee(p.employeeId);
        const QDate day = QDateTime::fromMSecsSinceEpoch(p.clockIn).date();
        const bool today = day == QDateTime::fromMSecsSinceEpoch(now()).date();
        clockedIn.append(QVariantMap{
            {u"id"_s, qint64(p.id)}, {u"name"_s, e ? qs(e->name) : qs(p.employeeId)},
            {u"since"_s, today ? clockText(p.clockIn) : i18n::locale().toString(day, u"ddd"_s) + u' ' + clockText(p.clockIn)},
            {u"overTwelve"_s, now() - p.clockIn > 12LL * 3'600'000}, {u"onBreak"_s, p.onBreak()},
        });
    }
    return {
        {u"clockedIn"_s, clockedIn},
        {u"closingLeft"_s, [&] {
             int left = 0;
             for (const QVariant &v : checklists().value(u"closing"_s).toList())
                 left += v.toMap().value(u"done"_s).toBool() ? 0 : 1;
             return left;
         }()},
        {u"id"_s, qint64(s_->day.id)}, {u"opened"_s, dayLabel(s_->day)},
        {u"openChecks"_s, int(s_->open.size())}, {u"closedChecks"_s, int(s_->closedToday.size())},
        {u"netSales"_s, format(net)}, {u"drawerOpen"_s, anyOpen},
        {u"blockers"_s, blockers}, {u"ready"_s, blockers.isEmpty()},
        {u"backup"_s, s_->backup},
    };
}

bool PosService::clockOutPunch(qint64 punchId)
{
    if (!require(perm::Manager, tr("Clocking someone out")))
        return false;
    for (TimePunch &p : s_->punches) {
        if (p.id != punchId || !p.open())
            continue;
        p.clockOut = now();
        if (p.onBreak())
            p.breaks.back().end = p.clockOut;
        if (s_->sink)
            s_->sink->savePunch(p);
        const Employee *e = s_->employee(p.employeeId);
        logPunchChange(p, tr("clocked out at %1").arg(clockText(p.clockOut)), tr("End of Day: still clocked in"));
        emit notice(tr("%1 clocked out").arg(e ? qs(e->name) : qs(p.employeeId)));
        emit sessionChanged();
        emit s_->dayChanged();
        emit s_->staffChanged();
        return true;
    }
    return fail(tr("They're not clocked in anymore."));
}

QVariantList PosService::days() const
{
    QVariantList out{QVariantMap{{u"id"_s, qint64(0)}, {u"label"_s, tr("Today (since %1)").arg(clockText(s_->day.openedAt))}}};
    for (const PastDay &p : s_->pastDays)
        out.append(QVariantMap{{u"id"_s, qint64(p.day.id)}, {u"label"_s, dayLabel(p.day)}});
    return out;
}

bool PosService::endOfDay()
{
    if (!require(perm::Manager, tr("End of day")))
        return false;
    // Practice checks left open don't hold up the day.
    for (auto it = s_->open.begin(); it != s_->open.end();) {
        if (it->second.training && !s_->lockedBy.contains(it->first)) {
            it->second.status = CheckStatus::Discarded;
            it->second.closedAt = now();
            if (s_->sink)
                s_->sink->saveCheck(it->second);
            it = s_->open.erase(it);
        } else {
            ++it;
        }
    }
    // 86'd for today: back on the menu.
    bool back = false;
    if (s_->settings.soldOutBackNextDay)
        for (int i = 0; i < int(s_->menu.size()); ++i) {
            MenuItem &m = s_->menu[i];
            if (!m.soldOutToday)
                continue;
            m.soldOutToday = false;
            m.available = true;
            back = true;
            if (s_->sink)
                s_->sink->saveMenuItem(m, i);
        }
    if (back) {
        ++s_->adminRevision;
        emit s_->adminChanged();
    }
    // Event tickets sold today stay sold (their checks leave closedToday).
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (m.ticketCapacity <= 0)
            continue;
        int today = 0;
        for (const Check &c : s_->closedToday)
            if (!c.training)
                for (const OrderLine &l : c.lines)
                    if (!l.voided && l.itemId == m.id)
                        today += l.counted();
        if (today > 0) {
            m.ticketsSoldBefore += today;
            if (s_->sink)
                s_->sink->saveMenuItem(m, i);
        }
    }
    // Orders for another day wait for their day.
    const auto stillOpen = std::ranges::count_if(s_->open, [this](const auto &kv) { return !forAnotherDay(kv.second); });
    if (stillOpen > 0)
        return fail(stillOpen == 1 ? tr("Settle the open check first.")
                                   : tr("Settle the %1 open checks first.").arg(stillOpen));
    for (const DrawerSession &d : s_->drawers) {
        if (d.open())
            return fail(tr("Count %1 first.").arg(qs(d.name)));
    }

    QJsonObject reports;
    for (const QString &id : kReportIds)
        reports.insert(id, toJson(buildReport(id)));
    s_->day.closedAt = now();
    if (s_->sink)
        s_->sink->saveDay(s_->day, reports);
    s_->pastDays.insert(s_->pastDays.begin(), PastDay{s_->day, reports});
    if (s_->printer)
        s_->printer->printReport(s_->settings, buildReport(u"sales"_s), receiptPrinter());

    s_->closedToday.clear();
    s_->refundsToday.clear();
    // Finished punches stay a week, for weekly overtime.
    for (const TimePunch &p : s_->punches) {
        if (!p.open())
            s_->earlierPunches.push_back(p);
    }
    std::erase_if(s_->punches, [](const TimePunch &p) { return !p.open(); });
    std::erase_if(s_->earlierPunches, [this](const TimePunch &p) { return p.clockIn < now() - 8LL * 24 * 3'600'000; });
    s_->drawers.clear();
    lastClosedId_ = 0;
    const std::int64_t closedId = s_->day.id;
    s_->startDay();
    emit notice(tr("Day %1 closed. A new day has started.").arg(closedId));
    emit s_->drawerChanged();
    emit s_->dayChanged();
    return true;
}

// --- reports ---------------------------------------------------------------------------

Report PosService::buildReport(const QString &id) const
{
    const ReportContext ctx = reportContext(tr("Today, since %1").arg(clockText(s_->day.openedAt)));
    if (id == u"items")
        return itemSales(s_->closedToday, s_->menu, ctx);
    if (id == u"checklists")
        return checklistReport(ctx);
    if (id == u"servers")
        return serverSales(s_->closedToday, ctx);
    if (id == u"drivers")
        return driverReport(s_->closedToday, ctx);
    if (id == u"labor") {
        Money net;
        for (const Check &c : s_->closedToday)
            if (!c.training)
                net += c.totals(s_->settings.tax).subtotal;
        return laborReport(s_->punches, s_->employees, ctx, s_->earlierPunches, net);
    }
    if (id == u"drawer")
        return drawerReport(s_->drawers, s_->closedToday, ctx);
    if (id == u"expenses")
        return expensesReport(s_->drawers, ctx);
    if (id == u"purchases")
        return purchasesReport(ctx);
    if (id == u"tips")
        return tipsReport(allTipShares(), ctx);
    if (id == u"hourly")
        return hourlySales(s_->closedToday, ctx);
    if (id == u"categories")
        return categorySales(s_->closedToday, s_->menu, ctx);
    if (id == u"foodcost")
        return foodCostReport(s_->closedToday, ctx);
    if (id == u"engineering")
        return menuEngineeringReport(s_->closedToday, ctx);
    if (id == u"turns")
        return tableTurns(s_->closedToday, ctx);
    if (id == u"customers")
        return customersReport(s_->closedToday, ctx);
    if (id == u"royalty" || id == u"accounting")
        return rangeCapableReport(id, s_->closedToday, ctx);
    if (id == u"kitchen") {
        std::vector<const Check *> checks;
        for (const Check &c : s_->closedToday)
            checks.push_back(&c);
        for (const auto &[cid, c] : s_->open)
            if (!c.training)
                checks.push_back(&c);
        return kitchenReport(checks, s_->settings.kitchenLateMinutes, ctx);
    }
    if (id == u"accounts")
        return accountsReport(s_->giftCards, s_->customers, s_->day.openedAt, ctx);
    if (id == u"audit" || id == u"exceptions") {
        std::vector<const Check *> checks;
        for (const Check &c : s_->closedToday)
            checks.push_back(&c);
        for (const auto &[id, c] : s_->open)
            if (!c.training)
                checks.push_back(&c);
        return id == u"audit" ? auditReport(checks, ctx) : exceptionsReport(checks, s_->drawers, ctx);
    }
    if (id == u"deposit") {
        std::vector<const Check *> open;
        for (const auto &[cid, c] : s_->open)
            if (!c.training)
                open.push_back(&c);
        return depositReport(s_->drawers, s_->closedToday, open, ctx, s_->refundsToday);
    }
    return salesSummary(s_->closedToday, ctx, &s_->refundsToday);
}

// --- reports over a range ---------------------------------------------------------

namespace {
const QStringList kRangeReports = {u"sales"_s, u"items"_s, u"categories"_s, u"hourly"_s, u"servers"_s,
                                   u"kitchen"_s, u"audit"_s, u"foodcost"_s, u"engineering"_s, u"turns"_s, u"exceptions"_s, u"customers"_s,
                                   u"royalty"_s, u"accounting"_s};
} // namespace

Report PosService::rangeCapableReport(const QString &id, const std::vector<Check> &closed, const ReportContext &ctx,
                                      const std::vector<Refund> *refunds) const
{
    std::vector<const Check *> ptrs;
    for (const Check &c : closed)
        ptrs.push_back(&c);
    if (id == u"items")
        return itemSales(closed, s_->menu, ctx);
    if (id == u"categories")
        return categorySales(closed, s_->menu, ctx);
    if (id == u"hourly")
        return hourlySales(closed, ctx);
    if (id == u"servers")
        return serverSales(closed, ctx);
    if (id == u"drivers")
        return driverReport(closed, ctx);
    if (id == u"kitchen")
        return kitchenReport(ptrs, s_->settings.kitchenLateMinutes, ctx);
    if (id == u"audit")
        return auditReport(ptrs, ctx);
    if (id == u"exceptions")   // no-sales are kept with today's drawers only
        return exceptionsReport(ptrs, {}, ctx);
    if (id == u"foodcost")
        return foodCostReport(closed, ctx);
    if (id == u"engineering")
        return menuEngineeringReport(closed, ctx);
    if (id == u"turns")
        return tableTurns(closed, ctx);
    if (id == u"customers")
        return customersReport(closed, ctx);
    if (id == u"royalty")
        return royaltyReport(closed, ctx);
    if (id == u"accounting") {
        std::map<std::string, std::string> familyOf;
        for (const MenuItem &m : s_->menu)
            familyOf[m.id] = m.family;
        return accountingReport(closed, familyOf, ctx);
    }
    return salesSummary(closed, ctx, refunds);
}

bool PosService::requestRangeReport(const QString &id, const QString &period, const QString &fromText,
                                    const QString &toText, bool compare)
{
    if (!require(perm::Manager, tr("Reports")))
        return false;
    const QDate today = QDateTime::fromMSecsSinceEpoch(now()).date();
    QDate from, to = today;
    if (period == u"week") {
        from = today.addDays(-((today.dayOfWeek() % 7 - s_->settings.weekStartsOn + 7) % 7));
    } else if (period == u"lastWeek") {
        to = today.addDays(-((today.dayOfWeek() % 7 - s_->settings.weekStartsOn + 7) % 7) - 1);
        from = to.addDays(-6);
    } else if (period == u"month") {
        from = QDate(today.year(), today.month(), 1);
    } else if (period == u"lastMonth") {
        from = QDate(today.year(), today.month(), 1).addMonths(-1);
        to = QDate(today.year(), today.month(), 1).addDays(-1);
    } else if (period == u"year") {
        from = QDate(today.year(), 1, 1);
    } else {
        from = QDate::fromString(fromText.trimmed(), u"yyyy-MM-dd"_s);
        to = QDate::fromString(toText.trimmed(), u"yyyy-MM-dd"_s);
        if (!from.isValid() || !to.isValid())
            return fail(tr("Dates are written like 2026-09-01."));
        if (to < from)
            std::swap(from, to);
        if (from.daysTo(to) > 366)
            return fail(tr("Choose at most a year at a time."));
    }
    const auto startOf = [](QDate d) { return QDateTime(d, QTime(0, 0)).toMSecsSinceEpoch(); };
    const QString label = from == to ? i18n::locale().toString(from, u"ddd MMM d, yyyy"_s)
                                     : tr("%1 - %2").arg(i18n::locale().toString(from, u"MMM d"_s),
                                                         i18n::locale().toString(to, u"MMM d, yyyy"_s));
    const QString beforeLabel = from.addYears(-1).year() == to.addYears(-1).year()
                                    ? QString::number(from.year() - 1) : tr("A year before");
    const std::int64_t a = startOf(from), b = startOf(to.addDays(1));
    const std::int64_t a0 = startOf(from.addYears(-1)), b0 = startOf(to.addDays(1).addYears(-1));

    const int request = ++rangeRequest_;
    rangeReport_ = {{u"loading"_s, true}, {u"id"_s, id}, {u"period"_s, period}, {u"label"_s, label}};
    emit sessionChanged();

    // Read the checks away from the screen, then build here.
    auto history = s_->history;
    auto refundHistory = s_->refundHistory;
    std::vector<Check> today_ = s_->closedToday;
    std::vector<Refund> refundsToday = s_->refundsToday;
    QPointer<PosService> self(this);
    QThreadPool::globalInstance()->start([=, today_ = std::move(today_), refundsToday = std::move(refundsToday)]() mutable {
        std::vector<Check> now = history ? history(a, b) : std::vector<Check>{};
        std::vector<Check> before = history && compare ? history(a0, b0) : std::vector<Check>{};
        // Refunds by when they were made (today's may not be written yet).
        std::vector<Refund> refundsNow = refundHistory ? refundHistory(a, b) : std::vector<Refund>{};
        std::vector<Refund> refundsBefore = refundHistory && compare ? refundHistory(a0, b0) : std::vector<Refund>{};
        for (const Refund &r : refundsToday)
            if (r.at >= a && r.at < b && std::ranges::none_of(refundsNow, [&](const Refund &x) { return x.id == r.id; }))
                refundsNow.push_back(r);
        // Today's closed checks may not be written yet: the ones in memory count.
        for (Check &c : today_) {
            if (c.closedAt >= a && c.closedAt < b
                && std::ranges::none_of(now, [&](const Check &x) { return x.id == c.id; }))
                now.push_back(std::move(c));
        }
        const bool byDate = bool(refundHistory) || !refundsNow.empty();
        QMetaObject::invokeMethod(self, [self, request, id, label, beforeLabel, compare, now = std::move(now),
                                         before = std::move(before), refundsNow = std::move(refundsNow),
                                         refundsBefore = std::move(refundsBefore), byDate] {
            if (!self || request != self->rangeRequest_)
                return;   // a newer request replaced it
            const ReportContext ctx = self->reportContext(label);
            Report r = self->rangeCapableReport(kRangeReports.contains(id) ? id : u"sales"_s, now, ctx,
                                                byDate ? &refundsNow : nullptr);
            if (!kRangeReports.contains(id))
                r.note(tr("That report is one day at a time (pick Day). Showing sales.").toStdString());
            if (compare) {
                const ReportContext was = self->reportContext(beforeLabel);
                r = compareReports(r, self->rangeCapableReport(kRangeReports.contains(id) ? id : u"sales"_s, before, was,
                                                               byDate ? &refundsBefore : nullptr),
                                   beforeLabel.toStdString());
            }
            QVariantMap out = self->rangeReport_;
            out.insert(u"loading"_s, false);
            out.insert(u"report"_s, toVariant(r));
            out.insert(u"checks"_s, int(now.size()));
            self->rangeReport_ = out;
            emit self->sessionChanged();
        }, Qt::QueuedConnection);
    });
    return true;
}

QVariantMap PosService::reportVariant(const Report &r) const
{
    return toVariant(r);
}

QVariantMap PosService::report(const QString &id, qint64 dayId)
{
    if (dayId == 0 || dayId == s_->day.id)
        return toVariant(buildReport(id));
    for (const PastDay &p : s_->pastDays) {
        if (p.day.id == dayId) {
            const QJsonObject stored = p.reports.value(id).toObject();
            if (stored.isEmpty())
                return {{u"title"_s, tr("No report")}, {u"rows"_s, QVariantList()}};
            return toVariant(reportFromJson(stored));
        }
    }
    return {{u"title"_s, tr("No such day")}, {u"rows"_s, QVariantList()}};
}

bool PosService::printReport(const QString &id, qint64 dayId)
{
    if (!require(perm::Manager, tr("Printing reports")))
        return false;
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    Report r;
    if (id == u"forecast") {   // the one last shown
        if (forecast_.rows.empty())
            return fail(tr("Show the forecast first."));
        r = forecast_;
    } else if (dayId == 0 || dayId == s_->day.id) {
        r = buildReport(id);
    } else {
        for (const PastDay &p : s_->pastDays) {
            if (p.day.id == dayId)
                r = reportFromJson(p.reports.value(id).toObject());
        }
    }
    s_->printer->printReport(s_->settings, r, receiptPrinter());
    emit notice(tr("Printing %1").arg(qs(r.title)));
    return true;
}

} // namespace vt::app
