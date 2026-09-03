#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"

#include "InventorySystem.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"

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
#include "../EntityFactory.hpp"
#include "../EntityInvariants.hpp"
#include "../SpatialHelpers.hpp"
#include "../services/SpatialService.hpp"
#include "../EventDispatcher.hpp"
#include "../events.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/visibility_components.hpp"
#include <Core/Logging.hpp>

namespace
{

ecs::QuickSlots* GetQuickSlots(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return &g_registry.get_or_emplace<ecs::QuickSlots>(e);
}

void SyncItemLocation(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	g_registry.emplace_or_replace<ecs::ItemLocation>(e,
		static_cast<uint8_t>(ItemSystem::GetItemWindow(e)),
		static_cast<uint16_t>(ItemSystem::GetItemCell(e)));
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

	uint8_t slot = 0;
	if (equipped) {
		const uint16_t cell = ItemSystem::GetItemCell(e);
		if (cell >= INVENTORY_MAX_NUM)
			slot = static_cast<uint8_t>(cell - INVENTORY_MAX_NUM);
	}

	g_registry.emplace_or_replace<ecs::ItemEquipped>(e, equipped, slot);
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

	InventorySystem::SyncQuickslot(GetEntityHandle(), bType, bOldPos, bNewPos);
}

bool CHARACTER::GetQuickslot(uint8_t pos, TQuickslot** ppSlot)
{
	if (pos >= QUICKSLOT_MAX_NUM)
		return false;

	*ppSlot = &m_quickslot[pos];

	TQuickslot ecsSlot {};
	if (InventorySystem::GetQuickslot(GetEntityHandle(), pos, ecsSlot))
		m_quickslot[pos] = ecsSlot;
	else
		InventorySystem::SetQuickslot(GetEntityHandle(), pos, m_quickslot[pos]);

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
	InventorySystem::SetQuickslot(GetEntityHandle(), pos, rSlot);

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
	InventorySystem::DelQuickslot(GetEntityHandle(), pos);

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
	InventorySystem::SwapQuickslot(GetEntityHandle(), a, b);

	pack_quickslot_swap.header = HEADER_GC_QUICKSLOT_SWAP;
	pack_quickslot_swap.pos = a;
	pack_quickslot_swap.pos_to = b;

	if (GetDesc())
		GetDesc()->Packet(&pack_quickslot_swap, sizeof(pack_quickslot_swap));

	return true;
}

void CHARACTER::ChainQuickslotItem(entt::entity item, uint8_t bType, uint8_t bOldPos)
{
	if (ItemSystem::IsDragonSoulItem(item))
		return;

	for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i)
	{
		if (m_quickslot[i].type == bType && m_quickslot[i].pos == bOldPos)
		{
			TQuickslot slot;
			slot.type = bType;
			slot.pos = ItemSystem::GetItemCell(item);

			SetQuickslot(i, slot);
			break;
		}
	}
}

