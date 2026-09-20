#include "stdafx.h"
#include "MountInventory.h"
#include "char_interface.hpp"
#include "db.h"
#include "desc.h"
#include "item_manager.h"
#include "ecs/Registry.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/components/character_runtime_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/MountSystem.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace
{
using Inventory = ecs::MountInventoryComponent;

Inventory* FindInventory(entt::entity inventory)
{
    if (inventory == entt::null || !g_registry.valid(inventory))
        return nullptr;

    auto* state = g_registry.try_get<Inventory>(inventory);
    if (!state || !state->loaded || state->destroying)
        return nullptr;

    return state;
}

const Inventory* FindInventory(entt::entity inventory, bool allowDestroying)
{
    if (inventory == entt::null || !g_registry.valid(inventory))
        return nullptr;

    const auto* state = g_registry.try_get<Inventory>(inventory);
    if (!state || (!state->loaded && !allowDestroying) ||
        (state->destroying && !allowDestroying))
        return nullptr;

    return state;
}

Inventory* FindInventoryForOwner(entt::entity owner)
{
    const entt::entity inventory = MountSystem::GetMountInventory(owner);
    return FindInventory(inventory);
}

const Inventory* FindInventoryForOwner(entt::entity owner, bool allowDestroying)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;

    const auto* ref = g_registry.try_get<ecs::MountInventoryRef>(owner);
    const auto* inventory = ref ? FindInventory(ref->inventory, allowDestroying) : nullptr;
    if (!inventory || inventory->owner != owner)
        return nullptr;

    const uint32_t accountId = ecs::PlayerRuntime::GetAccountID(owner);
    if (accountId == 0 || inventory->accountId != accountId)
        return nullptr;
    return inventory;
}

bool IsValidPosition(const Inventory& inventory, uint32_t pos)
{
    return inventory.height != 0 && inventory.height <= Inventory::MaxHeight &&
        pos < static_cast<uint32_t>(Inventory::Width) * inventory.height;
}

bool IsEmpty(const Inventory& inventory, uint32_t pos, uint8_t size)
{
    if (!IsValidPosition(inventory, pos) || size == 0)
        return false;

    const uint32_t row = pos / Inventory::Width;
    const uint32_t column = pos % Inventory::Width;
    if (row + size > inventory.height || column >= Inventory::Width)
        return false;

    for (uint32_t y = 0; y < size; ++y)
    {
        const uint32_t cell = pos + y * Inventory::Width;
        if (cell >= inventory.occupied.size() || inventory.occupied[cell] != 0)
            return false;
    }

    return true;
}

void Put(Inventory& inventory, uint32_t pos, uint8_t size)
{
    for (uint32_t y = 0; y < size; ++y)
        inventory.occupied[pos + y * Inventory::Width] = 1;
}

void Get(Inventory& inventory, uint32_t pos, uint8_t size)
{
    if (!IsValidPosition(inventory, pos))
        return;

    for (uint32_t y = 0; y < size; ++y)
    {
        const uint32_t cell = pos + y * Inventory::Width;
        if (cell < inventory.occupied.size())
            inventory.occupied[cell] = 0;
    }
}

bool StartMountExpireIfNeeded(entt::entity itemEntity)
{
    if (itemEntity == entt::null)
        return false;

    const TItemTable* itemProto = ItemSystem::GetItemProto(itemEntity);
    if (!itemProto)
        return false;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    const bool bIsMountLikeItem =
        ItemSystem::IsRideItem(itemEntity) || ItemSystem::IsMountItem(itemEntity);
#else
    const bool bIsMountLikeItem = ItemSystem::IsRideItem(itemEntity);
#endif
    if (!bIsMountLikeItem || itemProto->cLimitRealTimeFirstUseIndex == -1)
        return false;

    bool changed = false;
    if (ItemSystem::GetItemSocket(itemEntity, 1) == 0)
    {
        const uint8_t index = static_cast<uint8_t>(itemProto->cLimitRealTimeFirstUseIndex);
        int32_t duration = ItemSystem::GetItemSocket(itemEntity, 0);
        if (duration == 0)
            duration = itemProto->aLimits[index].lValue;
        if (duration == 0)
            duration = 60 * 60 * 24 * 7;

        ItemSystem::SetItemSocket(itemEntity, 0, time(nullptr) + duration);
        ItemSystem::SetItemSocket(itemEntity, 1, 1);
        changed = true;
    }

    ItemSystem::StartRealTimeExpireEventEcs(itemEntity);
    return changed;
}

