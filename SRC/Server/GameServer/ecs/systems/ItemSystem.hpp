#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../../../common/length.h"
#include "../../typedef.h"

namespace ItemSystem {

LPITEM GetItem(entt::entity e, TItemPos cell);
LPITEM GetInventoryItem(entt::entity e, uint16_t cell);
#ifdef ENABLE_EXTRA_INVENTORY
LPITEM GetExtraInventoryItem(entt::entity e, uint16_t cell);
#endif
LPITEM FindSpecifyItem(entt::entity e, uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
                       , bool reinforce
#endif
);
LPITEM FindItemByID(entt::entity e, uint32_t id);
int CountItemRenewal(entt::entity e, uint32_t vnum);
int CountItem(entt::entity e, uint32_t vnum);
int CountTypeItem(entt::entity e, uint8_t type);
bool HasItem(entt::entity e, uint32_t vnum, uint32_t count = 1);

// Slice B - equip / unequip
LPITEM GetWearItem(entt::entity e, uint8_t wearPos);
void SetWearItem(entt::entity e, uint8_t wearPos, LPITEM item);
bool UnequipItem(entt::entity e, LPITEM item);
bool EquipItem(entt::entity e, LPITEM item, int candidateCell = -1);
bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum);
bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum);
bool UnEquipSpecialRideUniqueItem(entt::entity e);
bool CanEquipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell);

// Slice C1 - drop / move / pickup
bool DropItem(entt::entity e, TItemPos cell,
#ifdef ENABLE_NEW_STACK_LIMIT
              int
#else
              uint8_t
#endif
                  count);
bool DropGold(entt::entity e, int64_t gold);
bool MoveItem(entt::entity e, TItemPos fromCell, TItemPos toCell,
#ifdef ENABLE_NEW_STACK_LIMIT
              int
#else
              uint8_t
#endif
                  count);
bool PickupItem(entt::entity e, uint32_t vid);


} // namespace ItemSystem
