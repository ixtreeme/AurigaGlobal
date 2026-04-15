#include "../../stdafx.h"

#include "ItemSystem.hpp"

#include "../../utils.h"
#include "../../config.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../item_manager.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../desc_manager.h"
#include "../../packet.h"
#include "../../affect.h"
#include "../../skill.h"
#include "../../start_position.h"
#include "../../mob_manager.h"
#include "../../db.h"
#include "../../log.h"
#include "../../vector.h"
#include "../../buffer_manager.h"
#include "../../questmanager.h"
#include "../../fishing.h"
#include "../../party.h"
#include "../../dungeon.h"
#include "../../refine.h"
#include "../../unique_item.h"
#include "../../war_map.h"
#include "../../marriage.h"
#include "../../polymorph.h"
#include "../../blend_item.h"
#include "../../BattleArena.h"
#include "../../arena.h"
#include "../../dev_log.h"
#include "../../pcbang.h"
#include "../../../common/VnumHelper.h"
#include "../../belt_inventory_helper.h"
#include "../../MountSystem.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../shop.h"
#include "../../safebox.h"
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
#endif
#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif
#include "../../DragonSoul.h"
#include "../../buff_on_attributes.h"
#include "../../ItemUse.h"
#include "../../../common/CommonDefines.h"

#include "../Registry.hpp"
#include "../components/identity_components.hpp"

bool IS_SUMMONABLE_ZONE(int map_index);

namespace {

LPCHARACTER LegacyCharacter(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* vid = g_registry.try_get<ecs::VIDComponent>(e);
    if (!vid)
        return nullptr;

    return CHARACTER_MANAGER::instance().Find(vid->value);
}

static inline LPCHARACTER LegacyCharOf(entt::entity e)
{
    return LegacyCharacter(e);
}

static void FN_copy_item_socket(LPITEM dest, LPITEM src)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		dest->SetSocket(i, src->GetSocket(i));
	}
}

#ifdef ENABLE_PVP_ADVANCED
static bool IS_POTION_PVP_BLOCKED(int vnum)
{
	switch (vnum)
	{
	case 72725:
	case 72726:
		return true;
	}
	return false;
}
#endif

static bool IS_SUMMON_ITEM(int vnum)
{
	switch (vnum)
	{
	case 22000:
	case 22010:
	case 22011:
	case 22012:
	case 22013:
	case 22014:
	case 22015:
		return true;
	}

	return false;
}


static bool FN_check_item_sex(LPCHARACTER ch, LPITEM item)
{
#ifdef ENABLE_WOLFMAN_CHARACTER
    if (ITEM_RING == item->GetType())
        return true;
#endif

    if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE))
    {
        if (SEX_MALE == GET_SEX(ch))
            return false;
    }

    if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE))
    {
        if (SEX_FEMALE == GET_SEX(ch))
            return false;
    }

    return true;
}

} // namespace

namespace ItemSystem {

LPITEM GetItem(entt::entity e, TItemPos cell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->GetItem(cell) : nullptr;
}

LPITEM GetInventoryItem(entt::entity e, uint16_t cell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->GetInventoryItem(cell) : nullptr;
}

#ifdef ENABLE_EXTRA_INVENTORY
LPITEM GetExtraInventoryItem(entt::entity e, uint16_t cell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->GetExtraInventoryItem(cell) : nullptr;
}
#endif

LPITEM FindSpecifyItem(entt::entity e, uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
                       , bool reinforce
#endif
)
{
    LPCHARACTER ch = LegacyCharacter(e);
    if (!ch)
        return nullptr;

#ifdef ENABLE_EXTRA_INVENTORY
    return ch->FindSpecifyItem(vnum, reinforce);
#else
    return ch->FindSpecifyItem(vnum);
#endif
}

LPITEM FindItemByID(entt::entity e, uint32_t id)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->FindItemByID(id) : nullptr;
}

int CountItemRenewal(entt::entity e, uint32_t vnum)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->CountSpecifyItemRenewal(vnum) : 0;
}

int CountItem(entt::entity e, uint32_t vnum)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->CountSpecifyItem(vnum) : 0;
}

int CountTypeItem(entt::entity e, uint8_t type)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->CountSpecifyTypeItem(type) : 0;
}

bool HasItem(entt::entity e, uint32_t vnum, uint32_t count)
{
    return CountItem(e, vnum) >= static_cast<int>(count);
}

LPITEM GetWearItem(entt::entity e, uint8_t wearPos)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->GetWear(wearPos) : nullptr;
}

void SetWearItem(entt::entity e, uint8_t wearPos, LPITEM item)
{
    LPCHARACTER ch = LegacyCharacter(e);
    if (ch)
        ch->SetWear(wearPos, item);
}

bool UnequipItem(entt::entity e, LPITEM item)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->UnequipItem(item) : false;
}

bool EquipItem(entt::entity e, LPITEM item, int candidateCell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->EquipItem(item, candidateCell) : false;
}

bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->IsEquipUniqueItem(itemVnum) : false;
}

bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->IsEquipUniqueGroup(groupVnum) : false;
}

bool UnEquipSpecialRideUniqueItem(entt::entity e)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->UnEquipSpecialRideUniqueItem() : false;
}

bool CanEquipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->CanEquipNow(item, srcCell, destCell) : false;
}

bool CanUnequipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->CanUnequipNow(item, srcCell, destCell) : false;
}

bool DropItem(entt::entity e, TItemPos cell,
#ifdef ENABLE_NEW_STACK_LIMIT
              int
#else
              uint8_t
#endif
                  count)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->DropItem(cell, count) : false;
}

bool DropGold(entt::entity e, int64_t gold)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->DropGold(gold) : false;
}

bool MoveItem(entt::entity e, TItemPos fromCell, TItemPos toCell,
#ifdef ENABLE_NEW_STACK_LIMIT
              int
#else
              uint8_t
#endif
                  count)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->MoveItem(fromCell, toCell, count) : false;
}

bool PickupItem(entt::entity e, uint32_t vid)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->PickupItem(vid) : false;
}

bool UseItem(entt::entity e, TItemPos cell, TItemPos destCell)
{
    LPCHARACTER ch = LegacyCharacter(e);
    return ch ? ch->UseItem(cell, destCell) : false;
}


} // namespace ItemSystem

// char_item.cpp slice A moved into ItemSystem.cpp

LPITEM CHARACTER::GetInventoryItem(uint16_t wCell) const
{
	return GetItem(TItemPos(INVENTORY, wCell));
}


#ifdef ENABLE_EXTRA_INVENTORY
LPITEM CHARACTER::GetExtraInventoryItem(uint16_t wCell) const
{
	return GetItem(TItemPos(EXTRA_INVENTORY, wCell));
	return GetItem(TItemPos(INVENTORY, wCell));
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: Called: Char_item.cpp LPITEM CHARACTER::GetExtraInventoryItem(uint16_t wCell) const");
#endif
}
#endif

LPITEM CHARACTER::GetItem(TItemPos Cell) const
{

	if (!IsValidItemPosition(Cell))
		return nullptr;
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid item cell %d", wCell);
			return nullptr;
		}
		return m_pointsInstant.pItems[wCell];
	case DRAGON_SOUL_INVENTORY:
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid DS item cell %d", wCell);
			return nullptr;
		}
		return m_pointsInstant.pDSItems[wCell];


#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :315: case switch :if (wCell >= EXTRA_INVENTORY_MAX_NUM)");
#endif
			sys_err("CHARACTER::GetInventoryItem: invalid EXTRA item cell %d", wCell);
			return nullptr;
		}
		return m_pointsInstant.pExtraItems[wCell];
#endif

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid switchbot item cell %d", wCell);
			return nullptr;
		}
		return m_pointsInstant.pSwitchbotItems[wCell];
#endif
	default:
		return nullptr;
	}
	return nullptr;
}


LPITEM CHARACTER::FindSpecifyItem(uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
	, bool reinforce
#endif
) const
{
#ifdef ENABLE_EXTRA_INVENTORY
	if (reinforce) {
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i) {
			if (GetExtraInventoryItem(i) && GetExtraInventoryItem(i)->GetVnum() == vnum) {
				return GetExtraInventoryItem(i);
			}
		}
	}
	else {
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		for (int i = 0; i < Inventory_Size(); ++i)
#else
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		{
			if (GetInventoryItem(i) && GetInventoryItem(i)->GetVnum() == vnum) {
				return GetInventoryItem(i);
			}
		}
	}
#else
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	for (int i = 0; i < Inventory_Size(); ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (GetInventoryItem(i) && GetInventoryItem(i)->GetVnum() == vnum)
			return GetInventoryItem(i);
#endif

	return nullptr;
}

