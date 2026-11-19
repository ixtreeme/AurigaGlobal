#pragma once
#include <algorithm>
#include <cstdint>
namespace auriga::item {
// The separate premium point uses whole damage points, a 20-percent aggregate
// limit and a 300-point addition ceiling. It is not the attack stat multiplier.
constexpr std::int64_t PremiumAttackAddition(std::int64_t damage,std::int64_t percent) noexcept
{
    if(damage<=0 || percent<=0)return 0;
    percent=std::min<std::int64_t>(percent,20);
    if(damage>=30000)return 300; // Even one percent reaches the ceiling.
    return std::min<std::int64_t>(300,damage*percent/100);
}
}
