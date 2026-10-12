#pragma once

#include "core/money.hh"

#include <cstdint>
#include <set>
#include <algorithm>
#include <string>
#include <vector>

namespace vt::core {

// Permissions replace the legacy "employee id 1 is superuser, 2 is editor".
namespace perm {
inline constexpr const char *Order = "order";            // take orders
inline constexpr const char *Settle = "check.settle";    // take payments, close checks
inline constexpr const char *Void = "order.void";        // void items already sent
inline constexpr const char *Manager = "manager";        // manager pages
inline constexpr const char *EditLayout = "layout.edit"; // edit pages
inline constexpr const char *Discount = "check.discount"; // discounts and comps
inline constexpr const char *OthersChecks = "check.others"; // open checks that are someone else's
} // namespace perm

// Every permission, for the per-person settings.
inline constexpr const char *AllPermissions[] = {perm::Order, perm::Settle, perm::OthersChecks, perm::Discount,
                                                 perm::Void, perm::Manager, perm::EditLayout};

// Built-in roles. M4 makes these editable.
std::set<std::string> permissionsForRole(const std::string &role);

// A job someone can work, at its pay (dollars an hour, before tips).
struct Job {
    std::string role;   // server, bartender...
    Money rate;
    bool operator==(const Job &) const = default;
};

struct Employee {
    std::string id;
    std::string name;
    std::string role = "server";   // server | bartender | cashier | host | busser | manager | admin
    // PINs are stored hashed (see app::hashPin); never the digits.
    std::string pinSalt;
    std::string pinHash;
    bool active = true;
    bool sample = false;   // one of the demo's staff (their PINs are public): the setup guide turns them off
    // Practice only: their checks never reach the kitchen, sales or stock.
    bool training = false;
    // Cash handling for this person: "serverBank" (own bank), "drawer" (the
    // terminal's drawer), or empty for the store's setting.
    std::string language;          // "en", "es"...; empty: the store's
    // Their own screen: text size (percent, 100 normal), the order screens
    // mirrored for the left hand (the check on the right), and the page they
    // start on at login (empty: their job's, PosSettings::startPages).
    int textSize = 100;
    bool leftHanded = false;
    std::string startPage;
    // Pay for their role, and other jobs they can clock in as (a server who
    // also bartends), each at its own rate.
    Money payRate;
    std::vector<Job> otherJobs;
    std::string cashMode;
    // Phone orders need a name before Send: "yes", "no", or empty = the store's.
    std::string requireName;
    // Checking out with checks still open: "closeChecks" (not allowed),
    // "anyTime" (allowed), or empty for the store's setting.
    std::string checkout;
    // Counting a drawer or bank: "blind" (not seeing what's expected), "sees",
    // or empty for the store's setting (PosSettings::blindDrawerCount).
    std::string blindCount;
    // In the tip pool: "" by their job (PosSettings::tipPool), "in" always
    // (a full share), "out" never (they keep their own tips).
    std::string tipPool;

    // Per-person changes to the role's permissions.
    std::set<std::string> allow;
    std::set<std::string> deny;

    std::set<std::string> permissions() const
    {
        std::set<std::string> out = permissionsForRole(role);
        out.insert(allow.begin(), allow.end());
        for (const std::string &p : deny)
            out.erase(p);
        return out;
    }
    bool can(const std::string &permission) const { return permissions().contains(permission); }
    // Their role first, then the other jobs.
    std::vector<Job> jobs() const
    {
        std::vector<Job> out{{role, payRate}};
        for (const Job &j : otherJobs)
            if (j.role != role)
                out.push_back(j);
        return out;
    }
    bool operator==(const Employee &) const = default;
};

// One shift on the clock. clockOut == 0 means still clocked in.
struct TimePunch {
    std::int64_t id = 0;
    std::string employeeId;
    std::int64_t clockIn = 0;    // epoch ms
    std::int64_t clockOut = 0;
    struct Break {
        std::int64_t start = 0;
        std::int64_t end = 0;    // 0: still on it
        bool operator==(const Break &) const = default;
    };
    std::vector<Break> breaks;
    // The job worked and its pay, as they were at clock in (a raise later
    // doesn't change what this shift cost).
    std::string job;
    Money rate;
    // Cash tips they kept, told at clock out (PosSettings::declareCashTips).
    bool cashTipsDeclared = false;
    Money cashTips;

    bool open() const { return clockOut == 0; }
    bool onBreak() const { return !breaks.empty() && breaks.back().end == 0; }
    // Time on the clock / on breaks, up to `now` for what is still running.
    std::int64_t spanMs(std::int64_t now) const { return std::max<std::int64_t>(0, (open() ? now : clockOut) - clockIn); }
    std::int64_t breakMs(std::int64_t now) const
    {
        std::int64_t ms = 0;
        for (const Break &b : breaks)
            ms += std::max<std::int64_t>(0, (b.end == 0 ? (open() ? now : clockOut) : b.end) - b.start);
        return ms;
    }
    // Paid time: breaks count only if they are paid.
    std::int64_t workedMs(std::int64_t now, bool paidBreaks) const
    {
        return paidBreaks ? spanMs(now) : std::max<std::int64_t>(0, spanMs(now) - breakMs(now));
    }
    bool operator==(const TimePunch &) const = default;
};

// A scheduled shift. With PosSettings::scheduleRequired, staff clock in
// only from a little before a shift until it ends.
struct Shift {
    std::int64_t id = 0;
    std::string employeeId;
    std::int64_t start = 0;   // epoch ms
    std::int64_t end = 0;
    std::string note;         // "patio", "close"...

    double hours() const { return double(end - start) / 3'600'000.0; }
    bool operator==(const Shift &) const = default;
};

} // namespace vt::core