LPITEM CHARACTER::FindItemByID(uint32_t id) const
{
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	for (int i = 0; i < Inventory_Size(); ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		if (nullptr != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

	for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
	{
		if (nullptr != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
		if (nullptr != GetExtraInventoryItem(i) && GetExtraInventoryItem(i)->GetID() == id)
			return GetExtraInventoryItem(i);
	}
#endif

	return nullptr;
}
int CHARACTER::CountSpecifyItemRenewal(uint32_t vnum) const
{

	int	count = 0;
	LPITEM item;


#ifdef ENABLE_EXTRA_INVENTORY
	if (ITEM_MANAGER::instance().IsExtraItem(vnum))
	{
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			item = GetExtraInventoryItem(i);

			if (item && item->GetVnum() == vnum)
			{
				if (item->GetLockedAttr() != -1) {
					continue;
				}

				if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
					continue;
				else
					count += item->GetCount();
			}
		}
	}
	else {
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		for (int i = 0; i < Inventory_Size(); ++i)
#else
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		{
			item = GetInventoryItem(i);
			if (nullptr != item && item->GetVnum() == vnum)
			{
				// ∞3AŒ ªÛ¡!?! µÓ∑IµE 1∞∞«AI∏È 3N3Ó∞L¥U.
				if (item->GetLockedAttr() != -1) {
					continue;
				}

				if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
				{
					continue;
				}
				else
				{
					count += item->GetCount();
				}
			}
		}
#ifdef ENABLE_EXTRA_INVENTORY
	}
#endif
	return count;

}

int CHARACTER::CountSpecifyItem(uint32_t vnum) const
{
	int	count = 0;
	LPITEM item;
#ifdef ENABLE_EXTRA_INVENTORY
	if (ITEM_MANAGER::instance().IsExtraItem(vnum))
	{
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			item = GetExtraInventoryItem(i);
			if (item && item->GetVnum() == vnum)
			{
				if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID())) {
					continue;
				}
				else {
					count += item->GetCount();
				}
			}
		}
	}
	else {
#endif


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		for (int i = 0; i < Inventory_Size(); ++i)
#else
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		{
			item = GetInventoryItem(i);
			if (nullptr != item && item->GetVnum() == vnum)
			{
				// ∞3AŒ ªÛ¡!?! µÓ∑IµE 1∞∞«AI∏È 3N3Ó∞L¥U.
				if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
				{
					continue;
				}
				else {
					count += item->GetCount();
				}
			}
		}
#ifdef ENABLE_EXTRA_INVENTORY
	}
#endif

	return count;
}

void CHARACTER::RemoveSpecifyItem(uint32_t vnum, int count, bool cuberenewal)
{
	if (0 == count)
		return;


#ifdef ENABLE_EXTRA_INVENTORY
	if (ITEM_MANAGER::instance().IsExtraItem(vnum))
	{
		for (uint16_t i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = GetExtraInventoryItem(i);

			if (!item)
				continue;

			if (item->GetVnum() != vnum)
				continue;

			if (m_pkMyShop)
			{
				if (m_pkMyShop->IsSellingItem(item->GetID()))
					continue;
			}

			if (cuberenewal) {
				if (item->GetLockedAttr() != -1) {
					continue;
				}
			}

			if (count >= item->GetCount())
			{
				count -= item->GetCount();
				item->SetCount(0);

				if (0 == count)
					return;
			}
			else
			{
				item->SetCount(item->GetCount() - count);
				return;
			}
		}
	}
	else
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		for (int i = 0; i < Inventory_Size(); ++i)
#else
		for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		{
			LPITEM item = GetInventoryItem(i);
			if (!item)
				continue;

			if (item->GetVnum() != vnum)
				continue;

			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
				continue;

			if (cuberenewal && item->GetLockedAttr() != -1)
				continue;

			if (vnum >= 80003 && vnum <= 80007)
				LogManager::instance().GoldBarLog(GetPlayerID(), item->GetID(), QUEST, "RemoveSpecifyItem");

			const int itemCount = item->GetCount();
			if (count >= itemCount)
			{
				count -= itemCount;
				item->SetCount(0);

				if (0 == count)
					return;
			}
			else
			{
				item->SetCount(itemCount - count);
				return;
			}
		}

	// ?1?‹A3∏Æ∞! 3a«I¥U.
	if (count)
		sys_log(0, "CHARACTER::RemoveSpecifyItem cannot remove enough item vnum %u, still remain %d", vnum, count);
}

int CHARACTER::CountSpecifyTypeItem(uint8_t type) const
{
	int	count = 0;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	for (int i = 0; i < Inventory_Size(); ++i)
#else
	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		LPITEM pItem = GetInventoryItem(i);
		if (pItem != nullptr && pItem->GetType() == type)
		{
			count += pItem->GetCount();
		}
	}

	return count;
}


LPITEM CHARACTER::GetWear(uint8_t bCell) const
{

	// > WEAR_MAX_NUM : ?ÎEY1Æ 11∑‘µÈ.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::GetWear: invalid wear cell %d", bCell);
		return nullptr;
	}

	return m_pointsInstant.pItems[INVENTORY_MAX_NUM + bCell];
}


void CHARACTER::SetWear(uint8_t bCell, LPITEM item)
{
	// > WEAR_MAX_NUM : ?ÎEY1Æ 11∑‘µÈ.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::SetItem: invalid item cell %d", bCell);
		return;
	}

#ifdef __HIGHLIGHT_SYSTEM__
	SetItem(TItemPos(INVENTORY, INVENTORY_MAX_NUM + bCell), item, false);
#else
	SetItem(TItemPos(INVENTORY, INVENTORY_MAX_NUM + bCell), item);
#endif

#ifndef ENABLE_BUG_FIXES
	if (!item && bCell == WEAR_WEAPON) {
		if (IsAffectFlag(AFF_GWIGUM))
			RemoveAffect(SKILL_GWIGEOM);

		if (IsAffectFlag(AFF_GEOMGYEONG))
			RemoveAffect(SKILL_GEOMKYUNG);
	}
#endif
}


bool CHARACTER::UnequipItem(LPITEM item)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: CHARACTER::UnequipItem ");//INGAME_DEBUG_RAZOR93
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	int iWearCell = item->FindEquipCell(this);
	if (iWearCell == WEAR_WEAPON)
	{
		LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
		if (costumeWeapon && !UnequipItem(costumeWeapon))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
			return false;
		}
	}
#elif defined(ENABLE_BUG_FIXES)
	int iWearCell = item->FindEquipCell(this);
#endif

	if (false == CanUnequipNow(item))
		return false;
	
	int pos;
	if (item->IsDragonSoul())
		pos = GetEmptyDragonSoulInventory(item);
	else
		pos = GetEmptyInventory(item->GetSize());

	// HARD CODING
	/*if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		ShowAlignment(true);*/

	item->RemoveFromCharacter();
	if (item->IsDragonSoul())
#ifdef __HIGHLIGHT_SYSTEM__
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, pos));
#endif
	else
#ifdef __HIGHLIGHT_SYSTEM__
		item->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
#else
		item->AddToCharacter(this, TItemPos(INVENTORY, pos));
#endif

	CheckMaximumPoints();
#ifdef ENABLE_BUG_FIXES
	if (iWearCell == WEAR_WEAPON) {
		if (IsAffectFlag(AFF_GWIGUM)) {
			RemoveAffect(SKILL_GWIGEOM);
		}

		if (IsAffectFlag(AFF_GEOMGYEONG)) {
			RemoveAffect(SKILL_GEOMKYUNG);
		}
	}
#endif
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	if (iWearCell == WEAR_BELT) {

		UpdateItemOnTitleName();
	}
#endif
	return true;
}


bool CHARACTER::EquipItem(LPITEM item, int iCandidateCell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: CHARACTER::UnequipItem ");// 1993
#endif
	if (item->IsExchanging())

		return false;

	if (false == item->IsEquipable())
		return false;

	if (false == CanEquipNow(item))
		return false;

	int iWearCell = item->FindEquipCell(this, iCandidateCell);

	if (iWearCell < 0)
		return false;

	// 1´3?∞!∏¶ Ao ªÛA¬?!1≠ AŒ1Aµµ A‘±‚ ±›¡ˆ
	if (iWearCell == WEAR_BODY && IsRiding() && (item->GetVnum() >= 11901 && item->GetVnum() <= 11904))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 693, "");
#endif
		return false;
	}

	if (iWearCell != WEAR_ARROW && IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 315, "");
#endif
		return false;
	}

	if (FN_check_item_sex(this, item) == false)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 496, "");
#endif
		return false;
	}

	//1A±‘ Aª∞Õ ªÁ?Î1A ±‚¡∏ ∏ª ªÁ?Î?©oŒ A1A©
	if (item->IsRideItem() && IsRiding())
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 532, "");
#endif
		return false;
	}

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	if (iWearCell == WEAR_WEAPON)
	{
		if (item->GetType() == ITEM_WEAPON)
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && costumeWeapon->GetValue(3) != item->GetSubType() && !UnequipItem(costumeWeapon))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
				return false;
			}
		}
		else //fishrod/pickaxe
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && !UnequipItem(costumeWeapon))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
				return false;
			}
		}
	}
	else if (iWearCell == WEAR_COSTUME_WEAPON)
	{
		if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_WEAPON)
		{
			LPITEM pkWeapon = GetWear(WEAR_WEAPON);
			if (!pkWeapon || pkWeapon->GetType() != ITEM_WEAPON || item->GetValue(3) != pkWeapon->GetSubType())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 694, "");
#endif
				return false;
			}
		}
	}
#endif

	// ?ÎEY1Æ Aó1ˆ A3∏Æ
	if (item->IsDragonSoul())
	{
		// ∞∞Ao A∏A‘A« ?ÎEY1ÆAI AI1I µÈ3Ó∞! A÷¥U∏È ¬o?Î«O 1ˆ 3o¥U.
		// ?ÎEY1ÆAo swapAª ¡ˆ?o«I∏È 3EµE.
		if (GetInventoryItem(INVENTORY_MAX_NUM + iWearCell))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 796, "");
