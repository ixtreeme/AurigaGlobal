#pragma once

#include <algorithm>
#include <vector>

#include <entt/entt.hpp>

#include "../../char.h"
#include "../../config.h"
#include "../../sectree.h"
#include "../../utils.h"
#include "../AIHelpers.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/session_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
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

    // Phase 15E-final.LPENTITY.4-architect.D.6.fixup-4:
    // Sectree lookup via ECS Position + MapIndex instead of
    // ecs::PlayerRuntime::GetSectree (which uses LegacyCharOf and only
    // works for character entities). Building / item / shop entities
    // are not characters - their LegacyCharPtr component is absent -
    // so PlayerRuntime::GetSectree returns nullptr and this function
    // returns an empty recipient set.
    //
    // This silently broke despawn broadcasts for Metin stone fragments,
    // dropped items, destroyed buildings, and closed offline shops:
    // SpatialService::RemoveEntity uses GetEntitiesInRange to compute
    // who needs the SendRemove packet. With an empty result, no remove
    // packet is emitted and the entity stays rendered on every viewer's
    // client until they relog or move out of range.
    //
    // ECS Position + MapIndex are populated for every spatial entity
    // type (chars, items, buildings, shops) by SpatialService::SyncSpatialComponents,
    // so the sectree lookup via SectorAt works uniformly.
    const auto* sourcePos = reg.try_get<ecs::Position>(source);
    const auto* sourceMap = reg.try_get<ecs::MapIndex>(source);
    if (!sourcePos || !sourceMap)
        return result;
    LPSECTREE sectree = ecs::SectorAt(sourceMap->value, sourcePos->x, sourcePos->y);
    if (!sectree)
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

inline std::vector<entt::entity> GetVisibleEntitiesNative(entt::registry& reg, entt::entity source)
{
    std::vector<entt::entity> result;
    if (source == entt::null || !reg.valid(source))
        return result;

    const auto* view = reg.try_get<ecs::ViewMap>(source);
    if (!view)
        return result;

    result.reserve(view->visible.size());
    for (const entt::entity visible : view->visible) {
        if (visible != entt::null && reg.valid(visible))
            PushUnique(result, visible);
    }
    return result;
}

inline std::vector<entt::entity> GetViewersOfNative(entt::registry& reg, entt::entity source)
{
    std::vector<entt::entity> result;
    if (source == entt::null || !reg.valid(source))
        return result;

    const auto* viewers = reg.try_get<ecs::ViewerMap>(source);
    if (!viewers)
        return result;

    result.reserve(viewers->viewers.size());
    for (const entt::entity viewer : viewers->viewers) {
        if (viewer != entt::null && reg.valid(viewer))
            PushUnique(result, viewer);
    }
    return result;
}

inline void OnPositionChanged(entt::registry&, entt::entity) {}
inline void OnSpawn(entt::registry&, entt::entity) {}
inline void OnRemove(entt::registry&, entt::entity) {}

} // namespace ecs::VisibilityService
