#include "app/pos_json.hh"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {
QJsonArray strings(const std::vector<std::string> &v);
std::vector<std::string> strings(const QJsonArray &a);
} // namespace

QString qs(const std::string &s) { return QString::fromStdString(s); }
std::string ss(const QString &s) { return s.toStdString(); }

std::int64_t centsFromDecimal(double value) { return std::llround(value * 100.0); }
std::int64_t ppmFromPercent(double percent) { return std::llround(percent * 10000.0); }
double decimalFromCents(std::int64_t cents) { return double(cents) / 100.0; }
double percentFromPpm(std::int64_t ppm) { return double(ppm) / 10000.0; }

namespace {

qint64 i64(const QJsonValue &v) { return v.toInteger(); }
Money money(const QJsonValue &v) { return Money::fromCents(v.toInteger()); }

} // namespace

// --- checks --------------------------------------------------------------------

QJsonObject toJson(const Refund &r)
{
    return {{u"id"_s, qint64(r.id)}, {u"at"_s, qint64(r.at)}, {u"paymentId"_s, qint64(r.paymentId)},
            {u"amount"_s, qint64(r.amount.cents())}, {u"reason"_s, qs(r.reason)}, {u"by"_s, qs(r.by)},
            {u"reference"_s, qs(r.reference)}, {u"tenderName"_s, qs(r.tenderName)}, {u"day"_s, qint64(r.day)},
            {u"checkId"_s, qint64(r.checkId)}, {u"checkLabel"_s, qs(r.checkLabel)}, {u"method"_s, qs(r.method)}};
}

Refund refundFromJson(const QJsonObject &o)
{
    Refund r;
    r.id = i64(o.value(u"id"));
    r.at = i64(o.value(u"at"));
    r.paymentId = i64(o.value(u"paymentId"));
    r.amount = money(o.value(u"amount"));
    r.reason = ss(o.value(u"reason").toString());
    r.by = ss(o.value(u"by").toString());
    r.reference = ss(o.value(u"reference").toString());
    r.tenderName = ss(o.value(u"tenderName").toString());
    r.day = i64(o.value(u"day"));
    r.checkId = i64(o.value(u"checkId"));
    r.checkLabel = ss(o.value(u"checkLabel").toString());
    r.method = ss(o.value(u"method").toString());
    return r;
}

QJsonObject toJson(const Check &c)
{
    QJsonArray lines;
    for (const OrderLine &l : c.lines) {
        QJsonArray mods;
        for (const Modifier &m : l.modifiers) {
            QJsonObject mo{{u"itemId"_s, qs(m.itemId)}, {u"name"_s, qs(m.name)}, {u"unitPrice"_s, qint64(m.unitPrice.cents())}};
            if (m.qualifier != Qualifier::None)
                mo.insert(u"qualifier"_s, qs(toString(m.qualifier)));
            if (!m.group.empty())
                mo.insert(u"group"_s, qs(m.group));
            if (!m.kitchenName.empty())
                mo.insert(u"kitchenName"_s, qs(m.kitchenName));
            if (m.kitchenHide)
                mo.insert(u"kitchenHide"_s, true);
            if (!m.station.empty())
                mo.insert(u"station"_s, qs(m.station));
            if (m.made) {
                mo.insert(u"made"_s, true);
                mo.insert(u"madeAt"_s, qint64(m.madeAt));
            }
            mods.append(mo);
        }
        QJsonObject lo{
            {u"id"_s, qint64(l.id)}, {u"itemId"_s, qs(l.itemId)}, {u"name"_s, qs(l.name)},
            {u"unitPrice"_s, qint64(l.unitPrice.cents())}, {u"quantity"_s, l.quantity},
            {u"taxClass"_s, qs(toString(l.taxClass))}, {u"sent"_s, l.sent}, {u"voided"_s, l.voided},
            {u"sentAt"_s, qint64(l.sentAt)},
        };
        if (l.qualifier != Qualifier::None)
            lo.insert(u"qualifier"_s, qs(toString(l.qualifier)));
        if (!mods.isEmpty())
            lo.insert(u"modifiers"_s, mods);
        if (!l.printer.empty())
            lo.insert(u"printer"_s, qs(l.printer));
        if (!l.station.empty())
            lo.insert(u"station"_s, qs(l.station));
        if (l.weight > 0) {
            lo.insert(u"weight"_s, qint64(l.weight));
            lo.insert(u"weightUnit"_s, qs(l.weightUnit));
        }
        if (l.made) {
            lo.insert(u"made"_s, true);
            lo.insert(u"madeAt"_s, qint64(l.madeAt));
        }
        if (l.seat != 0)
            lo.insert(u"seat"_s, l.seat);
        if (l.course != 1)
            lo.insert(u"course"_s, l.course);
        if (!l.kitchenName.empty())
            lo.insert(u"kitchenName"_s, qs(l.kitchenName));
        if (!l.kitchenColor.empty())
            lo.insert(u"kitchenColor"_s, qs(l.kitchenColor));
        if (!l.allergens.empty())
            lo.insert(u"allergens"_s, strings(l.allergens));
        if (l.kitchenHide)
            lo.insert(u"kitchenHide"_s, true);
        if (l.noDiscount)
            lo.insert(u"noDiscount"_s, true);
        if (l.noStaffDiscount)
            lo.insert(u"noStaffDiscount"_s, true);
        if (l.served) {
            lo.insert(u"served"_s, true);
            lo.insert(u"servedAt"_s, qint64(l.servedAt));
        }
        if (l.isShare()) {
            lo.insert(u"share"_s, l.share);
            lo.insert(u"shareOf"_s, l.shareOf);
            lo.insert(u"shareGroup"_s, qint64(l.shareGroup));
        }
        lines.append(lo);
    }
    QJsonArray payments;
    for (const Payment &p : c.payments) {
        payments.append(QJsonObject{
            {u"id"_s, qint64(p.id)}, {u"tenderId"_s, qs(p.tenderId)}, {u"tenderName"_s, qs(p.tenderName)},
            {u"kind"_s, qs(toString(p.kind))}, {u"amount"_s, qint64(p.amount.cents())}, {u"percentBp"_s, qint64(p.percentBp)},
            {u"tip"_s, qint64(p.tip.cents())}, {u"reference"_s, qs(p.reference)}, {u"staffMeal"_s, p.staffMeal},
            {u"processor"_s, qs(p.processor)}, {u"cardBrand"_s, qs(p.cardBrand)}, {u"last4"_s, qs(p.last4)},
            {u"offline"_s, qs(p.offline)},
        });
    }
    QJsonArray events;
    for (const CheckEvent &e : c.events) {
        QJsonObject eo{{u"at"_s, qint64(e.at)}, {u"who"_s, qs(e.who)}, {u"what"_s, qs(e.what)}, {u"kind"_s, qs(e.kind)}};
        if (e.amount.cents() != 0)
            eo.insert(u"amount"_s, qint64(e.amount.cents()));
        if (!e.reason.empty())
            eo.insert(u"reason"_s, qs(e.reason));
        events.append(eo);
    }
    return {
        {u"schemaVersion"_s, PosSchemaVersion},
        {u"id"_s, qint64(c.id)}, {u"type"_s, qs(toString(c.type))}, {u"status"_s, qs(toString(c.status))},
        {u"label"_s, qs(c.label)}, {u"guests"_s, c.guests},
        {u"serverId"_s, qs(c.serverId)}, {u"serverName"_s, qs(c.serverName)},
        {u"openedAt"_s, qint64(c.openedAt)}, {u"closedAt"_s, qint64(c.closedAt)},
        {u"lines"_s, lines}, {u"payments"_s, payments},
        {u"nextLineId"_s, qint64(c.nextLineId)}, {u"nextPaymentId"_s, qint64(c.nextPaymentId)},
        {u"businessDay"_s, qint64(c.businessDay)}, {u"drawerSession"_s, qint64(c.drawerSession)},
        {u"gratuityBp"_s, qint64(c.gratuityBp)}, {u"autoGratuity"_s, c.autoGratuity},
        {u"customer"_s, QJsonObject{{u"name"_s, qs(c.customer.name)}, {u"phone"_s, qs(c.customer.phone)},
                                    {u"address"_s, qs(c.customer.address)}, {u"note"_s, qs(c.customer.note)}}},
        {u"events"_s, events}, {u"firedCourse"_s, c.firedCourse}, {u"customerId"_s, qs(c.customerId)},
        {u"rush"_s, c.rush}, {u"vip"_s, c.vip}, {u"allergies"_s, strings(c.allergies)}, {u"coupons"_s, strings(c.coupons)}, {u"hold"_s, [&c] {
            if (c.hold.empty())
                return QJsonValue();
            return QJsonValue(QJsonObject{{u"reference"_s, qs(c.hold.reference)}, {u"processor"_s, qs(c.hold.processor)},
                                          {u"brand"_s, qs(c.hold.cardBrand)}, {u"last4"_s, qs(c.hold.last4)},
                                          {u"amount"_s, qint64(c.hold.amount.cents())}, {u"at"_s, qint64(c.hold.at)}});
        }()}, {u"kiosk"_s, c.kiosk}, {u"dueAt"_s, qint64(c.dueAt)}, {u"fireAt"_s, qint64(c.fireAt)},
        {u"promisedAt"_s, qint64(c.promisedAt)},
        {u"refunds"_s, [&] {
             QJsonArray a;
             for (const Refund &r : c.refunds)
                 a.append(toJson(r));
             return a;
         }()}, {u"driverId"_s, qs(c.driverId)}, {u"driverName"_s, qs(c.driverName)},
        {u"outAt"_s, qint64(c.outAt)}, {u"deliveredAt"_s, qint64(c.deliveredAt)}, {u"pointsEarned"_s, c.pointsEarned}, {u"training"_s, c.training},
    };
}

