// PosService: the self-order kiosk - guests order on their own, then pay at
// the counter (or the order goes straight to the kitchen).

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QFile>
#include <QUrl>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

const Employee &kioskEmployee()
{
    static const Employee e = [] {
        Employee k;
        k.id = "kiosk";
        k.name = "Self-order kiosk";
        k.role = "kiosk";
        return k;
    }();
    return e;
}

namespace {

// What guests may order on their own: not modifiers (they come as choices),
// not what the store keeps off the kiosk, and never alcohol (ID check).
bool onKiosk(const MenuItem &m)
{
    return !m.isModifier && !m.kioskHide && !m.byWeight && m.taxClass != TaxClass::Alcohol;   // no scale there
}

} // namespace

void PosService::enableSelfOrder()
{
    if (selfOrder_)
        return;
    releaseCheck();
    selfOrder_ = true;
    userId_ = kioskEmployee().id;
    trainingOn_ = false;
    lastKioskOrder_.clear();
    emit sessionChanged();
    emit loggedInChanged(true);
    emit checkChanged();
    emit adminChanged();   // the kiosk's menu
}

bool PosService::setSelfOrder(bool on)
{
    if (!on)
        return fail(tr("A manager's PIN ends self-order mode."));
    if (!require(perm::Manager, tr("Self-order kiosk")))
        return false;
    if (current())
        return fail(tr("Close or put away the check first."));
    enableSelfOrder();
    return true;
}

bool PosService::leaveSelfOrder(const QString &managerPin)
{
    if (!selfOrder_)
        return true;
    const Employee *m = employeeByPin(managerPin);
    if (!m || !m->can(perm::Manager))
        return fail(pinRefused(tr("That isn't a manager's PIN.")));
    kioskCancel();
    selfOrder_ = false;
    userId_ = m->id;   // the manager, logged in here now
    lastKioskOrder_.clear();
    emit sessionChanged();
    emit checkChanged();
    emit adminChanged();
    emit notice(tr("Self-order kiosk off"));
    return true;
}

bool PosService::kioskStart(bool toGo)
{
    if (!selfOrder_)
        return fail(tr("This screen is not a self-order kiosk."));
    if (current())
        kioskCancel();
    lastKioskOrder_.clear();
    kioskToGo_ = toGo;
    if (!startCheck(toGo ? CheckType::Takeout : CheckType::Quick))
        return false;
    Check &c = *current();
    c.kiosk = true;
    c.label = ss(tr("Kiosk %1").arg(c.id));
    changed(c);
    return true;
}

bool PosService::kioskAdd(const QString &itemId)
{
    if (!selfOrder_)
        return fail(tr("This screen is not a self-order kiosk."));
    const MenuItem *item = findItem(itemId);
    if (!item || !onKiosk(*item))
        return fail(tr("'%1' is not on the menu.").arg(itemId));
    if (!current() && !kioskStart(kioskToGo_))
        return false;
    return addItem(itemId);
}

bool PosService::kioskRemove(qint64 lineId)
{
    Check *c = current();
    if (!selfOrder_ || !c)
        return fail(tr("No check is open."));
    const OrderLine *l = c->line(lineId);
    if (!l || l->sent)
        return fail(tr("Only items not yet sent can be changed."));
    if (choosingLine_ == lineId)
        choosingLine_ = 0;
    c->removeLine(lineId);
    selectedLine_ = 0;
    changed(*c);
    return true;
}

bool PosService::kioskFinish(const QVariantMap &guest)
{
    Check *c = current();
    if (!selfOrder_ || !c || c->lines.empty())
        return fail(tr("Add something to your order first."));
    const QString name = guest.value(u"name"_s).toString().trimmed();
    if (name.isEmpty() && s_->settings.kioskLook.askName)   // otherwise called by number
        return fail(tr("Type a name so we can call your order."));
    if (const QString missing = missingChoice(c->lines); !missing.isEmpty())
        return fail(missing);
    c->customer.name = ss(name.left(40));
    c->customer.phone = ss(guest.value(u"phone"_s).toString().trimmed().left(20));
    // What the guest avoided (Allergies): the kitchen sees it as an allergy.
    const QStringList avoid = guest.value(u"avoid"_s).toStringList();
    c->allergies.clear();
    for (const std::string &a : allergenIds())
        if (avoid.contains(qs(a)))
            c->allergies.push_back(a);
    changed(*c);
    const bool sendNow = s_->settings.kioskSendNow;
    if (sendNow && !sendOrder())
        return false;
    const Totals t = c->totals(s_->settings.tax);
    // Their slip: the number to show at the counter, and what they ordered.
    const bool slip = s_->settings.kioskSlip && s_->printer && s_->settings.printer(receiptPrinter());
    if (slip)
        s_->printer->printOrderSlip(s_->settings, *c, receiptPrinter(), sendNow);
    lastKioskOrder_ = {{u"number"_s, qint64(c->id)}, {u"name"_s, name}, {u"sent"_s, sendNow},
                       {u"total"_s, format(t.total)}, {u"slip"_s, slip}};
    choosingLine_ = 0;
    releaseCheck();   // for the counter now
    emit checkChanged();
    return true;
}

