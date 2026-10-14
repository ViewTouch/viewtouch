#pragma once

// Shared helpers for POS tests: the shipped seed data and a recording sink.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJsonDocument>

#include <map>

namespace vt::test {

inline QJsonDocument readSeed(const char *relative)
{
    QFile f(QStringLiteral(VTM_SEED_DIR "/") + QString::fromLatin1(relative));
    REQUIRE(f.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(f.readAll());
}

// The starter data. Modifier choices are optional here unless
// `requiredChoices`: tests about other things can send and close without
// picking a beer; the modifier tests ask for the store's real rules.
inline app::PosData seedPosData(bool requiredChoices = false)
{
    app::PosData d;
    d.settings = app::settingsFromJson(readSeed("pos/settings.json").object());
    d.menu = app::menuFromJson(readSeed("pos/menu.json").array());
    d.employees = app::employeesFromJson(readSeed("pos/employees.json").array());
    d.ingredients = app::ingredientsFromJson(readSeed("pos/ingredients.json").array());
    // The drawer tests were written for a cash drawer per terminal; server
    // banks (the starter setting) have tests of their own.
    d.settings.cashMode = core::CashMode::TerminalDrawer;
    // Prices must not depend on the time the tests run: meal-period prices
    // have tests of their own, with a fixed clock.
    for (core::MenuItem &m : d.menu)
        m.periodPrices.clear();
    d.settings.promotions.clear();   // they depend on the day and time: their own tests set a clock
    d.settings.setupDone = true;     // the setup guide has tests of its own
    if (!requiredChoices) {
        for (core::ModifierGroup &g : d.settings.modifierGroups)
            g.min = 0;
    }
    return d;
}

struct RecordingSink : app::PosSink {
    // The menu as stored: saved items by id (deleted ones gone).
    std::map<std::string, core::MenuItem> menu;
    void saveMenuItem(const core::MenuItem &m, int) override { menu[m.id] = m; }
    void deleteMenuItem(const std::string &id) override { menu.erase(id); }
    QStringList savedMenuIds() const
    {
        QStringList out;
        for (const auto &[id, m] : menu)
            out << QString::fromStdString(id);
        return out;
    }
    std::map<std::int64_t, core::Check> checks;
    std::map<std::int64_t, core::TimePunch> punches;
    int checkSaves = 0;

    void saveCheck(const core::Check &c) override
    {
        checks[c.id] = c;
        ++checkSaves;
    }
    void savePunch(const core::TimePunch &p) override { punches[p.id] = p; }
};

} // namespace vt::test
