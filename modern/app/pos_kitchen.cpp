// PosService: the kitchen screens and the expediter: tickets by station, bumping,
// recalling.

#include "app/pos_service.hh"

#include "app/i18n.hh"
#include "app/pos_json.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>
#include <set>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {
// What the kitchen must know on a ticket: the table's allergies, and each
// guest's with items on it ("S2: Peanuts").
QStringList ticketAllergies(const Check &c, const std::vector<const OrderLine *> &lines)
{
    QStringList out = PosSession::allergenNames(c.allergies);
    for (const auto &[seat, list] : c.seatAllergies)
        if (std::ranges::any_of(lines, [seat](const OrderLine *l) { return l->seat == seat; }))
            out << PosSession::tr("S%1: %2").arg(seat).arg(PosSession::allergenNames(list).join(u", "_s));
    return out;
}
} // namespace

// --- kitchen display -----------------------------------------------------------------------

QVariantList PosService::kitchenTicketsNow() const
{
    // A ticket is everything sent in one go from one check that the kitchen
    // has not bumped yet. Closed checks count too (pay-first counters).
    struct Ticket { const Check *check; std::int64_t sentAt; std::vector<const OrderLine *> lines; };
    std::vector<Ticket> tickets;
    const std::int64_t t0 = now();
    auto collect = [&](const Check &c) {
        // A send with something still to make (or a void the kitchen hasn't
        // seen: crossed out until bumped); what's made there stays, dimmed.
        std::map<std::int64_t, std::vector<const OrderLine *>> bySend;
        std::set<std::int64_t> open;
        for (const OrderLine &l : c.lines) {
            if (!l.sent || !(l.isComment() || l.forKitchen()))
                continue;
            const bool partsLeft = std::ranges::any_of(l.modifiers, [&](const Modifier &m) { return isPart(l, m) && !m.made; });
            const bool freshVoid = l.voided && !l.made && t0 - l.sentAt < 3 * 3'600'000;
            if (l.voided && !freshVoid)
                continue;
            bySend[l.sentAt].push_back(&l);
            if ((!l.made || partsLeft) && (!l.isComment() || l.voided))
                open.insert(l.sentAt);
        }
        bool any = false;
        for (auto &[sentAt, lines] : bySend)
            if (open.contains(sentAt)) {
                tickets.push_back({&c, sentAt, std::move(lines)});
                any = true;
            }
        return any;
    };
    for (const auto &[id, c] : s_->open)
        if (!c.training)   // practice never reaches the kitchen
            collect(c);
    for (const Check &c : s_->closedToday)
        if (!s_->kitchenDone.contains(c.id) && !collect(c))
            s_->kitchenDone.insert(c.id);
    // Rush orders first, then the oldest.
    std::ranges::sort(tickets, [](const Ticket &a, const Ticket &b) {
        return a.check->rush != b.check->rush ? a.check->rush : a.sentAt < b.sentAt;
    });

    QVariantList out;
    for (const Ticket &t : tickets) {
        QVariantList lines;
        for (const OrderLine *l : t.lines) {
            QStringList mods;
            for (const Modifier &m : l->modifiers) {
                if (!m.kitchenHide && !isPart(*l, m))
                    mods << qs(m.kitchenText());
            }
            const bool partsLeft = std::ranges::any_of(l->modifiers, [&](const Modifier &m) { return isPart(*l, m) && !m.made; });
            if (!l->made || partsLeft || !l->isComment())
                lines.append(QVariantMap{{u"name"_s, qs(l->isComment() ? l->name : l->kitchenText())},
                                         {u"id"_s, qint64(l->id)}, {u"item"_s, qs(l->itemId)},
                                         {u"made"_s, l->made}, {u"voided"_s, l->voided},
                                         {u"color"_s, qs(l->kitchenColor)}, {u"quantity"_s, l->quantity},
                                         {u"contains"_s, allergenNames(allergyHits(*t.check, *l))},
                                         {u"modifiers"_s, mods}, {u"comment"_s, l->isComment()}, {u"note"_s, qs(l->note)},
                                         {u"printer"_s, qs(l->printerOf())}, {u"station"_s, qs(stationOf(*l))},
                                         {u"seat"_s, l->seat}, {u"course"_s, l->course}});
            // Its parts made at other stations (a combo's fries): lines of their own there.
            for (const Modifier &m : l->modifiers) {
                if (isPart(*l, m) && !m.made)
                    lines.append(QVariantMap{{u"name"_s, qs(m.kitchenText())}, {u"color"_s, QString()},
                                             {u"quantity"_s, l->quantity},
                                             {u"modifiers"_s, QStringList{tr("with %1").arg(qs(l->kitchenText()))}},
                                             {u"comment"_s, false}, {u"part"_s, true},
                                             {u"printer"_s, qs(l->printerOf())}, {u"station"_s, qs(m.station)},
                                             {u"seat"_s, l->seat}, {u"course"_s, l->course}});
            }
        }
        // Late past its slowest item's time (set, or learned) plus two minutes;
        // without any, the store's.
        int target = 0;
        for (const OrderLine &l : t.check->lines)
            if (l.sent && !l.voided && l.sentAt == t.sentAt && !l.isComment())
                target = std::max(target, prepMinutesFor(l.itemId));
        const int late = target > 0 ? target + 2 : s_->settings.kitchenLateMinutes;
        const int warn = target > 0 ? std::max(1, target * 3 / 4) : s_->settings.kitchenWarnMinutes;
        // The check's next course, on hold: on its latest ticket, to see coming.
        QVariantList held;
        if (std::ranges::none_of(tickets, [&](const Ticket &o) { return o.check == t.check && o.sentAt > t.sentAt; }))
            for (const OrderLine &l : t.check->lines)
                if (t.check->held(l) && l.forKitchen() && !l.isComment())
                    held.append(QVariantMap{{u"name"_s, qs(l.kitchenText())}, {u"quantity"_s, l.quantity},
                                            {u"course"_s, l.course}, {u"seat"_s, l.seat},
                                            {u"printer"_s, qs(l.printerOf())}, {u"station"_s, qs(stationOf(l))}});
        out.append(QVariantMap{
            {u"held"_s, held},
            {u"firesAt"_s, t.check->fireAt && !held.isEmpty() ? i18n::timeText(QDateTime::fromMSecsSinceEpoch(t.check->fireAt).time()) : QString()},
            {u"checkId"_s, qint64(t.check->id)}, {u"sentAt"_s, qint64(t.sentAt)},
            {u"label"_s, qs(t.check->label)}, {u"server"_s, qs(t.check->serverName)},
            {u"type"_s, qs(toString(t.check->type))}, {u"customer"_s, qs(t.check->customer.name)},
            {u"note"_s, qs(t.check->customer.note)}, {u"lines"_s, lines},
            {u"rush"_s, t.check->rush}, {u"vip"_s, t.check->vip},
            {u"allergies"_s, ticketAllergies(*t.check, t.lines)},
            {u"due"_s, t.check->dueAt ? dueText(t.check->dueAt) : QString()},
            {u"warnMinutes"_s, warn}, {u"lateMinutes"_s, late}, {u"targetMinutes"_s, target},
        });
    }
    return out;
}

