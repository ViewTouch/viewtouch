// PosService: a screen that can't reach the store's computer keeps selling.
// The store hands each screen an offline kit (what it needs to take orders:
// settings without their secrets, the menu, the staff, today's drawers, the
// checks open now), and takes in the checks it made while it was cut off.

#include "app/pos_service.hh"

#include "app/pos_json.hh"

#include <QJsonArray>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// What a screen may take offline: cash, and discounts (a card needs its
// processor, a gift card or house account the store's balances).
bool offlineTender(TenderKind kind) { return kind == TenderKind::Cash || kind == TenderKind::Discount; }

} // namespace

QJsonObject PosService::offlineKit() const
{
    QJsonObject kit = offlineKitShared();
    const QJsonObject own = offlineKitOwn();
    for (auto it = own.begin(); it != own.end(); ++it)
        kit.insert(it.key(), it.value());
    return kit;
}

QJsonObject PosService::offlineKitShared() const
{
    // Settings without what only the store's computer may know: the screens'
    // keys, the backup and standby keys, Stripe's, the texting address, the
    // maintainer's code.
    PosSettings settings = s_->settings;
    for (TerminalConfig &t : settings.terminals) {
        t.key.clear();
        t.cardReader.clear();
    }
    settings.backupKey.clear();
    settings.backupSalt.clear();
    settings.replicaKey.clear();
    settings.stripeSecretKey.clear();
    settings.stripeReaders.clear();
    settings.textWebhook.clear();
    settings.maintainerPinHash.clear();
    settings.maintainerPinSalt.clear();
    std::erase_if(settings.tenders, [](const Tender &t) { return !offlineTender(t.kind); });

    QJsonArray menu, employees, open, punches, drawers;
    for (const MenuItem &m : s_->menu)
        menu.append(toJson(m));
    for (const Employee &e : s_->employees)
        if (e.active)
            employees.append(toJson(e));
    for (const auto &[id, c] : s_->open)
        open.append(toJson(c));
    for (const TimePunch &p : s_->punches)
        if (p.open())
            punches.append(toJson(p));
    for (const DrawerSession &d : s_->drawers)
        if (d.open())
            drawers.append(toJson(d));
    return {{u"version"_s, 1}, {u"made"_s, qint64(now())},
            {u"settings"_s, toJson(settings)}, {u"menu"_s, menu}, {u"employees"_s, employees},
            {u"openChecks"_s, open}, {u"punches"_s, punches}, {u"drawers"_s, drawers},
            {u"day"_s, QJsonObject{{u"id"_s, qint64(s_->day.id)}, {u"openedAt"_s, qint64(s_->day.openedAt)}}},
            {u"lastCheckId"_s, qint64(s_->lastCheckId)},
            {u"lastDrawerId"_s, qint64(s_->lastDrawerId)}, {u"lastPunchId"_s, qint64(s_->lastPunchId)}};
}

QJsonObject PosService::offlineKitOwn() const
{
    // Its checks' numbers start after the store's, a hundred apart for each
    // screen (two screens cut off at once don't make the same numbers).
    qint64 place = 0;
    for (const TerminalConfig &t : s_->settings.terminals) {
        if (qs(t.name) == terminal_)
            break;
        ++place;
    }
    if (place >= qint64(s_->settings.terminals.size()))
        place = 0;
    // A food truck's orders too, a hundred on from the store's (which keeps
    // counting meanwhile): #3 next at the store, #103 on the first screen cut off.
    int lastOrder = 0;
    for (const auto &[id, c] : s_->open)
        lastOrder = std::max(lastOrder, c.orderNumber);
    for (const Check &c : s_->closedToday)
        lastOrder = std::max(lastOrder, c.orderNumber);
    return {{u"terminal"_s, terminal_}, {u"firstCheckId"_s, qint64(s_->lastCheckId + 1 + 100 * place)},
            {u"firstOrderNumber"_s, lastOrder + 1 + 100 * int(place + 1)}};
}

std::optional<PosData> offlineData(const QJsonObject &kit)
{
    if (kit.value(u"version").toInt() != 1 || !kit.value(u"settings").isObject())
        return std::nullopt;
    PosData d;
    d.settings = settingsFromJson(kit.value(u"settings").toObject());
    d.menu = menuFromJson(kit.value(u"menu").toArray());
    d.employees = employeesFromJson(kit.value(u"employees").toArray());
    for (const QJsonValue &v : kit.value(u"openChecks").toArray())
        if (auto c = checkFromJson(v.toObject()))
            d.openChecks.push_back(*c);
    for (const QJsonValue &v : kit.value(u"punches").toArray())
        d.punches.push_back(punchFromJson(v.toObject()));
    for (const QJsonValue &v : kit.value(u"drawers").toArray())
        d.drawers.push_back(drawerFromJson(v.toObject()));
    const QJsonObject day = kit.value(u"day").toObject();
    d.currentDay = BusinessDay{day.value(u"id").toInteger(), day.value(u"openedAt").toInteger(), 0};
    d.lastDayId = d.currentDay->id;
    // New checks take the numbers set aside for this screen.
    d.lastCheckId = std::max(kit.value(u"lastCheckId").toInteger(), kit.value(u"firstCheckId").toInteger() - 1);
    d.firstOrderNumber = kit.value(u"firstOrderNumber").toInt();
    d.lastDrawerId = kit.value(u"lastDrawerId").toInteger();
    d.lastPunchId = kit.value(u"lastPunchId").toInteger();
    return d;
}

