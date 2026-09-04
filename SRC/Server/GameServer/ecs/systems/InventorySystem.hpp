#pragma once

#include "../Registry.hpp"
#include "../components/inventory_components.hpp"

namespace InventorySystem {

#ifdef __HIGHLIGHT_SYSTEM__
bool AddToCharacter(entt::entity item, entt::entity ch, TItemPos Cell, bool isHighLight = true);
#else
bool AddToCharacter(entt::entity item, entt::entity ch, TItemPos Cell);
#endif
void SetOwnership(entt::entity item, entt::entity character, int iSec = 10);
entt::entity RemoveFromGround(entt::entity item);
bool EquipTo(entt::entity item, entt::entity ch, uint8_t bWearCell);
bool Unequip(entt::entity item);
entt::entity RemoveFromCharacter(entt::entity item);

void SyncQuickslot(entt::entity e, uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out);
void SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& slot);
void DelQuickslot(entt::entity e, uint8_t pos);
void SwapQuickslot(entt::entity e, uint8_t posA, uint8_t posB);

} // namespace InventorySystem
