#pragma once

// Money as a signed count of cents. Replaces the legacy mix of int cents and
// Flt rates; all arithmetic is integral and rounding is explicit.

#include <compare>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace vt {

class Money {
public:
    constexpr Money() = default;
    static constexpr Money fromCents(std::int64_t cents) { return Money(cents); }

    constexpr std::int64_t cents() const { return cents_; }

    constexpr Money operator+(Money o) const { return Money(cents_ + o.cents_); }
    constexpr Money operator-(Money o) const { return Money(cents_ - o.cents_); }
    constexpr Money operator-() const { return Money(-cents_); }
    constexpr Money operator*(std::int64_t qty) const { return Money(cents_ * qty); }
    constexpr Money &operator+=(Money o) { cents_ += o.cents_; return *this; }
    constexpr Money &operator-=(Money o) { cents_ -= o.cents_; return *this; }

    constexpr auto operator<=>(const Money &) const = default;

    // this * numerator / denominator, rounded half away from zero to the
    // nearest cent (the legacy FltToPrice rule), in exact integer math.
    constexpr Money scaled(std::int64_t numerator, std::int64_t denominator) const
    {
        const std::int64_t num = cents_ * numerator;
        const std::int64_t q = num / denominator;
        const std::int64_t r = num % denominator;
        const std::int64_t twice = (r < 0 ? -r : r) * 2;
        if (twice >= (denominator < 0 ? -denominator : denominator))
            return Money(num < 0 ? q - 1 : q + 1);
        return Money(q);
    }

    // Percentage in basis points (1 bp = 0.01%), e.g. 10% = 1000.
    constexpr Money percent(std::int64_t basisPoints) const { return scaled(basisPoints, 10000); }

    // Rate in parts per million, e.g. 9.975% = 99750. Tax rates use this so
    // three-decimal rates (Quebec QST) are exact.
    constexpr Money ppm(std::int64_t partsPerMillion) const { return scaled(partsPerMillion, 1000000); }

    // "12.34", "-0.05"
    std::string toString() const
    {
        const std::int64_t a = cents_ < 0 ? -cents_ : cents_;
        std::string frac = std::to_string(a % 100);
        if (frac.size() < 2) frac.insert(0, "0");
        return (cents_ < 0 ? "-" : "") + std::to_string(a / 100) + "." + frac;
    }

private:
    constexpr explicit Money(std::int64_t cents) : cents_(cents) {}
    std::int64_t cents_ = 0;
};

} // namespace vt
