#pragma once
#include <entt/entity/fwd.hpp>
#include <item/DungeonItems.hpp>
bool TryUseTypedDungeonKey(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);
std::optional<auriga::item::ItemKey> FindTypedDungeonUseKey(auriga::item::DungeonUseRole role);
void RequestTypedDungeonReward(entt::entity character,entt::entity dungeon,std::string rewardFlag,std::string_view code);
void QueueTypedDungeonCleanup(entt::entity,std::string_view code,std::uint32_t maximum=UINT32_MAX);
void TickTypedDungeonCleanup();
