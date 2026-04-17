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

// Tag: entity is currently visible (in-sector, has active view).
// Set additively after legacy view/sectree updates succeed.
// Cleared on RemoveEntity / despawn.
struct ViewActiveTag {};

} // namespace ecs
