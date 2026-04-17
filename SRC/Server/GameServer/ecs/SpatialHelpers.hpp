#pragma once

#include <entt/entt.hpp>

#include "../sectree_manager.h"
#include "../typedef.h"
#include "Registry.hpp"
#include "components/spatial_components.hpp"

namespace ecs {

// Resolve the legacy LPSECTREE for an ECS entity.
// Returns nullptr if the entity has no SectorPlacement component.
inline LPSECTREE SectorOf(entt::registry& reg, entt::entity e)
{
    auto* sp = reg.try_get<ecs::SectorPlacement>(e);
    if (!sp)
        return nullptr;

    return SECTREE_MANAGER::instance().Get(
        sp->mapIndex, static_cast<int32_t>(sp->sectorX), static_cast<int32_t>(sp->sectorY));
}

// Update SectorPlacement after a legacy position change.
// Call this after any legacy sectree insert/move, not before.
inline void SyncSectorPlacement(entt::registry& reg,
                                entt::entity e,
                                int32_t mapIndex,
                                int32_t x, int32_t y)
{
    if (e == entt::null)
        return;

    reg.emplace_or_replace<ecs::SectorPlacement>(
        e,
        ecs::SectorPlacement{ mapIndex, static_cast<uint32_t>(x), static_cast<uint32_t>(y) });
}

// Iterate all entities visible from the sector of entity `e`.
// Calls func(LPENTITY) for each entity in the sector and its neighbors.
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

} // namespace ecs
