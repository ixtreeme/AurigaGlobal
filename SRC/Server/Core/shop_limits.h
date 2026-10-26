#pragma once
#include <cstdint>
#include <limits>
#include <optional>

namespace auriga::shop_limits
{
// Check before multiplying: signed overflow is undefined, even if a later
// wallet-limit check would reject the wrapped result.
inline std::optional<int64_t> Multiply(int64_t price, uint64_t count)
{
    if (price < 0 || (count && uint64_t(price) > uint64_t(std::numeric_limits<int64_t>::max()) / count))
        return {};
    return int64_t(uint64_t(price) * count);
}
inline bool Ready(uint32_t now, uint32_t previous, uint32_t delay)
{
    return !delay || uint32_t(now - previous) >= delay;
}
} // namespace auriga::shop_limits
