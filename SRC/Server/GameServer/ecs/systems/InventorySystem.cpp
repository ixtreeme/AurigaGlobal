#include "../../stdafx.h"

#include "InventorySystem.hpp"

#include "../../config.h"
#include "../../char.h"
#include "../../desc.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../MountInventory.h"
#include "../../packet.h"
#include "../../sectree_manager.h"
#include "../AIHelpers.hpp"
#include "../ItemRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/inventory_components.hpp"

namespace
{

ecs::QuickSlots* GetQuickSlots(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<ecs::QuickSlots>(e);
}

LPITEM LegacyItemOf(entt::entity e)
{
	auto* id = g_registry.try_get<ecs::ItemIdentity>(e);
	if (!id)
		return nullptr;

	return ITEM_MANAGER::instance().Find(id->id);
}

entt::entity ItemEntityOf(LPITEM item)
{
	if (!item)
		return entt::null;

	return CItemRegistry::Instance().Find(item->GetID());
}

void SyncItemLocation(entt::entity e, LPITEM item)
{
	if (e == entt::null || !item || !g_registry.valid(e))
		return;

	g_registry.emplace_or_replace<ecs::ItemLocation>(e,
		static_cast<uint8_t>(item->GetWindow()),
		static_cast<uint16_t>(item->GetCell()));
}

void SyncItemOwner(entt::entity e, uint32_t ownerPID, uint32_t lastOwnerPID, uint32_t ownershipPID)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.emplace_or_replace<ecs::ItemOwner>(e, ownerPID, lastOwnerPID, ownershipPID);
}

void SyncItemEquipped(entt::entity e, bool equipped)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.emplace_or_replace<ecs::ItemEquipped>(e, equipped);
}

} // namespace

EVENTFUNC(ownership_event);

namespace InventorySystem {

void SyncQuickslot(entt::entity e, uint8_t bType, uint8_t bOldPos, uint8_t bNewPos)
{
	if (bOldPos == bNewPos)
		return;

	auto* qs = GetQuickSlots(e);
	if (!qs)
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (qs->slots[i].type == bType && qs->slots[i].pos == bOldPos)
		{
			if (bNewPos == 255)
				memset(&qs->slots[i], 0, sizeof(TQuickslot));
			else
			{
				qs->slots[i].type = bType;
				qs->slots[i].pos = bNewPos;
			}
		}
	}

	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

bool GetQuickslot(entt::entity e, uint8_t pos, TQuickslot& out)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return false;

	out = qs->slots[pos];
	return true;
}

void SetQuickslot(entt::entity e, uint8_t pos, const TQuickslot& slot)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return;

	qs->slots[pos] = slot;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void DelQuickslot(entt::entity e, uint8_t pos)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || pos >= QUICKSLOT_MAX_NUM)
		return;

	memset(&qs->slots[pos], 0, sizeof(TQuickslot));
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void SwapQuickslot(entt::entity e, uint8_t posA, uint8_t posB)
{
	auto* qs = GetQuickSlots(e);
	if (!qs || posA >= QUICKSLOT_MAX_NUM || posB >= QUICKSLOT_MAX_NUM)
		return;

	const TQuickslot slot = qs->slots[posA];
	qs->slots[posA] = qs->slots[posB];
	qs->slots[posB] = slot;
	g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace InventorySystem

void CHARACTER::SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos)
{
	if (bOldPos == bNewPos)
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (m_quickslot[i].type == bType && m_quickslot[i].pos == bOldPos)
		{
			if (bNewPos == 255)
				DelQuickslot(i);
			else
			{
				TQuickslot slot;
				slot.type = bType;
				slot.pos = bNewPos;

				SetQuickslot(i, slot);
			}
		}
	}

	InventorySystem::SyncQuickslot(AIHelpers::EcsOf(this), bType, bOldPos, bNewPos);
}

bool CHARACTER::GetQuickslot(uint8_t pos, TQuickslot** ppSlot)
{
	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	*ppSlot = &m_quickslot[pos];

	TQuickslot ecsSlot {};
	if (InventorySystem::GetQuickslot(AIHelpers::EcsOf(this), pos, ecsSlot))
		m_quickslot[pos] = ecsSlot;
	else
		InventorySystem::SetQuickslot(AIHelpers::EcsOf(this), pos, m_quickslot[pos]);

	return true;
}

