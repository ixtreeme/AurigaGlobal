#include "AccountCollectionSystem.hpp"
#include "stdafx.h"
#include "PetInventory.h"
#include "char_interface.hpp"
#include "db.h"
#include "desc.h"
#include "item_manager.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/identity_components.hpp"
#include "../ecs/components/inventory_components.hpp"
#include "../ecs/components/character_runtime_components.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PetInventorySystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "pet_inventory_helper.h"
#include "constants.h"
#include "packet.h"
#include <map>
#include <Core/buffer.h>

#include <array>
#include <cstring>
#include <vector>

namespace
{
using Inventory = ecs::PetInventoryComponent;

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
    const entt::entity inventory = PetInventorySystem::GetPetInventory(owner);
    return FindInventory(inventory);
}

const Inventory* FindInventoryForOwner(entt::entity owner, bool allowDestroying)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;

    const auto* ref = g_registry.try_get<ecs::PetInventoryRef>(owner);
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

bool StartPetExpireIfNeeded(entt::entity) { return false; }

uint32_t AccountId(const Inventory& inventory) { return inventory.accountId; }

bool SaveItem(const Inventory& inventory, uint32_t pos, entt::entity itemEntity)
{
    if (!ItemSystem::IsValidItem(itemEntity) || AccountId(inventory) == 0)
        return false;

    // The attribute column list is built from ITEM_ATTRIBUTE_MAX_NUM. The old
    // hardcoded attrtype0..5 dropped the last slot (6, and now 7) on save.
    char query[2048];
    size_t len = 0;
    std::array<int32_t, ITEM_SOCKET_MAX_NUM> sockets {};
    std::array<TPlayerItemAttribute, ITEM_ATTRIBUTE_MAX_NUM> attrs {};
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        sockets[i] = ItemSystem::GetItemSocket(itemEntity, i);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        attrs[i] = ItemSystem::GetItemAttribute(itemEntity, i);

    len += static_cast<size_t>(snprintf(query + len, sizeof(query) - len,
        "REPLACE INTO account_pet_inventory (id, account_id, slot, vnum, count, "
        "socket0, socket1, socket2"));

    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        len += static_cast<size_t>(snprintf(query + len, sizeof(query) - len,
            ", attrtype%d, attrvalue%d", i, i));

    len += static_cast<size_t>(snprintf(query + len, sizeof(query) - len,
		") VALUES(%u, %u, %u, %u, %u, %d, %d, %d",
        ItemSystem::GetItemID(itemEntity),
        AccountId(inventory),
        pos,
        ItemSystem::GetItemVnum(itemEntity),
        ItemSystem::GetItemCount(itemEntity),
        sockets[0], sockets[1], sockets[2]));

    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        len += static_cast<size_t>(snprintf(query + len, sizeof(query) - len,
            ", %d, %.3f", attrs[i].bType, attrs[i].sValue));

    snprintf(query + len, sizeof(query) - len, ")");

    return AccountCollectionSystem::Persist(query,AccountId(inventory),PET_INVENTORY);
}

bool DeleteItem(const Inventory& inventory,uint32_t pos,uint32_t id)
{
    if (!AccountId(inventory) || !id) return false;
    char query[256];snprintf(query,sizeof(query),
        "DELETE FROM account_pet_inventory WHERE account_id=%u AND slot=%u AND id=%u",AccountId(inventory),pos,id);
    return AccountCollectionSystem::Persist(query,AccountId(inventory),PET_INVENTORY);
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
        !ItemSystem::SetItemWindow(itemEntity, PET_INVENTORY) ||
        !ItemSystem::SetItemCell(itemEntity, owner, pos))
    {
        ItemSystem::SetItemSkipSave(itemEntity, false);
        return false;
    }

    Put(inventory, pos, ItemSystem::GetItemSize(itemEntity));
    inventory.items[pos] = itemEntity;

    const bool expireStateChanged = StartPetExpireIfNeeded(itemEntity);
    if ((!skipSave || expireStateChanged) && !SaveItem(inventory, pos, itemEntity))
    {
        ItemSystem::RemoveItemEcs(itemEntity);
        Get(inventory,pos,ItemSystem::GetItemSize(itemEntity));
        inventory.items[pos]=entt::null;
        return false;
    }
    return true;
}