std::optional<Check> checkFromJson(const QJsonObject &o)
{
    if (o.value(u"schemaVersion").toInt(PosSchemaVersion) > PosSchemaVersion || !o.contains(u"id"))
        return std::nullopt;
    Check c;
    c.id = i64(o.value(u"id"));
    c.type = checkTypeFromString(ss(o.value(u"type").toString()));
    c.status = checkStatusFromString(ss(o.value(u"status").toString()));
    c.label = ss(o.value(u"label").toString());
    c.guests = o.value(u"guests").toInt(1);
    c.serverId = ss(o.value(u"serverId").toString());
    c.serverName = ss(o.value(u"serverName").toString());
    c.openedAt = i64(o.value(u"openedAt"));
    c.closedAt = i64(o.value(u"closedAt"));
    for (const QJsonValue &v : o.value(u"lines").toArray()) {
        const QJsonObject lo = v.toObject();
        OrderLine l;
        l.id = i64(lo.value(u"id"));
        l.itemId = ss(lo.value(u"itemId").toString());
        l.name = ss(lo.value(u"name").toString());
        l.unitPrice = money(lo.value(u"unitPrice"));
        l.quantity = lo.value(u"quantity").toInt(1);
        l.taxClass = taxClassFromString(ss(lo.value(u"taxClass").toString()));
        l.qualifier = qualifierFromString(ss(lo.value(u"qualifier").toString()));
        l.printer = ss(lo.value(u"printer").toString());
        l.station = ss(lo.value(u"station").toString());
        l.weight = i64(lo.value(u"weight"));
        l.weightUnit = ss(lo.value(u"weightUnit").toString());
        l.sent = lo.value(u"sent").toBool();
        l.voided = lo.value(u"voided").toBool();
        l.sentAt = i64(lo.value(u"sentAt"));
        l.made = lo.value(u"made").toBool();
        l.madeAt = i64(lo.value(u"madeAt"));
        l.seat = lo.value(u"seat").toInt(0);
        l.course = std::max(1, lo.value(u"course").toInt(1));
        l.kitchenName = ss(lo.value(u"kitchenName").toString());
        l.kitchenColor = ss(lo.value(u"kitchenColor").toString());
        l.allergens = strings(lo.value(u"allergens").toArray());
        l.kitchenHide = lo.value(u"kitchenHide").toBool();
        l.noDiscount = lo.value(u"noDiscount").toBool();
        l.noStaffDiscount = lo.value(u"noStaffDiscount").toBool();
        l.served = lo.value(u"served").toBool();
        l.servedAt = i64(lo.value(u"servedAt"));
        l.share = lo.value(u"share").toInt();
        l.shareOf = lo.value(u"shareOf").toInt();
        l.shareGroup = i64(lo.value(u"shareGroup"));
        for (const QJsonValue &mv : lo.value(u"modifiers").toArray()) {
            const QJsonObject mo = mv.toObject();
            l.modifiers.push_back({ss(mo.value(u"itemId").toString()), ss(mo.value(u"name").toString()),
                                   money(mo.value(u"unitPrice")),
                                   qualifierFromString(ss(mo.value(u"qualifier").toString())),
                                   ss(mo.value(u"group").toString()), ss(mo.value(u"kitchenName").toString()),
                                   mo.value(u"kitchenHide").toBool(), ss(mo.value(u"station").toString()),
                                   mo.value(u"made").toBool(), i64(mo.value(u"madeAt"))});
        }
        c.lines.push_back(l);
    }
    for (const QJsonValue &v : o.value(u"payments").toArray()) {
        const QJsonObject po = v.toObject();
        Payment p;
        p.id = i64(po.value(u"id"));
        p.tenderId = ss(po.value(u"tenderId").toString());
        p.tenderName = ss(po.value(u"tenderName").toString());
        p.kind = tenderKindFromString(ss(po.value(u"kind").toString()));
        p.amount = money(po.value(u"amount"));
        p.percentBp = i64(po.value(u"percentBp"));
        p.tip = money(po.value(u"tip"));
        p.reference = ss(po.value(u"reference").toString());
        p.staffMeal = po.value(u"staffMeal").toBool();
        p.processor = ss(po.value(u"processor").toString());
        p.cardBrand = ss(po.value(u"cardBrand").toString());
        p.last4 = ss(po.value(u"last4").toString());
        p.offline = ss(po.value(u"offline").toString());
        c.payments.push_back(p);
    }
    c.nextLineId = std::max<std::int64_t>(i64(o.value(u"nextLineId")), 1);
    c.nextPaymentId = std::max<std::int64_t>(i64(o.value(u"nextPaymentId")), 1);
    c.businessDay = i64(o.value(u"businessDay"));
    c.drawerSession = i64(o.value(u"drawerSession"));
    c.gratuityBp = i64(o.value(u"gratuityBp"));
    c.firedCourse = std::max(1, o.value(u"firedCourse").toInt(1));
    c.autoGratuity = o.value(u"autoGratuity").toBool();
    const QJsonObject cust = o.value(u"customer").toObject();
    c.customer = {ss(cust.value(u"name").toString()), ss(cust.value(u"phone").toString()),
                  ss(cust.value(u"address").toString()), ss(cust.value(u"note").toString())};
    c.customerId = ss(o.value(u"customerId").toString());
    c.rush = o.value(u"rush").toBool();
    c.kiosk = o.value(u"kiosk").toBool();
    c.dueAt = o.value(u"dueAt").toInteger(0);
    c.fireAt = o.value(u"fireAt").toInteger(0);
    c.promisedAt = o.value(u"promisedAt").toInteger(0);
    for (const QJsonValue &v : o.value(u"refunds").toArray())
        c.refunds.push_back(refundFromJson(v.toObject()));
    c.driverId = ss(o.value(u"driverId").toString());
    c.driverName = ss(o.value(u"driverName").toString());
    c.outAt = o.value(u"outAt").toInteger(0);
    c.deliveredAt = o.value(u"deliveredAt").toInteger(0);
    c.vip = o.value(u"vip").toBool();
    c.allergies = strings(o.value(u"allergies").toArray());
    c.coupons = strings(o.value(u"coupons").toArray());
    if (const QJsonObject h = o.value(u"hold").toObject(); !h.isEmpty()) {
        c.hold.reference = ss(h.value(u"reference").toString());
        c.hold.processor = ss(h.value(u"processor").toString());
        c.hold.cardBrand = ss(h.value(u"brand").toString());
        c.hold.last4 = ss(h.value(u"last4").toString());
        c.hold.amount = Money::fromCents(h.value(u"amount").toInteger());
        c.hold.at = h.value(u"at").toInteger();
    }
    c.pointsEarned = o.value(u"pointsEarned").toInt();
    c.training = o.value(u"training").toBool();
    for (const QJsonValue &v : o.value(u"events").toArray()) {
        const QJsonObject e = v.toObject();
        c.events.push_back({i64(e.value(u"at")), ss(e.value(u"who").toString()), ss(e.value(u"what").toString()),
                            ss(e.value(u"kind").toString()), Money::fromCents(i64(e.value(u"amount"))),
                            ss(e.value(u"reason").toString())});
    }
    return c;
}

// --- menu ------------------------------------------------------------------------

QJsonObject toJson(const MenuItem &m)
{
    QJsonObject o{
        {u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"price"_s, decimalFromCents(m.price.cents())},
        {u"taxClass"_s, qs(toString(m.taxClass))},
    };
    if (!m.family.empty()) o.insert(u"family"_s, qs(m.family));
    if (!m.number.empty()) o.insert(u"number"_s, qs(m.number));
    if (!m.barcodes.empty()) o.insert(u"barcodes"_s, strings(m.barcodes));
    if (m.prepMinutes > 0) o.insert(u"prepMinutes"_s, m.prepMinutes);
    if (!m.buttonColor.empty()) o.insert(u"buttonColor"_s, qs(m.buttonColor));
    if (!m.section.empty()) o.insert(u"section"_s, qs(m.section));
    if (!m.breakBefore.empty()) o.insert(u"breakBefore"_s, qs(m.breakBefore));
    if (m.isModifier) o.insert(u"modifier"_s, true);
    if (!m.printer.empty()) o.insert(u"printer"_s, qs(m.printer));
    if (!m.station.empty()) o.insert(u"station"_s, qs(m.station));
    if (m.ticketCapacity > 0) {
        o.insert(u"ticketCapacity"_s, m.ticketCapacity);
        o.insert(u"eventAt"_s, qint64(m.eventAt));
        o.insert(u"ticketsSoldBefore"_s, m.ticketsSoldBefore);
    }
    if (m.substitute) {
        o.insert(u"substitute"_s, true);
        o.insert(u"substitutePrice"_s, decimalFromCents(m.substitutePrice.cents()));
    }
    if (m.byWeight) {
        o.insert(u"byWeight"_s, true);
        o.insert(u"weightUnit"_s, qs(m.weightUnit));
    }
    if (!m.available) o.insert(u"available"_s, false);
    if (!m.modifierGroups.empty()) {
        QJsonArray groups;
        for (const std::string &g : m.modifierGroups)
            groups.append(qs(g));
        o.insert(u"modifierGroups"_s, groups);
    }
    if (!m.periodPrices.empty()) {
        QJsonObject prices;
        for (const auto &[period, price] : m.periodPrices)
            prices.insert(qs(period), decimalFromCents(price.cents()));
        o.insert(u"periodPrices"_s, prices);
    }
    if (!m.recipe.empty()) {
        QJsonArray recipe;
        for (const RecipeLine &r : m.recipe)
            recipe.append(QJsonObject{{u"ingredient"_s, qs(r.ingredientId)}, {u"qty"_s, r.quantity}});
        o.insert(u"recipe"_s, recipe);
    }
    if (m.autoSoldOut)
        o.insert(u"autoSoldOut"_s, true);
    if (m.soldOutToday)
        o.insert(u"soldOutToday"_s, true);
    if (!m.kitchenName.empty())
        o.insert(u"kitchenName"_s, qs(m.kitchenName));
    if (!m.kitchenColor.empty())
        o.insert(u"kitchenColor"_s, qs(m.kitchenColor));
    if (m.kitchenHide)
        o.insert(u"kitchenHide"_s, true);
    if (!m.description.empty())
        o.insert(u"description"_s, qs(m.description));
    if (m.takeoutPrice.cents() > 0)
        o.insert(u"takeoutPrice"_s, decimalFromCents(m.takeoutPrice.cents()));
    if (m.deliveryPrice.cents() > 0)
        o.insert(u"deliveryPrice"_s, decimalFromCents(m.deliveryPrice.cents()));
    if (m.noDiscount)
        o.insert(u"noDiscount"_s, true);
    if (m.noStaffDiscount)
        o.insert(u"noStaffDiscount"_s, true);
    if (!m.image.empty())
        o.insert(u"image"_s, qs(m.image));
    if (m.kioskHide)
        o.insert(u"kioskHide"_s, true);
    if (!m.allergens.empty())
        o.insert(u"allergens"_s, strings(m.allergens));
    if (m.favorite)
        o.insert(u"favorite"_s, true);
    return o;
}

MenuItem menuItemFromJson(const QJsonObject &o)
{
    MenuItem m;
    m.id = ss(o.value(u"id").toString());
    m.name = ss(o.value(u"name").toString());
    if (m.name.empty())
        m.name = m.id;
    m.family = ss(o.value(u"family").toString());
    m.number = ss(o.value(u"number").toVariant().toString().trimmed());
    m.barcodes = strings(o.value(u"barcodes").toArray());
    m.prepMinutes = std::clamp(o.value(u"prepMinutes").toVariant().toInt(), 0, 240);
    m.buttonColor = ss(o.value(u"buttonColor").toString());
    m.section = ss(o.value(u"section").toString());
    m.breakBefore = ss(o.value(u"breakBefore").toString());
    m.price = Money::fromCents(centsFromDecimal(o.value(u"price").toDouble()));
    m.taxClass = taxClassFromString(ss(o.value(u"taxClass").toString(u"food"_s)));
    m.isModifier = o.value(u"modifier").toBool();
    m.printer = ss(o.value(u"printer").toString());
    m.station = ss(o.value(u"station").toString());
    m.byWeight = o.value(u"byWeight").toBool();
    m.substitute = o.value(u"substitute").toBool();
    m.ticketCapacity = std::max(0, o.value(u"ticketCapacity").toInt());
    m.eventAt = o.value(u"eventAt").toInteger(0);
    m.ticketsSoldBefore = std::max(0, o.value(u"ticketsSoldBefore").toInt());
    m.substitutePrice = Money::fromCents(centsFromDecimal(o.value(u"substitutePrice").toDouble()));
    m.weightUnit = ss(o.value(u"weightUnit").toString(u"lb"_s));
    m.available = o.value(u"available").toBool(true);
    for (const QJsonValue &g : o.value(u"modifierGroups").toArray())
        m.modifierGroups.push_back(ss(g.toString()));
    const QJsonObject prices = o.value(u"periodPrices").toObject();
    for (auto it = prices.begin(); it != prices.end(); ++it)
        m.periodPrices[ss(it.key())] = Money::fromCents(centsFromDecimal(it.value().toDouble()));
    for (const QJsonValue &v : o.value(u"recipe").toArray()) {
        const QJsonObject r = v.toObject();
        m.recipe.push_back({ss(r.value(u"ingredient").toString()), r.value(u"qty").toDouble(1)});
    }
    m.autoSoldOut = o.value(u"autoSoldOut").toBool();
    m.soldOutToday = o.value(u"soldOutToday").toBool();
    m.kitchenName = ss(o.value(u"kitchenName").toString().trimmed());
    m.kitchenColor = ss(o.value(u"kitchenColor").toString());
    m.kitchenHide = o.value(u"kitchenHide").toBool();
    m.description = ss(o.value(u"description").toString().trimmed());
    m.takeoutPrice = Money::fromCents(centsFromDecimal(o.value(u"takeoutPrice").toDouble()));
    m.deliveryPrice = Money::fromCents(centsFromDecimal(o.value(u"deliveryPrice").toDouble()));
    m.noDiscount = o.value(u"noDiscount").toBool();
    m.noStaffDiscount = o.value(u"noStaffDiscount").toBool();
    m.image = ss(o.value(u"image").toString().trimmed());
    m.kioskHide = o.value(u"kioskHide").toBool();
    m.allergens = strings(o.value(u"allergens").toArray());
    m.favorite = o.value(u"favorite").toBool();
    return m;
}