bool CHARACTER::SetQuickslot(uint8_t pos, TQuickslot& rSlot)
{
	packet_quickslot_add pack_quickslot_add;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	if (rSlot.type >= QUICKSLOT_TYPE_MAX_NUM)
		return false;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (rSlot.type == 0)
			continue;
		else if (m_quickslot[i].type == rSlot.type && m_quickslot[i].pos == rSlot.pos)
			DelQuickslot(i);
	}

#ifdef ENABLE_EXTRA_INVENTORY
	uint8_t type = rSlot.type == QUICKSLOT_TYPE_ITEM_EXTRA ? EXTRA_INVENTORY : INVENTORY;
	TItemPos srcCell(type, rSlot.pos);
#else
	TItemPos srcCell(INVENTORY, rSlot.pos);
#endif

	switch (rSlot.type)
	{
#ifdef ENABLE_EXTRA_INVENTORY
		case QUICKSLOT_TYPE_ITEM_EXTRA:
			{
				if (rSlot.pos >= EXTRA_INVENTORY_MAX_NUM)
					return false;

				break;
			}
#endif

		case QUICKSLOT_TYPE_ITEM:
			{
				if (false == srcCell.IsDefaultInventoryPosition() && false == srcCell.IsBeltInventoryPosition())
					return false;
			}
			break;

		case QUICKSLOT_TYPE_SKILL:
			if (rSlot.pos >= SKILL_MAX_NUM)
				return false;
			break;

		case QUICKSLOT_TYPE_COMMAND:
			break;

		default:
			return false;
	}

	m_quickslot[pos] = rSlot;
	InventorySystem::SetQuickslot(AIHelpers::EcsOf(this), pos, rSlot);

	if (GetDesc())
	{
		pack_quickslot_add.header = HEADER_GC_QUICKSLOT_ADD;
		pack_quickslot_add.pos = pos;
		pack_quickslot_add.slot = m_quickslot[pos];

		GetDesc()->Packet(&pack_quickslot_add, sizeof(pack_quickslot_add));
	}

	return true;
}

bool CHARACTER::DelQuickslot(uint8_t pos)
{
	packet_quickslot_del pack_quickslot_del;

	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	memset(&m_quickslot[pos], 0, sizeof(TQuickslot));
	InventorySystem::DelQuickslot(AIHelpers::EcsOf(this), pos);

	pack_quickslot_del.header = HEADER_GC_QUICKSLOT_DEL;
	pack_quickslot_del.pos = pos;

	if (GetDesc())
		GetDesc()->Packet(&pack_quickslot_del, sizeof(pack_quickslot_del));

	return true;
}

bool CHARACTER::SwapQuickslot(uint8_t a, uint8_t b)
{
	packet_quickslot_swap pack_quickslot_swap;

	if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
		return false;

	const TQuickslot quickslot = m_quickslot[a];

	m_quickslot[a] = m_quickslot[b];
	m_quickslot[b] = quickslot;
	InventorySystem::SwapQuickslot(AIHelpers::EcsOf(this), a, b);

	pack_quickslot_swap.header = HEADER_GC_QUICKSLOT_SWAP;
	pack_quickslot_swap.pos = a;
	pack_quickslot_swap.pos_to = b;

	if (GetDesc())
		GetDesc()->Packet(&pack_quickslot_swap, sizeof(pack_quickslot_swap));

	return true;
}

void CHARACTER::ChainQuickslotItem(LPITEM pItem, uint8_t bType, uint8_t bOldPos)
{
	if (pItem->IsDragonSoul())
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (m_quickslot[i].type == bType && m_quickslot[i].pos == bOldPos)
		{
			TQuickslot slot;
			slot.type = bType;
			slot.pos = pItem->GetCell();

			SetQuickslot(i, slot);
			break;
		}
	}
}

