#pragma once

#include "../components/item_components.hpp"

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

#include "../../../common/length.h"
#include "../../tables.h"
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
bool RemoveSpecifyItemEcs(entt::entity e, uint32_t vnum, uint32_t count = 1,
                          bool cubeRenewal = false);

// Slice B - equip / unequip
entt::entity GetWearItem(entt::entity e, uint8_t wearPos);
bool UnequipItemEcs(entt::entity owner, entt::entity item);
bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell = -1);
bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum);
bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum);
bool UnEquipSpecialRideUniqueItem(entt::entity e);
bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell = NPOS);

// Slice D - item creation / give / remove
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
entt::entity CreateItemEcs(uint32_t itemVnum, uint32_t count = 1,
                           uint32_t id = 0, bool tryMagic = false,
                           int rarePct = -1, bool skipSave = false);
bool IsValidItem(entt::entity item);
bool IsDragonSoulItem(entt::entity item);
bool IsExtraItem(entt::entity item);
bool IsRideItem(entt::entity item);
bool IsMountItem(entt::entity item);
#ifdef ENABLE_RUNE_SYSTEM
bool IsRuneItem(entt::entity item);
bool ActivateRuneLegacyBoundary(entt::entity item);
bool DeactivateRuneLegacyBoundary(entt::entity item);
bool ChangeRuneAttributesLegacyBoundary(entt::entity item, int32_t time);
bool ActivateRuneBonusLegacyBoundary(entt::entity item);
#endif
uint32_t GetItemID(entt::entity item);
uint32_t GetItemVID(entt::entity item);
uint32_t GetItemVnum(entt::entity item);
uint32_t GetItemOriginalVnum(entt::entity item);
uint32_t GetItemSIGVnum(entt::entity item);
int32_t GetItemSpecialGroup(entt::entity item);
uint32_t GetItemTransmutationVnum(entt::entity item);
uint8_t GetItemType(entt::entity item);
uint8_t GetItemSubType(entt::entity item);
uint32_t GetItemCount(entt::entity item);
int32_t GetItemValue(entt::entity item, uint32_t index);
int64_t GetItemShopBuyPrice(entt::entity item);
const char* GetItemName(entt::entity item);
const char* GetItemNameByVnum(uint32_t vnum);
uint8_t GetItemSize(entt::entity item);
uint8_t GetItemExtraCategory(entt::entity item);
uint32_t GetItemRefineVnum(entt::entity item);
int GetItemRefineLevel(entt::entity item);
int GetItemLevelLimit(entt::entity item);
int GetItemLimitTimerBasedOnWearIndex(entt::entity item);
int GetItemDuration(entt::entity item);
int32_t GetItemFlags(entt::entity item);
uint32_t GetItemWearFlags(entt::entity item);
uint32_t GetItemWearFlag(entt::entity item);

