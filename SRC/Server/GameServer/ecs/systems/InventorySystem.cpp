#include "../../stdafx.h"

#include "InventorySystem.hpp"

#include "../../config.h"
#include "../../char.h"
#include "../../desc.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../MountInventory.h"
#include "../../DragonSoul.h"
#include "../../packet.h"
#include "../../sectree_manager.h"
#include "../../../common/VnumHelper.h"
#include "../AIHelpers.hpp"
#include "../SpatialHelpers.hpp"
#include "../EventDispatcher.hpp"
#include "../ItemRegistry.hpp"
#include "../events.hpp"
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

void SyncCharacterEquipmentSlot(LPCHARACTER ch, uint8_t wearCell, LPITEM item)
{
	const entt::entity chEntity = AIHelpers::EcsOf(ch);
	if (chEntity == entt::null || !g_registry.valid(chEntity))
		return;

	auto& slots = g_registry.get_or_emplace<ecs::EquipmentSlots>(chEntity);
	if (wearCell < slots.items.size())
		slots.items[wearCell] = item;
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
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 35, "%s", GetName());
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

		const entt::entity itemEntity = ItemEntityOf(this);
		if (itemEntity != entt::null && g_registry.valid(itemEntity))
		{
			g_registry.remove<ecs::SectorPlacement>(itemEntity);
			g_registry.remove<ecs::ViewActiveTag>(itemEntity);
		}

		ViewCleanup();

		Save();

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

	LPSECTREE tree = ecs::SectorAt(lMapIndex, pos.x, pos.y);

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
	ecs::SyncSectorPlacement(g_registry, itemEntity, lMapIndex, GetX(), GetY());
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.emplace_or_replace<ecs::ViewActiveTag>(itemEntity);
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

bool CItem::CanUsedBy(LPCHARACTER ch)
{
	// Anti flag check
	switch (ch->GetJob())
	{
	case JOB_WARRIOR:
		if (GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
			return false;
		break;

	case JOB_ASSASSIN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
			return false;
		break;

	case JOB_SHAMAN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
			return false;
		break;

	case JOB_SURA:
		if (GetAntiFlag() & ITEM_ANTIFLAG_SURA)
			return false;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		if (GetAntiFlag() & ITEM_ANTIFLAG_WOLFMAN)
			return false;
		break;
#endif
	}

	return true;
}

int CItem::FindEquipCell(LPCHARACTER ch, int iCandidateCell)
{
	if ((0 == GetWearFlag() || ITEM_TOTEM == GetType()) && ITEM_COSTUME != GetType() && ITEM_DS != GetType() && ITEM_SPECIAL_DS != GetType() && ITEM_RING != GetType() && ITEM_BELT != GetType())
		return -1;

	if (GetType() == ITEM_DS || GetType() == ITEM_SPECIAL_DS)
	{
		if (iCandidateCell < 0)
		{
			return WEAR_MAX_NUM + GetSubType();
		}
		else
		{
			for (int i = 0; i < DRAGON_SOUL_DECK_MAX_NUM; i++)
			{
				if (WEAR_MAX_NUM + i * DS_SLOT_MAX + GetSubType() == iCandidateCell)
				{
					return iCandidateCell;
				}
			}
			return -1;
		}
	}
	else if (GetType() == ITEM_COSTUME)
	{
		if (GetSubType() == COSTUME_BODY)
			return WEAR_COSTUME_BODY;
		else if (GetSubType() == COSTUME_HAIR)
			return WEAR_COSTUME_HAIR;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_MOUNT)
			return WEAR_COSTUME_MOUNT;
#endif
#ifdef ENABLE_ACCE_SYSTEM
		else if (GetSubType() == COSTUME_ACCE)
			return WEAR_COSTUME_ACCE_SLOT;
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_WEAPON)
			return WEAR_COSTUME_WEAPON;
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (GetSubType() == COSTUME_STOLE)
			return WEAR_COSTUME_ACCE;
#endif
#ifdef ENABLE_COSTUME_PET
		else if (GetSubType() == COSTUME_PET_SKIN)
			return WEAR_COSTUME_PET_SKIN;
#endif
#ifdef ENABLE_COSTUME_MOUNT
		else if (GetSubType() == COSTUME_MOUNT_SKIN)
			return WEAR_COSTUME_MOUNT_SKIN;
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (GetSubType() == COSTUME_EFFECT_BODY)
			return WEAR_COSTUME_EFFECT_BODY;
		else if (GetSubType() == COSTUME_EFFECT_WEAPON)
			return WEAR_COSTUME_EFFECT_WEAPON;
#endif
#ifdef ENABLE_RUNE_SYSTEM
		else if (GetSubType() == RUNE_SLOT1)
			return WEAR_RUNE1;
		else if (GetSubType() == RUNE_SLOT2)
			return WEAR_RUNE2;
		else if (GetSubType() == RUNE_SLOT3)
			return WEAR_RUNE3;
		else if (GetSubType() == RUNE_SLOT4)
			return WEAR_RUNE4;
		else if (GetSubType() == RUNE_SLOT5)
			return WEAR_RUNE5;
		else if (GetSubType() == RUNE_SLOT6)
			return WEAR_RUNE6;
		else if (GetSubType() == RUNE_SLOT7)
			return WEAR_RUNE7;
#endif
	}
