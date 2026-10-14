#pragma once

#include "core/check.hh"
#include "core/money.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace vt::core {

// One change to a balance: + loaded / charged, - spent / paid.
struct LedgerEntry {
    std::int64_t at = 0;
    Money amount;
    std::string what;          // "Sold on check #12", "Paid check #15", "Payment (cash)"...
    std::int64_t checkId = 0;
    // sale (card sold / reloaded) | spend | refund (back to the card) | reopen |
    // charge | uncharge (charge removed) | payment
    std::string kind;
    bool operator==(const LedgerEntry &) const = default;
};

// A regular: found by phone or name when they call or come in. Their
// details fill a takeout / delivery check, and closed checks add up their
// visits. A house account lets them charge to the store and pay later.
struct CustomerRecord {
    std::string id;
    std::string name;
    std::string phone;
    std::string email;
    std::string address;
    std::string note;          // "allergic to nuts", "gate code 1234"...
    std::int64_t createdAt = 0;

    int visits = 0;            // closed checks
    // What they had last time (for "Same as Last Time"), and when.
    std::vector<OrderLine> lastOrder;
    std::int64_t lastOrderAt = 0;
    Money spent;               // their totals, before tips
    std::int64_t lastVisit = 0;

    int points = 0;            // loyalty points to spend
    int lifetimePoints = 0;

    bool houseAccount = false;
    Money accountLimit;        // 0 = no limit
    Money accountBalance;      // what they owe
    std::vector<LedgerEntry> account;   // charges and payments, oldest first

    void post(std::int64_t at, Money amount, std::string what, std::int64_t checkId, std::string kind)
    {
        accountBalance += amount;
        account.push_back({at, amount, std::move(what), checkId, std::move(kind)});
    }

    // Phone digits only, for matching "555-0101" with "(555) 0101".
    static std::string digits(const std::string &phone)
    {
        std::string out;
        for (char ch : phone) {
            if (ch >= '0' && ch <= '9')
                out += ch;
        }
        return out;
    }
    // Room left on the account (a large number when there is no limit).
    Money accountRoom() const
    {
        return accountLimit.cents() <= 0 ? Money::fromCents(INT64_MAX / 4) : accountLimit - accountBalance;
    }

    bool operator==(const CustomerRecord &) const = default;
};

// A gift card: sold or reloaded on a check (live once that check is paid),
// spent as a payment. Every change is kept.
struct GiftCard {
    using Entry = LedgerEntry;

    std::string number;
    Money balance;
    std::int64_t issuedAt = 0;
    std::vector<Entry> history;    // oldest first

    void post(std::int64_t at, Money amount, std::string what, std::int64_t checkId, std::string kind)
    {
        balance += amount;
        history.push_back({at, amount, std::move(what), checkId, std::move(kind)});
    }

    bool operator==(const GiftCard &) const = default;
};

} // namespace vt::core