uint32_t AccountId(const Inventory& inventory)
{
    return inventory.accountId;
}

void SaveItem(const Inventory& inventory, uint32_t pos, entt::entity itemEntity)
{
    if (!ItemSystem::IsValidItem(itemEntity) || AccountId(inventory) == 0)
        return;

    char query[512];
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> attrs {};
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        sockets[i] = ItemSystem::GetItemSocket(itemEntity, i);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        attrs[i] = ItemSystem::GetItemAttribute(itemEntity, i);

    snprintf(query, sizeof(query),
        "REPLACE INTO account_mount_inventory (id, account_id, slot, vnum, count, "
        "socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5) "
        "VALUES(%u, %u, %u, %u, %u, %ld, %ld, %ld, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
        ItemSystem::GetItemID(itemEntity),
        AccountId(inventory),
        pos,
        ItemSystem::GetItemVnum(itemEntity),
        ItemSystem::GetItemCount(itemEntity),
        sockets[0], sockets[1], sockets[2],
        attrs[0].bType, attrs[0].sValue,
        attrs[1].bType, attrs[1].sValue,
        attrs[2].bType, attrs[2].sValue,
        attrs[3].bType, attrs[3].sValue,
        attrs[4].bType, attrs[4].sValue,
        attrs[5].bType, attrs[5].sValue);

    DBManager::instance().Query("%s", query);
}

void DeleteItem(const Inventory& inventory, uint32_t pos, uint32_t id)
{
    if (AccountId(inventory) == 0)
        return;

    DBManager::instance().Query(
        "DELETE FROM account_mount_inventory WHERE account_id=%u AND slot=%u",
        AccountId(inventory), pos);

    if (id != 0)
    {
        DBManager::instance().Query(
            "DELETE FROM account_mount_inventory WHERE id=%u", id);
    }
}

bool AddToInventory(Inventory& inventory, entt::entity owner, uint32_t pos,
    entt::entity itemEntity, bool skipSave)
{
    if (inventory.owner != owner || inventory.destroying ||
        !ItemSystem::IsValidItem(itemEntity) ||
        ItemSystem::GetItemOwner(itemEntity) != entt::null ||
        ItemSystem::IsItemEquipped(itemEntity) ||
        ItemSystem::IsItemExchanging(itemEntity) ||
        ItemSystem::IsItemLocked(itemEntity) ||
        !IsValidPosition(inventory, pos))
        return false;

    if (!IsEmpty(inventory, pos, ItemSystem::GetItemSize(itemEntity)))
        return false;

    if (!ItemSystem::SetItemSkipSave(itemEntity, true) ||
        !ItemSystem::SetItemWindow(itemEntity, MOUNT_INVENTORY) ||
        !ItemSystem::SetItemCell(itemEntity, owner, pos))
    {
        ItemSystem::SetItemSkipSave(itemEntity, false);
        return false;
    }

    Put(inventory, pos, ItemSystem::GetItemSize(itemEntity));
    inventory.items[pos] = itemEntity;

    const bool expireStateChanged = StartMountExpireIfNeeded(itemEntity);
    if (!skipSave || expireStateChanged)
        SaveItem(inventory, pos, itemEntity);
    return true;
}

bool RestoreToInventory(Inventory& inventory, entt::entity owner, uint32_t pos,
    entt::entity itemEntity)
{
    if (inventory.owner != owner || inventory.destroying ||
        !ItemSystem::IsValidItem(itemEntity) ||
        !IsEmpty(inventory, pos, ItemSystem::GetItemSize(itemEntity)))
        return false;

    if (!ItemSystem::SetItemWindow(itemEntity, MOUNT_INVENTORY) ||
        !ItemSystem::SetItemCell(itemEntity, owner, pos))
        return false;

    Put(inventory, pos, ItemSystem::GetItemSize(itemEntity));
    inventory.items[pos] = itemEntity;
    SaveItem(inventory, pos, itemEntity);
    return true;
}