#endif
			return false;
		}

		if (!item->EquipTo(this, iWearCell))
		{
			return false;
		}
	}
	// ?ÎEY1ÆAI 3A¥‘.
	else
	{
		// ¬o?Î«O ∞˜?! 3AAIAUAI A÷¥U∏È,
		if (GetWear(iWearCell) && !IS_SET(GetWear(iWearCell)->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			// AI 3AAIAUAo «N1o 1⁄E˜∏È oó∞a oO∞!. swap ?a1A ?IA¸ oO∞!
			if (item->GetWearFlag() == WEARABLE_ABILITY)
				return false;

			if (false == SwapItem(item->GetCell(), INVENTORY_MAX_NUM + iWearCell))
			{
				return false;
			}
		}
		else
		{
			uint8_t bOldCell = item->GetCell();

			if (item->EquipTo(this, iWearCell))
			{
				SyncQuickslot(QUICKSLOT_TYPE_ITEM, bOldCell, iWearCell);
			}
		}
	}

	if (true == item->IsEquipped())
	{
		// 3AAIAU A÷AE ªÁ?Î AIEƒoŒAÕ¥¬ ªÁ?Î«I¡ˆ 3E3Aµµ 1A∞LAI ¬˜∞®µ«¥¬ 1a1ƒ A3∏Æ.
		if (-1 != item->GetProto()->cLimitRealTimeFirstUseIndex)
		{
			// «N 1oAI∂Ûµµ ªÁ?Î«N 3AAIAUAŒ¡ˆ ?©oŒ¥¬ Socket1Aª o∏∞Ì A«¥‹«N¥U. (Socket1?! ªÁ?ÎE11ˆ ±‚∑I)
			if (0 == item->GetSocket(1))
			{
				// ªÁ?Î∞!¥…1A∞LAo Default ∞aA∏∑Œ Limit Value ∞aAª ªÁ?Î«Iµ«, Socket0?! ∞aAI A÷A∏∏È ±◊ ∞aAª ªÁ?Î«Iµµ∑I «N¥U. (¥‹Aß¥¬ AE)
				int32_t duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[(unsigned char)(item->GetProto()->cLimitRealTimeFirstUseIndex)].lValue;

				if (0 == duration)
					duration = 60 * 60 * 24 * 7;

				item->SetSocket(0, time(nullptr) + duration);
				item->StartRealTimeExpireEvent();
			}

			item->SetSocket(1, item->GetSocket(1) + 1);
		}

		/*if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
			ShowAlignment(false);*/

		const uint32_t& dwVnum = item->GetVnum();

		// ∂Û∏∂¥‹ AIoYAÆ AE1¬¥?A« 1›¡ˆ(71135) ¬o?Î1A AIAaAÆ 1ﬂµ?
		if (true == CItemVnumHelper::IsRamadanMoonRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_RAMADAN_RING);
		}
		// «O∑ŒA© ªÁA¡(71136) ¬o?Î1A AIAaAÆ 1ﬂµ?
		else if (true == CItemVnumHelper::IsHalloweenCandy(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HALLOWEEN_CANDY);
		}
		// «ao1A« 1›¡ˆ(71143) ¬o?Î1A AIAaAÆ 1ﬂµ?
		else if (true == CItemVnumHelper::IsHappinessRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HAPPINESS_RING);
		}
		// ªÁ∂uA« AO¥oAÆ(71145) ¬o?Î1A AIAaAÆ 1ﬂµ?
		else if (true == CItemVnumHelper::IsLovePendant(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_LOVE_PENDANT);
		}
		// ITEM_UNIQUEA« ∞a?i, SpecialItemGroup?! ¡§A«µ«3Ó A÷∞Ì, (item->GetSIGVnum() != NULL)
		//
		else if (ITEM_UNIQUE == item->GetType() && 0 != item->GetSIGVnum())
		{
			const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(item->GetSIGVnum());
			if (nullptr != pGroup)
			{
				const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(pGroup->GetAttrVnum(item->GetVnum()));
				if (nullptr != pAttrGroup)
				{
					const std::string& std = pAttrGroup->m_stEffectFileName;
					SpecificEffectPacket(std.c_str());
				}
			}
		}
#ifdef ENABLE_ACCE_SYSTEM
		else if ((item->GetType() == ITEM_COSTUME) && (item->GetSubType() == COSTUME_ACCE))
			this->EffectPacket(SE_EFFECT_ACCE_EQUIP);
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if ((item->GetType() == ITEM_COSTUME) && (item->GetSubType() == COSTUME_STOLE))
			this->EffectPacket(SE_EFFECT_ACCE_EQUIP);
#endif
#ifdef ENABLE_TALISMAN_EFFECT
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 9600 && item->GetVnum() <= 9800))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_FIRE);
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 9830 && item->GetVnum() <= 10030))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_ICE);
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 10520 && item->GetVnum() <= 10720))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_WIND);
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 10060 && item->GetVnum() <= 10260))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_EARTH);
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 10290 && item->GetVnum() <= 10490))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_DARK);
		else if (/*(item->GetType() == ITEM_ARMOR) && (item->GetWearFlag() ==WEARABLE_PENDANT) && */(item->GetVnum() >= 10750 && item->GetVnum() <= 10950))
			this->EffectPacket(SE_EFFECT_TALISMAN_EQUIP_ELEC);
#endif

		if (
			(ITEM_UNIQUE == item->GetType() && UNIQUE_SPECIAL_RIDE == item->GetSubType() && IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE))
			|| (ITEM_UNIQUE == item->GetType() && UNIQUE_SPECIAL_MOUNT_RIDE == item->GetSubType() && IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE))
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			|| (ITEM_COSTUME == item->GetType() && COSTUME_MOUNT == item->GetSubType())
#endif
			)
		{
			quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
		}

	}
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	// Automatikus mount aktivalas, ha felszereltek a mountot
	if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_MOUNT)
	{
		CMountSystem* mountSystem = GetMountSystem();
		if (mountSystem)
		{
			uint32_t mountVnum = item->GetValue(1);
			mountSystem->Mount(mountVnum, item);
		}
	}
#endif

	return true;
}


bool CHARACTER::IsEquipUniqueItem(uint32_t dwItemVnum) const
{
	{
		LPITEM u = GetWear(WEAR_UNIQUE1);

		if (u && u->GetVnum() == dwItemVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_UNIQUE2);

		if (u && u->GetVnum() == dwItemVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_COSTUME_MOUNT);

		if (u && u->GetVnum() == dwItemVnum)
			return true;
	}

	// 3?3Ó1›¡ˆAŒ ∞a?i 3?3Ó1›¡ˆ(∞ﬂoª) AŒ¡ˆµµ A1A©«N¥U.
	if (dwItemVnum == UNIQUE_ITEM_RING_OF_LANGUAGE)
		return IsEquipUniqueItem(UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);

	return false;
}


bool CHARACTER::IsEquipUniqueGroup(uint32_t dwGroupVnum) const
{
	{
		LPITEM u = GetWear(WEAR_UNIQUE1);

		if (u && u->GetSpecialGroup() == (int)dwGroupVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_UNIQUE2);

		if (u && u->GetSpecialGroup() == (int)dwGroupVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_COSTUME_MOUNT);

		if (u && u->GetSpecialGroup() == (int)dwGroupVnum)
			return true;
	}

	return false;
}


bool CHARACTER::UnEquipSpecialRideUniqueItem()
{
	LPITEM Unique1 = GetWear(WEAR_UNIQUE1);
	LPITEM Unique2 = GetWear(WEAR_UNIQUE2);
	LPITEM Unique3 = GetWear(WEAR_COSTUME_MOUNT);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	LPITEM MountCostume = GetWear(WEAR_COSTUME_MOUNT);
#endif


	if (nullptr != Unique1)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == Unique1->GetSpecialGroup())
		{
			return UnequipItem(Unique1);
		}
	}

	if (nullptr != Unique2)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == Unique2->GetSpecialGroup())
		{
			return UnequipItem(Unique2);
		}
	}

	if (nullptr != Unique3)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == Unique3->GetSpecialGroup())
		{
			return UnequipItem(Unique3);
		}
	}

	/*#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (MountCostume)
			return UnequipItem(MountCostume);
	#endif*/

	return true;
}


bool CHARACTER::CanEquipNow(const LPITEM item, const TItemPos & srcCell, const TItemPos & destCell) /*const*/
{
	const TItemTable* itemTable = item->GetProto();
	//uint8_t itemType = item->GetType();
	//uint8_t itemSubType = item->GetSubType();

#ifdef ENABLE_PVP_ADVANCED
	if ((GetDuel("BlockChangeItem")))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif

	switch (GetJob())
	{
	case JOB_WARRIOR:
		if (item->GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
			return false;
		break;

	case JOB_ASSASSIN:
		if (item->GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
			return false;
		break;

	case JOB_SHAMAN:
		if (item->GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
			return false;
		break;

	case JOB_SURA:
		if (item->GetAntiFlag() & ITEM_ANTIFLAG_SURA)
			return false;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		if (item->GetAntiFlag() & ITEM_ANTIFLAG_WOLFMAN)
			return false;
		break; // TODO: 1ˆAŒ¡∑ 3AAIAU ¬o?Î∞!¥…?©oŒ A3∏Æ
#endif
	}

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limit = itemTable->aLimits[i].lValue;
		switch (itemTable->aLimits[i].bType)
		{
		case LIMIT_LEVEL:
			if (GetLevel() < limit) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 325, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_STR:
			if (GetPoint(POINT_ST) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 269, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_INT:
			if (GetPoint(POINT_IQ) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 468, "%d", limit);
#endif
				return false;
			}
			break;
		case LIMIT_DEX:
			if (GetPoint(POINT_DX) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 352, "%d", limit);
#endif
				return false;
			}
			break;

		case LIMIT_CON:
			if (GetPoint(POINT_HT) < limit) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 481, "%d", limit);
#endif
				return false;
			}
			break;
		}
	}

	if (item->GetWearFlag() & WEARABLE_UNIQUE)
	{
		const bool bAllowDualUnique =
			item->GetSubType() == 4 ||
			item->GetSubType() == 5;

		if (!bAllowDualUnique &&
			((GetWear(WEAR_UNIQUE1) && GetWear(WEAR_UNIQUE1)->IsSameSpecialGroup(item)) ||
				(GetWear(WEAR_UNIQUE2) && GetWear(WEAR_UNIQUE2)->IsSameSpecialGroup(item)) ||
				(GetWear(WEAR_COSTUME_MOUNT) && GetWear(WEAR_COSTUME_MOUNT)->IsSameSpecialGroup(item))))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 695, "");
