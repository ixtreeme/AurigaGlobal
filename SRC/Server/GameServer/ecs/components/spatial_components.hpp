#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <entt/entity/entity.hpp>

namespace ecs {

// Marker tag: entity participates in sectree visibility.
struct SpatialEntity {};

// Final retirement is distinct from a reversible sector detach/move. A remove
// callback must not republish the world object whose lifetime is ending.
struct SpatialRetiring {};

// Runtime world-object kind. Identity and state live exclusively in components.
enum class SpatialKind : uint8_t {
    Character = 0,
    Item = 1,
    Building = 2,
    TypedLoot = 3, // typed ground loot (item system F4 C7c, IC-035)
};

struct SpatialKindTag {
    SpatialKind kind;
};

// What a typed ground loot entry shows to the clients that see it: the encoded
// GC_TYPED_GROUND_ADD, and the claimant's name for GC_ITEM_OWNERSHIP while the
// claim lasts. The loot itself is held by GameServer/item/TypedLoot.cpp.
struct TypedLootView {
    uint32_t vid = 0;
    std::vector<std::byte> insert;
    std::string claimName;
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
    uint32_t objectId { 0 };
    int32_t life { 0 };
    entt::entity npc { entt::null };
    bool destroying { false };
    bool attributesApplied { false };
    uint32_t effectGuildId { 0 };
};

} // namespace ecs