namespace {
// The check with this id, open or closed today.
Check *findAnyCheck(PosShared *s, std::int64_t id)
{
    if (auto it = s->open.find(id); it != s->open.end())
        return &it->second;
    for (Check &c : s->closedToday) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}
} // namespace

// Stations count only while they're on the store's list: with none, the
// kitchen is one screen and an item's parts stay with it.
bool PosService::knownStation(const std::string &id) const
{
    return !id.empty() && std::ranges::any_of(s_->settings.stations, [&](const Station &x) { return x.id == id; });
}

std::string PosService::stationOf(const OrderLine &l) const
{
    return knownStation(l.station) ? l.station : l.printerOf();
}

// A modifier made at another station than its line (a combo's fries at the fryer).
bool PosService::isPart(const OrderLine &l, const Modifier &m) const
{
    return knownStation(m.station) && m.station != stationOf(l) && !m.kitchenHide && m.qualifier != Qualifier::No;
}

// The line and its parts at other stations are all made.
bool PosService::allMade(const OrderLine &l) const
{
    return l.made && std::ranges::all_of(l.modifiers, [&](const Modifier &m) { return !isPart(l, m) || m.made; });
}

// A screen for `station` (a station id, a printer id, or "" for all) makes this line / this part.
bool PosService::atStation(const OrderLine &l, const std::string &station) const
{
    return station.empty() || stationOf(l) == station || l.printerOf() == station;
}

