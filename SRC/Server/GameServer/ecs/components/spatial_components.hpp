#pragma once

#include <cstdint>

namespace ecs {

// Marker tag: entity participates in sectree visibility.
struct SpatialEntity {};

// ECS-side entity kind for future tag-based replacement of CEntity::m_iType.
enum class SpatialKind : uint8_t {
    Character = 0,
    Item = 1,
    Building = 2,
    OfflineShop = 3,
};

struct SpatialKindTag {
    SpatialKind kind;
};

// Transitional explicit Z component. Position already stores z, but this keeps
// Z coverage queryable while LPENTITY position state is being dismantled.
struct PositionZ {
    int32_t z;
};

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

struct BuildingState {
    uint32_t vnum { 0 };
    uint32_t landId { 0 };
    uint32_t guildId { 0 };
    float rotationX { 0.0f };
    float rotationY { 0.0f };
    float rotationZ { 0.0f };
};

struct OfflineShopState {
    uint32_t vid { 0 };
    uint32_t race { 0 };
    int shopType { 0 };
};

} // namespace ecs
