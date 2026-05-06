#include "../../stdafx.h"

#include "VisibilitySystem.hpp"

#include "../../config.h"
#include "../../sectree.h"
#include "../../utils.h"
#include "../AIHelpers.hpp"
#include "../EventDispatcher.hpp"
#include "../Registry.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/spatial_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
#include "../events.hpp"
#include "../services/EntityNetworkDispatch.hpp"

#include <Core/Logging.hpp>

#include <unordered_map>
#include <unordered_set>

namespace ecs::VisibilitySystem {

namespace {
    bool g_initialized = false;

    // Phase 15E-final.LPENTITY.4-architect.D.4:
    // Sectree-based viewer-set computation at an arbitrary (mapIndex, x, y).
    //
    // Mirrors the structure of VisibilityService::GetEntitiesInRange but
    // takes (mapIndex, x, y) directly rather than reading the source's
    // current Position. Required because the handler must compute the
    // OLD viewer set after Position has already been written to the new
    // location.
    //
    // mapIndex == 0 is the spawn/despawn sentinel (real maps start at 1).
    // The handler treats it as "no prior viewers" by short-circuiting to
    // an empty set.
    std::unordered_set<entt::entity> ComputeViewersAt(
        entt::registry& reg,
        entt::entity self,
        int32_t mapIndex,
        int32_t x,
        int32_t y,
        int32_t range)
    {
        std::unordered_set<entt::entity> result;
        if (mapIndex == 0)
            return result;

        LPSECTREE sectree = ecs::SectorAt(mapIndex, x, y);
        if (!sectree)
            return result;

        struct Collector {
            entt::registry& reg;
            entt::entity self;
            int32_t cx;
            int32_t cy;
            int32_t range;
            std::unordered_set<entt::entity>& out;

            void operator()(LPENTITY entity)
            {
                if (!entity || !entity->IsType(ENTITY_CHARACTER))
                    return;

                const auto e = AIHelpers::EcsOf(static_cast<LPCHARACTER>(entity));
                if (e == entt::null || !reg.valid(e) || e == self)
                    return;

                const auto* pos = reg.try_get<ecs::Position>(e);
                if (!pos)
                    return;

                if (DISTANCE_APPROX(pos->x - cx, pos->y - cy) > range)
                    return;

                out.insert(e);
            }
        } collector { reg, self, x, y, range, result };

        sectree->ForEachAround(collector);
        return result;
    }

    // Bidirectional ViewMap/ViewerMap maintenance. The pair-invariant
    // (Section 3 of the architect doc) is that B in A.viewers <=> A in
    // B.visible. These helpers update both sides atomically before any
    // packet is emitted.
    void InsertBidirectional(entt::registry& reg, entt::entity self, entt::entity viewer)
    {
        reg.get_or_emplace<ecs::ViewerMap>(self).viewers.insert(viewer);
        reg.get_or_emplace<ecs::ViewMap>(viewer).visible.insert(self);
    }

    void RemoveBidirectional(entt::registry& reg, entt::entity self, entt::entity viewer)
    {
        if (auto* viewerMap = reg.try_get<ecs::ViewerMap>(self))
            viewerMap->viewers.erase(viewer);
        if (auto* viewMap = reg.try_get<ecs::ViewMap>(viewer))
            viewMap->visible.erase(self);
    }

