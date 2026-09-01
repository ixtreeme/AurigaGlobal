#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "MountInventory.h"
#include "char_interface.hpp"
#include "db.h"
#include "desc.h"
#include "item_manager.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include <array>
namespace
{
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
        if (!bIsMountLikeItem)
            return false;

        if (-1 == itemProto->cLimitRealTimeFirstUseIndex)
            return false;

        bool bChanged = false;

        if (ItemSystem::GetItemSocket(itemEntity, 1) == 0)
        {
            const uint8_t idx = static_cast<uint8_t>(itemProto->cLimitRealTimeFirstUseIndex);

            int32_t duration = ItemSystem::GetItemSocket(itemEntity, 0);
            if (duration == 0)
                duration = itemProto->aLimits[idx].lValue;

            if (duration == 0)
                duration = 60 * 60 * 24 * 7;

            ItemSystem::SetItemSocket(itemEntity, 0, time(nullptr) + duration);
            ItemSystem::SetItemSocket(itemEntity, 1, 1); // innentol "aktivalt"
            bChanged = true;
        }

        ItemSystem::StartRealTimeExpireEventEcs(itemEntity);
        return bChanged;
    }
}
CMountInventory::CMountInventory(LPCHARACTER pkOwner, int iHeight)
    : m_pkOwner(pkOwner), m_iHeight(iHeight)
{
    m_items.assign(MOUNT_INVENTORY_WIDTH * m_iHeight, entt::null);
    m_grid = std::make_unique<CGrid>(MOUNT_INVENTORY_WIDTH, m_iHeight);
}

CMountInventory::~CMountInventory()
{
    Destroy();
}

bool CMountInventory::IsValidPosition(uint32_t pos) const
{
    return m_grid && pos < m_grid->GetSize();
}

bool CMountInventory::IsEmpty(uint32_t pos, uint8_t size) const
{
    if (!m_grid)
        return false;

    return m_grid->IsEmpty(pos, 1, size);
}

entt::entity CMountInventory::Get(uint32_t pos) const
{
    if (!IsValidPosition(pos))
        return entt::null;

    return m_items[pos];
}

bool CMountInventory::Add(uint32_t pos, entt::entity itemEntity, bool skipSave)
{
    if (!ItemSystem::IsValidItem(itemEntity) || !IsValidPosition(pos))
        return false;

    if (!IsEmpty(pos, ItemSystem::GetItemSize(itemEntity)))
        return false;

    ItemSystem::SetItemSkipSave(itemEntity, true);
    ItemSystem::SetItemWindow(itemEntity, MOUNT_INVENTORY);
    ItemSystem::SetItemCell(itemEntity, AIHelpers::EcsOf(m_pkOwner), pos);

    m_grid->Put(pos, 1, ItemSystem::GetItemSize(itemEntity));
    m_items[pos] = itemEntity;

    const bool bExpireStateChanged = StartMountExpireIfNeeded(itemEntity);

    if (!skipSave || bExpireStateChanged)
        SaveItem(pos, itemEntity);

    return true;
}



bool CMountInventory::DetachSlot(uint32_t pos, entt::entity expectedItem, bool skipDbDelete)
{
    if (!IsValidPosition(pos))
        return false;

    const entt::entity item = m_items[pos];
    if (!ItemSystem::IsValidItem(item))
        return false;

    if (expectedItem != entt::null && item != expectedItem)
        return false;

    if (m_grid)
        m_grid->Get(pos, 1, ItemSystem::GetItemSize(item));

    m_items[pos] = entt::null;

    if (!skipDbDelete)
        DeleteItem(pos, ItemSystem::GetItemID(item));

    return true;
}

bool CMountInventory::RemoveByItem(entt::entity itemEntity, bool skipDbDelete)
{
    if (itemEntity == entt::null)
        return false;

    for (uint32_t pos = 0; pos < m_items.size(); ++pos)
    {
        if (m_items[pos] == entt::null || m_items[pos] != itemEntity)
            continue;

        return DetachSlot(pos, itemEntity, skipDbDelete);
    }

    return false;
}

entt::entity CMountInventory::Remove(uint32_t pos, bool skipDbDelete)
{
    const entt::entity item = Get(pos);

    if (!ItemSystem::IsValidItem(item))
        return entt::null;

    DetachSlot(pos, item, skipDbDelete);
    ItemSystem::RemoveItemEcs(item);
    return item;
}

bool CMountInventory::MoveItem(uint32_t from, uint32_t to)
{
    const entt::entity item = Get(from);

    if (!ItemSystem::IsValidItem(item) || !IsValidPosition(to))
        return false;

    if (!IsEmpty(to, ItemSystem::GetItemSize(item)))
        return false;

    if (m_grid)
    {
        m_grid->Get(from, 1, ItemSystem::GetItemSize(item));

        if (!m_grid->Put(to, 1, ItemSystem::GetItemSize(item)))
        {
            m_grid->Put(from, 1, ItemSystem::GetItemSize(item));
            return false;
        }
    }

    m_items[from] = entt::null;
    m_items[to] = item;

    ItemSystem::SetItemCell(item, AIHelpers::EcsOf(m_pkOwner), to);
    SaveItem(to, item);
    DeleteItem(from, 0);
    return true;
}

uint32_t CMountInventory::GetAccountId() const
{
    return m_pkOwner && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pkOwner)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pkOwner))->GetAccountTable().id : 0;
}

void CMountInventory::CollectItems(std::vector<TMountInventoryItemTable>& out) const
{
    out.clear();
    out.reserve(m_items.size());

    for (uint32_t pos = 0; pos < m_items.size(); ++pos)
    {
        const entt::entity item = m_items[pos];
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

void CMountInventory::SaveItem(uint32_t pos, entt::entity itemEntity)
{
    if (!ItemSystem::IsValidItem(itemEntity))
        return;

    const uint32_t accountId = GetAccountId();
    if (accountId == 0)
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
        accountId,
        pos,
        ItemSystem::GetItemVnum(itemEntity),
        ItemSystem::GetItemCount(itemEntity),
        sockets[0],
        sockets[1],
        sockets[2],
        attrs[0].bType, attrs[0].sValue,
        attrs[1].bType, attrs[1].sValue,
        attrs[2].bType, attrs[2].sValue,
        attrs[3].bType, attrs[3].sValue,
        attrs[4].bType, attrs[4].sValue,
        attrs[5].bType, attrs[5].sValue);

    DBManager::instance().Query("%s", query);
}

void CMountInventory::DeleteItem(uint32_t pos, uint32_t id)
{
    const uint32_t accountId = GetAccountId();
    if (accountId == 0)
        return;

    DBManager::instance().Query(
        "DELETE FROM account_mount_inventory WHERE account_id=%u AND slot=%u",
        accountId,
        pos);

    if (id != 0)
    {
        DBManager::instance().Query(
            "DELETE FROM account_mount_inventory WHERE id=%u",
            id);
    }
}

void CMountInventory::Destroy()
{
    for (uint32_t pos = 0; pos < m_items.size(); ++pos)
    {
        const entt::entity item = m_items[pos];
        if (!ItemSystem::IsValidItem(item))
            continue;

        if (m_grid)
            m_grid->Get(pos, 1, ItemSystem::GetItemSize(item));

        m_items[pos] = entt::null;

        ItemSystem::SetItemSkipSave(item, true);
        ItemSystem::FlushDelayedSaveEcs(item);
        ItemSystem::RemoveItemEcs(item);
        ItemSystem::DestroyItemEntityEcs(item, "MOUNT_INVENTORY_DESTROY");
    }

    m_items.clear();
    m_grid.reset();
}
