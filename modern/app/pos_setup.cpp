// PosService: the setup guide - a new store's name, logo, look, taxes, first
// menu items and staff, a step at a time (SetupGuide.qml).

#include "app/i18n.hh"
#include "app/pos_json.hh"
#include "app/pos_service.hh"
#include "app/tax_areas.hh"

#include <QDateTime>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

QVariantMap PosService::setupInfo() const
{
    if (!can(QString::fromLatin1(perm::Manager)))
        return {{u"done"_s, s_->settings.setupDone}};
    QStringList families;
    int items = 0;
    for (const MenuItem &m : s_->menu) {
        if (m.isModifier)
            continue;
        ++items;
        if (!m.family.empty() && !families.contains(qs(m.family)))
            families << qs(m.family);
    }
    QVariantList staff;
    int samples = 0;
    for (const Employee &e : s_->employees) {
        if (!e.active)
            continue;
        samples += e.sample && !(retireMeAtFinish_ && user() && e.id == user()->id) ? 1 : 0;
        staff.append(QVariantMap{{u"name"_s, qs(e.name)}, {u"role"_s, qs(e.role)}, {u"sample"_s, e.sample}});
    }
    const auto pct = [](std::int64_t ppm) { return double(ppm) / 10000.0; };
    // Getting ready: what a new store still has to do before it opens, each
    // with where it's done (a step of the guide, or a page). `need`: without
    // it the store isn't ready; the rest are worth doing.
    QVariantList ready;
    int left = 0;
    const auto row = [&](const char *id, bool done, bool need, const QString &title, const QString &text,
                         const QString &step, const QString &page = {}) {
        ready.append(QVariantMap{{u"id"_s, QString::fromLatin1(id)}, {u"done"_s, done}, {u"need"_s, need},
                                 {u"title"_s, title}, {u"text"_s, text}, {u"step"_s, step}, {u"page"_s, page}});
        left += !done && need;
    };
    const QString name = qs(s_->settings.storeName).trimmed();
    row("name", !name.isEmpty() && name != u"ViewTouch" && name != u"ViewTouch Café", true, tr("Your store's name"),
        tr("On every receipt and the login screen."), u"store"_s);
    row("taxes", s_->settings.tax.foodPpm > 0 || !s_->settings.storeZip.empty(), true, tr("Sales tax"),
        s_->settings.tax.foodPpm > 0 ? tr("Food %1%").arg(pct(s_->settings.tax.foodPpm))
                                     : tr("0% now: type your ZIP code for the rate, or the rate itself."),
        u"taxes"_s);
    // (The sample café's menu: its burgers and salads are still on it.)
    const bool sampleMenu = std::ranges::count_if(s_->menu, [](const MenuItem &m) {
        return m.id == "classic-burger" || m.id == "cheeseburger" || m.id == "house-salad";
    }) == 3;
    row("menu", items > 0 && !sampleMenu, true, tr("Your menu"),
        sampleMenu ? tr("Still the sample café's: type yours, or bring it in from a file or from legacy ViewTouch.")
                   : tr("%n item(s)", nullptr, items),
        u"menu"_s);
    const bool ownManager = std::ranges::any_of(s_->employees, [](const Employee &e) {
        return e.active && !e.sample && (e.role == "manager" || e.role == "admin" || e.role == "owner");
    });
    row("staff", ownManager && samples == 0, true, tr("Your team"),
        !ownManager ? tr("Add yourself as a manager, with a PIN only you know.")
        : samples > 0 ? tr("The sample staff are still on: their PINs (1234, 1111...) are public. Turn them off.")
                      : tr("%n people", nullptr, int(staff.size())),
        u"staff"_s);
    const bool printer = std::ranges::any_of(s_->settings.printers, [](const PrinterConfig &p) { return p.type != "none"; });
    row("printers", printer, false, tr("Printers"),
        printer ? tr("Print a test on each: Manager → Printers.")
                : tr("None yet: receipts and kitchen tickets show on screen only. Manager → Printers."),
        u"printers"_s);
    if (const QVariantMap n = s_->network ? s_->network() : QVariantMap(); n.contains(u"backups"_s)) {
        const bool backedUp = n.value(u"lastBackup"_s).toLongLong() > 0;
        row("backup", backedUp, false, tr("Backups"),
            backedUp ? tr("Made every night. A second copy on a USB stick or another computer is safer: Store Settings → Backups.")
                     : tr("None yet: one is made tonight. Make one now in Store Settings → Backups."),
            u"backups"_s);
    }
    row("tables", true, false, tr("Your tables and screens"),
        tr("Arrange the dining room: Manager → Edit Pages. Other screens and tablets: Manager → Network."), {}, u"tables"_s);
    QVariantList printers;
    for (const PrinterConfig &p : s_->settings.printers)
        if (p.type != "none")
            printers.append(QVariantMap{{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)},
                                        {u"where"_s, p.type == "network" ? qs(p.host) : qs(p.type)}});
    QVariantMap backup;
    if (const QVariantMap n = s_->network ? s_->network() : QVariantMap(); n.contains(u"backups"_s)) {
        const std::int64_t last = n.value(u"lastBackup"_s).toLongLong();
        backup = {{u"folder"_s, n.value(u"backups"_s)},
                  {u"last"_s, last > 0 ? i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(last), u"ddd MMM d, h:mm AP"_s)
                                       : QString()}};
    }
    return {{u"printers"_s, printers}, {u"backup"_s, backup}, {u"ready"_s, ready}, {u"left"_s, left},
            {u"done"_s, s_->settings.setupDone}, {u"storeName"_s, qs(s_->settings.storeName)},
            {u"receiptHeader"_s, qs(s_->settings.receiptHeader)}, {u"logo"_s, qs(s_->settings.displayLogo)},
            {u"receiptLogo"_s, s_->settings.receiptLogo}, {u"foodTax"_s, pct(s_->settings.tax.foodPpm)},
            {u"alcoholTax"_s, pct(s_->settings.tax.alcoholPpm)}, {u"families"_s, families}, {u"items"_s, items},
            {u"zip"_s, qs(s_->settings.storeZip)}, {u"taxArea"_s, s_->settings.storeZip.empty() ? QString() : taxAreaText(taxHere())},
            {u"staff"_s, staff}, {u"samples"_s, samples}, {u"serviceMode"_s, qs(s_->settings.serviceMode)}};
}

