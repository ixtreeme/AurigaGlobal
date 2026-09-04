#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "MountSystem.hpp"

#include "InventorySystem.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "ViewSystem.hpp"
#include "../CharacterAccessors.hpp"

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

// Despite the name this copies nothing: GetItemWindow and GetItemCell read
// ecs::ItemLocation, so the old body wrote the component back onto itself. All
// it ever did was create the component, zeroed, when it was missing - and
// EquipItemEcs checks all_of<ItemLocation> for its rollback, so that presence
// is load-bearing. Kept as the ensure it actually is. The values come from
// ItemSystem::SetItemCell and SetItemWindow.
void EnsureItemLocation(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	(void)g_registry.get_or_emplace<ecs::ItemLocation>(e);
}

// lastOwnerPID and ownershipPID used to be passed in from CItem members and
// written back into the component alongside the owner. Those members are gone
// and the component is their only home, so this writes just the two fields it
// actually changes rather than rewriting the other two with themselves.
void SyncItemOwner(entt::entity e, entt::entity owner, uint32_t ownerPID)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(e);
	itemOwner.owner = owner;
	itemOwner.ownerPID = ownerPID;
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

	if (!skipOwnerCheck && GetOwnerEntity() != entt::null)
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
		EnsureItemLocation(itemEntity);
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



void CItem::SetOwnershipEvent(LPEVENT pkEvent)
{
	ItemSystem::GetItemEvents(GetEntityHandle()).ownership = pkEvent;
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
// The cell lives in ecs::ItemLocation. SetCell is the legacy-facing name for
// what SetItemCell already does, so it forwards rather than mirroring a field.
// The owner lives in ecs::ItemOwner. GetOwner keeps returning a pointer
// because that is what its callers are typed on; GetOwnerEntity is the form
// this migration moves them to.
void CItem::SetSkipSave(bool b)
{
	ItemSystem::SetItemSkipSave(GetEntityHandle(), b);
}

bool CItem::GetSkipSave() const
{
	return ItemSystem::GetItemSkipSave(GetEntityHandle());
}

uint32_t CItem::GetLastOwnerPID() const
{
	return ItemSystem::GetItemLastOwnerPID(GetEntityHandle());
}

bool CItem::HasExtraProto() const
{
	return ItemSystem::GetItemExtraProto(GetEntityHandle()) != nullptr;
}

bool CItem::HaveOwnership() const
{
	return ItemSystem::GetItemEvents(GetEntityHandle()).ownership != nullptr;
}

entt::entity CItem::GetOwnerEntity() const
{
	return ItemSystem::GetItemOwner(GetEntityHandle());
}

void CItem::SetOwnerEntity(entt::entity owner)
{
	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity == entt::null || !g_registry.valid(itemEntity))
		return;

	auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(itemEntity);
	itemOwner.owner = owner;
	itemOwner.ownerPID = ecs::PlayerRuntime::GetPlayerID(owner);
}

// Same shape as SetCell: ItemSystem::SetItemWindow mirrors through this
// method, so the component is written here directly.
void CItem::SetWindow(uint8_t b)
{
	const entt::entity itemEntity = GetEntityHandle();
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.get_or_emplace<ecs::ItemLocation>(itemEntity).window = b;
}

uint8_t CItem::GetWindow() const
{
	return ItemSystem::GetItemWindow(GetEntityHandle());
}

uint16_t CItem::GetCell() const
{
	return ItemSystem::GetItemCell(GetEntityHandle());
}