#if !defined(ENABLE_MOUNT_COSTUME_SYSTEM) && !defined(ENABLE_ACCE_SYSTEM)
	else if (GetType() == ITEM_RING)
	{
		if (ch->GetWear(WEAR_RING1))
			return WEAR_RING2;
		else
			return WEAR_RING1;
	}
#endif
	else if (GetType() == ITEM_BELT)
		return WEAR_BELT;
	else if (GetWearFlag() & WEARABLE_BODY)
		return WEAR_BODY;
	else if (GetWearFlag() & WEARABLE_HEAD)
		return WEAR_HEAD;
	else if (GetWearFlag() & WEARABLE_FOOTS)
		return WEAR_FOOTS;
	else if (GetWearFlag() & WEARABLE_WRIST)
		return WEAR_WRIST;
	else if (GetWearFlag() & WEARABLE_WEAPON)
		return WEAR_WEAPON;
	else if (GetWearFlag() & WEARABLE_SHIELD)
		return WEAR_SHIELD;
	else if (GetWearFlag() & WEARABLE_NECK)
		return WEAR_NECK;
	else if (GetWearFlag() & WEARABLE_EAR)
		return WEAR_EAR;
	else if (GetWearFlag() & WEARABLE_ARROW)
		return WEAR_ARROW;
	else if (GetWearFlag() & WEARABLE_UNIQUE)
	{
#ifdef ENABLE_NEW_UNIQUE_WEAR_LIMITED
		if (GetSubType() == UNIQUE_PVM || GetSubType() == UNIQUE_PVP || GetSubType() == UNIQUE_NONE)
		{
			const int iSlot1 = WEAR_UNIQUE1;
			const int iSlot2 = WEAR_UNIQUE2;

			if (iCandidateCell == iSlot1 || iCandidateCell == iSlot2)
				return iCandidateCell;

			if (!ch->GetWear(iSlot1))
				return iSlot1;

			if (!ch->GetWear(iSlot2))
				return iSlot2;

			return -1;
		}
		else
		{
			return -1;
		}
#else
		if (ch->GetWear(WEAR_UNIQUE1))
			return WEAR_UNIQUE2;
		else
			return WEAR_UNIQUE1;
#endif
	}
#ifdef ENABLE_PENDANT
	else if (GetSubType() == ARMOR_PENDANT || GetWearFlag() & WEARABLE_PENDANT)
		return WEAR_PENDANT;
