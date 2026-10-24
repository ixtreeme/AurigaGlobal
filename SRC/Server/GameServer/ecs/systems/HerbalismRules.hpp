#pragma once
#include <array>
#include <cstdint>

namespace HerbalismRules {
constexpr uint32_t FirstShears = 230, LastShears = 239;
constexpr uint32_t FirstPlant = 34800, LastPlant = 34811;
constexpr uint32_t FirstHerb = 50701;
constexpr uint32_t TrainingMarker = 0x48455242; // HERB, never sword sockets or axe training
constexpr std::array<uint32_t, 10> RequiredPoints {100,250,500,1000,2000,4000,8000,16000,32000,0};
constexpr bool IsShears(uint32_t vnum) { return vnum >= FirstShears && vnum <= LastShears; }
constexpr bool IsPlantRace(uint32_t race) { return race >= FirstPlant && race <= LastPlant; }
constexpr uint32_t HerbFor(uint32_t race) { return IsPlantRace(race) ? FirstHerb + race - FirstPlant : 0; }
constexpr uint32_t MaxPoints(uint32_t vnum) { return IsShears(vnum) ? RequiredPoints[vnum - FirstShears] : 0; }
constexpr bool CanRefine(uint32_t vnum, uint32_t points) { return IsShears(vnum) && vnum < LastShears && points >= MaxPoints(vnum); }
constexpr uint32_t AddPoint(uint32_t vnum, uint32_t points) {
    const auto max = MaxPoints(vnum);
    return max == 0 ? 0 : points >= max ? max : points + 1;
}
constexpr bool Due(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now-deadline) >= 0; }
}
