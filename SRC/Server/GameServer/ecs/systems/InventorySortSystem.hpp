#pragma once

#ifdef ENABLE_SORT_INVEN

#include <entt/entt.hpp>

// ECS-native rebuild of CHARACTER::EditMyInven / EditMyExtraInven.
//
// The main inventory is sorted by item name, the extra inventory by vnum.
// Expiring items keep their cells. Stackables are re-created so equal stacks
// merge; every other item is moved as the same entity. Quickslots that pointed
// at a moved item follow it. Each command has its own 30 second cooldown and is
// rejected while dead, trading, shopping, using a safebox/cube or visiting an
// offline shop / auction.
namespace InventorySortSystem {

void SortInventory(entt::entity character);
void SortExtraInventory(entt::entity character);

} // namespace InventorySortSystem

#endif // ENABLE_SORT_INVEN
