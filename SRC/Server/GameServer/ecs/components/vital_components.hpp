#pragma once

#include <cstdint>

#include "../../char.h"

namespace ecs {

struct Health { int32_t current; int32_t max; };
struct Mana { int32_t current; int32_t max; };
struct Stamina { int32_t current; int32_t max; };

struct LevelComponent { int32_t value; };
struct Experience { int64_t current; int64_t next; };

// Only the persistent half. The instant array that used to sit beside it was
// a second copy of CharacterStatsComponent::points, kept in step by
// PointSystem::Set and cleared by the recompute, and read by nothing.
struct CharacterPoints {
    CHARACTER_POINT base;
};

} // namespace ecs