#endif
			return false;
		}

		if (marriage::CManager::instance().IsMarriageUniqueItem(item->GetVnum()) &&
			!marriage::CManager::instance().IsMarried(GetPlayerID()))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 696, "");
#endif
			return false;
		}
	}

#ifdef ENABLE_BUG_FIXES
	if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_BODY)
	{
		LPITEM atakanxd = GetWear(WEAR_BODY);
		if (atakanxd && (atakanxd->GetVnum() >= 11901 && atakanxd->GetVnum() <= 11914))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 1129, "");
#endif
			return false;
		}
	}

	if (item->GetVnum() >= 11901 && item->GetVnum() <= 11914)
	{
		LPITEM atakan = GetWear(WEAR_COSTUME_BODY);
		if (atakan && (atakan->GetType() == ITEM_COSTUME && atakan->GetSubType() == COSTUME_BODY))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 1129, "");
#endif
			return false;
		}
	}
#endif

#ifdef ENABLE_DS_SET
	if ((DragonSoul_IsDeckActivated()) && (item->IsDragonSoul())) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 76, "");
#endif
		return false;
	}
#endif

	return true;
}


bool CHARACTER::CanUnequipNow(const LPITEM item, const TItemPos & srcCell, const TItemPos & destCell) {
	if (ITEM_BELT == item->GetType() && CBeltInventoryHelper::IsExistItemInBeltInventory(this)) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
		return false;
	}

	// ?µ?oE˜ «O¡¶«O 1ˆ 3o¥¬ 3AAIAU
	if (IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	// 3AAIAU unequip1A AŒoYA‰∏Æ∑Œ ?A±a ∂ß oÛ A⁄∏Æ∞! A÷¥¬ ¡ˆ EÆAŒ
	{
		int pos = -1;

		if (item->IsDragonSoul())
			pos = GetEmptyDragonSoulInventory(item);
		else
			pos = GetEmptyInventory(item->GetSize());

		if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
			return false;
		}
	}

#ifdef ENABLE_DS_SET
	if ((DragonSoul_IsDeckActivated()) && (item->IsDragonSoul())) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 76, "");
#endif
		return false;
	}
#endif

	return true;
}




// char_item.cpp slice C1 moved into ItemSystem.cpp

namespace NPartyPickupDistribute
{
	struct FFindOwnership
	{
		LPITEM item;
		LPCHARACTER owner;

		FFindOwnership(LPITEM item)
			: item(item), owner(nullptr)
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (item->IsOwnership(ch))
				owner = ch;
		}
	};

	struct FCountNearMember
	{
		int		total;
		int		x, y;

		FCountNearMember(LPCHARACTER center)
			: total(0), x(center->GetX()), y(center->GetY())
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct FMoneyDistributor
	{
		int		total;
		LPCHARACTER	c;
		int		x, y;
		int64_t		iMoney;

		FMoneyDistributor(LPCHARACTER center, int64_t iMoney)
			: total(0), c(center), x(center->GetX()), y(center->GetY()), iMoney(iMoney)
		{
		}

		void operator ()(LPCHARACTER ch)
		{
			if (ch != c)
				if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				{
					ch->PointChange(POINT_GOLD, iMoney, true);

					if (iMoney > 1000) // √É¬µ¬ø√∏ √Ä√å¬ª√≥¬∏¬∏ ¬±√¢¬∑√è√á√ë¬¥√ô.
					{
						LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(ch, iMoney, "GET_GOLD", ""));
					}
				}
		}
	};
}

bool CHARACTER::DropItem(TItemPos Cell,
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
	bCount)
{
	bool stupid = false;
	if (bCount < 0)
	{
		sys_err("I am a stupid hacker 1: %s %d", GetName(), bCount);
		stupid = true;
	}

	bCount = abs(bCount);
	if (stupid)
	{
		sys_err("I am a stupid hacker 2: %s %d", GetName(), bCount);
		return false;
	}

	LPITEM item = nullptr;

	if (!CanHandleItem())
	{
#ifdef TEXTS_IMPROVEMENT
		if (nullptr != DragonSoul_RefineWindow_GetOpener()) {
			ChatPacketNew(CHAT_TYPE_INFO, 232, "");
		}
#endif

		return false;
	}

#ifdef ENABLE_ANTICHEAT
	if (thecore_pulse() > m_lastdropitem + 25)
	{
		m_dropitemcount = 0;
	}

	if (thecore_pulse() < m_lastdropitem + 25 && m_dropitemcount >= 4)
	{
		m_dropitemcount = 0;
		LPDESC desc = GetDesc();
		if (desc)
		{
			LogManager::instance().HackLog("DROP_HACK", this);
			desc->SetPhase(PHASE_CLOSE);
		}

		return false;
	}
#endif

	if (IsDead())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	if (item->isLocked() || item->IsExchanging() || item->IsEquipped())
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 353, "");
#endif
		return false;
	}

	if (bCount == 0 || bCount > item->GetCount())
		bCount = item->GetCount();

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem()) {
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (item->IsExtraItem()) {");//INGAME_DEBUG_RAZOR93

		sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line 8391 if (item->IsExtraItem()) { ");

#endif
		SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, 255);
	}
	else {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
	}
#else
	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
#endif

	LPITEM pkItemToDrop;

	if (bCount == item->GetCount())
	{
		item->RemoveFromCharacter();
		pkItemToDrop = item;
	}
	else
	{
		if (bCount == 0)
		{
			if (test_server)
				sys_log(0, "[DROP_ITEM] drop item count == 0");
			return false;
		}

		item->SetCount(item->GetCount() - bCount);
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		pkItemToDrop = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), bCount);

		// copy item socket -- by mhh
		FN_copy_item_socket(pkItemToDrop, item);

		char szBuf[51 + 1];
		snprintf(szBuf, sizeof(szBuf), "%u %u", pkItemToDrop->GetID(), pkItemToDrop->GetCount());
		LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
	}

	PIXEL_POSITION pxPos = GetXYZ();

	if (pkItemToDrop->AddToGround(GetMapIndex(), pxPos))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 321, "%d",
#ifdef ENABLE_NEWSTUFF
			g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]
#else
			300
#endif
		);
#endif
		pkItemToDrop->StartDestroyEvent(
#ifdef ENABLE_NEWSTUFF
			g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]
#endif
		);

		ITEM_MANAGER::instance().FlushDelayedSave(pkItemToDrop);

		char szHint[32 + 1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", pkItemToDrop->GetName(), pkItemToDrop->GetCount(), pkItemToDrop->GetOriginalVnum());
		LogManager::instance().ItemLog(this, pkItemToDrop, "DROP", szHint);
		//Motion(MOTION_PICKUP);
#ifdef ENABLE_ANTICHEAT
		m_lastdropitem = thecore_pulse();
		m_dropitemcount++;
#endif
	}

	return true;
}

bool CHARACTER::DropGold(int64_t gold)
{
	if (gold <= 0 || gold > GetGold())
		return false;

	if (!CanHandleItem())
		return false;

	if (0 != g_GoldDropTimeLimitValue)
	{
		if (get_dword_time() < m_dwLastGoldDropTime + g_GoldDropTimeLimitValue)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 510, "");
#endif
			return false;
		}
	}

	m_dwLastGoldDropTime = get_dword_time();

	LPITEM item = ITEM_MANAGER::instance().CreateItem(1, gold);

	if (item)
	{
		PIXEL_POSITION pos = GetXYZ();

		if (item->AddToGround(GetMapIndex(), pos))
		{
			//Motion(MOTION_PICKUP);
			PointChange(POINT_GOLD, -gold, true);

			if (gold > 1000) // √É¬µ¬ø√∏ √Ä√å¬ª√≥¬∏¬∏ ¬±√¢¬∑√è√á√ë¬¥√ô.
				LogManager::instance().CharLog(this, gold, "DROP_GOLD", "");

#ifdef ENABLE_NEWSTUFF
			item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);
#else
			item->StartDestroyEvent();
#endif
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 321, "%d", (150 / 60));
#endif
		}

		Save();
		return true;
	}

	return false;
}

bool CHARACTER::MoveItem(TItemPos Cell, TItemPos DestCell,
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
	count)

