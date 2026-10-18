#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace time64 {
// Persisted Unix deadlines are nonnegative; saturate an untrusted duration
// before addition rather than overflowing the signed timestamp.
inline int64_t AddSeconds(int64_t epoch, int64_t duration)
{
    epoch = std::max<int64_t>(0, epoch);
    duration = std::max<int64_t>(0, duration);
    return duration > INT64_MAX - epoch ? INT64_MAX : epoch + duration;
}
} // namespace time64