void PosService::kioskCancel()
{
    if (!selfOrder_)
        return;   // a kiosk's own: never a check staff have open
    Check *c = current();
    if (c && c->kiosk) {
        // Nothing of it was sent: it was never really an order.
        std::vector<std::int64_t> unsent;
        for (const OrderLine &l : c->lines)
            if (!l.sent)
                unsent.push_back(l.id);
        for (std::int64_t id : unsent)
            c->removeLine(id);
    }
    choosingLine_ = 0;
    releaseCheck();
    lastKioskOrder_.clear();
    emit checkChanged();
}

QVariantMap PosService::selfOrderInfo() const
{
    if (!selfOrder_)
        return {{u"on"_s, false}};
    const Check *c = currentCheck();
    return {{u"on"_s, true}, {u"ordering"_s, c != nullptr}, {u"toGo"_s, kioskToGo_},
            {u"idleSeconds"_s, s_->settings.kioskIdleSeconds}, {u"lastOrder"_s, lastKioskOrder_},
            {u"items"_s, c ? qint64(c->lines.size()) : 0},
            {u"look"_s, QVariantMap{{u"background"_s, qs(s_->settings.kioskLook.background)},
                                    {u"card"_s, qs(s_->settings.kioskLook.card)}, {u"go"_s, qs(s_->settings.kioskLook.go)},
                                    {u"text"_s, qs(s_->settings.kioskLook.text)}, {u"font"_s, qs(s_->settings.kioskLook.font)},
                                    {u"welcome"_s, qs(s_->settings.kioskLook.welcome)},
                                    {u"size"_s, s_->settings.kioskLook.sizePercent / 100.0},
                                    {u"askWhere"_s, s_->settings.kioskLook.askWhere},
                                    {u"askName"_s, s_->settings.kioskLook.askName},
                                    {u"easyReach"_s, s_->settings.kioskLook.easyReach}}}};
}

QString PosService::storeImage(const QString &path) const
{
    // The store's pictures, by name.
    if (path.startsWith(u"store:")) {
        const auto it = s_->images.find(ss(path.mid(6)));
        return it == s_->images.end() ? QString() : QString::fromLatin1(it->second.toBase64());
    }
    // Only the pictures the store has set up: never any other file.
    const std::string p = ss(path);
    bool known = !p.empty() && (p == s_->settings.displayLogo);
    for (const MenuItem &m : s_->menu)
        known = known || m.image == p;
    for (const std::string &slide : s_->settings.displaySlides)
        known = known || slide == "image:" + p;
    if (!known)
        return {};
    QFile f(path);
    constexpr qint64 kMaxBytes = 8 * 1024 * 1024;
    if (f.size() > kMaxBytes || !f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(f.readAll().toBase64());
}

QVariantMap PosService::kioskMenu() const
{
    if (!selfOrder_)
        return {};
    const std::string period = currentMealPeriod();
    QStringList families;
    QVariantList items;
    for (const MenuItem &m : s_->menu) {
        if (!onKiosk(m))
            continue;
        const QString family = m.family.empty() ? tr("Menu") : qs(m.family);
        if (!families.contains(family))
            families << family;
        items.append(QVariantMap{
            {u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"family"_s, family},
            {u"price"_s, format(m.priceFor(period, kioskToGo_, false))}, {u"description"_s, qs(m.description)},
            {u"image"_s, qs(m.image)},   // a ref: each screen shows its own copy (imageUrl)
            {u"available"_s, m.available && ticketsLeft(m) != 0}, {u"choices"_s, !m.modifierGroups.empty()},
            {u"left"_s, ticketsLeft(m)},
            {u"allergens"_s, [&m] { QStringList a; for (const std::string &x : m.allergens) a << qs(x); return a; }()},
            {u"contains"_s, allergenNames(m.allergens)}});
    }
    return {{u"families"_s, families}, {u"items"_s, items}};
}

} // namespace vt::app