namespace InventorySystem {

void SetOwnership(entt::entity itemEntity, entt::entity character, int iSec)
{
	if (character == entt::null)
	{
		if (ItemSystem::GetItemEvents(itemEntity).ownership)
		{
			event_cancel(&ItemSystem::GetItemEvents(itemEntity).ownership);
			ItemSystem::SetItemOwnershipPID(itemEntity, 0);

			TPacketGCItemOwnership p;

			p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
			p.dwVID = ItemSystem::GetItemVID(itemEntity);
			p.szName[0] = '\0';

			ItemSystem::BroadcastToViewers(itemEntity, &p, sizeof(p));

					SyncItemOwner(itemEntity, entt::null, 0);
			if (itemEntity != entt::null && g_registry.valid(itemEntity))
				g_registry.remove<ecs::ItemOwnershipDisplay>(itemEntity);
		}
		return;
	}

	if (ItemSystem::GetItemEvents(itemEntity).ownership)
		return;

	if (iSec <= 10)
		iSec = 30;

	ItemSystem::SetItemOwnershipPID(itemEntity, ecs::PlayerRuntime::GetPlayerID(character));

	item_event_info* info = AllocEventInfo<item_event_info>();
	strlcpy(info->szOwnerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(info->szOwnerName));
	info->item = itemEntity;

	ItemSystem::GetItemEvents(itemEntity).ownership = event_create(ownership_event, info, PASSES_PER_SEC(iSec));

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = ItemSystem::GetItemVID(itemEntity);
	strlcpy(p.szName, ecs::PlayerRuntime::GetName(character).data(), sizeof(p.szName));

	ItemSystem::BroadcastToViewers(itemEntity, &p, sizeof(p));

	const entt::entity ownerEntity = ItemSystem::GetItemOwner(itemEntity);
	SyncItemOwner(itemEntity, ownerEntity, ecs::PlayerRuntime::GetPlayerID(ownerEntity));
	if (itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.emplace_or_replace<ecs::ItemOwnershipDisplay>(
			itemEntity, ecs::ItemOwnershipDisplay{ecs::PlayerRuntime::GetName(character).data()});
}

entt::entity RemoveFromGround(entt::entity itemEntity)
{
	if (itemEntity == entt::null || !g_registry.valid(itemEntity))
	{
		// The method this replaced fell through to GetSectree()->RemoveEntity(this)
		// here. That path removed the item from the sectree but emitted no
		// SendRemove, so the item stayed rendered on every client in range -
		// the exact symptom the fixup-5 note in DestroyItemEntityAndLegacy
		// describes. Nothing can broadcast without a valid entity either way,
		// so this says so instead of doing it quietly.
		LOG_ERROR("RemoveFromGround: no valid entity ({}), skipping",
			static_cast<uint32_t>(itemEntity));
		return itemEntity;
	}

	if (!ecs::PlayerRuntime::GetSectree(itemEntity))
		return itemEntity;

	SetOwnership(itemEntity, entt::null, 10);


	ecs::SpatialService::RemoveEntity(g_registry, itemEntity);

	g_registry.remove<ecs::SectorPlacement>(itemEntity);
	g_registry.remove<ecs::ViewActiveTag>(itemEntity);
	g_registry.remove<ecs::SpatialEntity>(itemEntity);
	// LPENTITY.4-fixup-item: keep SpatialKindTag intact. Removing it
	// here breaks any subsequent EntityNetworkDispatch::SendRemove
	// (e.g. PC UpdateSectree age-out) since SendRemove returns
	// silently on missing SpatialKindTag. SpatialEntity is the
	// gating tag for spatial queries; the kind tag is identity.
	g_registry.remove<ecs::ItemGroundPosition>(itemEntity);

	ecs::ViewSystem::ViewCleanup(itemEntity);

	ItemSystem::SaveItem(itemEntity);

	EnsureItemLocation(itemEntity);
	g_registry.remove<ecs::ItemOwner>(itemEntity);
	g_registry.remove<ecs::ItemEquipped>(itemEntity);

	return itemEntity;
}

#ifdef __HIGHLIGHT_SYSTEM__
bool AddToCharacter(entt::entity itemEntity, entt::entity character, TItemPos Cell, bool isHighLight)
#else
bool AddToCharacter(entt::entity itemEntity, entt::entity character, TItemPos Cell)
#endif
{
	assert(ecs::PlayerRuntime::GetSectree(itemEntity) == NULL);
	assert(ItemSystem::GetItemOwner(itemEntity) == entt::null);
	if (itemEntity == entt::null)
	{
		LOG_ERROR("AddToCharacter: item {} has no ECS entity", ItemSystem::GetItemID(itemEntity));
		return false;
	}
	uint16_t pos = Cell.cell;
	uint8_t window_type = Cell.window_type;

	if (INVENTORY == window_type)
	{
#ifdef ENABLE_RUNE_SYSTEM
		if (ItemSystem::IsRuneItem(itemEntity) && character != entt::null) {
			int iFindCell = ItemSystem::FindEquipCell(character, itemEntity);
			const entt::entity equipped = ItemSystem::GetWearItem(character, iFindCell);
			if (ItemSystem::IsValidItem(equipped)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 35, "%s", ItemSystem::GetItemName(itemEntity));
#endif
				ItemSystem::DestroyItemEntityEcs(
					itemEntity,
					"INVENTORY_RUNE_ADD_FAILED");
				return false;
			}
			else {
				InventorySystem::EquipTo(itemEntity, character, iFindCell);
				if (ecs::PlayerRuntime::GetDesc(character))
					ItemSystem::SetItemLastOwnerPID(itemEntity, ecs::PlayerRuntime::GetPlayerID(character));

				event_cancel(&ItemSystem::GetItemEvents(itemEntity).destroy);

				ecs::PlayerRuntime::SetItem(character, TItemPos(EQUIPMENT, iFindCell), itemEntity);
				ItemSystem::SetItemOwnerEntity(itemEntity, character);
				ItemSystem::SaveItem(itemEntity);

				SyncItemOwner(itemEntity, character, ecs::PlayerRuntime::GetPlayerID(character));
				EnsureItemLocation(itemEntity);
				SyncItemEquipped(itemEntity, true);
				return true;
			}
		}
#endif
#ifdef ENABLE_MOUNT_INVENTORY_FIX_RAZOR93_egyenlore_kikapcsolva

		if (pos >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > pos)
#else
		if (ItemSystem::GetItemCell(itemEntity) >= INVENTORY_MAX_NUM && BELT_INVENTORY_SLOT_START > ItemSystem::GetItemCell(itemEntity))
#endif
		{
			LOG_ERROR("AddToCharacter: cell overflow: {} to {} cell {}", ItemSystem::GetItemProto(itemEntity)->szName, ecs::PlayerRuntime::GetName(character).data(), ItemSystem::GetItemCell(itemEntity));
			return false;
		}
	}
	else if (DRAGON_SOUL_INVENTORY == window_type)
	{
		if (ItemSystem::GetItemCell(itemEntity) >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("AddToCharacter: cell overflow: {} to {} cell {}", ItemSystem::GetItemProto(itemEntity)->szName, ecs::PlayerRuntime::GetName(character).data(), ItemSystem::GetItemCell(itemEntity));
			return false;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (window_type == EXTRA_INVENTORY)
	{
		if (ItemSystem::GetItemCell(itemEntity) >= EXTRA_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("AddToCharacter: EXTRA cell overflow: {} to {} cell {}", ItemSystem::GetItemProto(itemEntity)->szName, ecs::PlayerRuntime::GetName(character).data(), ItemSystem::GetItemCell(itemEntity));
			return false;
		}
	}
#endif
#ifdef ENABLE_SWITCHBOT
	else if (SWITCHBOT == window_type)
	{
		if (ItemSystem::GetItemCell(itemEntity) >= SWITCHBOT_SLOT_COUNT)
		{
			LOG_ERROR("AddToCharacter:switchbot cell overflow: {} to {} cell {}", ItemSystem::GetItemProto(itemEntity)->szName, ecs::PlayerRuntime::GetName(character).data(), ItemSystem::GetItemCell(itemEntity));
			return false;
		}
	}
#endif
	if (ecs::PlayerRuntime::GetDesc(character))
		ItemSystem::SetItemLastOwnerPID(itemEntity, ecs::PlayerRuntime::GetPlayerID(character));


#ifdef ENABLE_ACCE_SYSTEM
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE) && (ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET) == 0))
	{
		int32_t lVal = ItemSystem::GetItemValue(itemEntity, ACCE_GRADE_VALUE_FIELD);
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

		ItemSystem::SetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET, lVal);
	}
