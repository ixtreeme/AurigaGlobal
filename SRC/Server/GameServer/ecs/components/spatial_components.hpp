#pragma once

#include <cstdint>

namespace ecs {

// Mirrors the entity's current sector placement.
// Updated additively alongside legacy SECTREE::InsertEntity / RemoveEntity.
struct SectorPlacement {
    int32_t mapIndex;
    uint32_t sectorX;
    uint32_t sectorY;
};

} // namespace ecs