bool RestoreToInventory(Inventory& inventory, entt::entity owner, uint32_t pos,
    entt::entity itemEntity)
{
    if (inventory.owner != owner || inventory.destroying ||
        !ItemSystem::IsValidItem(itemEntity) ||
        !IsEmpty(inventory, pos, ItemSystem::GetItemSize(itemEntity)))
        return false;

    if (!ItemSystem::SetItemWindow(itemEntity, PET_INVENTORY) ||
        !ItemSystem::SetItemCell(itemEntity, owner, pos))
        return false;

    Put(inventory, pos, ItemSystem::GetItemSize(itemEntity));
    inventory.items[pos] = itemEntity;
    return SaveItem(inventory, pos, itemEntity);
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

    if (!skipDbDelete && !DeleteItem(inventory,pos,ItemSystem::GetItemID(item))) return false;
    Get(inventory, pos, ItemSystem::GetItemSize(item));
    inventory.items[pos] = entt::null;
    return true;
}

} // namespace

namespace PetInventorySystem
{

entt::entity GetPetInventory(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return entt::null;

    const auto* ref = g_registry.try_get<ecs::PetInventoryRef>(rider);
    if (!ref || ref->inventory == entt::null)
        return entt::null;

    const auto* inventory = FindInventory(ref->inventory, false);
    if (!inventory || inventory->owner != rider ||
        inventory->accountId != ecs::PlayerRuntime::GetAccountID(rider))
        return entt::null;

    return ref->inventory;
}

bool SetPetInventory(entt::entity rider, entt::entity inventory)
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

    auto& ref = g_registry.get_or_emplace<ecs::PetInventoryRef>(rider);
    if (ref.inventory != entt::null && ref.inventory != inventory)
        return false;
    ref.inventory = inventory;
    return true;
}

entt::entity CreatePetInventory(entt::entity owner, uint32_t accountId,
    uint8_t height)
{
    if (owner == entt::null || !g_registry.valid(owner) || accountId == 0 ||
        height == 0 || height > ecs::PetInventoryComponent::MaxHeight)
        return entt::null;

    if (const entt::entity current = GetPetInventory(owner); current != entt::null)
        return current;

    if (auto* ref = g_registry.try_get<ecs::PetInventoryRef>(owner);
        ref && ref->inventory != entt::null)
    {
        if (g_registry.valid(ref->inventory) &&
            g_registry.try_get<ecs::PetInventoryComponent>(ref->inventory))
            DestroyPetInventory(owner);
        else if (g_registry.valid(ref->inventory))
            g_registry.destroy(ref->inventory);
        ref->inventory = entt::null;
    }

    const entt::entity inventory = g_registry.create();
    auto& state = g_registry.emplace<ecs::PetInventoryComponent>(inventory);
    state.owner = owner;
    state.accountId = accountId;
    state.height = height;
    state.loaded = true;
    if (!SetPetInventory(owner, inventory))
    {
        g_registry.destroy(inventory);
        return entt::null;
    }
    return inventory;
}

void DestroyPetInventory(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    const auto* ref = g_registry.try_get<ecs::PetInventoryRef>(rider);
    const entt::entity inventory = ref ? ref->inventory : entt::null;
    g_registry.remove<ecs::PetInventoryLoadState>(rider);

    if (ref)
        g_registry.get<ecs::PetInventoryRef>(rider).inventory = entt::null;

    if (inventory == entt::null || !g_registry.valid(inventory))
        return;

    auto* state = g_registry.try_get<ecs::PetInventoryComponent>(inventory);
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
        ItemSystem::DestroyItemEntityEcs(item, "PET_INVENTORY_DESTROY");
    }

    if (g_registry.valid(inventory))
    {
        const auto* current = g_registry.try_get<ecs::PetInventoryComponent>(inventory);
        if (current && current->owner == rider && current->destroying)
            g_registry.destroy(inventory);
    }
}

