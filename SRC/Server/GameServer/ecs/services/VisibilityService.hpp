#pragma once

#include <algorithm>
#include <vector>

#include <entt/entt.hpp>

#include "SpatialService.hpp"
#include "../../story/SpyQuestSystem.hpp"
#include "../../core/config.h"
#include "../../world/sectree.h"
#include "../../core/utils.h"
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

// Characters on the source's map within `range` of it, each once, in sector
// walk order; the source itself is always included, last if the walk did not
// list it. Runs for every broadcast, so the walk copies nothing and the
// component pools are resolved once per call, not per member.
inline std::vector<entt::entity> GetEntitiesInRange(entt::registry& reg, entt::entity source, int32_t range)
{
    std::vector<entt::entity> result;
    if (source == entt::null || !reg.valid(source))
        return result;

    // Resolve from position/map so a detached source can still notify nearby
    // clients. Membership of each recipient is checked by native enumeration.
    const auto* sourcePos = reg.try_get<ecs::Position>(source);
    const auto* sourceMap = reg.try_get<ecs::MapIndex>(source);
    if (!sourcePos || !sourceMap)
        return result;
    LPSECTREE sectree = ecs::SectorAt(sourceMap->value, sourcePos->x, sourcePos->y);
    if (!sectree)
        return result;

    const auto origin = *sourcePos;
    const int32_t mapIndex = sourceMap->value;
    const auto& kinds = reg.storage<ecs::SpatialKindTag>();
    const auto& positions = reg.storage<ecs::Position>();
    const auto& maps = reg.storage<ecs::MapIndex>();
    const bool storyActor = reg.all_of<SpyQuest::Actor>(source);
    bool sourceListed = false;
    // A sector lists each member once and every member resolves to exactly one
    // sector (SECTREE::VisitAround), so the walk cannot repeat an entity.
    sectree->VisitAround([&](entt::entity e) {
        if (!kinds.contains(e) || kinds.get(e).kind != ecs::SpatialKind::Character) return;
        if (!positions.contains(e) || !maps.contains(e) || maps.get(e).value != mapIndex) return;
        const auto& pos = positions.get(e);
        const auto dx = std::abs(int64_t(pos.x) - origin.x), dy = std::abs(int64_t(pos.y) - origin.y);
        if (e != source && std::max(dx, dy) + std::min(dx, dy) / 2 > range) return;
        if (storyActor && !SpyQuest::CanView(reg, source, e)) return;
        result.push_back(e);
        sourceListed = sourceListed || e == source;
    });
    if (!sourceListed)
        result.push_back(source);
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
    // The set already holds each entity once.
    for (const entt::entity visible : view->visible) {
        if (visible != entt::null && reg.valid(visible))
            result.push_back(visible);
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
    // The set already holds each entity once.
    for (const entt::entity viewer : viewers->viewers) {
        if (viewer != entt::null && reg.valid(viewer))
            result.push_back(viewer);
    }
    return result;
}

inline void OnPositionChanged(entt::registry&, entt::entity) {}
inline void OnSpawn(entt::registry&, entt::entity) {}
inline void OnRemove(entt::registry&, entt::entity) {}

} // namespace ecs::VisibilityService