#endif


	event_cancel(&ItemSystem::GetItemEvents(itemEntity).destroy);

#ifdef __HIGHLIGHT_SYSTEM__
	ecs::PlayerRuntime::SetItem(character, TItemPos(window_type, pos), itemEntity, isHighLight);
#else
	ecs::PlayerRuntime::SetItem(character, TItemPos(window_type, pos), itemEntity);
#endif
	ItemSystem::SetItemOwnerEntity(itemEntity, character);

	ItemSystem::SaveItem(itemEntity);

	SyncItemOwner(itemEntity, character, ecs::PlayerRuntime::GetPlayerID(character));
	EnsureItemLocation(itemEntity);
	SyncItemEquipped(itemEntity, false);
	return true;
}

} // namespace InventorySystem

namespace InventorySystem {

bool Unequip(entt::entity itemEntity)
{
	if (ItemSystem::GetItemOwner(itemEntity) == entt::null || ItemSystem::GetItemCell(itemEntity) < INVENTORY_MAX_NUM)
	{
		LOG_ERROR("{} {} owner {}, GetCell {}", ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemID(itemEntity), static_cast<uint32_t>(ItemSystem::GetItemOwner(itemEntity)), ItemSystem::GetItemCell(itemEntity));
		return false;
	}

	const entt::entity charEntity = ItemSystem::GetItemOwner(itemEntity);
	if (ItemSystem::GetWearItem(
			charEntity, static_cast<uint8_t>(ItemSystem::GetItemCell(itemEntity) - INVENTORY_MAX_NUM)) != itemEntity)
	{
		LOG_ERROR("GetWearItem(owner, cell) is not this item");
		return false;
	}

	const uint8_t wearCell = static_cast<uint8_t>(ItemSystem::GetItemCell(itemEntity) - INVENTORY_MAX_NUM);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ItemSystem::IsMountItem(itemEntity))
		MountSystem::MountUnsummon(charEntity, itemEntity);
#endif

	if (ItemSystem::IsRideItem(itemEntity))
		ItemSystem::ClearMountAttributeAndAffect(itemEntity);

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		DSManager::instance().DeactivateDragonSoul(itemEntity);
	}