std::vector<MenuItem> menuFromJson(const QJsonArray &a)
{
    std::vector<MenuItem> out;
    for (const QJsonValue &v : a)
        out.push_back(menuItemFromJson(v.toObject()));
    return out;
}

// --- employees ---------------------------------------------------------------------

std::string newSalt()
{
    quint32 words[4];
    QRandomGenerator::system()->fillRange(words);
    return QByteArray(reinterpret_cast<const char *>(words), sizeof words).toHex().toStdString();
}

std::string hashPin(const QString &pin, const std::string &salt)
{
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(QByteArray::fromStdString(salt));
    h.addData(pin.toUtf8());
    return h.result().toHex().toStdString();
}

namespace {
QJsonArray strings(const std::set<std::string> &set)
{
    QJsonArray out;
    for (const std::string &s : set)
        out.append(qs(s));
    return out;
}
} // namespace

QJsonObject toJson(const Employee &e)
{
    return {
        {u"id"_s, qs(e.id)}, {u"name"_s, qs(e.name)}, {u"role"_s, qs(e.role)},
        {u"pinSalt"_s, qs(e.pinSalt)}, {u"pinHash"_s, qs(e.pinHash)}, {u"active"_s, e.active}, {u"training"_s, e.training}, {u"sample"_s, e.sample},
        {u"cashMode"_s, qs(e.cashMode)}, {u"requireName"_s, qs(e.requireName)}, {u"checkout"_s, qs(e.checkout)}, {u"blindCount"_s, qs(e.blindCount)}, {u"reasons"_s, qs(e.reasons)}, {u"tipPool"_s, qs(e.tipPool)}, {u"language"_s, qs(e.language)},
        {u"textSize"_s, e.textSize}, {u"leftHanded"_s, e.leftHanded}, {u"startPage"_s, qs(e.startPage)},
        {u"payRate"_s, e.payRate.cents() / 100.0}, {u"otherJobs"_s, [&] {
             QJsonArray jobs;
             for (const Job &job : e.otherJobs)
                 jobs.append(QJsonObject{{u"role"_s, qs(job.role)}, {u"rate"_s, job.rate.cents() / 100.0}});
             return jobs;
         }()},
        {u"allow"_s, strings(e.allow)}, {u"deny"_s, strings(e.deny)},
    };
}

Employee employeeFromJson(const QJsonObject &o)
{
    Employee e;
    e.id = ss(o.value(u"id").toString());
    e.name = ss(o.value(u"name").toString());
    e.role = ss(o.value(u"role").toString(u"server"_s));
    e.active = o.value(u"active").toBool(true);
    e.training = o.value(u"training").toBool(false);
    e.sample = o.value(u"sample").toBool(false);
    e.cashMode = ss(o.value(u"cashMode").toString());
    e.requireName = ss(o.value(u"requireName").toString());
    e.checkout = ss(o.value(u"checkout").toString());
    e.blindCount = ss(o.value(u"blindCount").toString());
    e.reasons = ss(o.value(u"reasons").toString());
    e.tipPool = ss(o.value(u"tipPool").toString());
    e.language = ss(o.value(u"language").toString());
    e.textSize = std::clamp(o.value(u"textSize").toInt(100), 80, 160);
    e.leftHanded = o.value(u"leftHanded").toBool(false);
    e.startPage = ss(o.value(u"startPage").toString());
    e.payRate = Money::fromCents(centsFromDecimal(o.value(u"payRate").toDouble()));
    for (const QJsonValue &v : o.value(u"otherJobs").toArray())
        e.otherJobs.push_back({ss(v.toObject().value(u"role").toString()),
                               Money::fromCents(centsFromDecimal(v.toObject().value(u"rate").toDouble()))});
    for (const QJsonValue &v : o.value(u"allow").toArray())
        e.allow.insert(ss(v.toString()));
    for (const QJsonValue &v : o.value(u"deny").toArray())
        e.deny.insert(ss(v.toString()));
    if (o.contains(u"pin")) {
        e.pinSalt = newSalt();
        e.pinHash = hashPin(o.value(u"pin").toString(), e.pinSalt);
    } else {
        e.pinSalt = ss(o.value(u"pinSalt").toString());
        e.pinHash = ss(o.value(u"pinHash").toString());
    }
    return e;
}

std::vector<Employee> employeesFromJson(const QJsonArray &a)
{
    std::vector<Employee> out;
    for (const QJsonValue &v : a)
        out.push_back(employeeFromJson(v.toObject()));
    return out;
}

// --- punches -------------------------------------------------------------------------

QJsonObject toJson(const TimePunch &p)
{
    QJsonArray breaks;
    for (const TimePunch::Break &b : p.breaks)
        breaks.append(QJsonObject{{u"start"_s, qint64(b.start)}, {u"end"_s, qint64(b.end)}});
    return {{u"id"_s, qint64(p.id)}, {u"employeeId"_s, qs(p.employeeId)}, {u"clockIn"_s, qint64(p.clockIn)},
            {u"clockOut"_s, qint64(p.clockOut)}, {u"breaks"_s, breaks}, {u"job"_s, qs(p.job)},
            {u"rate"_s, qint64(p.rate.cents())}, {u"cashTips"_s, p.cashTipsDeclared ? qint64(p.cashTips.cents()) : qint64(-1)}};
}

TimePunch punchFromJson(const QJsonObject &o)
{
    TimePunch p{i64(o.value(u"id")), ss(o.value(u"employeeId").toString()), i64(o.value(u"clockIn")),
                i64(o.value(u"clockOut")), {}};
    for (const QJsonValue &b : o.value(u"breaks").toArray())
        p.breaks.push_back({i64(b.toObject().value(u"start")), i64(b.toObject().value(u"end"))});
    p.job = ss(o.value(u"job").toString());
    p.rate = Money::fromCents(i64(o.value(u"rate")));
    const std::int64_t cash = o.contains(u"cashTips") ? i64(o.value(u"cashTips")) : -1;
    p.cashTipsDeclared = cash >= 0;
    p.cashTips = Money::fromCents(std::max<std::int64_t>(0, cash));
    return p;
}

// --- reports and drawers ---------------------------------------------------------------

namespace {
QString kindName(ReportRow::Kind k)
{
    switch (k) {
    case ReportRow::Kind::Section: return u"section"_s;
    case ReportRow::Kind::Total: return u"total"_s;
    case ReportRow::Kind::Note: return u"note"_s;
    case ReportRow::Kind::Line: break;
    }
    return u"line"_s;
}

ReportRow::Kind kindFromName(const QString &s)
{
    if (s == u"section") return ReportRow::Kind::Section;
    if (s == u"total") return ReportRow::Kind::Total;
    if (s == u"note") return ReportRow::Kind::Note;
    return ReportRow::Kind::Line;
}

QJsonArray strings(const std::vector<std::string> &v)
{
    QJsonArray a;
    for (const std::string &s : v)
        a.append(qs(s));
    return a;
}

std::vector<std::string> strings(const QJsonArray &a)
{
    std::vector<std::string> v;
    for (const QJsonValue &x : a)
        v.push_back(ss(x.toString()));
    return v;
}
} // namespace

QJsonObject toJson(const Report &r)
{
    QJsonArray rows;
    for (const ReportRow &row : r.rows)
        rows.append(QJsonObject{{u"kind"_s, kindName(row.kind)}, {u"cells"_s, strings(row.cells)}});
    return {{u"id"_s, qs(r.id)}, {u"title"_s, qs(r.title)}, {u"subtitle"_s, qs(r.subtitle)},
            {u"columns"_s, strings(r.columns)}, {u"rows"_s, rows}};
}

Report reportFromJson(const QJsonObject &o)
{
    Report r;
    r.id = ss(o.value(u"id").toString());
    r.title = ss(o.value(u"title").toString());
    r.subtitle = ss(o.value(u"subtitle").toString());
    r.columns = strings(o.value(u"columns").toArray());
    for (const QJsonValue &v : o.value(u"rows").toArray()) {
        const QJsonObject row = v.toObject();
        r.rows.push_back({kindFromName(row.value(u"kind").toString()), strings(row.value(u"cells").toArray())});
    }
    return r;
}

QJsonObject toJson(const DrawerSession &d)
{
    QJsonArray movements;
    for (const CashMovement &m : d.movements) {
        movements.append(QJsonObject{
            {u"id"_s, qint64(m.id)}, {u"kind"_s, qs(toString(m.kind))}, {u"amount"_s, qint64(m.amount.cents())},
            {u"reason"_s, qs(m.reason)}, {u"by"_s, qs(m.by)}, {u"employeeId"_s, qs(m.employeeId)},
            {u"at"_s, qint64(m.at)}, {u"category"_s, qs(m.category)}});
    }
    return {{u"id"_s, qint64(d.id)}, {u"name"_s, qs(d.name)}, {u"terminal"_s, qs(d.terminal)},
            {u"employeeId"_s, qs(d.employeeId)},
            {u"openedAt"_s, qint64(d.openedAt)},
            {u"openedBy"_s, qs(d.openedBy)}, {u"startingCash"_s, qint64(d.startingCash.cents())},
            {u"closedAt"_s, qint64(d.closedAt)}, {u"closedBy"_s, qs(d.closedBy)},
            {u"expected"_s, qint64(d.expected.cents())}, {u"counted"_s, qint64(d.counted.cents())},
            {u"movements"_s, movements}, {u"nextMovementId"_s, qint64(d.nextMovementId)},
            {u"pieces"_s, [&] {
                 QJsonArray a;
                 for (const auto &[cents, n] : d.countedPieces)
                     a.append(QJsonArray{qint64(cents), n});
                 return a;
             }()},
            {u"blind"_s, d.blind}};
}

DrawerSession drawerFromJson(const QJsonObject &o)
{
    DrawerSession d;
    d.id = i64(o.value(u"id"));
    d.name = ss(o.value(u"name").toString(u"Drawer 1"_s));
    d.openedAt = i64(o.value(u"openedAt"));
    d.openedBy = ss(o.value(u"openedBy").toString());
    d.startingCash = money(o.value(u"startingCash"));
    d.closedAt = i64(o.value(u"closedAt"));
    d.closedBy = ss(o.value(u"closedBy").toString());
    d.expected = money(o.value(u"expected"));
    d.counted = money(o.value(u"counted"));
    d.terminal = ss(o.value(u"terminal").toString());
    d.employeeId = ss(o.value(u"employeeId").toString());
    for (const QJsonValue &v : o.value(u"movements").toArray()) {
        const QJsonObject m = v.toObject();
        d.movements.push_back({i64(m.value(u"id")), cashMovementKindFromString(ss(m.value(u"kind").toString())),
                               money(m.value(u"amount")), ss(m.value(u"reason").toString()),
                               ss(m.value(u"by").toString()), ss(m.value(u"employeeId").toString()),
                               i64(m.value(u"at")), ss(m.value(u"category").toString())});
    }
    d.nextMovementId = std::max<std::int64_t>(i64(o.value(u"nextMovementId")), 1);
    for (const QJsonValue &v : o.value(u"pieces").toArray()) {
        const QJsonArray p = v.toArray();
        if (p.size() == 2 && i64(p[0]) > 0 && p[1].toInt() > 0)
            d.countedPieces.emplace_back(i64(p[0]), p[1].toInt());
    }
    d.blind = o.value(u"blind").toBool();
    return d;
}

