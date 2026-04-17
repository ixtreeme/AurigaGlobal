#pragma once

#include <cstdint>
#include <string>

#include "spatial_components.hpp"

namespace ecs {

struct VIDComponent { uint32_t value; };
struct PlayerID { uint32_t pid; };
struct AccountID { uint32_t aid; };
struct EmpireComponent {
    uint8_t value;
    uint32_t changeCount { 0 };
};
struct RaceComponent { uint16_t value; };
struct PlayerName { std::string value; };
struct GMLevel { uint8_t level; };

struct TagPC {};
struct TagNPC {};
struct TagMonster {};
struct TagStone {};
struct TagPet {};
struct TagMount {};
struct TagHorse {};

} // namespace ecs
