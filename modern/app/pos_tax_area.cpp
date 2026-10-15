// Where the store is (Store Settings -> Taxes): its state and ZIP code, the
// rate suggested there from each state's published rates (TaxAreas), for
// the manager to confirm.

#include "app/pos_service.hh"
#include "app/pos_json.hh"
#include "app/tax_areas.hh"
#include "app/i18n.hh"

#include <QDate>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {

QString pct(double rate)
{
    return i18n::locale().toString(rate, 'f', QLocale::FloatingPointShortest) + u"%"_s;
}

} // namespace

QVariantList PosService::stateOptions() const
{
    QVariantList o{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(not chosen)")}}};
    for (const core::StateRules &s : core::stateRules())
        o.append(QVariantMap{{u"value"_s, QString::fromLatin1(s.code)}, {u"text"_s, QString::fromLatin1(s.name)}});
    return o;
}

TaxSuggestion PosService::taxHere() const
{
    return TaxAreas::instance().suggest(qs(s_->settings.laborState), qs(s_->settings.storeZip), qs(s_->settings.taxWhere));
}

QVariantList PosService::taxWhereOptions() const
{
    QVariantList o;
    for (const TaxSuggestion::Choice &c : taxHere().choices)
        o.append(QVariantMap{{u"value"_s, c.key}, {u"text"_s, c.where + u" · "_s + pct(c.rate)}});
    if (o.isEmpty())
        o.append(QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(one rate in this ZIP code)")}});
    return o;
}

QString PosService::taxAreaText(const TaxSuggestion &s) const
{
    if (!s.found)
        return s.problem;
    QStringList parts;
    for (const auto &[name, rate] : s.parts)
        parts << tr("%1 %2").arg(name, pct(rate));
    QStringList out;
    const QString place = !s.city.isEmpty() ? s.city : s.county;
    out << (place.isEmpty() ? tr("%1 in %2: %3.").arg(pct(s.sales), s.stateName, parts.join(u" + "_s))
                            : tr("%1 in %2, %3: %4.").arg(pct(s.sales), place, s.stateName, parts.join(u" + "_s)));
    if (s.food != s.sales || s.alcohol != s.sales)
        out << tr("Restaurants: meals %1, drinks %2.").arg(pct(s.food), pct(s.alcohol));
    if (!s.lookUp.isEmpty())
        out << tr("The local part isn't included here: look it up at %1 and add it to the rates below.").arg(s.lookUp);
    if (!s.confirm.isEmpty())
        out << tr("Worked out from the state's file: confirm it at %1.").arg(s.confirm);
    out << s.notes;
    const QDate asOf = QDate::fromString(s.asOf, Qt::ISODate);
    out << tr("From %1; the table of %2. Check with your accountant.")
               .arg(s.source, asOf.isValid() ? i18n::locale().toString(asOf, QLocale::LongFormat) : s.asOf);
    return out.join(u"\n"_s);
}

bool PosService::setStoreState(const std::string &state, bool *applied)
{
    if (applied)
        *applied = false;
    if (state == s_->settings.laborState)
        return true;
    if (state.empty()) {
        s_->settings.laborState.clear();
        return true;
    }
    if (!core::stateRulesFor(state))
        return fail(tr("Choose the store's state from the list."));
    core::applyStateRules(s_->settings, state);
    if (applied)
        *applied = true;
    return true;
}

} // namespace vt::app