bool Detach(Inventory& inventory, uint32_t pos, entt::entity expectedItem,
    bool skipDbDelete)
{
    if (!IsValidPosition(inventory, pos))
        return false;

    const entt::entity item = inventory.items[pos];
    if (!ItemSystem::IsValidItem(item) ||
        (expectedItem != entt::null && item != expectedItem))
        return false;

    Get(inventory, pos, ItemSystem::GetItemSize(item));
    inventory.items[pos] = entt::null;
    if (!skipDbDelete)
        DeleteItem(inventory, pos, ItemSystem::GetItemID(item));
    return true;
}

} // namespace

namespace MountSystem
{

entt::entity GetMountInventory(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return entt::null;

    const auto* ref = g_registry.try_get<ecs::MountInventoryRef>(rider);
    if (!ref || ref->inventory == entt::null)
        return entt::null;

    const auto* inventory = FindInventory(ref->inventory, false);
    if (!inventory || inventory->owner != rider ||
        inventory->accountId != ecs::PlayerRuntime::GetAccountID(rider))
        return entt::null;

    return ref->inventory;
}

bool SetMountInventory(entt::entity rider, entt::entity inventory)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return false;

    if (inventory != entt::null)
    {
        const auto* state = FindInventory(inventory, false);
        if (!state || state->owner != rider ||
            state->accountId != ecs::PlayerRuntime::GetAccountID(rider))
            return false;
    }

    auto& ref = g_registry.get_or_emplace<ecs::MountInventoryRef>(rider);
    if (ref.inventory != entt::null && ref.inventory != inventory)
        return false;
    ref.inventory = inventory;
    return true;
}

entt::entity CreateMountInventory(entt::entity owner, uint32_t accountId,
    uint8_t height)
{
    if (owner == entt::null || !g_registry.valid(owner) || accountId == 0 ||
        height == 0 || height > ecs::MountInventoryComponent::MaxHeight)
        return entt::null;

    if (const entt::entity current = GetMountInventory(owner); current != entt::null)
        return current;

    if (auto* ref = g_registry.try_get<ecs::MountInventoryRef>(owner);
        ref && ref->inventory != entt::null)
    {
        if (g_registry.valid(ref->inventory) &&
            g_registry.try_get<ecs::MountInventoryComponent>(ref->inventory))
            DestroyMountInventory(owner);
        else if (g_registry.valid(ref->inventory))
            g_registry.destroy(ref->inventory);
        ref->inventory = entt::null;
    }

    const entt::entity inventory = g_registry.create();
    auto& state = g_registry.emplace<ecs::MountInventoryComponent>(inventory);
    state.owner = owner;
    state.accountId = accountId;
    state.height = height;
    state.loaded = true;
    if (!SetMountInventory(owner, inventory))
    {
        g_registry.destroy(inventory);
        return entt::null;
    }
    return inventory;
}

void DestroyMountInventory(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    const auto* ref = g_registry.try_get<ecs::MountInventoryRef>(rider);
    const entt::entity inventory = ref ? ref->inventory : entt::null;
    g_registry.remove<ecs::MountInventoryLoadState>(rider);

    if (ref)
        g_registry.get<ecs::MountInventoryRef>(rider).inventory = entt::null;

    if (inventory == entt::null || !g_registry.valid(inventory))
        return;

    auto* state = g_registry.try_get<ecs::MountInventoryComponent>(inventory);
    if (!state || state->owner != rider)
        return;

    state->destroying = true;
    std::vector<entt::entity> items;
    items.reserve(state->items.size());
    for (auto& item : state->items)
    {
        if (item != entt::null)
            items.push_back(item);
        item = entt::null;
    }
    state->occupied.fill(0);
    state->loaded = false;

    for (const entt::entity item : items)
    {
        if (!ItemSystem::IsValidItem(item))
            continue;

        ItemSystem::SetItemSkipSave(item, true);
        ItemSystem::FlushDelayedSaveEcs(item);
        ItemSystem::RemoveItemEcs(item);
        ItemSystem::DestroyItemEntityEcs(item, "MOUNT_INVENTORY_DESTROY");
    }

    if (g_registry.valid(inventory))
    {
        const auto* current = g_registry.try_get<ecs::MountInventoryComponent>(inventory);
        if (current && current->owner == rider && current->destroying)
            g_registry.destroy(inventory);
    }
}

bool IsMountInventoryPositionValid(entt::entity rider, uint32_t pos)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsValidPosition(*inventory, pos);
}

bool IsMountInventoryPositionEmpty(entt::entity rider, uint32_t pos, uint8_t size)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsEmpty(*inventory, pos, size);
}

