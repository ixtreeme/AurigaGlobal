#include "../../stdafx.h"
#include "VisibilitySystem.hpp"
#include "ViewSystem.hpp"
#include "../../config.h"
#include "../../sectree.h"
#include "../EventDispatcher.hpp"
#include "../Registry.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
#include "../components/status_components.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include "../services/VisibilityService.hpp"
#include "../events.hpp"
#include <Core/Logging.hpp>
#include <chrono>

namespace ecs::VisibilitySystem {
namespace {
bool initialized = false;
std::set<std::pair<const entt::registry*, entt::entity>> removing;
struct Removal {
    const entt::registry* registry;
    entt::entity entity;
    bool entered;
    Removal(const entt::registry& reg, entt::entity e)
        : registry(&reg), entity(e), entered(removing.emplace(&reg, e).second) {}
    void Release() { if (entered) removing.erase({registry, entity}); entered = false; }
    ~Removal() { Release(); }
};
template<class T> bool Prepare(entt::registry& reg, entt::entity e) {
    if (!reg.valid(e)) return false;
    if (!reg.all_of<T>(e)) reg.insert<T>(&e, &e + 1);
    return reg.valid(e) && reg.all_of<T>(e);
}
bool Active(entt::registry& reg, entt::entity e) {
    if (!reg.valid(e) || !reg.all_of<SpatialEntity, SpatialKindTag, Position, MapIndex>(e)) return false;
    auto* tree = SectorOf(reg, e);
    return tree && tree->Contains(e) && !tree->IsDestroying();
}
bool CanSee(entt::registry& reg, entt::entity source, entt::entity viewer) {
    if (source == viewer || !Active(reg, source) || !Active(reg, viewer) ||
        reg.get<SpatialKindTag>(viewer).kind != SpatialKind::Character ||
        reg.get<MapIndex>(source).value != reg.get<MapIndex>(viewer).value) return false;
    const auto* status = reg.try_get<StatusFlags>(source);
    if (status && status->isObserverMode) return false;
    const auto a = reg.get<Position>(source), b = reg.get<Position>(viewer);
    // Buildings have the historical whole-neighbor-sector visibility exception.
    if (reg.get<SpatialKindTag>(source).kind == SpatialKind::Building)
        return std::abs(int64_t(a.x / SECTREE_SIZE) - b.x / SECTREE_SIZE) <= 1 &&
            std::abs(int64_t(a.y / SECTREE_SIZE) - b.y / SECTREE_SIZE) <= 1;
    const auto dx = std::abs(int64_t(a.x) - b.x), dy = std::abs(int64_t(a.y) - b.y);
    return std::max(dx, dy) + std::min(dx, dy) / 2 <= VIEW_RANGE + VIEW_BONUS_RANGE;
}
struct Snapshot {
    entt::entity e;
    Position pos;
    int32_t map;
    uint64_t revision;
    bool Matches(entt::registry& reg) const {
        if (!Active(reg, e)) return false;
        const auto now = reg.get<Position>(e);
        const auto* version = reg.try_get<SpatialRevision>(e);
        return now.x == pos.x && now.y == pos.y && now.z == pos.z &&
            reg.get<MapIndex>(e).value == map && (version ? version->value : 0) == revision;
    }
};
Snapshot Capture(entt::registry& reg, entt::entity e) {
    const auto* revision = reg.try_get<SpatialRevision>(e);
    return {e, reg.get<Position>(e), reg.get<MapIndex>(e).value, revision ? revision->value : 0};
}
void Edge(entt::registry& reg, entt::entity source, entt::entity viewer, bool add) {
    if (!reg.valid(source) || !reg.valid(viewer)) return;
    if (add) {
        if (!Prepare<ViewerMap>(reg, source) || !Prepare<ViewMap>(reg, viewer) ||
            !reg.valid(source) || !reg.all_of<ViewerMap>(source) || !CanSee(reg, source, viewer)) return;
        auto& views = reg.get<ViewMap>(viewer).visible;
        const bool fresh = views.insert(source).second;
        reg.get<ViewerMap>(source).viewers.insert(viewer);
        if (fresh) EntityNetworkDispatch::SendInsert(reg, source, viewer);
    } else {
        bool present = false;
        if (auto* views = reg.try_get<ViewMap>(viewer)) present = views->visible.erase(source) != 0;
        if (auto* viewers = reg.try_get<ViewerMap>(source)) present |= viewers->viewers.erase(viewer) != 0;
        if (auto* age = reg.try_get<ViewAgeMap>(viewer)) age->ageByEntity.erase(source);
        if (present) EntityNetworkDispatch::SendRemove(reg, source, viewer);
    }
}
void ClearEdges(entt::registry& reg, entt::entity e) {
    if (auto* view = reg.try_get<ViewMap>(e)) {
        for (auto other : view->visible)
            if (reg.valid(other))
                if (auto* reverse = reg.try_get<ViewerMap>(other)) reverse->viewers.erase(e);
        view->visible.clear();
    }
    if (auto* viewers = reg.try_get<ViewerMap>(e)) {
        for (auto other : viewers->viewers) if (reg.valid(other)) {
            if (auto* view = reg.try_get<ViewMap>(other)) view->visible.erase(e);
            if (auto* age = reg.try_get<ViewAgeMap>(other)) age->ageByEntity.erase(e);
        }
        viewers->viewers.clear();
    }
    if (auto* age = reg.try_get<ViewAgeMap>(e)) age->ageByEntity.clear();
}
void OnPositionChanged(const PositionChangedEvent& ev) { Refresh(g_registry, ev.entity); }
}

void Refresh(entt::registry& reg, entt::entity e) {
    if (!Active(reg, e)) return;
    const auto snapshot = Capture(reg, e);
    std::unordered_set<entt::entity> candidates;
    auto collect = [&](entt::entity other) { if (other != e) candidates.insert(other); };
    SectorOf(reg, e)->ForEachAround(collect);
    // Actual published edges are the baseline, not an inferred old position.
    // This also heals stationary/late-arriving viewers of newly dropped items.
    if (auto* view = reg.try_get<ViewMap>(e)) candidates.insert(view->visible.begin(), view->visible.end());
    if (auto* viewers = reg.try_get<ViewerMap>(e)) candidates.insert(viewers->viewers.begin(), viewers->viewers.end());
    for (auto other : candidates) {
        if (!snapshot.Matches(reg)) return;
        if (!reg.valid(other)) {
            if (auto* view = reg.try_get<ViewMap>(e)) view->visible.erase(other);
            if (auto* viewers = reg.try_get<ViewerMap>(e)) viewers->viewers.erase(other);
            continue;
        }
        Edge(reg, e, other, CanSee(reg, e, other));
        if (!snapshot.Matches(reg)) return;
        Edge(reg, other, e, CanSee(reg, other, e));
    }
}

void Remove(entt::registry& reg, entt::entity e) {
    if (!reg.valid(e)) return;
    Removal action(reg, e);
    if (!action.entered) return;
    const auto* initialTree = SectorOf(reg, e);
    std::unordered_set<entt::entity> recipients;
    if (auto* viewers = reg.try_get<ViewerMap>(e)) recipients.insert(viewers->viewers.begin(), viewers->viewers.end());
    for (auto viewer : VisibilityService::GetEntitiesInRange(reg, e, VIEW_RANGE + VIEW_BONUS_RANGE))
        if (viewer != e) recipients.insert(viewer);
    auto* version = reg.try_get<SpatialRevision>(e);
    if (version) ++version->value;
    const uint64_t revision = version ? version->value : 0;
    const auto detached = [&] {
        if (!reg.valid(e) || SectorOf(reg, e) != initialTree) return false;
        const auto* current = reg.try_get<SpatialRevision>(e);
        return (current ? current->value : 0) == revision;
    };
    ClearEdges(reg, e);
    if (!detached()) return;
    reg.remove<SpatialEntity>(e);
    if (!detached()) return;
    reg.remove<ViewActiveTag>(e);
    if (!detached()) return;
    reg.remove<VisibilityDirty>(e);
    action.Release(); // Packet callbacks may legitimately respawn the entity.
    // SpatialKindTag is identity: retain it until remove packets are encoded.
    for (auto viewer : recipients) {
        if (!detached()) return;
        if (reg.valid(viewer)) EntityNetworkDispatch::SendRemove(reg, e, viewer);
    }
}

bool IsRemoving(const entt::registry& reg, entt::entity e) {
    return removing.contains({&reg, e});
}

void Reencode(entt::registry& reg, entt::entity e) {
    if (!Active(reg, e)) return;
    const auto snapshot = Capture(reg, e);
    const auto* status = reg.try_get<StatusFlags>(e);
    if (status && status->isObserverMode) return;
    if (reg.get<SpatialKindTag>(e).kind != SpatialKind::Character) {
        // Shops/buildings have no client of their own; reencode the source to
        // its viewers (e.g. a shop name change), not an empty outgoing ViewMap.
        const auto* viewers = reg.try_get<ViewerMap>(e);
        const auto recipients = viewers ? viewers->viewers : std::unordered_set<entt::entity>{};
        for (auto viewer : recipients) {
            if (!snapshot.Matches(reg)) return;
            if (!CanSee(reg, e, viewer)) continue;
            EntityNetworkDispatch::SendRemove(reg, e, viewer);
            if (!snapshot.Matches(reg)) return;
            if (CanSee(reg, e, viewer)) EntityNetworkDispatch::SendInsert(reg, e, viewer);
        }
        return;
    }
    EntityNetworkDispatch::SendRemove(reg, e, e);
    if (!snapshot.Matches(reg)) return;
    EntityNetworkDispatch::SendInsert(reg, e, e);
    if (!snapshot.Matches(reg)) return;
    const auto* view = reg.try_get<ViewMap>(e);
    const auto visible = view ? view->visible : std::unordered_set<entt::entity>{};
    for (auto other : visible) {
        if (!snapshot.Matches(reg)) return;
        if (CanSee(reg, other, e)) EntityNetworkDispatch::SendInsert(reg, other, e);
    }
}
void Reencode(entt::entity e) { Reencode(g_registry, e); }
void Init(entt::registry&) {
    if (initialized) return;
    g_dispatcher.sink<PositionChangedEvent>().connect<&OnPositionChanged>();
    initialized = true;
}
void Shutdown(entt::registry&) {
    if (!initialized) return;
    g_dispatcher.sink<PositionChangedEvent>().disconnect<&OnPositionChanged>();
    initialized = false;
}
void DriftSweep(entt::registry& reg) {
#ifdef AURIGA_LPENTITY_FIXUP_AUDIT
    const auto now = std::chrono::steady_clock::now();
    static auto lastSweep = now - std::chrono::seconds(5);
    if (now - lastSweep < std::chrono::seconds(5)) return;
    lastSweep = now;
    // Read-only directed-edge audit, shared by character and item visibility.
    for (auto e : reg.view<ViewerMap>())
        for (auto viewer : reg.get<ViewerMap>(e).viewers)
            if (!reg.valid(viewer) || !reg.all_of<ViewMap>(viewer) ||
                !reg.get<ViewMap>(viewer).visible.contains(e))
                LOG_WARN("[VISIBILITY_DRIFT] missing reverse edge {} -> {}",
                    entt::to_integral(e), entt::to_integral(viewer));
#else
    (void)reg;
#endif
}
} // namespace ecs::VisibilitySystem

namespace ecs::ViewSystem {
void ViewCleanup(entt::entity entity) { VisibilitySystem::Remove(g_registry, entity); }
void ViewReencode(entt::entity entity) { VisibilitySystem::Reencode(g_registry, entity); }
}