QJsonObject toJson(const StockCount &c)
{
    QJsonArray lines;
    for (const StockCount::Line &l : c.lines)
        lines.append(QJsonObject{{u"ingredient"_s, qs(l.ingredientId)}, {u"name"_s, qs(l.name)}, {u"unit"_s, qs(l.unit)},
                                 {u"since"_s, qint64(l.since)}, {u"start"_s, l.start}, {u"received"_s, l.received},
                                 {u"sold"_s, l.sold}, {u"adjusted"_s, l.adjusted}, {u"expected"_s, l.expected},
                                 {u"counted"_s, l.counted}, {u"unitCost"_s, qint64(l.unitCost.cents())},
                                 {u"wasted"_s, l.wasted}});
    return {{u"id"_s, qint64(c.id)}, {u"at"_s, qint64(c.at)}, {u"by"_s, qs(c.by)}, {u"lines"_s, lines}};
}

StockCount stockCountFromJson(const QJsonObject &o)
{
    StockCount c{i64(o.value(u"id")), i64(o.value(u"at")), ss(o.value(u"by").toString()), {}};
    for (const QJsonValue &v : o.value(u"lines").toArray()) {
        const QJsonObject l = v.toObject();
        c.lines.push_back({ss(l.value(u"ingredient").toString()), ss(l.value(u"name").toString()),
                           ss(l.value(u"unit").toString()), i64(l.value(u"since")), l.value(u"start").toDouble(),
                           l.value(u"received").toDouble(), l.value(u"sold").toDouble(), l.value(u"adjusted").toDouble(),
                           l.value(u"expected").toDouble(), l.value(u"counted").toDouble(),
                           Money::fromCents(i64(l.value(u"unitCost"))), l.value(u"wasted").toDouble()});
    }
    return c;
}

QJsonObject toJson(const Waste &w)
{
    QJsonArray lines;
    for (const Waste::Line &l : w.lines)
        lines.append(QJsonObject{{u"ingredient"_s, qs(l.ingredientId)}, {u"name"_s, qs(l.name)}, {u"unit"_s, qs(l.unit)},
                                 {u"qty"_s, l.qty}, {u"unitCost"_s, qint64(l.unitCost.cents())}});
    return {{u"id"_s, qint64(w.id)}, {u"at"_s, qint64(w.at)}, {u"by"_s, qs(w.by)}, {u"what"_s, qs(w.what)},
            {u"qty"_s, w.qty}, {u"unit"_s, qs(w.unit)}, {u"reason"_s, qs(w.reason)}, {u"lines"_s, lines}};
}

Waste wasteFromJson(const QJsonObject &o)
{
    Waste w{i64(o.value(u"id")), i64(o.value(u"at")), ss(o.value(u"by").toString()), ss(o.value(u"what").toString()),
            o.value(u"qty").toDouble(), ss(o.value(u"unit").toString()), ss(o.value(u"reason").toString()), {}};
    for (const QJsonValue &v : o.value(u"lines").toArray()) {
        const QJsonObject l = v.toObject();
        w.lines.push_back({ss(l.value(u"ingredient").toString()), ss(l.value(u"name").toString()),
                           ss(l.value(u"unit").toString()), l.value(u"qty").toDouble(),
                           Money::fromCents(i64(l.value(u"unitCost")))});
    }
    return w;
}

QJsonObject toJson(const Change &c)
{
    return {{u"id"_s, qint64(c.id)}, {u"at"_s, qint64(c.at)}, {u"day"_s, qint64(c.day)}, {u"who"_s, qs(c.who)},
            {u"area"_s, qs(c.area)}, {u"item"_s, qs(c.item)}, {u"field"_s, qs(c.field)},
            {u"before"_s, qs(c.before)}, {u"after"_s, qs(c.after)}};
}

Change changeFromJson(const QJsonObject &o)
{
    return {i64(o.value(u"id")), i64(o.value(u"at")), i64(o.value(u"day")), ss(o.value(u"who").toString()),
            ss(o.value(u"area").toString()), ss(o.value(u"item").toString()), ss(o.value(u"field").toString()),
            ss(o.value(u"before").toString()), ss(o.value(u"after").toString())};
}

// --- settings ---------------------------------------------------------------------------

QJsonObject toJson(const PrinterConfig &p)
{
    QJsonObject o{{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)}, {u"type"_s, qs(p.type)},
                  {u"width"_s, p.width}, {u"cutter"_s, p.cutter}, {u"drawerKick"_s, p.drawerKick},
                  {u"receipts"_s, p.receipts}};
    if (!p.charset.empty())
        o.insert(u"charset"_s, qs(p.charset));
    o.insert(u"watch"_s, p.watch);
    if (p.reportsStatus) o.insert(u"reportsStatus"_s, true);
    if (!p.host.empty()) o.insert(u"host"_s, qs(p.host));
    if (p.port != 9100) o.insert(u"port"_s, p.port);
    if (!p.path.empty()) o.insert(u"path"_s, qs(p.path));
    if (!p.format.empty()) o.insert(u"format"_s, qs(p.format));
    return o;
}

PrinterConfig printerFromJson(const QJsonObject &o)
{
    PrinterConfig p;
    p.id = ss(o.value(u"id").toString());
    p.name = ss(o.value(u"name").toString());
    if (p.name.empty())
        p.name = p.id;
    p.type = ss(o.value(u"type").toString(u"none"_s));
    p.host = ss(o.value(u"host").toString());
    p.port = o.value(u"port").toInt(9100);
    p.path = ss(o.value(u"path").toString());
    p.format = ss(o.value(u"format").toString());
    p.width = std::clamp(o.value(u"width").toInt(42), 16, 80);
    p.cutter = o.value(u"cutter").toBool(true);
    p.drawerKick = o.value(u"drawerKick").toBool(false);
    // Saved before this setting: the receipt printer, and those with a drawer, print receipts.
    p.receipts = o.value(u"receipts").toBool(p.id == "receipt" || p.drawerKick);
    p.charset = ss(o.value(u"charset").toString());
    p.watch = o.value(u"watch").toBool(true);
    p.reportsStatus = o.value(u"reportsStatus").toBool();
    return p;
}

QJsonArray modifierGroupsToJson(const std::vector<ModifierGroup> &groups)
{
    QJsonArray out;
    for (const ModifierGroup &g : groups) {
        QJsonArray options;
        for (const ModifierOption &o : g.options) {
            QJsonObject opt{{u"name"_s, qs(o.name)}, {u"price"_s, decimalFromCents(o.price.cents())}};
            if (!o.kitchenName.empty())
                opt.insert(u"kitchenName"_s, qs(o.kitchenName));
            if (o.kitchenHide)
                opt.insert(u"kitchenHide"_s, true);
            if (!o.itemId.empty())
                opt.insert(u"item"_s, qs(o.itemId));
            if (o.included)
                opt.insert(u"included"_s, true);
            if (!o.sizePrices.empty()) {
                QJsonObject sizes;
                for (const auto &[size, price] : o.sizePrices)
                    sizes.insert(qs(size), decimalFromCents(price.cents()));
                opt.insert(u"sizePrices"_s, sizes);
            }
            options.append(opt);
        }
        QJsonObject go{{u"id"_s, qs(g.id)}, {u"name"_s, qs(g.name)}, {u"min"_s, g.min}, {u"max"_s, g.max},
                       {u"options"_s, options}};
        if (g.menuItems)
            go.insert(u"menuItems"_s, true);
        if (g.askHow)
            go.insert(u"askHow"_s, true);
        out.append(go);
    }
    return out;
}

std::vector<ModifierGroup> modifierGroupsFromJson(const QJsonArray &a)
{
    std::vector<ModifierGroup> out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        ModifierGroup g;
        g.id = ss(o.value(u"id").toString());
        g.name = ss(o.value(u"name").toString(qs(g.id)));
        g.min = std::max(0, o.value(u"min").toInt(0));
        g.max = std::max(0, o.value(u"max").toInt(1));
        g.menuItems = o.value(u"menuItems").toBool();
        g.askHow = o.value(u"askHow").toBool();
        for (const QJsonValue &ov : o.value(u"options").toArray()) {
            const QJsonObject opt = ov.toObject();
            ModifierOption m;
            m.name = ss(opt.value(u"name").toString());
            m.price = Money::fromCents(centsFromDecimal(opt.value(u"price").toDouble()));
            m.kitchenName = ss(opt.value(u"kitchenName").toString());
            m.kitchenHide = opt.value(u"kitchenHide").toBool();
            m.itemId = ss(opt.value(u"item").toString());
            m.included = opt.value(u"included").toBool();
            const QJsonObject sizes = opt.value(u"sizePrices").toObject();
            for (auto it = sizes.begin(); it != sizes.end(); ++it)
                m.sizePrices[ss(it.key().toLower())] = Money::fromCents(centsFromDecimal(it.value().toDouble()));
            g.options.push_back(std::move(m));
        }
        out.push_back(std::move(g));
    }
    return out;
}

QString clockText(int minutes)
{
    return u"%1:%2"_s.arg(minutes / 60, 2, 10, QChar(u'0')).arg(minutes % 60, 2, 10, QChar(u'0'));
}

int clockMinutes(const QString &text)
{
    static const QRegularExpression re(u"^\\s*([01]?[0-9]|2[0-3])(?::([0-5][0-9]))?\\s*$"_s);
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() ? m.captured(1).toInt() * 60 + m.captured(2).toInt() : -1;
}