#endif

	else if (GetWearFlag() & WEARABLE_ABILITY)
	{
		if (!ch->GetWear(WEAR_ABILITY1))
		{
			return WEAR_ABILITY1;
		}
		else if (!ch->GetWear(WEAR_ABILITY2))
		{
			return WEAR_ABILITY2;
		}
		else if (!ch->GetWear(WEAR_ABILITY3))
		{
			return WEAR_ABILITY3;
		}
		else if (!ch->GetWear(WEAR_ABILITY4))
		{
			return WEAR_ABILITY4;
		}
		else if (!ch->GetWear(WEAR_ABILITY5))
		{
			return WEAR_ABILITY5;
		}
		else if (!ch->GetWear(WEAR_ABILITY6))
		{
			return WEAR_ABILITY6;
		}
		else if (!ch->GetWear(WEAR_ABILITY7))
		{
			return WEAR_ABILITY7;
		}
#ifndef ENABLE_STOLE_REAL
		else if (!ch->GetWear(WEAR_ABILITY8))
		{
			return WEAR_ABILITY8;
		}
#endif
		else
		{
			return -1;
		}
	}
	return -1;
}

bool CItem::IsEquipable()
{
	switch (GetType())
	{
	case ITEM_COSTUME:
	case ITEM_ARMOR:
	case ITEM_WEAPON:
	case ITEM_ROD:
	case ITEM_PICK:
	case ITEM_UNIQUE:
	case ITEM_DS:
	case ITEM_SPECIAL_DS:
	case ITEM_RING:
		return true;

	case ITEM_BELT:
		return (GetValue(5) == 1);

	default:
		return false;
	}
}


