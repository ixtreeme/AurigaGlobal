#pragma once

#include <array>
#include <cstdint>

// Deliberately independent of mining, inventory and the ECS: both the server
// and the regression tests use these bounds and progression rules.
namespace WoodcuttingRules {
constexpr uint32_t FirstAxe = 210;
constexpr uint32_t LastAxe = 219;
constexpr uint32_t TreeRace = 34090;
constexpr uint32_t LogItem = 90011;
constexpr uint32_t TrainingMarker = 0x57434f44; // WCOD; legacy sword sockets are never training
constexpr std::array<uint32_t, 10> RequiredPoints {
    100, 250, 500, 1000, 2000, 4000, 8000, 16000, 32000, 0
};
constexpr bool IsAxe(uint32_t vnum) { return vnum >= FirstAxe && vnum <= LastAxe; }
constexpr uint32_t MaxPoints(uint32_t vnum) {
    return IsAxe(vnum) ? RequiredPoints[vnum - FirstAxe] : 0;
}
constexpr bool CanRefine(uint32_t vnum, uint32_t points) {
    return IsAxe(vnum) && vnum < LastAxe && points >= MaxPoints(vnum);
}
constexpr uint32_t AddPoint(uint32_t vnum, uint32_t points) {
    const auto maximum = MaxPoints(vnum);
    return maximum == 0 ? 0 : points >= maximum ? maximum : points + 1;
}
constexpr bool Due(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
constexpr bool StayedInPlace(int32_t startX, int32_t startY, int32_t startMap,
    int32_t x, int32_t y, int32_t map) {
    return startX == x && startY == y && startMap == map;
}
}
