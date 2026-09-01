#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Base/grid.h>
#include "constants.h"
#include "safebox.h"
#include "packet.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "desc_client.h"
#include "item.h"
#include "item_manager.h"
#include "config.h"

CSafebox::CSafebox(LPCHARACTER pkChrOwner, int iSize, uint32_t dwGold) : m_pkChrOwner(pkChrOwner), m_iSize(iSize), m_lGold(dwGold)
{
	assert(m_pkChrOwner != nullptr);
	m_items.fill(entt::null);

	if (m_iSize)
		m_pkGrid = M2_NEW CGrid(16, m_iSize);
	else
		m_pkGrid = nullptr;

	m_bWindowMode = SAFEBOX;
}

CSafebox::~CSafebox()
{
	__Destroy();
}

void CSafebox::SetWindowMode(uint8_t bMode)
{
	m_bWindowMode = bMode;
}

void CSafebox::__Destroy()
{
	for (entt::entity& item : m_items)
	{
		if (!ItemSystem::IsValidItem(item))
		{
			item = entt::null;
			continue;
		}

		const entt::entity itemToDestroy = item;
		item = entt::null;
		ItemSystem::SetItemSkipSave(itemToDestroy, true);
		ItemSystem::FlushDelayedSaveEcs(itemToDestroy);
		ItemSystem::RemoveItemEcs(itemToDestroy);
		ItemSystem::DestroyItemEntityEcs(itemToDestroy, "SAFEBOX_DESTRUCT");
	}

	if (m_pkGrid)
	{
		M2_DELETE(m_pkGrid);
		m_pkGrid = nullptr;
	}
}

bool CSafebox::Add(uint32_t dwPos, entt::entity item)
{
	if (!IsValidPosition(dwPos) || !ItemSystem::IsValidItem(item))
	{
		LOG_ERROR("SAFEBOX: item on wrong position at {}", dwPos);
		return false;
	}

	ItemSystem::SetItemWindow(item, m_bWindowMode);
	ItemSystem::SetItemCell(item, ((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null), dwPos);
	if (!ItemSystem::SaveItemEcs(item))
		return false;

	m_pkGrid->Put(dwPos, 1, ItemSystem::GetItemSize(item));
	m_items[dwPos] = item;

	TPacketGCItemSet pack{};
	pack.header = m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.Cell = TItemPos(m_bWindowMode, dwPos);
	pack.vnum = ItemSystem::GetItemVnum(item);
	pack.count = ItemSystem::GetItemCount(item);
	pack.flags = ItemSystem::GetItemFlags(item);
#ifdef ATTR_LOCK
	pack.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
	pack.anti_flags = ItemSystem::GetItemAntiFlags(item);
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		pack.alSockets[i] = ItemSystem::GetItemSocket(item, i);
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		pack.aAttr[i] = ItemSystem::GetItemAttribute(item, i);

	if (LPDESC desc = ecs::PlayerRuntime::GetDesc(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)))
		desc->Packet(&pack, sizeof(pack));
	LOG_INFO("SAFEBOX: ADD {} {} count {}", ecs::PlayerRuntime::GetName(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
	return true;
}

entt::entity CSafebox::Get(uint32_t dwPos) const
{
	if (!m_pkGrid || dwPos >= m_pkGrid->GetSize())
		return entt::null;

	const entt::entity item = m_items[dwPos];
	return ItemSystem::IsValidItem(item) ? item : entt::null;
}

entt::entity CSafebox::Remove(uint32_t dwPos)
{
	const entt::entity item = Get(dwPos);
	if (!ItemSystem::IsValidItem(item))
		return entt::null;

	if (!m_pkGrid)
		LOG_ERROR("Safebox::Remove : nil grid");
	else
		m_pkGrid->Get(dwPos, 1, ItemSystem::GetItemSize(item));

	m_items[dwPos] = entt::null;
	ItemSystem::RemoveItemEcs(item);

	TPacketGCItemDel pack{};
	pack.header = m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos = dwPos;
	if (LPDESC desc = ecs::PlayerRuntime::GetDesc(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)))
		desc->Packet(&pack, sizeof(pack));
	LOG_INFO("SAFEBOX: REMOVE {} {} count {}", ecs::PlayerRuntime::GetName(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)).data(), ItemSystem::GetItemName(item), ItemSystem::GetItemCount(item));
	return item;
}