bool PosService::partAt(const OrderLine &l, const Modifier &m, const std::string &station) const
{
    return isPart(l, m) && (station.empty() || m.station == station || l.printerOf() == station);
}

int PosService::prepMinutesFor(const std::string &itemId) const
{
    if (const MenuItem *m = findItem(qs(itemId)); m && m->prepMinutes > 0)
        return m->prepMinutes;
    const auto it = s_->settings.prepSeconds.find(itemId);
    return it == s_->settings.prepSeconds.end() ? 0 : std::max(1, int((it->second + 59) / 60));
}

bool PosService::bumpTicket(qint64 checkId, qint64 sentAt, const QString &station)
{
    Check *c = findAnyCheck(s_, checkId);
    if (!c)
        return fail(tr("That ticket is gone."));
    const std::string where = ss(station);
    int n = 0;
    bool learned = false;
    for (OrderLine &l : c->lines) {
        if (!l.sent || l.sentAt != sentAt)
            continue;
        if (l.voided) {   // the kitchen saw it crossed out
            if (!l.made && atStation(l, where)) {
                l.made = true;
                l.madeAt = now();
                ++n;
            }
            continue;
        }
        if (!l.made && atStation(l, where)) {
            l.made = true;
            l.madeAt = now();
            ++n;
            // Learn what it usually takes (ignoring the odd forgotten ticket).
            const std::int64_t took = (l.madeAt - l.sentAt) / 1000;
            if (!l.isComment() && took > 0 && took < 90 * 60 && !c->training) {
                int &avg = s_->settings.prepSeconds[l.itemId];
                avg = avg == 0 ? int(took) : int(std::lround(0.8 * avg + 0.2 * double(took)));
                // And the station's pace: this long with that much ahead of it.
                if (l.queueAtSend >= 0 && !stationOf(l).empty())
                    s_->settings.stationPace[stationOf(l)].learn(l.queueAtSend, double(took) / 60.0);
                learned = true;
            }
        }
        for (Modifier &m : l.modifiers) {
            if (!m.made && partAt(l, m, where)) {
                m.made = true;
                m.madeAt = now();
                ++n;
            }
        }
    }
    if (n == 0)
        return fail(tr("That ticket was already bumped."));
    if (learned)
        s_->savePrepTimes();
    s_->bumped.push_back({checkId, sentAt, where});
    if (s_->sink)
        s_->sink->saveCheck(*c);
    tellFoodReady(*c, sentAt);
    emit s_->checksChanged();
    // A food truck's order, all made: the window calls it.
    if (c->orderNumber > 0 && !c->readyAt && std::ranges::none_of(c->lines, [](const OrderLine &l) {
            return l.sent && !l.voided && !l.isComment() && l.forKitchen() && !l.made;
        }))
        callOrder(*c);
    return true;
}