#define ENABLE_IMMUNE_FIX
// return false on error state
bool CItem::EquipTo(LPCHARACTER ch, uint8_t bWearCell)
{
	if (!ch)
	{
		sys_err("EquipTo: nil character");
		return false;
	}

	if (IsDragonSoul())
	{
		if (bWearCell < WEAR_MAX_NUM || bWearCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
		{
			sys_err("EquipTo: invalid dragon soul cell (this: #%d %s wearflag: %d cell: %d)", GetOriginalVnum(), GetName(), GetSubType(), bWearCell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (bWearCell >= WEAR_MAX_NUM)
		{
			sys_err("EquipTo: invalid wear cell (this: #%d %s wearflag: %d cell: %d)", GetOriginalVnum(), GetName(), GetWearFlag(), bWearCell);
			return false;
		}
	}

	if (ch->GetWear(bWearCell))
	{
		sys_err("EquipTo: item already exist (this: #%d %s cell: %d %s)", GetOriginalVnum(), GetName(), bWearCell, ch->GetWear(bWearCell)->GetName());
		return false;
	}

	if (GetOwner())
		RemoveFromCharacter();

	ch->SetWear(bWearCell, this);

	m_pOwner = ch;
	m_bEquipped = true;
	m_wCell = INVENTORY_MAX_NUM + bWearCell;

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if (m_pOwner->GetWear(i))
		{
			SET_BIT(dwImmuneFlag, m_pOwner->GetWear(i)->m_pProto->dwImmuneFlag);
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	if (IsDragonSoul())
	{
		DSManager::instance().ActivateDragonSoul(this);
	}
	else
	{
#ifdef ENABLE_RUNE_SYSTEM
		if (!IsRune())
			ModifyPoints(true);
		else if (GetSocket(1) == 1)
			ModifyPoints(true);
#else
		ModifyPoints(true);
#endif
		StartUniqueExpireEvent();
		if (-1 != GetProto()->cLimitTimerBasedOnWearIndex)
			StartTimerBasedOnWearExpireEvent();

		// ACCESSORY_REFINE
		StartAccessorySocketExpireEvent();
		// END_OF_ACCESSORY_REFINE
	}

	ch->BuffOnAttr_AddBuffsFromItem(this);

	m_pOwner->ComputeBattlePoints();

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (IsMountItem())
		ch->MountSummon(this);
#endif
	m_pOwner->UpdatePacket();
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	if (bWearCell == WEAR_BELT)
		ch->UpdateItemOnTitleName(true);
#endif

#ifdef ENABLE_COSTUME_PET
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_PET_SKIN)) {
		m_pOwner->UpdatePetSkin();
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_MOUNT_SKIN)) {
		m_pOwner->UpdateMountSkin();
	}
#endif

	const entt::entity itemEntity = ItemEntityOf(this);
	SyncItemEquipped(itemEntity, true);
	SyncItemLocation(itemEntity, this);
	SyncItemOwner(itemEntity, ch->GetPlayerID(), m_dwLastOwnerPID, m_dwOwnershipPID);
	SyncCharacterEquipmentSlot(ch, bWearCell, this);
	g_dispatcher.trigger(ecs::EvItemEquipped { AIHelpers::EcsOf(ch), itemEntity });

	Save();
	return (true);
}

bool CItem::Unequip()
{
	if (!m_pOwner || GetCell() < INVENTORY_MAX_NUM)
	{
		sys_err("%s %u m_pOwner %p, GetCell %d",
			GetName(), GetID(), get_pointer(m_pOwner), GetCell());
		return false;
	}

	if (this != m_pOwner->GetWear(GetCell() - INVENTORY_MAX_NUM))
	{
		sys_err("m_pOwner->GetWear() != this");
		return false;
	}

	LPCHARACTER owner = m_pOwner;
	const uint8_t wearCell = static_cast<uint8_t>(GetCell() - INVENTORY_MAX_NUM);
	const entt::entity itemEntity = ItemEntityOf(this);
	const entt::entity charEntity = AIHelpers::EcsOf(owner);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (IsMountItem())
		m_pOwner->MountUnsummon(this);
#endif

	if (IsRideItem())
		ClearMountAttributeAndAffect();

	if (IsDragonSoul())
	{
		DSManager::instance().DeactivateDragonSoul(this);
	}
#ifdef ENABLE_RUNE_SYSTEM
	else if (IsRune()) {
		if (GetSocket(1) == 1)
			ModifyPoints(false);
	}
#endif
	else
	{
		ModifyPoints(false);
	}

	StopUniqueExpireEvent();

	if (-1 != GetProto()->cLimitTimerBasedOnWearIndex)
		StopTimerBasedOnWearExpireEvent();

	StopAccessorySocketExpireEvent();

	m_pOwner->BuffOnAttr_RemoveBuffsFromItem(this);

	m_pOwner->SetWear(GetCell() - INVENTORY_MAX_NUM, nullptr);

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		if (m_pOwner->GetWear(i))
		{
			SET_BIT(dwImmuneFlag, m_pOwner->GetWear(i)->m_pProto->dwImmuneFlag);
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	m_pOwner->ComputeBattlePoints();

	m_pOwner->UpdatePacket();
#ifdef ENABLE_COSTUME_PET
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_PET_SKIN)) {
		m_pOwner->UpdatePetSkin();
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_MOUNT_SKIN)) {
		m_pOwner->UpdateMountSkin();
	}
#endif
	m_pOwner = nullptr;
	m_wCell = 0;
	m_bEquipped = false;

	SyncItemEquipped(itemEntity, false);
	SyncItemLocation(itemEntity, this);
	SyncCharacterEquipmentSlot(owner, wearCell, nullptr);
	g_dispatcher.trigger(ecs::EvItemUnequipped { charEntity, itemEntity });
	return true;
}

void CItem::ModifyPoints(bool bAdd)
{
#ifdef ENABLE_BUG_FIXES
	if (!m_pOwner) {
		return;
	}
#endif

	int accessoryGrade;

	if (false == IsAccessoryForSocket())
	{
		if (m_pProto->bType == ITEM_WEAPON || m_pProto->bType == ITEM_ARMOR)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				uint32_t dwVnum;

				if ((dwVnum = GetSocket(i)) <= 2)
					continue;

				TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

				if (!p)
				{
					sys_err("cannot find table by vnum %u", dwVnum);
					continue;
				}

				if (ITEM_METIN == p->bType)
				{
					for (auto& aApplie : p->aApplies)
					{
						if (aApplie.bType == APPLY_NONE)
							continue;

						if (aApplie.bType == APPLY_SKILL)
							m_pOwner->ApplyPoint(aApplie.bType, bAdd ? aApplie.lValue : aApplie.lValue ^ 0x00800000);
						else
							m_pOwner->ApplyPoint(aApplie.bType, bAdd ? aApplie.lValue : -aApplie.lValue);
					}
				}
			}
		}

		accessoryGrade = 0;
	}
	else
	{
		accessoryGrade = MIN(GetAccessorySocketGrade(), ITEM_ACCESSORY_SOCKET_MAX_NUM);
	}


