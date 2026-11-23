#pragma once

#include <cstdint>

// Deliberately independent of mining, inventory and the ECS: both the server
// and the regression tests use these bounds and progression rules.
namespace WoodcuttingRules {
constexpr uint32_t TreeRace = 34090;
constexpr bool Due(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
constexpr bool StayedInPlace(int32_t startX, int32_t startY, int32_t startMap,
    int32_t x, int32_t y, int32_t map) {
    return startX == x && startY == y && startMap == map;
}
}
