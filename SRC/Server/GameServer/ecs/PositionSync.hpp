#pragma once

#include "EventDispatcher.hpp"
#include "Registry.hpp"
#include "components/dirty_components.hpp"
#include "components/spatial_components.hpp"
#include "components/transform_components.hpp"
#include "events.hpp"

namespace ecs {

inline void SyncPositionComponents(entt::registry& reg, entt::entity e, int32_t mapIndex, int32_t x, int32_t y, int32_t z)
{
    if (e == entt::null || !reg.valid(e))
        return;

    // Phase 15E-final.LPENTITY.4-architect.D.2:
    // Capture old position+map BEFORE the writes so the PositionChangedEvent
    // carries an accurate diff. If Position is missing this is a first-time
    // emplace (spawn-adjacent path) and we use sentinel zeros - the
    // VisibilitySystem (D.4) treats oldMapIndex==0 as "no prior viewers".
    int32_t oldX = 0;
    int32_t oldY = 0;
    int32_t oldZ = 0;
    int32_t oldMapIndex = 0;
    if (const auto* p = reg.try_get<ecs::Position>(e)) {
        oldX = p->x;
        oldY = p->y;
        oldZ = p->z;
    }
    if (const auto* m = reg.try_get<ecs::MapIndex>(e))
        oldMapIndex = m->value;

    reg.emplace_or_replace<ecs::Position>(e, x, y, z);
    reg.emplace_or_replace<ecs::PositionZ>(e, z);
    reg.emplace_or_replace<ecs::MapIndex>(e, mapIndex);
    reg.emplace_or_replace<ecs::DirtyTag>(e);

    // Suppress no-op events to keep the dispatcher quiet on idempotent writes
    // (Sync called twice with the same coordinates is a frequent pattern in
    // the legacy paths).
    if (oldX == x && oldY == y && oldZ == z && oldMapIndex == mapIndex)
        return;

    g_dispatcher.trigger(ecs::PositionChangedEvent {
        e,
        oldX, oldY, oldZ,
        x, y, z,
        oldMapIndex, mapIndex });
}

} // namespace ecs
