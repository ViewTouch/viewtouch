#include "core/rush.hh"

#include <algorithm>
#include <cmath>

namespace vt::core {

namespace {
constexpr double kKeep = 0.98;           // each ticket weighs a little less than the next (about the last 50)
constexpr double kPerItemDefault = 0.5;  // minutes an item ahead adds, before the station has shown its own
}

void StationPace::learn(double queue, double minutes)
{
    if (queue < 0 || minutes <= 0 || minutes > 90)
        return;   // a ticket forgotten on the screen says nothing about the kitchen
    n = n * kKeep + 1;
    sumQ = sumQ * kKeep + queue;
    sumT = sumT * kKeep + minutes;
    sumQQ = sumQQ * kKeep + queue * queue;
    sumQT = sumQT * kKeep + queue * minutes;
}

double StationPace::minutesAt(double queue, double typical) const
{
    if (!known())
        return std::max(1.0, typical) + kPerItemDefault * queue;
    const double meanQ = sumQ / n, meanT = sumT / n;
    const double varQ = sumQQ / n - meanQ * meanQ;
    // Each item ahead adds this much: from the tickets when the line has
    // varied enough to tell, never less than nothing nor more than 5 minutes.
    double slope = varQ > 0.5 ? (sumQT / n - meanQ * meanT) / varQ : kPerItemDefault;
    slope = std::clamp(slope, 0.05, 5.0);
    const double base = std::max(1.0, meanT - slope * meanQ);
    return base + slope * std::max(0.0, queue);
}

double StationPace::throughput(double typical) const
{
    // An item more ahead adding `slope` minutes means the station clears an
    // item every `slope` minutes.
    const double slope = minutesAt(1, typical) - minutesAt(0, typical);
    return std::clamp(1.0 / std::max(0.05, slope), 0.2, 20.0);
}

std::vector<RushPoint> projectRush(double queueNow, const std::vector<double> &arrivals, const StationPace &pace,
                                   double typicalMinutes)
{
    std::vector<RushPoint> out;
    out.reserve(arrivals.size() + 1);
    const double clears = pace.throughput(typicalMinutes);
    double q = std::max(0.0, queueNow);
    out.push_back({0, q, pace.minutesAt(q, typicalMinutes)});
    for (std::size_t m = 0; m < arrivals.size(); ++m) {
        q = std::max(0.0, q + std::max(0.0, arrivals[m]) - clears);
        out.push_back({int(m) + 1, q, pace.minutesAt(q, typicalMinutes)});
    }
    return out;
}

} // namespace vt::core
