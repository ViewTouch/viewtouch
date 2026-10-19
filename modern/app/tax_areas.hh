#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace vt::app {

// The sales tax rate where the store is, suggested from seed/tax/areas.json
// (each state's published rates by ZIP code, tools/taxrates). The manager
// confirms it; nothing changes by itself.
struct TaxSuggestion {
    struct Choice {
        QString key;     // the table's words, kept in the settings
        QString where;   // shown: "Kansas City, inside city limits"
        double rate = 0;
    };
    bool found = false;          // the state is known (and the ZIP, or its rate is the state's)
    QString state, stateName, zip, city, county;
    QString where, whereKey;     // the choice used (of `choices`, when there are more than one)
    QList<Choice> choices;       // the rates in this ZIP code and where each applies
    double sales = 0;            // the general rate (merchandise)
    double food = 0;             // prepared food and meals
    double alcohol = 0;          // drinks served
    QList<QPair<QString, double>> parts;   // what the general rate is made of
    QStringList notes;           // local restaurant taxes not in the rate, and the like
    QString source, asOf;
    QString lookUp;              // the local part isn't in the table: look it up here
    QString confirm;             // worked out by rules from the state's file: confirm here
    QString problem;             // why there's no suggestion ("That ZIP code is in Texas")
};

class TaxAreas {
public:
    // The table built in (or a newer one saved in `dataDir`, if any).
    static const TaxAreas &instance();
    bool loaded() const { return loaded_; }
    QString asOf() const { return asOf_; }
    // States in the table, two letters.
    QStringList states() const;
    QString stateName(const QString &state) const;
    // The rates for a ZIP code in `state` ("" for the ZIP's own state), and
    // `where` among its choices (a Choice's key; "" for the usual one).
    TaxSuggestion suggest(const QString &state, const QString &zip, const QString &where = {}) const;
    // The state a ZIP code is in, if the table knows ("" if not).
    QString stateOf(const QString &zip) const;

    // Tests: a table from JSON text.
    static TaxAreas fromJson(const QByteArray &json);

private:
    void load(const QByteArray &json);
    bool loaded_ = false;
    QString asOf_;
    struct Impl;
    std::shared_ptr<const Impl> d_;
};

} // namespace vt::app
