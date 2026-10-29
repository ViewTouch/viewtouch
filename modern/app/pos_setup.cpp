// PosService: the setup guide - a new store's name, logo, look, taxes, first
// menu items and staff, a step at a time (SetupGuide.qml).

#include "app/pos_json.hh"
#include "app/pos_service.hh"
#include "app/tax_areas.hh"

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
    return {{u"done"_s, s_->settings.setupDone}, {u"storeName"_s, qs(s_->settings.storeName)},
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
