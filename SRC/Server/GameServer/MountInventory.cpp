#include "stdafx.h"
#include "MountInventory.h"
#include "char_interface.hpp"
#include "db.h"
#include "desc.h"
#include "item_manager.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include <cstring>
namespace
{
    bool StartMountExpireIfNeeded(LPITEM item)
    {
        if (!item)
            return false;

        const entt::entity itemEntity = EntityFactory::CreateItemEntity(g_registry, item);
        const TItemTable* itemProto = ItemSystem::GetItemProto(itemEntity);
        if (!itemProto)
            return false;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
        const bool bIsMountLikeItem = item->IsRideItem() || item->IsMountItem();
#else
        const bool bIsMountLikeItem = item->IsRideItem();
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

            item->SetSocket(0, time(nullptr) + duration);
            item->SetSocket(1, 1); // innentol "aktivalt"
            bChanged = true;
        }

        item->StartRealTimeExpireEvent();
        return bChanged;
    }
}
CMountInventory::CMountInventory(LPCHARACTER pkOwner, int iHeight)
    : m_pkOwner(pkOwner), m_iHeight(iHeight)
{
    m_items.assign(MOUNT_INVENTORY_WIDTH * m_iHeight, nullptr);
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

LPITEM CMountInventory::Get(uint32_t pos) const
{
    if (!IsValidPosition(pos))
        return nullptr;

    return m_items[pos];
}

bool CMountInventory::Add(uint32_t pos, LPITEM item, bool skipSave)
{
    if (!item || !IsValidPosition(pos))
        return false;

    if (!IsEmpty(pos, item->GetSize()))
        return false;

    item->SetSkipSave(true);
    item->SetWindow(MOUNT_INVENTORY);
    item->SetCell(m_pkOwner, pos);

    m_grid->Put(pos, 1, item->GetSize());
    m_items[pos] = item;

    const bool bExpireStateChanged = StartMountExpireIfNeeded(item);

    if (!skipSave || bExpireStateChanged)
        SaveItem(pos, item);

    return true;
}



bool CMountInventory::DetachSlot(uint32_t pos, LPITEM expectedItem, bool skipDbDelete)
{
    if (!IsValidPosition(pos))
        return false;

    LPITEM item = m_items[pos];
    if (!item)
        return false;

    if (expectedItem && item != expectedItem)
        return false;

    if (m_grid)
        m_grid->Get(pos, 1, item->GetSize());

    m_items[pos] = nullptr;

    if (!skipDbDelete)
        DeleteItem(pos, ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, item)));

    return true;
}

bool CMountInventory::RemoveByItem(LPITEM item, bool skipDbDelete)
{
    if (!item)
        return false;

    for (uint32_t pos = 0; pos < m_items.size(); ++pos)
    {
        if (m_items[pos] != item)
            continue;

        return DetachSlot(pos, item, skipDbDelete);
    }

    return false;
}

LPITEM CMountInventory::Remove(uint32_t pos, bool skipDbDelete)
{
    LPITEM item = Get(pos);

    if (!item)
        return nullptr;

    DetachSlot(pos, item, skipDbDelete);
    item->RemoveFromCharacter();
    return item;
}

bool CMountInventory::MoveItem(uint32_t from, uint32_t to)
{
    LPITEM item = Get(from);

    if (!item || !IsValidPosition(to))
        return false;

    if (!IsEmpty(to, item->GetSize()))
        return false;

    if (m_grid)
    {
        m_grid->Get(from, 1, item->GetSize());

        if (!m_grid->Put(to, 1, item->GetSize()))
        {
            m_grid->Put(from, 1, item->GetSize());
            return false;
        }
    }

    m_items[from] = nullptr;
    m_items[to] = item;

    item->SetCell(m_pkOwner, to);
    SaveItem(to, item);
    DeleteItem(from, 0);
    return true;
}

uint32_t CMountInventory::GetAccountId() const
{
    return m_pkOwner && m_pkOwner->GetDesc() ? m_pkOwner->GetDesc()->GetAccountTable().id : 0;
}

void CMountInventory::CollectItems(std::vector<TMountInventoryItemTable>& out) const
{
    out.clear();
    out.reserve(m_items.size());

    for (uint32_t pos = 0; pos < m_items.size(); ++pos)
    {
        const LPITEM item = m_items[pos];
        if (!item)
            continue;

        TMountInventoryItemTable entry{};
        entry.id = ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, item));
        entry.slot = pos;
        entry.vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
        entry.count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item));

        auto sockets = item->GetSockets();
        memcpy(entry.alSockets, sockets, sizeof(entry.alSockets));

        const TPlayerItemAttribute* attrs = item->GetAttributes();
        memcpy(entry.aAttr, attrs, sizeof(entry.aAttr));

        out.push_back(entry);
    }
}

void CMountInventory::SaveItem(uint32_t pos, LPITEM item)
{
    if (!item)
        return;

    const uint32_t accountId = GetAccountId();
    if (accountId == 0)
        return;

    char query[512];
    auto sockets = item->GetSockets();
    const TPlayerItemAttribute* attrs = item->GetAttributes();

    snprintf(query, sizeof(query),
        "REPLACE INTO account_mount_inventory (id, account_id, slot, vnum, count, "
        "socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5) "
        "VALUES(%u, %u, %u, %u, %u, %ld, %ld, %ld, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
        ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, item)),
        accountId,
        pos,
        ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item)),
        ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)),
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
        LPITEM item = m_items[pos];
        if (!item)
            continue;

        if (m_grid)
            m_grid->Get(pos, 1, item->GetSize());

        m_items[pos] = nullptr;

        item->SetSkipSave(true);
        ITEM_MANAGER::instance().FlushDelayedSave(item);
        LPITEM removed = item->RemoveFromCharacter();
        ItemSystem::DestroyItemEntityEcs(
            EntityFactory::CreateItemEntity(g_registry, removed),
            "MOUNT_INVENTORY_DESTROY");
    }

    m_items.clear();
    m_grid.reset();
}