QJsonObject toJson(const PosSettings &s)
{
    QJsonArray terminals;
    for (const TerminalConfig &t : s.terminals)
        terminals.append(QJsonObject{{u"name"_s, qs(t.name)}, {u"receiptPrinter"_s, qs(t.receiptPrinter)},
                                     {u"drawer"_s, qs(t.drawer)}, {u"id"_s, qs(t.id)}, {u"key"_s, qs(t.key)},
                                     {u"pairedAt"_s, qint64(t.pairedAt)}, {u"screen"_s, qs(t.screen)}, {u"look"_s, qs(t.look)},
                                     {u"station"_s, qs(t.station)}, {u"keyboard"_s, qs(t.keyboard)},
                                     {u"requireName"_s, qs(t.requireName)}, {u"cardReader"_s, qs(t.cardReader)},
                                     {u"afterPaying"_s, qs(t.afterPaying)}, {u"startCategory"_s, qs(t.startCategory)}});
    QJsonArray printers;
    for (const PrinterConfig &p : s.printers)
        printers.append(toJson(p));
    QJsonArray tenders;
    for (const Tender &t : s.tenders) {
        QJsonObject o{{u"id"_s, qs(t.id)}, {u"name"_s, qs(t.name)}, {u"kind"_s, qs(toString(t.kind))}};
        if (t.kind == TenderKind::Discount)
            o.insert(u"percent"_s, double(t.percentBp) / 100.0);
        if (t.staffMeal)
            o.insert(u"staffMeal"_s, true);
        tenders.append(o);
    }
    QJsonArray mealPeriods;
    for (const MealPeriod &m : s.mealPeriods)
        mealPeriods.append(QJsonObject{{u"id"_s, qs(m.id)}, {u"name"_s, qs(m.name)}, {u"start"_s, clockText(m.start)}});
    return {
        {u"schemaVersion"_s, PosSchemaVersion},
        {u"storeName"_s, qs(s.storeName)}, {u"setupDone"_s, s.setupDone},
        {u"currencySymbol"_s, qs(s.currencySymbol)},
        {u"customColors"_s, [&s] { QJsonArray a; for (const std::string &c : s.customColors) a.append(qs(c)); return a; }()},
        {u"priceChanges"_s, [&s] {
             QJsonArray a;
             for (const PosSettings::PriceChange &c : s.priceChanges)
                 a.append(QJsonObject{{u"id"_s, qint64(c.id)}, {u"at"_s, qint64(c.at)}, {u"label"_s, qs(c.label)},
                                      {u"prices"_s, qs(c.prices)}});
             return a;
         }()},
        {u"tax"_s, QJsonObject{
             {u"food"_s, percentFromPpm(s.tax.foodPpm)}, {u"alcohol"_s, percentFromPpm(s.tax.alcoholPpm)},
             {u"merchandise"_s, percentFromPpm(s.tax.merchandisePpm)}, {u"room"_s, percentFromPpm(s.tax.roomPpm)},
             {u"taxTakeoutFood"_s, s.tax.taxTakeoutFood}, {u"cashRounding"_s, s.tax.cashRoundingCents}}},
        {u"tenders"_s, tenders}, {u"tendersV2"_s, true},
        {u"printers"_s, printers},
        {u"receiptHeader"_s, qs(s.receiptHeader)}, {u"receiptLogo"_s, s.receiptLogo},
        {u"receiptFreeChoices"_s, s.receiptFreeChoices},
        {u"soldOutBackNextDay"_s, s.soldOutBackNextDay},
        {u"receiptFooter"_s, qs(s.receiptFooter)},
        {u"gratuity"_s, QJsonObject{{u"percent"_s, double(s.gratuityBp) / 100.0}, {u"minGuests"_s, s.gratuityMinGuests}}},
        {u"terminals"_s, terminals},
        {u"mealPeriods"_s, mealPeriods},
        {u"menuCategories"_s, [&] {
             QJsonArray out;
             for (const MenuCategory &c : s.menuCategories) {
                 QJsonArray periods;
                 for (const std::string &p : c.periods)
                     periods.append(qs(p));
                 out.append(QJsonObject{{u"id"_s, qs(c.id)}, {u"name"_s, qs(c.name)}, {u"color"_s, qs(c.color)},
                                        {u"periods"_s, periods}, {u"printer"_s, qs(c.printer)},
                                        {u"station"_s, qs(c.station)}, {u"taxClass"_s, qs(c.taxClass)},
                                        {u"buttonSize"_s, qs(c.buttonSize)}, {u"photos"_s, c.photos},
                                        {u"hidePrice"_s, c.hidePrice}, {u"shades"_s, c.shades}});
             }
             return out;
         }()},
        {u"cashMode"_s, qs(toString(s.cashMode))},
        {u"labor"_s, QJsonObject{{u"paidBreaks"_s, s.paidBreaks}, {u"overtimeDailyHours"_s, s.overtimeDailyHours},
                                 {u"overtimeWeeklyHours"_s, s.overtimeWeeklyHours}, {u"weekStartsOn"_s, s.weekStartsOn}}},
        {u"modifierGroups"_s, modifierGroupsToJson(s.modifierGroups)},
        {u"terminalsHaveDrawer"_s, s.terminalsHaveDrawer},
        {u"serverId"_s, qs(s.serverId)},
        {u"checkoutNeedsClosedChecks"_s, s.checkoutNeedsClosedChecks},
        {u"blindDrawerCount"_s, s.blindDrawerCount},
        {u"askReasons"_s, s.askReasons}, {u"stockCountShowsExpected"_s, s.stockCountShowsExpected},
        {u"wasteReasons"_s, [&] { QJsonArray a; for (const std::string &r : s.wasteReasons) a.append(qs(r)); return a; }()},
        {u"voidReasons"_s, [&] { QJsonArray a; for (const std::string &r : s.voidReasons) a.append(qs(r)); return a; }()},
        {u"compReasons"_s, [&] { QJsonArray a; for (const std::string &r : s.compReasons) a.append(qs(r)); return a; }()},
        {u"cashDenominations"_s, [&] { QJsonArray a; for (std::int64_t c : s.cashDenominations) a.append(qint64(c)); return a; }()},
        {u"backupCopyDir"_s, qs(s.backupCopyDir)},
        {u"backupKey"_s, qs(s.backupKey)}, {u"backupSalt"_s, qs(s.backupSalt)},
        {u"waitMinutesPerParty"_s, s.waitMinutesPerParty},
        {u"autoLogoutMinutes"_s, s.autoLogoutMinutes}, {u"tableLongMinutes"_s, s.tableLongMinutes},
        {u"screenSaverMinutes"_s, s.screenSaverMinutes},
        {u"replicaKey"_s, qs(s.replicaKey)}, {u"language"_s, qs(s.language)},
        {u"startPages"_s, [&] {
             QJsonObject o;
             for (const auto &[job, page] : s.startPages)
                 o.insert(qs(job), qs(page));
             return o;
         }()},
        {u"extraPercent"_s, s.extraPercent}, {u"extraCharge"_s, decimalFromCents(s.extraCharge.cents())},
        {u"royaltyBp"_s, qint64(s.royaltyBp)}, {u"adFundBp"_s, qint64(s.adFundBp)},
        {u"accounts"_s, [&] {
             QJsonObject a;
             for (const auto &[k, v] : s.accounts)
                 a.insert(qs(k), qs(v));
             return a;
         }()},
        {u"expenseCategories"_s, [&] {
             QJsonArray a;
             for (const std::string &c : s.expenseCategories)
                 a.append(qs(c));
             return a;
         }()},
        {u"kioskSendNow"_s, s.kioskSendNow}, {u"kioskIdleSeconds"_s, s.kioskIdleSeconds}, {u"kioskSlip"_s, s.kioskSlip},
        {u"kioskLook"_s, QJsonObject{{u"background"_s, qs(s.kioskLook.background)}, {u"card"_s, qs(s.kioskLook.card)},
                                     {u"go"_s, qs(s.kioskLook.go)}, {u"text"_s, qs(s.kioskLook.text)},
                                     {u"font"_s, qs(s.kioskLook.font)}, {u"welcome"_s, qs(s.kioskLook.welcome)},
                                     {u"sizePercent"_s, s.kioskLook.sizePercent}, {u"askWhere"_s, s.kioskLook.askWhere},
                                     {u"askName"_s, s.kioskLook.askName}, {u"easyReach"_s, s.kioskLook.easyReach}}}, {u"serverTerm"_s, s.serverTerm},
        {u"builds"_s, [&] {
             QJsonArray a;
             for (const PosSettings::BuildRun &b : s.builds)
                 a.append(QJsonObject{{u"version"_s, qs(b.version)}, {u"number"_s, b.number},
                                      {u"commit"_s, qs(b.commit)}, {u"at"_s, qint64(b.at)}});
             return a;
         }()},
        {u"display"_s, [&] {
             QJsonArray slides;
             for (const std::string &sl : s.displaySlides) slides.append(qs(sl));
             return QJsonObject{{u"logo"_s, qs(s.displayLogo)}, {u"accent"_s, qs(s.displayAccent)}, {u"slides"_s, slides}};
         }()},
        {u"loyalty"_s, [&] {
             QJsonArray rewards;
             for (const PosSettings::Reward &r : s.rewards)
                 rewards.append(QJsonObject{{u"points"_s, r.points}, {u"value"_s, decimalFromCents(r.value.cents())}});
             return QJsonObject{{u"enabled"_s, s.loyaltyEnabled}, {u"pointsPerDollar"_s, s.pointsPerDollar},
                                {u"rewards"_s, rewards}};
         }()},
        {u"openingChecklist"_s, [&] { QJsonArray a; for (const std::string &t : s.openingChecklist) a.append(qs(t)); return a; }()},
        {u"closingChecklist"_s, [&] { QJsonArray a; for (const std::string &t : s.closingChecklist) a.append(qs(t)); return a; }()},
        {u"checklistDayId"_s, qint64(s.checklistDayId)},
        {u"tableStates"_s, [&] {
             QJsonArray a;
             for (const PosSettings::TableState &t : s.tableStates)
                 a.append(QJsonObject{{u"table"_s, qs(t.table)}, {u"state"_s, qs(t.state)}, {u"with"_s, qs(t.with)},
                                      {u"by"_s, qs(t.by)}, {u"partyId"_s, qint64(t.partyId)}, {u"since"_s, qint64(t.since)}});
             return a;
         }()},
        {u"prepSeconds"_s, [&] {
             QJsonObject o;
             for (const auto &[id, sec] : s.prepSeconds)
                 o.insert(qs(id), sec);
             return o;
         }()},
        {u"checklistTicks"_s, [&] {
             QJsonArray a;
             for (const PosSettings::ChecklistTick &t : s.checklistTicks)
                 a.append(QJsonObject{{u"list"_s, qs(t.list)}, {u"task"_s, qs(t.task)}, {u"by"_s, qs(t.by)}, {u"at"_s, qint64(t.at)}});
             return a;
         }()},
        {u"staffRequests"_s, [&] {
             QJsonArray a;
             for (const PosSettings::StaffRequest &r : s.staffRequests)
                 a.append(QJsonObject{{u"id"_s, qint64(r.id)}, {u"kind"_s, qs(r.kind)}, {u"employeeId"_s, qs(r.employeeId)},
                                      {u"at"_s, qint64(r.at)}, {u"day"_s, qint64(r.day)}, {u"shiftId"_s, qint64(r.shiftId)},
                                      {u"takerId"_s, qs(r.takerId)}, {u"note"_s, qs(r.note)}, {u"status"_s, qs(r.status)},
                                      {u"decidedBy"_s, qs(r.decidedBy)}, {u"decidedAt"_s, qint64(r.decidedAt)}});
             return a;
         }()},
        {u"punchChanges"_s, [&] {
             QJsonArray a;
             for (const PosSettings::PunchChange &c : s.punchChanges)
                 a.append(QJsonObject{{u"at"_s, qint64(c.at)}, {u"punchId"_s, qint64(c.punchId)}, {u"by"_s, qs(c.by)},
                                      {u"employee"_s, qs(c.employee)}, {u"what"_s, qs(c.what)}, {u"why"_s, qs(c.why)}});
             return a;
         }()},
        {u"notices"_s, [&] {
             QJsonArray a;
             for (const PosSettings::Notice &n : s.notices)
                 a.append(QJsonObject{{u"id"_s, qint64(n.id)}, {u"at"_s, qint64(n.at)}, {u"until"_s, qint64(n.until)},
                                      {u"from"_s, qs(n.from)}, {u"to"_s, qs(n.to)}, {u"text"_s, qs(n.text)}});
             return a;
         }()},
        {u"stations"_s, [&] {
             QJsonArray a;
             for (const Station &st : s.stations)
                 a.append(QJsonObject{{u"id"_s, qs(st.id)}, {u"name"_s, qs(st.name)}});
             return a;
         }()},
        {u"vendors"_s, [&] {
             QJsonArray a;
             for (const Vendor &v : s.vendors)
                 a.append(QJsonObject{{u"id"_s, qs(v.id)}, {u"name"_s, qs(v.name)}, {u"phone"_s, qs(v.phone)},
                                      {u"account"_s, qs(v.account)}, {u"note"_s, qs(v.note)}});
             return a;
         }()},
        {u"promotions"_s, [&] {
             QJsonArray a;
             for (const PosSettings::Promotion &p : s.promotions) {
                 QJsonArray fam, items;
                 for (const std::string &f : p.families) fam.append(qs(f));
                 for (const std::string &i : p.items) items.append(qs(i));
                 a.append(QJsonObject{{u"id"_s, qs(p.id)}, {u"name"_s, qs(p.name)}, {u"active"_s, p.active},
                                      {u"families"_s, fam}, {u"items"_s, items}, {u"percent"_s, double(p.percentBp) / 100.0},
                                      {u"buy"_s, p.buy}, {u"get"_s, p.get}, {u"start"_s, p.startMinute},
                                      {u"end"_s, p.endMinute}, {u"days"_s, p.days}, {u"code"_s, qs(p.code)},
                                      {u"amount"_s, qint64(p.amount.cents())}, {u"minimum"_s, qint64(p.minimum.cents())},
                                      {u"fromDay"_s, qs(p.fromDay)}, {u"untilDay"_s, qs(p.untilDay)},
                                      {u"maxUses"_s, p.maxUses}, {u"uses"_s, p.uses}});
             }
             return a;
         }()},
        {u"scheduleRequired"_s, s.scheduleRequired}, {u"clockInEarlyMinutes"_s, s.clockInEarlyMinutes},
        {u"tipPool"_s, [&] {
             QJsonObject o;
             for (const auto &[role, share] : s.tipPool)
                 o.insert(qs(role), share);
             return o;
         }()},
        {u"declareCashTips"_s, s.declareCashTips},
        {u"tipOuts"_s, [&] {
             QJsonArray a;
             for (const PosSettings::TipOut &t : s.tipOuts)
                 a.append(QJsonObject{{u"role"_s, qs(t.role)}, {u"percent"_s, double(t.percentBp) / 100.0},
                                      {u"basis"_s, qs(t.basis)}});
             return a;
         }()},
        {u"kitchenWarnMinutes"_s, s.kitchenWarnMinutes}, {u"kitchenLateMinutes"_s, s.kitchenLateMinutes},
        {u"laterLeadMinutes"_s, s.laterLeadMinutes},
        {u"requireOrderName"_s, s.requireOrderName}, {u"takeoutMinutes"_s, s.takeoutMinutes},
        {u"deliveryMinutes"_s, s.deliveryMinutes}, {u"minutesPerOrderWaiting"_s, s.minutesPerOrderWaiting},
        {u"deliveryFee"_s, qint64(s.deliveryFee.cents())},
        {u"stripeSecretKey"_s, qs(s.stripeSecretKey)}, {u"cardCurrency"_s, qs(s.cardCurrency)},
        {u"offlineCards"_s, s.offlineCards}, {u"offlineCardMax"_s, qint64(s.offlineCardMax)}, {u"tabHold"_s, qint64(s.tabHold)},
        {u"offlineCardTotal"_s, qint64(s.offlineCardTotal)},
        {u"stripeLocation"_s, qs(s.stripeLocation)}, {u"cardTipOn"_s, qs(s.cardTipOn)},
        {u"stripeReaders"_s, [&] {
             QJsonArray a;
             for (const PosSettings::StripeReader &r : s.stripeReaders)
                 a.append(QJsonObject{{u"id"_s, qs(r.id)}, {u"label"_s, qs(r.label)}, {u"deviceType"_s, qs(r.deviceType)}});
             return a;
         }()},
        {u"tipPercents"_s, [&] { QJsonArray a; for (int p : s.tipPercents) a.append(p); return a; }()}, {u"tableReadyText"_s, qs(s.tableReadyText)},
        {u"textWebhook"_s, qs(s.textWebhook)},
    };
}