// Everything sent in one go is made: its server hears "T5: food is up" on
// their screens (a food truck's window calls its number instead).
void PosService::tellFoodReady(const Check &c, std::int64_t sentAt)
{
    if (!s_->settings.foodReadyNotice || c.training || c.orderNumber > 0 || c.serverName.empty())
        return;
    QStringList items;
    int count = 0;
    for (const OrderLine &l : c.lines) {
        if (!l.sent || l.voided || l.sentAt != sentAt || l.isComment() || !l.forKitchen())
            continue;
        if (!allMade(l))
            return;   // not all of it yet
        ++count;
        if (items.size() < 3)
            items << (l.quantity > 1 ? u"%1 %2"_s.arg(l.quantity).arg(qs(l.kitchenText())) : qs(l.kitchenText()));
    }
    if (count == 0)
        return;
    for (const PosShared::Message &m : s_->messages)   // told already (a recall, bumped again)
        if (m.kind == "ready" && m.to == c.serverName && m.text.starts_with(c.label + ":") && now() - m.at < 60'000)
            return;
    QString what = items.join(u", "_s);
    if (count > items.size())
        what += u" "_s + tr("+%1 more").arg(count - items.size());
    PosShared::Message m{++s_->lastMessageId, now(), ss(tr("Kitchen")), c.serverName,
                         ss(tr("%1: food is up (%2)").arg(qs(c.label), what)), "ready"};
    s_->messages.push_back(m);
    if (s_->messages.size() > 50)
        s_->messages.erase(s_->messages.begin());
    emit s_->dayChanged();   // every screen looks
}

// One item on a ticket made (or not after all); the ticket goes when it's all made.
bool PosService::bumpLine(qint64 checkId, qint64 lineId, bool made)
{
    Check *c = findAnyCheck(s_, checkId);
    if (!c)
        return fail(tr("That ticket is gone."));
    OrderLine *l = c->line(lineId);
    if (!l || !l->sent)
        return fail(tr("That item is gone."));
    if (l->made == made)
        return true;
    l->made = made;
    l->madeAt = made ? now() : 0;
    if (made) {
        // Learn what it takes, as a whole ticket's bump does.
        const std::int64_t took = (l->madeAt - l->sentAt) / 1000;
        if (!l->isComment() && !l->voided && took > 0 && took < 90 * 60 && !c->training) {
            int &avg = s_->settings.prepSeconds[l->itemId];
            avg = avg == 0 ? int(took) : int(std::lround(0.8 * avg + 0.2 * double(took)));
            s_->savePrepTimes();
        }
        // The last one: the ticket's done (Recall brings it back).
        const bool done = std::ranges::none_of(c->lines, [&](const OrderLine &x) {
            return x.sent && x.sentAt == l->sentAt && !x.made && (x.forKitchen() || x.isComment()) && !x.isComment();
        });
        if (done) {
            for (OrderLine &x : c->lines)   // its notes go with it
                if (x.sent && x.sentAt == l->sentAt && x.isComment() && !x.made) {
                    x.made = true;
                    x.madeAt = now();
                }
            s_->bumped.push_back({checkId, l->sentAt, {}});
        }
    } else {
        s_->kitchenAgain(c->id);
    }
    if (s_->sink)
        s_->sink->saveCheck(*c);
    if (made)
        tellFoodReady(*c, l->sentAt);
    emit s_->checksChanged();
    return true;
}

// The kitchen's out of it: sold out (86) for everyone, and the floor told.
bool PosService::kitchenSoldOut(const QString &itemId)
{
    for (int i = 0; i < int(s_->menu.size()); ++i) {
        MenuItem &m = s_->menu[i];
        if (qs(m.id) != itemId)
            continue;
        if (!m.available)
            return true;
        m.available = false;
        m.autoSoldOut = false;
        m.soldOutToday = true;
        if (s_->sink)
            s_->sink->saveMenuItem(m, i);
        ++s_->adminRevision;
        emit s_->menuChanged();
        PosShared::Message msg{++s_->lastMessageId, now(), ss(tr("Kitchen")), "floor", ss(tr("86: %1").arg(qs(m.name)))};
        s_->messages.push_back(msg);
        if (s_->messages.size() > 50)
            s_->messages.erase(s_->messages.begin());
        emit s_->dayChanged();
        emit notice(tr("%1 is sold out (86)").arg(qs(m.name)));
        return true;
    }
    return fail(tr("'%1' is not on the menu.").arg(itemId));
}

// --- the expediter -------------------------------------------------------------------------

bool PosService::expoSkips(const OrderLine &l) const
{
    if (s_->settings.expoSkips.empty())
        return false;
    // Its station and its printer, by id or by name.
    QStringList where;
    for (const std::string &id : {stationOf(l), l.printerOf()}) {
        where << qs(id) << stationName(id);
        if (const PrinterConfig *p = s_->settings.printer(id))
            where << qs(p->name);
    }
    for (const std::string &skip : s_->settings.expoSkips)
        if (where.contains(qs(skip).trimmed(), Qt::CaseInsensitive))
            return true;
    return false;
}

QVariantList PosService::expoTicketsNow() const
{
    // Everything sent and not yet run to the table, a ticket per send, with
    // what each station has made.
    struct Ticket { const Check *check; std::int64_t sentAt; std::vector<const OrderLine *> lines; };
    std::vector<Ticket> tickets;
    const auto collect = [&](const Check &c) {
        std::map<std::int64_t, std::vector<const OrderLine *>> bySend;
        for (const OrderLine &l : c.lines) {
            if (l.sent && !l.voided && !l.served && !l.isComment() && l.forKitchen() && !expoSkips(l))
                bySend[l.sentAt].push_back(&l);
        }
        for (auto &[sentAt, lines] : bySend)
            tickets.push_back({&c, sentAt, std::move(lines)});
        return !bySend.empty();
    };
    for (const auto &[id, c] : s_->open)
        if (!c.training)   // practice never reaches the kitchen
            collect(c);
    for (const Check &c : s_->closedToday)
        if (!s_->expoDone.contains(c.id) && !collect(c))
            s_->expoDone.insert(c.id);
    std::ranges::sort(tickets, [](const Ticket &a, const Ticket &b) {
        return a.check->rush != b.check->rush ? a.check->rush : a.sentAt < b.sentAt;
    });
    QVariantList out;
    for (const Ticket &t : tickets) {
        QVariantList lines;
        QStringList waitingOn;
        bool ready = true;
        for (const OrderLine *l : t.lines) {
            const QString station = stationName(stationOf(*l));
            QStringList mods;
            for (const Modifier &m : l->modifiers)
                if (!m.kitchenHide)
                    mods << qs(m.kitchenText());
            lines.append(QVariantMap{{u"name"_s, qs(l->kitchenText())}, {u"quantity"_s, l->quantity},
                                     {u"modifiers"_s, mods}, {u"station"_s, station}, {u"made"_s, allMade(*l)}, {u"note"_s, qs(l->note)},
                                     {u"seat"_s, l->seat}, {u"color"_s, qs(l->kitchenColor)}});
            if (!l->made && !waitingOn.contains(station))
                waitingOn << station;
            for (const Modifier &m : l->modifiers) {   // parts at other stations
                if (isPart(*l, m) && !m.made && !waitingOn.contains(stationName(m.station)))
                    waitingOn << stationName(m.station);
            }
            if (!allMade(*l))
                ready = false;
        }
        out.append(QVariantMap{
            {u"checkId"_s, qint64(t.check->id)}, {u"sentAt"_s, qint64(t.sentAt)}, {u"label"_s, qs(t.check->label)},
            {u"server"_s, qs(t.check->serverName)}, {u"type"_s, qs(toString(t.check->type))},
            {u"customer"_s, qs(t.check->customer.name)}, {u"lines"_s, lines}, {u"ready"_s, ready},
            {u"waitingOn"_s, waitingOn}, {u"rush"_s, t.check->rush}, {u"vip"_s, t.check->vip},
            {u"allergies"_s, ticketAllergies(*t.check, t.lines)},
            {u"warnMinutes"_s, s_->settings.kitchenWarnMinutes}, {u"lateMinutes"_s, s_->settings.kitchenLateMinutes},
        });
    }
    return out;
}

QString PosService::stationName(const std::string &id) const
{
    for (const Station &x : s_->settings.stations)
        if (x.id == id)
            return qs(x.name);
    if (const PrinterConfig *p = s_->settings.printer(id))
        return qs(p->name);
    QString name = qs(id);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

QVariantList PosService::kitchenStations() const
{
    QVariantList out;
    for (const Station &x : s_->settings.stations)
        out.append(QVariantMap{{u"id"_s, qs(x.id)}, {u"name"_s, qs(x.name)}});
    for (const PrinterConfig &p : s_->settings.printers)   // a printer's screen: everything for that ticket
        out.append(QVariantMap{{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)}, {u"printer"_s, true}});
    return out;
}

QString PosService::kitchenStation() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.station);
    return {};
}