LPITEM CItem::RemoveFromCharacter()
{
	if (!m_pOwner)
	{
		LOG_ERROR("Item::RemoveFromCharacter owner null");
		return (this);
	}

	LPCHARACTER pOwner = m_pOwner;

	if (m_bEquipped)
	{
		Unequip();
		SetWindow(RESERVED_WINDOW);
		Save();

		const entt::entity itemEntity = GetEntityHandle();
		SyncItemLocation(itemEntity);
		SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		return (this);
	}
	else
	{
		if (GetWindow() == MOUNT_INVENTORY)
		{
			if (CMountInventory* mi = pOwner->GetMountInventory())
				mi->RemoveByItem(GetEntityHandle());

			m_pOwner = nullptr;
			m_wCell = 0;
			SetWindow(RESERVED_WINDOW);
			Save();

			const entt::entity itemEntity = GetEntityHandle();
			SyncItemLocation(itemEntity);
			SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
			g_registry.remove<ecs::ItemEquipped>(itemEntity);
			return (this);
		}

		if (GetWindow() != SAFEBOX && GetWindow() != MALL)
		{
			if (IsDragonSoul())
			{
				if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					LOG_ERROR("CItem::RemoveFromCharacter: pos >= DRAGON_SOUL_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), entt::null);
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (IsExtraItem())
			{
				if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
					LOG_ERROR("CItem::RemoveFromCharacter: pos >= EXTRA_INVENTORY_MAX_NUM");
				else
					pOwner->SetItem(TItemPos(m_bWindow, m_wCell), entt::null);
			}
#endif
#ifdef ENABLE_SWITCHBOT
			else if (m_bWindow == SWITCHBOT)
			{
				if (m_wCell >= SWITCHBOT_SLOT_COUNT)
				{
					LOG_ERROR("CItem::RemoveFromCharacter: pos >= SWITCHBOT_SLOT_COUNT");
				}
				else
				{
					pOwner->SetItem(TItemPos(SWITCHBOT, m_wCell), entt::null);
				}
			}
#endif
			else
			{
				TItemPos cell(INVENTORY, m_wCell);

				if (false == cell.IsDefaultInventoryPosition() && false == cell.IsBeltInventoryPosition())
					LOG_ERROR("CItem::RemoveFromCharacter: Invalid Item Position");
				else
					pOwner->SetItem(cell, entt::null);
			}
		}

		m_pOwner = nullptr;
		m_wCell = 0;

		SetWindow(RESERVED_WINDOW);
		Save();

		const entt::entity itemEntity = GetEntityHandle();
		SyncItemLocation(itemEntity);
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
	const entt::entity character = ch
		? ch->GetEntityHandle()
		: entt::null;
	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity == entt::null)
	{
		LOG_ERROR("CItem::AddToCharacter: item {} has no ECS entity", GetID());
		return false;
	}
	uint16_t pos = Cell.cell;
	uint8_t window_type = Cell.window_type;

	if (INVENTORY == window_type)
	{
#ifdef ENABLE_RUNE_SYSTEM
		if ((IsRune()) && (ch)) {
			int iFindCell = FindEquipCell(ch);
			const entt::entity equipped = ItemSystem::GetWearItem(character, iFindCell);
			if (ItemSystem::IsValidItem(equipped)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 35, "%s", GetName());
#endif
				ItemSystem::DestroyItemEntityEcs(
					GetEntityHandle(),
					"INVENTORY_RUNE_ADD_FAILED");
				return false;
			}
			else {
				this->EquipTo(ch, iFindCell);
				if (ecs::PlayerRuntime::GetDesc(character))
					m_dwLastOwnerPID = ecs::PlayerRuntime::GetPlayerID(character);

				event_cancel(&m_pkDestroyEvent);

				ch->SetItem(TItemPos(EQUIPMENT, iFindCell), GetEntityHandle());
				m_pOwner = ch;
				Save();

				const entt::entity itemEntity = GetEntityHandle();
				SyncItemOwner(itemEntity, ecs::PlayerRuntime::GetPlayerID(character), m_dwLastOwnerPID, m_dwOwnershipPID);
				SyncItemLocation(itemEntity);
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
			LOG_ERROR("CItem::AddToCharacter: cell overflow: {} to {} cell {}", m_pProto->szName, ecs::PlayerRuntime::GetName(character).data(), m_wCell);
			return false;
		}
	}
	else if (DRAGON_SOUL_INVENTORY == window_type)
	{
		if (m_wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("CItem::AddToCharacter: cell overflow: {} to {} cell {}", m_pProto->szName, ecs::PlayerRuntime::GetName(character).data(), m_wCell);
			return false;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (window_type == EXTRA_INVENTORY)
	{
		if (m_wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("CItem::AddToCharacter: EXTRA cell overflow: {} to {} cell {}", m_pProto->szName, ecs::PlayerRuntime::GetName(character).data(), m_wCell);
			return false;
		}
	}
#endif
#ifdef ENABLE_SWITCHBOT
	else if (SWITCHBOT == window_type)
	{
		if (m_wCell >= SWITCHBOT_SLOT_COUNT)
		{
			LOG_ERROR("CItem::AddToCharacter:switchbot cell overflow: {} to {} cell {}", m_pProto->szName, ecs::PlayerRuntime::GetName(character).data(), m_wCell);
			return false;
		}
	}
#endif
	if (ecs::PlayerRuntime::GetDesc(character))
		m_dwLastOwnerPID = ecs::PlayerRuntime::GetPlayerID(character);


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
	ch->SetItem(TItemPos(window_type, pos), GetEntityHandle(), isHighLight);
#else
	ch->SetItem(TItemPos(window_type, pos), GetEntityHandle());
#endif
	m_pOwner = ch;

	Save();

	SyncItemOwner(itemEntity, ecs::PlayerRuntime::GetPlayerID(character), m_dwLastOwnerPID, m_dwOwnershipPID);
	SyncItemLocation(itemEntity);
	SyncItemEquipped(itemEntity, false);
	return true;
}

LPITEM CItem::RemoveFromGround()
{
	if (GetSectree())
	{
		SetOwnership(nullptr);

		const entt::entity itemEntity = GetEntityHandle();
		if (itemEntity != entt::null && g_registry.valid(itemEntity))
			ecs::SpatialService::RemoveEntity(g_registry, itemEntity);
		else
			GetSectree()->RemoveEntity(this);

		if (itemEntity != entt::null && g_registry.valid(itemEntity))
		{
			g_registry.remove<ecs::SectorPlacement>(itemEntity);
			g_registry.remove<ecs::ViewActiveTag>(itemEntity);
			g_registry.remove<ecs::SpatialEntity>(itemEntity);
			// LPENTITY.4-fixup-item: keep SpatialKindTag intact. Removing it
			// here breaks any subsequent EntityNetworkDispatch::SendRemove
			// (e.g. PC UpdateSectree age-out) since SendRemove returns
			// silently on missing SpatialKindTag. SpatialEntity is the
			// gating tag for spatial queries; the kind tag is identity.
			g_registry.remove<ecs::ItemGroundPosition>(itemEntity);
		}

		ViewCleanup();

		Save();

		SyncItemLocation(itemEntity);
		g_registry.remove<ecs::ItemOwner>(itemEntity);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
	}

	return (this);
}

bool CItem::AddToGround(int32_t lMapIndex, const PIXEL_POSITION& pos, bool skipOwnerCheck)
{
	if (0 == lMapIndex)
	{
		LOG_ERROR("wrong map index argument: {}", lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		LOG_ERROR("sectree already assigned");
		return false;
	}

	if (!skipOwnerCheck && m_pOwner)
	{
		LOG_ERROR("owner pointer not null");
		return false;
	}

	LPSECTREE tree = ecs::SectorAt(lMapIndex, pos.x, pos.y);

	if (!tree)
	{
		LOG_ERROR("cannot find sectree by {}x{}", pos.x, pos.y);
		return false;
	}

	//tree->Touch();

	SetWindow(GROUND);
	SetXYZ(pos.x, pos.y, pos.z);

	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity != entt::null && g_registry.valid(itemEntity)) {
		g_registry.emplace_or_replace<ecs::SpatialEntity>(itemEntity);
		g_registry.emplace_or_replace<ecs::SpatialKindTag>(itemEntity, ecs::SpatialKindTag{ecs::SpatialKind::Item});
		g_registry.emplace_or_replace<ecs::Position>(itemEntity, pos.x, pos.y, pos.z);
		g_registry.emplace_or_replace<ecs::PositionZ>(itemEntity, ecs::PositionZ{pos.z});
		g_registry.emplace_or_replace<ecs::MapIndex>(itemEntity, lMapIndex);
		g_registry.emplace_or_replace<ecs::VIDComponent>(itemEntity, GetVID());
		g_registry.emplace_or_replace<ecs::ItemGroundPosition>(itemEntity, ecs::ItemGroundPosition{pos.x, pos.y, pos.z});
		(void)g_registry.get_or_emplace<ecs::ViewMap>(itemEntity);
		(void)g_registry.get_or_emplace<ecs::ViewerMap>(itemEntity);
		(void)g_registry.get_or_emplace<ecs::ViewAgeMap>(itemEntity);
		SyncItemLocation(itemEntity);
		g_registry.remove<ecs::ItemOwner>(itemEntity);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		ecs::Invariants::ValidateSpatialCoverage(g_registry, itemEntity, "item.add_to_ground");
	}

	if (!ecs::SpatialService::InsertEntity(g_registry, itemEntity, static_cast<uint32_t>(lMapIndex), pos.x, pos.y, pos.z))
	{
		LOG_ERROR("cannot insert ground item entity id {} vid {} by {}x{} mapindex {}",
			GetID(), GetVID(), pos.x, pos.y, lMapIndex);
		return false;
	}
	ecs::SpatialService::UpdateSectree(g_registry, itemEntity);
	Save();
	return true;
}

bool CItem::DistanceValid(LPCHARACTER ch)
{
	if (!GetSectree())
		return false;

	const entt::entity character = ch ? ch->GetEntityHandle() : entt::null;
	int iDist = DISTANCE_APPROX(
		GetX() - ecs::PlayerRuntime::GetX(character),
		GetY() - ecs::PlayerRuntime::GetY(character));
	if (iDist > 2400)
		return false;

	return true;
}

bool CItem::IsOwnership(LPCHARACTER ch)
{
	if (!m_pkOwnershipEvent)
		return true;

	const entt::entity character = ch ? ch->GetEntityHandle() : entt::null;
	return m_dwOwnershipPID == ecs::PlayerRuntime::GetPlayerID(character);
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

			const entt::entity itemEntity = GetEntityHandle();
			SyncItemOwner(itemEntity, 0, m_dwLastOwnerPID, m_dwOwnershipPID);
			if (itemEntity != entt::null && g_registry.valid(itemEntity))
				g_registry.remove<ecs::ItemOwnershipDisplay>(itemEntity);
		}
		return;
	}

	if (m_pkOwnershipEvent)
		return;

	if (iSec <= 10)
		iSec = 30;

	const entt::entity character = ch->GetEntityHandle();
	m_dwOwnershipPID = ecs::PlayerRuntime::GetPlayerID(character);

	item_event_info* info = AllocEventInfo<item_event_info>();
	strlcpy(info->szOwnerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(info->szOwnerName));
	info->item = GetEntityHandle();

	SetOwnershipEvent(event_create(ownership_event, info, PASSES_PER_SEC(iSec)));

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = m_dwVID;
	strlcpy(p.szName, ecs::PlayerRuntime::GetName(character).data(), sizeof(p.szName));

	PacketAround(&p, sizeof(p));

	const entt::entity itemEntity = GetEntityHandle();
	SyncItemOwner(
		itemEntity,
		m_pOwner ? ecs::PlayerRuntime::GetPlayerID(m_pOwner->GetEntityHandle()) : 0,
		m_dwLastOwnerPID,
		m_dwOwnershipPID);
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.emplace_or_replace<ecs::ItemOwnershipDisplay>(
			itemEntity, ecs::ItemOwnershipDisplay{ecs::PlayerRuntime::GetName(character).data()});
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
	const entt::entity ownerEntity = ch ? ch->GetEntityHandle() : entt::null;
	const auto hasWearItem = [ownerEntity](uint8_t wearCell) {
		return ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, wearCell));
	};

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
		if (hasWearItem(WEAR_RING1))
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

			if (!hasWearItem(iSlot1))
				return iSlot1;

			if (!hasWearItem(iSlot2))
				return iSlot2;

			return -1;
		}
		else
		{
			return -1;
		}
