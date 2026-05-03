#include "../stdafx.h"

#include "OfflineShopEntityRegistry.hpp"

namespace {

std::unordered_map<uint32_t, entt::entity> g_byID;
std::unordered_map<uint32_t, entt::entity> g_byVID;
std::unordered_map<uint32_t, offlineshop::ShopEntity*> g_legacyByEntity;
std::unordered_map<uint32_t, uint32_t> g_idToVID;

}

namespace ecs::OfflineShopEntityRegistry {

void Register(uint32_t shopId, uint32_t vid, entt::entity e, offlineshop::ShopEntity* legacyEntity)
{
    if (shopId == 0 || e == entt::null)
        return;

    const auto oldVID = g_idToVID.find(shopId);
    if (oldVID != g_idToVID.end() && oldVID->second != vid)
        g_byVID.erase(oldVID->second);

    g_byID[shopId] = e;
    g_legacyByEntity[static_cast<uint32_t>(e)] = legacyEntity;

    if (vid != 0) {
        g_byVID[vid] = e;
        g_idToVID[shopId] = vid;
    } else {
        g_idToVID.erase(shopId);
    }
}

void Unregister(uint32_t shopId)
{
    const auto entityIt = g_byID.find(shopId);
    if (entityIt != g_byID.end())
        g_legacyByEntity.erase(static_cast<uint32_t>(entityIt->second));

    const auto oldVID = g_idToVID.find(shopId);
    if (oldVID != g_idToVID.end()) {
        g_byVID.erase(oldVID->second);
        g_idToVID.erase(oldVID);
    }

    g_byID.erase(shopId);
}

entt::entity FindByID(uint32_t shopId)
{
    const auto it = g_byID.find(shopId);
    return it != g_byID.end() ? it->second : entt::null;
}

entt::entity FindByVID(uint32_t vid)
{
    const auto it = g_byVID.find(vid);
    return it != g_byVID.end() ? it->second : entt::null;
}

offlineshop::ShopEntity* FindLegacyByEntity(entt::entity e)
{
    const auto it = g_legacyByEntity.find(static_cast<uint32_t>(e));
    return it != g_legacyByEntity.end() ? it->second : nullptr;
}

}