{
	//ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::MoveItem called.");
	bool stupid = false;
	if (count < 0)
	{
		sys_err("I am a stupid hacker 3: %s %d", GetName(), count);
		stupid = true;
	}

	count = abs(count);
	if (stupid)
	{
		sys_err("I am a stupid hacker 4: %s %d", GetName(), count);
		return false;
	}

	if (Cell.cell == DestCell.cell && Cell.window_type == DestCell.window_type)
	{
		return false;
	}


	LPITEM item = nullptr;
	if (!IsValidItemPosition(Cell))
		return false;
	// Belt inventoryol nem lehet safeboxba vagy mallba helyezni
	if (Cell.IsBeltInventoryPosition() &&
		(DestCell.window_type == SAFEBOX || DestCell.window_type == MALL))
	{
		ChatPacket(CHAT_TYPE_INFO, "You cannot place items from the mount inventory into the storage.");
		//sys_log(0, "BELT_TO_SAFEBOX_BLOCKED: FROM belt cell=%d TO window=%d, cell=%d", Cell.cell, DestCell.window_type, DestCell.cell);
		// BELT INVENTORY: csak KIVENNI lehessen, BERAKNI TILOS
		//if (DestCell.IsBeltInventoryPosition() && !Cell.IsBeltInventoryPosition())
		//{
		//	ChatPacket(CHAT_TYPE_INFO, "Belt inventory is disabled. You can only take items out.");//ezt majd be kell kapcsolni ha beker√ºl √©lesre
		//	return false;
		//}

		return false;
	}

	if (DestCell.IsBeltInventoryPosition())
	{
		if (!IsValidItemPosition(DestCell))
		{
			sys_err("BELT_SLOT_INVALID: window=%d, cell=%d", DestCell.window_type, DestCell.cell);
			return false;
		}

		LPITEM targetItem = GetItem(DestCell);
		if (targetItem)
		{
			//sys_log(0, "BELT_SLOT_OCCUPIED: Attempt to move item to occupied slot cell=%d", DestCell.cell);
			ChatPacket(CHAT_TYPE_INFO, "This place is already taken.");
			return false;
		}
	}




	if (!(item = GetItem(Cell)))
		return false;

	// Duplikacio ellen?rzes belt inventoryba mozgataskor
	if (DestCell.IsBeltInventoryPosition())
	{
		for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
		{
			LPITEM beltItem = GetInventoryItem(i);
			if (!beltItem)
				continue;

			if (beltItem->GetVnum() == item->GetVnum() && beltItem != item)
			{
				ChatPacket(CHAT_TYPE_INFO, "This mount already added,you can't add two time!");
				return false;
			}
		}
		if (item && item->GetProto())
		{
			const TItemLimit& limit = item->GetProto()->aLimits[0];
			if (limit.bType == LIMIT_LEVEL && GetLevel() < limit.lValue)
			{
				ChatPacket(CHAT_TYPE_INFO, "You need to be at least level %d to equip this item.", limit.lValue);
				return false;
			}
		}

		const uint32_t vnum = item->GetVnum();

		if (vnum >= 18000 && vnum <= 18159)
		{
			// Hat√°rozd meg a t√≠pus azonos√≠t√≥t (pl. 18000‚Äì18009 = 1800, 18010‚Äì18019 = 1801 stb.)
			const uint32_t itemGroup = vnum / 10;

			for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
			{
				LPITEM beltItem = GetInventoryItem(i);
				if (!beltItem)
					continue;

				const uint32_t otherVnum = beltItem->GetVnum();
				const uint32_t otherGroup = otherVnum / 10;

				if (itemGroup == otherGroup && beltItem != item)
				{
					ChatPacket(CHAT_TYPE_INFO, "You already have a belt of this type in your inventory.");
					return false;
				}
			}
		}
	}


	if (item->IsExchanging())
		return false;

	if (item->GetCount() < count)
		return false;

	if (INVENTORY == Cell.window_type && Cell.cell >= INVENTORY_MAX_NUM && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	if (true == item->isLocked())
		return false;

	if (!IsValidItemPosition(DestCell))
		return false;

	if (!CanHandleItem())
	{
#ifdef TEXTS_IMPROVEMENT
		if (nullptr != DragonSoul_RefineWindow_GetOpener()) {
			ChatPacketNew(CHAT_TYPE_INFO, 232, "");
		}
#endif

		return false;
	}

	// ¬±√¢√à¬π√Ä√ö√Ä√á ¬ø√§√É¬ª√Ä¬∏¬∑√é ¬∫¬ß√Ü¬Æ √Ä√é¬∫¬•√Ö√§¬∏¬Æ¬ø¬°¬¥√Ç √Ü¬Ø√Å¬§ √Ö¬∏√Ä√î√Ä√á ¬æ√Ü√Ä√å√Ö√õ¬∏¬∏ ¬≥√ñ√Ä¬ª ¬º√∂ √Ä√ñ¬¥√ô.
	if (DestCell.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(item))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacket(CHAT_TYPE_INFO, "Belt Only // Csak √∂veket tehetsz ide.");
#endif
		return false;
	}

#ifdef ENABLE_SWITCHBOT
	if (Cell.IsSwitchbotPosition() && CSwitchbotManager::Instance().IsActive(GetPlayerID(), Cell.cell))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 690, "");
#endif
		return false;
	}

	if ((DestCell.IsSwitchbotPosition() && item->IsEquipped()) || (Cell.IsSwitchbotPosition() && DestCell.IsEquipPosition()))
	{
		return false;
	}

	if (DestCell.IsSwitchbotPosition() && !SwitchbotHelper::IsValidItem(item))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 691, "");
#endif
		return false;
	}
#endif
	// √Ä√å¬π√å √Ç√∏¬ø√´√Å√ü√Ä√é ¬æ√Ü√Ä√å√Ö√õ√Ä¬ª ¬¥√ô¬∏¬• ¬∞√∑√Ä¬∏¬∑√é ¬ø√Ö¬±√¢¬¥√Ç ¬∞√¶¬ø√¨, '√Ä√•√É¬• √á√ò√Å¬¶' ¬∞¬°¬¥√â√á√ë √Å√∂ √à¬Æ√Ä√é√á√è¬∞√≠ ¬ø√Ö¬±√®

	if (Cell.IsEquipPosition())
	{
		if (!CanUnequipNow(item))
			return false;

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		int iWearCell = item->FindEquipCell(this);
		if (iWearCell == WEAR_WEAPON)
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && !UnequipItem(costumeWeapon))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
				return false;
			}

			if (!IsEmptyItemGrid(DestCell, item->GetSize(), Cell.cell))
				return UnequipItem(item);
		}
#endif
	}

#ifdef ENABLE_EXTRA_INVENTORY
	if (!item->IsExtraItem() && DestCell.IsEquipPosition())

#else
	if (DestCell.IsEquipPosition())
#endif
	{
		if (GetItem(DestCell))	// √Ä√•¬∫√±√Ä√è ¬∞√¶¬ø√¨ √á√ë ¬∞√∑¬∏¬∏ ¬∞√ã¬ª√ß√á√ò¬µ¬µ ¬µ√à¬¥√ô.
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 538, "");
#endif
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:	if (GetItem(DestCell))	// √Ä√•¬∫√±√Ä√è ¬∞√¶¬ø√¨ √á√ë ¬∞√∑¬∏¬∏ ¬∞√ã¬ª√ß√á√ò¬µ¬µ ¬µ√à¬¥√ô.");//INGAME_DEBUG_RAZOR93

			sys_log(0, "Razor93 LOG:: Called: Char_item.cpp 	if (GetItem(DestCell))	// √Ä√•¬∫√±√Ä√è ¬∞√¶¬ø√¨ √á√ë ¬∞√∑¬∏¬∏ ¬∞√ã¬ª√ß√á√ò¬µ¬µ ¬µ√à¬¥√ô. ");

