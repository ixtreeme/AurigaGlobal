#pragma once

#include "Registry.hpp"
#include "components/dirty_components.hpp"
#include "components/transform_components.hpp"

namespace ecs {

inline void SyncPositionComponents(entt::registry& reg, entt::entity e, int32_t mapIndex, int32_t x, int32_t y, int32_t z)
{
    if (e == entt::null || !reg.valid(e))
        return;

    reg.emplace_or_replace<ecs::Position>(e, x, y, z);
    reg.emplace_or_replace<ecs::MapIndex>(e, mapIndex);
    reg.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace ecs
