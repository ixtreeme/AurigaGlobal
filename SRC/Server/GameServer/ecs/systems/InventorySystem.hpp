#pragma once
#include <span>

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

void SyncQuickslot(entt::entity e, uint16_t type, uint16_t oldPos, uint16_t newPos);
ecs::QuickSlots MakeQuickSlots(std::span<const TQuickslot, QUICKSLOT_MAX_NUM> saved);
void SendQuickslots(entt::entity e);
bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out);
bool SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& slot);
bool SetQuickslotFromClient(entt::entity e, uint8_t pos, TQuickslot slot);
bool DelQuickslot(entt::entity e, uint8_t pos);
bool SwapQuickslot(entt::entity e, uint8_t posA, uint8_t posB);

} // namespace InventorySystem
