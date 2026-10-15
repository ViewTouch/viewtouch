#include <catch2/catch_test_macros.hpp>

#include "app/tax_areas.hh"
#include "pos_fixture.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt;
using vt::app::PosService;
using vt::app::TaxAreas;

// The sales tax rate where the store is (seed/tax/areas.json, built by
// tools/taxrates from each state's published rates).

namespace {

const QByteArray kTable = R"({
  "asOf": "2026-10-01",
  "states": {
    "VT": {"rate": 6, "meals": {"rate": 9}, "alcohol": {"rate": 10}, "source": "VT"},
    "TX": {"rate": 6.25, "alcohol": {"rate": 8.25, "noLocal": true}, "source": "TX"},
    "TN": {"rate": 7, "alcohol": {"add": 15}, "source": "TN"},
    "MO": {"rate": 4.225, "source": "MO", "notes": ["A note."]},
    "LA": {"rate": 5, "source": "LA", "lookUp": "https://lulstb.com/"}
  },
  "zips": {
    "05401": {"r": 7, "c": "Burlington", "s": "VT"},
    "77002": {"r": 8.25, "c": "Houston", "s": "TX"},
    "37203": {"r": 9.75, "p": [["Tennessee", 7], ["Nashville", 2.25], ["Local district 91951", 0.5]], "c": "Nashville", "s": "TN"},
    "65201": {"r": 7.975, "c": "Columbia", "n": "Boone County", "where": "Columbia, inside city limits",
              "alt": [["Boone County, outside a city", 5.975]], "s": "MO"}
  }
})";

QString checkItem(PosService &pos, const QString &id)
{
    for (const QVariant &v : pos.storeCheck()[u"checks"_s].toList())
        if (v.toMap()[u"id"_s] == id)
            return v.toMap()[u"detail"_s].toString();
    return {};
}

} // namespace

TEST_CASE("Tax areas: a ZIP code's rate, and what restaurants pay on meals and drinks", "[tax][areas]")
{
    const TaxAreas t = TaxAreas::fromJson(kTable);
    REQUIRE(t.loaded());
    CHECK(t.asOf() == u"2026-10-01"_s);

    // Vermont: meals 9% and drinks 10% instead of the state's 6%, the town's 1% on top.
    auto vt = t.suggest(u"VT"_s, u"05401"_s);
    REQUIRE(vt.found);
    CHECK(vt.sales == 7);
    CHECK(vt.food == 10);
    CHECK(vt.alcohol == 11);
    CHECK(vt.city == u"Burlington"_s);

    // Texas: drinks served pay the 8.25% mixed beverage tax, no local sales tax.
    auto tx = t.suggest(u"TX"_s, u"77002-1234"_s);
    CHECK(tx.sales == 8.25);
    CHECK(tx.alcohol == 8.25);
    CHECK(tx.zip == u"77002"_s);

    // Tennessee: liquor by the drink 15% on top; the parts as the state names them.
    auto tn = t.suggest({}, u"37203"_s);   // the state from the ZIP code
    CHECK(tn.state == u"TN"_s);
    CHECK(tn.alcohol == 24.75);
    REQUIRE(tn.parts.size() == 3);
    CHECK(tn.parts[2].second == 0.5);

    // Two rates in a ZIP code: the usual one, or the one chosen.
    auto mo = t.suggest(u"MO"_s, u"65201"_s);
    REQUIRE(mo.choices.size() == 2);
    CHECK(mo.sales == 7.975);
    CHECK(mo.whereKey == u"Columbia, inside city limits"_s);
    mo = t.suggest(u"MO"_s, u"65201"_s, u"Boone County, outside a city"_s);
    CHECK(mo.sales == 5.975);
    CHECK(mo.notes == QStringList{u"A note."_s});

    // A ZIP code the table doesn't list: the state's rate; one in another state: said.
    CHECK(t.suggest(u"MO"_s, u"63999"_s).sales == 4.225);
    auto wrong = t.suggest(u"MO"_s, u"77002"_s);
    CHECK_FALSE(wrong.found);
    CHECK(wrong.problem.contains(u"Texas"_s));
    // Louisiana: the state's rate, and where to look up the local part.
    CHECK(t.suggest(u"LA"_s, u"70112"_s).lookUp.contains(u"lulstb"_s));
}