bool PosService::setKitchenStation(const QString &station)
{
    // Kitchen screens have nobody logged in: this screen only changes itself,
    // and only to one of the store's stations (or none: its page's orders).
    if (!station.isEmpty() && std::ranges::none_of(kitchenStations(), [&](const QVariant &v) {
            return v.toMap().value(u"id"_s).toString() == station;
        }))
        return fail(tr("That isn't one of the kitchen's stations."));
    auto &list = s_->settings.terminals;
    auto it = std::ranges::find_if(list, [&](const TerminalConfig &t) { return qs(t.name) == terminal_; });
    if (it == list.end()) {
        TerminalConfig t;
        t.name = ss(terminal_);
        list.push_back(t);
        it = list.end() - 1;
    }
    it->station = ss(station);
    s_->saveSettings();
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(station.isEmpty() ? tr("This screen shows its page's orders") : tr("This screen: %1").arg(stationName(ss(station))));
    return true;
}

bool PosService::kitchenSound() const
{
    if (!s_->settings.kitchenSounds)
        return false;
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return !t.quiet;
    return true;
}

bool PosService::setKitchenSound(bool on)
{
    // Like the station: this screen only, nobody logged in needed.
    auto &list = s_->settings.terminals;
    auto it = std::ranges::find_if(list, [&](const TerminalConfig &t) { return qs(t.name) == terminal_; });
    if (it == list.end()) {
        TerminalConfig t;
        t.name = ss(terminal_);
        list.push_back(t);
        it = list.end() - 1;
    }
    it->quiet = !on;
    s_->saveSettings();
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(on ? tr("Sound on") : tr("Sound off"));
    return true;
}

