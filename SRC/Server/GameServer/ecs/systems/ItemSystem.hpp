#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

#include "../../../common/length.h"
#include "../../typedef.h"

namespace ItemSystem {

entt::entity GetItem(entt::entity e, TItemPos cell);
entt::entity GetInventoryItem(entt::entity e, uint16_t cell);
#ifdef ENABLE_EXTRA_INVENTORY
entt::entity GetExtraInventoryItem(entt::entity e, uint16_t cell);
void SyncExtraInventoryAll(entt::entity e);
#endif
entt::entity FindSpecifyItem(entt::entity e, uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
                       , bool reinforce
#endif
);
entt::entity FindItemByID(uint32_t id);
entt::entity FindItemByVID(uint32_t vid);
entt::entity FindItemByID(entt::entity e, uint32_t id);
int CountItemRenewal(entt::entity e, uint32_t vnum);
int CountItem(entt::entity e, uint32_t vnum);
int CountTypeItem(entt::entity e, uint8_t type);
bool HasItem(entt::entity e, uint32_t vnum, uint32_t count = 1);

// Slice B - equip / unequip
entt::entity GetWearItem(entt::entity e, uint8_t wearPos);
void SetWearItem(entt::entity e, uint8_t wearPos, entt::entity item);
bool UnequipItem(entt::entity e, entt::entity item);
bool EquipItem(entt::entity e, entt::entity item, int candidateCell = -1);
bool UnequipItemEcs(entt::entity owner, entt::entity item);
bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell = -1);
bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum);
bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum);
bool UnEquipSpecialRideUniqueItem(entt::entity e);
bool CanEquipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);
bool CanUnequipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell);

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

// Slice C2a - use item wrapper
bool UseItem(entt::entity e, TItemPos cell, TItemPos destCell = NPOS);
bool UseItemEx(entt::entity e, entt::entity item, TItemPos destCell = NPOS);
bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell = NPOS);

// Slice D - item creation / give / remove
void RemoveTypeItem(entt::entity e, uint8_t type, int count = 1);
void AutoGiveItem(entt::entity e, entt::entity item, bool longOwnerShip = false
#ifdef __HIGHLIGHT_SYSTEM__
                  , bool isHighLight = true
#endif
);
#ifdef ENABLE_DS_REFINE_ALL
bool AutoGiveDS(entt::entity e, entt::entity item, bool longOwnerShip = false);
#endif
entt::entity AutoGiveItemEcs(entt::entity owner, uint32_t itemVnum,
                             uint32_t count = 1, int rarePct = -1,
                             bool sendMessage = true);
bool IsValidItem(entt::entity item);
uint32_t GetItemID(entt::entity item);
uint32_t GetItemVID(entt::entity item);
uint32_t GetItemVnum(entt::entity item);
uint32_t GetItemOriginalVnum(entt::entity item);
uint8_t GetItemType(entt::entity item);
uint8_t GetItemSubType(entt::entity item);
uint32_t GetItemCount(entt::entity item);
int32_t GetItemValue(entt::entity item, uint32_t index);
const char* GetItemName(entt::entity item);
uint8_t GetItemSize(entt::entity item);
uint32_t GetItemRefineVnum(entt::entity item);
int GetItemRefineLevel(entt::entity item);
int GetItemLevelLimit(entt::entity item);
int GetItemLimitTimerBasedOnWearIndex(entt::entity item);
int32_t GetItemFlags(entt::entity item);
uint32_t GetItemWearFlags(entt::entity item);
uint32_t GetItemAntiFlags(entt::entity item);
uint32_t GetItemImmuneFlags(entt::entity item);
void SetItemCount(entt::entity item, uint32_t count);
bool ConsumeItem(entt::entity item, uint32_t amount = 1);
bool SetItemCountEcs(entt::entity item, uint32_t count);
bool AddItemCountEcs(entt::entity item, int delta);
bool ConsumeItemEcs(entt::entity item, uint32_t amount = 1);
bool DestroyItemEntityEcs(entt::entity item, const char* reason = nullptr);
entt::entity GetItemOwner(entt::entity item);
uint32_t GetItemLastOwnerPID(entt::entity item);
uint32_t GetItemSocket(entt::entity item, int index);
bool HasItemSocket(entt::entity item, int index);
int GetItemAttributeType(entt::entity item, int index);
int GetItemAttributeValue(entt::entity item, int index);
bool SetItemSocket(entt::entity item, int index, uint32_t value);
bool SetItemSocketEcs(entt::entity item, int index, uint32_t value);
bool SyncItemSocketsFromLegacy(entt::entity item);
bool SyncLegacySocketsFromEcs(entt::entity item);
bool SetItemAttribute(entt::entity item, int index, int type, int value);
bool ClearItemAttribute(entt::entity item, int index);
bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value);
bool AddItemAttributeEcs(entt::entity item);
bool ChangeItemAttributeEcs(entt::entity item);
bool ClearItemAttributesEcs(entt::entity item);
bool SyncItemAttributesFromLegacy(entt::entity item);
bool SyncLegacyAttributesFromEcs(entt::entity item);
uint8_t GetItemWindow(entt::entity item);
uint16_t GetItemCell(entt::entity item);
bool IsItemEquipped(entt::entity item);
bool IsItemInInventory(entt::entity item);
bool IsItemInExtraInventory(entt::entity item);
bool IsItemInDragonSoulInventory(entt::entity item);
bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
bool RemoveItemEcs(entt::entity item);
bool SyncItemLocationFromLegacy(entt::entity item);
bool SyncItemOwnerFromLegacy(entt::entity item);
bool SyncItemStateFromLegacy(entt::entity item);
bool DestroyLoadedDuplicateItem(entt::entity item);
bool GiveItem(entt::entity from, entt::entity victim, TItemPos cell);
bool CanReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
void ReceiveItem(entt::entity receiver, entt::entity from, entt::entity item);
bool TransferItemOwnership(entt::entity item, entt::entity from, entt::entity to);
bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item);
struct SpecialItemGroupResult {
    std::vector<entt::entity> itemEntities;
    std::vector<uint32_t> itemVnums;
    std::vector<uint32_t> itemCounts;
    int count = 0;
};
SpecialItemGroupResult GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum);
bool DestroyItem(entt::entity e, TItemPos cell);
void ItemDivision(entt::entity e, TItemPos cell);

// Slice E - shop / trade / refine
struct RefineInput {
    entt::entity item = entt::null;
    std::vector<entt::entity> materials;
};

struct RefineResult {
    entt::entity resultItem = entt::null;
    bool success = false;
};

void SetRefineNPC(entt::entity e, entt::entity npc);
bool DoRefine(entt::entity e, entt::entity item, bool moneyOnly = false);
bool DoRefineWithScroll(entt::entity e, entt::entity item);
bool DoRefineItemSoul(entt::entity e, entt::entity item);
bool RefineInformation(entt::entity e, uint8_t cell, uint8_t type, int additionalCell = -1);
bool RefineItem(entt::entity e, entt::entity item, entt::entity target);
RefineResult RefineItemEcs(entt::entity e, const RefineInput& input, entt::entity target);
void UseSilkBotary(entt::entity e);
void SetRefineMode(entt::entity e, int additionalCell = -1);
void ClearRefineMode(entt::entity e);

} // namespace ItemSystem

