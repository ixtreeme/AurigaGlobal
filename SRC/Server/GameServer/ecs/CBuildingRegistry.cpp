#include "../stdafx.h"

#include "CBuildingRegistry.hpp"

namespace {

std::unordered_map<uint32_t, entt::entity> g_byID;
std::unordered_map<uint32_t, entt::entity> g_byVID;
std::unordered_map<uint32_t, building::CObject*> g_legacyByEntity;
std::unordered_map<uint32_t, uint32_t> g_idToVID;

}

namespace ecs::CBuildingRegistry {

void Register(uint32_t buildingId, uint32_t vid, entt::entity e, building::CObject* legacyObject)
{
    if (buildingId == 0 || e == entt::null)
        return;

    const auto oldVID = g_idToVID.find(buildingId);
    if (oldVID != g_idToVID.end() && oldVID->second != vid)
        g_byVID.erase(oldVID->second);

    g_byID[buildingId] = e;
    g_legacyByEntity[static_cast<uint32_t>(e)] = legacyObject;

    if (vid != 0) {
        g_byVID[vid] = e;
        g_idToVID[buildingId] = vid;
    } else {
        g_idToVID.erase(buildingId);
    }
}

void Unregister(uint32_t buildingId)
{
    const auto entityIt = g_byID.find(buildingId);
    if (entityIt != g_byID.end())
        g_legacyByEntity.erase(static_cast<uint32_t>(entityIt->second));

    const auto oldVID = g_idToVID.find(buildingId);
    if (oldVID != g_idToVID.end()) {
        g_byVID.erase(oldVID->second);
        g_idToVID.erase(oldVID);
    }

    g_byID.erase(buildingId);
}

entt::entity FindByID(uint32_t buildingId)
{
    const auto it = g_byID.find(buildingId);
    return it != g_byID.end() ? it->second : entt::null;
}

entt::entity FindByVID(uint32_t vid)
{
    const auto it = g_byVID.find(vid);
    return it != g_byVID.end() ? it->second : entt::null;
}

building::CObject* FindLegacyByEntity(entt::entity e)
{
    const auto it = g_legacyByEntity.find(static_cast<uint32_t>(e));
    return it != g_legacyByEntity.end() ? it->second : nullptr;
}

}
