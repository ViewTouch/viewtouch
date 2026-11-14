// PosService: loss alerts. What a manager would want to know about as it
// happens (Dashboard): a big void of an item already sent, a big discount or
// comp, a payment taken back after the check printed, a cash check reopened,
// a refund, a third drawer opening with no sale in an hour by one person.
// Worked out from today's checks, refunds and drawers (nothing more is kept),
// so every screen sees the same alerts, and a restart loses none.

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QDateTime>

#include <algorithm>
#include <map>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr std::int64_t kHour = 3'600'000;

bool watched(const std::string &kind)
{
    return kind == "void" || kind == "discount" || kind == "unpay" || kind == "undiscount" || kind == "reopen";
}

} // namespace

bool PosService::watchesEvent(const char *kind) { return watched(kind ? std::string(kind) : std::string()); }

QVariantList PosService::lossAlertsNow() const
{
    if (!user() || !user()->can(perm::Manager) || !s_->settings.watch)
        return {};
    const PosSettings &st = s_->settings;
    // Today's: since the business day opened, or this morning if that's earlier.
    const std::int64_t since = std::min<std::int64_t>(s_->day.openedAt > 0 ? s_->day.openedAt : now(),
                                        QDateTime::fromMSecsSinceEpoch(now()).date().startOfDay().toMSecsSinceEpoch());
    struct Alert {
        std::int64_t at;
        QString id, kind, who, text;
        std::int64_t checkId;
        qint64 cents;
    };
    std::vector<Alert> out;
    const auto who = [this](const std::string &name) { return name.empty() ? tr("Someone") : qs(name); };

    const auto look = [&](const Check &c) {
        if (c.training)
            return;
        const QString label = qs(c.label).isEmpty() ? tr("#%1").arg(c.id) : qs(c.label);
        const Money items = c.totals(st.tax).items;
        const bool cash = std::ranges::any_of(c.payments, [](const Payment &p) { return p.kind == TenderKind::Cash; });
        for (const CheckEvent &e : c.events) {
            if (e.at < since || !watched(e.kind))
                continue;
            const QString reason = e.reason.empty() ? QString() : u": "_s + qs(e.reason);
            const QString money = format(e.amount);
            QString text;
            if (e.kind == "void" && e.amount >= st.watchVoid && e.amount.cents() > 0) {
                text = tr("%1 voided %2 on %3 after it was sent").arg(who(e.who), money, label) + reason;
            } else if (e.kind == "discount" && e.amount.cents() > 0
                       && (e.amount >= st.watchComp || (items.cents() > 0 && e.amount.cents() * 2 >= items.cents()))) {
                text = tr("%1 took %2 off %3 (%4)").arg(who(e.who), money, label, qs(e.what)) + reason;
            } else if (e.kind == "unpay" && e.amount.cents() > 0) {
                const bool printed = std::ranges::any_of(c.events, [&](const CheckEvent &p) {
                    return p.kind == "printed" && p.at <= e.at && &p < &e;   // (printed first)
                });
                if (printed || e.amount >= st.watchVoid)
                    text = tr("%1 took back a %2 payment on %3%4").arg(who(e.who), money, label,
                                                                     printed ? tr(", after it printed") : QString());
            } else if (e.kind == "reopen" && cash) {
                text = tr("%1 reopened %2, paid in cash (%3)").arg(who(e.who), label, money);
            }
            if (!text.isEmpty())
                out.push_back({e.at, QString::fromStdString(e.kind) + u'-' + QString::number(c.id) + u'-' + QString::number(e.at),
                               QString::fromStdString(e.kind), qs(e.who), text, c.id, e.amount.cents()});
        }
    };
    for (const auto &[id, c] : s_->open)
        look(c);
    for (const Check &c : s_->closedToday)
        look(c);

    for (const Refund &r : s_->refundsToday) {
        if (r.at < since)
            continue;
        const QString label = r.checkLabel.empty() ? tr("#%1").arg(r.checkId) : qs(r.checkLabel);
        out.push_back({r.at, u"refund-"_s + QString::number(r.id), u"refund"_s, qs(r.by),
                       tr("%1 refunded %2 on %3").arg(who(r.by), format(r.amount), label)
                           + (r.reason.empty() ? QString() : u": "_s + qs(r.reason)),
                       r.checkId, r.amount.cents()});
    }

    // The drawer opened with nothing sold: the third time within an hour.
    std::map<std::string, std::vector<std::int64_t>> noSales;
    for (const DrawerSession &d : s_->drawers)
        for (const CashMovement &m : d.movements)
            if (m.kind == CashMovement::Kind::NoSale && m.at >= since)
                noSales[m.by].push_back(m.at);
    for (auto &[by, times] : noSales) {
        std::ranges::sort(times);
        std::int64_t last = -kHour;
        for (std::size_t i = 2; i < times.size(); ++i)
            if (times[i] - times[i - 2] <= kHour && times[i] - last > kHour) {
                last = times[i];
                out.push_back({times[i], u"nosale-"_s + qs(by) + u'-' + QString::number(times[i]), u"noSale"_s, qs(by),
                               tr("%1 opened the drawer with no sale 3 times in an hour").arg(who(by)), 0, 0});
            }
    }

    // Newest first (at the same moment, the one that came later).
    std::ranges::reverse(out);
    std::ranges::stable_sort(out, [](const Alert &a, const Alert &b) { return a.at > b.at; });
    QVariantList list;
    for (const Alert &a : out)
        list.append(QVariantMap{{u"id"_s, a.id}, {u"at"_s, qint64(a.at)}, {u"kind"_s, a.kind}, {u"who"_s, a.who}, {u"text"_s, a.text},
                                {u"checkId"_s, qint64(a.checkId)}, {u"cents"_s, a.cents},
                                {u"time"_s, i18n::timeText(QDateTime::fromMSecsSinceEpoch(a.at).time())},
                                {u"new"_s, a.at > st.alertsSeenAt}});
    return list;
}

QVariantList PosService::lossAlerts() const
{
    return s_->memo(u"lossAlerts "_s + language(), [this] { return QVariant(lossAlertsNow()); }).toList();
}

bool PosService::alertsSeen()
{
    if (!require(perm::Manager, tr("Alerts")))
        return false;
    s_->settings.alertsSeenAt = now();
    if (s_->sink)
        s_->sink->saveSettings(s_->settings);
    emit s_->dayChanged();
    return true;
}

} // namespace vt::app