#ifdef ENABLE_RUNE_SYSTEM
	else if (ItemSystem::IsRuneItem(itemEntity)) {
		if (ItemSystem::GetItemSocket(itemEntity, 1) == 1)
			ItemSystem::ModifyPoints(itemEntity, false);
	}
#endif
	else
	{
		ItemSystem::ModifyPoints(itemEntity, false);
	}

	ItemSystem::StopUniqueExpireEvent(itemEntity);

	if (-1 != ItemSystem::GetItemProto(itemEntity)->cLimitTimerBasedOnWearIndex)
		ItemSystem::StopTimerBasedOnWearExpireEvent(itemEntity);

	ItemSystem::StopAccessorySocketExpireEvent(itemEntity);

	ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(charEntity, itemEntity);

	ecs::PlayerRuntime::SetWear(charEntity, wearCell, entt::null);

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

	ecs::PlayerRuntime::SetImmuneFlag(charEntity, dwImmuneFlag);
#endif

	ecs::PointSystem::ComputeBattlePoints(charEntity);

	NetworkSyncSystem::UpdatePacket(charEntity);
#ifdef ENABLE_COSTUME_PET
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_PET_SKIN)) {
		MountSystem::UpdatePetSkin(charEntity);
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT_SKIN)) {
		MountSystem::UpdateMountSkin(charEntity);
	}
#endif
	ItemSystem::SetItemOwnerEntity(itemEntity, entt::null);
	// SetWear(.., entt::null) leaves the cell alone - SetItem only writes it on
	// the has-item branch - so ecs::ItemLocation kept the equipment cell while
	// m_wCell went to 0. Writing through the component fixes both at once.
	ItemSystem::SetItemCell(itemEntity, charEntity, 0);

	SyncItemEquipped(itemEntity, false);
	EnsureItemLocation(itemEntity);
	g_dispatcher.trigger(ecs::EvItemUnequipped { charEntity, itemEntity });
	return true;
}

bool EquipTo(entt::entity itemEntity, entt::entity charEntity, uint8_t bWearCell)
{
	if (charEntity == entt::null || !g_registry.valid(charEntity))
	{
		LOG_ERROR("EquipTo: nil character");
		return false;
	}

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		if (bWearCell < WEAR_MAX_NUM || bWearCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
		{
			LOG_ERROR("EquipTo: invalid dragon soul cell (item: #{} {} wearflag: {} cell: {})", ItemSystem::GetItemOriginalVnum(itemEntity), ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemSubType(itemEntity), bWearCell - WEAR_MAX_NUM);
			return false;
		}
	}
	else
	{
		if (bWearCell >= WEAR_MAX_NUM)
		{
			LOG_ERROR("EquipTo: invalid wear cell (item: #{} {} wearflag: {} cell: {})", ItemSystem::GetItemOriginalVnum(itemEntity), ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemWearFlag(itemEntity), bWearCell);
			return false;
		}
	}

	const entt::entity occupied = ItemSystem::GetWearItem(charEntity, bWearCell);
	if (ItemSystem::IsValidItem(occupied))
	{
		LOG_ERROR("EquipTo: item already exist (item: #{} {} cell: {} {})",
			ItemSystem::GetItemOriginalVnum(itemEntity), ItemSystem::GetItemName(itemEntity), bWearCell, ItemSystem::GetItemName(occupied));
		return false;
	}

	if (ItemSystem::GetItemOwner(itemEntity) != entt::null)
		RemoveFromCharacter(itemEntity);

	ecs::PlayerRuntime::SetWear(charEntity, bWearCell, itemEntity);

	ItemSystem::SetItemOwnerEntity(itemEntity, charEntity);
	// SetWear above already routed the cell through ItemSystem::SetItemCell,
	// which writes ecs::ItemLocation and mirrors it back into m_wCell.
	SyncItemEquipped(itemEntity, true);

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

	ecs::PlayerRuntime::SetImmuneFlag(charEntity, dwImmuneFlag);
#endif

	if (ItemSystem::IsDragonSoulItem(itemEntity))
	{
		DSManager::instance().ActivateDragonSoul(itemEntity);
	}
	else
	{
#ifdef ENABLE_RUNE_SYSTEM
		if (!ItemSystem::IsRuneItem(itemEntity))
			ItemSystem::ModifyPoints(itemEntity, true);
		else if (ItemSystem::GetItemSocket(itemEntity, 1) == 1)
			ItemSystem::ModifyPoints(itemEntity, true);
#else
		ItemSystem::ModifyPoints(itemEntity, true);
#endif
		ItemSystem::StartUniqueExpireEvent(itemEntity);
		if (-1 != ItemSystem::GetItemProto(itemEntity)->cLimitTimerBasedOnWearIndex)
			ItemSystem::StartTimerBasedOnWearExpireEvent(itemEntity);

		// ACCESSORY_REFINE
		ItemSystem::StartAccessorySocketExpireEvent(itemEntity);
		// END_OF_ACCESSORY_REFINE
	}

	ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(charEntity, itemEntity);

	ecs::PointSystem::ComputeBattlePoints(charEntity);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ItemSystem::IsMountItem(itemEntity))
		MountSystem::MountSummon(charEntity, itemEntity);