    // Phase 15E-final.LPENTITY.4-architect.D.4 handler.
    //
    // Runs IN PARALLEL with the legacy UpdateSectree polling: both paths
    // maintain ViewMap/ViewerMap, both emit SendInsert/SendRemove. The
    // server protocol is idempotent for character add/remove (the legacy
    // path already calls these multiple times for the same VID under
    // movement/Show/UpdateSectree overlap), so duplicate packets are not
    // a correctness concern - just bandwidth. D.5 will quantify the
    // overlap via a drift detector; D.6 will disable the legacy polling
    // once D.5 reports zero divergence.
    //
    // Scope guard: only character entities. Items / buildings / shops
    // continue to use the existing legacy InsertEntity/RemoveEntity
    // recipient-broadcast machinery in SpatialService.cpp - their
    // PositionChangedEvent (fired from D.2's SpatialService::InsertEntity
    // trigger) is observed here but skipped.
    void OnPositionChanged(const ecs::PositionChangedEvent& ev)
    {
        auto& reg = g_registry;

        if (ev.entity == entt::null || !reg.valid(ev.entity))
            return;

        const auto* kind = reg.try_get<ecs::SpatialKindTag>(ev.entity);
        if (!kind || kind->kind != ecs::SpatialKind::Character)
            return;

        const int32_t kRange = VIEW_RANGE + VIEW_BONUS_RANGE;

        const auto oldViewers = ComputeViewersAt(reg, ev.entity, ev.oldMapIndex, ev.oldX, ev.oldY, kRange);
        const auto newViewers = ComputeViewersAt(reg, ev.entity, ev.newMapIndex, ev.newX, ev.newY, kRange);

        // Leaving: in oldViewers, not in newViewers
        for (const entt::entity viewer : oldViewers) {
            if (newViewers.count(viewer))
                continue;
            if (viewer == entt::null || !reg.valid(viewer))
                continue;
            RemoveBidirectional(reg, ev.entity, viewer);
            ecs::EntityNetworkDispatch::SendRemove(reg, ev.entity, viewer);
        }

        // Entering: in newViewers, not in oldViewers
        for (const entt::entity viewer : newViewers) {
            if (oldViewers.count(viewer))
                continue;
            if (viewer == entt::null || !reg.valid(viewer))
                continue;
            InsertBidirectional(reg, ev.entity, viewer);
            ecs::EntityNetworkDispatch::SendInsert(reg, ev.entity, viewer);
        }
    }
}

void Init(entt::registry& /*reg*/)
{
    if (g_initialized)
        return;

    g_dispatcher.sink<ecs::PositionChangedEvent>()
        .connect<&OnPositionChanged>();

    g_initialized = true;
    LOG_INFO("[VISIBILITY] Init: PositionChangedEvent handler connected (D.4 diff handler)");
}

void Shutdown(entt::registry& /*reg*/)
{
    if (!g_initialized)
        return;

    g_dispatcher.sink<ecs::PositionChangedEvent>()
        .disconnect<&OnPositionChanged>();

    g_initialized = false;
}

void DriftSweep(entt::registry& reg)
{
#ifdef AURIGA_LPENTITY_FIXUP_AUDIT
    // Self-throttle: at most one full sweep every 5 seconds. The sweep
    // walks every character entity with a ViewerMap and runs a sectree
    // query per entity, so it's O(N) sectree queries per sweep - cheap
    // enough at 0.2 Hz, prohibitive at tick rate.
    static uint32_t s_lastSweepMs = 0;
    const uint32_t nowMs = static_cast<uint32_t>(get_dword_time());
    if (s_lastSweepMs != 0 && nowMs - s_lastSweepMs < 5000u)
        return;
    s_lastSweepMs = nowMs;

    // Per-entity log throttle: at most one drift report per entity per
    // 30 seconds. Without this, a single stuck-mirror entity would log
    // every sweep (12 logs/min) and 50+ such entities under load would
    // saturate the log queue. The throttle matches the ValidateViewMapMirror
    // [VIEWMAP_DRIFT] pattern in entity_view.cpp.
    static std::unordered_map<uint32_t, uint32_t> s_entityLastLogMs;

    const int32_t kRange = VIEW_RANGE + VIEW_BONUS_RANGE;

    auto charView = reg.view<ecs::ViewerMap, ecs::SpatialKindTag, ecs::Position, ecs::MapIndex>();
    for (auto e : charView) {
        const auto& kind = charView.get<ecs::SpatialKindTag>(e);
        if (kind.kind != ecs::SpatialKind::Character)
            continue;

        const auto& pos = charView.get<ecs::Position>(e);
        const auto& mapIdx = charView.get<ecs::MapIndex>(e);
        const auto& mirror = charView.get<ecs::ViewerMap>(e);

        const auto truth = ComputeViewersAt(reg, e, mapIdx.value, pos.x, pos.y, kRange);

        // Diff
        size_t onlyInMirror = 0;
        for (entt::entity v : mirror.viewers) {
            if (v == entt::null || !reg.valid(v))
                continue;
            if (truth.find(v) == truth.end())
                ++onlyInMirror;
        }
        size_t onlyInTruth = 0;
        for (entt::entity v : truth) {
            if (mirror.viewers.find(v) == mirror.viewers.end())
                ++onlyInTruth;
        }

        if (onlyInMirror == 0 && onlyInTruth == 0)
            continue;

        const uint32_t entityKey = static_cast<uint32_t>(e);
        if (auto it = s_entityLastLogMs.find(entityKey); it != s_entityLastLogMs.end()) {
            if (nowMs - it->second < 30000u)
                continue;
        }
        s_entityLastLogMs[entityKey] = nowMs;

        LOG_WARN("[VISIBILITY_DRIFT] entity={} mirror_size={} truth_size={} only_in_mirror={} only_in_truth={}",
            entityKey,
            mirror.viewers.size(),
            truth.size(),
            onlyInMirror,
            onlyInTruth);
    }
#else
    (void)reg;
#endif
}

} // namespace ecs::VisibilitySystem