PosSettings settingsFromJson(const QJsonObject &o)
{
    PosSettings s;
    s.storeName = ss(o.value(u"storeName").toString(qs(s.storeName)));
    s.setupDone = o.value(u"setupDone").toBool(false);
    s.currencySymbol = ss(o.value(u"currencySymbol").toString(qs(s.currencySymbol)));
    for (const QJsonValue &c : o.value(u"customColors").toArray())
        s.customColors.push_back(ss(c.toString()));
    for (const QJsonValue &v : o.value(u"priceChanges").toArray()) {
        const QJsonObject c = v.toObject();
        s.priceChanges.push_back({c.value(u"id").toInteger(), c.value(u"at").toInteger(), ss(c.value(u"label").toString()),
                                  ss(c.value(u"prices").toString())});
    }
    const QJsonObject tax = o.value(u"tax").toObject();
    s.tax.foodPpm = ppmFromPercent(tax.value(u"food").toDouble());
    s.tax.alcoholPpm = ppmFromPercent(tax.value(u"alcohol").toDouble());
    s.tax.merchandisePpm = ppmFromPercent(tax.value(u"merchandise").toDouble());
    s.tax.roomPpm = ppmFromPercent(tax.value(u"room").toDouble());
    s.tax.taxTakeoutFood = tax.value(u"taxTakeoutFood").toBool(true);
    s.tax.cashRoundingCents = tax.value(u"cashRounding").toInt(0);
    for (const QJsonValue &v : o.value(u"printers").toArray())
        s.printers.push_back(printerFromJson(v.toObject()));
    s.receiptHeader = ss(o.value(u"receiptHeader").toString());
    s.receiptFooter = ss(o.value(u"receiptFooter").toString());
    const QJsonObject gratuity = o.value(u"gratuity").toObject();
    s.gratuityBp = std::llround(gratuity.value(u"percent").toDouble() * 100.0);
    s.gratuityMinGuests = gratuity.value(u"minGuests").toInt(6);
    for (const QJsonValue &v : o.value(u"terminals").toArray()) {
        const QJsonObject t = v.toObject();
        // By name: a new setting can't shift the others.
        TerminalConfig c;
        c.name = ss(t.value(u"name").toString());
        c.receiptPrinter = ss(t.value(u"receiptPrinter").toString());
        c.drawer = ss(t.value(u"drawer").toString());
        c.id = ss(t.value(u"id").toString());
        c.key = ss(t.value(u"key").toString());
        c.pairedAt = i64(t.value(u"pairedAt"));
        c.screen = ss(t.value(u"screen").toString());
        c.station = ss(t.value(u"station").toString());
        c.look = ss(t.value(u"look").toString());
        c.keyboard = ss(t.value(u"keyboard").toString());
        c.requireName = ss(t.value(u"requireName").toString());
        c.cardReader = ss(t.value(u"cardReader").toString());
        c.afterPaying = ss(t.value(u"afterPaying").toString());
        c.startCategory = ss(t.value(u"startCategory").toString());
        s.terminals.push_back(std::move(c));
    }
    s.cashMode = cashModeFromString(ss(o.value(u"cashMode").toString()));
    const QJsonObject labor = o.value(u"labor").toObject();
    s.paidBreaks = labor.value(u"paidBreaks").toBool(false);
    s.overtimeDailyHours = std::clamp(labor.value(u"overtimeDailyHours").toInt(0), 0, 24);
    s.overtimeWeeklyHours = std::clamp(labor.value(u"overtimeWeeklyHours").toInt(40), 0, 168);
    s.weekStartsOn = std::clamp(labor.value(u"weekStartsOn").toInt(0), 0, 6);
    s.modifierGroups = modifierGroupsFromJson(o.value(u"modifierGroups").toArray());
    s.terminalsHaveDrawer = o.value(u"terminalsHaveDrawer").toBool(true);
    s.serverId = ss(o.value(u"serverId").toString());
    s.checkoutNeedsClosedChecks = o.value(u"checkoutNeedsClosedChecks").toBool(true);
    s.blindDrawerCount = o.value(u"blindDrawerCount").toBool();
    s.askReasons = o.value(u"askReasons").toBool(true);
    s.stockCountShowsExpected = o.value(u"stockCountShowsExpected").toBool();
    for (auto [key, list] : {std::pair{u"voidReasons", &s.voidReasons}, std::pair{u"compReasons", &s.compReasons},
                             std::pair{u"wasteReasons", &s.wasteReasons}}) {
        if (!o.value(QStringView(key)).isArray())
            continue;
        list->clear();
        for (const QJsonValue &v : o.value(QStringView(key)).toArray())
            if (!v.toString().trimmed().isEmpty())
                list->push_back(ss(v.toString().trimmed()));
    }
    if (o.value(u"cashDenominations").isArray()) {
        std::vector<std::int64_t> pieces;
        for (const QJsonValue &v : o.value(u"cashDenominations").toArray())
            if (i64(v) > 0 && pieces.size() < 16)
                pieces.push_back(i64(v));
        std::ranges::sort(pieces, std::greater<>());
        if (!pieces.empty())
            s.cashDenominations = pieces;
    }
    s.backupCopyDir = ss(o.value(u"backupCopyDir").toString());
    s.backupKey = ss(o.value(u"backupKey").toString());
    s.backupSalt = ss(o.value(u"backupSalt").toString());
    s.waitMinutesPerParty = std::clamp(o.value(u"waitMinutesPerParty").toInt(10), 1, 120);
    s.autoLogoutMinutes = std::clamp(o.value(u"autoLogoutMinutes").toInt(0), 0, 120);
    s.screenSaverMinutes = std::clamp(o.value(u"screenSaverMinutes").toInt(10), 0, 240);
    s.replicaKey = ss(o.value(u"replicaKey").toString());
    s.language = ss(o.value(u"language").toString(u"en"_s));
    const QJsonObject starts = o.value(u"startPages").toObject();
    for (auto it = starts.begin(); it != starts.end(); ++it)
        if (!it.value().toString().isEmpty())
            s.startPages[ss(it.key())] = ss(it.value().toString());
    s.extraPercent = std::clamp(o.value(u"extraPercent").toInt(0), 0, 500);
    s.extraCharge = Money::fromCents(centsFromDecimal(o.value(u"extraCharge").toDouble()));
    s.royaltyBp = std::clamp<std::int64_t>(o.value(u"royaltyBp").toInteger(0), 0, 10000);
    s.adFundBp = std::clamp<std::int64_t>(o.value(u"adFundBp").toInteger(0), 0, 10000);
    {
        const QJsonObject a = o.value(u"accounts").toObject();
        for (auto it = a.begin(); it != a.end(); ++it)
            s.accounts[ss(it.key())] = ss(it.value().toString());
    }
    if (o.contains(u"expenseCategories")) {
        s.expenseCategories.clear();
        for (const QJsonValue &v : o.value(u"expenseCategories").toArray())
            s.expenseCategories.push_back(ss(v.toString()));
    }
    s.kioskSendNow = o.value(u"kioskSendNow").toBool(false);
    s.kioskSlip = o.value(u"kioskSlip").toBool(true);
    s.kioskIdleSeconds = std::clamp(o.value(u"kioskIdleSeconds").toInt(90), 30, 600);
    {
        const QJsonObject k = o.value(u"kioskLook").toObject();
        auto &l = s.kioskLook;
        l.background = ss(k.value(u"background").toString());
        l.card = ss(k.value(u"card").toString());
        l.go = ss(k.value(u"go").toString());
        l.text = ss(k.value(u"text").toString());
        l.font = ss(k.value(u"font").toString());
        l.welcome = ss(k.value(u"welcome").toString());
        l.sizePercent = std::clamp(k.value(u"sizePercent").toInt(100), 60, 200);
        l.askWhere = k.value(u"askWhere").toBool(true);
        l.askName = k.value(u"askName").toBool(true);
        l.easyReach = k.value(u"easyReach").toBool(true);
    }
    s.serverTerm = o.value(u"serverTerm").toInt(0);
    for (const QJsonValue &v : o.value(u"builds").toArray()) {
        const QJsonObject b = v.toObject();
        s.builds.push_back({ss(b.value(u"version").toString()), b.value(u"number").toInt(),
                            ss(b.value(u"commit").toString()), b.value(u"at").toInteger()});
    }
    s.tableLongMinutes = std::clamp(o.value(u"tableLongMinutes").toInt(90), 10, 600);
    const QJsonObject display = o.value(u"display").toObject();
    s.displayLogo = ss(display.value(u"logo").toString());
    s.receiptLogo = o.value(u"receiptLogo").toBool();
    s.receiptFreeChoices = o.value(u"receiptFreeChoices").toBool(false);
    s.soldOutBackNextDay = o.value(u"soldOutBackNextDay").toBool(true);
    s.displayAccent = ss(display.value(u"accent").toString(u"#2f6fd6"_s));
    for (const QJsonValue &v : display.value(u"slides").toArray())
        if (!v.toString().trimmed().isEmpty()) s.displaySlides.push_back(ss(v.toString().trimmed()));
    const QJsonObject loyalty = o.value(u"loyalty").toObject();
    s.loyaltyEnabled = loyalty.value(u"enabled").toBool(false);
    s.pointsPerDollar = std::clamp(loyalty.value(u"pointsPerDollar").toInt(1), 1, 100);
    for (const QJsonValue &v : loyalty.value(u"rewards").toArray()) {
        const QJsonObject r = v.toObject();
        if (r.value(u"points").toInt() > 0)
            s.rewards.push_back({r.value(u"points").toInt(), Money::fromCents(centsFromDecimal(r.value(u"value").toDouble()))});
    }
    for (const QJsonValue &v : o.value(u"openingChecklist").toArray())
        s.openingChecklist.push_back(ss(v.toString()));
    for (const QJsonValue &v : o.value(u"closingChecklist").toArray())
        s.closingChecklist.push_back(ss(v.toString()));
    s.checklistDayId = i64(o.value(u"checklistDayId"));
    for (const QJsonValue &v : o.value(u"tableStates").toArray()) {
        const QJsonObject x = v.toObject();
        s.tableStates.push_back({ss(x.value(u"table").toString()), ss(x.value(u"state").toString()),
                                 ss(x.value(u"with").toString()), ss(x.value(u"by").toString()),
                                 i64(x.value(u"partyId")), i64(x.value(u"since"))});
    }
    const QJsonObject prep = o.value(u"prepSeconds").toObject();
    for (auto it = prep.begin(); it != prep.end(); ++it)
        s.prepSeconds[ss(it.key())] = it.value().toInt();
    for (const QJsonValue &v : o.value(u"checklistTicks").toArray()) {
        const QJsonObject x = v.toObject();
        s.checklistTicks.push_back({ss(x.value(u"list").toString()), ss(x.value(u"task").toString()),
                                    ss(x.value(u"by").toString()), i64(x.value(u"at"))});
    }
    for (const QJsonValue &v : o.value(u"staffRequests").toArray()) {
        const QJsonObject x = v.toObject();
        PosSettings::StaffRequest r;
        r.id = i64(x.value(u"id"));
        r.kind = ss(x.value(u"kind").toString());
        r.employeeId = ss(x.value(u"employeeId").toString());
        r.at = i64(x.value(u"at"));
        r.day = i64(x.value(u"day"));
        r.shiftId = i64(x.value(u"shiftId"));
        r.takerId = ss(x.value(u"takerId").toString());
        r.note = ss(x.value(u"note").toString());
        r.status = ss(x.value(u"status").toString(u"pending"_s));
        r.decidedBy = ss(x.value(u"decidedBy").toString());
        r.decidedAt = i64(x.value(u"decidedAt"));
        s.staffRequests.push_back(r);
    }
    for (const QJsonValue &v : o.value(u"punchChanges").toArray()) {
        const QJsonObject x = v.toObject();
        s.punchChanges.push_back({i64(x.value(u"at")), i64(x.value(u"punchId")), ss(x.value(u"by").toString()),
                                  ss(x.value(u"employee").toString()), ss(x.value(u"what").toString()),
                                  ss(x.value(u"why").toString())});
    }
    for (const QJsonValue &v : o.value(u"notices").toArray()) {
        const QJsonObject x = v.toObject();
        s.notices.push_back({i64(x.value(u"id")), i64(x.value(u"at")), i64(x.value(u"until")),
                             ss(x.value(u"from").toString()), ss(x.value(u"to").toString()), ss(x.value(u"text").toString())});
    }
    for (const QJsonValue &v : o.value(u"stations").toArray()) {
        const QJsonObject x = v.toObject();
        s.stations.push_back({ss(x.value(u"id").toString()), ss(x.value(u"name").toString())});
    }
    for (const QJsonValue &v : o.value(u"vendors").toArray()) {
        const QJsonObject x = v.toObject();
        s.vendors.push_back({ss(x.value(u"id").toString()), ss(x.value(u"name").toString()), ss(x.value(u"phone").toString()),
                             ss(x.value(u"account").toString()), ss(x.value(u"note").toString())});
    }
    for (const QJsonValue &v : o.value(u"promotions").toArray()) {
        const QJsonObject p = v.toObject();
        PosSettings::Promotion promo;
        promo.id = ss(p.value(u"id").toString());
        promo.name = ss(p.value(u"name").toString());
        promo.active = p.value(u"active").toBool(true);
        for (const QJsonValue &f : p.value(u"families").toArray()) promo.families.push_back(ss(f.toString()));
        for (const QJsonValue &i : p.value(u"items").toArray()) promo.items.push_back(ss(i.toString()));
        promo.percentBp = std::clamp<std::int64_t>(std::llround(p.value(u"percent").toDouble() * 100.0), 0, 10000);
        promo.buy = std::max(0, p.value(u"buy").toInt());
        promo.get = std::max(0, p.value(u"get").toInt());
        promo.startMinute = std::clamp(p.value(u"start").toInt(), 0, 24 * 60);
        promo.endMinute = std::clamp(p.value(u"end").toInt(), 0, 24 * 60);
        promo.days = p.value(u"days").toInt(0x7F) & 0x7F;
        promo.code = ss(p.value(u"code").toString());
        promo.amount = Money::fromCents(std::max<qint64>(0, p.value(u"amount").toInteger()));
        promo.minimum = Money::fromCents(std::max<qint64>(0, p.value(u"minimum").toInteger()));
        promo.fromDay = ss(p.value(u"fromDay").toString());
        promo.untilDay = ss(p.value(u"untilDay").toString());
        promo.maxUses = std::max(0, p.value(u"maxUses").toInt());
        promo.uses = std::max(0, p.value(u"uses").toInt());
        if (!promo.id.empty())
            s.promotions.push_back(promo);
    }
    s.scheduleRequired = o.value(u"scheduleRequired").toBool(false);
    s.clockInEarlyMinutes = std::clamp(o.value(u"clockInEarlyMinutes").toInt(15), 0, 240);
    {
        const QJsonObject pool = o.value(u"tipPool").toObject();
        for (auto it = pool.begin(); it != pool.end(); ++it)
            if (const int share = std::clamp(it.value().toInt(), 0, 1000); share > 0)
                s.tipPool[ss(it.key())] = share;
    }
    s.declareCashTips = o.value(u"declareCashTips").toBool();
    for (const QJsonValue &v : o.value(u"tipOuts").toArray()) {
        const QJsonObject t = v.toObject();
        const std::int64_t bp = std::llround(t.value(u"percent").toDouble() * 100.0);
        if (!t.value(u"role").toString().isEmpty() && bp > 0 && bp <= 10000)
            s.tipOuts.push_back({ss(t.value(u"role").toString()), bp, t.value(u"basis").toString() == u"sales" ? "sales" : "tips"});
    }
    s.kitchenWarnMinutes = std::clamp(o.value(u"kitchenWarnMinutes").toInt(8), 1, 120);
    s.kitchenLateMinutes = std::clamp(o.value(u"kitchenLateMinutes").toInt(15), s.kitchenWarnMinutes, 240);
    s.laterLeadMinutes = std::clamp(o.value(u"laterLeadMinutes").toInt(20), 0, 240);
    s.requireOrderName = o.value(u"requireOrderName").toBool();
    s.takeoutMinutes = std::clamp(o.value(u"takeoutMinutes").toInt(15), 0, 240);
    s.deliveryMinutes = std::clamp(o.value(u"deliveryMinutes").toInt(35), 0, 240);
    s.minutesPerOrderWaiting = std::clamp(o.value(u"minutesPerOrderWaiting").toInt(2), 0, 60);
    s.deliveryFee = money(o.value(u"deliveryFee"));
    s.stripeSecretKey = ss(o.value(u"stripeSecretKey").toString());
    s.offlineCards = o.value(u"offlineCards").toBool(false);
    s.offlineCardMax = o.value(u"offlineCardMax").toInteger(10000);
    s.tabHold = std::max<qint64>(0, o.value(u"tabHold").toInteger(5000));
    s.offlineCardTotal = o.value(u"offlineCardTotal").toInteger(100000);
    s.cardCurrency = ss(o.value(u"cardCurrency").toString(u"usd"_s));
    if (s.cardCurrency.empty())
        s.cardCurrency = "usd";
    s.stripeLocation = ss(o.value(u"stripeLocation").toString());
    s.cardTipOn = ss(o.value(u"cardTipOn").toString());
    for (const QJsonValue &v : o.value(u"stripeReaders").toArray()) {
        const QJsonObject r = v.toObject();
        s.stripeReaders.push_back({ss(r.value(u"id").toString()), ss(r.value(u"label").toString()),
                                   ss(r.value(u"deviceType").toString())});
    }
    if (o.value(u"tipPercents").isArray()) {
        s.tipPercents.clear();
        for (const QJsonValue &v : o.value(u"tipPercents").toArray()) {
            if (v.toInt() > 0 && v.toInt() <= 100 && s.tipPercents.size() < 6)
                s.tipPercents.push_back(v.toInt());
        }
    }
    if (o.contains(u"tableReadyText") && !o.value(u"tableReadyText").toString().trimmed().isEmpty())
        s.tableReadyText = ss(o.value(u"tableReadyText").toString());
    s.textWebhook = ss(o.value(u"textWebhook").toString());
    for (const QJsonValue &v : o.value(u"menuCategories").toArray()) {
        const QJsonObject c = v.toObject();
        MenuCategory cat;
        cat.id = ss(c.value(u"id").toString());
        cat.name = ss(c.value(u"name").toString(c.value(u"id").toString()));
        cat.color = ss(c.value(u"color").toString());
        for (const QJsonValue &p : c.value(u"periods").toArray())
            cat.periods.push_back(ss(p.toString()));
        cat.printer = ss(c.value(u"printer").toString());
        cat.station = ss(c.value(u"station").toString());
        cat.taxClass = ss(c.value(u"taxClass").toString());
        cat.buttonSize = ss(c.value(u"buttonSize").toString());
        cat.photos = c.value(u"photos").toBool();
        cat.hidePrice = c.value(u"hidePrice").toBool();
        cat.shades = c.value(u"shades").toBool();
        if (!cat.id.empty())
            s.menuCategories.push_back(std::move(cat));
    }
    if (o.contains(u"mealPeriods")) {   // older settings keep the defaults
        s.mealPeriods.clear();
        for (const QJsonValue &v : o.value(u"mealPeriods").toArray()) {
            const QJsonObject m = v.toObject();
            s.mealPeriods.push_back({ss(m.value(u"id").toString()), ss(m.value(u"name").toString()),
                                     std::max(0, clockMinutes(m.value(u"start").toString()))});
        }
    }
    for (const QJsonValue &v : o.value(u"tenders").toArray()) {
        const QJsonObject t = v.toObject();
        Tender tender;
        tender.id = ss(t.value(u"id").toString());
        tender.name = ss(t.value(u"name").toString());
        tender.kind = tenderKindFromString(ss(t.value(u"kind").toString()));
        tender.percentBp = std::llround(t.value(u"percent").toDouble() * 100.0);
        tender.staffMeal = t.value(u"staffMeal").toBool();
        s.tenders.push_back(tender);
    }
    // Settings from before gift cards and house accounts (once; afterwards
    // the managers' choices stand): the starter "gift" tender was a plain
    // card tender, and there was no house account tender.
    if (!o.value(u"tendersV2").toBool() && !s.tenders.empty()) {
        for (Tender &t : s.tenders) {
            if (t.id == "gift" && t.kind == TenderKind::Card)
                t.kind = TenderKind::GiftCard;
        }
        if (std::ranges::none_of(s.tenders, [](const Tender &t) { return t.kind == TenderKind::HouseAccount; })
            && !s.tender("house"))
            s.tenders.push_back({"house", "House Account", TenderKind::HouseAccount, 0});
    }
    return s;
}

