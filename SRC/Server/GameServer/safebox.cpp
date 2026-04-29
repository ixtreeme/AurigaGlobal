#include "stdafx.h"
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
	assert(m_pkChrOwner != NULL);
	memset(m_pkItems, 0, sizeof(m_pkItems));

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
	for (int i = 0; i < SAFEBOX_MAX_NUM; ++i)
	{
		if (m_pkItems[i])
		{
			ItemSystem::SetItemSkipSave(EntityFactory::CreateItemEntity(g_registry, m_pkItems[i]), true);
			ITEM_MANAGER::instance().FlushDelayedSave(m_pkItems[i]);

			LPITEM removed = m_pkItems[i]->RemoveFromCharacter();
			ItemSystem::DestroyItemEntityEcs(
				EntityFactory::CreateItemEntity(g_registry, removed),
				"SAFEBOX_DESTRUCT");
			m_pkItems[i] = nullptr;
		}
	}

	if (m_pkGrid)
	{
		M2_DELETE(m_pkGrid);
		m_pkGrid = nullptr;
	}
}

bool CSafebox::Add(uint32_t dwPos, LPITEM pkItem)
{
	if (!IsValidPosition(dwPos))
	{
		sys_err("SAFEBOX: item on wrong position at %d (size of grid = %d)", dwPos, m_pkGrid->GetSize());
		return false;
	}

	const entt::entity itemEntity = EntityFactory::CreateItemEntity(g_registry, pkItem);
	ItemSystem::SetItemWindow(itemEntity, m_bWindowMode);
	ItemSystem::SetItemCell(itemEntity, AIHelpers::EcsOf(m_pkChrOwner), dwPos);
	pkItem->Save(); // 강제로 Save를 불러줘야 한다.
	ITEM_MANAGER::instance().FlushDelayedSave(pkItem);

	m_pkGrid->Put(dwPos, 1, pkItem->GetSize());
	m_pkItems[dwPos] = pkItem;

	TPacketGCItemSet pack;

	pack.header	= m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_SET : HEADER_GC_MALL_SET;
	pack.Cell	= TItemPos(m_bWindowMode, dwPos);
	pack.vnum	= ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pkItem));
	pack.count	= ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkItem));
	pack.flags	= pkItem->GetFlag();
#ifdef ATTR_LOCK
	pack.lockedattr = pkItem->GetLockedAttr();
#endif
	pack.anti_flags	= ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, pkItem));
	memcpy(pack.alSockets, pkItem->GetSockets(), sizeof(pack.alSockets));
	memcpy(pack.aAttr, pkItem->GetAttributes(), sizeof(pack.aAttr));

	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pkChrOwner))->Packet(&pack, sizeof(pack));
	sys_log(1, "SAFEBOX: ADD %s %s count %d", m_pkChrOwner->GetName(), pkItem->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkItem)));
	return true;
}

LPITEM CSafebox::Get(uint32_t dwPos)
{
	if (dwPos >= m_pkGrid->GetSize())
		return nullptr;

	return m_pkItems[dwPos];
}

LPITEM CSafebox::Remove(uint32_t dwPos)
{
	LPITEM pkItem = Get(dwPos);

	if (!pkItem)
		return nullptr;

	if (!m_pkGrid)
		sys_err("Safebox::Remove : nil grid");
	else
		m_pkGrid->Get(dwPos, 1, pkItem->GetSize());

	pkItem->RemoveFromCharacter();

	m_pkItems[dwPos] = nullptr;

	TPacketGCItemDel pack;

	pack.header	= m_bWindowMode == SAFEBOX ? HEADER_GC_SAFEBOX_DEL : HEADER_GC_MALL_DEL;
	pack.pos	= dwPos;

	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pkChrOwner))->Packet(&pack, sizeof(pack));
	sys_log(1, "SAFEBOX: REMOVE %s %s count %d", m_pkChrOwner->GetName(), pkItem->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, pkItem)));
	return pkItem;
}

void CSafebox::Save()
{
	TSafeboxTable t;

	memset(&t, 0, sizeof(TSafeboxTable));

	t.dwID = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(m_pkChrOwner))->GetAccountTable().id;
	t.dwGold = m_lGold;

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_SAVE, 0, &t, sizeof(TSafeboxTable));
	sys_log(1, "SAFEBOX: SAVE %s", m_pkChrOwner->GetName());
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

LPITEM CSafebox::GetItem(uint32_t bCell)
{
	if (bCell >= 16 * m_iSize)
	{
		sys_err("CHARACTER::GetItem: invalid item cell %d", bCell);
		return nullptr;
	}

	return m_pkItems[bCell];
}

bool CSafebox::MoveItem(uint32_t bCell, uint32_t bDestCell,
#ifdef ENABLE_NEW_STACK_LIMIT
	uint32_t
#else
uint32_t 
#endif
count)
{
	bool stupid = false;
	if (count < 0)
	{
		sys_err("I am a stupid hacker 5: %s %d", count);
		stupid = true;
	}

	count = static_cast<uint32_t>(std::abs(static_cast<int>(count)));

	if (stupid)
	{
		sys_err("I am a stupid hacker 6: %s %d", count);
		return false;
	}

	LPITEM item;

	int max_position = 16 * m_iSize;

	if (bCell >= max_position || bDestCell >= max_position)
		return false;

	if (!(item = GetItem(bCell)))
		return false;

	if (item->IsExchanging())
		return false;

	if (ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)) < count)
		return false;

	{
		LPITEM item2;

		if ((item2 = GetItem(bDestCell)) && item != item2 && item2->IsStackable() &&
				!IS_SET(ItemSystem::GetItemAntiFlag(EntityFactory::CreateItemEntity(g_registry, item2)), ITEM_ANTIFLAG_STACK) &&
				ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item2)) == ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item))) // 합칠 수 있는 아이템의 경우
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item2), i) != ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), i))
					return false;

			if (count == 0)
				count = ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item));

			count = MIN(g_bItemCountLimit - ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item2)), count);

			if (ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)) >= count)
				Remove(bCell);

			ItemSystem::ConsumeItemEcs(
				EntityFactory::CreateItemEntity(g_registry, item),
				count);
			ItemSystem::AddItemCountEcs(
				EntityFactory::CreateItemEntity(g_registry, item2),
				count);

			sys_log(1, "SAFEBOX: STACK %s %d -> %d %s count %d", m_pkChrOwner->GetName(), bCell, bDestCell, item2->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item2)));
			return true;
		}

		if (!IsEmpty(bDestCell, item->GetSize()))
			return false;

		m_pkGrid->Get(bCell, 1, item->GetSize());

		if (!m_pkGrid->Put(bDestCell, 1, item->GetSize()))
		{
			m_pkGrid->Put(bCell, 1, item->GetSize());
			return false;
		}
		else
		{
			m_pkGrid->Get(bDestCell, 1, item->GetSize());
			m_pkGrid->Put(bCell, 1, item->GetSize());
		}

		sys_log(1, "SAFEBOX: MOVE %s %d -> %d %s count %d", m_pkChrOwner->GetName(), bCell, bDestCell, item->GetName(), ItemSystem::GetItemCount(EntityFactory::CreateItemEntity(g_registry, item)));

		Remove(bCell);
		Add(bDestCell, item);
	}

	return true;
}

bool CSafebox::IsValidPosition(uint32_t dwPos)
{
	if (!m_pkGrid)
		return false;

	if (dwPos >= m_pkGrid->GetSize())
		return false;

	return true;
}


