#pragma once

#include <cstdint>
#include <string>

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

// Authoritative sector membership, maintained only by SECTREE insert/remove.
// sectorX/Y contain world coordinates used to resolve the owning sector.
struct SectorPlacement {
    int32_t mapIndex;
    uint32_t sectorX;
    uint32_t sectorY;
};

// Tag: entity is currently visible (in-sector, has active view).
// Set by the spatial lifecycle.
// Cleared on RemoveEntity / despawn.
struct ViewActiveTag {};

// Survives detach/reinsert, so an older publication cannot act on a new spawn
// of the same entity at the same coordinates.
struct SpatialRevision { uint64_t value { 0 }; };

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
    std::string name {};
};

} // namespace ecs