#endif
			return false;
		}

		EquipItem(item, DestCell.cell - INVENTORY_MAX_NUM);
	}
	else
	{
		if (item->IsDragonSoul())
		{
			if (item->IsEquipped())
			{
				return DSManager::instance().PullOut(this, DestCell, item);
			}
			else
			{
				if (DestCell.window_type != DRAGON_SOUL_INVENTORY)
				{
					return false;
				}

				if (!DSManager::instance().IsValidCellForThisItem(item, DestCell))
				{
					return false;
				}
			}
		}
		else if (DestCell.window_type == DRAGON_SOUL_INVENTORY)
			return false;

		// ¬ø√´√à¬•¬º¬Æ√Ä√å ¬æ√Ü¬¥√ë ¬æ√Ü√Ä√å√Ö√õ√Ä¬∫ ¬ø√´√à¬•¬º¬Æ √Ä√é¬∫¬•¬ø¬° ¬µ√©¬æ√Æ¬∞¬• ¬º√∂ ¬æ√∏¬¥√ô.
#ifdef ENABLE_EXTRA_INVENTORY
		if (item->IsExtraItem())
		{
			if (DestCell.window_type != EXTRA_INVENTORY)
				return false;

			uint8_t category = item->GetExtraCategory();
			if (DestCell.cell < category * EXTRA_INVENTORY_CATEGORY_MAX_NUM || DestCell.cell >= (category + 1) * EXTRA_INVENTORY_CATEGORY_MAX_NUM)
				return false;
		}
		else if (DestCell.window_type == EXTRA_INVENTORY)
			return false;
#else
		else if (DRAGON_SOUL_INVENTORY == DestCell.window_type)
			return false;
#endif

		LPITEM item2;

		if ((item2 = GetItem(DestCell)) && item != item2 && item2->IsStackable() &&
			!IS_SET(item2->GetAntiFlag(), ITEM_ANTIFLAG_STACK) &&
			item2->GetVnum() == item->GetVnum()) // √á√ï√Ñ¬• ¬º√∂ √Ä√ñ¬¥√Ç ¬æ√Ü√Ä√å√Ö√õ√Ä√á ¬∞√¶¬ø√¨
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->GetSocket(i) != item->GetSocket(i))
					return false;

			if (count == 0)
				count = item->GetCount();

			sys_log(0, "%s: ITEM_STACK %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell,
				DestCell.window_type, DestCell.cell, count);

			count = std::min(g_bItemCountLimit - item2->GetCount(), count);

			item->SetCount(item->GetCount() - count);
			item2->SetCount(item2->GetCount() + count);
			return true;
		}

		if (!IsEmptyItemGrid(DestCell, item->GetSize(), Cell.cell))
			return false;
		static const std::set<uint32_t> mount_bonus_items = {
			14590, 14591, 14592, 14593, 52040, 60001, 48421, 49009,
			49049, 60003, 71223, 71253, 71224, 71228, 71251, 71125,
			71126, 71127, 71139, 71166, 71171, 71176, 71177, 71221,
			71222, 71252, 71256, 71225, 71226, 71227, 71255, 71254, 71233, 71250, 71128, 23014, 23015, 23016, 71137, 71140, 71185,
			// √ñvek: 18000 - 18119
			18000, 18001, 18002, 18003, 18004, 18005, 18006, 18007, 18008, 18009,
			18010, 18011, 18012, 18013, 18014, 18015, 18016, 18017, 18018, 18019,
			18020, 18021, 18022, 18023, 18024, 18025, 18026, 18027, 18028, 18029,
			18030, 18031, 18032, 18033, 18034, 18035, 18036, 18037, 18038, 18039,
			18040, 18041, 18042, 18043, 18044, 18045, 18046, 18047, 18048, 18049,
			18050, 18051, 18052, 18053, 18054, 18055, 18056, 18057, 18058, 18059,
			18060, 18061, 18062, 18063, 18064, 18065, 18066, 18067, 18068, 18069,
			18070, 18071, 18072, 18073, 18074, 18075, 18076, 18077, 18078, 18079,
			18080, 18081, 18082, 18083, 18084, 18085, 18086, 18087, 18088, 18089,
			18090, 18091, 18092, 18093, 18094, 18095, 18096, 18097, 18098, 18099,
			18100, 18101, 18102, 18103, 18104, 18105, 18106, 18107, 18108, 18109,
			18110, 18111, 18112, 18113, 18114, 18115, 18116, 18117, 18118, 18119,
			18120, 18121, 18122, 18123, 18124, 18125, 18126, 18127, 18128, 18129,
			18130, 18131, 18132, 18133, 18134, 18135, 18136, 18137, 18138, 18139,
			//k√°rty√°k: 18140 - 18149
			18140, 18141, 18142, 18143, 18144, 18145, 18146, 18147, 18148, 18149,
			18150, 18151, 18152, 18153, 18154, 18155, 18156, 18157, 18158, 18159

			// uj mountok 
			,611500, 611501, 611502, 611503, 611504, 611505, 611506, 611507, 611508,
			611510, 611511, 611512, 611513, 611514, 611515, 611516, 611517, 611518,
			611520, 611521, 611522, 611523, 611524, 611525, 611526, 611527, 611528,
			611530, 611531, 611532, 611533, 611534, 611535, 611536, 611537, 611538,
			611540, 611541, 611542, 611543, 611544,
						611545,
			611546,
			611547,
			611548,
			611549,
			611550,
			611551,
			611552,
			611553,
			611554,
			611555,
			611556,
			611557,
			611558,
			611559,
			611560,
			611561,
			611562,
			611563,
			611564,
			611565,
			611566,
			611567,
			611568,
			611569,
			611570,
			611571,
			611572,
			611573,
			611574,
			611575,
			611576,
			611577,
			611578,
			611579,
			611580,
			611581,
			611582,
			611583,
			611584,
			611585,
			611586,
			611587,
			611588,
			611589,
			611590,
			611591,
			611592,
			611593,
			611594,
			611595,
			611596,
			611597,
						611598,
			611599,
			611600,
			611601,
			611602,
			611603,
			611604,
			611605,
			611606,
			611607,
			611608,
			611609,
			611610,
			611611,
			611612,
			611613,
			611614,
			611615,
			611616,
			611617,
			611618,
			611619,
			611620,
			611621,
			611622,
			611623,
			611624,
			611625,
			611626,
			611627,
			611628,
			611629,
			611630,
			611631,
			611632,
			611633,
			611634,
			611635,
			611636,
			611637,
			611638,
			611639,
			611640,
			611641,
			611642,
			611643,
			611644,
			611645,
			611646,
			611647,
			611648,
			611649,
			611650,
			611651,
			611652,
			611653,
			611654,
			611655,
			611656,
			611657,
			611658,
			611659,
			611660,
			611661,
			611662,
			611663,
			611664,
			611665,
			611666
		};

		if (count == 0 || count >= item->GetCount() || !item->IsStackable() || IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
		{
			//sys_log(0, "%s: ITEM_MOVE %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d",
				//GetName(), item->GetName(), Cell.window_type, Cell.cell, DestCell.window_type, DestCell.cell, count);

			// Ha itemet a belt inventorybol mozditjuk el, es az mount bonuszos, toroljuk az affectet
			if (Cell.IsBeltInventoryPosition() && mount_bonus_items.count(item->GetVnum()))
			{
				RemoveAffect(AFFECT_MOUNT_BONUS);
				//sys_log(0, "BELT_MOUNT: affect removed (item taken from belt inventory) vnum: %u", item->GetVnum());
			}


			item->RemoveFromCharacter();
			SetItem(DestCell, item

#ifdef __HIGHLIGHT_SYSTEM__
				, false
#endif
			);
			//		// Ha az item a belt inventoryba kerult, frissitsuk a pontokat
			//		if (DestCell.cell >= BELT_INVENTORY_SLOT_START && DestCell.cell < BELT_INVENTORY_SLOT_END)
			//		{
			//			//sys_log(0, "DEBUG: ComputePoints hivas belt inventory item mozgatas utan");
			//			//ComputePoints();
			//			//UpdatePacket();
			//			//GetDisplayedNameWithBeltCount();
			//#ifdef ENABLE_FAKE_SHOP_HEADER
			//			UpdateMountCountOverhead();
			//			ChatPacket(CHAT_TYPE_INFO, "item beker√ºlt!");
			//#endif
			//		}
					// Ha belt inventory-ba mozgattak, vagy onnan el, akkor ujraszamolas
			if (Cell.IsBeltInventoryPosition() || DestCell.IsBeltInventoryPosition())
			{
				//sys_log(0, "DEBUG: Belt inventory valtozas - ComputePoints ujrahivas");
				ComputePoints();
				//UpdatePacket();
#ifdef ENABLE_FAKE_SHOP_HEADER
		// Friss√≠t√©s saj√°t magunknak
				UpdateMountInventoryCountOverhead(this);
				//SendLeaderboardData();
				SendLeaderboardDataSkillMob(this);

				// Friss√≠t√©s a k√∂r√ºl√∂tt√ºnk l√©v≈ë j√°t√©kosoknak
				for (const auto& it : m_map_view)
				{
					LPENTITY ent = it.first;
					if (!ent || !ent->IsType(ENTITY_CHARACTER))
						continue;

					LPCHARACTER viewer = (LPCHARACTER)ent;
					if (viewer == this)
						continue;

					if (viewer->IsPC() && viewer->GetDesc())
						UpdateMountInventoryCountOverhead(viewer);
				}
#endif


			}



#ifdef ENABLE_EXTRA_INVENTORY
			else if (EXTRA_INVENTORY == Cell.window_type && EXTRA_INVENTORY == DestCell.window_type) {
				SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, DestCell.cell);

				//ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:else if (EXTRA_INVENTORY == Cell.window_type && EXTRA_INVENTORY == DestCell.window_type)");//INGAME_DEBUG_RAZOR93

				//sys_log(0, "Razor93 LOG:: Called: Char_item.cpp else if (EXTRA_INVENTORY == Cell.window_type && EXTRA_INVENTORY == DestCell.window_type) ");


			}
#endif
		}
		else if (count < item->GetCount())
		{

			sys_log(0, "%s: ITEM_SPLIT %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell,
				DestCell.window_type, DestCell.cell, count);

			item->SetCount(item->GetCount() - count);
			LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), count);

			// copy socket -- by mhh
			FN_copy_item_socket(item2, item);

			item2->AddToCharacter(this, DestCell
#ifdef __HIGHLIGHT_SYSTEM__
				, false
#endif
			);

			char szBuf[51 + 1];
			snprintf(szBuf, sizeof(szBuf), "%u %u %u %u ", item2->GetID(), item2->GetCount(), item->GetCount(), item->GetCount() + item2->GetCount());
			LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if (EXTRA_INVENTORY == Cell.window_type && EXTRA_INVENTORY == DestCell.window_type) {
			SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, DestCell.cell);
		}
#ifdef ENABLE_EXTRA_INVENTORY
		else if ((Cell.window_type == EXTRA_INVENTORY && DestCell.window_type == INVENTORY) ||
			(Cell.window_type == INVENTORY && DestCell.window_type == EXTRA_INVENTORY)) {
			SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, DestCell.cell);
		}
#endif

#endif
	}

	return true;
}

void CHARACTER::GiveGold(int64_t iAmount)
{
	if (iAmount <= 0)
		return;

	sys_log(0, "GIVE_GOLD: %s %lld", GetName(), iAmount);
	//#ifdef TEXTS_IMPROVEMENT
	//	ChatPacketNew(CHAT_TYPE_INFO, 3, "%lld", iAmount);
	//#endif

#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = GetBattlePassId();
	if (bBattlePassId)
	{
		uint32_t dwYangCount, dwNotUsed;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, FARM_YANG, &dwNotUsed, &dwYangCount))
		{
			if (GetMissionProgress(FARM_YANG, bBattlePassId) < dwYangCount)
				UpdateMissionProgress(FARM_YANG, bBattlePassId, iAmount, dwYangCount);
		}
	}
#endif

	/*
	// PARTY GOLD SPLIT -  kikommentelve


	if (GetParty())
	{
		LPPARTY pParty = GetParty();

		int64_t dwTotal = iAmount;
		int64_t dwMyAmount = dwTotal;

		NPartyPickupDistribute::FCountNearMember funcCountNearMember(this);
		pParty->ForEachOnlineMember(funcCountNearMember);

		if (funcCountNearMember.total > 1)
		{
			int64_t dwShare = dwTotal / funcCountNearMember.total;
			dwMyAmount -= dwShare * (funcCountNearMember.total - 1);

			NPartyPickupDistribute::FMoneyDistributor funcMoneyDist(this, dwShare);
			pParty->ForEachOnlineMember(funcMoneyDist);
		}

		PointChange(POINT_GOLD, dwMyAmount, true);

		if (dwMyAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, dwMyAmount, "GET_GOLD", ""));
		}
	}
	else
	{
		PointChange(POINT_GOLD, iAmount, true);

		if (iAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, iAmount, "GET_GOLD", ""));
		}
	}
	*/

	// Mindig csak az kapja a goldot, akihez a GiveGold() meghivodik
	PointChange(POINT_GOLD, iAmount, true);

	//if (iAmount > 1000)
	//{
	//	LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, iAmount, "GET_GOLD", ""));
	//}
}