// --- customers and gift cards ---------------------------------------------------------

namespace {
QJsonArray ledgerJson(const std::vector<LedgerEntry> &entries)
{
    QJsonArray out;
    for (const LedgerEntry &e : entries)
        out.append(QJsonObject{{u"at"_s, qint64(e.at)}, {u"amount"_s, qint64(e.amount.cents())},
                               {u"what"_s, qs(e.what)}, {u"checkId"_s, qint64(e.checkId)}, {u"kind"_s, qs(e.kind)}});
    return out;
}

std::vector<LedgerEntry> ledgerFromJson(const QJsonArray &a)
{
    std::vector<LedgerEntry> out;
    for (const QJsonValue &v : a) {
        const QJsonObject e = v.toObject();
        out.push_back({i64(e.value(u"at")), money(e.value(u"amount")), ss(e.value(u"what").toString()),
                       i64(e.value(u"checkId")), ss(e.value(u"kind").toString())});
    }
    return out;
}
} // namespace

QJsonObject toJson(const CustomerRecord &c)
{
    return {
        {u"id"_s, qs(c.id)}, {u"name"_s, qs(c.name)}, {u"phone"_s, qs(c.phone)}, {u"email"_s, qs(c.email)},
        {u"address"_s, qs(c.address)}, {u"note"_s, qs(c.note)}, {u"createdAt"_s, qint64(c.createdAt)},
        {u"visits"_s, c.visits}, {u"spent"_s, qint64(c.spent.cents())}, {u"lastVisit"_s, qint64(c.lastVisit)},
        {u"houseAccount"_s, c.houseAccount}, {u"accountLimit"_s, qint64(c.accountLimit.cents())},
        {u"accountBalance"_s, qint64(c.accountBalance.cents())}, {u"account"_s, ledgerJson(c.account)},
        {u"points"_s, c.points}, {u"lifetimePoints"_s, c.lifetimePoints},
        {u"lastOrder"_s, [&] {
             Check holder;
             holder.lines = c.lastOrder;
             return toJson(holder).value(u"lines"_s);
         }()},
        {u"lastOrderAt"_s, qint64(c.lastOrderAt)},
    };
}

