#pragma once
#include <span>
#include <functional>

#include "../Registry.hpp"
#include "../components/inventory_components.hpp"

namespace InventorySystem {
bool CanEquipNow(entt::entity owner, entt::entity item);
bool CanUnequipNow(entt::entity owner, entt::entity item, bool requireSpace = true);
bool IsEquipmentSexAllowed(entt::entity owner, entt::entity item);

bool CanHandleItems(entt::entity owner, bool skipRefine = false, bool skipObserver = false);
bool IsValidItemPosition(entt::entity owner, TItemPos Pos);
int GetEmptyInventory(entt::entity owner, uint8_t size);
// Client drag/drop: both positions and the owner remain native entity state.
// True means committed, including when a publication callback removes the item.
bool MoveItem(entt::entity owner, TItemPos source, TItemPos destination, int count);
// Splits only (never moves/merges). The companion commit runs after all split
// preparation and final validation, before debit/placement/publication. It must
// not call services, allocate components, or change inventory/ownership/counts.
// False must leave companion state unchanged; true commits with the split.
// A committed split stays successful even if publication removes its entities.
bool SplitItemWithCommit(entt::entity owner, entt::entity item, int count, TItemPos destination,
    const std::function<bool(entt::entity)>& commit);
int GetInventorySize(entt::entity owner);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
// Spends the keys for the next inventory page and opens it.
bool ExpandInventory(entt::entity e);
#endif
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
// Spends the keys for the next stage of an extra inventory category.
void UnlockExtraInventory(entt::entity e, uint8_t category);
#endif
#ifdef ENABLE_SPAM_CHECK
// Inventory unlocks are three seconds apart: when the next is allowed, and
// the call that starts the wait.
int32_t GetLastUnlock(entt::entity e);
void SetLastUnlock(entt::entity e);
#endif
bool IsEmptyItemGrid(entt::entity owner, TItemPos cell, uint8_t size, int exceptionCell = -1);
bool HasBeltItems(entt::entity owner);
bool IsRefining(entt::entity owner);
int GetRefineScrollCell(entt::entity owner);
entt::entity GetRefineNPC(entt::entity owner);
void SetRefineNPC(entt::entity owner, entt::entity npc);
void SetRefineMode(entt::entity owner, int additionalCell);
void ClearRefineMode(entt::entity owner);
int64_t ComputeRefineFee(entt::entity owner, int64_t cost, int64_t multiply = 5);
void PayRefineFee(entt::entity owner, int64_t total);

#ifdef __HIGHLIGHT_SYSTEM__
bool AddToCharacter(entt::entity item, entt::entity ch, TItemPos Cell, bool isHighLight = true);
#else
bool AddToCharacter(entt::entity item, entt::entity ch, TItemPos Cell);
#endif
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