#endif
	NetworkSyncSystem::UpdatePacket(charEntity);
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	if (bWearCell == WEAR_BELT)
		NetworkSyncSystem::UpdateItemOnTitleName(g_registry, charEntity, true);
#endif

#ifdef ENABLE_COSTUME_PET
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_PET_SKIN)) {
		MountSystem::UpdatePetSkin(charEntity);
	}
#endif
#ifdef ENABLE_COSTUME_MOUNT
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT_SKIN)) {
		MountSystem::UpdateMountSkin(charEntity);
	}
#endif

	EnsureItemLocation(itemEntity);
	SyncItemOwner(itemEntity, charEntity, ecs::PlayerRuntime::GetPlayerID(charEntity));
	g_dispatcher.trigger(ecs::EvItemEquipped { charEntity, itemEntity });

	ItemSystem::SaveItem(itemEntity);
	return (true);
}

entt::entity RemoveFromCharacter(entt::entity itemEntity)
{
	if (ItemSystem::GetItemOwner(itemEntity) == entt::null)
	{
		LOG_ERROR("RemoveFromCharacter: owner null");
		return itemEntity;
	}

	const entt::entity ownerEntity = ItemSystem::GetItemOwner(itemEntity);

	// Detaching means three component writes, and each one used to be a bare
	// field assignment plus a call to the old SyncItemLocation, which copied
	// nothing - so ecs::ItemLocation kept the cell and window the item had
	// while it was still carried. SetItemCell and SetItemWindow write the
	// component and mirror into m_wCell / m_bWindow, so both agree.
	const auto detach = [&]() {
		ItemSystem::SetItemCell(itemEntity, ownerEntity, 0);
		ItemSystem::SetItemWindow(itemEntity, RESERVED_WINDOW);
		ItemSystem::SetItemOwnerEntity(itemEntity, entt::null);
		ItemSystem::SaveItem(itemEntity);

		EnsureItemLocation(itemEntity);
		SyncItemOwner(itemEntity, entt::null, 0);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
	};

	if (ItemSystem::IsItemEquipped(itemEntity))
	{
		Unequip(itemEntity);
		ItemSystem::SetItemWindow(itemEntity, RESERVED_WINDOW);
		ItemSystem::SaveItem(itemEntity);

		EnsureItemLocation(itemEntity);
		SyncItemOwner(itemEntity, entt::null, 0);
		g_registry.remove<ecs::ItemEquipped>(itemEntity);
		return itemEntity;
	}

	const uint8_t window = ItemSystem::GetItemWindow(itemEntity);
	const uint16_t cell = ItemSystem::GetItemCell(itemEntity);

	if (window == MOUNT_INVENTORY)
	{
		if (CMountInventory* mi = ecs::LegacyCharOf(ownerEntity)->GetMountInventory())
			mi->RemoveByItem(itemEntity);

		detach();
		return itemEntity;
	}

	if (window != SAFEBOX && window != MALL)
	{
		if (ItemSystem::IsDragonSoulItem(itemEntity))
		{
			if (cell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				LOG_ERROR("RemoveFromCharacter: pos >= DRAGON_SOUL_INVENTORY_MAX_NUM");
			else
				ecs::PlayerRuntime::SetItem(ownerEntity, TItemPos(window, cell), entt::null);
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if (ItemSystem::IsExtraItem(itemEntity))
		{
			if (cell >= EXTRA_INVENTORY_MAX_NUM)
				LOG_ERROR("RemoveFromCharacter: pos >= EXTRA_INVENTORY_MAX_NUM");
			else
				ecs::PlayerRuntime::SetItem(ownerEntity, TItemPos(window, cell), entt::null);
		}
#endif
#ifdef ENABLE_SWITCHBOT
		else if (window == SWITCHBOT)
		{
			if (cell >= SWITCHBOT_SLOT_COUNT)
			{
				LOG_ERROR("RemoveFromCharacter: pos >= SWITCHBOT_SLOT_COUNT");
			}
			else
			{
				ecs::PlayerRuntime::SetItem(ownerEntity, TItemPos(SWITCHBOT, cell), entt::null);
			}
		}
#endif
		else
		{
			TItemPos pos(INVENTORY, cell);

			if (false == pos.IsDefaultInventoryPosition() && false == pos.IsBeltInventoryPosition())
				LOG_ERROR("RemoveFromCharacter: Invalid Item Position");
			else
				ecs::PlayerRuntime::SetItem(ownerEntity, pos, entt::null);
		}
	}

	detach();
	return itemEntity;
}

} // namespace InventorySystem

namespace ItemSystem {

void ModifyPoints(entt::entity itemEntity, bool bAdd)
{
	const entt::entity ownerEntity = ItemSystem::GetItemOwner(itemEntity);
#ifdef ENABLE_BUG_FIXES
	if (ownerEntity == entt::null) {
		return;
	}
#endif

	const TItemTable* proto = ItemSystem::GetItemProto(itemEntity);
	int accessoryGrade;

	if (false == ItemSystem::IsAccessoryForSocket(itemEntity))
	{
		if (proto->bType == ITEM_WEAPON || proto->bType == ITEM_ARMOR)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				uint32_t dwVnum;

				if ((dwVnum = ItemSystem::GetItemSocket(itemEntity, i)) <= 2)
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
							ecs::PointSystem::ApplyPoint(ownerEntity, aApplie.bType, bAdd ? aApplie.lValue : aApplie.lValue ^ 0x00800000);
						else
							ecs::PointSystem::ApplyPoint(ownerEntity, aApplie.bType, bAdd ? aApplie.lValue : -aApplie.lValue);
					}
				}
			}
		}

		accessoryGrade = 0;
	}
	else
	{
		accessoryGrade = MIN(ItemSystem::GetItemAccessorySocketGrade(itemEntity), ITEM_ACCESSORY_SOCKET_MAX_NUM);
	}


