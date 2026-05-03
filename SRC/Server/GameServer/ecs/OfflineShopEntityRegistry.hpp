#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

namespace offlineshop {
class ShopEntity;
}

namespace ecs::OfflineShopEntityRegistry {

void Register(uint32_t shopId, uint32_t vid, entt::entity e, offlineshop::ShopEntity* legacyEntity);
void Unregister(uint32_t shopId);

entt::entity FindByID(uint32_t shopId);
entt::entity FindByVID(uint32_t vid);
offlineshop::ShopEntity* FindLegacyByEntity(entt::entity e);

}