bool IsPetInventoryPositionValid(entt::entity rider, uint32_t pos)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsValidPosition(*inventory, pos);
}

bool IsPetInventoryPositionEmpty(entt::entity rider, uint32_t pos, uint8_t size)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsEmpty(*inventory, pos, size);
}

int GetPetInventorySize(entt::entity rider)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory ? inventory->height : 0;
}

int GetPetInventoryWidth(entt::entity rider)
{
    return GetPetInventory(rider) != entt::null ? Inventory::Width : 0;
}

entt::entity GetPetInventoryItem(entt::entity rider, uint32_t cell)
{
    const auto* inventory = FindInventoryForOwner(rider, false);
    return inventory && IsValidPosition(*inventory, cell) ? inventory->items[cell] : entt::null;
}

bool AddPetInventoryItem(entt::entity rider, uint32_t pos,
    entt::entity item, bool skipSave)
{
    if(!CPetInventoryHelper::CanMoveIntoPetInventory(item))return false;
    const entt::entity inventoryHandle = GetPetInventory(rider);
    auto* inventory = FindInventory(inventoryHandle);
    return inventory && AddToInventory(*inventory, rider, pos, item, skipSave);
}

entt::entity RemovePetInventoryItem(entt::entity rider, uint32_t pos,
    bool skipDbDelete)
{
    auto* inventory = FindInventoryForOwner(rider);
    if (!inventory || !IsValidPosition(*inventory, pos))
        return entt::null;

    const entt::entity item = inventory->items[pos];
    if (!ItemSystem::IsValidItem(item) ||
        ItemSystem::GetItemOwner(item) != rider ||
        ItemSystem::GetItemWindow(item) != PET_INVENTORY ||
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

bool RemovePetInventoryItemByEntity(entt::entity rider, entt::entity item,
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

bool MovePetInventoryItem(entt::entity rider, uint32_t from, uint32_t to)
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
        ItemSystem::GetItemWindow(item) != PET_INVENTORY ||
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
    // The unique item ID makes REPLACE an atomic move between bank cells.
    if (!SaveItem(*inventory,to,item)) {
        inventory->items[to]=entt::null;inventory->items[from]=item;
        Get(*inventory,to,size);Put(*inventory,from,size);
        ItemSystem::SetItemCell(item,rider,from);
        return false;
    }
    return true;
}

void CollectPetInventoryItems(entt::entity rider,
    std::vector<TPetInventoryItemTable>& out)
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

        TPetInventoryItemTable entry{};
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

} // namespace PetInventorySystem

namespace {
uint64_t NextPetInventoryRequestId()
{
    static uint64_t nextRequestId = 0;
    return ++nextRequestId;
}
}

void PetInventorySystem::QueryPetInventory(entt::entity e)
{
    if (PetInventorySystem::GetPetInventory(e) != entt::null || !ecs::PlayerRuntime::GetDesc(e) ||
        !g_registry.valid(e))
        return;

    const uint32_t accountId = ecs::PlayerRuntime::GetAccountID(e);
    if (accountId == 0)
        return;

    if (const auto* pending = g_registry.try_get<ecs::PetInventoryLoadState>(e);
        pending && pending->accountId == accountId)
        return;

    const uint64_t requestId = NextPetInventoryRequestId();
    g_registry.emplace_or_replace<ecs::PetInventoryLoadState>(e, accountId, requestId);

    auto* request = new PetInventoryLoadRequest;
    request->character = e;
    request->accountId = accountId;
    request->requestId = requestId;

    DBManager::instance().ReturnQuery(QID_PET_INVENTORY_LOAD,
        ecs::PlayerRuntime::GetPlayerID(e),
        request,
        "SELECT id, slot, vnum, count, socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5, "
        "attrtype6, attrvalue6, attrtype7, attrvalue7 "
        "FROM account_pet_inventory WHERE account_id=%u ORDER BY slot",
        accountId);
}

void PetInventorySystem::LoadPetInventory(entt::entity e, uint32_t accountId,
    uint64_t requestId, const std::vector<TPetInventoryItemTable>& items)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* pending = g_registry.try_get<ecs::PetInventoryLoadState>(e);
    if (accountId == 0 || !pending || pending->accountId != accountId ||
        pending->requestId != requestId || ecs::PlayerRuntime::GetAccountID(e) != accountId)
        return;

    if (PetInventorySystem::GetPetInventory(e) != entt::null)
    {
        g_registry.remove<ecs::PetInventoryLoadState>(e);
        return;
    }

    const entt::entity inventory = PetInventorySystem::CreatePetInventory(e, accountId, 16);
    if (inventory == entt::null)
    {
        g_registry.remove<ecs::PetInventoryLoadState>(e);
        return;
    }

    for (const auto& entry : items)
    {
        const entt::entity item = ITEM_MANAGER::instance().CreateItem(
            entry.vnum, entry.count, entry.id);
        if (!ItemSystem::IsValidItem(item))
            continue;

        ItemSystem::SetItemSkipSave(item, true);
        for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
            ItemSystem::SetItemSocketEcs(item, socket, entry.alSockets[socket]);
        for (int attribute = 0; attribute < ITEM_ATTRIBUTE_MAX_NUM; ++attribute)
            ItemSystem::SetItemForceAttributeEcs(
                item, attribute, entry.aAttr[attribute].bType,
                entry.aAttr[attribute].sValue);

        if (!PetInventorySystem::AddPetInventoryItem(e, entry.slot, item, true))
            ItemSystem::DestroyItemEntityEcs(item, "PET_INVENTORY_LOAD_ADD_FAILED");
    }

    g_registry.remove<ecs::PetInventoryLoadState>(e);
    if (!g_registry.valid(e) || PetInventorySystem::GetPetInventory(e) != inventory)
        return;

    PetInventorySystem::SendPetInventory(e);
    if (g_registry.valid(e))
        ecs::PointSystem::Compute(e);
}

