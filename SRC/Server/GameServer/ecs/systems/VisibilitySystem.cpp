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

    // Bidirectional ViewMap/ViewerMap maintenance.
    //
    // Phase 15E-final.LPENTITY.4-architect.D.6:
    // Visibility is symmetric for non-observer characters (A sees B <=> B
    // sees A, A views B <=> B views A). When the handler fires for `self`
    // moving and a viewer enters/leaves, we update all four ECS fields:
    //
    //   self.ViewerMap.viewers   += viewer  (viewer views self)
    //   self.ViewMap.visible     += viewer  (self sees viewer)
    //   viewer.ViewerMap.viewers += self    (self views viewer)
    //   viewer.ViewMap.visible   += self    (viewer sees self)
    //
    // Pre-D.6 the helper updated only sides 1 and 4 - the other two were
    // populated by the legacy CFuncViewInsert polling running in parallel
    // (the per-entity UpdateSectree adds the entity to its own ViewMap).
    // After D.6 stubs out that polling for characters, the helper must
    // fill all four sides itself or GetVisibleEntities(self) and
    // GetViewersOf(viewer) return empty for the just-formed visibility
    // pair.
    void InsertBidirectional(entt::registry& reg, entt::entity self, entt::entity viewer)
    {
        reg.get_or_emplace<ecs::ViewerMap>(self).viewers.insert(viewer);
        reg.get_or_emplace<ecs::ViewMap>(self).visible.insert(viewer);
        reg.get_or_emplace<ecs::ViewerMap>(viewer).viewers.insert(self);
        reg.get_or_emplace<ecs::ViewMap>(viewer).visible.insert(self);
    }

    void RemoveBidirectional(entt::registry& reg, entt::entity self, entt::entity viewer)
    {
        if (auto* selfViewer = reg.try_get<ecs::ViewerMap>(self))
            selfViewer->viewers.erase(viewer);
        if (auto* selfView = reg.try_get<ecs::ViewMap>(self))
            selfView->visible.erase(viewer);
        if (auto* viewerViewer = reg.try_get<ecs::ViewerMap>(viewer))
            viewerViewer->viewers.erase(self);
        if (auto* viewerView = reg.try_get<ecs::ViewMap>(viewer))
            viewerView->visible.erase(self);
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

        const auto newViewers = ComputeViewersAt(reg, ev.entity, ev.newMapIndex, ev.newX, ev.newY, kRange);

        // Phase 15E-final.LPENTITY.4-architect H fixup-3:
        // Spawn-shape sentinel (oldMapIndex==0) becomes "heal-only" mode.
        //
        // The fixup-1 / fixup-2 spawn triggers in CHARACTER::Show emit a
        // PositionChangedEvent with old==(0,0,0,0) on every Show invocation
        // (login spawn, map warp, dungeon entry, anti-cheat backport,
        // intra-sectree /warp). Pre-fixup-3 that sentinel was processed as
        // "fresh spawn - empty oldViewers, full newViewers, every newViewer
        // entering" - so EVERY existing viewer received a duplicate
        // SendInsert (CharacterAdd packet) on every Show.
        //
        // Symptom: the moving character's animation freezes on peer clients
        // after the first stop-and-restart cycle. The peer client renders
        // the CharacterAdd as a respawn at the indicated position and
        // resets the moving state. If the next HEADER_GC_MOVE arrives
        // before the peer's render unfreezes (which is "never" because
        // the cycle repeats on every move command), the character looks
        // stuck.
        //
        // Heal-only mode: snapshot the entity's CURRENT ViewerMap as the
        // baseline instead of the empty oldViewers from oldMapIndex==0.
        // - Viewers already known stay - no duplicate SendInsert.
        // - Newly-seen viewers (in sectree truth, not in ViewerMap) get a
        //   proper SendInsert + bidirectional state update.
        // - Stale viewers (in ViewerMap, not in sectree truth) are NOT
        //   removed by the spawn-shape - additive only. The next regular
        //   move-driven PositionChangedEvent will catch them via the
        //   normal leaving loop.
        //
        // Real spawns (login/warp) hit this path with an empty ViewerMap
        // anyway, so they still emit SendInsert to every nearby peer -
        // identical to fixup-1's original intent.
        std::unordered_set<entt::entity> oldViewers;
        const bool healOnly = (ev.oldMapIndex == 0);
        if (healOnly) {
            if (const auto* selfViewer = reg.try_get<ecs::ViewerMap>(ev.entity)) {
                for (entt::entity v : selfViewer->viewers)
                    oldViewers.insert(v);
            }
        } else {
            oldViewers = ComputeViewersAt(reg, ev.entity, ev.oldMapIndex, ev.oldX, ev.oldY, kRange);
        }

        // Phase 15E-final.LPENTITY.4-architect.D.6:
        // Each viewer transition emits packets in BOTH directions
        // (self -> viewer and viewer -> self). Pre-D.6 the legacy
        // CFuncViewInsert polling running on self's own UpdateSectree
        // emitted the viewer -> self direction; after D.6 stubs that
        // polling, the handler is the sole source. The character
        // add/remove protocol is idempotent under the legacy fallback
        // path inside EntityNetworkDispatch (EncodeInsert/Remove just
        // re-emit a packet), so duplicate wires are bandwidth, not
        // correctness.

        // Leaving: in oldViewers, not in newViewers.
        // Skipped in heal-only mode (additive: never removes from
        // ViewerMap on a spawn-shape event).
        if (!healOnly) {
            for (const entt::entity viewer : oldViewers) {
                if (newViewers.count(viewer))
                    continue;
                if (viewer == entt::null || !reg.valid(viewer))
                    continue;
                RemoveBidirectional(reg, ev.entity, viewer);
                ecs::EntityNetworkDispatch::SendRemove(reg, ev.entity, viewer);
                ecs::EntityNetworkDispatch::SendRemove(reg, viewer, ev.entity);
            }
        }

        // Entering: in newViewers, not in oldViewers
        for (const entt::entity viewer : newViewers) {
            if (oldViewers.count(viewer))
                continue;
            if (viewer == entt::null || !reg.valid(viewer))
                continue;
            InsertBidirectional(reg, ev.entity, viewer);
            ecs::EntityNetworkDispatch::SendInsert(reg, ev.entity, viewer);
            ecs::EntityNetworkDispatch::SendInsert(reg, viewer, ev.entity);
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