bool CHARACTER::PickupItem(uint32_t dwVID)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::PickupItem ");//INGAME_DEBUG_RAZOR93
#endif
	if (!IsPC() || IsDead() || IsObserverMode())
	{
		return false;
	}

	LPITEM item = ITEM_MANAGER::instance().FindByVID(dwVID);
	if (!item || !item->GetSectree())
		return false;

#ifdef ENABLE_BATTLE_PASS
	bool bIsBattlePass = item->HaveOwnership();
#endif

	if (item->DistanceValid(this))
	{
		// @fixme150 BEGIN
		if (item->GetType() == ITEM_QUEST)
		{
			if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 692, "");
#endif
				return false;
			}
		}
		// @fixme150 END

		if (item->IsOwnership(this))
		{
			// ¬∏¬∏¬æ√† √Å√ñ√Ä¬∏¬∑√Å √á√è¬¥√Ç ¬æ√Ü√Ä√å√Ö√õ√Ä√å ¬ø¬§√Ö¬©¬∂√≥¬∏√©
			if (item->GetType() == ITEM_ELK)
			{
				GiveGold((int64_t)item->GetCount());
				item->RemoveFromGround();
#ifdef ENABLE_RANKING
				SetRankPoints(10, GetRankPoints(10) + item->GetCount());
#endif
				M2_DESTROY_ITEM(item);

				Save();
			}
			// √Ü√≤¬π√º√á√ë ¬æ√Ü√Ä√å√Ö√õ√Ä√å¬∂√≥¬∏√©
			else
			{
#ifdef ENABLE_EXTRA_INVENTORY
				if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
				{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
					ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:else if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag()..");//INGAME_DEBUG_RAZOR93

					sys_log(0, "Razor93 LOG:: Called: Char_item.cpp if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK)) ");

#endif
#ifdef ENABLE_NEW_STACK_LIMIT
					int
#else
					uint8_t
#endif
						bCount = item->GetCount(); // change type for some

					for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
					{
						LPITEM item2 = GetExtraInventoryItem(i);

						if (!item2)
							continue;

						if (item2->GetVnum() == item->GetVnum())
						{
							int j = 0;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (item2->GetSocket(j) != item->GetSocket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
								bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount); // change type for some
							bCount -= bCount2;

#ifdef ENABLE_BATTLE_PASS
							if (bIsBattlePass)
							{
								uint8_t bBattlePassId = GetBattlePassId();
								if (bBattlePassId)
								{
									uint32_t dwItemVnum, dwCount;
									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM2, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
									}
								}
							}
#endif

							item2->SetCount(item2->GetCount() + bCount2);

							if (bCount == 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
									CHAT_TYPE_INFO_ITEM
#else
									CHAT_TYPE_INFO
#endif
									, 102, "%d#%s", bCount2, item2->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
								//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 01 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
								M2_DESTROY_ITEM(item);
								return true;
							}
						}
					}

					item->SetCount(bCount);
				}
				else if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#else
				if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#endif
				{
#ifdef ENABLE_NEW_STACK_LIMIT
					int
#else
					uint8_t
#endif
						bCount = item->GetCount();

					for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
					{
						LPITEM item2 = GetInventoryItem(i);

						if (!item2)
							continue;

						if (item2->GetVnum() == item->GetVnum())
						{
							int j;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (item2->GetSocket(j) != item->GetSocket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
								bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
							bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
							if (bIsBattlePass)
							{
								uint8_t bBattlePassId = GetBattlePassId();
								if (bBattlePassId)
								{
									uint32_t dwItemVnum, dwCount;
									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM2, bBattlePassId) < dwCount)
											UpdateMissionProgress(COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
									}
								}
							}
#endif
							item2->SetCount(item2->GetCount() + bCount2);

							if (bCount == 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
									CHAT_TYPE_INFO_ITEM
#else
									CHAT_TYPE_INFO
#endif
									, 102, "%d#%s", bCount2, item2->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
								//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 02 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
								M2_DESTROY_ITEM(item);
								return true;
							}
						}
					}

					item->SetCount(bCount);
				}

				int iEmptyCell;
				if (item->IsDragonSoul())
				{
					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
				}
#ifdef ENABLE_EXTRA_INVENTORY
				else if (item->IsExtraItem())
				{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
					ChatPacket(CHAT_TYPE_INFO, "char_item.cpp: line 9217  else if (item->IsExtraItem()).");//INGAME_DEBUG_RAZOR93

					sys_log(0, "Razor93 LOG:: Called: Char_item.cpp else if (item->IsExtraItem()) ");

#endif
					if ((iEmptyCell = GetEmptyExtraInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 539, "");
#endif
						return false;
					}
				}
#endif
				else
				{
					if ((iEmptyCell = GetEmptyInventory(item->GetSize())) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
				}

				item->RemoveFromGround();

				if (item->IsDragonSoul())
					item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef ENABLE_EXTRA_INVENTORY
				else if (item->IsExtraItem())
					item->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, iEmptyCell));
#endif
				else
					item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell));

#ifdef ENABLE_BATTLE_PASS
				if (bIsBattlePass)
				{
					uint8_t bBattlePassId = GetBattlePassId();
					if (bBattlePassId)
					{
						uint32_t dwItemVnum, dwCount;
						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
								UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, item->GetCount(), dwCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
								UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(COLLECT_ITEM2, bBattlePassId) < dwCount)
								UpdateMissionProgress(COLLECT_ITEM2, bBattlePassId, item->GetCount(), dwCount);
						}
					}
				}
#endif

				char szHint[32 + 1];
				snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
				LogManager::instance().ItemLog(this, item, "GET", szHint);
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));//f√∂ldr√∂l
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 03 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
			}

			//Motion(MOTION_PICKUP);
			return true;
		}
		else if (!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && GetParty())
		{
			// ¬¥√ô¬∏¬• √Ü√Ñ√Ü¬º¬ø√∏ ¬º√í√Ä¬Ø¬±√á ¬æ√Ü√Ä√å√Ö√õ√Ä¬ª √Å√ñ√Ä¬∏¬∑√Å¬∞√≠ √á√ë¬¥√ô¬∏√©
			NPartyPickupDistribute::FFindOwnership funcFindOwnership(item);

			GetParty()->ForEachOnlineMember(funcFindOwnership);

			LPCHARACTER owner = funcFindOwnership.owner;
			// @fixme115
			if (!owner)
				return false;

#ifdef ENABLE_EXTRA_INVENTORY
			if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
			{
#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount = item->GetCount(); // change type for some

				for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
				{
					LPITEM item2 = owner->GetExtraInventoryItem(i);

					if (!item2)
						continue;

					if (item2->GetVnum() == item->GetVnum())
					{
						int j = 0;

						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (item2->GetSocket(j) != item->GetSocket(j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
							bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount); // change type for some
						bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
						if (bIsBattlePass)
						{
							uint8_t bBattlePassId = owner->GetBattlePassId();
							if (bBattlePassId)
							{
								uint32_t dwItemVnum, dwCount;
								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM2, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
								}
							}
						}
#endif
						item2->SetCount(item2->GetCount() + bCount2);

						if (bCount == 0)
						{
#ifdef TEXTS_IMPROVEMENT
							owner->ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
								CHAT_TYPE_INFO_ITEM
#else
								CHAT_TYPE_INFO
#endif
								, 102, "%d#%s", item2->GetCount(), item2->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
							//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 04 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
							M2_DESTROY_ITEM(item);
							return true;
						}
					}
				}

				item->SetCount(bCount);
			}
			else if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#else
			if (item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
#endif
			{
#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount = item->GetCount();

				for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
				{
					LPITEM item2 = owner->GetInventoryItem(i);

					if (!item2)
						continue;

					if (item2->GetVnum() == item->GetVnum())
					{
						int j;

						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (item2->GetSocket(j) != item->GetSocket(j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
							bCount2 = std::min(g_bItemCountLimit - item2->GetCount(), bCount);
						bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
						if (bIsBattlePass)
						{
							uint8_t bBattlePassId = owner->GetBattlePassId();
							if (bBattlePassId)
							{
								uint32_t dwItemVnum, dwCount;
								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM2, bBattlePassId) < dwCount)
										owner->UpdateMissionProgress(COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
								}
							}
						}
#endif
						item2->SetCount(item2->GetCount() + bCount2);

						if (bCount == 0)
						{
#ifdef TEXTS_IMPROVEMENT
							owner->ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
								CHAT_TYPE_INFO_ITEM
#else
								CHAT_TYPE_INFO
#endif
								, 102, "%d#%s", bCount2, item2->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
							//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 05 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
							M2_DESTROY_ITEM(item);
							return true;
						}
					}
				}

				item->SetCount(bCount);
			}

			int iEmptyCell;

			if (item->IsDragonSoul())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyDragonSoulInventory(item)) != -1))
				{
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						owner->ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
#endif
				}
			}
#ifdef ENABLE_EXTRA_INVENTORY
			else if (item->IsExtraItem())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyExtraInventory(item)) != -1))
				{
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = GetEmptyExtraInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 539, "");
#endif
						return false;
					}
#endif
				}
			}