void PetInventorySystem::SendPetInventory(entt::entity owner)
{
    if (!ecs::PlayerRuntime::GetDesc(owner))
        return;
    if (GetPetInventory(owner) == entt::null)
        return;

    std::vector<TPetInventoryItemTable> items;
    CollectPetInventoryItems(owner, items);

    TPacketGCPetInventory header{};
    header.bHeader = HEADER_GC_PET_INVENTORY;
    header.size = sizeof(TPacketGCPetInventory) + static_cast<uint16_t>(items.size() * sizeof(TPetInventoryItemData));
    header.bWidth = static_cast<uint8_t>(GetPetInventoryWidth(owner));
    header.bHeight = static_cast<uint8_t>(GetPetInventorySize(owner));
    header.wCount = static_cast<uint16_t>(items.size());

    TEMP_BUFFER buf;
    buf.write(&header, sizeof(header));

    for (const auto& entry : items)
    {
        TPetInventoryItemData data{};
        data.wSlot = entry.slot;
        data.dwVnum = entry.vnum;
        data.dwCount = entry.count;
        memcpy(data.alSockets, entry.alSockets, sizeof(data.alSockets));
        memcpy(data.aAttr, entry.aAttr, sizeof(data.aAttr));
        buf.write(&data, sizeof(data));
    }

    if (auto* desc = ecs::PlayerRuntime::GetDesc(owner))
        desc->Packet(buf.read_peek(), buf.size());
    AccountCollectionSystem::Send(owner,PET_INVENTORY);
}

void PetInventorySystem::ComputePetInventoryBonuses(entt::entity owner)
{
    AccountCollectionSystem::Compute(owner,PET_INVENTORY);
}
