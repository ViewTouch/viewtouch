#pragma once

// Rush Radar: each kitchen station's next half hour. A station learns how
// long its tickets take for how much is ahead of them (StationPace); what's
// coming (items expected each minute) then gives the line and the wait,
// minute by minute (projectRush). Pure arithmetic: the POS gathers the inputs.

#include <vector>

namespace vt::core {

// Ticket time (sent -> made, minutes) against the items ahead at the station
// when it was sent: a line fitted to the latest tickets (older ones count
// less and less), so it follows the kitchen as it is.
struct StationPace {
    double n = 0, sumQ = 0, sumT = 0, sumQQ = 0, sumQT = 0;

    void learn(double queue, double minutes);
    bool known() const { return n >= 8; }
    // The ticket time with this much ahead: from the fitted line, or (too
    // few tickets yet) `typical` plus half a minute an item ahead.
    double minutesAt(double queue, double typical) const;
    // Items the station gets through in a minute (how fast a line goes down).
    double throughput(double typical) const;

    bool operator==(const StationPace &) const = default;
};

struct RushPoint {
    int minute = 0;      // from now
    double queue = 0;    // items waiting or being made
    double wait = 0;     // minutes for an item sent then
};

// The next `arrivals.size()` minutes: the line now, plus what comes each
// minute, less what the station gets through.
std::vector<RushPoint> projectRush(double queueNow, const std::vector<double> &arrivals, const StationPace &pace,
                                   double typicalMinutes);

} // namespace vt::core
