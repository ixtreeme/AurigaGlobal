#pragma once
#include <entt/entity/entity.hpp>
#include "../ecs/Registry.hpp"

inline constexpr int SHOP_MAX_DISTANCE = 1000;
namespace ecs {
struct ShopData { entt::entity owner {entt::null}; };
}
namespace ShopSystem {
inline bool IsValid(entt::entity shop)
{
    return g_registry.valid(shop) && g_registry.all_of<ecs::ShopData>(shop);
}
}
