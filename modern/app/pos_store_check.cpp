// Store Check (Manager -> Store Check): are we ready to open, and what went
// wrong lately (PosShared::problems).

#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <cmath>
#include <QDateTime>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

QString qs(const std::string &s) { return QString::fromStdString(s); }

} // namespace

QString PosService::printerProblemText(const QString &name, const std::string &what)
{
    return what == "paperOut"      ? tr("%1: out of paper").arg(name)
           : what == "coverOpen"    ? tr("%1: the cover is open").arg(name)
           : what == "paperLow"     ? tr("%1: paper running low").arg(name)
           : what == "offline"      ? tr("%1: not answering: check its paper, cover and power (or the network)").arg(name)
           : what == "offlinePaper" ? tr("%1: not answering, probably out of paper (it was running low): put in a new roll and close the cover").arg(name)
           : what == "silent"       ? tr("%1: not responding: check that the cover is closed and it has paper, else turn it off and on").arg(name)
                                    : tr("%1 has a problem (a paper jam or the cutter?)").arg(name);
}

QString PosService::problemText(const Problem &p) const
{
    const QString detail = qs(p.detail);
    if (p.kind == "printer")
        return printerProblemText(qs(p.subject), p.detail);
    if (p.kind == "database")
        return tr("Couldn't save to the database (it kept trying): %1").arg(detail);
    if (p.kind == "backup")
        return tr("A backup failed: %1").arg(detail);
    if (p.kind == "backupCopy")
        return tr("The second copy of a backup failed: %1").arg(detail);
    if (p.kind == "standby")
        return tr("The standby computer lost touch with this one");
    if (p.kind == "wrongPins")
        return p.subject == "store" ? tr("Many wrong PINs across the store (%1 in 10 minutes): someone may be guessing").arg(detail)
                                    : tr("%1 wrong PINs in a row on %2: someone may be guessing").arg(detail, qs(p.subject));
    if (p.kind == "crash")
        return tr("ViewTouch closed without being told to (a crash, or the power went off)");
    return detail.isEmpty() ? qs(p.subject) : qs(p.subject) + u": "_s + detail;
}

