// PosService: the setup guide - a new store's name, logo, look, taxes, first
// menu items and staff, a step at a time (SetupGuide.qml).

#include "app/pos_json.hh"
#include "app/pos_service.hh"

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
            {u"staff"_s, staff}, {u"samples"_s, samples}};
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

bool PosService::setupStore(const QString &name, const QString &receiptLines)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    if (name.trimmed().isEmpty())
        return fail(tr("Type the store's name."));
    const QVariantMap store = adminRecords(u"store"_s).value(0).toMap();
    return adminSave(u"store"_s, 0, withChanges(store, {{u"storeName"_s, name.trimmed()}, {u"receiptHeader"_s, receiptLines}}));
}

bool PosService::setupLogo(const QString &ref, bool onReceipts)
{
    if (!require(perm::Manager, tr("Setting up the store")))
        return false;
    const QVariantMap store = adminRecords(u"store"_s).value(0).toMap();
    return adminSave(u"store"_s, 0, withChanges(store, {{u"displayLogo"_s, ref}, {u"receiptLogo"_s, onReceipts}}));
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
        return e.active && !e.sample && (e.role == "manager" || e.role == "admin");
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