int GetMountInventorySize(entt::entity rider)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory ? inventory->height : 0;
}

int GetMountInventoryWidth(entt::entity rider)
{
    return GetMountInventory(rider) != entt::null ? Inventory::Width : 0;
}

entt::entity GetMountInventoryItem(entt::entity rider, uint32_t cell)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsValidPosition(*inventory, cell) ? inventory->items[cell] : entt::null;
}

bool AddMountInventoryItem(entt::entity rider, uint32_t pos,
    entt::entity item, bool skipSave)
{
    const entt::entity inventoryHandle = GetMountInventory(rider);
    auto* inventory = FindInventory(inventoryHandle);
    return inventory && AddToInventory(*inventory, rider, pos, item, skipSave);
}

entt::entity RemoveMountInventoryItem(entt::entity rider, uint32_t pos,
    bool skipDbDelete)
{
    auto* inventory = FindInventoryForOwner(rider);
    if (!inventory || !IsValidPosition(*inventory, pos))
        return entt::null;

    const entt::entity item = inventory->items[pos];
    if (!ItemSystem::IsValidItem(item) ||
        ItemSystem::GetItemOwner(item) != rider ||
        ItemSystem::GetItemWindow(item) != MOUNT_INVENTORY ||
        ItemSystem::GetItemCell(item) != pos ||
        !Detach(*inventory, pos, item, skipDbDelete))
        return entt::null;

    if (!ItemSystem::RemoveItemEcs(item))
    {
        RestoreToInventory(*inventory, rider, pos, item);
        return entt::null;
    }
    return item;
}

bool RemoveMountInventoryItemByEntity(entt::entity rider, entt::entity item,
    bool skipDbDelete)
{
    auto* inventory = FindInventoryForOwner(rider);
    if (!inventory || item == entt::null)
        return false;

    for (uint32_t pos = 0; pos < inventory->items.size(); ++pos)
    {
        if (inventory->items[pos] == item)
            return Detach(*inventory, pos, item, skipDbDelete);
    }
    return false;
}

bool MoveMountInventoryItem(entt::entity rider, uint32_t from, uint32_t to)
{
    auto* inventory = FindInventoryForOwner(rider);
    if (!inventory || !IsValidPosition(*inventory, from) ||
        !IsValidPosition(*inventory, to))
        return false;

    if (from == to)
        return true;

    const entt::entity item = inventory->items[from];
    if (!ItemSystem::IsValidItem(item) ||
        ItemSystem::GetItemOwner(item) != rider ||
        ItemSystem::GetItemWindow(item) != MOUNT_INVENTORY ||
        ItemSystem::GetItemCell(item) != from)
        return false;

    const uint8_t size = ItemSystem::GetItemSize(item);
    if (!IsEmpty(*inventory, to, size))
        return false;

    Get(*inventory, from, size);
    if (!IsEmpty(*inventory, to, size))
    {
        Put(*inventory, from, size);
        return false;
    }

    Put(*inventory, to, size);
    if (!ItemSystem::SetItemCell(item, rider, to))
    {
        Get(*inventory, to, size);
        Put(*inventory, from, size);
        return false;
    }
    inventory->items[from] = entt::null;
    inventory->items[to] = item;
    SaveItem(*inventory, to, item);
    DeleteItem(*inventory, from, 0);
    return true;
}

void CollectMountInventoryItems(entt::entity rider,
    std::vector<TMountInventoryItemTable>& out)
{
    out.clear();
    const auto* inventory = FindInventoryForOwner(rider, false);
    if (!inventory)
        return;

    out.reserve(inventory->items.size());
    for (uint32_t pos = 0; pos < inventory->items.size(); ++pos)
    {
        const entt::entity item = inventory->items[pos];
        if (!ItemSystem::IsValidItem(item))
            continue;

        TMountInventoryItemTable entry{};
        entry.id = ItemSystem::GetItemID(item);
        entry.slot = pos;
        entry.vnum = ItemSystem::GetItemVnum(item);
        entry.count = ItemSystem::GetItemCount(item);
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
            entry.alSockets[i] = ItemSystem::GetItemSocket(item, i);
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
            entry.aAttr[i] = ItemSystem::GetItemAttribute(item, i);
        out.push_back(entry);
    }
}

} // namespace MountSystem