QVariantMap PosService::takeOfflineCheck(const QJsonObject &record)
{
    std::optional<Check> read = checkFromJson(record);
    if (!read || read->offlineId.empty())
        return {{u"error"_s, tr("That check can't be read.")}};
    Check c = std::move(*read);
    // Taken in before (the answer didn't get back to the screen): the same answer.
    const auto same = [&](const Check &x) { return x.offlineId == c.offlineId; };
    for (const auto &[id, x] : s_->open)
        if (same(x))
            return {{u"id"_s, qint64(id)}, {u"again"_s, true}};
    if (const auto it = std::ranges::find_if(s_->closedToday, same); it != s_->closedToday.end())
        return {{u"id"_s, qint64(it->id)}, {u"again"_s, true}};
    const Employee *server = s_->employee(c.serverId);
    if (!server)
        return {{u"error"_s, tr("%1 isn't on the staff here.").arg(qs(c.serverName))}};
    if (c.status != CheckStatus::Open && c.status != CheckStatus::Closed)
        return {{u"skipped"_s, true}};   // a practice check, or one thrown away: nothing to keep
    // Only what a screen can take offline.
    std::erase_if(c.payments, [](const Payment &p) { return !offlineTender(p.kind); });

    // Its number, if it's the next one here; otherwise the next one.
    const std::int64_t was = c.id;
    c.id = was == s_->lastCheckId + 1 ? was : s_->lastCheckId + 1;
    s_->lastCheckId = c.id;
    noteEvent(c, c.id == was ? tr("Taken while %1 was offline").arg(terminal_)
                             : tr("Taken while %1 was offline, as #%2").arg(terminal_).arg(was),
              "offline");
    // What went to the kitchen used up its stock here too.
    std::vector<OrderLine> sent;
    for (const OrderLine &l : c.lines)
        if (l.sent && !l.voided)
            sent.push_back(l);
    if (!c.training)
        takeStock(sent, 1);

    if (c.status == CheckStatus::Open) {
        s_->open[c.id] = c;
        if (s_->sink)
            s_->sink->saveCheck(c);
        emit s_->checksChanged();
        return {{u"id"_s, qint64(c.id)}};
    }
    // Closed: paid and gone, so made (not on the kitchen screens now), in
    // today's sales, its cash in the drawer it went in (or its server's bank).
    for (OrderLine &l : c.lines) {
        l.made = true;
        for (Modifier &m : l.modifiers)
            m.made = true;
    }
    c.businessDay = s_->day.id;
    const Totals t = c.totals(s_->settings.tax);
    if (t.cashPaid.cents() > 0 || c.cashTip.cents() > 0) {
        // The drawer it went in, still open here, and the same one (a bank the
        // screen opened offline has a number of its own there).
        const auto open = [&](std::int64_t id) {
            return std::ranges::any_of(s_->drawers, [&](const DrawerSession &d) {
                return d.id == id && d.open()
                       && (d.employeeId == c.serverId || (d.employeeId.empty() && qs(d.terminal) == terminal_));
            });
        };
        if (!open(c.drawerSession)) {
            // As at the register: a bank of their own (opened now, if it isn't),
            // else this screen's drawer.
            const bool bank = !server->cashMode.empty() ? server->cashMode == "serverBank"
                                                        : server->role == "driver" || s_->settings.cashMode == CashMode::ServerBank;
            DrawerSession *d = bank ? s_->openBankFor(server->id) : s_->openDrawerFor(terminal_.toStdString());
            if (!d && bank) {
                DrawerSession b;
                b.id = ++s_->lastDrawerId;
                b.name = ss(tr("%1's bank").arg(qs(server->name)));
                b.employeeId = server->id;
                b.openedAt = c.closedAt ? c.closedAt : now();
                b.openedBy = server->name;
                s_->drawers.push_back(b);
                if (s_->sink)
                    s_->sink->saveDrawer(b);
                d = &s_->drawers.back();
            }
            c.drawerSession = d ? d->id : 0;
            if (!d)
                s_->noteProblem("offline", ss(tr("#%1's cash").arg(c.id)),
                                ss(tr("%1 cash taken while %2 was offline isn't in an open drawer.")
                                       .arg(format(t.cashPaid), terminal_)));
        }
    }
    applyCloseEffects(c);
    if (s_->sink)
        s_->sink->saveCheck(c);
    s_->closedToday.push_back(c);
    emit s_->checksChanged();
    emit s_->dayChanged();
    emit s_->drawerChanged();
    return {{u"id"_s, qint64(c.id)}};
}

} // namespace vt::app