#ifdef ENABLE_ACCE_SYSTEM
	if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE) && (ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET)))
	{
		TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET));
		if (pkItemAbsorbed)
		{
			/* 			if ((pkItemAbsorbed->bType == ITEM_ARMOR) && (pkItemAbsorbed->bSubType == ARMOR_BODY))
						{
							int32_t lDefGrade = pkItemAbsorbed->alValues[1] + int32_t(pkItemAbsorbed->alValues[5] * 2);
							double dValue = lDefGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefGrade = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[1] > 0 && (lDefGrade <= 0)) || (pkItemAbsorbed->alValues[5] > 0 && (lDefGrade < 1)))
								lDefGrade += 1;
							else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[5] > 0))
								lDefGrade += 1;

							ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_DEF_GRADE_BONUS, bAdd ? lDefGrade : -lDefGrade);

							int32_t lDefMagicBonus = pkItemAbsorbed->alValues[0];
							dValue = lDefMagicBonus * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
							dValue = (double)dValue / 100;
							dValue = (double)dValue + .5;
							lDefMagicBonus = (int32_t) dValue;
							if ((pkItemAbsorbed->alValues[0] > 0) && (lDefMagicBonus < 1))
								lDefMagicBonus += 1;
							else if (pkItemAbsorbed->alValues[0] > 0)
								lDefMagicBonus += 1;

							ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_MAGIC_DEF_GRADE, bAdd ? lDefMagicBonus : -lDefMagicBonus);
						} */
			/* else  */if (pkItemAbsorbed->bType == ITEM_WEAPON)
			{
				int32_t lAttGrade = pkItemAbsorbed->alValues[4] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[3] > pkItemAbsorbed->alValues[4])
					lAttGrade = pkItemAbsorbed->alValues[3] + pkItemAbsorbed->alValues[5];

				double dValue = lAttGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[3] > 0) && (lAttGrade < 1)) || ((pkItemAbsorbed->alValues[4] > 0) && (lAttGrade < 1)))
					lAttGrade += 1;
				else if ((pkItemAbsorbed->alValues[3] > 0) || (pkItemAbsorbed->alValues[4] > 0))
					lAttGrade += 1;

				ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_ATT_GRADE_BONUS, bAdd ? lAttGrade : -lAttGrade);

				int32_t lAttMagicGrade = pkItemAbsorbed->alValues[2] + pkItemAbsorbed->alValues[5];
				if (pkItemAbsorbed->alValues[1] > pkItemAbsorbed->alValues[2])
					lAttMagicGrade = pkItemAbsorbed->alValues[1] + pkItemAbsorbed->alValues[5];

				dValue = lAttMagicGrade * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
				dValue = dValue / 100;
				dValue = dValue + .5;
				lAttMagicGrade = (int32_t)dValue;
				if (((pkItemAbsorbed->alValues[1] > 0) && (lAttMagicGrade < 1)) || ((pkItemAbsorbed->alValues[2] > 0) && (lAttMagicGrade < 1)))
					lAttMagicGrade += 1;
				else if ((pkItemAbsorbed->alValues[1] > 0) || (pkItemAbsorbed->alValues[2] > 0))
					lAttMagicGrade += 1;

				ecs::PointSystem::ApplyPoint(ownerEntity, APPLY_MAGIC_ATT_GRADE, bAdd ? lAttMagicGrade : -lAttMagicGrade);
			}
		}
	}
