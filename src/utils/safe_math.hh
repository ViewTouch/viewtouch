#pragma once

#include <cstddef>
#include <limits>

namespace vt {
namespace safe_math {

inline bool safe_mul_size_t(size_t a, size_t b, size_t &out) {
    if (b != 0 && a > std::numeric_limits<size_t>::max() / b) return false;
    out = a * b;
    return true;
}

inline bool safe_mul3_size_t(size_t a, size_t b, size_t c, size_t &out) {
    size_t t;
    if (!safe_mul_size_t(a, b, t)) return false;
    if (!safe_mul_size_t(t, c, out)) return false;
    return true;
}

} // namespace safe_math
} // namespace vt