namespace {
// The first (only) record of a settings panel, with changes on top.
QVariantMap withChanges(QVariantMap record, const QVariantMap &changes)
{
    for (auto it = changes.begin(); it != changes.end(); ++it)
        record.insert(it.key(), it.value());
    return record;
}
} // namespace

bool PosService::setupStore(const QString &name, const QString &receiptLines, const QString &serviceMode)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    if (name.trimmed().isEmpty())
        return fail(tr("Type the store's name."));
    const QVariantMap store = adminRecords(u"store"_s).value(0).toMap();
    QVariantMap changes{{u"storeName"_s, name.trimmed()}, {u"receiptHeader"_s, receiptLines}};
    if (!serviceMode.isEmpty())   // "restaurant", "foodTruck" or "retail"
        changes.insert(u"serviceMode"_s, serviceMode);
    return adminSave(u"store"_s, 0, withChanges(store, changes));
}

bool PosService::setupLogo(const QString &ref, bool onReceipts)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    const QVariantMap store = adminRecords(u"store"_s).value(0).toMap();
    return adminSave(u"store"_s, 0, withChanges(store, {{u"displayLogo"_s, ref}, {u"receiptLogo"_s, onReceipts}}));
}

bool PosService::setupTaxArea(const QString &zip)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    const QString z = zip.trimmed().left(5);
    const QString state = TaxAreas::instance().stateOf(z);
    if (z.size() != 5 || state.isEmpty())
        return fail(tr("That isn't a ZIP code the table knows."));
    // Where the store is, and its rates there (the next step can still change them).
    QVariantMap taxes = adminRecords(u"taxes"_s).value(0).toMap();
    taxes.insert(u"state"_s, state);
    taxes.insert(u"zip"_s, z);
    taxes.insert(u"taxWhere"_s, QString());
    taxes.insert(u"useSuggested"_s, true);
    return adminSave(u"taxes"_s, 0, taxes);
}

bool PosService::setupTaxes(double foodPercent, double alcoholPercent)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    const QVariantMap taxes = adminRecords(u"taxes"_s).value(0).toMap();
    return adminSave(u"taxes"_s, 0, withChanges(taxes, {{u"food"_s, foodPercent}, {u"alcohol"_s, alcoholPercent}}));
}