QVariantMap PosService::storeCheck() const
{
    if (!can(u"manager"_s))
        return {};
    const QVariantMap n = s_->network ? s_->network() : QVariantMap();
    const auto when = [](std::int64_t ms) {
        return i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms), u"ddd h:mm AP"_s);
    };
    QVariantList checks;
    bool ready = true;
    // info: worth knowing, not a reason to wait.
    const auto add = [&](const QString &id, bool ok, const QString &title, const QString &detail, bool info = false) {
        checks.append(QVariantMap{{u"id"_s, id}, {u"ok"_s, ok}, {u"info"_s, info}, {u"title"_s, title}, {u"detail"_s, detail}});
        ready = ready && (ok || info);
    };

    // The sample staff's PINs (1234, 1111...) are the same in every ViewTouch:
    // anyone could log in with them while they're on.
    {
        QStringList samples;
        for (const Employee &e : s_->employees)
            if (e.active && e.sample)
                samples << qs(e.name);
        if (!samples.isEmpty())
            add(u"samples"_s, false, tr("Sample staff"),
                tr("%1 still on, with PINs anyone could know. Turn them off (Manager → Employees, or the setup guide).")
                    .arg(samples.join(u", "_s)));
    }

    // A newer ViewTouch to install (found once a day).
    if (!s_->newVersion.isEmpty())
        add(u"newVersion"_s, false, tr("New ViewTouch"),
            tr("ViewTouch %1 is out. On the store computer: close ViewTouch, then Updates → Check for a New ViewTouch.")
                .arg(s_->newVersion), true);

    // The law and the store's data: settings worth having (not reasons to wait).
    if (s_->settings.laborState.empty())
        add(u"state"_s, false, tr("The store's state"),
            tr("Not chosen: overtime, meal breaks and gift cards follow federal rules only. Store Settings → Staff hours."), true);
    // The tax rate where the store is: its ZIP code, and a newer rate than
    // the one the store took (a store that changed its rates after taking
    // them, say for a local meals tax, isn't asked again until the next table).
    if (s_->settings.storeZip.empty()) {
        add(u"taxArea"_s, false, tr("Where the store is"),
            tr("No ZIP code: Store Settings → Taxes can show the sales tax rate there."), true);
    } else if (const TaxSuggestion here = taxHere(); here.found && qs(s_->settings.taxAreaAsOf) < here.asOf) {
        const auto ppm = [](double pct) { return std::llround(pct * 10000); };
        const core::TaxRates &t = s_->settings.tax;
        if (ppm(here.food) != t.foodPpm || ppm(here.alcohol) != t.alcoholPpm || ppm(here.sales) != t.merchandisePpm)
            add(u"taxArea"_s, false, tr("Tax rates"),
                tr("The rates for %1 (as of %2): food %3%, alcohol %4%, merchandise %5%. This store charges %6%, %7%, %8%. "
                   "Store Settings → Taxes.")
                    .arg(qs(s_->settings.storeZip), here.asOf)
                    .arg(here.food).arg(here.alcohol).arg(here.sales)
                    .arg(double(t.foodPpm) / 10000).arg(double(t.alcoholPpm) / 10000).arg(double(t.merchandisePpm) / 10000),
                true);
    }
    // An automatic gratuity, not taxed, in a state where it may be.
    if (s_->settings.gratuityBp > 0 && !s_->settings.tax.taxGratuity)
        add(u"gratuityTax"_s, false, tr("Tax on the automatic gratuity"),
            tr("The automatic gratuity isn't taxed. In most states a charge the guest can't decline is taxable "
               "(always in California and Florida): ask your accountant, then Store Settings → Taxes."), true);
    if (s_->settings.backupKey.empty())
        add(u"backupPassword"_s, false, tr("Encrypted backups"),
            tr("Backups are plain copies of everything (customers, staff, sales). Set a backup password: Store Settings → Backups."), true);
    if (s_->settings.stripeSecretKey.starts_with("sk_"))
        add(u"stripeKey"_s, false, tr("Stripe key"),
            tr("A full secret key is saved. A restricted key (rk_...) with only Terminal and refunds does far less harm if it's ever copied."), true);

    // Meal breaks the state requires: who's past due right now.
    if (const int due = s_->settings.mealBreakAfterHours; due > 0) {
        QStringList late;
        for (const TimePunch &p : s_->punches)
            if (p.open() && !p.mealStartedAt() && p.mealLate(due, now()))
                if (const Employee *e = s_->employee(p.employeeId))
                    late << tr("%1 (due %2)").arg(qs(e->name), i18n::timeText(QDateTime::fromMSecsSinceEpoch(p.mealDueAt(due)).time()));
        if (!late.isEmpty())
            add(u"meals"_s, false, tr("Meal breaks"), tr("Past due: %1").arg(late.join(u", "_s)));
    }

    // Printers: each one answering.
    if (s_->settings.printers.empty())
        add(u"printers"_s, false, tr("Printers"), tr("No printer is set up (Manager → Printers)."));
    QHash<QString, QString> status;
    for (const QVariant &v : n.value(u"printers"_s).toList())
        status.insert(v.toMap().value(u"name"_s).toString(), v.toMap().value(u"status"_s).toString());
    for (const PrinterConfig &p : s_->settings.printers) {
        const QString name = qs(p.name);
        const auto problem = s_->printerProblems.find(p.id);
        if (problem != s_->printerProblems.end() && problem->second != "paperLow")
            add(u"printer:"_s + qs(p.id), false, name, printerProblemText(name, problem->second));
        else if (problem != s_->printerProblems.end())
            add(u"printer:"_s + qs(p.id), true, name, printerProblemText(name, problem->second));
        else
            add(u"printer:"_s + qs(p.id), true, name,
                status.value(name) == u"ok"_s || status.value(name) == u"ready"_s ? tr("Answering")
                : p.watch ? tr("No problem reported") : tr("Not watched (its status isn't asked)"));
    }

    // A backup in the last day.
    if (n.contains(u"backups"_s)) {
        const std::int64_t last = n.value(u"lastBackup"_s).toLongLong();
        add(u"backup"_s, last > 0 && now() - last < 26LL * 3'600'000, tr("Backup"),
            last > 0 ? tr("Last one %1").arg(when(last)) : tr("No backup yet"));
    }
    // Room on the disk for the day's data and backups.
    if (n.contains(u"diskFree"_s)) {
        const double gb = n.value(u"diskFree"_s).toDouble() / 1e9;
        add(u"disk"_s, gb >= 1.0, tr("Disk space"), tr("%1 GB free").arg(QString::number(gb, 'f', 1)));
    }
    // The standby: a second computer ready to take over.
    if (n.value(u"role"_s) == u"main"_s) {
        const QVariantMap standby = n.value(u"standby"_s).toMap();
        if (!standby.isEmpty())
            add(u"standby"_s, true, tr("Standby computer"), tr("Connected: %1").arg(standby.value(u"name"_s).toString()));
        else
            add(u"standby"_s, true, tr("Standby computer"),
                tr("None connected (a second computer that takes over if this one fails)"), true);
    }
    // The clock: never behind what was already saved (a dead clock battery).
    std::int64_t latest = s_->day.openedAt;
    for (const Check &c : s_->closedToday)
        latest = std::max(latest, c.closedAt);
    if (!s_->pastDays.empty())
        latest = std::max(latest, s_->pastDays.front().day.closedAt);
    add(u"clock"_s, now() >= latest - 5 * 60'000, tr("Date and time"),
        now() >= latest - 5 * 60'000
            ? i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(now()), u"ddd MMM d, h:mm AP"_s)
            : tr("Behind the last sale (%1): set this computer's date and time").arg(when(latest)));
    // The cash drawers started (a drawer per screen).
    if (s_->settings.cashMode == CashMode::TerminalDrawer) {
        QStringList waiting, started;
        for (const TerminalConfig &t : s_->settings.terminals) {
            if (!s_->settings.hasDrawer(t.name))
                continue;
            const bool open = std::ranges::any_of(s_->drawers, [&](const DrawerSession &d) { return d.terminal == t.name && d.open(); });
            (open ? started : waiting) << qs(t.name);
        }
        if (!waiting.isEmpty() || !started.isEmpty())
            add(u"drawers"_s, waiting.isEmpty(), tr("Cash drawers"),
                waiting.isEmpty() ? tr("Started: %1").arg(started.join(u", "_s))
                                  : tr("Not started yet: %1").arg(waiting.join(u", "_s)));
    }
    // The opening checklist.
    const int tasks = int(s_->settings.openingChecklist.size());
    if (tasks > 0) {
        const int done = checklists().value(u"openingDone"_s).toInt();
        add(u"checklist"_s, done >= tasks, tr("Opening checklist"), tr("%1 of %2 done").arg(done).arg(tasks));
    }

    // What went wrong lately, newest first.
    QVariantList problems;
    int unseen = 0;
    for (auto it = s_->problems.rbegin(); it != s_->problems.rend(); ++it) {
        const bool isNew = it->id > s_->settings.problemsSeen;
        unseen += isNew ? 1 : 0;
        problems.append(QVariantMap{{u"when"_s, when(it->at)}, {u"kind"_s, qs(it->kind)},
                                    {u"text"_s, problemText(*it)}, {u"new"_s, isNew}});
    }
    add(u"problems"_s, unseen == 0, tr("Problems"),
        unseen == 0 ? tr("Nothing new") : tr("%n new (below)", "", unseen));
    return {{u"checks"_s, checks}, {u"problems"_s, problems}, {u"unseen"_s, unseen}, {u"ready"_s, ready}};
}

bool PosService::problemsSeen()
{
    if (!require(perm::Manager, tr("Store Check")))
        return false;
    s_->settings.problemsSeen = s_->lastProblemId;
    s_->saveSettings();
    emit s_->networkChanged();
    return true;
}

} // namespace vt::app
