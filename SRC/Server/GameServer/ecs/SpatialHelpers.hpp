#pragma once

#include <entt/entt.hpp>

#include "../sectree_manager.h"
#include "../typedef.h"
#include "Registry.hpp"
#include "components/spatial_components.hpp"

namespace ecs {

// Resolve the owning sector from authoritative ECS membership.
// Returns nullptr if the entity has no SectorPlacement component.
inline LPSECTREE SectorOf(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e)) return nullptr;
    auto* sp = reg.try_get<ecs::SectorPlacement>(e);
    if (!sp)
        return nullptr;

    return SECTREE_MANAGER::instance().Get(
        sp->mapIndex, static_cast<int32_t>(sp->sectorX), static_cast<int32_t>(sp->sectorY));
}

// Iterate all entities visible from the sector of entity `e`.
// Native callbacks take entt::entity; unmigrated callbacks use the sector adapter.
// Returns immediately if SectorPlacement is missing.
template <typename Func>
inline void ForEachAround(entt::registry& reg, entt::entity e, Func&& func)
{
    LPSECTREE tree = SectorOf(reg, e);
    if (!tree)
        return;

    tree->ForEachAround(func);
}

// Resolve LPSECTREE directly from map coordinates.
inline LPSECTREE SectorAt(int32_t mapIndex, int32_t x, int32_t y)
{
    return SECTREE_MANAGER::instance().Get(mapIndex, x, y);
}

// ECS-side wrapper for movable-position check.
// Delegates to legacy SECTREE_MANAGER::IsMovablePosition.
inline bool IsMovablePosition(int32_t mapIndex, int32_t x, int32_t y)
{
    return SECTREE_MANAGER::instance().IsMovablePosition(mapIndex, x, y);
}

// ECS-side wrapper for attribute flag check at position.
// Delegates to legacy SECTREE::IsAttr.
inline bool IsAttrAt(int32_t mapIndex, int32_t x, int32_t y, uint32_t dwFlag)
{
    LPSECTREE tree = SectorAt(mapIndex, x, y);
    if (!tree)
        return false;

    return tree->IsAttr(x, y, dwFlag);
}

// Empire lookup by map index.
inline uint8_t GetEmpireFromMap(int32_t mapIndex)
{
    return SECTREE_MANAGER::instance().GetEmpireFromMapIndex(mapIndex);
}

// Recall position lookup.
inline bool GetRecallPosition(int32_t mapIndex, uint8_t empire, PIXEL_POSITION& outPos)
{
    return SECTREE_MANAGER::instance().GetRecallPositionByEmpire(mapIndex, empire, outPos);
}

// Map metadata lookup by map index.
inline LPSECTREE_MAP GetMap(int32_t mapIndex)
{
    return SECTREE_MANAGER::instance().GetMap(mapIndex);
}

// Map index from world coordinates.
inline int32_t MapIndexAt(int32_t x, int32_t y)
{
    return SECTREE_MANAGER::instance().GetMapIndex(x, y);
}

// Movable position search (complex path - wraps as-is).
inline bool GetMovablePosition(int32_t mapIndex, int32_t x, int32_t y, PIXEL_POSITION& outPos)
{
    return SECTREE_MANAGER::instance().GetMovablePosition(mapIndex, x, y, outPos);
}

} // namespace ecs