#ifdef ENABLE_ACCE_SYSTEM
	if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE) && (GetSocket(ACCE_ABSORBED_SOCKET)))
	{
		TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(GetSocket(ACCE_ABSORBED_SOCKET));
		if (pkItemAbsorbed)
		{
			/* 			if ((pkItemAbsorbed->bType == ITEM_ARMOR) && (pkItemAbsorbed->bSubType == ARMOR_BODY))
						{
							int32_t lDefGrade = pkItemAbsorbed->alValues[1] + int32_t(pkItemAbsorbed->alValues[5] * 2);
							double dValue = lDefGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefGrade = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[1] > 0 && (lDefGrade <= 0)) || (pkItemAbsorbed->alValues[5] > 0 && (lDefGrade < 1)))
								lDefGrade += 1;
							else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[5] > 0))
								lDefGrade += 1;

							m_pOwner->ApplyPoint(APPLY_DEF_GRADE_BONUS, bAdd ? lDefGrade : -lDefGrade);

							int32_t lDefMagicBonus = pkItemAbsorbed->alValues[0];
							dValue = lDefMagicBonus * GetSocket(ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefMagicBonus = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[0] > 0) && (lDefMagicBonus < 1))
								lDefMagicBonus += 1;
							else if (pkItemAbsorbed->alValues[0] > 0)
								lDefMagicBonus += 1;

							m_pOwner->ApplyPoint(APPLY_MAGIC_DEF_GRADE, bAdd ? lDefMagicBonus : -lDefMagicBonus);
						} */
			/* else  */if (pkItemAbsorbed->bType == ITEM_WEAPON)
			{
				int32_t lAttGrade = pkItemAbsorbed->alValues[4] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[3] > pkItemAbsorbed->alValues[4])
					lAttGrade = pkItemAbsorbed->alValues[3] + pkItemAbsorbed->alValues[5];

				double dValue = lAttGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[3] > 0) && (lAttGrade < 1)) || ((pkItemAbsorbed->alValues[4] > 0) && (lAttGrade < 1)))
					lAttGrade += 1;
				else if ((pkItemAbsorbed->alValues[3] > 0) || (pkItemAbsorbed->alValues[4] > 0))
					lAttGrade += 1;

				m_pOwner->ApplyPoint(APPLY_ATT_GRADE_BONUS, bAdd ? lAttGrade : -lAttGrade);

				int32_t lAttMagicGrade = pkItemAbsorbed->alValues[2] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[1] > pkItemAbsorbed->alValues[2])
					lAttMagicGrade = pkItemAbsorbed->alValues[1] + pkItemAbsorbed->alValues[5];

				dValue = lAttMagicGrade * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttMagicGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[1] > 0) && (lAttMagicGrade < 1)) || ((pkItemAbsorbed->alValues[2] > 0) && (lAttMagicGrade < 1)))
					lAttMagicGrade += 1;
				else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[2] > 0))
					lAttMagicGrade += 1;

				m_pOwner->ApplyPoint(APPLY_MAGIC_ATT_GRADE, bAdd ? lAttMagicGrade : -lAttMagicGrade);
			}
		}
	}
#endif


	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
#ifdef ENABLE_ACCE_SYSTEM
		if ((m_pProto->aApplies[i].bType == APPLY_NONE) && (GetType() != ITEM_COSTUME) && (GetSubType() != COSTUME_ACCE))
#else
		if (m_pProto->aApplies[i].bType == APPLY_NONE)
#endif
			continue;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (IsMountItem())
			continue;
#endif

		int32_t value = m_pProto->aApplies[i].lValue;
