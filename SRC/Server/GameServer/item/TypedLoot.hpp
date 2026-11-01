#pragma once
// Typed ground loot on this server (item system F4 C7c, IC-035).
//
// Memory only, like the legacy ground items: an entry is a spatial entity
// (SpatialKind::TypedLoot) holding an item state, its claim and its lifetime.
// The item instance is made when someone picks it up, through the item store;
// the entry leaves the ground only once the store has written the item.
#include <cstdint>
#include <entt/entity/fwd.hpp>
#include <item/PersistentItems.hpp>

// Puts an item state on the ground at (x, y) on a map. With a claimant the
// loot is theirs for 60 s (not on battle arena maps); it vanishes after 5
// minutes. False when the place is not on a loaded map.
bool DropTypedLoot(const auriga::item::ItemGrant& grant, std::int32_t mapIndex, std::int32_t x, std::int32_t y,
	std::int32_t z, entt::entity claimant);
// CG_ITEM_PICKUP for a typed loot VID. True when the VID is typed loot
// (handled, whatever the outcome); false lets the legacy pickup run.
bool PickupTypedLoot(entt::entity character, std::uint32_t vid);
// Once a second from the heartbeat: lapsed claims and expired loot.
void TickTypedLoot();
// CG_TYPED_ITEM_DROP (228): concrete inventory instance, 0 means whole stack.
void RequestTypedItemDrop(entt::entity character, std::uint64_t id, std::uint32_t units);
bool HandleTypedItemDropPacket(entt::entity character, const char* data);