bool PosService::expoBump(qint64 checkId, qint64 sentAt)
{
    Check *c = findAnyCheck(s_, checkId);
    if (!c)
        return fail(tr("That ticket is gone."));
    int n = 0;
    for (OrderLine &l : c->lines) {
        if (l.sent && !l.voided && !l.served && l.sentAt == sentAt && l.forKitchen() && !l.isComment() && !expoSkips(l)) {
            if (!l.made) {   // run before the station bumped it: it's made
                l.made = true;
                l.madeAt = now();
            }
            for (Modifier &m : l.modifiers) {
                if (isPart(l, m) && !m.made) {
                    m.made = true;
                    m.madeAt = now();
                }
            }
            l.served = true;
            l.servedAt = now();
            ++n;
        }
    }
    if (n == 0)
        return fail(tr("That ticket is already out."));
    s_->served.push_back({checkId, sentAt, {}});
    if (s_->sink)
        s_->sink->saveCheck(*c);
    emit s_->checksChanged();
    return true;
}

bool PosService::expoRecall()
{
    while (!s_->served.empty()) {
        const PosShared::Bump b = s_->served.back();
        s_->served.pop_back();
        Check *c = findAnyCheck(s_, b.checkId);
        if (!c)
            continue;
        for (OrderLine &l : c->lines) {
            if (l.sentAt == b.sentAt && l.served) {
                l.served = false;
                l.servedAt = 0;
            }
        }
        s_->kitchenAgain(c->id);
        if (s_->sink)
            s_->sink->saveCheck(*c);
        emit s_->checksChanged();
        emit notice(tr("Recalled %1").arg(qs(c->label)));
        return true;
    }
    return fail(tr("Nothing to recall."));
}

bool PosService::recallTicket()
{
    while (!s_->bumped.empty()) {
        const PosShared::Bump b = s_->bumped.back();
        s_->bumped.pop_back();
        Check *c = findAnyCheck(s_, b.checkId);
        if (!c)
            continue;
        for (OrderLine &l : c->lines) {
            if (l.sentAt != b.sentAt)
                continue;
            if (l.made && atStation(l, b.station)) {
                l.made = false;
                l.madeAt = 0;
            }
            for (Modifier &m : l.modifiers) {
                if (m.made && partAt(l, m, b.station)) {
                    m.made = false;
                    m.madeAt = 0;
                }
            }
        }
        s_->kitchenAgain(c->id);
        if (s_->sink)
            s_->sink->saveCheck(*c);
        emit s_->checksChanged();
        emit notice(tr("Recalled %1").arg(qs(c->label)));
        return true;
    }
    return fail(tr("Nothing to recall."));
}

} // namespace vt::app
