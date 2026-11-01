#pragma once
// Monster drops on the typed item system (item system F4 C7b-3, IC-035).
//
// With items.drops = "typed" a kill rolls the catalog drop tables with the
// legacy GetDropPct modifiers, and each drop goes to its owner's inventory
// through the item store. One store operation per character runs at a time,
// so drops wait in the owner's queue meanwhile. A full inventory loses the
// drop until ground loot arrives (C7c).
#include <cstdint>
#include <vector>
#include <entt/entity/fwd.hpp>
#include <item/MobDrops.hpp>
#include <item/PersistentItems.hpp>

// The kill's drops from the catalog tables; empty while typed drops are off.
// Draws come from the legacy number() source, as the legacy tables' did.
std::vector<auriga::item::MobDrop> RollTypedMobDrops(const auriga::item::MobDropContext& kill);
// Gives one drop to its owner: at once when the owner has no typed operation
// in the store, else after it (merged with waiting drops of the same item
// state). An owner without typed items gets nothing.
void QueueTypedItemGrant(entt::entity owner, const auriga::item::ItemGrant& grant);
// Sends the owner's next waiting grant if no typed operation is in flight.
// Every completion of a typed operation calls it.
void PumpTypedItemGrants(entt::entity owner);
