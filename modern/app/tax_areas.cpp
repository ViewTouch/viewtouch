#include "app/tax_areas.hh"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <cmath>

using namespace Qt::StringLiterals;

namespace vt::app {

struct TaxAreas::Impl {
    QJsonObject states;
    QJsonObject prefixes;   // a ZIP code's first three digits: its state
    QJsonObject zips;
};

namespace {

double round4(double v) { return std::round(v * 10000) / 10000; }

const QHash<QString, QString> &stateNames()
{
    static const QHash<QString, QString> names{
        {u"AL"_s, u"Alabama"_s}, {u"AK"_s, u"Alaska"_s}, {u"AZ"_s, u"Arizona"_s}, {u"AR"_s, u"Arkansas"_s},
        {u"CA"_s, u"California"_s}, {u"CO"_s, u"Colorado"_s}, {u"CT"_s, u"Connecticut"_s}, {u"DE"_s, u"Delaware"_s},
        {u"DC"_s, u"District of Columbia"_s}, {u"FL"_s, u"Florida"_s}, {u"GA"_s, u"Georgia"_s}, {u"HI"_s, u"Hawaii"_s},
        {u"ID"_s, u"Idaho"_s}, {u"IL"_s, u"Illinois"_s}, {u"IN"_s, u"Indiana"_s}, {u"IA"_s, u"Iowa"_s},
        {u"KS"_s, u"Kansas"_s}, {u"KY"_s, u"Kentucky"_s}, {u"LA"_s, u"Louisiana"_s}, {u"ME"_s, u"Maine"_s},
        {u"MD"_s, u"Maryland"_s}, {u"MA"_s, u"Massachusetts"_s}, {u"MI"_s, u"Michigan"_s}, {u"MN"_s, u"Minnesota"_s},
        {u"MS"_s, u"Mississippi"_s}, {u"MO"_s, u"Missouri"_s}, {u"MT"_s, u"Montana"_s}, {u"NE"_s, u"Nebraska"_s},
        {u"NV"_s, u"Nevada"_s}, {u"NH"_s, u"New Hampshire"_s}, {u"NJ"_s, u"New Jersey"_s}, {u"NM"_s, u"New Mexico"_s},
        {u"NY"_s, u"New York"_s}, {u"NC"_s, u"North Carolina"_s}, {u"ND"_s, u"North Dakota"_s}, {u"OH"_s, u"Ohio"_s},
        {u"OK"_s, u"Oklahoma"_s}, {u"OR"_s, u"Oregon"_s}, {u"PA"_s, u"Pennsylvania"_s}, {u"RI"_s, u"Rhode Island"_s},
        {u"SC"_s, u"South Carolina"_s}, {u"SD"_s, u"South Dakota"_s}, {u"TN"_s, u"Tennessee"_s}, {u"TX"_s, u"Texas"_s},
        {u"UT"_s, u"Utah"_s}, {u"VT"_s, u"Vermont"_s}, {u"VA"_s, u"Virginia"_s}, {u"WA"_s, u"Washington"_s},
        {u"WV"_s, u"West Virginia"_s}, {u"WI"_s, u"Wisconsin"_s}, {u"WY"_s, u"Wyoming"_s}};
    return names;
}

// A restaurant rate: {"rate": R} for the state's part (local taxes still on
// top unless "noLocal"), {"add": A} on top of the whole.
double restaurantRate(const QJsonObject &rule, double total, double stateRate)
{
    if (rule.contains(u"rate"))
        return rule.value(u"noLocal").toBool() ? rule.value(u"rate").toDouble()
                                                : rule.value(u"rate").toDouble() + (total - stateRate);
    return total + rule.value(u"add").toDouble();
}

// The table's words, in this screen's language: "X local taxes", "Local
// district N", "X, inside city limits", "X, outside a city".
QString words(const QString &text)
{
    if (text.endsWith(u" local taxes"_s))
        return QCoreApplication::translate("TaxAreas", "%1 local taxes").arg(text.chopped(12));
    if (text.startsWith(u"Local district "_s))
        return QCoreApplication::translate("TaxAreas", "Local district %1").arg(text.mid(15));
    if (text.endsWith(u", inside city limits"_s))
        return QCoreApplication::translate("TaxAreas", "%1, inside city limits").arg(text.chopped(20));
    if (text.endsWith(u", outside a city"_s))
        return QCoreApplication::translate("TaxAreas", "%1, outside a city").arg(text.chopped(16));
    if (text == u"Elsewhere in this ZIP code"_s)
        return QCoreApplication::translate("TaxAreas", "Elsewhere in this ZIP code");
    return text;
}

} // namespace

const TaxAreas &TaxAreas::instance()
{
    static const TaxAreas table = [] {
        TaxAreas t;
        QFile f(u":/seed/tax/areas.json"_s);
        if (f.open(QIODevice::ReadOnly))
            t.load(f.readAll());
        return t;
    }();
    return table;
}

TaxAreas TaxAreas::fromJson(const QByteArray &json)
{
    TaxAreas t;
    t.load(json);
    return t;
}

void TaxAreas::load(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    auto d = std::make_shared<Impl>();
    d->states = o.value(u"states").toObject();
    d->zips = o.value(u"zips").toObject();
    d->prefixes = o.value(u"prefixes").toObject();
    loaded_ = !d->states.isEmpty();
    asOf_ = o.value(u"asOf").toString();
    d_ = std::move(d);
}

QStringList TaxAreas::states() const
{
    return d_ ? d_->states.keys() : QStringList();
}

QString TaxAreas::stateName(const QString &state) const
{
    return stateNames().value(state.toUpper(), state);
}

QString TaxAreas::stateOf(const QString &zip) const
{
    if (!d_)
        return {};
    const QString z = zip.trimmed().left(5);
    const QString s = d_->zips.value(z).toObject().value(u"s").toString();
    return !s.isEmpty() || z.size() < 3 ? s : d_->prefixes.value(z.left(3)).toString();
}

TaxSuggestion TaxAreas::suggest(const QString &stateIn, const QString &zipIn, const QString &where) const
{
    TaxSuggestion s;
    if (!d_)
        return s;
    s.zip = zipIn.trimmed().left(5);
    const QJsonObject area = d_->zips.value(s.zip).toObject();
    const QString zipState = stateOf(s.zip);
    s.state = stateIn.isEmpty() ? zipState : stateIn.toUpper();
    if (s.state.isEmpty()) {
        s.problem = s.zip.isEmpty() ? QCoreApplication::translate("TaxAreas", "Choose the store's state and ZIP code.")
                                    : QCoreApplication::translate("TaxAreas", "Choose the store's state.");
        return s;
    }
    const QJsonObject rules = d_->states.value(s.state).toObject();
    if (rules.isEmpty()) {
        s.problem = QCoreApplication::translate("TaxAreas", "No rates for that state.");
        return s;
    }
    if (!zipState.isEmpty() && zipState != s.state) {
        s.problem = QCoreApplication::translate("TaxAreas", "That ZIP code is in %1, not %2.")
                        .arg(stateName(zipState), stateName(s.state));
        return s;
    }
    s.found = true;
    s.stateName = stateName(s.state);
    s.asOf = asOf_;
    s.source = rules.value(u"source").toString();
    s.lookUp = rules.value(u"lookUp").toString();
    s.confirm = rules.value(u"confirm").toString();
    for (const QJsonValue &n : rules.value(u"notes").toArray())
        s.notes << n.toString();
    const double stateRate = rules.value(u"rate").toDouble();
    s.city = area.value(u"c").toString();
    s.county = area.value(u"n").toString();

    // The usual rate here, and the others in the ZIP code.
    double total = area.isEmpty() ? stateRate : area.value(u"r").toDouble();
    if (!area.isEmpty() && area.contains(u"alt")) {
        const QString usual = area.value(u"where").toString();
        s.choices.append({usual, words(usual), total});
        for (const QJsonValue &v : area.value(u"alt").toArray()) {
            const QString key = v.toArray().at(0).toString();
            s.choices.append({key, words(key), v.toArray().at(1).toDouble()});
        }
        s.where = s.choices.first().where;
        s.whereKey = s.choices.first().key;
        for (const TaxSuggestion::Choice &c : std::as_const(s.choices))
            if (!where.isEmpty() && c.key == where) {
                s.where = c.where;
                s.whereKey = c.key;
                total = c.rate;
            }
    }
    s.sales = round4(total);

    // What it's made of: the state's details if any, else the state and the rest.
    if (area.contains(u"p") && s.whereKey == (s.choices.isEmpty() ? QString() : s.choices.first().key)) {
        for (const QJsonValue &v : area.value(u"p").toArray())
            s.parts.append({words(v.toArray().at(0).toString()), v.toArray().at(1).toDouble()});
    } else {
        s.parts.append({s.stateName, stateRate});
        if (round4(total - stateRate) > 0) {
            const QString place = !s.whereKey.isEmpty() ? s.whereKey.section(u", "_s, 0, 0)
                                  : !s.city.isEmpty() ? s.city : !s.county.isEmpty() ? s.county : QString();
            s.parts.append({place.isEmpty() ? QCoreApplication::translate("TaxAreas", "Local taxes")
                                            : QCoreApplication::translate("TaxAreas", "%1 local taxes").arg(place),
                            round4(total - stateRate)});
        }
    }

    // Restaurants: meals and drinks may be taxed differently.
    s.food = round4(rules.contains(u"meals") ? restaurantRate(rules.value(u"meals").toObject(), total, stateRate) : total);
    s.alcohol = round4((rules.contains(u"alcohol") ? restaurantRate(rules.value(u"alcohol").toObject(), total, stateRate)
                                                   : total)
                       + area.value(u"alc").toDouble());
    return s;
}

} // namespace vt::app
