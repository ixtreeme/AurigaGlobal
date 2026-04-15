#pragma once

#include <cstdint>

#include "../../char.h"

namespace ecs {

struct Health { int32_t current; int32_t max; };
struct Mana { int32_t current; int32_t max; };
struct Stamina { int32_t current; int32_t max; };

struct LevelComponent { int32_t value; };
struct Experience { int64_t current; int64_t next; };

struct CharacterPoints {
    CHARACTER_POINT base;
    CHARACTER_POINT_INSTANT instant;
};

} // namespace ecs