int FindEquipCell(entt::entity owner, entt::entity item, int candidateCell = -1);
uint32_t GetItemAntiFlags(entt::entity item);
uint32_t GetItemAntiFlag(entt::entity item);
uint32_t GetItemImmuneFlags(entt::entity item);
const TItemTable* GetItemProto(entt::entity item);
void SetItemCount(entt::entity item, uint32_t count);
bool ConsumeItem(entt::entity item, uint32_t amount = 1);
bool SetItemCountEcs(entt::entity item, uint32_t count);
bool AddItemCountEcs(entt::entity item, int delta);
bool ConsumeItemEcs(entt::entity item, uint32_t amount = 1);
bool DestroyItemEntityEcs(entt::entity item, const char* reason = nullptr);
ecs::ItemEvents& GetItemEvents(entt::entity item);
bool SaveItemEcs(entt::entity item, bool flush = true);
bool FlushDelayedSaveEcs(entt::entity item);
entt::entity GetItemOwner(entt::entity item);
entt::entity GetItemOwnerEntity(entt::entity item);
entt::entity RollPartyDropOwnership(entt::entity item, entt::entity initialOwner);
uint32_t GetItemLastOwnerPID(entt::entity item);
uint32_t GetItemSocket(entt::entity item, int index);
bool HasItemSocket(entt::entity item, int index);
TPlayerItemAttribute GetItemAttribute(entt::entity item, int index);
int GetItemAttributeType(entt::entity item, int index);
int GetItemAttributeValue(entt::entity item, int index);
bool SetItemSocket(entt::entity item, int index, uint32_t value);
bool SetItemSocketEcs(entt::entity item, int index, uint32_t value);
bool SyncItemSocketsFromLegacy(entt::entity item);
bool SyncLegacySocketsFromEcs(entt::entity item);
bool SetItemAttribute(entt::entity item, int index, int type, int value);
bool ClearItemAttribute(entt::entity item, int index);
bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value);
int GetItemAttributeCount(entt::entity item);
int GetItemRareAttributeCount(entt::entity item);
bool AddItemAttributeEcs(entt::entity item);
bool AddItemRareAttributeEcs(entt::entity item);
bool ChangeItemAttributeEcs(entt::entity item);
bool ChangeItemRareAttributeEcs(entt::entity item);
bool ClearItemAttributesEcs(entt::entity item);
bool CopyItemAttributesEcs(entt::entity source, entt::entity target);
bool CopyItemSocketsEcs(entt::entity source, entt::entity target);
bool CopyAllAttrToEcs(entt::entity source, entt::entity target);
bool AttrLogEcs(entt::entity item);
bool SyncItemAttributesFromLegacy(entt::entity item);
bool SyncLegacyAttributesFromEcs(entt::entity item);
bool SetItemExchanging(entt::entity item, bool flag);
bool IsItemExchanging(entt::entity item);
bool IsItemLocked(entt::entity item);
bool IsItemBound(entt::entity item);
int16_t GetItemLockedAttributeIndex(entt::entity item);
bool LockItem(entt::entity item, bool locked = true);
bool UnlockItem(entt::entity item);
bool SetItemSkipSave(entt::entity item, bool flag);
bool SetItemWindow(entt::entity item, uint8_t window);
bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell);
bool AlterItemToMagicItem(entt::entity item);
uint8_t GetItemWindow(entt::entity item);
uint16_t GetItemCell(entt::entity item);
bool IsItemEquipped(entt::entity item);
bool IsItemInInventory(entt::entity item);
bool IsItemInExtraInventory(entt::entity item);
bool IsItemInDragonSoulInventory(entt::entity item);
bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell);
bool RemoveItemEcs(entt::entity item);
bool RemoveItemFromCharacterLegacyBoundary(entt::entity item);
int GetEmptyInventoryPositionEcs(entt::entity owner, entt::entity item);
bool HasMainInventorySpaceEcs(entt::entity owner, uint8_t itemSize = 1);
bool HasInventorySpaceForItemVnum(entt::entity owner, uint32_t itemVnum);
// Explicit transition boundary. Ground insertion still depends on CItem /
// LPENTITY and must not be presented as a native ECS operation.
bool PlaceItemOnGroundLegacyBoundary(entt::entity item, int32_t mapIndex,
                                     const PIXEL_POSITION& position,
                                     int destroySeconds = 300);
int GetEmptyDragonSoulInventory(entt::entity owner, entt::entity item);
bool IsItemVnumStackable(uint32_t vnum);
bool ModifyItemPointsEcs(entt::entity item, bool add);
bool StartTimerBasedOnWearExpireEventEcs(entt::entity item);
bool StopTimerBasedOnWearExpireEventEcs(entt::entity item);
bool StartRealTimeExpireEventEcs(entt::entity item);
bool SyncItemLocationFromLegacy(entt::entity item);
bool SyncItemOwnerFromLegacy(entt::entity item);
bool SyncItemStateFromLegacy(entt::entity item);
bool DestroyLoadedDuplicateItem(entt::entity item);
bool TransferItemOwnership(entt::entity item, entt::entity from, entt::entity to);
// Explicit transition boundary. The ownership timeout event still stores a
// legacy CHARACTER pointer until the ground-item event layer is migrated.
bool SetGroundOwnershipLegacyBoundary(entt::entity item, entt::entity owner,
                                      int seconds = 10);
bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item);
struct SpecialItemGroupResult {
    std::vector<entt::entity> itemEntities;
    std::vector<uint32_t> itemVnums;
    std::vector<uint32_t> itemCounts;
    int count = 0;
};
SpecialItemGroupResult GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum);
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

bool DoRefine(entt::entity e, entt::entity item, bool moneyOnly = false);
bool DoRefineWithScroll(entt::entity e, entt::entity item);
bool DoRefineItemSoul(entt::entity e, entt::entity item);

} // namespace ItemSystem

