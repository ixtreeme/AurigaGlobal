#pragma once

#include "../Registry.hpp"
#include "../components/inventory_components.hpp"

namespace InventorySystem {

void SyncQuickslot(entt::entity e, uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out);
void SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& slot);
void DelQuickslot(entt::entity e, uint8_t pos);
void SwapQuickslot(entt::entity e, uint8_t posA, uint8_t posB);

} // namespace InventorySystem
