// PosService: the kitchen screens and the expediter: tickets by station, bumping,
// recalling.

#include "app/pos_service.hh"

#include "app/i18n.hh"
#include "app/pos_json.hh"

#include <QDateTime>
#include <QLocale>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

// --- kitchen display -----------------------------------------------------------------------

QVariantList PosService::kitchenTicketsNow() const
{
    // A ticket is everything sent in one go from one check that the kitchen
    // has not bumped yet. Closed checks count too (pay-first counters).
    struct Ticket { const Check *check; std::int64_t sentAt; std::vector<const OrderLine *> lines; };
    std::vector<Ticket> tickets;
    auto collect = [&](const Check &c) {
        std::map<std::int64_t, std::vector<const OrderLine *>> bySend;
        for (const OrderLine &l : c.lines) {
            const bool partsLeft = std::ranges::any_of(l.modifiers, [&](const Modifier &m) { return isPart(l, m) && !m.made; });
            if (l.sent && (!l.made || partsLeft) && !l.voided && (l.isComment() || l.forKitchen()))
                bySend[l.sentAt].push_back(&l);
        }
        for (auto &[sentAt, lines] : bySend)
            tickets.push_back({&c, sentAt, std::move(lines)});
    };
    for (const auto &[id, c] : s_->open)
        if (!c.training)   // practice never reaches the kitchen
            collect(c);
    for (const Check &c : s_->closedToday)
        collect(c);
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
            if (!l->made)
                lines.append(QVariantMap{{u"name"_s, qs(l->isComment() ? l->name : l->kitchenText())},
                                         {u"color"_s, qs(l->kitchenColor)}, {u"quantity"_s, l->quantity},
                                         {u"contains"_s, allergenNames(allergyHits(*t.check, *l))},
                                         {u"modifiers"_s, mods}, {u"comment"_s, l->isComment()},
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
        out.append(QVariantMap{
            {u"checkId"_s, qint64(t.check->id)}, {u"sentAt"_s, qint64(t.sentAt)},
            {u"label"_s, qs(t.check->label)}, {u"server"_s, qs(t.check->serverName)},
            {u"type"_s, qs(toString(t.check->type))}, {u"customer"_s, qs(t.check->customer.name)},
            {u"note"_s, qs(t.check->customer.note)}, {u"lines"_s, lines},
            {u"rush"_s, t.check->rush}, {u"vip"_s, t.check->vip},
            {u"allergies"_s, allergenNames(t.check->allergies)},
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
        if (!l.sent || l.voided || l.sentAt != sentAt)
            continue;
        if (!l.made && atStation(l, where)) {
            l.made = true;
            l.madeAt = now();
            ++n;
            // Learn what it usually takes (ignoring the odd forgotten ticket).
            const std::int64_t took = (l.madeAt - l.sentAt) / 1000;
            if (!l.isComment() && took > 0 && took < 90 * 60 && !c->training) {
                int &avg = s_->settings.prepSeconds[l.itemId];
                avg = avg == 0 ? int(took) : int(std::lround(0.8 * avg + 0.2 * double(took)));
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
        s_->saveSettings();
    s_->bumped.push_back({checkId, sentAt, where});
    if (s_->sink)
        s_->sink->saveCheck(*c);
    emit s_->checksChanged();
    return true;
}

// --- the expediter -------------------------------------------------------------------------

QVariantList PosService::expoTicketsNow() const
{
    // Everything sent and not yet run to the table, a ticket per send, with
    // what each station has made.
    struct Ticket { const Check *check; std::int64_t sentAt; std::vector<const OrderLine *> lines; };
    std::vector<Ticket> tickets;
    const auto collect = [&](const Check &c) {
        std::map<std::int64_t, std::vector<const OrderLine *>> bySend;
        for (const OrderLine &l : c.lines) {
            if (l.sent && !l.voided && !l.served && !l.isComment() && l.forKitchen())
                bySend[l.sentAt].push_back(&l);
        }
        for (auto &[sentAt, lines] : bySend)
            tickets.push_back({&c, sentAt, std::move(lines)});
    };
    for (const auto &[id, c] : s_->open)
        if (!c.training)   // practice never reaches the kitchen
            collect(c);
    for (const Check &c : s_->closedToday)
        collect(c);
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
                                     {u"modifiers"_s, mods}, {u"station"_s, station}, {u"made"_s, allMade(*l)},
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
            {u"allergies"_s, allergenNames(t.check->allergies)},
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
    // Kitchen screens have nobody logged in: this screen only changes itself.
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

bool PosService::expoBump(qint64 checkId, qint64 sentAt)
{
    Check *c = findAnyCheck(s_, checkId);
    if (!c)
        return fail(tr("That ticket is gone."));
    int n = 0;
    for (OrderLine &l : c->lines) {
        if (l.sent && !l.voided && !l.served && l.sentAt == sentAt && l.forKitchen() && !l.isComment()) {
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
        if (s_->sink)
            s_->sink->saveCheck(*c);
        emit s_->checksChanged();
        emit notice(tr("Recalled %1").arg(qs(c->label)));
        return true;
    }
    return fail(tr("Nothing to recall."));
}

} // namespace vt::app