#endif
			else
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyInventory(item->GetSize())) != -1))
				{
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = GetEmptyInventory(item->GetSize())) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						owner->ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
#endif
				}
			}

			item->RemoveFromGround();

			if (item->IsDragonSoul())
				item->AddToCharacter(owner, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef ENABLE_EXTRA_INVENTORY
			else if (item->IsExtraItem())
				item->AddToCharacter(owner, TItemPos(EXTRA_INVENTORY, iEmptyCell));
#endif
			else
				item->AddToCharacter(owner, TItemPos(INVENTORY, iEmptyCell));

#ifdef ENABLE_BATTLE_PASS
			if (bIsBattlePass)
			{
				uint8_t bBattlePassId = owner->GetBattlePassId();
				if (bBattlePassId)
				{
					uint32_t dwItemVnum, dwCount;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM, bBattlePassId) < dwCount)
							owner->UpdateMissionProgress(COLLECT_ITEM, bBattlePassId, item->GetCount(), dwCount);
					}

					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
							owner->UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
					}

					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && owner->GetMissionProgress(COLLECT_ITEM1, bBattlePassId) < dwCount)
							owner->UpdateMissionProgress(COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
					}
				}
			}
#endif

			char szHint[32 + 1];
			snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
			LogManager::instance().ItemLog(owner, item, "GET", szHint);

			if (owner == this) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 06 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				owner->ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kapt√°l:]|r 07 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
#ifdef TEXTS_IMPROVEMENT
				owner->ChatPacketNew(CHAT_TYPE_INFO, 401, "%s", item->GetName());
#endif
			}

			return true;
		}
	}

	return false;
}

// char_item.cpp slice C2a moved into ItemSystem.cpp

bool CHARACTER::UseItem(TItemPos Cell, TItemPos DestCell)
{

#ifdef ENABLE_USEITEM_COOLDOWN
	if (GetMapIndex() == 113) {
		return false;
	}
#endif

	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	//uint16_t wDestCell = DestCell.cell;
	//uint8_t bDestInven = DestCell.window_type;
	LPITEM item;

	if (!CanHandleItem())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

#ifdef ENABLE_USEITEM_COOLDOWN
	if (item->GetVnum() >= 39999 && item->GetType() == ITEM_QUEST) {
		int pulse = thecore_pulse();
		if (pulse > GetCmdAntiFloodPulse() + PASSES_PER_SEC(1)) {
			SetItemUseAntiFloodCount(0);
			SetItemUseAntiFloodPulse(thecore_pulse());
		}

		if (IncreaseItemUseAntiFloodCount() >= 10) {
			GetDesc()->DelayedDisconnect(0);
			return false;
		}

		SetCmdAntiFloodPulse(pulse);
	}
#endif

	LPITEM destItem = GetItem(DestCell);
	if (destItem && item != destItem && destItem->IsStackable() && !IS_SET(destItem->GetAntiFlag(), ITEM_ANTIFLAG_STACK) && destItem->GetVnum() == item->GetVnum())
	{
		if (MoveItem(Cell, DestCell, 0))
			return false;
	}

#ifdef ENABLE_BUG_FIXES
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 1247, "");
#endif
		//if (GetDesc()) {
		//	GetDesc()->DelayedDisconnect(3);
		//}
		return false;
	}
#endif

	sys_log(0, "%s: USE_ITEM %s (inven %d, cell: %d)", GetName(), item->GetName(), window_type, wCell);

	if (item->IsExchanging())
		return false;
	// Lua-less item_change quest handlers
	if (item_change::HandleUse(this, item))
		return true;
#ifdef ENABLE_SWITCHBOT
	if (Cell.IsSwitchbotPosition())
	{
		CSwitchbot* pkSwitchbot = CSwitchbotManager::Instance().FindSwitchbot(GetPlayerID());
		if (pkSwitchbot && pkSwitchbot->IsActive(Cell.cell))
		{
			return false;
		}

		int iEmptyCell = GetEmptyInventory(item->GetSize());

		if (iEmptyCell == -1)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 687, "");
#endif
			return false;
		}

		MoveItem(Cell, TItemPos(INVENTORY, iEmptyCell), item->GetCount());
		return true;
	}
#endif
	if (!item->CanUsedBy(this))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 495, "");
#endif
		return false;
	}

	if (IsStun())
		return false;

	if (false == FN_check_item_sex(this, item))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 496, "");
#endif
		return false;
	}

#ifdef ENABLE_PVP_ADVANCED	
	if ((GetDuel("BlockPotion")) && IS_POTION_PVP_BLOCKED(item->GetVnum()))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif	

	//PREVENT_TRADE_WINDOW
	if (IS_SUMMON_ITEM(item->GetVnum()))
	{
		if (false == IS_SUMMONABLE_ZONE(GetMapIndex()))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 688, "");
#endif
			return false;
		}

		int iPulse = thecore_pulse();

		//√É¬¢¬∞√≠ ¬ø¬¨√à√Ñ √É¬º√Ö¬©
		if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}

		//¬∞√Ö¬∑¬°¬∞√º¬∑√É √É¬¢ √É¬º√Ö¬©
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 235, "");
#endif
			return false;
		}

#ifdef __ATTR_TRANSFER_SYSTEM__
		if (IsAttrTransferOpen())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 235, "");
#endif
			return false;
		}
#endif

		//PREVENT_REFINE_HACK
		//¬∞¬≥¬∑¬Æ√à√Ñ ¬Ω√É¬∞¬£√É¬º√Ö¬©
		{
			if (iPulse - GetRefineTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
				return false;
			}
		}
		//END_PREVENT_REFINE_HACK


		//PREVENT_ITEM_COPY
		{
			if (iPulse - GetMyShopTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
				return false;
			}

		}
		//END_PREVENT_ITEM_COPY


		//¬±√ç√à¬Ø¬∫√é ¬∞√Ö¬∏¬Æ√É¬º√Ö¬©
		if (item->GetVnum() != 70302)
		{
			PIXEL_POSITION posWarp;

			int x = 0;
			int y = 0;

			double nDist = 0;
			const double nDistant = 5000.0;
			//¬±√ç√à¬Ø¬±√¢¬æ√Ø¬∫√é
			if (item->GetVnum() == 22010)
			{
				x = item->GetSocket(0) - GetX();
				y = item->GetSocket(1) - GetY();
			}
			//¬±√ç√à¬Ø¬∫√é
			else if (item->GetVnum() == 22000)
			{
				SECTREE_MANAGER::instance().GetRecallPositionByEmpire(GetMapIndex(), GetEmpire(), posWarp);

				if (item->GetSocket(0) == 0)
				{
					x = posWarp.x - GetX();
					y = posWarp.y - GetY();
				}
				else
				{
					x = item->GetSocket(0) - GetX();
					y = item->GetSocket(1) - GetY();
				}
			}

			nDist = sqrt(pow((float)x, 2) + pow((float)y, 2));
			if (nDistant > nDist) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 433, "");
#endif
				return false;
			}
		}

		//PREVENT_PORTAL_AFTER_EXCHANGE
		//¬±¬≥√à¬Ø √à√Ñ ¬Ω√É¬∞¬£√É¬º√Ö¬©
		if (iPulse - GetExchangeTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

	}

	//¬∫¬∏¬µ√ª¬∏¬Æ ¬∫√±¬¥√ú ¬ª√ß¬ø√´¬Ω√É ¬∞√Ö¬∑¬°√É¬¢ √Å¬¶√á√ë √É¬º√Ö¬©
	if ((item->GetVnum() == 50200) || (item->GetVnum() == 71049)
#ifdef KASMIR_PAKET_SYSTEM
		|| (item->GetVnum() == 88901)
#endif
		)
	{
		if (GetExchange() || GetMyShop() || GetShopOwner() || IsOpenSafebox() || IsCubeOpen())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 237, "");
#endif
			return false;
		}

#ifdef __ATTR_TRANSFER_SYSTEM__
		if (IsAttrTransferOpen())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 237, "");
#endif
			return false;
		}
#endif
	}
	//END_PREVENT_TRADE_WINDOW

	if (IS_SET(item->GetFlag(), ITEM_FLAG_LOG)) // ¬ª√ß¬ø√´ ¬∑√é¬±√ó¬∏¬¶ ¬≥¬≤¬±√¢¬¥√Ç ¬æ√Ü√Ä√å√Ö√õ √É¬≥¬∏¬Æ
	{
		uint32_t vid = item->GetVID();
		int oldCount = item->GetCount();
		uint32_t vnum = item->GetVnum();

		char hint[ITEM_NAME_MAX_LEN + 48 + 1];
		int len = snprintf(hint, sizeof(hint) - 48, "%s", item->GetName());

		if (len < 0 || len >= (int)sizeof(hint) - 48)
			len = (sizeof(hint) - 48) - 1;

		bool ret = UseItemEx(item, DestCell);

		if (nullptr == ITEM_MANAGER::instance().FindByVID(vid)) // UseItemEx¬ø¬°¬º¬≠ ¬æ√Ü√Ä√å√Ö√õ√Ä√å ¬ª√®√Å¬¶ ¬µ√á¬æ√∫¬¥√ô. ¬ª√®√Å¬¶ ¬∑√é¬±√ó¬∏¬¶ ¬≥¬≤¬±√®
		{
			LogManager::instance().ItemLog(this, vid, vnum, "REMOVE", hint);
		}
		else if (oldCount != item->GetCount())
		{
			snprintf(hint + len, sizeof(hint) - len, " %u", oldCount - 1);
			LogManager::instance().ItemLog(this, vid, vnum, "USE_ITEM", hint);
		}
		return (ret);
	}
	else
		return UseItemEx(item, DestCell);
}