#endif


	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
#ifdef ENABLE_ACCE_SYSTEM
		if ((proto->aApplies[i].bType == APPLY_NONE) && (ItemSystem::GetItemType(itemEntity) != ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) != COSTUME_ACCE))
#else
		if (proto->aApplies[i].bType == APPLY_NONE)
#endif
			continue;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (ItemSystem::IsMountItem(itemEntity))
			continue;
#endif

		int32_t value = proto->aApplies[i].lValue;
#ifdef ENABLE_ACCE_SYSTEM
		if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE))
		{
			TItemTable* pkItemAbsorbed = ITEM_MANAGER::instance().GetTable(ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORBED_SOCKET));
			if (pkItemAbsorbed)
			{
				if (pkItemAbsorbed->aApplies[i].bType == APPLY_NONE)
					continue;

				value = pkItemAbsorbed->aApplies[i].lValue;
				if (value < 0)
					continue;

				double dValue = value * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
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
		if (proto->aApplies[i].bType == APPLY_SKILL)
		{
			ecs::PointSystem::ApplyPoint(ownerEntity, proto->aApplies[i].bType, bAdd ? value : value ^ 0x00800000);
		}
		else
		{
			if (0 != accessoryGrade)
				value += MAX(accessoryGrade, value * aiAccessorySocketEffectivePct[accessoryGrade] / 100);

			ecs::PointSystem::ApplyPoint(ownerEntity, proto->aApplies[i].bType, bAdd ? value : -value);
		}
	}

#ifdef ENABLE_ITEM_EXTRA_PROTO
	if (ItemSystem::GetItemExtraProto(itemEntity) != nullptr)
	{
#ifdef ENABLE_NEW_EXTRA_BONUS
		for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; i++)
		{
			auto type = ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].bType;
			if (type != APPLY_NONE) {
				auto value = ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].lValue;
				ecs::PointSystem::ApplyPoint(ownerEntity, ItemSystem::GetItemExtraProto(itemEntity)->ExtraBonus[i].bType, bAdd ? value : -value);
			}
		}
#endif
	}
#endif

	if (true == CItemVnumHelper::IsRamadanMoonRing(ItemSystem::GetItemVnum(itemEntity)) || true == CItemVnumHelper::IsHalloweenCandy(ItemSystem::GetItemVnum(itemEntity))
		|| true == CItemVnumHelper::IsHappinessRing(ItemSystem::GetItemVnum(itemEntity)) || true == CItemVnumHelper::IsLovePendant(ItemSystem::GetItemVnum(itemEntity)))
	{
		// Do not anything.
	}
	else
	{
		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			if (ItemSystem::GetItemAttributeType(itemEntity, i))
			{
				const TPlayerItemAttribute& ia = ItemSystem::GetItemAttribute(itemEntity, i);
				int32_t sValue = ia.sValue;
#ifdef ENABLE_ACCE_SYSTEM
				if ((ItemSystem::GetItemType(itemEntity) == ITEM_COSTUME) && (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE)) {
					double dValue = sValue * ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET);
					dValue = dValue / 100;
					dValue = dValue + .5;
					sValue = (int32_t)dValue;
					if ((ia.sValue > 0) && (sValue <= 0))
						sValue += 1;
				}
#endif

#ifdef ATTR_LOCK
				if (ItemSystem::GetItemLockedAttr(itemEntity) == i) {
					continue;
				}
#endif

				if (ia.bType == APPLY_SKILL)
					ecs::PointSystem::ApplyPoint(ownerEntity, ia.bType, bAdd ? sValue : sValue ^ 0x00800000);
				else
					ecs::PointSystem::ApplyPoint(ownerEntity, ia.bType, bAdd ? sValue : -sValue);
			}
		}
	}

	switch (proto->bType)
	{
	case ITEM_PICK:
	case ITEM_ROD:
	{
		if (bAdd)
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, ItemSystem::GetItemVnum(itemEntity));
		}
		else
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_WEAPON:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if ((ItemSystem::GetItemSubType(itemEntity) == WEAPON_SWORD) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_DAGGER) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_BOW) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_TWO_HANDED) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_BELL) || (ItemSystem::GetItemSubType(itemEntity) == WEAPON_FAN)) {
			const entt::entity item = ItemSystem::GetWearItem(
				ownerEntity, WEAR_COSTUME_EFFECT_WEAPON);
			if (ItemSystem::IsValidItem(item)) {
				uint32_t toSetValueEffect;
				switch (ItemSystem::GetItemSubType(itemEntity)) {
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
					uint32_t dwWeaponVnum = ItemSystem::GetItemVnum(itemEntity);
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
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, ItemSystem::GetItemVnum(itemEntity));
		}
		else
		{
			if (ItemSystem::GetItemCell(itemEntity) == INVENTORY_MAX_NUM + WEAR_WEAPON)
				ecs::PlayerRuntime::SetPart(ownerEntity, PART_WEAPON, 0);
		}
	}
	break;

	case ITEM_ARMOR:
	{
#ifdef ENABLE_COSTUME_EFFECT
		if (ItemSystem::GetItemSubType(itemEntity) == ARMOR_BODY) {
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

		if (ItemSystem::GetItemSubType(itemEntity) == ARMOR_BODY || ItemSystem::GetItemSubType(itemEntity) == ARMOR_HEAD || ItemSystem::GetItemSubType(itemEntity) == ARMOR_FOOTS || ItemSystem::GetItemSubType(itemEntity) == ARMOR_SHIELD)
		{
			if (bAdd)
			{
				if (ItemSystem::GetItemProto(itemEntity)->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, ItemSystem::GetItemVnum(itemEntity));
			}
			else
			{
				if (ItemSystem::GetItemProto(itemEntity)->bSubType == ARMOR_BODY)
					ecs::PlayerRuntime::SetPart(ownerEntity, PART_MAIN, ecs::PlayerRuntime::GetOriginalPart(ownerEntity, PART_MAIN));
			}
		}
	}
	break;

	case ITEM_COSTUME:
	{
		uint32_t toSetValue = ItemSystem::GetItemVnum(itemEntity);
		EParts toSetPart = PART_MAX_NUM;

		if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_BODY)
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
					: ecs::PlayerRuntime::GetOriginalPart(ownerEntity, PART_MAIN);
			}
		}
