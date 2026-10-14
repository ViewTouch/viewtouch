#pragma once

#include <cstdint>
#include <string>

namespace vt::core {

// A party waiting for a table, or booked for later. A reservation (with
// `reservedFor`) joins the line when the guests arrive.
struct Party {
    enum class Status { Booked, Waiting, Notified, Seated, Left, NoShow };

    std::int64_t id = 0;
    std::string name;
    std::string phone;
    int size = 2;
    std::string note;             // "high chair", "booth please"...
    std::string customerId;       // when they are on file

    std::int64_t addedAt = 0;     // joined the line (or booked)
    std::int64_t reservedFor = 0; // reservation time; 0 = walk-in
    int quotedMinutes = 0;        // the wait they were told
    std::int64_t arrivedAt = 0;   // reservation: checked in
    std::int64_t notifiedAt = 0;  // told their table is ready
    std::int64_t seatedAt = 0;
    std::string table;            // where they sat
    bool walkIn = false;          // seated straight from the door (no wait to count)
    std::int64_t checkId = 0;
    Status status = Status::Waiting;

    bool inLine() const { return status == Status::Waiting || status == Status::Notified; }
    bool done() const { return status == Status::Seated || status == Status::Left || status == Status::NoShow; }
    // When their wait started: arrival for a reservation, else joining.
    std::int64_t waitingSince() const { return arrivedAt ? arrivedAt : addedAt; }

    bool operator==(const Party &) const = default;
};

std::string toString(Party::Status s);
Party::Status partyStatusFromString(const std::string &s);

} // namespace vt::core
