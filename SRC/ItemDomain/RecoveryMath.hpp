#pragma once
#include <cstdint>
#include <item/RefinementProfiles.hpp>

namespace auriga::item::detail
{
// Exact floor(value*bp/10000). Callers validate nonnegative value<=10^15
// and bp<=110000 first. Intermediate result <=11*10^15; no raw*bp overflow.
static_assert(MaxItemStatRaw <= INT64_MAX / 12);
constexpr std::int64_t ScaleRecovery(std::int64_t value, std::int64_t bp) noexcept
{
    return (value / 10000) * bp + (value % 10000) * bp / 10000;
}
constexpr bool ValidRecoveryPool(std::int64_t current, std::int64_t maximum) noexcept
{
    return current >= 0 && maximum >= 0 && maximum <= MaxItemStatRaw && current <= maximum;
}
}