#else
		if (hasWearItem(WEAR_UNIQUE1))
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
		if (!hasWearItem(WEAR_ABILITY1))
		{
			return WEAR_ABILITY1;
		}
		else if (!hasWearItem(WEAR_ABILITY2))
		{
			return WEAR_ABILITY2;
		}
		else if (!hasWearItem(WEAR_ABILITY3))
		{
			return WEAR_ABILITY3;
		}
		else if (!hasWearItem(WEAR_ABILITY4))
		{
			return WEAR_ABILITY4;
		}
		else if (!hasWearItem(WEAR_ABILITY5))
		{
			return WEAR_ABILITY5;
		}
		else if (!hasWearItem(WEAR_ABILITY6))
		{
			return WEAR_ABILITY6;
		}
		else if (!hasWearItem(WEAR_ABILITY7))
		{
			return WEAR_ABILITY7;
		}
#ifndef ENABLE_STOLE_REAL
		else if (!hasWearItem(WEAR_ABILITY8))
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
		LOG_ERROR("EquipTo: nil character");
		return false;
	}

	if (IsDragonSoul())
	{
		if (bWearCell < WEAR_MAX_NUM || bWearCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
		{
			LOG_ERROR("EquipTo: invalid dragon soul cell (this: #{} {} wearflag: {} cell: {})", GetOriginalVnum(), GetName(), GetSubType(), bWearCell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (bWearCell >= WEAR_MAX_NUM)
		{
			LOG_ERROR("EquipTo: invalid wear cell (this: #{} {} wearflag: {} cell: {})", GetOriginalVnum(), GetName(), GetWearFlag(), bWearCell);
			return false;
		}
	}

	const entt::entity charEntity = ch->GetEntityHandle();
	const entt::entity occupied = ItemSystem::GetWearItem(charEntity, bWearCell);
	if (ItemSystem::IsValidItem(occupied))
	{
		LOG_ERROR("EquipTo: item already exist (this: #{} {} cell: {} {})",
			GetOriginalVnum(), GetName(), bWearCell, ItemSystem::GetItemName(occupied));
		return false;
	}

	if (GetOwner())
		RemoveFromCharacter();

	ch->SetWear(bWearCell, GetEntityHandle());

	m_pOwner = ch;
	m_bEquipped = true;
	m_wCell = INVENTORY_MAX_NUM + bWearCell;

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetWearItem(charEntity, i);
		if (ItemSystem::IsValidItem(item))
		{
			SET_BIT(dwImmuneFlag, ItemSystem::GetItemImmuneFlags(item));
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	if (IsDragonSoul())
	{
		DSManager::instance().ActivateDragonSoul(GetEntityHandle());
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
		ch->MountSummon(GetEntityHandle());
#endif
	NetworkSyncSystem::UpdatePacket(charEntity);
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	if (bWearCell == WEAR_BELT)
		NetworkSyncSystem::UpdateItemOnTitleName(g_registry, charEntity, true);
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

	const entt::entity itemEntity = GetEntityHandle();
	SyncItemEquipped(itemEntity, true);
	SyncItemLocation(itemEntity);
	SyncItemOwner(itemEntity, ecs::PlayerRuntime::GetPlayerID(charEntity), m_dwLastOwnerPID, m_dwOwnershipPID);
	g_dispatcher.trigger(ecs::EvItemEquipped { charEntity, itemEntity });

	Save();
	return (true);
}

bool CItem::Unequip()
{
	if (!m_pOwner || GetCell() < INVENTORY_MAX_NUM)
	{
		LOG_ERROR("{} {} m_pOwner {}, GetCell {}", GetName(), GetID(), static_cast<const void*>(get_pointer(m_pOwner)), GetCell());
		return false;
	}

	const entt::entity itemEntity = GetEntityHandle();
	const entt::entity charEntity = m_pOwner->GetEntityHandle();
	if (ItemSystem::GetWearItem(
			charEntity, static_cast<uint8_t>(GetCell() - INVENTORY_MAX_NUM)) != itemEntity)
	{
		LOG_ERROR("m_pOwner->GetWear() != this");
		return false;
	}

	LPCHARACTER owner = m_pOwner;
	const uint8_t wearCell = static_cast<uint8_t>(GetCell() - INVENTORY_MAX_NUM);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (IsMountItem())
		m_pOwner->MountUnsummon(GetEntityHandle());
#endif

	if (IsRideItem())
		ClearMountAttributeAndAffect();

	if (IsDragonSoul())
	{
		DSManager::instance().DeactivateDragonSoul(itemEntity);
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

	m_pOwner->SetWear(GetCell() - INVENTORY_MAX_NUM, entt::null);

#ifndef ENABLE_IMMUNE_FIX
	uint32_t dwImmuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; ++i)
	{
		const entt::entity item = ItemSystem::GetWearItem(charEntity, i);
		if (ItemSystem::IsValidItem(item))
		{
			SET_BIT(dwImmuneFlag, ItemSystem::GetItemImmuneFlags(item));
		}
	}

	m_pOwner->SetImmuneFlag(dwImmuneFlag);
#endif

	m_pOwner->ComputeBattlePoints();

	NetworkSyncSystem::UpdatePacket(charEntity);
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
	SyncItemLocation(itemEntity);
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

	const entt::entity ownerEntity = m_pOwner->GetEntityHandle();
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
					LOG_ERROR("cannot find table by vnum {}", dwVnum);
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
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_WEAPON:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if ((GetSubType() == WEAPON_SWORD) || (GetSubType() == WEAPON_DAGGER) || (GetSubType() == WEAPON_BOW) || (GetSubType() == WEAPON_TWO_HANDED) || (GetSubType() == WEAPON_BELL) || (GetSubType() == WEAPON_FAN)) {
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_WEAPON);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				switch (this->GetSubType()) {
				case WEAPON_SWORD:
					toSetValueEffect = ItemSystem::GetItemValue(item, 0);
					break;
				case WEAPON_DAGGER:
					toSetValueEffect = ItemSystem::GetItemValue(item, 2);
					break;
				case WEAPON_BOW:
					toSetValueEffect = ItemSystem::GetItemValue(item, 3);
					break;
				case WEAPON_TWO_HANDED:
					toSetValueEffect = ItemSystem::GetItemValue(item, 1);
					break;
				case WEAPON_BELL:
					toSetValueEffect = ItemSystem::GetItemValue(item, 4);
					break;
				case WEAPON_FAN:
					toSetValueEffect = ItemSystem::GetItemValue(item, 5);
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

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_WEAPON, toSetValueEffect);
			}
		}
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_WEAPON)))
			break;