void CSafebox::Save()
{
	TSafeboxTable t;

	memset(&t, 0, sizeof(TSafeboxTable));

	t.dwID = ecs::PlayerRuntime::GetDesc(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null))->GetAccountTable().id;
	t.dwGold = m_lGold;

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(TSafeboxTable));
	LOG_INFO("SAFEBOX: SAVE {}", ecs::PlayerRuntime::GetName(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)).data());
}

bool CSafebox::IsEmpty(uint32_t dwPos, uint8_t bSize)
{
	if (!m_pkGrid)
		return false;

	return m_pkGrid->IsEmpty(dwPos, 1, bSize);
}

void CSafebox::ChangeSize(int iSize)
{
	// 현재 사이즈가 인자보다 크면 사이즈를 가만 둔다.
	if (m_iSize >= iSize)
		return;

	m_iSize = iSize;

	CGrid * pkOldGrid = m_pkGrid;

	if (pkOldGrid) {
		m_pkGrid = M2_NEW CGrid(pkOldGrid, 16, m_iSize);
#ifdef ENABLE_BUG_FIXES
		delete pkOldGrid;
#endif
	} else {
		m_pkGrid = M2_NEW CGrid(16, m_iSize);
	}
}

entt::entity CSafebox::GetItem(uint32_t bCell) const
{
	if (bCell >= static_cast<uint32_t>(16 * m_iSize))
	{
		LOG_ERROR("CHARACTER::GetItem: invalid item cell {}", bCell);
		return entt::null;
	}

	return Get(bCell);
}

bool CSafebox::MoveItem(uint32_t bCell, uint32_t bDestCell, uint32_t count)
{
	const uint32_t maxPosition = static_cast<uint32_t>(16 * m_iSize);
	if (bCell >= maxPosition || bDestCell >= maxPosition)
		return false;

	const entt::entity item = GetItem(bCell);
	if (!ItemSystem::IsValidItem(item) || ItemSystem::IsItemExchanging(item))
		return false;

	const uint32_t sourceCount = ItemSystem::GetItemCount(item);
	if (sourceCount < count)
		return false;

	const entt::entity destination = GetItem(bDestCell);
	if (ItemSystem::IsValidItem(destination) && destination != item &&
		IS_SET(ItemSystem::GetItemFlags(destination), ITEM_FLAG_STACKABLE) &&
		!IS_SET(ItemSystem::GetItemAntiFlags(destination), ITEM_ANTIFLAG_STACK) &&
		ItemSystem::GetItemVnum(destination) == ItemSystem::GetItemVnum(item))
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (ItemSystem::GetItemSocket(destination, i) != ItemSystem::GetItemSocket(item, i))
				return false;

		if (count == 0)
			count = sourceCount;

		count = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(destination), count);
		if (count == 0)
			return false;

		if (count >= sourceCount)
			Remove(bCell);

		ItemSystem::ConsumeItemEcs(item, count);
		ItemSystem::AddItemCountEcs(destination, count);
		LOG_INFO("SAFEBOX: STACK {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)).data(), static_cast<int>(bCell), static_cast<int>(bDestCell), ItemSystem::GetItemName(destination), ItemSystem::GetItemCount(destination));
		return true;
	}

	if (!IsEmpty(bDestCell, ItemSystem::GetItemSize(item)))
		return false;

	LOG_INFO("SAFEBOX: MOVE {} {} -> {} {} count {}", ecs::PlayerRuntime::GetName(((m_pkChrOwner) ? (m_pkChrOwner)->GetEntityHandle() : entt::null)).data(), static_cast<int>(bCell), static_cast<int>(bDestCell), ItemSystem::GetItemName(item), sourceCount);
	if (Remove(bCell) == entt::null)
		return false;
	return Add(bDestCell, item);
}

bool CSafebox::IsValidPosition(uint32_t dwPos)
{
	if (!m_pkGrid)
		return false;

	if (dwPos >= m_pkGrid->GetSize())
		return false;

	return true;
}