CustomerRecord customerFromJson(const QJsonObject &o)
{
    CustomerRecord c;
    c.id = ss(o.value(u"id").toString());
    c.name = ss(o.value(u"name").toString());
    c.phone = ss(o.value(u"phone").toString());
    c.email = ss(o.value(u"email").toString());
    c.address = ss(o.value(u"address").toString());
    c.note = ss(o.value(u"note").toString());
    c.createdAt = i64(o.value(u"createdAt"));
    c.visits = o.value(u"visits").toInt();
    c.spent = money(o.value(u"spent"));
    c.lastVisit = i64(o.value(u"lastVisit"));
    c.houseAccount = o.value(u"houseAccount").toBool();
    c.accountLimit = money(o.value(u"accountLimit"));
    c.accountBalance = money(o.value(u"accountBalance"));
    c.account = ledgerFromJson(o.value(u"account").toArray());
    c.points = o.value(u"points").toInt();
    c.lifetimePoints = o.value(u"lifetimePoints").toInt();
    if (const QJsonArray last = o.value(u"lastOrder").toArray(); !last.isEmpty())
        if (const std::optional<Check> holder = checkFromJson(QJsonObject{{u"id"_s, 0}, {u"lines"_s, last}}))
            c.lastOrder = holder->lines;
    c.lastOrderAt = i64(o.value(u"lastOrderAt"));
    return c;
}

QJsonObject toJson(const GiftCard &g)
{
    return {{u"number"_s, qs(g.number)}, {u"balance"_s, qint64(g.balance.cents())},
            {u"issuedAt"_s, qint64(g.issuedAt)}, {u"history"_s, ledgerJson(g.history)}};
}

GiftCard giftCardFromJson(const QJsonObject &o)
{
    GiftCard g;
    g.number = ss(o.value(u"number").toString());
    g.balance = money(o.value(u"balance"));
    g.issuedAt = i64(o.value(u"issuedAt"));
    g.history = ledgerFromJson(o.value(u"history").toArray());
    return g;
}

// --- waitlist and reservations ---------------------------------------------------------

QJsonObject toJson(const Party &p)
{
    return {
        {u"id"_s, qint64(p.id)}, {u"name"_s, qs(p.name)}, {u"phone"_s, qs(p.phone)}, {u"size"_s, p.size},
        {u"note"_s, qs(p.note)}, {u"customerId"_s, qs(p.customerId)}, {u"addedAt"_s, qint64(p.addedAt)},
        {u"reservedFor"_s, qint64(p.reservedFor)}, {u"quotedMinutes"_s, p.quotedMinutes},
        {u"arrivedAt"_s, qint64(p.arrivedAt)}, {u"notifiedAt"_s, qint64(p.notifiedAt)},
        {u"seatedAt"_s, qint64(p.seatedAt)}, {u"table"_s, qs(p.table)}, {u"checkId"_s, qint64(p.checkId)},
        {u"walkIn"_s, p.walkIn},
        {u"status"_s, qs(toString(p.status))},
    };
}

Party partyFromJson(const QJsonObject &o)
{
    Party p;
    p.id = i64(o.value(u"id"));
    p.name = ss(o.value(u"name").toString());
    p.phone = ss(o.value(u"phone").toString());
    p.size = std::max(1, o.value(u"size").toInt(2));
    p.note = ss(o.value(u"note").toString());
    p.customerId = ss(o.value(u"customerId").toString());
    p.addedAt = i64(o.value(u"addedAt"));
    p.reservedFor = i64(o.value(u"reservedFor"));
    p.quotedMinutes = o.value(u"quotedMinutes").toInt();
    p.arrivedAt = i64(o.value(u"arrivedAt"));
    p.notifiedAt = i64(o.value(u"notifiedAt"));
    p.seatedAt = i64(o.value(u"seatedAt"));
    p.table = ss(o.value(u"table").toString());
    p.checkId = i64(o.value(u"checkId"));
    p.walkIn = o.value(u"walkIn").toBool();
    p.status = partyStatusFromString(ss(o.value(u"status").toString()));
    return p;
}

// --- inventory ---------------------------------------------------------------------------

QJsonObject toJson(const Ingredient &i)
{
    QJsonObject o{{u"id"_s, qs(i.id)}, {u"name"_s, qs(i.name)}, {u"unit"_s, qs(i.unit)}, {u"onHand"_s, i.onHand},
                  {u"lowAt"_s, i.lowAt}, {u"cost"_s, decimalFromCents(i.cost.cents())}, {u"vendor"_s, qs(i.vendor)}};
    if (i.par > 0)
        o.insert(u"par"_s, i.par);
    if (i.countedAt || i.start || i.received || i.sold || i.wasted || i.adjusted)
        o.insert(u"sinceCount"_s, QJsonObject{{u"at"_s, qint64(i.countedAt)}, {u"start"_s, i.start},
                                              {u"received"_s, i.received}, {u"sold"_s, i.sold},
                                              {u"wasted"_s, i.wasted}, {u"adjusted"_s, i.adjusted}});
    return o;
}

Ingredient ingredientFromJson(const QJsonObject &o)
{
    Ingredient i;
    i.id = ss(o.value(u"id").toString());
    i.name = ss(o.value(u"name").toString());
    if (i.name.empty())
        i.name = i.id;
    i.unit = ss(o.value(u"unit").toString(u"each"_s));
    i.onHand = o.value(u"onHand").toDouble();
    i.lowAt = o.value(u"lowAt").toDouble();
    i.cost = Money::fromCents(centsFromDecimal(o.value(u"cost").toDouble()));
    i.vendor = ss(o.value(u"vendor").toString());
    i.par = std::max(0.0, o.value(u"par").toDouble());
    if (o.contains(u"sinceCount")) {
        const QJsonObject c = o.value(u"sinceCount").toObject();
        i.countedAt = i64(c.value(u"at"));
        i.start = c.value(u"start").toDouble();
        i.received = c.value(u"received").toDouble();
        i.sold = c.value(u"sold").toDouble();
        i.wasted = c.value(u"wasted").toDouble();
        i.adjusted = c.value(u"adjusted").toDouble();
    } else {
        i.start = i.onHand;   // from before counts: what it had is where it starts
    }
    return i;
}

QJsonObject toJson(const Delivery &d)
{
    QJsonArray lines;
    for (const Delivery::Line &l : d.lines)
        lines.append(QJsonObject{{u"ingredientId"_s, qs(l.ingredientId)}, {u"name"_s, qs(l.name)}, {u"unit"_s, qs(l.unit)},
                                 {u"qty"_s, l.qty}, {u"unitCost"_s, qint64(l.unitCost.cents())}});
    return {{u"id"_s, qint64(d.id)}, {u"at"_s, qint64(d.at)}, {u"vendorId"_s, qs(d.vendorId)},
            {u"vendorName"_s, qs(d.vendorName)}, {u"invoice"_s, qs(d.invoice)}, {u"by"_s, qs(d.by)},
            {u"lines"_s, lines}};
}

Delivery deliveryFromJson(const QJsonObject &o)
{
    Delivery d;
    d.id = i64(o.value(u"id"));
    d.at = i64(o.value(u"at"));
    d.vendorId = ss(o.value(u"vendorId").toString());
    d.vendorName = ss(o.value(u"vendorName").toString());
    d.invoice = ss(o.value(u"invoice").toString());
    d.by = ss(o.value(u"by").toString());
    for (const QJsonValue &v : o.value(u"lines").toArray()) {
        const QJsonObject l = v.toObject();
        d.lines.push_back({ss(l.value(u"ingredientId").toString()), ss(l.value(u"name").toString()),
                           ss(l.value(u"unit").toString()), l.value(u"qty").toDouble(),
                           Money::fromCents(i64(l.value(u"unitCost")))});
    }
    return d;
}

std::vector<Ingredient> ingredientsFromJson(const QJsonArray &a)
{
    std::vector<Ingredient> out;
    for (const QJsonValue &v : a)
        out.push_back(ingredientFromJson(v.toObject()));
    return out;
}

// --- schedule ----------------------------------------------------------------------------

QJsonObject toJson(const Shift &s)
{
    return {{u"id"_s, qint64(s.id)}, {u"employeeId"_s, qs(s.employeeId)}, {u"start"_s, qint64(s.start)},
            {u"end"_s, qint64(s.end)}, {u"note"_s, qs(s.note)}};
}

Shift shiftFromJson(const QJsonObject &o)
{
    return {i64(o.value(u"id")), ss(o.value(u"employeeId").toString()), i64(o.value(u"start")), i64(o.value(u"end")),
            ss(o.value(u"note").toString())};
}

} // namespace vt::app
