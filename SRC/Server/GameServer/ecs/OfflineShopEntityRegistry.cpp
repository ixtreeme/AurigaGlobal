#include "../stdafx.h"

#include "OfflineShopEntityRegistry.hpp"
#include "Registry.hpp"
#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"
#include "services/SpatialService.hpp"

#include <unordered_map>
#include <unordered_set>
#include <exception>

namespace {

std::unordered_map<uint32_t, entt::entity> g_byVID;
std::unordered_set<entt::entity> g_destroying;
std::unordered_set<entt::entity> g_retired;
uint32_t g_nextVID = 1;

uint32_t AllocateVID()
{
    // Never reuse a wire ID during this process lifetime: delayed click packets
    // must not resolve to a different owner's shop after a despawn.
    if (g_nextVID == 0)
        return 0;
    return g_nextVID++;
}

void Retire(entt::entity entity)
{
    std::erase_if(g_byVID, [entity](const auto& entry) { return entry.second == entity; });
    if (!g_registry.valid(entity)) {
        g_retired.erase(entity);
        return;
    }
    if (!g_destroying.insert(entity).second)
        return;
    struct Exit {
        entt::entity entity;
        ~Exit() { g_destroying.erase(entity); }
    } exit { entity };
    g_retired.insert(entity);

    // A previous exceptional teardown can be retried. The active-call set, not
    // the persistent retirement tag, prevents recursion from REMOVE/on_destroy.
    if (!g_registry.all_of<ecs::SpatialRetiring>(entity))
        g_registry.insert<ecs::SpatialRetiring>(&entity, &entity + 1);
    if (!g_registry.valid(entity)) {
        g_retired.erase(entity);
        return;
    }
    ecs::SpatialService::RemoveEntity(g_registry, entity);
    if (g_registry.valid(entity))
        g_registry.destroy(entity);
    g_retired.erase(entity);
}

} // namespace

namespace ecs::OfflineShopEntityRegistry {

entt::entity Create(uint32_t ownerPID, std::string name, uint32_t race,
    int shopType, uint32_t mapIndex, int32_t x, int32_t y)
{
    if (ownerPID == 0 || mapIndex == 0 || mapIndex > INT32_MAX)
        return entt::null;
    const uint32_t vid = AllocateVID();
    if (vid == 0)
        return entt::null;

    const auto entity = g_registry.create();
    const OfflineShopState state { vid, race, shopType, std::move(name), ownerPID };
    const auto matches = [&] {
        if (!g_registry.valid(entity) || g_registry.all_of<SpatialRetiring>(entity))
            return false;
        const auto* current = g_registry.try_get<OfflineShopState>(entity);
        return current && current->vid == vid && current->ownerPID == ownerPID &&
            current->race == race && current->shopType == shopType && current->name == state.name;
    };
    const auto initialize = [&] {
        // insert() returns no component reference, which matters if a subscriber
        // destroys the entity or removes a component during construction.
        g_registry.insert<OfflineShopState>(&entity, &entity + 1, state);
        if (!matches()) return false;
        g_registry.insert<VIDComponent>(&entity, &entity + 1, VIDComponent { vid });
        if (!matches()) return false;
        const auto* identity = g_registry.try_get<VIDComponent>(entity);
        if (!identity || identity->value != vid) return false;

        g_byVID.emplace(vid, entity);
        if (!SpatialService::InsertEntity(g_registry, entity, mapIndex, x, y, 0)) return false;
        return matches() && FindByVID(vid) == entity;
    };
    try {
        if (initialize())
            return entity;
    } catch (...) {
        // Explicit rollback: a cleanup exception must propagate normally, not
        // terminate via a throwing destructor during exception unwinding.
        Retire(entity);
        throw;
    }
    // The creator owns this generation even if a construction signal removed
    // OfflineShopState, so partial entities are cleaned as well.
    Retire(entity);
    return entt::null;
}

void Destroy(entt::entity entity)
{
    if (g_registry.valid(entity) &&
        !g_registry.all_of<OfflineShopState>(entity) && !g_retired.contains(entity))
        return;
    Retire(entity);
}

void DestroyPending(std::vector<entt::entity>& pending)
{
    // Callbacks can append new work or reenter this function, so no iterator or
    // reference into the caller's ownership list may survive a Destroy call.
    const auto snapshot = pending;
    std::exception_ptr failure;
    for (const auto entity : snapshot) {
        try {
            Destroy(entity);
        } catch (...) {
            if (!failure)
                failure = std::current_exception();
        }
        if (!g_registry.valid(entity))
            std::erase(pending, entity);
    }
    if (failure)
        std::rethrow_exception(failure);
}

entt::entity FindByVID(uint32_t vid)
{
    const auto it = g_byVID.find(vid);
    if (it == g_byVID.end())
        return entt::null;
    const auto entity = it->second;
    if (!g_registry.valid(entity) || g_registry.all_of<SpatialRetiring>(entity))
        return entt::null;
    const auto* state = g_registry.try_get<OfflineShopState>(entity);
    const auto* identity = g_registry.try_get<VIDComponent>(entity);
    return state && state->vid == vid && identity && identity->value == vid
        ? entity : entt::null;
}

}