#ifdef ENABLE_ACCE_SYSTEM
		if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE))
		{
			TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(GetSocket(ACCE_ABSORBED_SOCKET));
			if (pkItemAbsorbed)
			{
				if (pkItemAbsorbed->aApplies[i].bType == APPLY_NONE)
					continue;

				value = pkItemAbsorbed->aApplies[i].lValue;
				if (value < 0)
					continue;

				double dValue = value * GetSocket(ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				value = (int32_t)dValue;
				if ((pkItemAbsorbed->aApplies[i].lValue > 0) && (value <= 0))
					value += 1;
			}
			else
				continue;
		}
#endif
		if (m_pProto->aApplies[i].bType == APPLY_SKILL)
		{
			m_pOwner->ApplyPoint(m_pProto->aApplies[i].bType, bAdd ? value : value ^ 0x00800000);
		}
		else
		{
			if (0 != accessoryGrade)
				value += MAX(accessoryGrade, value * aiAccessorySocketEffectivePct[accessoryGrade] / 100);

			m_pOwner->ApplyPoint(m_pProto->aApplies[i].bType, bAdd ? value : -value);
		}
	}

#ifdef ENABLE_ITEM_EXTRA_PROTO
	if (HasExtraProto())
	{
#ifdef ENABLE_NEW_EXTRA_BONUS
		for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; i++)
		{
			auto type = m_ExtraProto->ExtraBonus[i].bType;
			if (type != APPLY_NONE) {
				auto value = m_ExtraProto->ExtraBonus[i].lValue;
				m_pOwner->ApplyPoint(m_ExtraProto->ExtraBonus[i].bType, bAdd ? value : -value);
			}
		}
#endif
	}
