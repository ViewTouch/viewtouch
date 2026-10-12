#pragma once

#include "app/pos_service.hh"

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace vt::storage {

class AsyncWriter;

// POS tables in the shared SQLite database: settings, menu, employees,
// checks, time punches. Reads happen here (startup, reports); writes during
// service go through AsyncWriter via SqlPosSink.
class PosStore {
public:
    static constexpr int DbSchemaVersion = 15;

    explicit PosStore(QString databasePath);
    ~PosStore();
    PosStore(const PosStore &) = delete;
    PosStore &operator=(const PosStore &) = delete;

    bool open(QString *error = nullptr);
    QString path() const { return path_; }

    bool hasMenu() const;
    // First-run setup (one transaction). Replaces settings, menu, employees
    // and the inventory.
    bool seed(const core::PosSettings &settings, const std::vector<core::MenuItem> &menu,
              const std::vector<core::Employee> &employees, QString *error = nullptr,
              const std::vector<core::Ingredient> &ingredients = {});

    // Settings, menu, employees, open checks, open punches, and id counters.
    std::optional<app::PosData> load(QStringList *errors = nullptr) const;

    std::vector<core::Check> checks(core::CheckStatus status) const;
    std::vector<core::TimePunch> punches() const;

private:
    QString path_;
    QString connection_;
};

// PosSink that queues rows on an AsyncWriter.
class SqlPosSink : public app::PosSink {
public:
    explicit SqlPosSink(AsyncWriter &writer) : writer_(writer) {}
    void saveCheck(const core::Check &check) override;
    void savePunch(const core::TimePunch &punch) override;
    void saveDay(const core::BusinessDay &day, const QJsonObject &reports) override;
    void saveDrawer(const core::DrawerSession &drawer) override;
    void saveSettings(const core::PosSettings &settings) override;
    void saveMenuItem(const core::MenuItem &item, int position) override;
    void deleteMenuItem(const std::string &id) override;
    void saveEmployee(const core::Employee &employee) override;
    void saveCustomer(const core::CustomerRecord &customer) override;
    void saveGiftCard(const core::GiftCard &card) override;
    void saveParty(const core::Party &party) override;
    void saveIngredient(const core::Ingredient &ingredient, int position) override;
    void deleteIngredient(const std::string &id) override;
    void saveShift(const core::Shift &shift) override;
    void saveDelivery(const core::Delivery &delivery) override;
    void saveRefund(const core::Refund &refund) override;
    void saveChange(const core::Change &change) override;
    void saveStockCount(const core::StockCount &count) override;
    void saveImage(const std::string &name, const QByteArray &data) override;
    void deleteImage(const std::string &name) override;
    void deleteShift(std::int64_t id) override;
    void deletePunch(std::int64_t id) override;

private:
    AsyncWriter &writer_;
};

// Checks closed in [from, to) (epoch ms), read with a connection of its
// own: safe on a worker thread while the POS runs. For reports over a range.
// Time punches clocked in between from and to (Manager -> Time Punches, older ones).
std::vector<core::TimePunch> punchesBetween(const QString &dbPath, std::int64_t from, std::int64_t to);
// Closed checks, the newest first: closed in [from, to) (to 0: no end),
// whose saved record contains any of `words` (none: every one), `limit`
// of them from `offset`. Its own connection: for a worker thread.
// Refunds made in [from, to), any check's. Its own connection: for a worker thread.
std::vector<core::Refund> refundsBetween(const QString &dbPath, std::int64_t from, std::int64_t to);
std::vector<core::Check> findClosedChecks(const QString &dbPath, std::int64_t from, std::int64_t to,
                                          const QStringList &words, int limit, int offset, QString *error = nullptr);
std::vector<core::Check> closedChecksBetween(const QString &dbPath, std::int64_t from, std::int64_t to,
                                             QString *error = nullptr);

} // namespace vt::storage