LPITEM CItem::RemoveFromCharacter()
{
	if (!m_pOwner)
	{
		sys_err("Item::RemoveFromCharacter owner null");
		return (this);
	}

	LPCHARACTER pOwner = m_pOwner;

	if (m_bEquipped)
	{
		Unequip();
		SetWindow(RESERVED_WINDOW);
		Save();

		const entt::entity itemEntity = ItemEntityOf(this);
		SyncItemLocation(itemEntity, this);
		SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		return (this);
	}
	else
	{
		if (GetWindow() == MOUNT_INVENTORY)
		{
			if (CMountInventory* mi = pOwner->GetMountInventory())
				mi->RemoveByItem(this);

			m_pOwner = nullptr;
			m_wCell = 0;
			SetWindow(RESERVED_WINDOW);
			Save();

			const entt::entity itemEntity = ItemEntityOf(this);
			SyncItemLocation(itemEntity, this);
			SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
			g_registry.remove<ecs::ItemEquipped>(itemEntity);
			return (this);
		}

		if (GetWindow() != SAFEBOX && GetWindow() != MALL)
		{
			if (IsDragonSoul())
			{
				if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					sys_err("CItem::RemoveFromCharacter: pos >= DRAGON_SOUL_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), nullptr);
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (IsExtraItem())
			{
				if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
					sys_err("CItem::RemoveFromCharacter: pos >= EXTRA_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), nullptr);
			}
#endif
#ifdef ENABLE_SWITCHBOT
			else if (m_bWindow == SWITCHBOT)
			{
				if (m_wCell >= SWITCHBOT_SLOT_COUNT)
				{
					sys_err("CItem::RemoveFromCharacter: pos >= SWITCHBOT_SLOT_COUNT");
				}
				else
				{
					pOwner->SetItem(TItemPos(SWITCHBOT, m_wCell), nullptr);
				}
			}
#endif
			else
			{
				TItemPos cell(INVENTORY, m_wCell);

				if (false == cell.IsDefaultInventoryPosition() && false == cell.IsBeltInventoryPosition())
					sys_err("CItem::RemoveFromCharacter: Invalid Item Position");
				else
					pOwner->SetItem(cell, nullptr);
			}
		}

		m_pOwner = nullptr;
		m_wCell = 0;

		SetWindow(RESERVED_WINDOW);
		Save();

		const entt::entity itemEntity = ItemEntityOf(this);
		SyncItemLocation(itemEntity, this);
		SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		return (this);
	}
}

#ifdef __HIGHLIGHT_SYSTEM__
bool CItem::AddToCharacter(LPCHARACTER ch, TItemPos Cell, bool isHighLight)
#else
bool CItem::AddToCharacter(LPCHARACTER ch, TItemPos Cell)
#endif
{
	assert(GetSectree() == NULL);
	assert(m_pOwner == NULL);
	uint16_t pos = Cell.cell;
	uint8_t window_type = Cell.window_type;

	if (INVENTORY == window_type)
	{
#ifdef ENABLE_RUNE_SYSTEM
		if ((IsRune()) && (ch)) {
			int iFindCell = FindEquipCell(ch);
			LPITEM pkItem = ch->GetWear(iFindCell);
			if (pkItem) {
#ifdef TEXTS_IMPROVEMENT
				ch->ChatPacketNew(CHAT_TYPE_INFO, 35, "%s", GetName());
#endif
				M2_DESTROY_ITEM(this);
				return false;
			}
			else {
				this->EquipTo(ch, iFindCell);
				if (ch->GetDesc())
					m_dwLastOwnerPID = ch->GetPlayerID();

				event_cancel(&m_pkDestroyEvent);

				ch->SetItem(TItemPos(EQUIPMENT, iFindCell), this);
				m_pOwner = ch;
				Save();

				const entt::entity itemEntity = ItemEntityOf(this);
				SyncItemOwner(itemEntity, ch->GetPlayerID(), m_dwLastOwnerPID, m_dwOwnershipPID);
				SyncItemLocation(itemEntity, this);
				SyncItemEquipped(itemEntity, true);
				return true;
			}
		}
#endif
#ifdef ENABLE_MOUNT_INVENTORY_FIX_RAZOR93_egyenlore_kikapcsolva

		if (pos >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > pos)
#else
		if (m_wCell >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > m_wCell)
#endif
		{
			sys_err("CItem::AddToCharacter: cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
	else if (DRAGON_SOUL_INVENTORY == window_type)
	{
		if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			sys_err("CItem::AddToCharacter: cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (window_type == EXTRA_INVENTORY)
	{
		if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
			sys_err("CItem::AddToCharacter: EXTRA cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#endif
#ifdef ENABLE_SWITCHBOT
	else if (SWITCHBOT == window_type)
	{
		if (m_wCell >= SWITCHBOT_SLOT_COUNT)
		{
			sys_err("CItem::AddToCharacter:switchbot cell overflow: %s to %s cell %d", m_pProto->szName, ch->GetName(), m_wCell);
			return false;
		}
	}
#endif
	if (ch->GetDesc())
		m_dwLastOwnerPID = ch->GetPlayerID();


#ifdef ENABLE_ACCE_SYSTEM
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE) && (GetSocket(ACCE_ABSORPTION_SOCKET) == 0))
	{
		int32_t lVal = GetValue(ACCE_GRADE_VALUE_FIELD);
		switch (lVal)
		{
		case 2:
		{
			lVal = ACCE_GRADE_2_ABS;
		}
		break;
		case 3:
		{
			lVal = ACCE_GRADE_3_ABS;
		}
		break;
		case 4:
		{
			lVal = number(ACCE_GRADE_4_ABS_MIN, ACCE_GRADE_4_ABS_MAX_COMB);
		}
		break;
		default:
		{
			lVal = ACCE_GRADE_1_ABS;
		}
		break;
		}

		SetSocket(ACCE_ABSORPTION_SOCKET, lVal);
	}
#endif


	event_cancel(&m_pkDestroyEvent);

#ifdef __HIGHLIGHT_SYSTEM__
	ch->SetItem(TItemPos(window_type, pos), this, isHighLight);
#else
	ch->SetItem(TItemPos(window_type, pos), this);
#endif
	m_pOwner = ch;

	Save();

	const entt::entity itemEntity = ItemEntityOf(this);
	SyncItemOwner(itemEntity, ch->GetPlayerID(), m_dwLastOwnerPID, m_dwOwnershipPID);
	SyncItemLocation(itemEntity, this);
	SyncItemEquipped(itemEntity, false);
	return true;
}

LPITEM CItem::RemoveFromGround()
{
	if (GetSectree())
	{
		SetOwnership(nullptr);

		GetSectree()->RemoveEntity(this);

		ViewCleanup();

		Save();

		const entt::entity itemEntity = ItemEntityOf(this);
		SyncItemLocation(itemEntity, this);
		g_registry.remove<ecs::ItemOwner>(itemEntity);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
	}

	return (this);
}

bool CItem::AddToGround(int32_t lMapIndex, const PIXEL_POSITION& pos, bool skipOwnerCheck)
{
	if (0 == lMapIndex)
	{
		sys_err("wrong map index argument: %d", lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		sys_err("sectree already assigned");
		return false;
	}

	if (!skipOwnerCheck && m_pOwner)
	{
		sys_err("owner pointer not null");
		return false;
	}

	LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, pos.x, pos.y);

	if (!tree)
	{
		sys_err("cannot find sectree by %dx%d", pos.x, pos.y);
		return false;
	}

	//tree->Touch();

	SetWindow(GROUND);
	SetXYZ(pos.x, pos.y, pos.z);
	tree->InsertEntity(this);
	UpdateSectree();
	Save();

	const entt::entity itemEntity = ItemEntityOf(this);
	SyncItemLocation(itemEntity, this);
	g_registry.remove<ecs::ItemOwner>(itemEntity);
	g_registry.remove<ecs::ItemEquipped>(itemEntity);
	return true;
}

bool CItem::DistanceValid(LPCHARACTER ch)
{
	if (!GetSectree())
		return false;

	int iDist = DISTANCE_APPROX(GetX() - ch->GetX(), GetY() - ch->GetY());
	if (iDist > 2400)
		return false;

	return true;
}

bool CItem::IsOwnership(LPCHARACTER ch)
{
	if (!m_pkOwnershipEvent)
		return true;

	return m_dwOwnershipPID == ch->GetPlayerID() ? true : false;
}

void CItem::SetOwnershipEvent(LPEVENT pkEvent)
{
	m_pkOwnershipEvent = pkEvent;
}

void CItem::SetOwnership(LPCHARACTER ch, int iSec)
{
	if (!ch)
	{
		if (m_pkOwnershipEvent)
		{
			event_cancel(&m_pkOwnershipEvent);
			m_dwOwnershipPID = 0;

			TPacketGCItemOwnership p;

			p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
			p.dwVID = m_dwVID;
			p.szName[0] = '\0';

			PacketAround(&p, sizeof(p));

			const entt::entity itemEntity = ItemEntityOf(this);
			SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
		}
		return;
	}

	if (m_pkOwnershipEvent)
		return;

	if (iSec <= 10)
		iSec = 30;

	m_dwOwnershipPID = ch->GetPlayerID();

	item_event_info* info = AllocEventInfo<item_event_info>();
	strlcpy(info->szOwnerName, ch->GetName(), sizeof(info->szOwnerName));
	info->item = this;

	SetOwnershipEvent(event_create(ownership_event, info, PASSES_PER_SEC(iSec)));

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = m_dwVID;
	strlcpy(p.szName, ch->GetName(), sizeof(p.szName));

	PacketAround(&p, sizeof(p));

	const entt::entity itemEntity = ItemEntityOf(this);
	SyncItemOwner(itemEntity, m_pOwner ? m_pOwner->GetPlayerID() : 0, m_dwLastOwnerPID, m_dwOwnershipPID);
}
