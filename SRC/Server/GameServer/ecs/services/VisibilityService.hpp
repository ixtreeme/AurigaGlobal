#pragma once

#include <algorithm>
#include <vector>

#include <entt/entt.hpp>

#include "../../char.h"
#include "../../config.h"
#include "../../sectree.h"
#include "../../utils.h"
#include "../AIHelpers.hpp"
#include "../components/session_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../systems/PlayerRuntimeSystem.hpp"

namespace ecs::VisibilityService {

inline void PushUnique(std::vector<entt::entity>& out, entt::entity e)
{
    if (e == entt::null)
        return;
    if (std::find(out.begin(), out.end(), e) == out.end())
        out.push_back(e);
}

inline std::vector<entt::entity> GetEntitiesInRange(entt::registry& reg, entt::entity source, int32_t range)
{
    std::vector<entt::entity> result;
    if (source == entt::null || !reg.valid(source))
        return result;

    const auto* sourcePos = reg.try_get<ecs::Position>(source);
    LPSECTREE sectree = ecs::PlayerRuntime::GetSectree(source);
    if (!sourcePos || !sectree)
        return result;

    struct Collector {
        entt::registry& reg;
        entt::entity source;
        const ecs::Position& sourcePos;
        int32_t range;
        std::vector<entt::entity>& result;

        void operator()(LPENTITY entity)
        {
            if (!entity || !entity->IsType(ENTITY_CHARACTER))
                return;

            const auto e = AIHelpers::EcsOf(static_cast<LPCHARACTER>(entity));
            if (e == entt::null || !reg.valid(e))
                return;

            const auto* pos = reg.try_get<ecs::Position>(e);
            if (!pos)
                return;

            if (e != source && DISTANCE_APPROX(pos->x - sourcePos.x, pos->y - sourcePos.y) > range)
                return;

            PushUnique(result, e);
        }
    } collector { reg, source, *sourcePos, range, result };

    sectree->ForEachAround(collector);
    PushUnique(result, source);
    return result;
}

inline std::vector<entt::entity> GetViewersOf(entt::registry& reg, entt::entity source)
{
    if (source == entt::null || !reg.valid(source))
        return {};

    if (const auto* status = reg.try_get<ecs::StatusFlags>(source); status && status->isObserverMode) {
        std::vector<entt::entity> selfOnly;
        PushUnique(selfOnly, source);
        return selfOnly;
    }

    return GetEntitiesInRange(reg, source, VIEW_RANGE + VIEW_BONUS_RANGE);
}

inline std::vector<entt::entity> GetVisibleEntities(entt::registry& reg, entt::entity source)
{
    return GetViewersOf(reg, source);
}

inline void OnPositionChanged(entt::registry&, entt::entity) {}
inline void OnSpawn(entt::registry&, entt::entity) {}
inline void OnRemove(entt::registry&, entt::entity) {}

} // namespace ecs::VisibilityService