#endif

	if (true == CItemVnumHelper::IsRamadanMoonRing(GetVnum()) || true == CItemVnumHelper::IsHalloweenCandy(GetVnum())
		|| true == CItemVnumHelper::IsHappinessRing(GetVnum()) || true == CItemVnumHelper::IsLovePendant(GetVnum()))
	{
		// Do not anything.
	}
	else
	{
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (GetAttributeType(i))
			{
				const TPlayerItemAttribute& ia = GetAttribute(i);
				int32_t sValue = ia.sValue;
#ifdef ENABLE_ACCE_SYSTEM
				if ((GetType() == ITEM_COSTUME) && (GetSubType() == COSTUME_ACCE)) {
					double dValue = sValue * GetSocket(ACCE_ABSORPTION_SOCKET);
					dValue = dValue / 100;
					dValue = dValue + .5;
					sValue = (int32_t)dValue;
					if ((ia.sValue > 0) && (sValue <= 0))
						sValue += 1;
				}
#endif

#ifdef ATTR_LOCK
				if (GetLockedAttr() == i) {
					continue;
				}
#endif

				if (ia.bType == APPLY_SKILL)
					m_pOwner->ApplyPoint(ia.bType, bAdd ? sValue : sValue ^ 0x00800000);
				else
					m_pOwner->ApplyPoint(ia.bType, bAdd ? sValue : -sValue);
			}
		}
	}

	switch (m_pProto->bType)
	{
	case ITEM_PICK:
	case ITEM_ROD:
	{
		if (bAdd)
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_WEAPON:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if ((GetSubType() == WEAPON_SWORD) || (GetSubType() == WEAPON_DAGGER) || (GetSubType() == WEAPON_BOW) || (GetSubType() == WEAPON_TWO_HANDED) || (GetSubType() == WEAPON_BELL) || (GetSubType() == WEAPON_FAN)) {
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_WEAPON);
			if (item) {
				uint32_t toSetValueEffect;
				switch (this->GetSubType()) {
				case WEAPON_SWORD:
					toSetValueEffect = item->GetValue(0);
					break;
				case WEAPON_DAGGER:
					toSetValueEffect = item->GetValue(2);
					break;
				case WEAPON_BOW:
					toSetValueEffect = item->GetValue(3);
					break;
				case WEAPON_TWO_HANDED:
					toSetValueEffect = item->GetValue(1);
					break;
				case WEAPON_BELL:
					toSetValueEffect = item->GetValue(4);
					break;
				case WEAPON_FAN:
					toSetValueEffect = item->GetValue(5);
					break;
				default:
					toSetValueEffect = 0;
					break;
				}

				if (toSetValueEffect > 0) {
					uint32_t dwWeaponVnum = GetVnum();
					if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
						((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
						(dwWeaponVnum == 1199) ||
						(dwWeaponVnum == 1209) ||
						(dwWeaponVnum == 1219) ||
						(dwWeaponVnum == 1229) ||
						(dwWeaponVnum == 40099) ||
						((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
						)
						toSetValueEffect += 500;
				}

				if (!bAdd)
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_WEAPON, toSetValueEffect);
			}
		}
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		if (nullptr != m_pOwner->GetWear(WEAR_COSTUME_WEAPON))
			break;
#endif

		if (bAdd)
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				m_pOwner->SetPart(PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_ARMOR:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if (GetSubType() == ARMOR_BODY) {
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_BODY);
			if (item) {
				uint32_t toSetValueEffect;
				toSetValueEffect = item->GetValue(0);
				if ((!bAdd) && (!m_pOwner->GetWear(WEAR_COSTUME_BODY)))
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_BODY, toSetValueEffect);
			}
		}
#endif

		if (nullptr != m_pOwner->GetWear(WEAR_COSTUME_BODY))
			break;

		if (GetSubType() == ARMOR_BODY || GetSubType() == ARMOR_HEAD || GetSubType() == ARMOR_FOOTS || GetSubType() == ARMOR_SHIELD)
		{
			if (bAdd)
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					m_pOwner->SetPart(PART_MAIN, GetVnum());
			}
			else
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					m_pOwner->SetPart(PART_MAIN, m_pOwner->GetOriginalPart(PART_MAIN));
			}
		}
	}
	break;

	case ITEM_COSTUME:
	{
		uint32_t toSetValue = this->GetVnum();
		EParts toSetPart = PART_MAX_NUM;

		if (GetSubType() == COSTUME_BODY)
		{
#ifdef ENABLE_COSTUME_EFFECT
			CItem* item = m_pOwner->GetWear(WEAR_COSTUME_EFFECT_BODY);
			if (item) {
				uint32_t toSetValueEffect;
				toSetValueEffect = item->GetValue(0);
				if ((!bAdd) && (!m_pOwner->GetWear(WEAR_BODY)))
					toSetValueEffect = 0;

				m_pOwner->SetPart(PART_EFFECT_BODY, toSetValueEffect);
			}
#endif
			toSetPart = PART_MAIN;

			if (false == bAdd)
			{
				const CItem* pArmor = m_pOwner->GetWear(WEAR_BODY);
				toSetValue = (nullptr != pArmor) ? pArmor->GetVnum() : m_pOwner->GetOriginalPart(PART_MAIN);
			}
		}
#ifdef ENABLE_RUNE_SYSTEM
		else if (GetSubType() == RUNE_SLOT7)
		{
			toSetPart = PART_RUNE;
			toSetValue = (true == bAdd) ? m_pOwner->GetRuneEffect() : 0;
		}
#endif
		else if (GetSubType() == COSTUME_HAIR)
		{
			toSetPart = PART_HAIR;
			toSetValue = (true == bAdd) ? this->GetValue(3) : 0;
		}

#ifdef ENABLE_ACCE_SYSTEM
		else if (GetSubType() == COSTUME_ACCE)
		{
			toSetValue -= 85000;
			if (GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
				toSetValue += 1000;

			toSetValue = (bAdd == true) ? toSetValue : 0;

#ifdef ENABLE_STOLE_COSTUME
			const CItem* pAcce = m_pOwner->GetWear(WEAR_COSTUME_ACCE);
			if (pAcce) {
				toSetValue = pAcce->GetVnum();
				toSetValue -= 85000;
				toSetValue += 1000;
			}
#endif

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (GetSubType() == COSTUME_STOLE)
		{
			toSetValue -= 85000;
			if (!bAdd) {
				CItem* pAcce = m_pOwner->GetWear(WEAR_COSTUME_ACCE_SLOT);
				if (!pAcce)
					toSetValue = 0;
				else {
					toSetValue = pAcce->GetVnum();
					toSetValue -= 85000;
					if (pAcce->GetSocket(ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
						toSetValue += 1000;
				}
			}
			else
				toSetValue += 1000;

			toSetPart = PART_ACCE;
		}
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (GetSubType() == COSTUME_EFFECT_BODY)
		{
			if (bAdd) {
				CItem* item = m_pOwner->GetWear(WEAR_BODY);
				toSetValue = item != nullptr ? this->GetValue(0) : 0;
				if (toSetValue == 0) {
					item = m_pOwner->GetWear(WEAR_COSTUME_BODY);
					toSetValue = item != nullptr ? this->GetValue(0) : 0;
				}
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_BODY;
		}
		else if (GetSubType() == COSTUME_EFFECT_WEAPON)
		{
			if (bAdd) {
				CItem* item = m_pOwner->GetWear(WEAR_WEAPON);
				if (item) {
					switch (item->GetSubType()) {
					case WEAPON_SWORD:
						toSetValue = this->GetValue(0);
						break;
					case WEAPON_DAGGER:
						toSetValue = this->GetValue(2);
						break;
					case WEAPON_BOW:
						toSetValue = this->GetValue(3);
						break;
					case WEAPON_TWO_HANDED:
						toSetValue = this->GetValue(1);
						break;
					case WEAPON_BELL:
						toSetValue = this->GetValue(4);
						break;
					case WEAPON_FAN:
						toSetValue = this->GetValue(5);
						break;
					default:
						toSetValue = 0;
						break;
					}

					if (toSetValue > 0) {
						uint32_t dwWeaponVnum = item->GetVnum();
						if (((dwWeaponVnum >= 1180) && (dwWeaponVnum <= 1189)) ||
							((dwWeaponVnum >= 1090) && (dwWeaponVnum <= 1099)) ||
							(dwWeaponVnum == 1199) ||
							(dwWeaponVnum == 1209) ||
							(dwWeaponVnum == 1219) ||
							(dwWeaponVnum == 1229) ||
							(dwWeaponVnum == 40099) ||
							((dwWeaponVnum >= 7190) && (dwWeaponVnum <= 7199))
							)
							toSetValue += 500;
					}
				}
				else
					toSetValue = 0;
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_WEAPON;
		}
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_MOUNT)
		{
			// not need to do a thing in here
		}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (GetSubType() == COSTUME_WEAPON)
		{
			toSetPart = PART_WEAPON;
			if (false == bAdd)
			{
				const CItem* pWeapon = m_pOwner->GetWear(WEAR_WEAPON);
				if (pWeapon != nullptr) {
					toSetValue = (nullptr != pWeapon) ? pWeapon->GetVnum() : m_pOwner->GetOriginalPart(PART_WEAPON);
				}
				else {
					toSetValue = 0;
				}
			}
		}
#endif

		if (PART_MAX_NUM != toSetPart)
		{
			m_pOwner->SetPart((uint8_t)toSetPart, toSetValue);
			m_pOwner->UpdatePacket();

		}
	}
	break;
	case ITEM_UNIQUE:
	{
		if (0 != GetSIGVnum())
		{
			const CSpecialItemGroup* pItemGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(GetSIGVnum());
			if (nullptr == pItemGroup)
				break;
			uint32_t dwAttrVnum = pItemGroup->GetAttrVnum(GetVnum());
			const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(dwAttrVnum);
			if (nullptr == pAttrGroup)
				break;
			for (auto it = pAttrGroup->m_vecAttrs.begin(); it != pAttrGroup->m_vecAttrs.end(); ++it)
			{
				m_pOwner->ApplyPoint(it->apply_type, bAdd ? it->apply_value : it->apply_value); // -it->apply_value
			}
		}
	}
	break;
	}
}

