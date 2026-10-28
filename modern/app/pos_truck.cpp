// PosService: how a screen serves (restaurant, food truck, retail), and the
// food truck's counter - no login, straight to ordering and paying.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

const Employee &counterEmployee()
{
    // Nothing translated in here (see maintainerEmployee); shown translated by nameOf.
    static const Employee e = [] {
        Employee k;
        k.id = "counter";
        k.name = "Counter";
        k.role = "cashier";   // orders and payments, everyone's checks; never a manager's
        return k;
    }();
    return e;
}

QString PosService::serviceMode() const
{
    std::string mode = s_->settings.serviceMode;
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_ && !t.service.empty())
            mode = t.service;
    if (mode == "foodTruck" || mode == "retail")
        return qs(mode);
    return u"restaurant"_s;
}

QVariantMap PosService::serviceInfo() const
{
    const bool truck = counterMode();
    const Employee *person = truck ? s_->employee(counterUser_) : nullptr;
    return {{u"mode"_s, serviceMode()},
            {u"counter"_s, truck && !userId_.empty() && userId_ == counterUser_},
            {u"steppedIn"_s, truck && !counterUser_.empty() && !userId_.empty() && userId_ != counterUser_},
            {u"askStaff"_s, s_->settings.truckAskStaff},
            {u"person"_s, person && person->id != counterEmployee().id ? nameOf(*person) : QString()},
            {u"needsName"_s, s_->settings.truckNeedsName}};
}

void PosService::applyServiceMode()
{
    // Called with every settings change: says so only when something did.
    const auto shown = [this] {
        if (const QVariantMap now = serviceInfo(); now != serviceShown_) {
            serviceShown_ = now;
            emit sessionChanged();
        }
    };
    if (!counterMode()) {
        // Not a food truck (any more): the Counter isn't anyone to stay
        // logged in as (a person who said they're working stays).
        counterUser_.clear();
        if (userId_ == counterEmployee().id)
            logout();
        shown();
        return;
    }
    // Someone is here already (a manager, who switched it on), or the
    // screen asks who's working: the login page does.
    if (!userId_.empty() || s_->settings.truckAskStaff) {
        shown();
        return;
    }
    counterUser_ = counterEmployee().id;
    userId_ = counterUser_;
    serviceShown_ = serviceInfo();
    emit sessionChanged();
    emit loggedInChanged(true);
}

bool PosService::switchCounterPerson()
{
    if (!counterMode())
        return fail(tr("This screen isn't a food truck's."));
    // Who was working logs out (the order in progress waits for them);
    // the next PIN is whoever works now.
    if (!counterUser_.empty() && userId_ != counterUser_)
        logout();   // back to the counter first
    counterUser_.clear();
    logout();
    if (s_->settings.truckAskStaff)
        emit notice(tr("Who's working? Enter your PIN."));
    return true;
}

int PosService::nextOrderNumber() const
{
    // From 1 each business day: the day's checks (open ones too) say how far it is.
    int last = 0;
    for (const auto &[id, c] : s_->open)
        last = std::max(last, c.orderNumber);
    for (const Check &c : s_->closedToday)
        last = std::max(last, c.orderNumber);
    return last + 1;
}

QString PosService::orderLabel(const Check &c) const
{
    const QString name = qs(c.customer.name).left(20);
    return name.isEmpty() ? u"#%1"_s.arg(c.orderNumber) : u"#%1 %2"_s.arg(c.orderNumber).arg(name);
}

} // namespace vt::app