#endif

		if (bAdd)
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, GetVnum());
		}
		else
		{
			if (m_wCell == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_ARMOR:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if (GetSubType() == ARMOR_BODY) {
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_BODY);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				toSetValueEffect = ItemSystem::GetItemValue(item, 0);
				if ((!bAdd) &&
					!ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY)))
					toSetValueEffect = 0;

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_BODY, toSetValueEffect);
			}
		}
#endif

		if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY)))
			break;

		if (GetSubType() == ARMOR_BODY || GetSubType() == ARMOR_HEAD || GetSubType() == ARMOR_FOOTS || GetSubType() == ARMOR_SHIELD)
		{
			if (bAdd)
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, GetVnum());
			}
			else
			{
				if (GetProto()->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, m_pOwner->GetOriginalPart(PART_MAIN));
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
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_BODY);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				toSetValueEffect = ItemSystem::GetItemValue(item, 0);
				if ((!bAdd) &&
					!ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, WEAR_BODY)))
					toSetValueEffect = 0;

				ecs::PlayerRuntime::SetPart(ownerEntity, PART_EFFECT_BODY, toSetValueEffect);
			}
#endif
			toSetPart = PART_MAIN;

			if (false == bAdd)
			{
				const entt::entity armor = ItemSystem::GetWearItem(ownerEntity, WEAR_BODY);
				toSetValue = ItemSystem::IsValidItem(armor)
					? ItemSystem::GetItemVnum(armor)
					: m_pOwner->GetOriginalPart(PART_MAIN);
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
			const entt::entity acce = ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_ACCE);
			if (ItemSystem::IsValidItem(acce)) {
				toSetValue = ItemSystem::GetItemVnum(acce);
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
				const entt::entity acce = ItemSystem::GetWearItem(
					ownerEntity, WEAR_COSTUME_ACCE_SLOT);
				if (!ItemSystem::IsValidItem(acce))
					toSetValue = 0;
				else {
					toSetValue = ItemSystem::GetItemVnum(acce);
					toSetValue -= 85000;
					if (ItemSystem::GetItemSocket(acce, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
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
				entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_BODY);
				toSetValue = ItemSystem::IsValidItem(item) ? this->GetValue(0) : 0;
				if (toSetValue == 0) {
					item = ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY);
					toSetValue = ItemSystem::IsValidItem(item) ? this->GetValue(0) : 0;
				}
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_BODY;
		}
		else if (GetSubType() == COSTUME_EFFECT_WEAPON)
		{
			if (bAdd) {
				const entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_WEAPON);
				if (ItemSystem::IsValidItem(item)) {
					switch (ItemSystem::GetItemSubType(item)) {
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
					uint32_t dwWeaponVnum = ItemSystem::GetItemVnum(item);
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
				const entt::entity weapon = ItemSystem::GetWearItem(ownerEntity, WEAR_WEAPON);
				if (ItemSystem::IsValidItem(weapon)) {
					toSetValue = ItemSystem::GetItemVnum(weapon);
				}
				else {
					toSetValue = 0;
				}
			}
		}
#endif

		if (PART_MAX_NUM != toSetPart)
		{
			ecs::PlayerRuntime::SetPart(ownerEntity, (uint8_t)toSetPart, toSetValue);
			NetworkSyncSystem::UpdatePacket(ownerEntity);

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