bool PosService::setupAddItem(const QString &name, double price, const QString &family)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    if (name.trimmed().isEmpty())
        return fail(tr("Type the item's name."));
    QVariantMap item = adminNewRecord(u"menu"_s);
    item.insert(u"name"_s, name.trimmed());
    item.insert(u"price"_s, price);
    item.insert(u"family"_s, family.trimmed().toLower());
    return adminSave(u"menu"_s, -1, item);
}

bool PosService::setupAddEmployee(const QString &name, const QString &role, const QString &pin)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    QVariantMap e = adminNewRecord(u"employees"_s);
    e.insert(u"name"_s, name.trimmed());
    e.insert(u"role"_s, role);
    e.insert(u"pin"_s, pin);
    return adminSave(u"employees"_s, -1, e);
}

bool PosService::setupAddPrinter(const QString &kind, const QString &host)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    if (kind != u"receipt" && kind != u"kitchen")
        return false;
    const QString address = host.trimmed();
    if (address.isEmpty())
        return fail(tr("Type the printer's address (like 192.168.1.50)."));
    int index = -1;
    for (int i = 0; i < int(s_->settings.printers.size()); ++i)
        if (qs(s_->settings.printers[i].id) == kind)
            index = i;
    QVariantMap record = index >= 0 ? adminRecords(u"printers"_s).value(index).toMap() : adminNewRecord(u"printers"_s);
    if (index < 0) {
        record.insert(u"id"_s, kind);
        record.insert(u"name"_s, kind == u"receipt" ? tr("Receipts") : tr("Kitchen"));
        record.insert(u"receipts"_s, kind == u"receipt");
    }
    record.insert(u"type"_s, u"network"_s);
    record.insert(u"host"_s, address.section(u':', 0, 0));
    if (const int port = address.section(u':', 1, 1).toInt(); port > 0)
        record.insert(u"port"_s, port);
    return adminSave(u"printers"_s, index, record);
}

bool PosService::setupRetireSamples()
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    // Someone must still be able to run the store: a manager of their own.
    const bool ownManager = std::ranges::any_of(s_->employees, [](const Employee &e) {
        return e.active && !e.sample && (e.role == "manager" || e.role == "admin" || e.role == "owner");
    });
    if (!ownManager)
        return fail(tr("First add yourself as a manager, with a PIN of your own."));
    // Whoever is running the guide goes last, at Finish (then logs in with their own PIN).
    int n = 0;
    for (Employee &e : s_->employees) {
        if (!e.active || !e.sample)
            continue;
        if (user() && e.id == user()->id) {
            retireMeAtFinish_ = true;
            continue;
        }
        e.active = false;
        if (s_->sink)
            s_->sink->saveEmployee(e);
        ++n;
    }
    ++s_->adminRevision;
    emit s_->staffChanged();
    emit s_->adminChanged();
    emit notice(tr("%n sample employee(s) turned off", nullptr, n));
    return true;
}

void emptyStarter(PosData &d)
{
    d.menu.clear();
    d.ingredients.clear();
    // The sample manager only: someone has to log in to set the store up.
    std::erase_if(d.employees, [](const Employee &e) { return !(e.sample && e.role == "manager"); });
    if (d.employees.size() > 1)
        d.employees.resize(1);
    PosSettings &s = d.settings;
    s.storeName = PosSettings{}.storeName;
    s.receiptHeader.clear();
    s.menuCategories.clear();
    s.modifierGroups.clear();
    s.printers.clear();
    s.labelPrinter.clear();
    s.stations.clear();
    s.vendors.clear();
    s.promotions.clear();
    s.priceChanges.clear();
    s.openingChecklist.clear();
    s.closingChecklist.clear();
    s.displaySlides.clear();
    s.loyaltyEnabled = false;
    s.tax.foodPpm = s.tax.alcoholPpm = s.tax.merchandisePpm = s.tax.roomPpm = 0;
    s.setupDone = false;
}

bool PosService::setupFinish(bool done)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    s_->settings.setupDone = done;
    s_->saveSettings();
    ++s_->adminRevision;
    emit s_->adminChanged();
    if (retireMeAtFinish_ && user()) {   // the sample manager who ran it: off now (logs out)
        retireMeAtFinish_ = false;
        const std::string me = user()->id;   // user() is gone once they're off
        for (Employee &e : s_->employees) {
            if (e.id == me) {
                e.active = false;
                if (s_->sink)
                    s_->sink->saveEmployee(e);
            }
        }
        emit notice(tr("Done. Log in with your own PIN."));
        emit s_->staffChanged();
    }
    return true;
}

} // namespace vt::app
