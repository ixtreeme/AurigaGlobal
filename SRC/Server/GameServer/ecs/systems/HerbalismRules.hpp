#pragma once
#include <cstdint>

namespace HerbalismRules {
constexpr uint32_t FirstPlant = 34800, LastPlant = 34811;
constexpr bool IsPlantRace(uint32_t race) { return race >= FirstPlant && race <= LastPlant; }
constexpr bool Due(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now-deadline) >= 0; }
}
