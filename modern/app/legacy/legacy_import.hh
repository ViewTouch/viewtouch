#pragma once

#include "app/legacy/legacy_files.hh"
#include "app/legacy/legacy_pages.hh"
#include "core/check.hh"
#include "core/customer.hh"
#include "core/day.hh"
#include "core/employee.hh"
#include "core/menu.hh"
#include "core/settings.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTimeZone>

#include <optional>
#include <vector>

// A legacy store's records as ViewTouch Modern's: its menu, staff,
// customers, settings, pages, dining room and closed business days, all
// worked out before anything changes (what the import shows first), then
// put into a store (applyImport, legacy_apply.hh).
namespace vt::legacy {

struct ImportParts {
    bool pages = true;
    bool menu = true;
    bool tables = true;
    bool settings = true;
    bool staff = true;
    bool customers = true;
    bool history = true;
};

// One closed business day, as Modern keeps it (ids given when it's put in).
struct ImportedDay {
    int legacyId = 0;
    std::int64_t openedAt = 0;   // epoch ms
    std::int64_t closedAt = 0;
    std::vector<core::Check> checks;          // closed, oldest first
    std::vector<core::TimePunch> punches;
    std::vector<core::DrawerSession> drawers; // checks' drawerSession: an index into these, plus one (0: none)
    core::TaxRates rates;
    vt::Money sales;                        // what legacy took (payments less change)
};

// A check whose total in Modern isn't what legacy took for it.
struct Mismatch {
    int day = 0;          // legacy's archive number
    int serial = 0;       // legacy's check number
    QString label;
    vt::Money legacy, modern;
};

struct Imported {
    // settings: what's set from legacy (the rest of the store's stays)
    std::optional<QString> storeName;
    std::optional<QString> receiptHeader, receiptFooter;
    std::optional<core::TaxRates> tax;
    std::vector<core::Tender> tenders;         // added to the store's
    std::vector<core::PosSettings::Promotion> promotions;
    std::vector<core::PrinterConfig> printers; // by id, replacing the store's of that id
    std::vector<core::MenuCategory> categories;
    // records
    std::vector<core::MenuItem> menu;
    std::vector<core::Employee> employees;
    std::vector<core::CustomerRecord> customers;
    QList<QJsonObject> pages;                  // the store's own, in the Legacy style
    QList<NotInModern> notInModern;
    std::optional<QJsonArray> floor;           // its dining room's tables (table zones)
    std::vector<ImportedDay> days;
    // what it says before and after
    QStringList managersNewPin;                // short legacy PINs: asked for a new one at first login
    QStringList notes;                         // what isn't brought over, and why
    std::vector<Mismatch> mismatches;
    QStringList errors;                        // files that couldn't be read
};

struct ImportContext {
    ImportParts parts;
    // Legacy kept the store's clock without a zone: the zone it was in.
    QTimeZone zone = QTimeZone::LocalTime;
    // The store's own tenders and printers, so legacy's are added beside them.
    const core::PosSettings *current = nullptr;
};

// Everything worked out; nothing changed anywhere.
Imported convert(const Store &store, const ImportContext &context);

// What an import will do, for a person to read first: {storeName, pages,
// items, categories, tables, staff, managersNewPin, customers, days, from,
// to, checks, sales, notes, errors}.
QVariantMap preview(const Store &store, const Imported &imported);

// A legacy wall-clock time as a moment (epoch ms) in `zone`; 0 when unset.
std::int64_t toEpoch(const WallTime &t, const QTimeZone &zone);

// Legacy's family numbers' names (Appetizer, Beverage...).
QString familyName(int family);

} // namespace vt::legacy