TEST_CASE("Tax areas: the table built in has every state, and known cities' rates", "[tax][areas]")
{
    const TaxAreas &t = TaxAreas::instance();
    REQUIRE(t.loaded());
    CHECK(t.states().size() == 51);
    CHECK(t.suggest({}, u"10001"_s).sales == 8.875);       // New York City
    CHECK(t.suggest({}, u"77002"_s).sales == 8.25);        // Houston
    CHECK(t.suggest({}, u"85004"_s).sales == 9.1);         // Phoenix, restaurants and bars
    CHECK(t.suggest({}, u"35203"_s).sales == 10);          // Birmingham
    CHECK(t.suggest(u"MA"_s, u"02108"_s).sales == 6.25);   // Boston: the state's rate
    CHECK(t.suggest(u"NH"_s, u"03301"_s).food == 8.5);     // New Hampshire's meals tax
}

TEST_CASE("Tax areas: Store Settings sets the rates where the store is, when asked", "[tax][areas]")
{
    PosService pos(test::seedPosData(), nullptr);
    REQUIRE(pos.loginWithPin(u"1234"_s));
    QVariantMap taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    CHECK(taxes[u"suggested"_s].toString().contains(u"state"_s));   // nothing chosen yet
    CHECK_FALSE(checkItem(pos, u"taxArea"_s).isEmpty());              // Store Check: no ZIP code

    // Houston: shown, not applied until asked.
    taxes.insert(u"state"_s, u"TX"_s);
    taxes.insert(u"zip"_s, u"77002"_s);
    REQUIRE(pos.adminSave(u"taxes"_s, 0, taxes));
    taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    CHECK(taxes[u"suggested"_s].toString().contains(u"8.25%"_s));
    CHECK(pos.shared()->settings.tax.foodPpm == 82500);                // the sample's own, by chance the same
    CHECK(pos.shared()->settings.laborState == "TX");                  // the state's labor rules too
    taxes.insert(u"useSuggested"_s, true);
    REQUIRE(pos.adminSave(u"taxes"_s, 0, taxes));
    CHECK(pos.shared()->settings.tax.foodPpm == 82500);
    CHECK(pos.shared()->settings.tax.alcoholPpm == 82500);             // mixed beverage tax
    CHECK(pos.shared()->settings.taxAreaAsOf == TaxAreas::instance().asOf().toStdString());
    CHECK(checkItem(pos, u"taxArea"_s).isEmpty());

    // Columbia, Missouri: outside the city, chosen.
    taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    taxes.insert(u"state"_s, u"MO"_s);
    taxes.insert(u"zip"_s, u"65201"_s);
    REQUIRE(pos.adminSave(u"taxes"_s, 0, taxes));
    const QVariantList where = pos.adminFields(u"taxes"_s).value(2).toMap()[u"options"_s].toList();
    REQUIRE(where.size() >= 2);
    taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    taxes.insert(u"taxWhere"_s, where[1].toMap()[u"value"_s]);
    taxes.insert(u"useSuggested"_s, true);
    REQUIRE(pos.adminSave(u"taxes"_s, 0, taxes));
    CHECK(pos.shared()->settings.tax.merchandisePpm != 79750);          // not the city's 7.975%

    // A ZIP code in another state, and one that isn't one: said, nothing saved.
    taxes = pos.adminRecords(u"taxes"_s).value(0).toMap();
    taxes.insert(u"zip"_s, u"77002"_s);
    taxes.insert(u"useSuggested"_s, true);
    CHECK_FALSE(pos.adminSave(u"taxes"_s, 0, taxes));
    taxes.insert(u"zip"_s, u"7700"_s);
    CHECK_FALSE(pos.adminSave(u"taxes"_s, 0, taxes));
}
