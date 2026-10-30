#pragma once
// Player-requested typed item operations (item system F4 C3, IC-034).
//
// The game thread builds the complete commit from the character's current
// typed state with the pure domain builders, hands it to the item store and
// applies the reloaded, authoritative state when the store answers. One
// operation per character at a time; the client always gets the result as a
// fresh GC_TYPED_INVENTORY snapshot (also after a refusal).
#include <cstdint>
#include <entt/entity/fwd.hpp>

void RequestTypedItemMove(entt::entity character, std::uint64_t itemId, std::uint8_t storageKind, std::uint64_t storageId, std::uint32_t cell);
// CG_TYPED_ITEM_MOVE (225) payload; false when the packet is malformed.
bool HandleTypedItemMovePacket(entt::entity character, const char* data);