#ifdef ENABLE_RUNE_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == RUNE_SLOT7)
		{
			toSetPart = PART_RUNE;
			toSetValue = (true == bAdd) ? ecs::PlayerRuntime::GetRuneEffect(ownerEntity) : 0;
		}
#endif
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_HAIR)
		{
			toSetPart = PART_HAIR;
			toSetValue = (true == bAdd) ? ItemSystem::GetItemValue(itemEntity, 3) : 0;
		}

#ifdef ENABLE_ACCE_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_ACCE)
		{
			toSetValue -= 85000;
			if (ItemSystem::GetItemSocket(itemEntity, ACCE_ABSORPTION_SOCKET) >= ACCE_EFFECT_FROM_ABS)
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
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_STOLE)
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
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_EFFECT_BODY)
		{
			if (bAdd) {
				entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_BODY);
				toSetValue = ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(itemEntity, 0) : 0;
				if (toSetValue == 0) {
					item = ItemSystem::GetWearItem(ownerEntity, WEAR_COSTUME_BODY);
					toSetValue = ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(itemEntity, 0) : 0;
				}
			}
			else
				toSetValue = 0;

			toSetPart = PART_EFFECT_BODY;
		}
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_EFFECT_WEAPON)
		{
			if (bAdd) {
				const entt::entity item = ItemSystem::GetWearItem(ownerEntity, WEAR_WEAPON);
				if (ItemSystem::IsValidItem(item)) {
					switch (ItemSystem::GetItemSubType(item)) {
					case WEAPON_SWORD:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 0);
						break;
					case WEAPON_DAGGER:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 2);
						break;
					case WEAPON_BOW:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 3);
						break;
					case WEAPON_TWO_HANDED:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 1);
						break;
					case WEAPON_BELL:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 4);
						break;
					case WEAPON_FAN:
						toSetValue = ItemSystem::GetItemValue(itemEntity, 5);
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
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_MOUNT)
		{
			// not need to do a thing in here
		}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (ItemSystem::GetItemSubType(itemEntity) == COSTUME_WEAPON)
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
		if (0 != ItemSystem::GetItemSIGVnum(itemEntity))
		{
			const CSpecialItemGroup* pItemGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(ItemSystem::GetItemSIGVnum(itemEntity));
			if (nullptr == pItemGroup)
				break;
			uint32_t dwAttrVnum = pItemGroup->GetAttrVnum(ItemSystem::GetItemVnum(itemEntity));
			const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(dwAttrVnum);
			if (nullptr == pAttrGroup)
				break;
			for (auto it = pAttrGroup->m_vecAttrs.begin(); it != pAttrGroup->m_vecAttrs.end(); ++it)
			{
				ecs::PointSystem::ApplyPoint(ownerEntity, it->apply_type, bAdd ? it->apply_value : it->apply_value); // -it->apply_value
			}
		}
	}
	break;
	}
}

} // namespace ItemSystem

bool CItem::IsEquipped() const
{
	return ItemSystem::IsItemEquipped(GetEntityHandle());
}

void CItem::ModifyPoints(bool bAdd)
{
	ItemSystem::ModifyPoints(GetEntityHandle(), bAdd);
}
