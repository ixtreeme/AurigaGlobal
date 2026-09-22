#include "../core/stdafx.h"

#include "CBuildingRegistry.hpp"
#include "Registry.hpp"
#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"

namespace {

std::unordered_map<uint32_t, entt::entity> g_byID;
std::unordered_map<uint32_t, entt::entity> g_byVID;
std::unordered_map<uint32_t, uint32_t> g_idToVID;

bool IsLive(entt::entity e)
{
    const auto* state = g_registry.valid(e) ? g_registry.try_get<ecs::BuildingState>(e) : nullptr;
    return state && !state->destroying && !g_registry.all_of<ecs::SpatialRetiring>(e);
}

}

namespace ecs::CBuildingRegistry {

bool Register(uint32_t buildingId, uint32_t vid, entt::entity e)
{
    if (buildingId == 0 || vid == 0 || !IsLive(e))
        return false;
    const auto& state = g_registry.get<ecs::BuildingState>(e);
    const auto* identity = g_registry.try_get<ecs::VIDComponent>(e);
    if (state.objectId != buildingId || !identity || identity->value != vid)
        return false;
    const auto existingID = g_byID.find(buildingId);
    if (existingID != g_byID.end() && existingID->second != e && IsLive(existingID->second))
        return false;
    const auto existingVID = g_byVID.find(vid);
    if (existingVID != g_byVID.end() && existingVID->second != e && IsLive(existingVID->second))
        return false;

    const auto oldVID = g_idToVID.find(buildingId);
    if (oldVID != g_idToVID.end() && oldVID->second != vid) {
        const auto oldEntity = g_byVID.find(oldVID->second);
        if (oldEntity != g_byVID.end() && existingID != g_byID.end() && oldEntity->second == existingID->second)
            g_byVID.erase(oldEntity);
    }

    g_byID[buildingId] = e;
    g_byVID[vid] = e;
    g_idToVID[buildingId] = vid;
    return true;
}

void Unregister(uint32_t buildingId, entt::entity expected)
{
    const auto entityIt = g_byID.find(buildingId);
    if (entityIt == g_byID.end() || entityIt->second != expected)
        return;

    const auto oldVID = g_idToVID.find(buildingId);
    if (oldVID != g_idToVID.end()) {
        const auto vidIt = g_byVID.find(oldVID->second);
        if (vidIt != g_byVID.end() && vidIt->second == expected)
            g_byVID.erase(vidIt);
        g_idToVID.erase(oldVID);
    }

    g_byID.erase(buildingId);
}

entt::entity FindByID(uint32_t buildingId)
{
    const auto it = g_byID.find(buildingId);
    if (it == g_byID.end() || !IsLive(it->second))
        return entt::null;
    return g_registry.get<ecs::BuildingState>(it->second).objectId == buildingId ? it->second : entt::null;
}

entt::entity FindByVID(uint32_t vid)
{
    const auto it = g_byVID.find(vid);
    if (it == g_byVID.end() || !IsLive(it->second))
        return entt::null;
    const auto* identity = g_registry.try_get<ecs::VIDComponent>(it->second);
    return identity && identity->value == vid ? it->second : entt::null;
}

}
